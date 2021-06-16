
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
    shared_ptr<context_t> context = std::make_shared<context_t>();
    VideoDecoder* decoder = new VideoDecoder;

    ret = decoder->decode_proc(argc, argv);

    if (ret) {
        cout << "App run failed" << endl;
    }
    else {
        cout << "App run was successful" << endl;
    }

    return ret;
}
