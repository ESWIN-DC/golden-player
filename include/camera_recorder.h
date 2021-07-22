#ifndef __CAMERA_RECORDER__
#define __CAMERA_RECORDER__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "gp_beader.h"

namespace GPlayer {

class CameraRecorder : public IBeader {
public:
    std::string GetInfo() const;
    bool HasProc() override { return false; };
    void Process(GPData* data);
    bool Execute();

    void printHelp();

    bool parseCmdline(int argc, char** argv);

    int main(int argc, char* argv[]);
};

}  // namespace GPlayer

#endif  // __CAMERA_RECORDER__
