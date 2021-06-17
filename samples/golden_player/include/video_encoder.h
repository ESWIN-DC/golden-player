

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

#include "NvUtils.h"
#include "context.h"
#include "gplayer.h"
#include "nvbuf_utils.h"

#include "NvVideoEncoder.h"

namespace GPlayer {

#define TEST_ERROR(cond, str, label) \
    if (cond) {                      \
        cerr << str << endl;         \
        error = 1;                   \
        goto label;                  \
    }

#define TEST_PARSE_ERROR(cond, label)                                     \
    if (cond) {                                                           \
        cerr << "Error parsing runtime parameter changes string" << endl; \
        goto label;                                                       \
    }

#define IS_DIGIT(c) (c >= '0' && c <= '9')
#define MICROSECOND_UNIT 1000000

using namespace std;

class VideoEncoder {
private:
    VideoEncoder();

public:
    VideoEncoder(const shared_ptr<VideoEncodeContext_T> context);

    void Abort();

    // Initialise CRC Rec and creates CRC Table based on the polynomial.
    Crc* InitCrc(unsigned int CrcPolynomial)
    {
        unsigned short int i;
        unsigned short int j;
        unsigned int tempcrc;
        Crc* phCrc;
        phCrc = (Crc*)malloc(sizeof(Crc));
        if (phCrc == NULL) {
            cerr << "Mem allocation failed for Init CRC" << endl;
            return NULL;
        }

        memset(phCrc, 0, sizeof(Crc));

        for (i = 0; i <= 255; i++) {
            tempcrc = i;
            for (j = 8; j > 0; j--) {
                if (tempcrc & 1) {
                    tempcrc = (tempcrc >> 1) ^ CrcPolynomial;
                }
                else {
                    tempcrc >>= 1;
                }
            }
            phCrc->CRCTable[i] = tempcrc;
        }

        phCrc->CrcValue = 0;
        return phCrc;
    }

    // Calculates CRC of data provided in by buffer.

    void CalculateCrc(Crc* phCrc, unsigned char* buffer, uint32_t count)
    {
        unsigned char* p;
        unsigned int temp1;
        unsigned int temp2;
        unsigned int crc = phCrc->CrcValue;
        unsigned int* CRCTable = phCrc->CRCTable;

        if (!count)
            return;

        p = (unsigned char*)buffer;
        while (count-- != 0) {
            temp1 = (crc >> 8) & 0x00FFFFFFL;
            temp2 = CRCTable[((unsigned int)crc ^ *p++) & 0xFF];
            crc = temp1 ^ temp2;
        }

        phCrc->CrcValue = crc;
    }

    // Closes CRC related handles.

    void CloseCrc(Crc** phCrc)
    {
        if (*phCrc)
            free(*phCrc);
    }

    int write_encoder_output_frame(ofstream* stream, NvBuffer* buffer)
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

    int encode_proc(int argc, char* argv[]);

private:
    shared_ptr<VideoEncodeContext_T> ctx_;
};  // class VideoEncoder

}  // namespace GPlayer

#endif  // __VIDEOENCODER_H__