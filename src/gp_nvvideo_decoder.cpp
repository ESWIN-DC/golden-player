
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <nvbuf_utils.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <streambuf>

#include "gp_log.h"

#include "NvApplicationProfiler.h"
#include "NvUtils.h"

#include "gplayer.h"

namespace GPlayer {

#define MICROSECOND_UNIT 1000000
#define CHUNK_SIZE 4000000
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define IS_NAL_UNIT_START(buffer_ptr) \
    (!buffer_ptr[0] && !buffer_ptr[1] && !buffer_ptr[2] && (buffer_ptr[3] == 1))

#define IS_NAL_UNIT_START1(buffer_ptr) \
    (!buffer_ptr[0] && !buffer_ptr[1] && (buffer_ptr[2] == 1))

#define H264_NAL_UNIT_CODED_SLICE 1
#define H264_NAL_UNIT_CODED_SLICE_IDR 5

#define HEVC_NUT_TRAIL_N 0
#define HEVC_NUT_RASL_R 9
#define HEVC_NUT_BLA_W_LP 16
#define HEVC_NUT_CRA_NUT 21

#define IVF_FILE_HDR_SIZE 32
#define IVF_FRAME_HDR_SIZE 12

#define IS_H264_NAL_CODED_SLICE(buffer_ptr) \
    ((buffer_ptr[0] & 0x1F) == H264_NAL_UNIT_CODED_SLICE)
#define IS_H264_NAL_CODED_SLICE_IDR(buffer_ptr) \
    ((buffer_ptr[0] & 0x1F) == H264_NAL_UNIT_CODED_SLICE_IDR)

#define GET_H265_NAL_UNIT_TYPE(buffer_ptr) ((buffer_ptr[0] & 0x7E) >> 1)

using namespace std;

GPNvVideoDecoder::GPNvVideoDecoder(
    const shared_ptr<VideoDecodeContext_T> context)
    : buffer_(CHUNK_SIZE * 16)
{
    ctx_ = context;
    SetType(BeaderType::NvVideoDecoder);

    // decode_thread_ = std::thread(decodeProc, this);
}

GPNvVideoDecoder::~GPNvVideoDecoder()
{
    decode_thread_.join();
}

std::string GPNvVideoDecoder::GetInfo() const
{
    return "NvVideoDecoder";
}

void GPNvVideoDecoder::Process(GPData* data)
{
    std::lock_guard<std::mutex> guard(buffer_mutex_);
    GPBuffer* buffer = *data;

    size_t put_size = buffer_.put(buffer->GetData(), buffer->GetLength());
    if (put_size < buffer->GetLength()) {
        SPDLOG_WARN("Buffer full!");
    }

    thread_condition_.notify_one();
}

int GPNvVideoDecoder::read_decoder_input_nalu(NvBuffer* buffer)
{
    std::lock_guard<std::mutex> guard(buffer_mutex_);
    // Length is the size of the buffer in bytes
    char* buffer_ptr = (char*)buffer->planes[0].data;
    int h265_nal_unit_type;
    // char* stream_ptr;
    uint8_t stream_buffer[4];
    bool nalu_found = false;

    if (buffer_.size() == 0) {
        SPDLOG_WARN("No buffers in the {}", GetName());
        return buffer->planes[0].bytesused = 0;
    }

    // Find the first NAL unit in the buffer

    // stream_ptr = parse_buffer;
    // while ((stream_ptr - parse_buffer) < (bytes_read - 3)) {
    while (buffer_.snap(stream_buffer, 4) == 4) {
        nalu_found = IS_NAL_UNIT_START(stream_buffer) ||
                     IS_NAL_UNIT_START1(stream_buffer);
        if (nalu_found) {
            break;
        }
        // stream_ptr++;
        buffer_.drop();
    }

    // Reached end of buffer but could not find NAL unit
    if (!nalu_found) {
        SPDLOG_ERROR(
            "Could not read nal unit from file. EOF or file corrupted");
        return -1;
    }

    // memcpy(buffer_ptr, stream_ptr, 4);
    buffer_.get(reinterpret_cast<uint8_t*>(buffer_ptr), 4);
    buffer_ptr += 4;
    buffer->planes[0].bytesused = 4;
    // stream_ptr += 4;

    if (buffer_.snap(stream_buffer, 1) && ctx_->copy_timestamp) {
        if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H264) {
            if ((IS_H264_NAL_CODED_SLICE(stream_buffer)) ||
                (IS_H264_NAL_CODED_SLICE_IDR(stream_buffer)))
                ctx_->flag_copyts = true;
            else
                ctx_->flag_copyts = false;
        }
        else if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H265) {
            h265_nal_unit_type = GET_H265_NAL_UNIT_TYPE(stream_buffer);
            if ((h265_nal_unit_type >= HEVC_NUT_TRAIL_N &&
                 h265_nal_unit_type <= HEVC_NUT_RASL_R) ||
                (h265_nal_unit_type >= HEVC_NUT_BLA_W_LP &&
                 h265_nal_unit_type <= HEVC_NUT_CRA_NUT))
                ctx_->flag_copyts = true;
            else
                ctx_->flag_copyts = false;
        }
    }

    // Copy bytes till the next NAL unit is found
    // while ((stream_ptr - parse_buffer) < (bytes_read - 3)) {
    while (buffer_.snap(stream_buffer, 4) == 4) {
        if (IS_NAL_UNIT_START(stream_buffer) ||
            IS_NAL_UNIT_START1(stream_buffer)) {
            // streamsize seekto =
            //     stream_initial_pos + (stream_ptr - parse_buffer);
            // stream->seekg(seekto, stream->beg);
            return 0;
        }
        *buffer_ptr = stream_buffer[0];
        buffer_ptr++;
        // stream_ptr++;
        buffer_.drop();
        buffer->planes[0].bytesused++;
    }

    // Reached end of buffer but could not find NAL unit
    SPDLOG_ERROR("Could not read nal unit from file. EOF or file corrupted");
    return -1;
}

int GPNvVideoDecoder::read_decoder_input_chunk(NvBuffer* buffer)
{
    std::lock_guard<std::mutex> guard(buffer_mutex_);
    // Length is the size of the buffer in bytes
    streamsize bytes_to_read = MIN(CHUNK_SIZE, buffer->planes[0].length);

    if (buffer_.size() == 0) {
        SPDLOG_WARN("No buffers in the {}", GetInfo());
        return 0;
    }

    // stream->read((char*)buffer->planes[0].data, bytes_to_read);

    // It is necessary to set bytesused properly, so that decoder knows how
    // many bytes in the buffer are valid
    buffer->planes[0].bytesused =
        buffer_.get(buffer->planes[0].data, bytes_to_read);
    return 0;
}

int GPNvVideoDecoder::read_vpx_decoder_input_chunk(NvBuffer* buffer)
{
    // ifstream* stream = ctx_->in_file[0];
    size_t bytes_read = 0;
    size_t Framesize;
    unsigned char* bitstreambuffer = (unsigned char*)buffer->planes[0].data;
    if (ctx_->vp9_file_header_flag == 0) {
        // stream->read((char*)buffer->planes[0].data, IVF_FILE_HDR_SIZE);
        bytes_read = buffer_.get(buffer->planes[0].data, IVF_FILE_HDR_SIZE);
        if (bytes_read != IVF_FILE_HDR_SIZE) {
            SPDLOG_ERROR("Couldn't read IVF FILE HEADER");
            return -1;
        }
        if (!((bitstreambuffer[0] == 'D') && (bitstreambuffer[1] == 'K') &&
              (bitstreambuffer[2] == 'I') && (bitstreambuffer[3] == 'F'))) {
            SPDLOG_ERROR("It's not a valid IVF file \n");
            return -1;
        }
        SPDLOG_INFO("It's a valid IVF file");
        ctx_->vp9_file_header_flag = 1;
    }
    // stream->read((char*)buffer->planes[0].data, IVF_FRAME_HDR_SIZE);
    bytes_read = buffer_.get(buffer->planes[0].data, IVF_FRAME_HDR_SIZE);
    if (bytes_read != IVF_FRAME_HDR_SIZE) {
        SPDLOG_ERROR("Couldn't read IVF FRAME HEADER");
        return -1;
    }
    Framesize = (bitstreambuffer[3] << 24) + (bitstreambuffer[2] << 16) +
                (bitstreambuffer[1] << 8) + bitstreambuffer[0];
    buffer->planes[0].bytesused = Framesize;
    // stream->read((char*)buffer->planes[0].data, Framesize);
    bytes_read = buffer_.get(buffer->planes[0].data, Framesize);
    if (bytes_read != Framesize) {
        SPDLOG_ERROR("Couldn't read Framesize");
        return -1;
    }
    return 0;
}

