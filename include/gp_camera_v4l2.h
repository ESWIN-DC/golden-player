
#ifndef __CAMERA_V4L2__
#define __CAMERA_V4L2__

#include <functional>
#include <queue>

#include "NvUtils.h"
#include "NvVideoEncoder.h"
#include "nvbuf_utils.h"

#include "gp_beader.h"

#include "gp_nvjpeg_decoder.h"
#include "gp_nvvideo_encoder.h"

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

    // EGL renderer
    int render_dmabuf_fd;
    int fps;

    // CUDA processing
    bool enable_cuda;

    // Verbose option
    bool enable_verbose;

} v4l2_context_t;

// Correlate v4l2 pixel format and NvBuffer color format
typedef struct {
    const char* name;
    int v4l2_pixfmt;
    NvBufferColorFormat nvbuff_color;
} nv_color_fmt;

class GPCameraV4l2 : public IBeader {
private:
    std::vector<nv_color_fmt> nvcolor_fmt_;

public:
    explicit GPCameraV4l2();
    std::string GetInfo() const override;
    bool HasProc() override { return true; };
    int Proc() override;
    void Process(GPData* data);
    bool SaveConfiguration(const std::string& filename);
    bool LoadConfiguration(const std::string& filename);

private:
    void print_usage(void);
    bool parse_cmdline(v4l2_context_t* ctx, int argc, char** argv);
    void set_defaults();
    const nv_color_fmt* get_nvbuff_color_fmt(int v4l2_pixfmt);
    const nv_color_fmt* get_nvbuff_color_fmt(const char* fmtstr);
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

private:
    v4l2_context_t ctx_;
    GPNvJpegDecoder* jpegdec_;
};

}  // namespace GPlayer

#endif  // __CAMERA_V4L2__