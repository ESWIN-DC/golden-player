
#include <spdlog/common.h>

#include "gplayer.h"

using namespace GPlayer;

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::trace);

    int ret = 0;
    std::shared_ptr<VideoDecodeContext_T> dcontext =
        std::make_shared<VideoDecodeContext_T>();

    std::shared_ptr<GPNvVideoDecoder> nvvideodecoder =
        std::make_shared<GPNvVideoDecoder>(dcontext);
    std::shared_ptr<GPFileSrc> h264fileSrc = std::make_shared<GPFileSrc>(
        std::string("sample_outdoor_car_1080p_10fps.h264"));
    std::shared_ptr<GPFileSink> h264file =
        std::make_shared<GPFileSink>(std::string("try001.h264"));

    std::shared_ptr<GPPipeline> pipeline = std::make_shared<GPPipeline>();
    std::vector<std::shared_ptr<GPlayer::IBeader> > elements{
        h264fileSrc, nvvideodecoder, h264file};
    pipeline->Add(elements);

    h264fileSrc->Link(nvvideodecoder);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            std::shared_ptr<GPDisplayEGL> egl =
                std::make_shared<GPDisplayEGL>();
            egl->Initialize(30, false, 640 * i, 480 * j, 640, 480);
            pipeline->Add(egl);
            nvvideodecoder->Link(egl);
        }
    }

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