void GPNvVideoDecoder::Abort()
{
    ctx_->got_error = true;
    ctx_->dec->abort();
#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx_->conv) {
        ctx_->conv->abort();
        pthread_cond_broadcast(&ctx_->queue_cond);
    }
#endif
}

#ifndef USE_NVBUF_TRANSFORM_API
bool GPNvVideoDecoder::conv0_output_dqbuf_thread_callback(
    struct v4l2_buffer* v4l2_buf,
    NvBuffer* buffer,
    NvBuffer* shared_buffer,
    void* arg)
{
    context_t* ctx = (context_t*)arg;
    struct v4l2_buffer dec_capture_ret_buffer;
    struct v4l2_plane planes[MAX_PLANES];

    if (!v4l2_buf) {
        SPDLOG_ERROR("Error while dequeueing conv output plane buffer" << endl;
        Abort();
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0) {
        return false;
    }

    memset(&dec_capture_ret_buffer, 0, sizeof(dec_capture_ret_buffer));
    memset(planes, 0, sizeof(planes));

    dec_capture_ret_buffer.index = shared_buffer->index;
    dec_capture_ret_buffer.m.planes = planes;
    if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
        dec_capture_ret_buffer.m.planes[0].m.fd =
            ctx_->dmabuff_fd[shared_buffer->index];

    pthread_mutex_lock(&ctx_->queue_lock);
    ctx_->conv_output_plane_buf_queue->push(buffer);

    // Return the buffer dequeued from converter output plane
    // back to decoder capture plane
    if (ctx_->dec->capture_plane.qBuffer(dec_capture_ret_buffer, NULL) < 0) {
        Abort();
        return false;
    }

    pthread_cond_broadcast(&ctx_->queue_cond);
    pthread_mutex_unlock(&ctx_->queue_lock);

    return true;
}

bool GPNvVideoDecoder::conv0_capture_dqbuf_thread_callback(
    struct v4l2_buffer* v4l2_buf,
    NvBuffer* buffer,
    NvBuffer* shared_buffer,
    void* arg)
{
    context_t* ctx = (context_t*)arg;

    if (!v4l2_buf) {
        SPDLOG_ERROR("Error while dequeueing conv capture plane buffer" << endl;
        Abort();
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0) {
        return false;
    }

    // Write raw video frame to file and return the buffer to converter
    // capture plane
    if (!ctx_->stats && ctx_->out_file) {
        write_video_frame(ctx_->out_file, *buffer);
    }

    if (!ctx_->stats && !ctx_->disable_rendering) {
        ctx_->renderer->render(buffer->planes[0].fd);
    }

    if (ctx_->conv->capture_plane.qBuffer(*v4l2_buf, NULL) < 0) {
        return false;
    }
    return true;
}
#endif

int GPNvVideoDecoder::report_input_metadata(
    v4l2_ctrl_videodec_inputbuf_metadata* input_metadata)
{
    int ret = -1;
    uint32_t frame_num = ctx_->dec->output_plane.getTotalDequeuedBuffers() - 1;

    if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_SPS) {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_SPS " << endl;
    }
    else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_PPS) {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_PPS " << endl;
    }
    else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_SLICE_HDR) {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_SLICE_HDR "
             << endl;
    }
    else if (input_metadata->nBitStreamError &
             V4L2_DEC_ERROR_MISSING_REF_FRAME) {
        cout << "Frame " << frame_num
             << " BitStreamError : ERROR_MISSING_REF_FRAME " << endl;
    }
    else if (input_metadata->nBitStreamError & V4L2_DEC_ERROR_VPS) {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_VPS " << endl;
    }
    else {
        cout << "Frame " << frame_num << " BitStreamError : ERROR_None "
             << endl;
        ret = 0;
    }
    return ret;
}

void GPNvVideoDecoder::report_metadata(
    v4l2_ctrl_videodec_outputbuf_metadata* metadata)
{
    uint32_t frame_num = ctx_->dec->capture_plane.getTotalDequeuedBuffers() - 1;

    cout << "Frame " << frame_num << endl;

    if (metadata->bValidFrameStatus) {
        if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H264) {
            switch (metadata->CodecParams.H264DecParams.FrameType) {
                case 0:
                    cout << "FrameType = B" << endl;
                    break;
                case 1:
                    cout << "FrameType = P" << endl;
                    break;
                case 2:
                    cout << "FrameType = I";
                    if (metadata->CodecParams.H264DecParams.dpbInfo.currentFrame
                            .bIdrFrame) {
                        cout << " (IDR)";
                    }
                    cout << endl;
                    break;
            }
            cout << "nActiveRefFrames = "
                 << metadata->CodecParams.H264DecParams.dpbInfo.nActiveRefFrames
                 << endl;
        }

        if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H265) {
            switch (metadata->CodecParams.HEVCDecParams.FrameType) {
                case 0:
                    cout << "FrameType = B" << endl;
                    break;
                case 1:
                    cout << "FrameType = P" << endl;
                    break;
                case 2:
                    cout << "FrameType = I";
                    if (metadata->CodecParams.HEVCDecParams.dpbInfo.currentFrame
                            .bIdrFrame) {
                        cout << " (IDR)";
                    }
                    cout << endl;
                    break;
            }
            cout << "nActiveRefFrames = "
                 << metadata->CodecParams.HEVCDecParams.dpbInfo.nActiveRefFrames
                 << endl;
        }

        if (metadata->FrameDecStats.DecodeError) {
            v4l2_ctrl_videodec_statusmetadata* dec_stats =
                &metadata->FrameDecStats;
            cout << "ErrorType=" << dec_stats->DecodeError
                 << " Decoded MBs=" << dec_stats->DecodedMBs
                 << " Concealed MBs=" << dec_stats->ConcealedMBs << endl;
        }
    }
    else {
        cout << "No valid metadata for frame" << endl;
    }
}

#ifndef USE_NVBUF_TRANSFORM_API
int GPNvVideoDecoder::sendEOStoConverter()
{
    // Check if converter is running
    if (ctx_->conv->output_plane.getStreamStatus()) {
        NvBuffer* conv_buffer;
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(&planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        pthread_mutex_lock(&ctx_->queue_lock);
        while (ctx_->conv_output_plane_buf_queue->empty()) {
            pthread_cond_wait(&ctx_->queue_cond, &ctx_->queue_lock);
        }
        conv_buffer = ctx_->conv_output_plane_buf_queue->front();
        ctx_->conv_output_plane_buf_queue->pop();
        pthread_mutex_unlock(&ctx_->queue_lock);

        v4l2_buf.index = conv_buffer->index;

        // Queue EOS buffer on converter output plane
        return ctx_->conv->output_plane.qBuffer(v4l2_buf, NULL);
    }
    return 0;
}
#endif

void GPNvVideoDecoder::query_and_set_capture()
{
    NvVideoDecoder* dec = ctx_->dec;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    int ret = 0;
    int error = 0;
    uint32_t window_width;
    uint32_t window_height;
    NvBufferCreateParams input_params = {0};
    NvBufferCreateParams cParams = {0};

    GPDisplayEGL* display = dynamic_cast<GPDisplayEGL*>(
        GetBeader(BeaderType::EGLDisplaySink).get());

    // Get capture plane format from the decoder. This may change after
    // an resolution change event
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
               "Error: Could not get format from decoder capture plane", error);

    // Get the display resolution from the decoder
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0, "Error: Could not get crop from decoder capture plane",
               error);

    cout << "Video Resolution: " << crop.c.width << "x" << crop.c.height
         << endl;
    ctx_->display_height = crop.c.height;
    ctx_->display_width = crop.c.width;
