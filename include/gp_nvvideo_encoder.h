#ifndef __VIDEOENCODER_H__
#define __VIDEOENCODER_H__

#include <fcntl.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <poll.h>
#include <semaphore.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#include "NvUtils.h"
#include "NvVideoEncoder.h"

#include "context.h"
#include "gp_beader.h"
#include "gplayer.h"

class NvVideoEncoder;

namespace GPlayer {

#define USE_NVBUF_TRANSFORM_API

#define IS_DIGIT(c) (c >= '0' && c <= '9')

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

class GPNvVideoEncoder : public IBeader {
private:
    GPNvVideoEncoder();

public:
    explicit GPNvVideoEncoder(
        const std::shared_ptr<VideoEncodeContext_T> context);
    ~GPNvVideoEncoder();

    std::string GetInfo() const override;
    void Process(GPData* data);
    void Abort();

    int write_encoder_output_frame(std::ofstream* stream, NvBuffer* buffer)
    {
        stream->write((char*)buffer->planes[0].data,
                      buffer->planes[0].bytesused);

        return 0;
    }

    static bool encoder_capture_plane_dq_callback(struct v4l2_buffer* v4l2_buf,
                                                  NvBuffer* buffer,
                                                  NvBuffer* shared_buffer,
                                                  void* arg);

    int get_next_parsed_pair(char* id, uint32_t* value);

    int set_runtime_params();

    int get_next_runtime_param_change_frame();

    void set_defaults();

    void populate_roi_Param(std::ifstream* stream,
                            v4l2_enc_frame_ROI_params* VEnc_ROI_params);
    void populate_ext_rps_ctrl_Param(
        std::ifstream* stream,
        v4l2_enc_frame_ext_rps_ctrl_params* VEnc_ext_rps_ctrl_params);
    int setup_output_dmabuf(uint32_t num_buffers);

    void populate_ext_rate_ctrl_Param(
        std::ifstream* stream,
        v4l2_enc_frame_ext_rate_ctrl_params* VEnc_ext_rate_ctrl_params);

    void populate_gdr_Param(std::ifstream* stream,
                            uint32_t* start_frame_num,
                            uint32_t* gdr_num_frames);

    static void* encoder_pollthread_fcn(void* arg);
    int encoder_proc_nonblocking(bool eos);
    int encoder_proc_blocking(bool eos);
    int Proc() override;
    bool HasProc() override { return true; };
    static int encodeProc(GPNvVideoEncoder* encoder);
    int ReadFrame(NvBuffer& buffer);

    bool SaveConfiguration(const std::string& configuration);
    bool LoadConfiguration();

private:
    std::shared_ptr<VideoEncodeContext_T> ctx_;
    std::vector<GPData*> frames_;
    std::mutex frames_mutex_;
};  // class GPNvVideoEncoder

}  // namespace GPlayer

#endif  // __VIDEOENCODER_H__
