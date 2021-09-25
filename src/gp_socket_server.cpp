
#include <asio.hpp>
#include <ctime>
#include <iostream>
#include <string>

#include "gp_socket_server.h"

namespace GPlayer {

GPSocketServer::GPSocketServer(int port) : port_(port)
{
    SetProperties("GPSocketServer", "GPSocketServer",
                  BeaderType::SocketServerSrc, true);
}

GPSocketServer::~GPSocketServer()
{
    inputfile_->close();
    delete inputfile_;
}

std::string GPSocketServer::GetInfo() const
{
    return "GPSocketServer: " + filepath_;
}

void GPSocketServer::Process(GPData* data)
{
    GPBuffer* buffer = *data;
    inputfile_->read(reinterpret_cast<char*>(buffer->GetData()),
                     buffer->GetLength());
}

std::basic_istream<char>& GPSocketServer::Read(char* buffer,
                                               std::streamsize count)
{
    return inputfile_->read(buffer, count);
}

using asio::ip::tcp;
using asio::ip::udp;

std::string make_daytime_string()
{
    using namespace std;  // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
public:
    typedef std::shared_ptr<tcp_connection> pointer;

    static pointer create(asio::io_context& io_context)
    {
        return pointer(new tcp_connection(io_context));
    }

    tcp::socket& socket() { return socket_; }

    void start()
    {
        message_ = make_daytime_string();

        asio::async_write(
            socket_, asio::buffer(message_),
            std::bind(&tcp_connection::handle_write, shared_from_this()));
    }

private:
    tcp_connection(asio::io_context& io_context) : socket_(io_context) {}

    void handle_write() {}

    tcp::socket socket_;
    std::string message_;
};

class tcp_server {
public:
    tcp_server(asio::io_context& io_context, int port)
        : io_context_(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        start_accept();
    }

private:
    void start_accept()
    {
        tcp_connection::pointer new_connection =
            tcp_connection::create(io_context_);

        acceptor_.async_accept(
            new_connection->socket(),
            std::bind(&tcp_server::handle_accept, this, new_connection,
                      std::placeholders::_1));
    }

    void handle_accept(tcp_connection::pointer new_connection,
                       const asio::error_code& error)
    {
        if (!error) {
            new_connection->start();
        }

        // new_connection->async_read_some();

        start_accept();
    }

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

class udp_server {
public:
    udp_server(asio::io_context& io_context, int port)
        : socket_(io_context, udp::endpoint(udp::v4(), port))
    {
        start_receive();
    }

private:
    void start_receive()
    {
        socket_.async_receive_from(asio::buffer(recv_buffer_), remote_endpoint_,
                                   std::bind(&udp_server::handle_receive, this,
                                             std::placeholders::_1));
    }

    void handle_receive(const asio::error_code& error)
    {
        if (!error) {
            std::shared_ptr<std::string> message(
                new std::string(make_daytime_string()));

            socket_.async_send_to(
                asio::buffer(*message), remote_endpoint_,
                std::bind(&udp_server::handle_send, this, message));

            start_receive();
        }
    }

    void handle_send(std::shared_ptr<std::string> /*message*/) {}

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 1> recv_buffer_;
};

int GPSocketServer::Proc()
{
    try {
        asio::io_context io_context;
        tcp_server server1(io_context, port_);
        udp_server server2(io_context, port_);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

}  // namespace GPlayer

#include "asio/coroutine.hpp"
#include "asio/yield.hpp"

struct session : asio::coroutine {
    std::shared_ptr<asio::ip::tcp::socket> socket_;
    std::shared_ptr<std::vector<char> > buffer_;

    session(std::shared_ptr<asio::ip::tcp::socket> socket)
        : socket_(socket), buffer_(new std::vector<char>(1024))
    {
    }

    void operator()(asio::error_code ec = asio::error_code(), std::size_t n = 0)
    {
        if (!ec)
            reenter(this)
            {
                for (;;) {
                    yield socket_->async_read_some(asio::buffer(*buffer_),
                                                   *this);
                    yield asio::async_write(*socket_, asio::buffer(*buffer_, n),
                                            *this);
                }
            }
    }
};