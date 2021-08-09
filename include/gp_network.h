
#ifndef __GP_NETWORK_H__
#define __GP_NETWORK_H__

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

namespace GPlayer {
namespace net {

template <typename T>
struct message_header {
    T id{};
    uint32_t size = 0;
};

template <typename T>
struct message {
    message_header<T> header{};
    std::vector<uint8_t> body;

    size_t size() const { return body.size(); }

    friend std::ostream& operator<<(std::ostream& os, const message<T>& msg)
    {
        os << "ID:" << int(msg.header.id) << " Size:" << msg.header.size;
        return os;
    }

    template <typename DataType>
    friend message<T>& operator<<(message<T>& msg, const DataType& data)
    {
        static_assert(std::is_standard_layout<DataType>::value,
                      "Data is too complex to be pushed into vector");

        size_t i = msg.body.size();

        msg.body.resize(msg.body.size() + sizeof(DataType));

        std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

        msg.header.size = msg.size();

        return msg;
    }

    template <typename DataType>
    friend message<T>& operator>>(message<T>& msg, DataType& data)
    {
        static_assert(std::is_standard_layout<DataType>::value,
                      "Data is too complex to be pulled from vector");

        size_t i = msg.body.size() - sizeof(DataType);

        std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

        msg.body.resize(i);

        msg.header.size = msg.size();

        return msg;
    }
};

// Forward declare the connection
template <typename T>
class connection;

template <typename T>
struct owned_message {
    std::shared_ptr<connection<T>> remote = nullptr;
    message<T> msg;

    friend std::ostream& operator<<(std::ostream& os,
                                    const owned_message<T>& msg)
    {
        os << msg.msg;
        return os;
    }
};

// Queue
template <typename T>
class tsqueue {
public:
    tsqueue() = default;
    tsqueue(const tsqueue<T>&) = delete;
    virtual ~tsqueue() { clear(); }

public:
    const T& front()
    {
        std::scoped_lock lock(mux_queue_);
        return deq_queue_.front();
    }

    const T& back()
    {
        std::scoped_lock lock(mux_queue_);
        return deq_queue_.back();
    }

    T pop_front()
    {
        std::scoped_lock lock(mux_queue_);
        auto t = std::move(deq_queue_.front());
        deq_queue_.pop_front();
        return t;
    }

    T pop_back()
    {
        std::scoped_lock lock(mux_queue_);
        auto t = std::move(deq_queue_.back());
        deq_queue_.pop_back();
        return t;
    }

    void push_back(const T& item)
    {
        std::scoped_lock lock(mux_queue_);
        deq_queue_.emplace_back(std::move(item));

        std::unique_lock<std::mutex> ul(mux_blocking_);
        cv_blocking_.notify_one();
    }

    void push_front(const T& item)
    {
        std::scoped_lock lock(mux_queue_);
        deq_queue_.emplace_front(std::move(item));

        std::unique_lock<std::mutex> ul(mux_blocking_);
        cv_blocking_.notify_one();
    }

    bool empty()
    {
        std::scoped_lock lock(mux_queue_);
        return deq_queue_.empty();
    }

    size_t count()
    {
        std::scoped_lock lock(mux_queue_);
        return deq_queue_.size();
    }

    void clear()
    {
        std::scoped_lock lock(mux_queue_);
        deq_queue_.clear();
    }

    void wait()
    {
        while (empty()) {
            std::unique_lock<std::mutex> ul(mux_blocking_);
            cv_blocking_.wait(ul);
        }
    }

protected:
    std::mutex mux_queue_;
    std::deque<T> deq_queue_;
    std::condition_variable cv_blocking_;
    std::mutex mux_blocking_;
};

// Connection
template <typename T>
class server_interface;

template <typename T>
class connection : public std::enable_shared_from_this<connection<T>> {
public:
    enum class owner { server, client };

public:
    connection(owner parent,
               asio::io_context& io_context,
               asio::ip::tcp::socket socket,
               tsqueue<owned_message<T>>& message_in)
        : socket_(std::move(socket)),
          io_context_(io_context),
          messages_in_(message_in)
    {
        owner_type_ = parent;

        if (owner_type_ == owner::server) {
            handshake_out_ = uint64_t(
                std::chrono::system_clock::now().time_since_epoch().count());

            handshake_check_ = scramble(handshake_out_);
        }
        else {
            handshake_in_ = 0;
            handshake_out_ = 0;
        }
    }

