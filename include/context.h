
#ifndef __CONTEXT__
#define __CONTEXT__

#include <pthread.h>
#include <semaphore.h>
#include <fstream>
#include <queue>
#include "NvEglRenderer.h"
#include "NvVideoConverter.h"
#include "NvVideoDecoder.h"
#include "NvVideoEncoder.h"

namespace GPlayer {

#define USE_NVBUF_TRANSFORM_API

#define MAX_BUFFERS 32

typedef struct {
} context_t;

typedef struct : public context_t {
    NvVideoDecoder* dec;
    NvVideoConverter* conv;
    uint32_t decoder_pixfmt;

    NvEglRenderer* renderer;

    char** in_file_path;
    std::ifstream** in_file;

    char* out_file_path;
    std::ofstream* out_file;

    bool disable_rendering;
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
    uint32_t file_count;
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
    bool bLoop;
    bool bQueue;
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

    sem_t pollthread_sema;  // Polling thread waits on this to be signalled to
                            // issue Poll
    sem_t decoderthread_sema;  // Decoder thread waits on this to be signalled
                               // to continue q/dq loop
    pthread_t dec_pollthread;  // Polling thread, created if running in
                               // non-blocking mode.

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
    int blocking_mode;  // Set to true if running in blocking mode
} VideoDecodeContext_T;

#define CRC32_POLYNOMIAL 0xEDB88320L

typedef struct CrcRec {
    unsigned int CRCTable[256];
    unsigned int CrcValue;
} Crc;

typedef struct : public context_t {
    NvVideoEncoder* enc;
    uint32_t encoder_pixfmt;
    uint32_t raw_pixfmt;

    // char* in_file_path;
    // std::ifstream* in_file;

    uint32_t width;
    uint32_t height;

    // char* out_file_path;
    // std::ofstream* out_file;

    char* ROI_Param_file_path;
    char* Recon_Ref_file_path;
    char* RPS_Param_file_path;
    char* hints_Param_file_path;
    char* GDR_Param_file_path;
    char* GDR_out_file_path;
    std::ifstream* roi_Param_file;
    std::ifstream* recon_Ref_file;
    std::ifstream* rps_Param_file;
    std::ifstream* hints_Param_file;
    std::ifstream* gdr_Param_file;
    std::ofstream* gdr_out_file;

    uint32_t bitrate;
    uint32_t peak_bitrate;
    uint32_t profile;
    enum v4l2_mpeg_video_bitrate_mode ratecontrol;
    uint32_t iframe_interval;
    uint32_t idr_interval;
    enum v4l2_mpeg_video_h264_level level;
    uint32_t fps_n;
    uint32_t fps_d;
    uint32_t
        gdr_start_frame_number; /* Frame number where GDR has to be started */
    uint32_t gdr_num_frames;    /* Number of frames where GDR to be applied */
    uint32_t gdr_out_frame_number; /* Frames number from where encoded buffers
                                      are to be dumped */
    enum v4l2_enc_temporal_tradeoff_level_type temporal_tradeoff_level;
    enum v4l2_enc_hw_preset_type hw_preset_type;
    v4l2_enc_slice_length_type slice_length_type;
    uint32_t slice_length;
    uint32_t virtual_buffer_size;
    uint32_t num_reference_frames;
    uint32_t slice_intrarefresh_interval;
    uint32_t num_b_frames;
    uint32_t nMinQpI; /* Minimum QP value to use for index frames */
    uint32_t nMaxQpI; /* Maximum QP value to use for index frames */
    uint32_t nMinQpP; /* Minimum QP value to use for P frames */
    uint32_t nMaxQpP; /* Maximum QP value to use for P frames */
    uint32_t nMinQpB; /* Minimum QP value to use for B frames */
    uint32_t nMaxQpB; /* Maximum QP value to use for B frames */
    uint32_t sMaxQp;  /* Session Maximum QP value */
    int output_plane_fd[32];
    bool insert_sps_pps_at_idr;
    bool insert_vui;
    bool enable_extended_colorformat;
    bool insert_aud;
    bool alliframes;
    enum v4l2_memory output_memory_type;

    bool report_metadata;
    bool input_metadata;
    bool copy_timestamp;
    uint32_t start_ts;
    bool dump_mv;
    bool externalRPS;
    bool enableGDR;
    bool bGapsInFrameNumAllowed;
    bool bnoIframe;
    uint32_t nH264FrameNumBits;
    uint32_t nH265PocLsbBits;
    bool externalRCHints;
    bool enableROI;
    bool b_use_enc_cmd;
    bool enableLossless;
    bool got_eos;

    bool use_gold_crc;
    char gold_crc[20];
    Crc* pBitStreamCrc;

    bool bReconCrc;
    uint32_t rl; /* Reconstructed surface Left cordinate */
    uint32_t rt; /* Reconstructed surface Top cordinate */
    uint32_t rw; /* Reconstructed surface width */
    uint32_t rh; /* Reconstructed surface height */

    uint64_t timestamp;
    uint64_t timestampincr;

    std::stringstream* runtime_params_str;
    uint32_t next_param_change_frame;
    bool got_error;
    int stress_test;
    uint32_t endofstream_capture;
    uint32_t endofstream_output;

    uint32_t input_frames_queued_count;

    int max_perf;
    int blocking_mode;      // Set if running in blocking mode
    sem_t pollthread_sema;  // Polling thread waits on this to be signalled to
                            // issue Poll
    sem_t encoderthread_sema;    // Encoder thread waits on this to be signalled
                                 // to continue q/dq loop
    pthread_t enc_pollthread;    // Polling thread, created if running in
                                 // non-blocking mode.
    pthread_t enc_capture_loop;  // Encoder capture thread
} VideoEncodeContext_T;

typedef struct : public context_t {
    uint32_t encoder_pixfmt;
    uint32_t raw_pixfmt;

    uint32_t width;
    uint32_t height;

    char* ROI_Param_file_path;
    char* Recon_Ref_file_path;
    char* RPS_Param_file_path;
    char* hints_Param_file_path;
    char* GDR_Param_file_path;
    char* GDR_out_file_path;

    uint32_t bitrate;
    uint32_t peak_bitrate;
    uint32_t profile;
    uint32_t ratecontrol;

    uint32_t iframe_interval;
    uint32_t idr_interval;
    uint32_t level;
    uint32_t fps_n;
    uint32_t fps_d;
    uint32_t
        gdr_start_frame_number; /* Frame number where GDR has to be started */
    uint32_t gdr_num_frames;    /* Number of frames where GDR to be applied */
    uint32_t gdr_out_frame_number; /* Frames number from where encoded buffers
                                      are to be dumped */
    uint32_t temporal_tradeoff_level;
    uint32_t hw_preset_type;
    uint32_t slice_length_type;
    uint32_t slice_length;
    uint32_t virtual_buffer_size;
    uint32_t num_reference_frames;
    uint32_t slice_intrarefresh_interval;
    uint32_t num_b_frames;
    uint32_t nMinQpI; /* Minimum QP value to use for index frames */
    uint32_t nMaxQpI; /* Maximum QP value to use for index frames */
    uint32_t nMinQpP; /* Minimum QP value to use for P frames */
    uint32_t nMaxQpP; /* Maximum QP value to use for P frames */
    uint32_t nMinQpB; /* Minimum QP value to use for B frames */
    uint32_t nMaxQpB; /* Maximum QP value to use for B frames */
    uint32_t sMaxQp;  /* Session Maximum QP value */

    bool insert_sps_pps_at_idr;
    bool insert_vui;
    bool enable_extended_colorformat;
    bool insert_aud;
    bool alliframes;
    uint32_t output_memory_type;

    bool report_metadata;
    bool input_metadata;
    bool copy_timestamp;
    uint32_t start_ts;
    bool dump_mv;
    bool externalRPS;
    bool enableGDR;
    bool bGapsInFrameNumAllowed;
    bool bnoIframe;
    uint32_t nH264FrameNumBits;
    uint32_t nH265PocLsbBits;
    bool externalRCHints;
    bool enableROI;
    bool b_use_enc_cmd;
    bool enableLossless;
    bool got_eos;

    bool use_gold_crc;
    bool bReconCrc;
    uint32_t rl; /* Reconstructed surface Left cordinate */
    uint32_t rt; /* Reconstructed surface Top cordinate */
    uint32_t rw; /* Reconstructed surface width */
    uint32_t rh; /* Reconstructed surface height */

    uint64_t timestamp;
    uint64_t timestampincr;
    uint32_t next_param_change_frame;
    bool got_error;
    int stress_test;
    uint32_t endofstream_capture;
    uint32_t endofstream_output;

    uint32_t input_frames_queued_count;
    int max_perf;
    int blocking_mode;  // Set if running in blocking mode
} VideoEncodeConfiguratuion_T;

class VideoEncodeContext {
public:
    template <class Archive>
    void serialize(Archive& ar)
    {
        ar(context_->max_perf);
    }

private:
    std::shared_ptr<VideoEncodeConfiguratuion_T> context_;
};

int parse_csv_args(VideoDecodeContext_T* ctx, int argc, char* argv[]);
int parse_csv_args(VideoEncodeContext_T* ctx, int argc, char* argv[]);

};  // namespace GPlayer

#endif  // __CONTEXT__
