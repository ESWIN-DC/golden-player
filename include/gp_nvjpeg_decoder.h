#ifndef __GPNVJPEG_DECODER__
#define __GPNVJPEG_DECODER__

#include "gp_beader.h"

#include "NvJpegDecoder.h"

namespace GPlayer {
class GPNvJpegDecoder : public IBeader {
public:
    explicit GPNvJpegDecoder()
    {
        SetType(BeaderType::NvJpegDecoder);
        jpegdec_ = NvJPEGDecoder::createJPEGDecoder("jpegdec");
    }
    std::string GetInfo() const override { return "NVJpegDecoder"; }
    bool HasProc() override { return false; };

    // bool prepare_buffers_mjpeg(uint32_t width, uint32_t height)
    // {
    //     NvBufferCreateParams input_params = {0};

    //     // Allocate global buffer context
    //     ctx->g_buff = (nv_buffer*)malloc(V4L2_BUFFERS_NUM *
    //     sizeof(nv_buffer)); if (ctx->g_buff == NULL)
    //         ERROR_RETURN("Failed to allocate global buffer context");
    //     memset(ctx->g_buff, 0, V4L2_BUFFERS_NUM * sizeof(nv_buffer));

    //     input_params.payloadType = NvBufferPayload_SurfArray;
    //     input_params.width = width;
    //     input_params.height = height;
    //     input_params.layout = NvBufferLayout_Pitch;
    //     input_params.colorFormat =
    //     get_nvbuff_color_fmt(V4L2_PIX_FMT_YUV420M); input_params.nvbuf_tag =
    //     NvBufferTag_NONE;
    //     // Create Render buffer
    //     if (-1 == NvBufferCreateEx(&ctx->render_dmabuf_fd, &input_params))
    //         ERROR_RETURN("Failed to create NvBuffer");

    //     ctx->capture_dmabuf = false;
    //     if (!request_camera_buff_mmap(ctx))
    //         ERROR_RETURN("Failed to set up camera buff");

    //     INFO("Succeed in preparing mjpeg buffers");
    //     return true;
    // }

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