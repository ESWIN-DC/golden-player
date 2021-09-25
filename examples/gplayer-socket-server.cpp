
#include "gplayer.h"

using namespace GPlayer;

int main(int argc, char* argv[])
{
    int ret = 0;

    std::shared_ptr<GPFileSink> h264file =
        std::make_shared<GPFileSink>(std::string("try001.h264"));

    std::shared_ptr<GPPipeline> pipeline = std::make_shared<GPPipeline>();

    uint32_t column = 8;
    uint32_t row = 4;

    uint32_t w = 1920 / column;
    uint32_t h = 1080 / row;

    std::shared_ptr<GPMediaServer> socket_server =
        std::make_shared<GPMediaServer>(8888);
    std::shared_ptr<GPNvVideoDecoder> nvvideodecoder =
        std::make_shared<GPNvVideoDecoder>();
    std::shared_ptr<GPDisplayEGLSink> egl =
        std::make_shared<GPDisplayEGLSink>();
    egl->Initialize(30, false, 0, 0, w, h);
    pipeline->Add(socket_server);
    pipeline->Add(nvvideodecoder);
    pipeline->Add(egl);
    socket_server->Link(nvvideodecoder);
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
