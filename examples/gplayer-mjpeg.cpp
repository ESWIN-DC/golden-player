
#include "gplayer.h"

using namespace GPlayer;

int main(int argc, char* argv[])
{
    int ret = 0;
    std::shared_ptr<GPNvJpegDecoder> nvjpegdecoder =
        std::make_shared<GPNvJpegDecoder>();
    std::shared_ptr<GPCameraV4l2> v4l2 = std::make_shared<GPCameraV4l2>();
    std::shared_ptr<GPDisplayEGLSink> egl =
        std::make_shared<GPDisplayEGLSink>();
    std::shared_ptr<GPFileSink> mjpegfile =
        std::make_shared<GPFileSink>(std::string("try001.mjpeg"));

    std::shared_ptr<GPPipeline> pipeline = std::make_shared<GPPipeline>();
    std::vector<std::shared_ptr<GPlayer::IBeader> > elements{
        v4l2, mjpegfile, nvjpegdecoder, egl};
    pipeline->Add(elements);

    v4l2->LoadConfiguration("camera-v4l2.json");

    v4l2->Link(egl);
    v4l2->Link(mjpegfile);
    v4l2->Link(nvjpegdecoder);

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