#ifdef USE_NVBUF_TRANSFORM_API
    if (ctx_->dst_dma_fd != -1) {
        NvBufferDestroy(ctx_->dst_dma_fd);
        ctx_->dst_dma_fd = -1;
    }

    input_params.payloadType = NvBufferPayload_SurfArray;
    input_params.width = crop.c.width;
    input_params.height = crop.c.height;
    input_params.layout = NvBufferLayout_Pitch;
    input_params.colorFormat = ctx_->out_pixfmt == 1
                                   ? NvBufferColorFormat_NV12
                                   : NvBufferColorFormat_YUV420;
    input_params.nvbuf_tag = NvBufferTag_VIDEO_CONVERT;

    ret = NvBufferCreateEx(&ctx_->dst_dma_fd, &input_params);
    TEST_ERROR(ret == -1, "create dmabuf failed", error);
#else
    // For file write, first deinitialize output and capture planes
    // of video converter and then use the new resolution from
    // decoder event resolution change
    if (ctx_->conv) {
        ret = sendEOStoConverter(ctx);
        TEST_ERROR(ret < 0,
                   "Error while queueing EOS buffer on converter output",
                   error);

        ctx_->conv->capture_plane.waitForDQThread(2000);

        ctx_->conv->output_plane.deinitPlane();
        ctx_->conv->capture_plane.deinitPlane();

        while (!ctx_->conv_output_plane_buf_queue->empty()) {
            ctx_->conv_output_plane_buf_queue->pop();
        }
    }
#endif

    // if (!ctx_->disable_rendering) {
    if (display) {
        // Destroy the old instance of renderer as resolution might have changed
        // delete ctx_->renderer;

        if (ctx_->fullscreen) {
            // Required for fullscreen
            window_width = window_height = 0;
        }
        else if (ctx_->window_width && ctx_->window_height) {
            // As specified by user on commandline
            window_width = ctx_->window_width;
            window_height = ctx_->window_height;
        }
        else {
            // Resolution got from the decoder
            window_width = crop.c.width;
            window_height = crop.c.height;
        }

        bool renderer_error =
            display->Initialize(ctx_->fps, false, window_width, window_height,
                                ctx_->window_x, ctx_->window_y);
        TEST_ERROR(!renderer_error,
                   "Error in setting up renderer. "
                   "Check if X is running or run with --disable-rendering",
                   error);

        // If height or width are set to zero, EglRenderer creates a fullscreen
        // window
        // ctx_->renderer = NvEglRenderer::createEglRenderer(
        //     "renderer0", window_width, window_height, ctx_->window_x,
        //     ctx_->window_y);
        // TEST_ERROR(!ctx_->renderer,
        //            "Error in setting up renderer. "
        //            "Check if X is running or run with --disable-rendering",
        //            error);
        if (ctx_->stats) {
            // ctx_->renderer->enableProfiling();
            display->enableProfiling();
        }

        // ctx_->renderer->setFPS(ctx_->fps);
    }

    // deinitPlane unmaps the buffers and calls REQBUFS with count 0
    dec->capture_plane.deinitPlane();
    if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF) {
        for (int index = 0; index < ctx_->numCapBuffers; index++) {
            if (ctx_->dmabuff_fd[index] != 0) {
                ret = NvBufferDestroy(ctx_->dmabuff_fd[index]);
                TEST_ERROR(ret < 0, "Failed to Destroy NvBuffer", error);
            }
        }
    }

    // Not necessary to call VIDIOC_S_FMT on decoder capture plane.
    // But decoder setCapturePlaneFormat function updates the class variables
    ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                     format.fmt.pix_mp.width,
                                     format.fmt.pix_mp.height);
    TEST_ERROR(ret < 0, "Error in setting decoder capture plane format", error);

    ctx_->video_height = format.fmt.pix_mp.height;
    ctx_->video_width = format.fmt.pix_mp.width;
    // Get the minimum buffers which have to be requested on the capture plane
    ret = dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    TEST_ERROR(ret < 0,
               "Error while getting value of minimum capture plane buffers",
               error);

    // Request (min + extra) buffers, export and map buffers
    if (ctx_->capture_plane_mem_type == V4L2_MEMORY_MMAP) {
        ret = dec->capture_plane.setupPlane(
            V4L2_MEMORY_MMAP,
            min_dec_capture_buffers + ctx_->extra_cap_plane_buffer, false,
            false);
        TEST_ERROR(ret < 0, "Error in decoder capture plane setup", error);
    }
    else if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF) {
        switch (format.fmt.pix_mp.colorspace) {
            case V4L2_COLORSPACE_SMPTE170M:
                if (format.fmt.pix_mp.quantization ==
                    V4L2_QUANTIZATION_DEFAULT) {
                    cout << "Decoder colorspace ITU-R BT.601 with standard "
                            "range luma (16-235)"
                         << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12;
                }
                else {
                    cout << "Decoder colorspace ITU-R BT.601 with extended "
                            "range luma (0-255)"
                         << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_ER;
                }
                break;
            case V4L2_COLORSPACE_REC709:
                if (format.fmt.pix_mp.quantization ==
                    V4L2_QUANTIZATION_DEFAULT) {
                    cout << "Decoder colorspace ITU-R BT.709 with standard "
                            "range luma (16-235)"
                         << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_709;
                }
                else {
                    cout << "Decoder colorspace ITU-R BT.709 with extended "
                            "range luma (0-255)"
                         << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_709_ER;
                }
                break;
            case V4L2_COLORSPACE_BT2020: {
                cout << "Decoder colorspace ITU-R BT.2020" << endl;
                cParams.colorFormat = NvBufferColorFormat_NV12_2020;
            } break;
            default:
                cout
                    << "supported colorspace details not available, use default"
                    << endl;
                if (format.fmt.pix_mp.quantization ==
                    V4L2_QUANTIZATION_DEFAULT) {
                    cout << "Decoder colorspace ITU-R BT.601 with standard "
                            "range luma (16-235)"
                         << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12;
                }
                else {
                    cout << "Decoder colorspace ITU-R BT.601 with extended "
                            "range luma (0-255)"
                         << endl;
                    cParams.colorFormat = NvBufferColorFormat_NV12_ER;
                }
                break;
        }

        ctx_->numCapBuffers =
            min_dec_capture_buffers + ctx_->extra_cap_plane_buffer;

        for (int index = 0; index < ctx_->numCapBuffers; index++) {
            cParams.width = crop.c.width;
            cParams.height = crop.c.height;
            cParams.layout = NvBufferLayout_BlockLinear;
            cParams.payloadType = NvBufferPayload_SurfArray;
            cParams.nvbuf_tag = NvBufferTag_VIDEO_DEC;
            ret = NvBufferCreateEx(&ctx_->dmabuff_fd[index], &cParams);
            TEST_ERROR(ret < 0, "Failed to create buffers", error);
        }
        ret =
            dec->capture_plane.reqbufs(V4L2_MEMORY_DMABUF, ctx_->numCapBuffers);
        TEST_ERROR(ret, "Error in request buffers on capture plane", error);
    }

