
#include <linux/videodev2.h>
#include <nvbuf_utils.h>
#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#include "NvApplicationProfiler.h"
#include "NvUtils.h"

#include "gp_log.h"
#include "gplayer.h"

namespace GPlayer {

#define TEST_ERROR(cond, str, label) \
    if (cond) {                      \
        SPDLOG_ERROR(str);           \
        error = 1;                   \
        goto label;                  \
    }

#define TEST_PARSE_ERROR(cond, label)                                   \
    if (cond) {                                                         \
        SPDLOG_ERROR("Error parsing runtime parameter changes string"); \
        goto label;                                                     \
    }

const uint32_t MICROSECOND_UNIT = 1000000;
const uint32_t CHUNK_SIZE = 400000L;

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

GPNvVideoDecoder::GPNvVideoDecoder(
    const std::shared_ptr<VideoDecodeContext_T> context)
    : buffer_(CHUNK_SIZE * 16), pollthread_sema_(0), decoderthread_sema_(0)
{
    ctx_ = context;
    SetProperties("", "", BeaderType::NvVideoDecoder);
}

GPNvVideoDecoder::~GPNvVideoDecoder() {}

std::string GPNvVideoDecoder::GetInfo() const
{
    return "NvVideoDecoder";
}

void GPNvVideoDecoder::Process(GPData* data)
{
    std::lock_guard<std::mutex> guard(buffer_lock_);
    GPBuffer* buffer = *data;

    size_t put_size = buffer_.put(buffer->GetData(), buffer->GetLength());
    if (put_size < buffer->GetLength()) {
        SPDLOG_WARN("Buffer full!");
    }
    // SPDLOG_CRITICAL("Buffer received!");
    buffer_condition_.notify_one();
}

int GPNvVideoDecoder::read_decoder_input_nalu(NvBuffer* buffer)
{
    // std::lock_guard<std::mutex> lk(buffer_lock_);
    // Length is the size of the buffer in bytes
    char* buffer_ptr = (char*)buffer->planes[0].data;
    int h265_nal_unit_type;
    uint8_t stream_buffer[4];
    bool nalu_found = false;

    if (buffer_.size() == 0) {
        SPDLOG_TRACE("No buffers in the {}", GetName());
        buffer->planes[0].bytesused = 0;
        return 0;
    }

    // Find the first NAL unit in the buffer

    while (buffer_.snap(stream_buffer, 4) == 4) {
        nalu_found = IS_NAL_UNIT_START(stream_buffer) ||
                     IS_NAL_UNIT_START1(stream_buffer);
        if (nalu_found) {
            break;
        }
        buffer_.drop();
    }

    // Reached end of buffer but could not find NAL unit
    if (!nalu_found) {
        SPDLOG_ERROR(
            "Could not read nal unit from file. EOF or file corrupted");
        return -1;
    }

    buffer_.get(reinterpret_cast<uint8_t*>(buffer_ptr), 4);
    buffer_ptr += 4;
    buffer->planes[0].bytesused = 4;

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
    while (buffer_.snap(stream_buffer, 4) == 4) {
        if (IS_NAL_UNIT_START(stream_buffer) ||
            IS_NAL_UNIT_START1(stream_buffer)) {
            return buffer->planes[0].bytesused;
        }
        *buffer_ptr = stream_buffer[0];
        buffer_ptr++;
        buffer_.drop();
        buffer->planes[0].bytesused++;
    }

    // Reached end of buffer but could not find NAL unit
    SPDLOG_ERROR("Could not read nal unit from file. EOF or file corrupted");
    return -1;
}

int GPNvVideoDecoder::read_decoder_input_chunk(NvBuffer* buffer)
{
    // std::lock_guard<std::mutex> lk(buffer_lock_);

    GPFileSrc* file_src =
        dynamic_cast<GPFileSrc*>(FindParent(BeaderType::FileSrc).get());

    std::streamsize bytes_read;
    std::streamsize bytes_to_read =
        std::min(CHUNK_SIZE, buffer->planes[0].length);
#if 0
    if (buffer_.size() == 0) {
        SPDLOG_WARN("No data in the buffer of {}", GetInfo());
        return 0;
    }

    buffer->planes[0].bytesused =
        buffer_.get(buffer->planes[0].data, bytes_to_read);
    return buffer->planes[0].bytesused;
#endif

    std::basic_istream<char>& stream = file_src->Read(
        reinterpret_cast<char*>(buffer->planes[0].data), bytes_to_read);
    bytes_read = stream.gcount();
    if (bytes_read == 0) {
        // stream.clear();
        // stream.seekg(0, stream.beg);
    }
    buffer->planes[0].bytesused = bytes_read;
    return bytes_read;
}

int GPNvVideoDecoder::read_vpx_decoder_input_chunk(NvBuffer* buffer)
{
    // std::lock_guard<std::mutex> lk(buffer_lock_);
    size_t bytes_read = 0;
    size_t Framesize;
    unsigned char* bitstreambuffer = (unsigned char*)buffer->planes[0].data;
    if (ctx_->vp9_file_header_flag == 0) {
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
    bytes_read = buffer_.get(buffer->planes[0].data, IVF_FRAME_HDR_SIZE);
    if (bytes_read != IVF_FRAME_HDR_SIZE) {
        SPDLOG_ERROR("Couldn't read IVF FRAME HEADER");
        return -1;
    }
    Framesize = (bitstreambuffer[3] << 24) + (bitstreambuffer[2] << 16) +
                (bitstreambuffer[1] << 8) + bitstreambuffer[0];
    buffer->planes[0].bytesused = Framesize;
    bytes_read = buffer_.get(buffer->planes[0].data, Framesize);
    if (bytes_read != Framesize) {
        SPDLOG_ERROR("Couldn't read Framesize");
        return -1;
    }
    return bytes_read;
}

void GPNvVideoDecoder::Abort()
{
    ctx_->got_error = true;
    ctx_->dec->abort();
    if (!use_nvbuf_transform_api_) {
        if (ctx_->conv) {
            ctx_->conv->abort();
            pthread_cond_broadcast(&ctx_->queue_cond);
        }
    }
}

bool GPNvVideoDecoder::conv0_output_dqbuf_thread_callback(
    struct v4l2_buffer* v4l2_buf,
    NvBuffer* buffer,
    NvBuffer* shared_buffer,
    void* arg)
{
    GPNvVideoDecoder* decoder = static_cast<GPNvVideoDecoder*>(arg);
    VideoDecodeContext_T* ctx =
        static_cast<VideoDecodeContext_T*>(decoder->ctx_.get());
    struct v4l2_buffer dec_capture_ret_buffer;
    struct v4l2_plane planes[MAX_PLANES];

    if (!v4l2_buf) {
        SPDLOG_ERROR("Error while dequeueing conv output plane buffer");
        decoder->Abort();
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0) {
        return false;
    }

    memset(&dec_capture_ret_buffer, 0, sizeof(dec_capture_ret_buffer));
    memset(planes, 0, sizeof(planes));

    dec_capture_ret_buffer.index = shared_buffer->index;
    dec_capture_ret_buffer.m.planes = planes;
    if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
        dec_capture_ret_buffer.m.planes[0].m.fd =
            ctx->dmabuff_fd[shared_buffer->index];

    pthread_mutex_lock(&ctx->queue_lock);
    ctx->conv_output_plane_buf_queue->push(buffer);

    // Return the buffer dequeued from converter output plane
    // back to decoder capture plane
    if (ctx->dec->capture_plane.qBuffer(dec_capture_ret_buffer, NULL) < 0) {
        decoder->Abort();
        pthread_mutex_unlock(&ctx->queue_lock);
        return false;
    }

    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);

    return true;
}

bool GPNvVideoDecoder::conv0_capture_dqbuf_thread_callback(
    struct v4l2_buffer* v4l2_buf,
    NvBuffer* buffer,
    NvBuffer* shared_buffer,
    void* arg)
{
    GPNvVideoDecoder* decoder = static_cast<GPNvVideoDecoder*>(arg);
    VideoDecodeContext_T* ctx =
        static_cast<VideoDecodeContext_T*>(decoder->ctx_.get());
    GPDisplayEGL* display = dynamic_cast<GPDisplayEGL*>(
        decoder->GetChild(BeaderType::EGLDisplaySink).get());

    if (!v4l2_buf) {
        SPDLOG_ERROR("Error while dequeueing conv capture plane buffer");
        decoder->Abort();
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0) {
        return false;
    }

    // Write raw video frame to file and return the buffer to converter
    // capture plane
    if (!ctx->stats) {
        // write_video_frame(ctx->out_file, *buffer);
    }

    if (!ctx->stats && display) {
        display->Display(false, buffer->planes[0].fd);
    }

    if (ctx->conv->capture_plane.qBuffer(*v4l2_buf, NULL) < 0) {
        return false;
    }
    return true;
}

int GPNvVideoDecoder::report_input_metadata(
    v4l2_ctrl_videodec_inputbuf_metadata* input_metadata)
{
    using namespace std;

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
    using namespace std;

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

    GPDisplayEGL* display =
        dynamic_cast<GPDisplayEGL*>(GetChild(BeaderType::EGLDisplaySink).get());

    // Get capture plane format from the decoder. This may change after
    // an resolution change event
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
               "Error: Could not get format from decoder capture plane", error);

