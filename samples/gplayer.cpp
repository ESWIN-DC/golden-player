
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
    shared_ptr<VideoDecodeContext_T> dcontext =
        std::make_shared<VideoDecodeContext_T>();
    shared_ptr<VideoEncodeContext_T> econtext =
        std::make_shared<VideoEncodeContext_T>();
    shared_ptr<VideoDecoder> decoder = std::make_shared<VideoDecoder>(dcontext);
    shared_ptr<VideoEncoder> encoder = std::make_shared<VideoEncoder>(econtext);
    shared_ptr<CameraRecorder> recorder = std::make_shared<CameraRecorder>();
    shared_ptr<GPNVJpegDecoder> nvjpegdecoder =
        std::make_shared<GPNVJpegDecoder>();
    shared_ptr<CameraV4l2> v4l2 = std::make_shared<CameraV4l2>();
    shared_ptr<GPDisplayEGL> egl = std::make_shared<GPDisplayEGL>();
    shared_ptr<GPFileSink> file =
        std::make_shared<GPFileSink>(std::string("try001.h264"));

    encoder->AddBeader(file.get());
    v4l2->AddBeader(nvjpegdecoder.get());
    v4l2->AddBeader(egl.get());

    v4l2->AddBeader(encoder.get());

    // ret = decoder->decode_proc(argc, argv);
    // ret = encoder->encode_proc(argc, argv);
    // recorder->main(argc, argv);

    ret = v4l2->main(argc, argv);

    Pipeline* pipeline = new Pipeline();
    pipeline->Add(v4l2);
    pipeline->Add(recorder);
    pipeline->Add(encoder);
    pipeline->Add(decoder);

    // pipeline->Run();

    if (ret) {
        spdlog::info("App run failed\n");
    }
    else {
        spdlog::info("App run was successful\n");
    }

    delete pipeline;

    return ret;
}