#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx_->conv) {
        ret = ctx_->conv->setOutputPlaneFormat(
            format.fmt.pix_mp.pixelformat, format.fmt.pix_mp.width,
            format.fmt.pix_mp.height, V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
        TEST_ERROR(ret < 0, "Error in converter output plane set format",
                   error);

        ret = ctx_->conv->setCapturePlaneFormat(
            (ctx_->out_pixfmt == 1 ? V4L2_PIX_FMT_NV12M : V4L2_PIX_FMT_YUV420M),
            crop.c.width, crop.c.height, V4L2_NV_BUFFER_LAYOUT_PITCH);
        TEST_ERROR(ret < 0, "Error in converter capture plane set format",
                   error);

        ret = ctx_->conv->setCropRect(0, 0, crop.c.width, crop.c.height);
        TEST_ERROR(ret < 0, "Error while setting crop rect", error);

        if (ctx_->rescale_method) {
            // rescale full range [0-255] to limited range [16-235]
            ret = ctx_->conv->setYUVRescale(ctx_->rescale_method);
            TEST_ERROR(ret < 0, "Error while setting YUV rescale", error);
        }

        ret = ctx_->conv->output_plane.setupPlane(
            V4L2_MEMORY_DMABUF, dec->capture_plane.getNumBuffers(), false,
            false);
        TEST_ERROR(ret < 0, "Error in converter output plane setup", error);

        ret = ctx_->conv->capture_plane.setupPlane(
            V4L2_MEMORY_MMAP, dec->capture_plane.getNumBuffers(), true, false);
        TEST_ERROR(ret < 0, "Error in converter capture plane setup", error);

        ret = ctx_->conv->output_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in converter output plane streamon", error);

        ret = ctx_->conv->capture_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in converter output plane streamoff", error);

        // Add all empty conv output plane buffers to
        // conv_output_plane_buf_queue
        for (uint32_t i = 0; i < ctx_->conv->output_plane.getNumBuffers();
             i++) {
            ctx_->conv_output_plane_buf_queue->push(
                ctx_->conv->output_plane.getNthBuffer(i));
        }

        for (uint32_t i = 0; i < ctx_->conv->capture_plane.getNumBuffers();
             i++) {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));

            v4l2_buf.index = i;
            v4l2_buf.m.planes = planes;
            ret = ctx_->conv->capture_plane.qBuffer(v4l2_buf, NULL);
            TEST_ERROR(ret < 0, "Error Qing buffer at converter capture plane",
                       error);
        }
        ctx_->conv->output_plane.startDQThread(ctx);
        ctx_->conv->capture_plane.startDQThread(ctx);
    }
#endif

    // Capture plane STREAMON
    ret = dec->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", error);

    // Enqueue all the empty capture plane buffers
    for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++) {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = ctx_->capture_plane_mem_type;
        if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
            v4l2_buf.m.planes[0].m.fd = ctx_->dmabuff_fd[i];
        ret = dec->capture_plane.qBuffer(v4l2_buf, NULL);
        TEST_ERROR(ret < 0, "Error Qing buffer at output plane", error);
    }
    SPDLOG_INFO("Query and set capture successful");
    return;

error:
    if (error) {
        abort();
        SPDLOG_ERROR("Error in {}", __func__);
    }
}

void* GPNvVideoDecoder::decoder_pollthread_fcn(void* arg)
{
    VideoDecodeContext_T* ctx = static_cast<VideoDecodeContext_T*>(arg);
    v4l2_ctrl_video_device_poll devicepoll;

    SPDLOG_INFO("Starting Device Poll Thread ");

    memset(&devicepoll, 0, sizeof(v4l2_ctrl_video_device_poll));

    // wait here until you are signalled to issue the Poll call.
    // Check if the abort status is set , if so exit
    // Else issue the Poll on the decoder and block.
    // When the Poll returns, signal the decoder thread to continue.

    while (!ctx->got_error && !ctx->dec->isInError()) {
        sem_wait(&ctx->pollthread_sema);

        if (ctx->got_eos) {
            cout << "Decoder got eos, exiting poll thread \n";
            return NULL;
        }

        devicepoll.req_events = POLLIN | POLLOUT | POLLERR | POLLPRI;

        // This call shall wait in the v4l2 decoder library
        ctx->dec->DevicePoll(&devicepoll);

        // We can check the devicepoll.resp_events bitmask to see which events
        // are set.
        sem_post(&ctx->decoderthread_sema);
    }
    return NULL;
}

void* GPNvVideoDecoder::dec_capture_loop_fcn(void* arg)
{
    GPNvVideoDecoder* videoDecoder = (GPNvVideoDecoder*)arg;
    std::shared_ptr<GPlayer::VideoDecodeContext_T> ctx = videoDecoder->ctx_;
    NvVideoDecoder* dec = ctx->dec;
    struct v4l2_event ev;
    int ret;

    GPDisplayEGL* display = dynamic_cast<GPDisplayEGL*>(
        videoDecoder->GetBeader(BeaderType::EGLDisplaySink).get());

    SPDLOG_INFO("Starting decoder capture loop thread");
    // Need to wait for the first Resolution change event, so that
    // the decoder knows the stream resolution and can allocate appropriate
    // buffers when we call REQBUFS
    do {
        ret = dec->dqEvent(ev, 50000);
        if (ret < 0) {
            if (errno == EAGAIN) {
                SPDLOG_ERROR(
                    "Timed out waiting for first "
                    "V4L2_EVENT_RESOLUTION_CHANGE");
            }
            else {
                SPDLOG_ERROR("Error in dequeueing decoder event");
            }
            videoDecoder->Abort();
            break;
        }
    } while ((ev.type != V4L2_EVENT_RESOLUTION_CHANGE) && !ctx->got_error);

    // query_and_set_capture acts on the resolution change event
    if (!ctx->got_error)
        videoDecoder->query_and_set_capture();

    // Exit on error or EOS which is signalled in main()
    while (!(ctx->got_error || dec->isInError() || ctx->got_eos)) {
        NvBuffer* dec_buffer;

        // Check for Resolution change again
        ret = dec->dqEvent(ev, false);
        if (ret == 0) {
            switch (ev.type) {
                case V4L2_EVENT_RESOLUTION_CHANGE:
                    videoDecoder->query_and_set_capture();
                    continue;
            }
        }

        while (1) {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));
            v4l2_buf.m.planes = planes;

            // Dequeue a filled buffer
            if (dec->capture_plane.dqBuffer(v4l2_buf, &dec_buffer, NULL, 0)) {
                if (errno == EAGAIN) {
                    usleep(1000);
                }
                else {
                    videoDecoder->Abort();
                    SPDLOG_ERROR(
                        "Error while calling dequeue at capture plane");
                }
                break;
            }

            if (ctx->enable_metadata) {
                v4l2_ctrl_videodec_outputbuf_metadata dec_metadata;

                ret = dec->getMetadata(v4l2_buf.index, dec_metadata);
                if (ret == 0) {
                    videoDecoder->report_metadata(&dec_metadata);
                }
            }

            if (ctx->copy_timestamp && ctx->input_nalu && ctx->stats) {
                cout << "[" << v4l2_buf.index
                     << "]"
                        "dec capture plane dqB timestamp ["
                     << v4l2_buf.timestamp.tv_sec << "s"
                     << v4l2_buf.timestamp.tv_usec << "us]" << endl;
            }

            if (display && ctx->stats) {
                // EglRenderer requires the fd of the 0th plane to render the
                // buffer
                if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    dec_buffer->planes[0].fd = ctx->dmabuff_fd[v4l2_buf.index];
                // ctx->renderer->render(dec_buffer->planes[0].fd);
                display->Display(false, dec_buffer->planes[0].fd);
            }

            // If we need to write to file or display the buffer,
            // give the buffer to video converter output plane
            // instead of returning the buffer back to decoder capture plane
            // if (ctx->out_file || (!ctx->disable_rendering && !ctx->stats)) {
            if (display && !ctx->stats) {
#ifndef USE_NVBUF_TRANSFORM_API
                NvBuffer* conv_buffer;
                struct v4l2_buffer conv_output_buffer;
                struct v4l2_plane conv_planes[MAX_PLANES];

                memset(&conv_output_buffer, 0, sizeof(conv_output_buffer));
                memset(conv_planes, 0, sizeof(conv_planes));
                conv_output_buffer.m.planes = conv_planes;

                // Get an empty conv output plane buffer from
                // conv_output_plane_buf_queue
                pthread_mutex_lock(&ctx->queue_lock);
                while (ctx->conv_output_plane_buf_queue->empty()) {
                    pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
                }
                conv_buffer = ctx->conv_output_plane_buf_queue->front();
                ctx->conv_output_plane_buf_queue->pop();
                pthread_mutex_unlock(&ctx->queue_lock);

                conv_output_buffer.index = conv_buffer->index;
                if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    dec_buffer->planes[0].fd = ctx->dmabuff_fd[v4l2_buf.index];

                if (ctx->conv->output_plane.qBuffer(conv_output_buffer,
                                                    dec_buffer) < 0) {
                    Abort();
                    SPDLOG_ERROR(
                        "Error while queueing buffer at converter output "
                        "plane");
                    break;
                }
#else
                /* Clip & Stitch can be done by adjusting rectangle */
                NvBufferRect src_rect, dest_rect;
                src_rect.top = 0;
                src_rect.left = 0;
                src_rect.width = ctx->display_width;
                src_rect.height = ctx->display_height;
                dest_rect.top = 0;
                dest_rect.left = 0;
                dest_rect.width = ctx->display_width;
                dest_rect.height = ctx->display_height;

                NvBufferTransformParams transform_params;
                memset(&transform_params, 0, sizeof(transform_params));
                /* Indicates which of the transform parameters are valid */
                transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER;
                transform_params.transform_flip = NvBufferTransform_None;
                transform_params.transform_filter =
                    NvBufferTransform_Filter_Smart;
                transform_params.src_rect = src_rect;
                transform_params.dst_rect = dest_rect;

                if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    dec_buffer->planes[0].fd = ctx->dmabuff_fd[v4l2_buf.index];
                // Convert Blocklinear to PitchLinear
                ret = NvBufferTransform(dec_buffer->planes[0].fd,
                                        ctx->dst_dma_fd, &transform_params);
                if (ret == -1) {
                    SPDLOG_ERROR("Transform failed");
                }

                // TODO: Write raw video frame to file
                //
                // if (!ctx->stats && ctx->out_file) {
                //     // Dumping two planes of NV12 and three for I420
                //     dump_dmabuf(ctx->dst_dma_fd, 0, ctx->out_file);
                //     dump_dmabuf(ctx->dst_dma_fd, 1, ctx->out_file);
                //     if (ctx->out_pixfmt != 1) {
                //         dump_dmabuf(ctx->dst_dma_fd, 2, ctx->out_file);
                //     }
                // }

                if (!ctx->stats && display) {
                    // ctx->renderer->render(ctx->dst_dma_fd);
                    display->Display(false, ctx->dst_dma_fd);
                }

                // Not writing to file
                // Queue the buffer back once it has been used.
                if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    v4l2_buf.m.planes[0].m.fd = ctx->dmabuff_fd[v4l2_buf.index];
                if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0) {
                    videoDecoder->Abort();
                    SPDLOG_ERROR(
                        "Error while queueing buffer at decoder capture "
                        "plane");
                    break;
                }
#endif
            }
            else {
                // Not writing to file
                // Queue the buffer back once it has been used.
                if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    v4l2_buf.m.planes[0].m.fd = ctx->dmabuff_fd[v4l2_buf.index];
                if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0) {
                    videoDecoder->Abort();
                    SPDLOG_ERROR(
                        "Error while queueing buffer at decoder capture "
                        "plane");
                    break;
                }
            }
        }
    }
