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

#include "gp_log.h"

#include "Thread.h"
#include "beader.h"
#include "nvmmapi/NvNativeBuffer.h"

using namespace Argus;
using namespace ArgusSamples;

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

class CameraRecorder : public IBeader {
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