    // Get the display resolution from the decoder
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0, "Error: Could not get crop from decoder capture plane",
               error);

    SPDLOG_INFO("Video Resolution: {} x {} ", crop.c.width, crop.c.height);

    ctx_->display_height = crop.c.height;
    ctx_->display_width = crop.c.width;
    if (use_nvbuf_transform_api_) {
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
    }
    else {
        // For file write, first deinitialize output and capture planes
        // of video converter and then use the new resolution from
        // decoder event resolution change
        if (ctx_->conv) {
            ret = sendEOStoConverter();
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
    }

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
            display->Initialize(ctx_->fps, true, window_width, window_height,
                                ctx_->window_x, ctx_->window_y);
        TEST_ERROR(!renderer_error,
                   "Error in setting up renderer. "
                   "Check if X is running or run with --disable-rendering",
                   error);

        if (ctx_->stats) {
            display->enableProfiling();
        }
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
        ctx_->numCapBuffers =
            min_dec_capture_buffers + ctx_->extra_cap_plane_buffer;

        ret = dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                            ctx_->numCapBuffers, false, false);
        TEST_ERROR(ret < 0, "Error in decoder capture plane setup", error);
    }
    else if (ctx_->capture_plane_mem_type == V4L2_MEMORY_DMABUF) {
        switch (format.fmt.pix_mp.colorspace) {
            case V4L2_COLORSPACE_SMPTE170M:
                if (format.fmt.pix_mp.quantization ==
                    V4L2_QUANTIZATION_DEFAULT) {
                    SPDLOG_TRACE(
                        "Decoder colorspace ITU-R BT.601 with standard "
                        "range luma (16-235)");
                    cParams.colorFormat = NvBufferColorFormat_NV12;
                }
                else {
                    SPDLOG_TRACE(
                        "Decoder colorspace ITU-R BT.601 with extended "
                        "range luma (0-255)");
                    cParams.colorFormat = NvBufferColorFormat_NV12_ER;
                }
                break;
            case V4L2_COLORSPACE_REC709:
                if (format.fmt.pix_mp.quantization ==
                    V4L2_QUANTIZATION_DEFAULT) {
                    SPDLOG_TRACE(
                        "Decoder colorspace ITU-R BT.709 with standard "
                        "range luma (16-235)");
                    cParams.colorFormat = NvBufferColorFormat_NV12_709;
                }
                else {
                    SPDLOG_TRACE(
                        "Decoder colorspace ITU-R BT.709 with extended "
                        "range luma (0-255)");
                    cParams.colorFormat = NvBufferColorFormat_NV12_709_ER;
                }
                break;
            case V4L2_COLORSPACE_BT2020: {
                SPDLOG_TRACE("Decoder colorspace ITU-R BT.2020");
                cParams.colorFormat = NvBufferColorFormat_NV12_2020;
            } break;
            default:
                SPDLOG_TRACE(
                    "supported colorspace details not available, use default");
                if (format.fmt.pix_mp.quantization ==
                    V4L2_QUANTIZATION_DEFAULT) {
                    SPDLOG_TRACE(
                        "Decoder colorspace ITU-R BT.601 with standard "
                        "range luma (16-235)");
                    cParams.colorFormat = NvBufferColorFormat_NV12;
                }
                else {
                    SPDLOG_TRACE(
                        "Decoder colorspace ITU-R BT.601 with extended "
                        "range luma (0-255)");
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

    if (!use_nvbuf_transform_api_) {
        if (ctx_->conv) {
            ret = ctx_->conv->setOutputPlaneFormat(
                format.fmt.pix_mp.pixelformat, format.fmt.pix_mp.width,
                format.fmt.pix_mp.height, V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
            TEST_ERROR(ret < 0, "Error in converter output plane set format",
                       error);

            ret = ctx_->conv->setCapturePlaneFormat(
                (ctx_->out_pixfmt == 1 ? V4L2_PIX_FMT_NV12M
                                       : V4L2_PIX_FMT_YUV420M),
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
                V4L2_MEMORY_MMAP, dec->capture_plane.getNumBuffers(), true,
                false);
            TEST_ERROR(ret < 0, "Error in converter capture plane setup",
                       error);

            ret = ctx_->conv->output_plane.setStreamStatus(true);
            TEST_ERROR(ret < 0, "Error in converter output plane streamon",
                       error);

            ret = ctx_->conv->capture_plane.setStreamStatus(true);
            TEST_ERROR(ret < 0, "Error in converter capture plane streamon",
                       error);

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
                TEST_ERROR(ret < 0,
                           "Error Qing buffer at converter capture plane",
                           error);
            }
            ctx_->conv->output_plane.startDQThread(this);
            ctx_->conv->capture_plane.startDQThread(this);
        }
    }

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
        Abort();
        SPDLOG_ERROR("Error in {}", __func__);
    }
}

void* GPNvVideoDecoder::decoder_pollthread_fcn()
{
    VideoDecodeContext_T* ctx = ctx_.get();
    v4l2_ctrl_video_device_poll devicepoll;

    pthread_setname_np(pthread_self(), "DecoderDevicePoll");
    SPDLOG_INFO("Starting Device Poll Thread ");

    memset(&devicepoll, 0, sizeof(v4l2_ctrl_video_device_poll));

    ctx->dec->ClearPollInterrupt();

    // wait here until you are signalled to issue the Poll call.
    // Check if the abort status is set , if so exit
    // Else issue the Poll on the decoder and block.
    // When the Poll returns, signal the decoder thread to continue.

    while (!ctx->got_error && !ctx->dec->isInError()) {
        pollthread_sema_.acquire();

        if (ctx->got_eos) {
            SPDLOG_ERROR("Decoder got eos, exiting poll thread \n");
            return NULL;
        }

        devicepoll.req_events = POLLIN | POLLOUT | POLLERR | POLLPRI;

        // This call shall wait in the v4l2 decoder library
        ctx->dec->DevicePoll(&devicepoll);

        // We can check the devicepoll.resp_events bitmask to see which events
        // are set.
        decoderthread_sema_.release();
        SPDLOG_CRITICAL(
            "decoder_pollthread_fcn:devicepoll.resp_events={:x}, errno={}",
            devicepoll.resp_events, errno);
    }
    return NULL;
}

void* GPNvVideoDecoder::dec_capture_loop_fcn()
{
    VideoDecodeContext_T* ctx = ctx_.get();
    NvVideoDecoder* dec = ctx->dec;
    struct v4l2_event ev;
    int ret;
    GPDisplayEGL* display =
        dynamic_cast<GPDisplayEGL*>(GetChild(BeaderType::EGLDisplaySink).get());

    pthread_setname_np(pthread_self(), "DecCapPlane");

    SPDLOG_TRACE("Starting decoder capture loop thread");

    // Need to wait for the first Resolution change event, so that
    // the decoder knows the stream resolution and can allocate appropriate
    // buffers when we call REQBUFS
    do {
        ret = dec->dqEvent(ev, 1000);
        if (ret < 0) {
            if (errno == EAGAIN) {
                SPDLOG_ERROR(
                    "Timed out waiting for first "
                    "V4L2_EVENT_RESOLUTION_CHANGE");
            }
            else {
                SPDLOG_ERROR("Error in dequeueing decoder event");
            }
            Abort();
            break;
        }
    } while ((ev.type != V4L2_EVENT_RESOLUTION_CHANGE) && !ctx->got_error);

    // query_and_set_capture acts on the resolution change event
    if (!ctx->got_error)
        query_and_set_capture();

    // Exit on error or EOS which is signalled in main()
    while (!(ctx->got_error || dec->isInError() || ctx->got_eos)) {
        NvBuffer* dec_buffer;

        // Check for Resolution change again
        ret = dec->dqEvent(ev, 0);
        if (ret == 0) {
            switch (ev.type) {
                case V4L2_EVENT_RESOLUTION_CHANGE:
                    query_and_set_capture();
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
                    Abort();
                    SPDLOG_ERROR(
                        "Error while calling dequeue at capture plane");
                }
                break;
            }

            if (ctx->enable_metadata) {
                v4l2_ctrl_videodec_outputbuf_metadata dec_metadata;

                ret = dec->getMetadata(v4l2_buf.index, dec_metadata);
                if (ret == 0) {
                    report_metadata(&dec_metadata);
                }
            }

            if (ctx->copy_timestamp && ctx->input_nalu && ctx->stats) {
                SPDLOG_TRACE("[{}] dec capture plane dqB timestamp [{}s {}us]",
                             v4l2_buf.index, v4l2_buf.timestamp.tv_sec,
                             v4l2_buf.timestamp.tv_usec);
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
            if (display && !ctx->stats) {
                if (!use_nvbuf_transform_api_) {
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
                        dec_buffer->planes[0].fd =
                            ctx->dmabuff_fd[v4l2_buf.index];

                    if (ctx->conv->output_plane.qBuffer(conv_output_buffer,
                                                        dec_buffer) < 0) {
                        Abort();
                        SPDLOG_TRACE(
                            "Error while queueing buffer at converter output "
                            "plane");
                        break;
                    }
                }
                else {
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
                        dec_buffer->planes[0].fd =
                            ctx->dmabuff_fd[v4l2_buf.index];
                    // Convert Blocklinear to PitchLinear
                    ret = NvBufferTransform(dec_buffer->planes[0].fd,
                                            ctx->dst_dma_fd, &transform_params);
                    if (ret == -1) {
                        SPDLOG_TRACE("Transform failed");
                        break;
                    }

                    // Write raw video frame to file
                    if (!ctx->stats) {
                        // Dumping two planes of NV12 and three for I420
                        // dump_dmabuf(ctx->dst_dma_fd, 0, ctx->out_file);
                        // dump_dmabuf(ctx->dst_dma_fd, 1, ctx->out_file);
                        // if (ctx->out_pixfmt != 1) {
                        //     dump_dmabuf(ctx->dst_dma_fd, 2, ctx->out_file);
                        // }
                    }

                    if (!ctx->stats && display) {
                        display->Display(false, ctx->dst_dma_fd);
                    }

                    // Not writing to file
                    // Queue the buffer back once it has been used.
                    if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                        v4l2_buf.m.planes[0].m.fd =
                            ctx->dmabuff_fd[v4l2_buf.index];
                    if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0) {
                        Abort();
                        SPDLOG_TRACE(
                            "Error while queueing buffer at decoder capture "
                            "plane");
                        break;
                    }
                }
            }
            else {
                // Not writing to file
                // Queue the buffer back once it has been used.
                if (ctx->capture_plane_mem_type == V4L2_MEMORY_DMABUF)
                    v4l2_buf.m.planes[0].m.fd = ctx->dmabuff_fd[v4l2_buf.index];
                if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0) {
                    Abort();
                    SPDLOG_TRACE(
                        "Error while queueing buffer at decoder capture "
                        "plane");
                    break;
                }
            }
        }
    }
    if (!use_nvbuf_transform_api_) {
        // Send EOS to converter
        if (ctx->conv) {
            if (sendEOStoConverter() < 0) {
                SPDLOG_ERROR(
                    "Error while queueing EOS buffer on converter output");
            }
        }
    }
    SPDLOG_ERROR("Exiting decoder capture loop thread");
    return NULL;
}

