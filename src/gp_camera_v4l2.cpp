

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include "nlohmann/json.hpp"

#include "gp_camera_v4l2.h"
#include "gp_log.h"
namespace GPlayer {

#define MJPEG_EOS_SEARCH_SIZE 4096

static bool quit = false;

using namespace std;

GPCameraV4l2::GPCameraV4l2()
{
    SetType(BeaderType::CameraV4l2Src);

    nvcolor_fmt_ = {
        // TODO: add more pixel format mapping
        {"UYVY", V4L2_PIX_FMT_UYVY, NvBufferColorFormat_UYVY},
        {"VYUY", V4L2_PIX_FMT_VYUY, NvBufferColorFormat_VYUY},
        {"YUYV", V4L2_PIX_FMT_YUYV, NvBufferColorFormat_YUYV},
        {"YVYU", V4L2_PIX_FMT_YVYU, NvBufferColorFormat_YVYU},
        {"GREY", V4L2_PIX_FMT_GREY, NvBufferColorFormat_GRAY8},
        {"YUV420M", V4L2_PIX_FMT_YUV420M, NvBufferColorFormat_YUV420},
        {"MJPEG", V4L2_PIX_FMT_MJPEG, NvBufferColorFormat_YUV420},
        {"H264", V4L2_PIX_FMT_H264, NvBufferColorFormat_YUV420},
        {"H265", V4L2_PIX_FMT_H265, NvBufferColorFormat_YUV420},
        {"VP8", V4L2_PIX_FMT_VP8, NvBufferColorFormat_YUV420},
        {"VP9", V4L2_PIX_FMT_VP9, NvBufferColorFormat_YUV420},
        {"MPEG2", V4L2_PIX_FMT_MPEG2, NvBufferColorFormat_YUV420},
        {"MPEG4", V4L2_PIX_FMT_MPEG4, NvBufferColorFormat_YUV420},
        {"unknown", V4L2_PIX_FMT_YUYV, NvBufferColorFormat_YUV420},  // default
    };

    set_defaults();
}

std::string GPCameraV4l2::GetInfo() const
{
    return "CameraV4l2";
}

void GPCameraV4l2::print_usage(void)
{
    printf(
        "\n\tUsage: camera_v4l2_cuda [OPTIONS]\n\n"
        "\tExample: \n"
        "\t./camera_v4l2_cuda -d /dev/video0 -s 640x480 -f YUYV -n 30 "
        "-c\n\n"
        "\tSupported options:\n"
        "\t-d\t\tSet V4l2 video device node\n"
        "\t-s\t\tSet output resolution of video device\n"
        "\t-f\t\tSet output pixel format of video device (supports only "
        "YUYV/YVYU/UYVY/VYUY/GREY/MJPEG)\n"
        "\t-r\t\tSet renderer frame rate (30 fps by default)\n"
        "\t-n\t\tSave the n-th frame before VIC processing\n"
        "\t-c\t\tEnable CUDA aglorithm (draw a black box in the upper left "
        "corner)\n"
        "\t-v\t\tEnable verbose message\n"
        "\t-h\t\tPrint this usage\n\n"
        "\tNOTE: It runs infinitely until you terminate it with "
        "<ctrl+c>\n");
}

bool GPCameraV4l2::parse_cmdline(v4l2_context_t* ctx, int argc, char** argv)
{
    int c;

    if (argc < 2) {
        print_usage();
        exit(EXIT_SUCCESS);
    }

    while ((c = getopt(argc, argv, "d:s:f:r:n:cvh")) != -1) {
        switch (c) {
            case 'd':
                ctx->cam_devname = optarg;
                break;
            case 's':
                if (sscanf(optarg, "%dx%d", &ctx->cam_w, &ctx->cam_h) != 2) {
                    print_usage();
                    return false;
                }
                break;
            case 'f':
                if (strcmp(optarg, "YUYV") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_YUYV;
                else if (strcmp(optarg, "YVYU") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_YVYU;
                else if (strcmp(optarg, "VYUY") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_VYUY;
                else if (strcmp(optarg, "UYVY") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_UYVY;
                else if (strcmp(optarg, "GREY") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_GREY;
                else if (strcmp(optarg, "MJPEG") == 0)
                    ctx->cam_pixfmt = V4L2_PIX_FMT_MJPEG;
                else {
                    print_usage();
                    return false;
                }
                sprintf(ctx->cam_file, "camera.%s", optarg);
                break;
            case 'r':
                ctx->fps = strtol(optarg, NULL, 10);
                break;
            case 'n':
                ctx->save_n_frame = strtol(optarg, NULL, 10);
                break;
            case 'c':
                ctx->enable_cuda = true;
                break;
            case 'v':
                ctx->enable_verbose = true;
                break;
            case 'h':
                print_usage();
                exit(EXIT_SUCCESS);
                break;
            default:
                print_usage();
                return false;
        }
    }

    return true;
}

void GPCameraV4l2::set_defaults()
{
    v4l2_context_t* ctx = &ctx_;
    memset(ctx, 0, sizeof(v4l2_context_t));

    ctx->cam_devname = "/dev/video0";
    ctx->cam_fd = -1;
    ctx->cam_pixfmt = V4L2_PIX_FMT_YUYV;
    ctx->cam_w = 640;
    ctx->cam_h = 480;
    ctx->frame = 0;
    ctx->save_n_frame = 0;
    ctx->g_buff = NULL;
    ctx->capture_dmabuf = true;
    ctx->fps = 30;
    ctx->enable_cuda = false;
    ctx->enable_verbose = false;
}

const nv_color_fmt* GPCameraV4l2::get_nvbuff_color_fmt(int v4l2_pixfmt)
{
    unsigned i;

    for (i = 0; i < nvcolor_fmt_.size(); i++) {
        if (v4l2_pixfmt == nvcolor_fmt_[i].v4l2_pixfmt)
            return &nvcolor_fmt_[i];
    }

    return NULL;
}

const nv_color_fmt* GPCameraV4l2::get_nvbuff_color_fmt(const char* fmtstr)
{
    unsigned i;

    for (i = 0; i < nvcolor_fmt_.size(); i++) {
        if (std::strcmp(fmtstr, nvcolor_fmt_[i].name) == 0) {
            return &nvcolor_fmt_[i];
        }
    }

    return NULL;
}

bool GPCameraV4l2::save_frame_to_file(v4l2_context_t* ctx,
                                      struct v4l2_buffer* buf)
{
    int file;

    file = open(ctx->cam_file, O_CREAT | O_WRONLY | O_APPEND | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    if (-1 == file)
        ERROR_RETURN("Failed to open file for frame saving");

    if (-1 == write(file, ctx->g_buff[buf->index].start,
                    ctx->g_buff[buf->index].size)) {
        close(file);
        ERROR_RETURN("Failed to write frame into file");
    }

    close(file);

    return true;
}

bool GPCameraV4l2::nvbuff_do_clearchroma(int dmabuf_fd)
{
    NvBufferParams params = {0};
    void* sBaseAddr[3] = {NULL};
    int ret = 0;
    int size;
    unsigned i;

    ret = NvBufferGetParams(dmabuf_fd, &params);
    if (ret != 0)
        ERROR_RETURN("{}: NvBufferGetParams Failed \n", __func__);

    for (i = 1; i < params.num_planes; i++) {
        ret =
            NvBufferMemMap(dmabuf_fd, i, NvBufferMem_Read_Write, &sBaseAddr[i]);
        if (ret != 0)
            ERROR_RETURN("{}: NvBufferMemMap Failed \n", __func__);

        ret = NvBufferMemSyncForCpu(dmabuf_fd, i, &sBaseAddr[i]);
        if (ret != 0)
            ERROR_RETURN("{}: NvBufferMemSyncForCpu Failed \n", __func__);

        size = params.height[i] * params.pitch[i];
        memset(sBaseAddr[i], 0x80, size);

        ret = NvBufferMemSyncForDevice(dmabuf_fd, i, &sBaseAddr[i]);
        if (ret != 0)
            ERROR_RETURN("{}: NvBufferMemSyncForDevice Failed \n", __func__);

        ret = NvBufferMemUnMap(dmabuf_fd, i, &sBaseAddr[i]);
        if (ret != 0)
            ERROR_RETURN("{}: NvBufferMemUnMap Failed \n", __func__);
    }

    return true;
}

bool GPCameraV4l2::camera_initialize(v4l2_context_t* ctx)
{
    struct v4l2_format fmt;

    // Open camera device
    ctx->cam_fd = open(ctx->cam_devname.c_str(), O_RDWR);
    if (ctx->cam_fd == -1)
        ERROR_RETURN("Failed to open camera device {}: {} ({:d})",
                     ctx->cam_devname, strerror(errno), errno);

    // Set camera output format
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = ctx->cam_w;
    fmt.fmt.pix.height = ctx->cam_h;
    fmt.fmt.pix.pixelformat = ctx->cam_pixfmt;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if (ioctl(ctx->cam_fd, VIDIOC_S_FMT, &fmt) < 0)
        ERROR_RETURN("Failed to set camera output format: {} ({:d})",
                     strerror(errno), errno);

    // Get the real format in case the desired is not supported
    memset(&fmt, 0, sizeof fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->cam_fd, VIDIOC_G_FMT, &fmt) < 0)
        ERROR_RETURN("Failed to get camera output format: {} ({:d})",
                     strerror(errno), errno);
    if (fmt.fmt.pix.width != ctx->cam_w || fmt.fmt.pix.height != ctx->cam_h ||
        fmt.fmt.pix.pixelformat != ctx->cam_pixfmt) {
        WARN("The desired format is not supported");
        ctx->cam_w = fmt.fmt.pix.width;
        ctx->cam_h = fmt.fmt.pix.height;
        ctx->cam_pixfmt = fmt.fmt.pix.pixelformat;
    }

    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0x00, sizeof(struct v4l2_streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(ctx->cam_fd, VIDIOC_G_PARM, &streamparm);

    INFO(
        "Camera ouput format: ({:d} x {:d})  stride: {:d}, imagesize: {:d}, "
        "frate: {} / {}}",
        fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.bytesperline,
        fmt.fmt.pix.sizeimage, streamparm.parm.capture.timeperframe.denominator,
        streamparm.parm.capture.timeperframe.numerator);

    return true;
}

bool GPCameraV4l2::init_components(v4l2_context_t* ctx)
{
    GPDisplayEGL* display = dynamic_cast<GPDisplayEGL*>(
        GetBeader(BeaderType::EGLDisplaySink).get());

    if (!camera_initialize(ctx))
        ERROR_RETURN("Failed to initialize camera device");

    if (!display) {
        SPDLOG_TRACE("No display found");
    }

    if (display && !display->Initialize(ctx->fps, ctx->enable_cuda, ctx->cam_w,
                                        ctx->cam_h)) {
        SPDLOG_ERROR("Failed to initialize display");
    }

    SPDLOG_TRACE("Initialize v4l2 components successfully");
    return true;
}

bool GPCameraV4l2::request_camera_buff(v4l2_context_t* ctx)
{
    // Request camera v4l2 buffer
    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof(rb));
    rb.count = V4L2_BUFFERS_NUM;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_DMABUF;
    if (ioctl(ctx->cam_fd, VIDIOC_REQBUFS, &rb) < 0)
        ERROR_RETURN("Failed to request v4l2 buffers: {} ({:d})",
                     strerror(errno), errno);
    if (rb.count != V4L2_BUFFERS_NUM)
        ERROR_RETURN("V4l2 buffer number is not as desired");

    for (unsigned int index = 0; index < V4L2_BUFFERS_NUM; index++) {
        struct v4l2_buffer buf;

        // Query camera v4l2 buf length
        memset(&buf, 0, sizeof buf);
        buf.index = index;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_DMABUF;

        if (ioctl(ctx->cam_fd, VIDIOC_QUERYBUF, &buf) < 0)
            ERROR_RETURN("Failed to query buff: {} ({:d})", strerror(errno),
                         errno);

        // TODO add support for multi-planer
        // Enqueue empty v4l2 buff into camera capture plane
        buf.m.fd = (unsigned long)ctx->g_buff[index].dmabuff_fd;
        if (buf.length != ctx->g_buff[index].size) {
            SPDLOG_TRACE("Camera v4l2 buf length is not expected");
            ctx->g_buff[index].size = buf.length;
        }

        if (ioctl(ctx->cam_fd, VIDIOC_QBUF, &buf) < 0)
            ERROR_RETURN("Failed to enqueue buffers: {} ({:d})",
                         strerror(errno), errno);
    }

    return true;
}

bool GPCameraV4l2::request_camera_buff_mmap(v4l2_context_t* ctx)
{
    // Request camera v4l2 buffer
    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof(rb));
    rb.count = V4L2_BUFFERS_NUM;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx->cam_fd, VIDIOC_REQBUFS, &rb) < 0)
        ERROR_RETURN("Failed to request v4l2 buffers: {} ({:d})",
                     strerror(errno), errno);
    if (rb.count != V4L2_BUFFERS_NUM)
        ERROR_RETURN("V4l2 buffer number is not as desired");

    for (unsigned int index = 0; index < V4L2_BUFFERS_NUM; index++) {
        struct v4l2_buffer buf;

        // Query camera v4l2 buf length
        memset(&buf, 0, sizeof buf);
        buf.index = index;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->cam_fd, VIDIOC_QUERYBUF, &buf) < 0)
            ERROR_RETURN("Failed to query buff: {} ({:d})", strerror(errno),
                         errno);

        ctx->g_buff[index].size = buf.length;
        ctx->g_buff[index].start = (unsigned char*)mmap(
            NULL /* start anywhere */, buf.length,
            PROT_READ | PROT_WRITE /* required */, MAP_SHARED /* recommended */,
            ctx->cam_fd, buf.m.offset);
        if (MAP_FAILED == ctx->g_buff[index].start)
            ERROR_RETURN("Failed to map buffers");

        if (ioctl(ctx->cam_fd, VIDIOC_QBUF, &buf) < 0)
            ERROR_RETURN("Failed to enqueue buffers: {} ({:d})",
                         strerror(errno), errno);
    }

    return true;
}

bool GPCameraV4l2::prepare_buffers_mjpeg(v4l2_context_t* ctx)
{
    NvBufferCreateParams input_params = {0};

    // Allocate global buffer context
    ctx->g_buff = (nv_buffer*)malloc(V4L2_BUFFERS_NUM * sizeof(nv_buffer));
    if (ctx->g_buff == NULL)
        ERROR_RETURN("Failed to allocate global buffer context");
    memset(ctx->g_buff, 0, V4L2_BUFFERS_NUM * sizeof(nv_buffer));

    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.width = ctx->cam_w;
    input_params.height = ctx->cam_h;
    input_params.layout = NvBufferLayout_Pitch;
    const nv_color_fmt* fmt = get_nvbuff_color_fmt(V4L2_PIX_FMT_YUV420M);
    if (fmt == NULL) {
        input_params.colorFormat = fmt->nvbuff_color;
    }
    input_params.nvbuf_tag = NvBufferTag_NONE;
    // Create Render buffer
    if (-1 == NvBufferCreateEx(&ctx->render_dmabuf_fd, &input_params))
        ERROR_RETURN("Failed to create NvBuffer");

    ctx->capture_dmabuf = false;
    if (!request_camera_buff_mmap(ctx))
        ERROR_RETURN("Failed to set up camera buff");

    INFO("Succeed in preparing mjpeg buffers");
    return true;
}

bool GPCameraV4l2::prepare_buffers(v4l2_context_t* ctx)
{
    NvBufferCreateParams input_params = {0};

    // Allocate global buffer context
    ctx->g_buff = (nv_buffer*)malloc(V4L2_BUFFERS_NUM * sizeof(nv_buffer));
    if (ctx->g_buff == NULL)
        ERROR_RETURN("Failed to allocate global buffer context");

    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.width = ctx->cam_w;
    input_params.height = ctx->cam_h;
    input_params.layout = NvBufferLayout_Pitch;

    // Create buffer and provide it with camera
    for (unsigned int index = 0; index < V4L2_BUFFERS_NUM; index++) {
        int fd;
        NvBufferParams params = {0};

        input_params.colorFormat =
            get_nvbuff_color_fmt(ctx->cam_pixfmt)->nvbuff_color;

        input_params.nvbuf_tag = NvBufferTag_CAMERA;
        if (-1 == NvBufferCreateEx(&fd, &input_params))
            ERROR_RETURN("Failed to create NvBuffer");

        ctx->g_buff[index].dmabuff_fd = fd;

        if (-1 == NvBufferGetParams(fd, &params))
            ERROR_RETURN("Failed to get NvBuffer parameters");

        if (ctx->cam_pixfmt == V4L2_PIX_FMT_GREY &&
            params.pitch[0] != params.width[0])
            ctx->capture_dmabuf = false;

        // TODO add multi-planar support
        // Currently it supports only YUV422 interlaced single-planar
        if (ctx->capture_dmabuf) {
            if (-1 == NvBufferMemMap(ctx->g_buff[index].dmabuff_fd, 0,
                                     NvBufferMem_Read_Write,
                                     (void**)&ctx->g_buff[index].start))
                ERROR_RETURN("Failed to map buffer");
        }
    }

    input_params.colorFormat =
        get_nvbuff_color_fmt(V4L2_PIX_FMT_YUV420M)->nvbuff_color;
    input_params.nvbuf_tag = NvBufferTag_NONE;
    // Create Render buffer
    if (-1 == NvBufferCreateEx(&ctx->render_dmabuf_fd, &input_params))
        ERROR_RETURN("Failed to create NvBuffer");

    if (ctx->capture_dmabuf) {
        if (!request_camera_buff(ctx))
            ERROR_RETURN("Failed to set up camera buff");
    }
    else {
        if (!request_camera_buff_mmap(ctx))
            ERROR_RETURN("Failed to set up camera buff");
    }

    INFO("Succeed in preparing stream buffers");
    return true;
}

bool GPCameraV4l2::start_stream(v4l2_context_t* ctx)
{
    enum v4l2_buf_type type;

    // Start v4l2 streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->cam_fd, VIDIOC_STREAMON, &type) < 0)
        ERROR_RETURN("Failed to start streaming: {} ({:d})", strerror(errno),
                     errno);

    usleep(200);

    INFO("Camera video streaming on ...");
    return true;
}

void GPCameraV4l2::signal_handle(int signum)
{
    printf("Quit due to exit command from user!\n");
    quit = true;
}

bool GPCameraV4l2::start_capture(v4l2_context_t* ctx)
{
    struct sigaction sig_action;
    struct pollfd fds[1];
    NvBufferTransformParams transParams;
    GPNvJpegDecoder* jpeg_decoder = dynamic_cast<GPNvJpegDecoder*>(
        GetBeader(BeaderType::NvJpegDecoder).get());
    GPDisplayEGL* display = dynamic_cast<GPDisplayEGL*>(
        GetBeader(BeaderType::EGLDisplaySink).get());

    // Ensure a clean shutdown if user types <ctrl+c>
    sig_action.sa_handler = signal_handle;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = 0;
    sigaction(SIGINT, &sig_action, NULL);

    SPDLOG_TRACE("pixel format = {0:x}", ctx->cam_pixfmt);

    struct v4l2_fmtdesc fmtdesc;
    if (ioctl(ctx->cam_fd, ctx->cam_pixfmt, &fmtdesc) == 0) {
        SPDLOG_DEBUG("pixel format = {0:x} => {}", ctx->cam_pixfmt,
                     std::string((char*)fmtdesc.description),
                     sizeof(fmtdesc.description));
    }
    else {
        SPDLOG_WARN("pixel format = {0:x}, desc = NULL", ctx->cam_pixfmt);
    }

    if (ctx->cam_pixfmt == V4L2_PIX_FMT_MJPEG) {
        // TODO: v0.2, create from registry
        // registry->New("jpegdec");
    }

    // Init the NvBufferTransformParams
    memset(&transParams, 0, sizeof(transParams));
    transParams.transform_flag = NVBUFFER_TRANSFORM_FILTER;
    transParams.transform_filter = NvBufferTransform_Filter_Smart;

    // Enable render profiling information
    if (display) {
        display->enableProfiling();
    }

    fds[0].fd = ctx->cam_fd;
    fds[0].events = POLLIN;
    while (poll(fds, 1, 5000) > 0 && !quit) {
        if (fds[0].revents & POLLIN) {
            struct v4l2_buffer v4l2_buf;
            uint8_t* pbuf;
            size_t bufsize;

            // Dequeue camera buff
            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ctx->capture_dmabuf)
                v4l2_buf.memory = V4L2_MEMORY_DMABUF;
            else
                v4l2_buf.memory = V4L2_MEMORY_MMAP;
            if (ioctl(ctx->cam_fd, VIDIOC_DQBUF, &v4l2_buf) < 0)
                ERROR_RETURN("Failed to dequeue camera buff: {} ({:d})",
                             strerror(errno), errno);

            pbuf = ctx->g_buff[v4l2_buf.index].start;
            bufsize = v4l2_buf.bytesused;

            ctx->frame++;

            if (ctx->frame == ctx->save_n_frame)
                save_frame_to_file(ctx, &v4l2_buf);

            GPFileSink* buffer_handler = dynamic_cast<GPFileSink*>(
                GetBeader(BeaderType::FileSink).get());
            if (buffer_handler) {
                GPBuffer gpbuffer(pbuf, bufsize);
                GPData data(&gpbuffer);
                buffer_handler->Process(&data);
            }

            if (ctx->cam_pixfmt == V4L2_PIX_FMT_MJPEG) {
                int fd = 0;
                uint32_t width, height, pixfmt;  // out parameters
                unsigned int i = 0;
                unsigned int eos_search_size = MJPEG_EOS_SEARCH_SIZE;
                unsigned int bytesused = bufsize;
                uint8_t* p;

                // v4l2_buf.bytesused may have padding bytes for alignment
                // Search for EOF to get exact size
                if (eos_search_size > bytesused)
                    eos_search_size = bytesused;
                for (i = 0; i < eos_search_size; i++) {
                    p = (uint8_t*)(pbuf + bytesused);
                    if ((*(p - 2) == 0xff) && (*(p - 1) == 0xd9)) {
                        break;
                    }
                    bytesused--;
                }

                if (!jpeg_decoder) {
                    SPDLOG_TRACE("No found MJPEG decoder in this beader.");
                }
                else {
                    if (jpeg_decoder &&
                        jpeg_decoder->decodeToFd(fd, pbuf, bytesused, pixfmt,
                                                 width, height) < 0) {
                        SPDLOG_ERROR("Cannot decode MJPEG: jpeg_decoder={:p}",
                                     static_cast<void*>(jpeg_decoder));
                        return false;
                    }

                    // Convert the camera buffer to YUV420P
                    if (-1 == NvBufferTransform(fd, ctx->render_dmabuf_fd,
                                                &transParams))
                        ERROR_RETURN("Failed to convert the buffer");
                }
            }
            else if (ctx->cam_pixfmt == V4L2_PIX_FMT_H264 ||
                     ctx->cam_pixfmt == V4L2_PIX_FMT_H265 ||
                     ctx->cam_pixfmt == V4L2_PIX_FMT_VP8 ||
                     ctx->cam_pixfmt == V4L2_PIX_FMT_VP9 ||
                     ctx->cam_pixfmt == V4L2_PIX_FMT_MPEG2 ||
                     ctx->cam_pixfmt == V4L2_PIX_FMT_MPEG4) {
                IBeader* decoder = GetBeader(BeaderType::NvVideoDecoder).get();
                if (decoder) {
                    GPBuffer gpbuffer(pbuf, bufsize);
                    GPData data(&gpbuffer);
                    GPNvVideoDecoder* video_decoder =
                        dynamic_cast<GPNvVideoDecoder*>(decoder);
                    video_decoder->Process(&data);
                }
            }
            else {  // raw data
                if (ctx->capture_dmabuf) {
                    // Cache sync for VIC operation
                    NvBufferMemSyncForDevice(
                        ctx->g_buff[v4l2_buf.index].dmabuff_fd, 0,
                        (void**)&ctx->g_buff[v4l2_buf.index].start);
                }
                else {
                    Raw2NvBuffer(ctx->g_buff[v4l2_buf.index].start, 0,
                                 ctx->cam_w, ctx->cam_h,
                                 ctx->g_buff[v4l2_buf.index].dmabuff_fd);
                }

                // Convert the camera buffer from YUV422 to YUV420P
                if (-1 ==
                    NvBufferTransform(ctx->g_buff[v4l2_buf.index].dmabuff_fd,
                                      ctx->render_dmabuf_fd, &transParams))
                    ERROR_RETURN("Failed to convert the buffer");

                if (ctx->cam_pixfmt == V4L2_PIX_FMT_GREY) {
                    if (!nvbuff_do_clearchroma(ctx->render_dmabuf_fd))
                        ERROR_RETURN("Failed to clear chroma");
                }
            }

            // Display the camera buffer
            if (display) {
                display->Display(ctx->enable_cuda, ctx->render_dmabuf_fd);
            }

            // Enqueue camera buff
            if (ioctl(ctx->cam_fd, VIDIOC_QBUF, &v4l2_buf))
                ERROR_RETURN("Failed to queue camera buffers: {} ({:d})",
                             strerror(errno), errno);
        }
    }

