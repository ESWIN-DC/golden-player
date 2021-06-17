
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "gplayer.h"

using namespace GPlayer;

int main(int argc, char* argv[])
{
    int ret = 0;
    shared_ptr<VideoDecodeContext_T> dcontext =
        std::make_shared<VideoDecodeContext_T>();
    shared_ptr<VideoEncodeContext_T> econtext =
        std::make_shared<VideoEncodeContext_T>();
    shared_ptr<VideoDecoder> decoder = std::make_shared<VideoDecoder>(dcontext);
    shared_ptr<VideoEncoder> encoder = std::make_shared<VideoEncoder>(econtext);
    shared_ptr<CameraRecorder> recorder = std::make_shared<CameraRecorder>();

    ret = decoder->decode_proc(argc, argv);
    ret = encoder->encode_proc(argc, argv);

    recorder->parseCmdline(argc, argv);
    recorder->Execute();

    Pipeline* pipeline = new Pipeline();
    pipeline->Add(recorder);
    pipeline->Add(encoder);
    pipeline->Add(decoder);
    pipeline->Run();

    if (ret) {
        cout << "App run failed" << endl;
    }
    else {
        cout << "App run was successful" << endl;
    }

    delete pipeline;

    return ret;
}