void GPNvVideoDecoder::set_defaults()
{
    memset(ctx_.get(), 0, sizeof(VideoDecodeContext_T));
    ctx_->decoder_pixfmt = V4L2_PIX_FMT_H264;
    ctx_->fullscreen = false;
    // ctx_->window_height = 0;
    // ctx_->window_width = 0;
    ctx_->window_width = 640;
    ctx_->window_height = 480;
    ctx_->window_x = 0;
    ctx_->window_y = 0;
    ctx_->out_pixfmt = 1;
    ctx_->fps = 30;
    ctx_->output_plane_mem_type = V4L2_MEMORY_MMAP;
    ctx_->capture_plane_mem_type = V4L2_MEMORY_DMABUF;
    ctx_->vp9_file_header_flag = 0;
    ctx_->vp8_file_header_flag = 0;
    ctx_->skip_frames = V4L2_SKIP_FRAMES_TYPE_NONE;
    ctx_->copy_timestamp = false;
    ctx_->flag_copyts = false;
    ctx_->start_ts = 0;
    ctx_->dec_fps = 30;
    ctx_->dst_dma_fd = -1;
    ctx_->max_perf = 0;
    ctx_->extra_cap_plane_buffer = 1;
    if (!use_nvbuf_transform_api_) {
        ctx_->conv_output_plane_buf_queue = new std::queue<NvBuffer*>;
        ctx_->rescale_method = V4L2_YUV_RESCALE_NONE;
    }
    ctx_->blocking_mode = 1;

    pthread_mutex_init(&ctx_->queue_lock, NULL);
    pthread_cond_init(&ctx_->queue_cond, NULL);
}