    // Print profiling information when streaming stops.
    if (display)
        display->printProfilingStats();

    if (ctx->cam_pixfmt == V4L2_PIX_FMT_MJPEG) {
        // Unlink(jpeg_decoder);
    }

    return true;
}

bool GPCameraV4l2::stop_stream(v4l2_context_t* ctx)
{
    enum v4l2_buf_type type;

    // Stop v4l2 streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->cam_fd, VIDIOC_STREAMOFF, &type))
        ERROR_RETURN("Failed to stop streaming: {} ({:d})", strerror(errno),
                     errno);

    INFO("Camera video streaming off ...");
    return true;
}

int GPCameraV4l2::Proc()
{
    v4l2_context_t& ctx = ctx_;
    int error = 0;

    pthread_setname_np(pthread_self(), "GPCameraV4l2::Proc");

    CHECK_ERROR(init_components(&ctx), cleanup,
                "Failed to initialize v4l2 components");

    if (ctx.cam_pixfmt == V4L2_PIX_FMT_MJPEG) {
        CHECK_ERROR(prepare_buffers_mjpeg(&ctx), cleanup,
                    "Failed to prepare v4l2 buffs");
    }
    else {
        CHECK_ERROR(prepare_buffers(&ctx), cleanup,
                    "Failed to prepare v4l2 buffs");
    }

    CHECK_ERROR(start_stream(&ctx), cleanup, "Failed to start streaming");

    CHECK_ERROR(start_capture(&ctx), cleanup, "Failed to start capturing")

    CHECK_ERROR(stop_stream(&ctx), cleanup, "Failed to stop streaming");

cleanup:
    if (ctx.cam_fd > 0)
        close(ctx.cam_fd);

    // Unlink(BeaderType::EGLDisplaySink);

    if (ctx.g_buff != NULL) {
        for (unsigned i = 0; i < V4L2_BUFFERS_NUM; i++) {
            if (ctx.g_buff[i].dmabuff_fd)
                NvBufferDestroy(ctx.g_buff[i].dmabuff_fd);
            if (ctx.cam_pixfmt == V4L2_PIX_FMT_MJPEG)
                munmap(ctx.g_buff[i].start, ctx.g_buff[i].size);
        }
        free(ctx.g_buff);
    }

    NvBufferDestroy(ctx.render_dmabuf_fd);

    return -error;
}