#ifndef USE_NVBUF_TRANSFORM_API
    // Send EOS to converter
    if (ctx->conv) {
        if (sendEOStoConverter(ctx) < 0) {
            SPDLOG_ERROR("Error while queueing EOS buffer on converter output");
        }
    }
#endif
    SPDLOG_INFO("Exiting decoder capture loop thread");
    return NULL;
}

void GPNvVideoDecoder::set_defaults()
{
    memset(ctx_.get(), 0, sizeof(context_t));
    ctx_->fullscreen = false;
    ctx_->window_height = 0;
    ctx_->window_width = 0;
    ctx_->window_x = 0;
    ctx_->window_y = 0;
    ctx_->out_pixfmt = 1;
    ctx_->fps = 30;
    ctx_->output_plane_mem_type = V4L2_MEMORY_MMAP;
    ctx_->capture_plane_mem_type = V4L2_MEMORY_DMABUF;
    ctx_->vp9_file_header_flag = 0;
    ctx_->vp8_file_header_flag = 0;
    ctx_->copy_timestamp = false;
    ctx_->flag_copyts = false;
    ctx_->start_ts = 0;
    ctx_->file_count = 1;
    ctx_->dec_fps = 30;
    ctx_->dst_dma_fd = -1;
    ctx_->bLoop = false;
    ctx_->bQueue = false;
    ctx_->loop_count = 0;
    ctx_->max_perf = 0;
    ctx_->extra_cap_plane_buffer = 1;
    ctx_->blocking_mode = 1;
#ifndef USE_NVBUF_TRANSFORM_API
    ctx_->conv_output_plane_buf_queue = new queue<NvBuffer*>;
    ctx_->rescale_method = V4L2_YUV_RESCALE_NONE;
#endif
    pthread_mutex_init(&ctx_->queue_lock, NULL);
    pthread_cond_init(&ctx_->queue_cond, NULL);
}

