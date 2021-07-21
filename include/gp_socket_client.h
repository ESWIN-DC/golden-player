#ifndef __GP_SOCKET_CLIENT_H__
#define __GP_SOCKET_CLIENT_H__

#include <fstream>
#include <string>

#include "gp_beader.h"
#include "gp_data.h"

namespace GPlayer {

class GPSocketClient : public IBeader {
private:
    GPSocketClient() = delete;

public:
    GPSocketClient(int port);
    ~GPSocketClient();
    std::string GetInfo() const;
    int Proc() override;
    bool HasProc() override { return true; };
    void Process(GPData* data);
    std::basic_istream<char>& Read(char* buffer, std::streamsize count);

private:
    std::string filepath_;
    std::ifstream* inputfile_;
    int port_;
};

}  // namespace GPlayer

#endif  // __GP_SOCKET_CLIENT_H__
