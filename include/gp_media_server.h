#ifndef __GP_MEDIA_SERVER_H__
#define __GP_MEDIA_SERVER_H__

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "gp_beader.h"
#include "gp_data.h"
#include "gp_message.h"

namespace GPlayer {

class GPMediaServer : public net::server_interface<PlayerMsg>, public IBeader {
private:
    GPMediaServer() = delete;

public:
    GPMediaServer(uint16_t nPort);
    ~GPMediaServer();
    std::string GetInfo() const;
    bool HasProc() override { return true; };
    void Process(GPData* data);
    std::basic_istream<char>& Read(char* buffer, std::streamsize count);
    int Proc() override;

protected:
    bool OnClientConnect(
        std::shared_ptr<GPlayer::net::connection<PlayerMsg>> client) override;

    void OnClientValidated(
        std::shared_ptr<GPlayer::net::connection<PlayerMsg>> client) override;

    void OnClientDisconnect(
        std::shared_ptr<GPlayer::net::connection<PlayerMsg>> client) override;

    void OnMessage(std::shared_ptr<GPlayer::net::connection<PlayerMsg>> client,
                   GPlayer::net::message<PlayerMsg>& msg) override;

private:
    std::string filepath_;
    std::ifstream* inputfile_;
    uint16_t port_;

private:
    std::unordered_map<uint32_t, sPlayerDescription> m_mapPlayerRoster;
    std::vector<uint32_t> m_vGarbageIDs;
};

}  // namespace GPlayer

#endif  // __GP_MEDIA_SERVER_H__
