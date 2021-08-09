#include <iostream>
#include <unordered_map>

#include "gp_beader.h"
#include "gp_media_server.h"
#include "gp_message.h"

namespace GPlayer {

GPMediaServer::GPMediaServer(uint16_t port)
    : net::server_interface<PlayerMsg>(port)
{
}

GPMediaServer::~GPMediaServer() {}

std::string GPMediaServer::GetInfo() const
{
    return "GPMediaServer";
}

int GPMediaServer::Proc()
{
    GPMediaServer server(port_);
    server.Start();

    while (1) {
        server.Update(-1, true);
    }
    return 0;
}

bool GPMediaServer::OnClientConnect(
    std::shared_ptr<net::connection<PlayerMsg>> client)
{
    return true;
}

void GPMediaServer::OnClientValidated(
    std::shared_ptr<net::connection<PlayerMsg>> client)
{
    net::message<PlayerMsg> msg;
    msg.header.id = PlayerMsg::Client_Accepted;
    client->Send(msg);
}

void GPMediaServer::OnClientDisconnect(
    std::shared_ptr<net::connection<PlayerMsg>> client)
{
    if (client) {
        if (player_roster_.find(client->GetID()) == player_roster_.end()) {
        }
        else {
            auto& pd = player_roster_[client->GetID()];
            // std::cout << "[UNGRACEFUL REMOVAL]:" +
            //                  std::to_string(pd.nUniqueID) + "\n";
            player_roster_.erase(client->GetID());
            garbage_ids_.push_back(client->GetID());
        }
    }
}

void GPMediaServer::OnMessage(
    std::shared_ptr<net::connection<PlayerMsg>> client,
    net::message<PlayerMsg>& msg)
{
    if (!garbage_ids_.empty()) {
        for (auto pid : garbage_ids_) {
            net::message<PlayerMsg> m;
            m.header.id = PlayerMsg::Game_RemovePlayer;
            m << pid;
            std::cout << "Removing " << pid << "\n";
            MessageAllClients(m);
        }
        garbage_ids_.clear();
    }

    switch (msg.header.id) {
        case PlayerMsg::Client_RegisterWithServer: {
            // sPlayerDescription desc;
            // msg >> desc;
            // desc.nUniqueID = client->GetID();
            // player_roster_.insert_or_assign(desc.nUniqueID, desc);

            // net::message<PlayerMsg> msgSendID;
            // msgSendID.header.id = PlayerMsg::Client_AssignID;
            // msgSendID << desc.nUniqueID;
            // MessageClient(client, msgSendID);

            // net::message<PlayerMsg> msgAddPlayer;
            // msgAddPlayer.header.id = PlayerMsg::Game_AddPlayer;
            // msgAddPlayer << desc;
            // MessageAllClients(msgAddPlayer);

            // for (const auto& player : player_roster_) {
            //     net::message<PlayerMsg> msgAddOtherPlayers;
            //     msgAddOtherPlayers.header.id = PlayerMsg::Game_AddPlayer;
            //     msgAddOtherPlayers << player.second;
            //     MessageClient(client, msgAddOtherPlayers);
            // }

            break;
        }

        case PlayerMsg::Client_UnregisterWithServer: {
            break;
        }

        case PlayerMsg::Game_UpdatePlayer: {
            MessageAllClients(msg, client);
            break;
        }
    }
}

}  // namespace GPlayer