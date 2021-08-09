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
    GPMediaServer(uint16_t port);
    ~GPMediaServer();
    std::string GetInfo() const;
    bool HasProc() override { return true; };
    void Process(GPData* data);
    std::basic_istream<char>& Read(char* buffer, std::streamsize count);
    int Proc() override;

protected:
    bool OnClientConnect(
        std::shared_ptr<net::connection<PlayerMsg>> client) override;

    void OnClientValidated(
        std::shared_ptr<net::connection<PlayerMsg>> client) override;

    void OnClientDisconnect(
        std::shared_ptr<net::connection<PlayerMsg>> client) override;

    void OnMessage(std::shared_ptr<net::connection<PlayerMsg>> client,
                   net::message<PlayerMsg>& msg) override;

private:
    uint16_t port_;
    std::unordered_map<uint32_t, sPlayerDescription> player_roster_;
    std::vector<uint32_t> garbage_ids_;
};

}  // namespace GPlayer

#endif  // __GP_MEDIA_SERVER_H__
