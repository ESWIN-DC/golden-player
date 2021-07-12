
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <poll.h>
#include <pthread.h>
#include <spdlog/common.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "gplayer.h"

using namespace GPlayer;

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::trace);

    int ret = 0;
    std::shared_ptr<VideoDecodeContext_T> dcontext =
        std::make_shared<VideoDecodeContext_T>();
    std::shared_ptr<VideoEncodeContext_T> econtext =
        std::make_shared<VideoEncodeContext_T>();

    dcontext->decoder_pixfmt = V4L2_PIX_FMT_H264;

    std::shared_ptr<GPNvVideoDecoder> nvvideodecoder =
        std::make_shared<GPNvVideoDecoder>(dcontext);
    std::shared_ptr<GPCameraV4l2> v4l2 = std::make_shared<GPCameraV4l2>();
    std::shared_ptr<GPDisplayEGL> egl = std::make_shared<GPDisplayEGL>();
    std::shared_ptr<GPFileSink> h264file =
        std::make_shared<GPFileSink>(std::string("try001.h264"));

    std::shared_ptr<GPPipeline> pipeline = std::make_shared<GPPipeline>();
    std::vector<std::shared_ptr<GPlayer::IBeader> > elements{
        v4l2, h264file, nvvideodecoder, egl};
    pipeline->Add(elements);

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
