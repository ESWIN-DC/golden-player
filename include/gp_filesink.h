#ifndef __GP_FILESINK__
#define __GP_FILESINK__

#include <fstream>
#include <string>
#include "beader.h"
#include "gp_data.h"

namespace GPlayer {

class GPFileSink : public IBeader {
private:
    GPFileSink();

public:
    GPFileSink(std::string filename);
    ~GPFileSink();
    std::string GetInfo() const;
    void Process(GPData* data);

private:
    std::string filepath_;
    std::ofstream* outfile_;
};

}  // namespace GPlayer

#endif  // __GP_FILESINK__