    virtual ~connection() {}

    uint32_t GetID() const { return id_; }

public:
    void ConnectToClient(net::server_interface<T>* server, uint32_t uid = 0)
    {
        if (owner_type_ == owner::server) {
            if (socket_.is_open()) {
                id_ = uid;

                WriteValidation();

                ReadValidation(server);
            }
        }
    }

    void ConnectToServer(const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (owner_type_ == owner::client) {
            asio::async_connect(
                socket_, endpoints,
                [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
                    if (!ec) {
                        // Was: ReadHeader();

                        // First thing server will do is send packet to be
                        // validated so wait for that and respond
                        ReadValidation();
                    }
                });
        }
    }

    void Disconnect()
    {
        if (IsConnected())
            asio::post(io_context_, [this]() { socket_.close(); });
    }

    bool IsConnected() const { return socket_.is_open(); }

    void StartListening() {}

public:
    void Send(const message<T>& msg)
    {
        asio::post(io_context_, [this, msg]() {
            bool writingMessage = !messages_out_.empty();
            messages_out_.push_back(msg);
            if (!writingMessage) {
                WriteHeader();
            }
        });
    }

private:
    void WriteHeader()
    {
        asio::async_write(socket_,
                          asio::buffer(&messages_out_.front().header,
                                       sizeof(message_header<T>)),
                          [this](std::error_code ec, std::size_t length) {
                              if (!ec) {
                                  if (messages_out_.front().body.size() > 0) {
                                      WriteBody();
                                  }
                                  else {
                                      messages_out_.pop_front();
                                      if (!messages_out_.empty()) {
                                          WriteHeader();
                                      }
                                  }
                              }
                              else {
                                  std::cout << "[" << id_
                                            << "] Write Header Fail.\n";
                                  socket_.close();
                              }
                          });
    }

    void WriteBody()
    {
        asio::async_write(socket_,
                          asio::buffer(messages_out_.front().body.data(),
                                       messages_out_.front().body.size()),
                          [this](std::error_code ec, std::size_t length) {
                              if (!ec) {
                                  messages_out_.pop_front();
                                  if (!messages_out_.empty()) {
                                      WriteHeader();
                                  }
                              }
                              else {
                                  std::cout << "[" << id_
                                            << "] Write Body Fail.\n";
                                  socket_.close();
                              }
                          });
    }

    void ReadHeader()
    {
        asio::async_read(
            socket_,
            asio::buffer(&temporary_msg_in_.header, sizeof(message_header<T>)),
            [this](std::error_code ec, std::size_t length) {
                if (!ec) {
                    if (temporary_msg_in_.header.size > 0) {
                        temporary_msg_in_.body.resize(
                            temporary_msg_in_.header.size);
                        ReadBody();
                    }
                    else {
                        AddToIncomingMessageQueue();
                    }
                }
                else {
                    std::cout << "[" << id_ << "] Read Header Fail.\n";
                    socket_.close();
                }
            });
    }

    void ReadBody()
    {
        asio::async_read(socket_,
                         asio::buffer(temporary_msg_in_.body.data(),
                                      temporary_msg_in_.body.size()),
                         [this](std::error_code ec, std::size_t length) {
                             if (!ec) {
                                 AddToIncomingMessageQueue();
                             }
                             else {
                                 std::cout << "[" << id_
                                           << "] Read Body Fail.\n";
                                 socket_.close();
                             }
                         });
    }

