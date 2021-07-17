#ifndef __GP_FILESRC__
#define __GP_FILESRC__

#include <fstream>
#include <string>

#include "gp_beader.h"
#include "gp_data.h"

namespace GPlayer {

class GPFileSrc : public IBeader {
private:
    GPFileSrc() = delete;

public:
    GPFileSrc(std::string filename);
    ~GPFileSrc();
    std::string GetInfo() const;
    bool HasProc() override { return false; };
    void Process(GPData* data);
    std::basic_istream<char>& Read(char* buffer, std::streamsize count);

private:
    std::string filepath_;
    std::ifstream* inputfile_;
};

}  // namespace GPlayer

#endif  // __GP_FILESRC__
