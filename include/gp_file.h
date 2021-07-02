#ifndef __GP_FILE__
#define __GP_FILE__

#include <fstream>
#include <string>
#include "beader.h"
#include "gp_data.h"

namespace GPlayer {

class GPFile : public IBeader {
private:
    GPFile();

public:
    GPFile(std::string filename);
    ~GPFile();
    std::string GetInfo() const;
    void AddBeader(IBeader* module);
    void Process(GPData* data);

private:
    std::string filepath_;
    std::ofstream* outfile_;
};

}  // namespace GPlayer

#endif  // __GP_FILE__