bool GPNvVideoDecoder::decoder_proc_nonblocking(bool eos)
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
    struct v4l2_event ev;

    GPFileSrc* file_src =
        dynamic_cast<GPFileSrc*>(FindParent(BeaderType::FileSrc).get());
    GPDisplayEGL* display =
        dynamic_cast<GPDisplayEGL*>(GetChild(BeaderType::EGLDisplaySink).get());
    int plane_buffer_index = 0;
    int max_plane_buffer = ctx_->dec->output_plane.getNumBuffers();

    // while (!ctx_->got_error && !ctx_->dec->isInError()) {
    while (!ctx_->got_error) {
        struct v4l2_buffer v4l2_output_buf;
        struct v4l2_plane output_planes[MAX_PLANES];

        memset(&v4l2_output_buf, 0, sizeof(v4l2_output_buf));
        memset(output_planes, 0, sizeof(output_planes));
        v4l2_output_buf.m.planes = output_planes;

        pollthread_sema_.release();

        ctx_->dec->SetPollInterrupt();

        // Since buffers have been queued, issue a post to start polling and
        // then wait here

        if (!file_src) {
            std::unique_lock<std::mutex> lock(buffer_lock_);
            buffer_condition_.wait(lock,
                                   [this]() { return buffer_.size() > 0; });
            SPDLOG_CRITICAL("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb:{}",
                            buffer_.size());
        }

        int x1 = ctx_->dec->output_plane.getNumBuffers();
        int y1 = ctx_->dec->output_plane.getNumPlanes();
        int x2 = ctx_->dec->output_plane.getNumQueuedBuffers();
        int y2 = ctx_->dec->output_plane.getTotalDequeuedBuffers();
        int x3 = ctx_->dec->output_plane.getTotalQueuedBuffers();

        if (ctx_->dec->output_plane.getNumQueuedBuffers() ==
            ctx_->dec->output_plane.getNumBuffers()) {
            decoderthread_sema_.acquire();

            if (!ctx_->dec->output_plane.getStreamStatus()) {
                SPDLOG_CRITICAL("Output plane not ON \n");
            }

            ret = ctx_->dec->dqEvent(ev, 0);
            if (ret == 0) {
                if (ev.type == V4L2_EVENT_RESOLUTION_CHANGE) {
                    SPDLOG_TRACE("Got V4L2_EVENT_RESOLUTION_CHANGE EVENT \n");
                    query_and_set_capture();
                }
            }
            else if (ret < 0) {
                SPDLOG_CRITICAL("Error in dequeueing decoder event: errno={}",
                                errno);
                if (errno == EINVAL) {
                    Abort();
                }
            }
        }

        for (;;) {
            std::lock_guard<std::mutex> lock(buffer_lock_);
            NvBuffer* output_buffer = NULL;

            if (!file_src) {
                if (buffer_.size() == 0) {
                    SPDLOG_TRACE("Input buffer empty.");
                    break;
                }
            }

            if (plane_buffer_index < max_plane_buffer) {
                output_buffer =
                    ctx_->dec->output_plane.getNthBuffer(plane_buffer_index);
                v4l2_output_buf.index = plane_buffer_index;
                plane_buffer_index++;
            }
            else {
                ret = ctx_->dec->output_plane.dqBuffer(v4l2_output_buf,
                                                       &output_buffer, NULL, 0);
                if (ret < 0) {
                    if (errno == EAGAIN)
                        break;
                    else {
                        SPDLOG_ERROR("Error DQing buffer at output plane");
                        Abort();
                        break;
                    }
                }

                if ((v4l2_output_buf.flags & V4L2_BUF_FLAG_ERROR) &&
                    ctx_->enable_input_metadata) {
                    v4l2_ctrl_videodec_inputbuf_metadata dec_input_metadata;

                    ret = ctx_->dec->getInputMetadata(v4l2_output_buf.index,
                                                      dec_input_metadata);
                    if (ret == 0) {
                        ret = report_input_metadata(&dec_input_metadata);
                        if (ret == -1) {
                            SPDLOG_ERROR(
                                "Error with input stream header parsing");
                        }
                    }
                }
            }

            if ((ctx_->decoder_pixfmt == V4L2_PIX_FMT_H264) ||
                (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H265) ||
                (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG2) ||
                (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG4)) {
                if (ctx_->input_nalu) {
                    ret = read_decoder_input_nalu(output_buffer);
                }
                else {
                    ret = read_decoder_input_chunk(output_buffer);
                }
            }
            else if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP9 ||
                     ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP8) {
                ret = read_vpx_decoder_input_chunk(output_buffer);
            }
            else {
                SPDLOG_CRITICAL("The warhog is coming.");
            }

            if (ret <= 0) {
                SPDLOG_ERROR("Couldn't read chunk:{}", ret);
                break;
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

            ret = ctx_->dec->output_plane.qBuffer(v4l2_output_buf, NULL);
            if (ret < 0) {
                SPDLOG_ERROR("Error Qing buffer at output plane. errno={}",
                             errno);
                Abort();
                break;
            }

            int x1 = ctx_->dec->output_plane.getNumBuffers();
            int y1 = ctx_->dec->output_plane.getNumPlanes();
            int x2 = ctx_->dec->output_plane.getNumQueuedBuffers();
            int y2 = ctx_->dec->output_plane.getTotalDequeuedBuffers();
            int x3 = ctx_->dec->output_plane.getTotalQueuedBuffers();

            SPDLOG_CRITICAL(
                "eeeeeeeeeeeeeeeeeeeeeeeee output_plane.qBuffer, "
                "{},{},{},{},{}",
                x1, y1, x2, y2, x3);

            if (v4l2_output_buf.m.planes[0].bytesused == 0) {
                // eos = true;
                SPDLOG_INFO("Input file read complete");
                break;
            }
        }

        struct v4l2_buffer v4l2_capture_buf;
        struct v4l2_plane capture_planes[MAX_PLANES];

        NvBuffer* capture_buffer = NULL;

        memset(&v4l2_capture_buf, 0, sizeof(v4l2_capture_buf));
        memset(capture_planes, 0, sizeof(capture_planes));
        v4l2_capture_buf.m.planes = capture_planes;

        // Dequeue from the capture plane and write them to file and enqueue
        // back
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
                //     dynamic_cast<GPFileSink*>(GetChild(BeaderType::FileSink));
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

bool GPNvVideoDecoder::decoder_proc_blocking(bool eos)
{
    // Since all the output plane buffers have been queued, we first need to
    // dequeue a buffer from output plane before we can read new data into it
    // and queue it again.
    int allow_DQ = true;
    int ret = 0;
    struct v4l2_buffer temp_buf;
    VideoDecodeContext_T& ctx = *ctx_.get();
    int plane_buffer_index = 0;
    int max_plane_buffer = ctx_->dec->output_plane.getNumBuffers();

    while (!eos && !ctx.got_error && !ctx.dec->isInError()) {
        struct v4l2_buffer v4l2_output_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer* output_buffer = NULL;

        memset(&v4l2_output_buf, 0, sizeof(v4l2_output_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_output_buf.m.planes = planes;

        // std::lock_guard<std::mutex> lock(buffer_lock_);

        // if (!file_src) {
        //     if (buffer_.size() == 0) {
        //         SPDLOG_TRACE("Input buffer empty.");
        //         break;
        //     }
        // }

        if (plane_buffer_index < max_plane_buffer) {
            v4l2_output_buf.index = plane_buffer_index;
            output_buffer =
                ctx_->dec->output_plane.getNthBuffer(plane_buffer_index);
            plane_buffer_index++;
        }
        else {
            ret = ctx_->dec->output_plane.dqBuffer(v4l2_output_buf,
                                                   &output_buffer, NULL, -1);
            if (ret < 0) {
                SPDLOG_ERROR("Error DQing buffer at output plane");
                Abort();
                break;
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
        }

        if ((ctx_->decoder_pixfmt == V4L2_PIX_FMT_H264) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_H265) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG2) ||
            (ctx_->decoder_pixfmt == V4L2_PIX_FMT_MPEG4)) {
            if (ctx_->input_nalu) {
                ret = read_decoder_input_nalu(output_buffer);
            }
            else {
                ret = read_decoder_input_chunk(output_buffer);
            }
        }
        else if (ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP9 ||
                 ctx_->decoder_pixfmt == V4L2_PIX_FMT_VP8) {
            ret = read_vpx_decoder_input_chunk(output_buffer);
        }
        else {
            SPDLOG_CRITICAL("The warhog is coming.");
        }

        if (ret < 0) {
            SPDLOG_ERROR("Couldn't read chunk:{}", ret);
            break;
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

        ret = ctx_->dec->output_plane.qBuffer(v4l2_output_buf, NULL);
        if (ret < 0) {
            SPDLOG_ERROR("Error Qing buffer at output plane. errno={}", errno);
            Abort();
            break;
        }
    }

    return eos;
}

int GPNvVideoDecoder::Proc()
{
    int ret = 0;
    int error = 0;
    NvApplicationProfiler& profiler =
        NvApplicationProfiler::getProfilerInstance();
    GPDisplayEGL* display =
        dynamic_cast<GPDisplayEGL*>(GetChild(BeaderType::EGLDisplaySink).get());

    set_defaults();

    pthread_setname_np(pthread_self(), "GPNvVideoDecoderProc");

    if (ctx_->blocking_mode) {
        SPDLOG_INFO("Creating decoder in blocking mode");
        ctx_->dec = NvVideoDecoder::createVideoDecoder("dec0");
    }
    else {
        SPDLOG_INFO("Creating decoder in non-blocking mode");
        ctx_->dec = NvVideoDecoder::createVideoDecoder("dec0", O_NONBLOCK);
    }
    TEST_ERROR(!ctx_->dec, "Could not create decoder", cleanup);

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
        SPDLOG_TRACE("Setting frame input mode to 0 \n");
        ret = ctx_->dec->setFrameInputMode(0);
        TEST_ERROR(ret < 0, "Error in decoder setFrameInputMode", cleanup);
    }
    else {
        // Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
        // so that application can send chunks of encoded data instead of
        // forming complete frames.
        SPDLOG_TRACE("Setting frame input mode to 1 \n");
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
        ret = ctx_->dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true,
                                                 false);
        TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);
    }
    else if (ctx_->output_plane_mem_type == V4L2_MEMORY_USERPTR) {
        ret = ctx_->dec->output_plane.setupPlane(V4L2_MEMORY_USERPTR, 10, false,
                                                 true);
        TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);
    }

    if (!use_nvbuf_transform_api_) {
        if (display) {
            // Create converter to convert from BL to PL for writing raw video
            // to file
            ctx_->conv = NvVideoConverter::createVideoConverter("conv0");
            TEST_ERROR(!ctx_->conv, "Could not create video converter",
                       cleanup);
            ctx_->conv->output_plane.setDQThreadCallback(
                conv0_output_dqbuf_thread_callback);
            ctx_->conv->capture_plane.setDQThreadCallback(
                conv0_capture_dqbuf_thread_callback);

            if (ctx_->stats) {
                ctx_->conv->enableProfiling();
            }
        }
    }

    ret = ctx_->dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);

    if (ctx_->blocking_mode) {
        dec_capture_loop_ =
            std::thread(&GPNvVideoDecoder::dec_capture_loop_fcn, this);
    }
    else {
        decoder_poll_thread_ =
            std::thread(&GPNvVideoDecoder::decoder_pollthread_fcn, this);
    }

    ProcessData();

    if (ctx_->copy_timestamp && ctx_->input_nalu) {
        ctx_->timestamp = (ctx_->start_ts * MICROSECOND_UNIT);
        ctx_->timestampincr =
            (MICROSECOND_UNIT * 16) / ((uint32_t)(ctx_->dec_fps * 16));
    }

    if (ctx_->stats) {
        profiler.stop();
        ctx_->dec->printProfilingStats(std::cout);
        if (!use_nvbuf_transform_api_) {
            if (ctx_->conv) {
                ctx_->conv->printProfilingStats(std::cout);
            }
        }
        if (display) {
            display->printProfilingStats();
        }
        profiler.printProfilerData(std::cout);
    }