bool GPNvVideoDecoder::decoder_proc_nonblocking(bool eos,
                                                uint32_t current_file,
                                                int current_loop)
{
    // In non-blocking mode, we will have this function do below things:
    // Issue signal to PollThread so it starts Poll and wait until we are
    // signalled. After we are signalled, it means there is something to
    // dequeue, either output plane or capture plane or there's an event. Try
    // dequeuing from all three and then act appropriately. After enqueuing go
    // back to the same loop.

    // Since all the output plane buffers have been queued, we first need to
    // dequeue a buffer from output plane before we can read new data into it
    // and queue it again.
    int allow_DQ = true;
    int ret = 0;
    struct v4l2_buffer temp_buf;
    struct v4l2_event ev;

    GPDisplayEGL* display = dynamic_cast<GPDisplayEGL*>(
        GetBeader(BeaderType::EGLDisplaySink).get());

    while (!ctx_->got_error && !ctx_->dec->isInError()) {
        struct v4l2_buffer v4l2_output_buf;
        struct v4l2_plane output_planes[MAX_PLANES];

        struct v4l2_buffer v4l2_capture_buf;
        struct v4l2_plane capture_planes[MAX_PLANES];

        NvBuffer* output_buffer = NULL;
        NvBuffer* capture_buffer = NULL;

        memset(&v4l2_output_buf, 0, sizeof(v4l2_output_buf));
        memset(output_planes, 0, sizeof(output_planes));
        v4l2_output_buf.m.planes = output_planes;

        memset(&v4l2_capture_buf, 0, sizeof(v4l2_capture_buf));
        memset(capture_planes, 0, sizeof(capture_planes));
        v4l2_capture_buf.m.planes = capture_planes;

        // Call SetPollInterrupt
        ctx_->dec->SetPollInterrupt();

        // Since buffers have been queued, issue a post to start polling and
        // then wait here
        sem_post(&ctx_->pollthread_sema);
        sem_wait(&ctx_->decoderthread_sema);

        ret = ctx_->dec->dqEvent(ev, 0);
        if (ret == 0) {
            if (ev.type == V4L2_EVENT_RESOLUTION_CHANGE) {
                cout << "Got V4L2_EVENT_RESOLUTION_CHANGE EVENT \n";
                query_and_set_capture();
            }
        }
        else if (ret < 0 && errno == EINVAL) {
            SPDLOG_ERROR("Error in dequeueing decoder event");
            Abort();
        }

        while (1) {
            // Now dequeue from the output plane and enqueue back the buffers
            // after reading
            if ((eos) && (ctx_->dec->output_plane.getNumQueuedBuffers() == 0)) {
                cout << "Done processing all the buffers returning \n";
                return true;
            }

            if (allow_DQ) {
                ret = ctx_->dec->output_plane.dqBuffer(v4l2_output_buf,
                                                       &output_buffer, NULL, 0);
                if (ret < 0) {
                    if (errno == EAGAIN)
                        goto check_capture_buffers;
                    else {
                        SPDLOG_ERROR("Error DQing buffer at output plane");
                        Abort();
                        break;
                    }
                }
            }
            else {
                allow_DQ = true;
                memcpy(&v4l2_output_buf, &temp_buf, sizeof(v4l2_buffer));
                output_buffer =
                    ctx_->dec->output_plane.getNthBuffer(v4l2_output_buf.index);
            }

            if ((v4l2_output_buf.flags & V4L2_BUF_FLAG_ERROR) &&
                ctx_->enable_input_metadata) {
                v4l2_ctrl_videodec_inputbuf_metadata dec_input_metadata;

                ret = ctx_->dec->getInputMetadata(v4l2_output_buf.index,
                                                  dec_input_metadata);
                if (ret == 0) {
                    ret = report_input_metadata(&dec_input_metadata);
                    if (ret == -1) {
                        SPDLOG_ERROR("Error with input stream header parsing");
                    }
                }
            }

            if (eos) {
                // cout << "Got EOS , no more queueing of buffers on OUTPUT
                // plane \n";
                goto check_capture_buffers;
            }

            if ((ctx_->decoder_pixfmt == V4L2_PIX_FMT_H264) ||
                (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H265) ||
                (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG2) ||
                (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG4)) {
                if (ctx_->input_nalu) {
                    read_decoder_input_nalu(output_buffer);
                }
                else {
                    read_decoder_input_chunk(output_buffer);
                }
            }
            if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP9 ||
                ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP8) {
                ret = read_vpx_decoder_input_chunk(output_buffer);
                if (ret != 0)
                    SPDLOG_ERROR("Couldn't read chunk");
            }
            v4l2_output_buf.m.planes[0].bytesused =
                output_buffer->planes[0].bytesused;

            if (ctx_->input_nalu && ctx_->copy_timestamp && ctx_->flag_copyts) {
                v4l2_output_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
                ctx_->timestamp += ctx_->timestampincr;
                v4l2_output_buf.timestamp.tv_sec =
                    ctx_->timestamp / (MICROSECOND_UNIT);
                v4l2_output_buf.timestamp.tv_usec =
                    ctx_->timestamp % (MICROSECOND_UNIT);
            }

            if (v4l2_output_buf.m.planes[0].bytesused == 0) {
                if (ctx_->bQueue) {
                    current_file++;
                    if (current_file != ctx_->file_count) {
                        allow_DQ = false;
                        memcpy(&temp_buf, &v4l2_output_buf,
                               sizeof(v4l2_buffer));
                        continue;
                    }
                }
                if (ctx_->bLoop) {
                    current_file = current_file % ctx_->file_count;
                    allow_DQ = false;
                    memcpy(&temp_buf, &v4l2_output_buf, sizeof(v4l2_buffer));
                    if (ctx_->loop_count == 0 ||
                        current_loop < ctx_->loop_count) {
                        current_loop++;
                        continue;
                    }
                }
            }
            ret = ctx_->dec->output_plane.qBuffer(v4l2_output_buf, NULL);
            if (ret < 0) {
                SPDLOG_ERROR("Error Qing buffer at output plane");
                Abort();
                break;
            }
            if (v4l2_output_buf.m.planes[0].bytesused == 0) {
                eos = true;
                SPDLOG_INFO("Input file read complete");
                goto check_capture_buffers;
            }
        }

        // Dequeue from the capture plane and write them to file and enqueue
        // back
    check_capture_buffers:
        while (1) {
            if (!ctx_->dec->capture_plane.getStreamStatus()) {
                SPDLOG_INFO("Capture plane not ON, skipping capture plane \n");
                break;
            }
            // Dequeue a filled buffer
            ret = ctx_->dec->capture_plane.dqBuffer(v4l2_capture_buf,
                                                    &capture_buffer, NULL, 0);
            if (ret < 0) {
                if (errno == EAGAIN)
                    break;
                else {
                    Abort();
                    SPDLOG_ERROR(
                        "Error while calling dequeue at capture plane");
                }
                break;
            }
            if (capture_buffer == NULL) {
                SPDLOG_INFO("Got CAPTURE BUFFER NULL \n");
                break;
            }

            if (ctx_->enable_metadata) {
                v4l2_ctrl_videodec_outputbuf_metadata dec_metadata;

                ret = ctx_->dec->getMetadata(v4l2_capture_buf.index,
                                             dec_metadata);
                if (ret == 0) {
                    report_metadata(&dec_metadata);
                }
            }

            if (ctx_->copy_timestamp && ctx_->input_nalu && ctx_->stats) {
                SPDLOG_INFO("[{}]dec capture plane dqB timestamp [{}s {}us]",
                            v4l2_capture_buf.index,
                            v4l2_capture_buf.timestamp.tv_sec,
                            v4l2_capture_buf.timestamp.tv_usec);
            }

            if (display && ctx_->stats) {
                // Rendering the buffer here
                // EglRenderer requires the fd of the 0th plane to render the
                // buffer
                if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    capture_buffer->planes[0].fd =
                        ctx_->dmabuff_fd[v4l2_capture_buf.index];
                // cout << "Enqueue the buffer to renderer " <<
                // capture_buffer->planes[0].fd << endl;
                if (display->Display(false, capture_buffer->planes[0].fd) ==
                    -1) {
                    Abort();
                    SPDLOG_ERROR("Error while queueing buffer for rendering ");
                    break;
                }
            }

            if (display && !ctx_->stats) {
                NvBufferRect src_rect, dest_rect;
                src_rect.top = 0;
                src_rect.left = 0;
                src_rect.width = ctx_->display_width;
                src_rect.height = ctx_->display_height;
                dest_rect.top = 0;
                dest_rect.left = 0;
                dest_rect.width = ctx_->display_width;
                dest_rect.height = ctx_->display_height;

                NvBufferTransformParams transform_params;
                /* Indicates which of the transform parameters are valid */
                memset(&transform_params, 0, sizeof(transform_params));
                transform_params.transform_flag = NVBUFFER_TRANSFORM_FILTER;
                transform_params.transform_flip = NvBufferTransform_None;
                transform_params.transform_filter =
                    NvBufferTransform_Filter_Smart;
                transform_params.src_rect = src_rect;
                transform_params.dst_rect = dest_rect;

                if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    capture_buffer->planes[0].fd =
                        ctx_->dmabuff_fd[v4l2_capture_buf.index];
                // Convert Blocklinear to PitchLinear
                ret = NvBufferTransform(capture_buffer->planes[0].fd,
                                        ctx_->dst_dma_fd, &transform_params);
                if (ret == -1) {
                    SPDLOG_ERROR("Transform failed");
                    break;
                }
                // Write raw video frame to file
                // if (!ctx_->stats && ctx_->out_file) {
                //     // Dumping two planes of NV12 and three for I420
                //     cout << "Writing to file \n";
                //     dump_dmabuf(ctx_->dst_dma_fd, 0, ctx_->out_file);
                //     dump_dmabuf(ctx_->dst_dma_fd, 1, ctx_->out_file);
                //     if (ctx_->out_pixfmt != 1) {
                //         dump_dmabuf(ctx_->dst_dma_fd, 2, ctx_->out_file);
                //     }

                //     //
                // }

                // TODO: Write video file
                // GPFileSink* buffer_handler =
                //     dynamic_cast<GPFileSink*>(GetBeader(BeaderType::FileSink));
                // if (buffer_handler) {
                //     GPBuffer gpbuffer(ctx->g_buff[v4l2_buf.index].start,
                //                       ctx->g_buff[v4l2_buf.index].size);
                //     GPData data(&gpbuffer);

                //     buffer_handler->Process(&data);
                // }

                if (!ctx_->stats && display) {
                    display->Display(false, ctx_->dst_dma_fd);
                }
                // Queue the buffer back once it has been used.
                // If we are not rendering, queue the buffer back here
                // immediately.
                if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    v4l2_capture_buf.m.planes[0].m.fd =
                        ctx_->dmabuff_fd[v4l2_capture_buf.index];
                if (ctx_->dec->capture_plane.qBuffer(v4l2_capture_buf, NULL) <
                    0) {
                    Abort();
                    SPDLOG_ERROR(
                        "Error while queueing buffer at decoder capture "
                        "plane");
                    break;
                }
            }
        }
    }
    return eos;
}

