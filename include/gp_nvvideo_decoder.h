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

#include "context.h"
#include "gp_beader.h"
#include "gp_circular_buffer.h"
#include "gp_threadpool.h"
#include "gplayer.h"

#define USE_NVBUF_TRANSFORM_API
#define MAX_BUFFERS 32

namespace GPlayer {

typedef struct : public context_t {
    NvVideoDecoder* dec;
    NvVideoConverter* conv;
    uint32_t decoder_pixfmt;
    bool fullscreen;
    uint32_t window_height;
    uint32_t window_width;
    uint32_t window_x;
    uint32_t window_y;
    uint32_t out_pixfmt;
    uint32_t video_height;
    uint32_t video_width;
    uint32_t display_height;
    uint32_t display_width;
    float fps;

    bool disable_dpb;

    bool input_nalu;

    bool copy_timestamp;
    bool flag_copyts;
    uint32_t start_ts;
    float dec_fps;
    uint64_t timestamp;
    uint64_t timestampincr;

    bool stats;

    bool enable_metadata;
    // bool bLoop;
    // bool bQueue;
    bool enable_input_metadata;
    enum v4l2_skip_frames_type skip_frames;
    enum v4l2_memory output_plane_mem_type;
    enum v4l2_memory capture_plane_mem_type;
#ifndef USE_NVBUF_TRANSFORM_API
    enum v4l2_yuv_rescale_method rescale_method;
#endif

    std::queue<NvBuffer*>* conv_output_plane_buf_queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;

    pthread_t dec_capture_loop;  // Decoder capture thread, created if running
                                 // in blocking mode.
    bool got_error;
    bool got_eos;
    bool vp9_file_header_flag;
    bool vp8_file_header_flag;
    int dst_dma_fd;
    int dmabuff_fd[MAX_BUFFERS];
    int numCapBuffers;
    int loop_count;
    int max_perf;
    int extra_cap_plane_buffer;
} VideoDecodeContext_T;

class GPNvVideoDecoder : public IBeader {
private:
    GPNvVideoDecoder();

public:
    explicit GPNvVideoDecoder(
        const std::shared_ptr<VideoDecodeContext_T> context);
    ~GPNvVideoDecoder();

    std::string GetInfo() const override;
    void Process(GPData* data);
    int Proc() override;
    bool HasProc() override { return true; };

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
    void* decoder_pollthread_fcn(void);
    void set_defaults();
    bool decoder_proc_nonblocking(bool eos);
    void ProcessData();

private:
    std::shared_ptr<VideoDecodeContext_T> ctx_;
    std::thread decoder_poll_thread_;
    gp_circular_buffer<uint8_t> buffer_;
    bool decoder_is_availiable_;
    std::mutex buffer_mutex_;
    std::mutex decoder_poll_mutex_;
    std::mutex process_mutex_;
    std::condition_variable decoder_poll_condition_;
    std::condition_variable process_condition_;
};

};  // namespace GPlayer

#endif  // __GP_NVVIDEO_DECODER__
