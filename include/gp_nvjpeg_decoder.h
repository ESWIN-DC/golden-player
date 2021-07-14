#ifndef __GPNVJPEG_DECODER__
#define __GPNVJPEG_DECODER__

#include "gp_beader.h"

#include "NvJpegDecoder.h"

namespace GPlayer {
class GPNvJpegDecoder : public IBeader {
public:
    explicit GPNvJpegDecoder()
    {
        SetProperties("", "", BeaderType::NvJpegDecoder);
        jpegdec_ = NvJPEGDecoder::createJPEGDecoder("jpegdec");
    }
    std::string GetInfo() const override { return "NVJpegDecoder"; }
    bool HasProc() override { return false; };
    int decodeToFd(int& fd,
                   unsigned char* buffer,
                   uint32_t bytesused,
                   uint32_t& pixfmt,
                   uint32_t& width,
                   uint32_t& height)
    {
        return jpegdec_->decodeToFd(fd, buffer, bytesused, pixfmt, width,
                                    height);
    }

private:
    // MJPEG decoding
    NvJPEGDecoder* jpegdec_;
};

}  // namespace GPlayer

#endif  // __GPNVJPEG_DECODER__