#ifndef __GP_FILE__
#define __GP_FILE__

#include <fstream>
#include <string>
#include "gp_buffer.h"
#include "module.h"

namespace GPlayer {

class GPFile : public IModule {
private:
    GPFile();

public:
    GPFile(std::string filename);
    ~GPFile();
    std::string GetInfo() const;
    void AddHandler(IModule* module);
    void Process(GPBuffer* buffer);

private:
    std::string filepath_;
    std::ofstream* outfile_;
};

}  // namespace GPlayer

#endif  // __GP_FILE__
