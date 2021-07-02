#ifndef __GP_FILESINK__
#define __GP_FILESINK__

#include <fstream>
#include <string>
<<<<<<< HEAD:include/gp_filesink.h
#include "gp_beader.h"
=======
#include "bead.h"
>>>>>>> change IModule to IBead:include/gp_file.h
#include "gp_data.h"

namespace GPlayer {

<<<<<<< HEAD:include/gp_filesink.h
class GPFileSink : public IBeader {
=======
class GPFile : public IBead {
>>>>>>> change IModule to IBead:include/gp_file.h
private:
    GPFileSink();

public:
    GPFileSink(std::string filename);
    ~GPFileSink();
    std::string GetInfo() const;
<<<<<<< HEAD:include/gp_filesink.h
    bool HasProc() override { return false; };
=======
    void AddHandler(IBead* module);
>>>>>>> change IModule to IBead:include/gp_file.h
    void Process(GPData* data);

private:
    std::string filepath_;
    std::ofstream* outfile_;
};

}  // namespace GPlayer

#endif  // __GP_FILESINK__
