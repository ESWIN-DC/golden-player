
#ifndef __CAMERA_V4L2__
#define __CAMERA_V4L2__

#include <functional>
#include <queue>

#include "NvCudaProc.h"
#include "NvEglRenderer.h"
#include "NvUtils.h"
#include "nvbuf_utils.h"

#include "NvJpegDecoder.h"

#include "beader.h"

#include "nvjpeg_decoder.h"
#include "video_encoder.h"

namespace GPlayer {

#define V4L2_BUFFERS_NUM 4

typedef struct {
    // Hold the user accessible pointer
    unsigned char* start;
    // Hold the memory length
    unsigned int size;
    // Hold the file descriptor of NvBuffer
    int dmabuff_fd;
} nv_buffer;

typedef struct {
    // camera v4l2 context
    std::string cam_devname;
    char cam_file[16];
    int cam_fd;
    unsigned int cam_pixfmt;
    unsigned int cam_w;
    unsigned int cam_h;
    unsigned int frame;
    unsigned int save_n_frame;

    // Global buffer ptr
    nv_buffer* g_buff;
    bool capture_dmabuf;

    // // EGL renderer
    // NvEglRenderer* renderer;
    int render_dmabuf_fd;
    int fps;

    // CUDA processing
    bool enable_cuda;
    // EGLDisplay egl_display;
    // EGLImageKHR egl_image;

    // // MJPEG decoding
    // NvJPEGDecoder* jpegdec;

    // Verbose option
    bool enable_verbose;

} v4l2_context_t;

// Correlate v4l2 pixel format and NvBuffer color format
typedef struct {
    unsigned int v4l2_pixfmt;
    NvBufferColorFormat nvbuff_color;
} nv_color_fmt;

class CameraV4l2 : public IBeader {
private:
    std::vector<nv_color_fmt> nvcolor_fmt_;

public:
    CameraV4l2()
    {
        SetType(CameraV4l2Src);

        nvcolor_fmt_ = {
            // TODO add more pixel format mapping
            {V4L2_PIX_FMT_UYVY, NvBufferColorFormat_UYVY},
            {V4L2_PIX_FMT_VYUY, NvBufferColorFormat_VYUY},
            {V4L2_PIX_FMT_YUYV, NvBufferColorFormat_YUYV},
            {V4L2_PIX_FMT_YVYU, NvBufferColorFormat_YVYU},
            {V4L2_PIX_FMT_GREY, NvBufferColorFormat_GRAY8},
            {V4L2_PIX_FMT_YUV420M, NvBufferColorFormat_YUV420},
        };
    }

    std::string GetInfo() const;
    void Process(GPData* data);
    void print_usage(void);
    bool parse_cmdline(v4l2_context_t* ctx, int argc, char** argv);
    void set_defaults(v4l2_context_t* ctx);
    NvBufferColorFormat get_nvbuff_color_fmt(unsigned int v4l2_pixfmt);
    bool save_frame_to_file(v4l2_context_t* ctx, struct v4l2_buffer* buf);
    bool nvbuff_do_clearchroma(int dmabuf_fd);
    bool camera_initialize(v4l2_context_t* ctx);
    bool init_components(v4l2_context_t* ctx);
    bool request_camera_buff(v4l2_context_t* ctx);
    bool request_camera_buff_mmap(v4l2_context_t* ctx);
    bool prepare_buffers_mjpeg(v4l2_context_t* ctx);
    bool prepare_buffers(v4l2_context_t* ctx);
    bool start_stream(v4l2_context_t* ctx);
    static void signal_handle(int signum);
    bool start_capture(v4l2_context_t* ctx);
    bool stop_stream(v4l2_context_t* ctx);
    // bool ReadFrame(NvBuffer& buffer);
    int main(int argc, char* argv[]);

    int SaveConfiguration(const std::string& configuration);
    int LoadConfiguration();

private:
    v4l2_context_t ctx_;
    GPNVJpegDecoder* jpegdec_;
};

}  // namespace GPlayer

#endif  // __CAMERA_V4L2__