bool GPNvVideoDecoder::decoder_proc_blocking(bool eos,
                                             uint32_t current_file,
                                             int current_loop)
{
    // Since all the output plane buffers have been queued, we first need to
    // dequeue a buffer from output plane before we can read new data into it
    // and queue it again.
    int allow_DQ = true;
    int ret = 0;
    struct v4l2_buffer temp_buf;

    while (!eos && !ctx_->got_error && !ctx_->dec->isInError()) {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer* buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        if (allow_DQ) {
            ret = ctx_->dec->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, -1);
            if (ret < 0) {
                SPDLOG_ERROR("Error DQing buffer at output plane");
                Abort();
                break;
            }
        }
        else {
            allow_DQ = true;
            memcpy(&v4l2_buf, &temp_buf, sizeof(v4l2_buffer));
            buffer = ctx_->dec->output_plane.getNthBuffer(v4l2_buf.index);
        }

        if ((v4l2_buf.flags & V4L2_BUF_FLAG_ERROR) &&
            ctx_->enable_input_metadata) {
            v4l2_ctrl_videodec_inputbuf_metadata dec_input_metadata;

            ret =
                ctx_->dec->getInputMetadata(v4l2_buf.index, dec_input_metadata);
            if (ret == 0) {
                ret = report_input_metadata(&dec_input_metadata);
                if (ret == -1) {
                    SPDLOG_ERROR("Error with input stream header parsing");
                }
            }
        }

        if ((ctx_->decoder_pixfmt == V4L2_PIX_FMT_H264) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H265) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG2) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG4)) {
            if (ctx_->input_nalu) {
                read_decoder_input_nalu(buffer);
            }
            else {
                read_decoder_input_chunk(buffer);
            }
        }
        if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP9 ||
            ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP8) {
            ret = read_vpx_decoder_input_chunk(buffer);
            if (ret != 0)
                SPDLOG_ERROR("Couldn't read chunk");
        }
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        if (ctx_->input_nalu && ctx_->copy_timestamp && ctx_->flag_copyts) {
            v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
            ctx_->timestamp += ctx_->timestampincr;
            v4l2_buf.timestamp.tv_sec = ctx_->timestamp / (MICROSECOND_UNIT);
            v4l2_buf.timestamp.tv_usec = ctx_->timestamp % (MICROSECOND_UNIT);
        }

        if (v4l2_buf.m.planes[0].bytesused == 0) {
            if (ctx_->bQueue) {
                current_file++;
                if (current_file != ctx_->file_count) {
                    allow_DQ = false;
                    memcpy(&temp_buf, &v4l2_buf, sizeof(v4l2_buffer));
                    continue;
                }
            }
            if (ctx_->bLoop) {
                current_file = current_file % ctx_->file_count;
                allow_DQ = false;
                memcpy(&temp_buf, &v4l2_buf, sizeof(v4l2_buffer));
            }
        }
        ret = ctx_->dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0) {
            SPDLOG_ERROR("Error Qing buffer at output plane");
            Abort();
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0) {
            eos = true;
            SPDLOG_INFO("Input file read complete");
            break;
        }
    }
    return eos;
}

int GPNvVideoDecoder::Proc()
{
    int ret = 0;
    int error = 0;
    uint32_t current_file = 0;
    uint32_t i;
    bool eos = false;
    int current_loop = 0;
    // char* nalu_parse_buffer = NULL;
    NvApplicationProfiler& profiler =
        NvApplicationProfiler::getProfilerInstance();

    GPDisplayEGL* display = dynamic_cast<GPDisplayEGL*>(
        GetBeader(BeaderType::EGLDisplaySink).get());

    set_defaults();

    pthread_setname_np(pthread_self(), "DecOutPlane");

    // if (parse_csv_args(ctx_.get(), argc, argv)) {
    //     fprintf(stderr, "Error parsing commandline arguments\n");
    //     return -1;
    // }

    if (ctx_->blocking_mode) {
        cout << "Creating decoder in blocking mode \n";
        ctx_->dec = NvVideoDecoder::createVideoDecoder("dec0");
    }
    else {
        cout << "Creating decoder in non-blocking mode \n";
        ctx_->dec = NvVideoDecoder::createVideoDecoder("dec0", O_NONBLOCK);
    }
    TEST_ERROR(!ctx_->dec, "Could not create decoder", cleanup);

    // ctx_->in_file =
    //     (std::ifstream**)malloc(sizeof(std::ifstream*) * ctx_->file_count);
    // for (uint32_t i = 0; i < ctx_->file_count; i++) {
    //     ctx_->in_file[i] = new ifstream(ctx_->in_file_path[i]);
    //     TEST_ERROR(!ctx_->in_file[i]->is_open(), "Error opening input file",
    //                cleanup);
    // }

    // if (ctx_->out_file_path) {
    //     ctx_->out_file = new ofstream(ctx_->out_file_path);
    //     TEST_ERROR(!ctx_->out_file->is_open(), "Error opening output file",
    //                cleanup);
    // }

    if (ctx_->stats) {
        profiler.start(NvApplicationProfiler::DefaultSamplingInterval);
        ctx_->dec->enableProfiling();
    }

    // Subscribe to Resolution change event
    ret = ctx_->dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE",
               cleanup);

    // Set format on the output plane
    ret = ctx_->dec->setOutputPlaneFormat(ctx_->decoder_pixfmt, CHUNK_SIZE);
    TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

    if (ctx_->input_nalu) {
        // nalu_parse_buffer = new char[CHUNK_SIZE];
        spdlog::info("Setting frame input mode to 0 \n");
        ret = ctx_->dec->setFrameInputMode(0);
        TEST_ERROR(ret < 0, "Error in decoder setFrameInputMode", cleanup);
    }
    else {
        // Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
        // so that application can send chunks of encoded data instead of
        // forming complete frames.
        spdlog::info("Setting frame input mode to 1 \n");
        ret = ctx_->dec->setFrameInputMode(1);
        TEST_ERROR(ret < 0, "Error in decoder setFrameInputMode", cleanup);
    }

    // V4L2_CID_MPEG_VIDEO_DISABLE_DPB should be set after output plane
    // set format
    if (ctx_->disable_dpb) {
        ret = ctx_->dec->disableDPB();
        TEST_ERROR(ret < 0, "Error in decoder disableDPB", cleanup);
    }

    if (ctx_->enable_metadata || ctx_->enable_input_metadata) {
        ret = ctx_->dec->enableMetadataReporting();
        TEST_ERROR(ret < 0, "Error while enabling metadata reporting", cleanup);
    }

    if (ctx_->max_perf) {
        ret = ctx_->dec->setMaxPerfMode(ctx_->max_perf);
        TEST_ERROR(ret < 0, "Error while setting decoder to max perf", cleanup);
    }

    if (ctx_->skip_frames) {
        ret = ctx_->dec->setSkipFrames(ctx_->skip_frames);
        TEST_ERROR(ret < 0, "Error while setting skip frames param", cleanup);
    }

    // Query, Export and Map the output plane buffers so that we can read
    // encoded data into the buffers
    if (ctx_->output_plane_mem_type == V4L2_MEMORY_MMAP) {
        ret = ctx_->dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 2, true,
                                                 false);
    }
    else if (ctx_->output_plane_mem_type == V4L2_MEMORY_USERPTR) {
        ret = ctx_->dec->output_plane.setupPlane(V4L2_MEMORY_USERPTR, 10, false,
                                                 true);
    }

    TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);

#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx_->out_file || (!ctx_->disable_rendering && !ctx_->stats)) {
        // Create converter to convert from BL to PL for writing raw video
        // to file
        ctx_->conv = NvVideoConverter::createVideoConverter("conv0");
        TEST_ERROR(!ctx_->conv, "Could not create video converter", cleanup);
        ctx_->conv->output_plane.setDQThreadCallback(
            conv0_output_dqbuf_thread_callback);
        ctx_->conv->capture_plane.setDQThreadCallback(
            conv0_capture_dqbuf_thread_callback);

        if (ctx_->stats) {
            ctx_->conv->enableProfiling();
        }
    }
