
#include <fstream>
#include <iostream>

#include "gplayer.h"

using namespace GPlayer;

int main(int argc, char* argv[])
{
    int ret = 0;

    std::shared_ptr<GPNvVideoDecoder> nvvideodecoder =
        std::make_shared<GPNvVideoDecoder>();
    std::shared_ptr<GPCameraV4l2> v4l2 = std::make_shared<GPCameraV4l2>();
    std::shared_ptr<GPDisplayEGLSink> egl =
        std::make_shared<GPDisplayEGLSink>();
    std::shared_ptr<GPFileSink> h264file =
        std::make_shared<GPFileSink>(std::string("try001.h264"));

    std::shared_ptr<GPPipeline> pipeline = std::make_shared<GPPipeline>();
    pipeline->AddMany(v4l2, h264file, nvvideodecoder, egl);

    v4l2->LoadConfiguration("camera-v4l2.json");

    v4l2->Link(h264file);
    v4l2->Link(nvvideodecoder);
    nvvideodecoder->Link(egl);

    ret = pipeline->Run();

    for (;;) {
        GPMessage msg;
        if (!pipeline->GetMessage(&msg)) {
            continue;
        }

        if (msg.type == GPMessageType::ERROR) {
            break;
        }
        else if (msg.type == GPMessageType::STATE_CHANGED) {
        }
        else {
        }
    };

    return ret;
}
