
#include <asio.hpp>

#include "gp_log.h"
#include "gp_socket_client.h"

namespace GPlayer {

GPSocketClient::GPSocketClient(int port) : port_(port)
{
    SetProperties("GPSocketClient", "GPSocketClient",
                  BeaderType::SocketServerSrc, true);
}

GPSocketClient::~GPSocketClient()
{
    inputfile_->close();
    delete inputfile_;
}

std::string GPSocketClient::GetInfo() const
{
    return "GPSocketClient: " + filepath_;
}

void GPSocketClient::Process(GPData* data)
{
    GPBuffer* buffer = *data;
    inputfile_->read(reinterpret_cast<char*>(buffer->GetData()),
                     buffer->GetLength());
}

std::basic_istream<char>& GPSocketClient::Read(char* buffer,
                                               std::streamsize count)
{
    return inputfile_->read(buffer, count);
}

using asio::ip::tcp;

void process_client(std::shared_ptr<tcp::socket> client)
{
    time_t now = time(0);
    std::shared_ptr<std::string> message(new std::string(ctime(&now)));

    auto callback = [=](const asio::error_code& err, size_t size) {
        if (size == message->length())
            SPDLOG_TRACE("write completed");
    };

    client->async_send(asio::buffer(*message), callback);
}

typedef std::function<void(const asio::error_code&)> accept_callback;

void start_accept(tcp::acceptor& server)
{
    std::shared_ptr<tcp::socket> client(new tcp::socket(server.get_executor()));
    accept_callback callback = [&server,
                                client](const asio::error_code& error) {
        if (!error)
            process_client(client);

        start_accept(server);
    };

    server.async_accept(*client, callback);
}

int GPSocketClient::Proc()
{
    try {
        asio::io_service io_service;
        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), port_));
        start_accept(acceptor);
        io_service.run();
    }
    catch (std::exception& e) {
        SPDLOG_CRITICAL(e.what());
    }
    return 0;
}

}  // namespace GPlayer