cleanup:
    if (ctx_->blocking_mode) {
        dec_capture_loop_.join();
    }
    else {
        // Clear the poll interrupt to get the decoder's poll thread out.
        ctx_->dec->ClearPollInterrupt();
        // If Pollthread is waiting on, signal it to exit the thread.
        pollthread_sema_.release();
        decoder_poll_thread_.join();
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
    if (!use_nvbuf_transform_api_) {
        if (ctx_->conv && ctx_->conv->isInError()) {
            SPDLOG_ERROR("Converter is in error");
            error = 1;
        }
    }
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
    if (!use_nvbuf_transform_api_) {
        delete ctx_->conv;
        delete ctx_->conv_output_plane_buf_queue;
    }
    else {
        if (ctx_->dst_dma_fd != -1) {
            NvBufferDestroy(ctx_->dst_dma_fd);
            ctx_->dst_dma_fd = -1;
        }
    }

    return -error;
}

void GPNvVideoDecoder::ProcessData()
{
    bool eos = false;
    int ret = 0;

    if (ctx_->blocking_mode) {
        eos = decoder_proc_blocking(eos);
    }
    else {
        eos = decoder_proc_nonblocking(eos);
    }

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
                        Abort();
                        break;
                    }
                }
            }
        }
    }

    // Signal EOS to the decoder capture loop
    ctx_->got_eos = true;
    if (!use_nvbuf_transform_api_) {
        if (ctx_->conv) {
            ctx_->conv->capture_plane.waitForDQThread(-1);
        }
    }
}

}  // namespace GPlayer