    // "Encrypt" data
    uint64_t scramble(uint64_t nInput)
    {
        uint64_t out = nInput ^ 0xDEADBEEFC0DECAFE;
        out = (out & 0xF0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F) << 4;
        return out ^ 0xC0DEFACE12345678;
    }

    void WriteValidation()
    {
        asio::async_write(socket_,
                          asio::buffer(&handshake_out_, sizeof(uint64_t)),
                          [this](std::error_code ec, std::size_t length) {
                              if (!ec) {
                                  if (owner_type_ == owner::client)
                                      ReadHeader();
                              }
                              else {
                                  socket_.close();
                              }
                          });
    }

    void ReadValidation(GPlayer::net::server_interface<T>* server = nullptr)
    {
        asio::async_read(
            socket_, asio::buffer(&handshake_in_, sizeof(uint64_t)),
            [this, server](std::error_code ec, std::size_t length) {
                if (!ec) {
                    if (owner_type_ == owner::server) {
                        if (handshake_in_ == handshake_check_) {
                            std::cout << "Client Validated" << std::endl;
                            server->OnClientValidated(this->shared_from_this());

                            ReadHeader();
                        }
                        else {
                            std::cout << "Client Disconnected (Fail Validation)"
                                      << std::endl;
                            socket_.close();
                        }
                    }
                    else {
                        handshake_out_ = scramble(handshake_in_);

                        WriteValidation();
                    }
                }
                else {
                    std::cout << "Client Disconnected (ReadValidation)"
                              << std::endl;
                    socket_.close();
                }
            });
    }

    void AddToIncomingMessageQueue()
    {
        if (owner_type_ == owner::server)
            messages_in_.push_back(
                {this->shared_from_this(), temporary_msg_in_});
        else
            messages_in_.push_back({nullptr, temporary_msg_in_});

        ReadHeader();
    }

protected:
    asio::ip::tcp::socket socket_;
    asio::io_context& io_context_;
    tsqueue<message<T>> messages_out_;
    tsqueue<owned_message<T>>& messages_in_;
    message<T> temporary_msg_in_;
    owner owner_type_ = owner::server;

    // Handshake Validation
    uint64_t handshake_out_ = 0;
    uint64_t handshake_in_ = 0;
    uint64_t handshake_check_ = 0;

    bool valid_handshake_ = false;
    bool connection_established_ = false;
    uint32_t id_ = 0;
};

// Client
template <typename T>
class client_interface {
public:
    client_interface() {}

    virtual ~client_interface() { Disconnect(); }

public:
    bool Connect(const std::string& host, const uint16_t port)
    {
        try {
            asio::ip::tcp::resolver resolver(io_context_);
            asio::ip::tcp::resolver::results_type endpoints =
                resolver.resolve(host, std::to_string(port));

            connection_ = std::make_unique<connection<T>>(
                connection<T>::owner::client, io_context_,
                asio::ip::tcp::socket(io_context_), messages_in_);

            connection_->ConnectToServer(endpoints);

            thread_context_ = std::thread([this]() { io_context_.run(); });
        }
        catch (std::exception& e) {
            std::cerr << "Client Exception: " << e.what() << "\n";
            return false;
        }
        return true;
    }

    void Disconnect()
    {
        if (IsConnected()) {
            connection_->Disconnect();
        }

        io_context_.stop();
        if (thread_context_.joinable())
            thread_context_.join();

        connection_.release();
    }

    bool IsConnected()
    {
        if (connection_)
            return connection_->IsConnected();
        else
            return false;
    }

public:
    void Send(const message<T>& msg)
    {
        if (IsConnected())
            connection_->Send(msg);
    }