bool GPCameraV4l2::SaveConfiguration(const std::string& filename)
{
    using nlohmann::json;

    std::ofstream o(filename);
    json j;

    j["device"] = ctx_.cam_devname;
    const nv_color_fmt* fmt = get_nvbuff_color_fmt(ctx_.cam_pixfmt);
    if (fmt == NULL) {
        j["cam_pixfmt"] = "unknown";
    }
    else {
        j["cam_pixfmt"] = fmt->name;
    }

    j["cam_w"] = ctx_.cam_w;
    j["cam_h"] = ctx_.cam_h;
    j["save_n_frame"] = ctx_.save_n_frame;
    j["fps"] = ctx_.fps;
    j["enable_cuda"] = ctx_.enable_cuda;
    j["enable_verbose"] = ctx_.enable_verbose;

    o << j.dump(4);

    return true;
}

bool GPCameraV4l2::LoadConfiguration(const std::string& filename)
{
    using nlohmann::json;

    v4l2_context_t* ctx = &ctx_;
    bool parse_ok = true;
    std::ifstream i(filename);
    json j;

    try {
        i >> j;
    }
    catch (json::parse_error& e) {
        SPDLOG_ERROR("Paser error: {}", e.what());
        parse_ok = false;
        return false;
    }

    if (parse_ok) {
        if (j.contains("device")) {
            ctx->cam_devname = j["device"].get<std::string>();
        }

        if (j.contains("cam_pixfmt")) {
            std::string cam_pixfmt = j["cam_pixfmt"].get<std::string>();
            const nv_color_fmt* fmt = get_nvbuff_color_fmt(cam_pixfmt.c_str());
            ctx->cam_pixfmt = fmt ? fmt->v4l2_pixfmt : V4L2_PIX_FMT_YUYV;
        }

        if (j.contains("cam_w")) {
            ctx->cam_w = j["cam_w"].get<int>();
        }

        if (j.contains("cam_h")) {
            ctx->cam_h = j["cam_h"].get<int>();
        }

        if (j.contains("save_n_frame")) {
            ctx->save_n_frame = j["save_n_frame"].get<int>();
        }

        if (j.contains("fps")) {
            ctx->fps = j["fps"].get<int>();
        }

        if (j.contains("enable_cuda")) {
            ctx->enable_cuda = j["enable_cuda"].get<bool>();
        }

        if (j.contains("enable_verbose")) {
            ctx->enable_verbose = j["enable_verbose"].get<bool>();
        }
    }

    return true;
}

}  // namespace GPlayer