#endif

    ret = ctx_->dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);

    if (ctx_->blocking_mode) {
        pthread_create(&ctx_->dec_capture_loop, NULL, dec_capture_loop_fcn,
                       this);
        pthread_setname_np(ctx_->dec_capture_loop, "DecCapPlane");
    }
    else {
        sem_init(&ctx_->pollthread_sema, 0, 0);
        sem_init(&ctx_->decoderthread_sema, 0, 0);
        pthread_create(&ctx_->dec_pollthread, NULL, decoder_pollthread_fcn,
                       this);
        cout << "Created the PollThread and Decoder Thread \n";
        pthread_setname_np(ctx_->dec_pollthread, "DecPollThread");
    }

    if (ctx_->copy_timestamp && ctx_->input_nalu) {
        ctx_->timestamp = (ctx_->start_ts * MICROSECOND_UNIT);
        ctx_->timestampincr =
            (MICROSECOND_UNIT * 16) / ((uint32_t)(ctx_->dec_fps * 16));
    }

    // Read encoded data and enqueue all the output plane buffers.
    // Exit loop in case file read is complete.
    i = 0;
    current_loop = 1;
    while (!eos && !ctx_->got_error && !ctx_->dec->isInError() &&
           i < ctx_->dec->output_plane.getNumBuffers()) {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer* buffer;

        {
            std::unique_lock<std::mutex> lk(buffer_mutex_);
            thread_condition_.wait(lk, [&] { return buffer_.size() > 0; });
        }

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        buffer = ctx_->dec->output_plane.getNthBuffer(i);
        if ((ctx_->decoder_pixfmt == V4L2_PIX_FMT_H264) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H265) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG2) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG4)) {
            if (ctx_->input_nalu) {
                // read_decoder_input_nalu(ctx_->in_file[current_file], buffer,
                //                         nalu_parse_buffer, CHUNK_SIZE);

                read_decoder_input_nalu(buffer);
            }
            else {
                read_decoder_input_chunk(buffer);
            }
        }
        else if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP9 ||
                 ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP8) {
            ret = read_vpx_decoder_input_chunk(buffer);
            if (ret != 0)
                SPDLOG_ERROR("Couldn't read chunk");
        }

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        if (ctx_->input_nalu && ctx_->copy_timestamp && ctx_->flag_copyts) {
            v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
            ctx_->timestamp += ctx_->timestampincr;
            v4l2_buf.timestamp.tv_sec = ctx_->timestamp / (MICROSECOND_UNIT);
            v4l2_buf.timestamp.tv_usec = ctx_->timestamp % (MICROSECOND_UNIT);
        }

        if (v4l2_buf.m.planes[0].bytesused == 0) {
            if (ctx_->bQueue) {
                current_file++;
                if (current_file != ctx_->file_count) {
                    continue;
                }
            }
            if (ctx_->bLoop) {
                current_file = current_file % ctx_->file_count;
                if (ctx_->loop_count == 0 || current_loop < ctx_->loop_count) {
                    current_loop++;
                    continue;
                }
            }
        }
        // It is necessary to queue an empty buffer to signal EOS to the decoder
        // i.e. set v4l2_buf.m.planes[0].bytesused = 0 and queue the buffer
        ret = ctx_->dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0) {
            SPDLOG_ERROR("Error Qing buffer at output plane");
            Abort();
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0) {
            eos = true;
            SPDLOG_INFO("Input file read complete");
            break;
        }
        i++;
    }
    if (ctx_->blocking_mode)
        eos = decoder_proc_blocking(eos, current_file, current_loop);
    else
        eos = decoder_proc_nonblocking(eos, current_file, current_loop);
    // After sending EOS, all the buffers from output plane should be dequeued.
    // and after that capture plane loop should be signalled to stop.
    if (ctx_->blocking_mode) {
        while (ctx_->dec->output_plane.getNumQueuedBuffers() > 0 &&
               !ctx_->got_error && !ctx_->dec->isInError()) {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));

            v4l2_buf.m.planes = planes;
            ret = ctx_->dec->output_plane.dqBuffer(v4l2_buf, NULL, NULL, -1);
            if (ret < 0) {
                SPDLOG_ERROR("Error DQing buffer at output plane");
                Abort();
                break;
            }

            if ((v4l2_buf.flags & V4L2_BUF_FLAG_ERROR) &&
                ctx_->enable_input_metadata) {
                v4l2_ctrl_videodec_inputbuf_metadata dec_input_metadata;

                ret = ctx_->dec->getInputMetadata(v4l2_buf.index,
                                                  dec_input_metadata);
                if (ret == 0) {
                    ret = report_input_metadata(&dec_input_metadata);
                    if (ret == -1) {
                        SPDLOG_ERROR("Error with input stream header parsing");
                        abort();
                        break;
                    }
                }
            }
        }
    }

    // Signal EOS to the decoder capture loop
    ctx_->got_eos = true;
#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx_->conv) {
        ctx_->conv->capture_plane.waitForDQThread(-1);
    }
#endif

    if (ctx_->stats) {
        profiler.stop();
        ctx_->dec->printProfilingStats(cout);
#ifndef USE_NVBUF_TRANSFORM_API
        if (ctx_->conv) {
            ctx_->conv->printProfilingStats(cout);
        }
#endif
        if (display) {
            display->printProfilingStats();
        }
        profiler.printProfilerData(cout);
    }

cleanup:
    if (ctx_->blocking_mode && ctx_->dec_capture_loop) {
        pthread_join(ctx_->dec_capture_loop, NULL);
    }
    else if (!ctx_->blocking_mode) {
        // Clear the poll interrupt to get the decoder's poll thread out.
        ctx_->dec->ClearPollInterrupt();
        // If Pollthread is waiting on, signal it to exit the thread.
        sem_post(&ctx_->pollthread_sema);
        pthread_join(ctx_->dec_pollthread, NULL);
    }
    if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF) {
        for (int index = 0; index < ctx_->numCapBuffers; index++) {
            if (ctx_->dmabuff_fd[index] != 0) {
                ret = NvBufferDestroy(ctx_->dmabuff_fd[index]);
                if (ret < 0) {
                    SPDLOG_ERROR("Failed to Destroy NvBuffer");
                }
            }
        }
    }
#ifndef USE_NVBUF_TRANSFORM_API
    if (ctx_->conv && ctx_->conv->isInError()) {
        SPDLOG_ERROR("Converter is in error");
        error = 1;
    }
#endif
    if (ctx_->dec && ctx_->dec->isInError()) {
        SPDLOG_ERROR("Decoder is in error");
        error = 1;
    }

    if (ctx_->got_error) {
        error = 1;
    }

    // The decoder destructor does all the cleanup i.e set streamoff on output
    // and capture planes, unmap buffers, tell decoder to deallocate buffer
    // (reqbufs ioctl with counnt = 0), and finally call v4l2_close on the fd.
    delete ctx_->dec;
#ifndef USE_NVBUF_TRANSFORM_API
    delete ctx_->conv;
#endif
    // Similarly, EglRenderer destructor does all the cleanup
    // delete ctx_->renderer;
    // for (uint32_t i = 0; i < ctx_->file_count; i++)
    //     delete ctx_->in_file[i];
    // delete ctx_->out_file;
#ifndef USE_NVBUF_TRANSFORM_API
    delete ctx_->conv_output_plane_buf_queue;
#else
    if (ctx_->dst_dma_fd != -1) {
        NvBufferDestroy(ctx_->dst_dma_fd);
        ctx_->dst_dma_fd = -1;
    }
#endif
    // delete[] nalu_parse_buffer;

    // free(ctx_->in_file);
    // for (uint32_t i = 0; i < ctx_->file_count; i++)
    //     free(ctx_->in_file_path[i]);
    // free(ctx_->in_file_path);
    // free(ctx_->out_file_path);
    if (!ctx_->blocking_mode) {
        sem_destroy(&ctx_->pollthread_sema);
        sem_destroy(&ctx_->decoderthread_sema);
    }

    return -error;
}

int GPNvVideoDecoder::decodeProc(GPNvVideoDecoder* decoder)
{
    return decoder->Proc();
}

}  // namespace GPlayer