#include "NvJpegDecoder.h"

#include "gp_nvjpeg_decoder.h"

namespace GPlayer {

GPNvJpegDecoder::GPNvJpegDecoder()
{
    SetProperties("", "", BeaderType::NvJpegDecoder);
    jpegdec_ = NvJPEGDecoder::createJPEGDecoder("jpegdec");
}

int GPNvJpegDecoder::decodeToFd(int& fd,
                                unsigned char* buffer,
                                uint32_t bytesused,
                                uint32_t& pixfmt,
                                uint32_t& width,
                                uint32_t& height)
{
    return jpegdec_->decodeToFd(fd, buffer, bytesused, pixfmt, width, height);
}

}  // namespace GPlayer