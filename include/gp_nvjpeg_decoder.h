#ifndef __GPNVJPEG_DECODER__
#define __GPNVJPEG_DECODER__

#include "gp_beader.h"

class NvJPEGDecoder;

namespace GPlayer {

class GPNvJpegDecoder : public IBeader {
public:
    explicit GPNvJpegDecoder();
    std::string GetInfo() const override { return "NVJpegDecoder"; }
    bool HasProc() override { return false; };
    int decodeToFd(int& fd,
                   unsigned char* buffer,
                   uint32_t bytesused,
                   uint32_t& pixfmt,
                   uint32_t& width,
                   uint32_t& height);

private:
    // MJPEG decoding
    NvJPEGDecoder* jpegdec_;
};

}  // namespace GPlayer

#endif  // __GPNVJPEG_DECODER__