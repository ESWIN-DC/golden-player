#ifndef __GP_NVVIDEO_DECODER__
#define __GP_NVVIDEO_DECODER__

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>

#include <nvbuf_utils.h>

#include "NvApplicationProfiler.h"
#include "NvUtils.h"

#include "beader.h"
#include "context.h"
#include "gp_circular_buffer.h"
#include "gplayer.h"

#define TEST_ERROR(cond, str, label) \
    if (cond) {                      \
        cerr << str << endl;         \
        error = 1;                   \
        goto label;                  \
    }

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

namespace GPlayer {

using namespace std;

class GPNvVideoDecoder : public IBeader {
private:
    GPNvVideoDecoder();

public:
    explicit GPNvVideoDecoder(const shared_ptr<VideoDecodeContext_T> context);
    ~GPNvVideoDecoder();

    std::string GetInfo() const override;

    void Process(GPData* data);

private:
    int read_decoder_input_nalu(NvBuffer* buffer);

    int read_decoder_input_chunk(NvBuffer* buffer);

    int read_vpx_decoder_input_chunk(NvBuffer* buffer);

    void Abort();

#ifndef USE_NVBUF_TRANSFORM_API
    bool conv0_output_dqbuf_thread_callback(struct v4l2_buffer* v4l2_buf,
                                            NvBuffer* buffer,
                                            NvBuffer* shared_buffer,
                                            void* arg);

    bool conv0_capture_dqbuf_thread_callback(struct v4l2_buffer* v4l2_buf,
                                             NvBuffer* buffer,
                                             NvBuffer* shared_buffer,
                                             void* arg);
#endif

    int report_input_metadata(
        v4l2_ctrl_videodec_inputbuf_metadata* input_metadata);

    void report_metadata(v4l2_ctrl_videodec_outputbuf_metadata* metadata);

#ifndef USE_NVBUF_TRANSFORM_API
    int sendEOStoConverter();
#endif

    void query_and_set_capture();

    static void* decoder_pollthread_fcn(void* arg);

    static void* dec_capture_loop_fcn(void* arg);

    void set_defaults();

    bool decoder_proc_nonblocking(bool eos,
                                  uint32_t current_file,
                                  int current_loop);
    bool decoder_proc_blocking(bool eos,
                               uint32_t current_file,
                               int current_loop);
    int Proc() override;
    bool HasProc() override { return true; };

    static int decodeProc(GPNvVideoDecoder* decoder);

private:
    std::shared_ptr<VideoDecodeContext_T> ctx_;
    gp_circular_buffer<uint8_t> buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable thread_condition_;
    std::thread decode_thread_;
};

};  // namespace GPlayer

#endif  // __GP_NVVIDEO_DECODER__
