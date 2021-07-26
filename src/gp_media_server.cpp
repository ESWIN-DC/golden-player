#include <iostream>
#include <unordered_map>

#include "gp_beader.h"
#include "gp_media_server.h"
#include "gp_message.h"

namespace GPlayer {

GPMediaServer::GPMediaServer(uint16_t nPort)
    : GPlayer::net::server_interface<PlayerMsg>(nPort)
{
}

GPMediaServer::~GPMediaServer() {}

bool GPMediaServer::OnClientConnect(
    std::shared_ptr<GPlayer::net::connection<PlayerMsg>> client)
{
    // For now we will allow all
    return true;
}

void GPMediaServer::OnClientValidated(
    std::shared_ptr<GPlayer::net::connection<PlayerMsg>> client)
{
    // Client passed validation check, so send them a message informing
    // them they can continue to communicate
    GPlayer::net::message<PlayerMsg> msg;
    msg.header.id = PlayerMsg::Client_Accepted;
    client->Send(msg);
}

void GPMediaServer::OnClientDisconnect(
    std::shared_ptr<GPlayer::net::connection<PlayerMsg>> client)
{
    if (client) {
        if (m_mapPlayerRoster.find(client->GetID()) ==
            m_mapPlayerRoster.end()) {
            // client never added to roster, so just let it disappear
        }
        else {
            auto& pd = m_mapPlayerRoster[client->GetID()];
            // std::cout << "[UNGRACEFUL REMOVAL]:" +
            //                  std::to_string(pd.nUniqueID) + "\n";
            m_mapPlayerRoster.erase(client->GetID());
            m_vGarbageIDs.push_back(client->GetID());
        }
    }
}

void GPMediaServer::OnMessage(
    std::shared_ptr<GPlayer::net::connection<PlayerMsg>> client,
    GPlayer::net::message<PlayerMsg>& msg)
{
    if (!m_vGarbageIDs.empty()) {
        for (auto pid : m_vGarbageIDs) {
            GPlayer::net::message<PlayerMsg> m;
            m.header.id = PlayerMsg::Game_RemovePlayer;
            m << pid;
            std::cout << "Removing " << pid << "\n";
            MessageAllClients(m);
        }
        m_vGarbageIDs.clear();
    }

    switch (msg.header.id) {
        case PlayerMsg::Client_RegisterWithServer: {
            // sPlayerDescription desc;
            // msg >> desc;
            // desc.nUniqueID = client->GetID();
            // m_mapPlayerRoster.insert_or_assign(desc.nUniqueID, desc);

            // GPlayer::net::message<PlayerMsg> msgSendID;
            // msgSendID.header.id = PlayerMsg::Client_AssignID;
            // msgSendID << desc.nUniqueID;
            // MessageClient(client, msgSendID);

            // GPlayer::net::message<PlayerMsg> msgAddPlayer;
            // msgAddPlayer.header.id = PlayerMsg::Game_AddPlayer;
            // msgAddPlayer << desc;
            // MessageAllClients(msgAddPlayer);

            // for (const auto& player : m_mapPlayerRoster) {
            //     GPlayer::net::message<PlayerMsg> msgAddOtherPlayers;
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
            // Simply bounce update to everyone except incoming client
            MessageAllClients(msg, client);
            break;
        }
    }
}

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

}  // namespace GPlayer