    tsqueue<owned_message<T>>& Incoming() { return messages_in_; }

protected:
    asio::io_context io_context_;
    std::thread thread_context_;
    std::unique_ptr<connection<T>> connection_;

private:
    tsqueue<owned_message<T>> messages_in_;
};

// Server
template <typename T>
class server_interface {
public:
    server_interface(uint16_t port)
        : asio_acceptor_(io_context_,
                         asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {
    }

    virtual ~server_interface() { Stop(); }

    bool Start()
    {
        try {
            WaitForClientConnection();

            thread_context_ = std::thread([this]() { io_context_.run(); });
        }
        catch (std::exception& e) {
            // Something prohibited the server from listening
            std::cerr << "[SERVER] Exception: " << e.what() << "\n";
            return false;
        }

        std::cout << "[SERVER] Started!\n";
        return true;
    }

    void Stop()
    {
        io_context_.stop();

        if (thread_context_.joinable())
            thread_context_.join();

        std::cout << "[SERVER] Stopped!\n";
    }

    void WaitForClientConnection()
    {
        asio_acceptor_.async_accept([this](std::error_code ec,
                                           asio::ip::tcp::socket socket) {
            if (!ec) {
                std::cout << "[SERVER] New Connection: "
                          << socket.remote_endpoint() << "\n";

                std::shared_ptr<connection<T>> newconn =
                    std::make_shared<connection<T>>(
                        connection<T>::owner::server, io_context_,
                        std::move(socket), messages_in_);

                if (OnClientConnect(newconn)) {
                    deq_connections_.push_back(std::move(newconn));

                    deq_connections_.back()->ConnectToClient(this,
                                                             id_counter_++);

                    std::cout << "[" << deq_connections_.back()->GetID()
                              << "] Connection Approved\n";
                }
                else {
                    std::cout << "[-----] Connection Denied\n";
                }
            }
            else {
                std::cout << "[SERVER] New Connection Error: " << ec.message()
                          << "\n";
            }

            WaitForClientConnection();
        });
    }

    void MessageClient(std::shared_ptr<connection<T>> client,
                       const message<T>& msg)
    {
        if (client && client->IsConnected()) {
            client->Send(msg);
        }
        else {
            OnClientDisconnect(client);

            client.reset();

            deq_connections_.erase(std::remove(deq_connections_.begin(),
                                               deq_connections_.end(), client),
                                   deq_connections_.end());
        }
    }

    void MessageAllClients(
        const message<T>& msg,
        std::shared_ptr<connection<T>> pIgnoreClient = nullptr)
    {
        bool bInvalidClientExists = false;

        for (auto& client : deq_connections_) {
            if (client && client->IsConnected()) {
                if (client != pIgnoreClient)
                    client->Send(msg);
            }
            else {
                OnClientDisconnect(client);
                client.reset();

                bInvalidClientExists = true;
            }
        }

        if (bInvalidClientExists)
            deq_connections_.erase(std::remove(deq_connections_.begin(),
                                               deq_connections_.end(), nullptr),
                                   deq_connections_.end());
    }

    void Update(size_t nMaxMessages = -1, bool bWait = false)
    {
        if (bWait)
            messages_in_.wait();

        size_t nMessageCount = 0;
        while (nMessageCount < nMaxMessages && !messages_in_.empty()) {
            auto msg = messages_in_.pop_front();

            OnMessage(msg.remote, msg.msg);

            nMessageCount++;
        }
    }

protected:
    virtual bool OnClientConnect(std::shared_ptr<connection<T>> client)
    {
        return false;
    }

    virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client) {}

    virtual void OnMessage(std::shared_ptr<connection<T>> client,
                           message<T>& msg)
    {
    }

public:
    virtual void OnClientValidated(std::shared_ptr<connection<T>> client) {}

protected:
    tsqueue<owned_message<T>> messages_in_;
    std::deque<std::shared_ptr<connection<T>>> deq_connections_;

    asio::io_context io_context_;
    std::thread thread_context_;
    asio::ip::tcp::acceptor asio_acceptor_;
    uint32_t id_counter_ = 10000;
};

}  // namespace net
}  // namespace GPlayer

#endif  // __GP_NETWORK_H__