
#include <gp_net.h>
#include <iostream>

enum class CustomMsgTypes : uint32_t {
    ServerAccept,
    ServerDeny,
    ServerPing,
    MessageAll,
    ServerMessage,
};

class CustomServer : public GPlayer::net::server_interface1<CustomMsgTypes> {
public:
    CustomServer(uint16_t nPort)
        : GPlayer::net::server_interface1<CustomMsgTypes>(nPort)
    {
    }

protected:
    virtual bool OnClientConnect(
        std::shared_ptr<GPlayer::net::connection<CustomMsgTypes>> client)
    {
        GPlayer::net::message<CustomMsgTypes> msg;
        msg.header.id = CustomMsgTypes::ServerAccept;
        client->Send(msg);
        return true;
    }

    // Called when a client appears to have disconnected
    virtual void OnClientDisconnect(
        std::shared_ptr<GPlayer::net::connection<CustomMsgTypes>> client)
    {
        std::cout << "Removing client [" << client->GetID() << "]\n";
    }

    // Called when a message arrives
    virtual void OnMessage(
        std::shared_ptr<GPlayer::net::connection<CustomMsgTypes>> client,
        GPlayer::net::message<CustomMsgTypes>& msg)
    {
        switch (msg.header.id) {
            case CustomMsgTypes::ServerPing: {
                std::cout << "[" << client->GetID() << "]: Server Ping\n";

                // Simply bounce message back to client
                client->Send(msg);
            } break;

            case CustomMsgTypes::MessageAll: {
                std::cout << "[" << client->GetID() << "]: Message All\n";

                // Construct a new message and send it to all clients
                GPlayer::net::message<CustomMsgTypes> msg;
                msg.header.id = CustomMsgTypes::ServerMessage;
                msg << client->GetID();
                MessageAllClients(msg, client);

            } break;
        }
    }
};

int main()
{
    CustomServer server(60000);
    server.Start();

    while (1) {
        server.Update(-1, true);
    }

    return 0;
}