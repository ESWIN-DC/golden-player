#ifndef __CAMERA_RECORDER__
#define __CAMERA_RECORDER__

#include <Argus/Argus.h>
#include <NvApplicationProfiler.h>
#include <NvVideoEncoder.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "gp_error.h"

#include "Thread.h"
#include "module.h"
#include "nvmmapi/NvNativeBuffer.h"

using namespace Argus;
using namespace ArgusSamples;

// Constant configuration.
static const int MAX_ENCODER_FRAMES = 5;
static const int DEFAULT_FPS = 30;
static const int Y_INDEX = 0;
static const int START_POS = 32;
static const int FONT_SIZE = 64;
static const int SHIFT_BITS = 3;
static const int array_n[8][8] = {
    {1, 1, 0, 0, 0, 0, 1, 1}, {1, 1, 1, 0, 0, 0, 1, 1},
    {1, 1, 1, 1, 0, 0, 1, 1}, {1, 1, 1, 1, 1, 0, 1, 1},
    {1, 1, 0, 1, 1, 1, 1, 1}, {1, 1, 0, 0, 1, 1, 1, 1},
    {1, 1, 0, 0, 0, 1, 1, 1}, {1, 1, 0, 0, 0, 0, 1, 1}};
static const int NUM_BUFFERS = 10;  // This value is tricky.
                                    // Too small value will impact the FPS.

// Configurations which can be overrided by cmdline
static int CAPTURE_TIME = 5;  // In seconds.
static uint32_t CAMERA_INDEX = 0;
static Size2D<uint32_t> STREAM_SIZE(640, 480);
static std::string OUTPUT_FILENAME("output.h264");
static uint32_t ENCODER_PIXFMT = V4L2_PIX_FMT_H264;
static bool DO_STAT = false;
static bool VERBOSE_ENABLE = false;
static bool DO_CPU_PROCESS = false;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

static EGLDisplay eglDisplay = EGL_NO_DISPLAY;

namespace GPlayer {

/*******************************************************************************
 * Consumer thread:
 *   Acquire frames from BufferOutputStream and extract the DMABUF fd from it.
 *   Provide DMABUF to V4L2 for video encoding. The encoder will save the
 *encoded stream to disk.
 ******************************************************************************/
class ConsumerThread : public Thread {
public:
    explicit ConsumerThread(OutputStream* stream);
    ~ConsumerThread();

    bool isInError() { return m_gotError; }

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    bool createVideoEncoder();
    void abort();

    static bool encoderCapturePlaneDqCallback(struct v4l2_buffer* v4l2_buf,
                                              NvBuffer* buffer,
                                              NvBuffer* shared_buffer,
                                              void* arg);

    OutputStream* m_stream;
    NvVideoEncoder* m_VideoEncoder;
    std::ofstream* m_outputFile;
    bool m_gotError;
};

class CameraRecorder : public IModule {
public:
    std::string GetInfo() const;
    void Process(GPData* data);
    bool Execute();

    void printHelp();

    bool parseCmdline(int argc, char** argv);

    int main(int argc, char* argv[]);
};

}  // namespace GPlayer

#endif  // __CAMERA_RECORDER__
