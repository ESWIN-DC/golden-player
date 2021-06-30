
#include <fstream>
#include <iostream>

#include <spdlog/spdlog.h>

#include "camera_recorder.h"
#include "dma_buffer.h"

using namespace Argus;

namespace GPlayer {

ConsumerThread::ConsumerThread(OutputStream* stream)
    : m_stream(stream),
      m_VideoEncoder(NULL),
      m_outputFile(NULL),
      m_gotError(false)
{
}

ConsumerThread::~ConsumerThread()
{
    if (m_VideoEncoder)
        delete m_VideoEncoder;

    if (m_outputFile)
        delete m_outputFile;
}

bool ConsumerThread::threadInitialize()
{
    // Create Video Encoder
    if (!createVideoEncoder())
        GP_ORIGINATE_ERROR("Failed to create video m_VideoEncoderoder");

    // Create output file
    m_outputFile = new std::ofstream(OUTPUT_FILENAME.c_str());
    if (!m_outputFile)
        GP_ORIGINATE_ERROR("Failed to open output file.");

    // Stream on
    int e = m_VideoEncoder->output_plane.setStreamStatus(true);
    if (e < 0)
        GP_ORIGINATE_ERROR("Failed to stream on output plane");
    e = m_VideoEncoder->capture_plane.setStreamStatus(true);
    if (e < 0)
        GP_ORIGINATE_ERROR("Failed to stream on capture plane");

    // Set video encoder callback
    m_VideoEncoder->capture_plane.setDQThreadCallback(
        encoderCapturePlaneDqCallback);

    // startDQThread starts a thread internally which calls the
    // encoderCapturePlaneDqCallback whenever a buffer is dequeued
    // on the plane
    m_VideoEncoder->capture_plane.startDQThread(this);

    // Enqueue all the empty capture plane buffers
    for (uint32_t i = 0; i < m_VideoEncoder->capture_plane.getNumBuffers();
         i++) {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        CHECK_ERROR(m_VideoEncoder->capture_plane.qBuffer(v4l2_buf, NULL));
    }

    return true;
}

bool ConsumerThread::threadExecute()
{
    IBufferOutputStream* stream = interface_cast<IBufferOutputStream>(m_stream);
    if (!stream)
        GP_ORIGINATE_ERROR("Failed to get IBufferOutputStream interface");

    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));
    v4l2_buf.m.planes = planes;

    for (int bufferIndex = 0; bufferIndex < MAX_ENCODER_FRAMES; bufferIndex++) {
        v4l2_buf.index = bufferIndex;
        Buffer* buffer = stream->acquireBuffer();
        // Convert Argus::Buffer to DmaBuffer and queue into v4l2 encoder
        DmaBuffer* dmabuf = DmaBuffer::fromArgusBuffer(buffer);
        CHECK_ERROR(m_VideoEncoder->output_plane.qBuffer(v4l2_buf, dmabuf));
    }

    // Keep acquire frames and queue into encoder
    while (!m_gotError) {
        NvBuffer* share_buffer;

        // Dequeue from encoder first.
        CHECK_ERROR(m_VideoEncoder->output_plane.dqBuffer(
            v4l2_buf, NULL, &share_buffer, 10 /*retry*/));
        // Release the frame.
        DmaBuffer* dmabuf = static_cast<DmaBuffer*>(share_buffer);
        stream->releaseBuffer(dmabuf->getArgusBuffer());

        assert(dmabuf->getFd() == v4l2_buf.m.planes[0].m.fd);

        if (VERBOSE_ENABLE)
            CONSUMER_PRINT("Released frame. %d\n", dmabuf->getFd());

        // Acquire a Buffer from a completed capture request.
        Argus::Status status = STATUS_OK;
        Buffer* buffer = stream->acquireBuffer(TIMEOUT_INFINITE, &status);
        if (status == STATUS_END_OF_STREAM) {
            // Timeout or error happen, exit.
            break;
        }

        // Convert Argus::Buffer to DmaBuffer and get FD
        dmabuf = DmaBuffer::fromArgusBuffer(buffer);
        int dmabuf_fd = dmabuf->getFd();

        if (VERBOSE_ENABLE)
            CONSUMER_PRINT("Acquired Frame. %d\n", dmabuf_fd);

        if (DO_CPU_PROCESS) {
            NvBufferParams par;
            NvBufferGetParams(dmabuf_fd, &par);
            void* ptr_y;
            uint8_t* ptr_cur;
            int i, j, a, b;
            NvBufferMemMap(dmabuf_fd, Y_INDEX, NvBufferMem_Write, &ptr_y);
            NvBufferMemSyncForCpu(dmabuf_fd, Y_INDEX, &ptr_y);
            ptr_cur =
                (uint8_t*)ptr_y + par.pitch[Y_INDEX] * START_POS + START_POS;

            // overwrite some pixels to put an 'N' on each Y plane
            // scan array_n to decide which pixel should be overwritten
            for (i = 0; i < FONT_SIZE; i++) {
                for (j = 0; j < FONT_SIZE; j++) {
                    a = i >> SHIFT_BITS;
                    b = j >> SHIFT_BITS;
                    if (array_n[a][b])
                        (*ptr_cur) = 0xff;  // white color
                    ptr_cur++;
                }
                ptr_cur = (uint8_t*)ptr_y +
                          par.pitch[Y_INDEX] * (START_POS + i) + START_POS;
            }
            NvBufferMemSyncForDevice(dmabuf_fd, Y_INDEX, &ptr_y);
            NvBufferMemUnMap(dmabuf_fd, Y_INDEX, &ptr_y);
        }

        // Push the frame into V4L2.
        CHECK_ERROR(m_VideoEncoder->output_plane.qBuffer(v4l2_buf, dmabuf));
    }

    // Print profile result before EOS to make FPS more accurate
    // Otherwise, the total duration will include timeout period which makes the
    // FPS a bit lower.
    if (DO_STAT)
        m_VideoEncoder->printProfilingStats(std::cout);

    // Send EOS
    v4l2_buf.m.planes[0].m.fd = -1;
    v4l2_buf.m.planes[0].bytesused = 0;
    CHECK_ERROR(m_VideoEncoder->output_plane.qBuffer(v4l2_buf, NULL));

    // Wait till capture plane DQ Thread finishes
    // i.e. all the capture plane buffers are dequeued
    m_VideoEncoder->capture_plane.waitForDQThread(2000);

    CONSUMER_PRINT("Done.\n");

    requestShutdown();

    return true;
}

bool ConsumerThread::threadShutdown()
{
    return true;
}

bool ConsumerThread::createVideoEncoder()
{
    int ret = 0;

    m_VideoEncoder = NvVideoEncoder::createVideoEncoder("enc0");
    if (!m_VideoEncoder)
        GP_ORIGINATE_ERROR("Could not create m_VideoEncoderoder");

    if (DO_STAT)
        m_VideoEncoder->enableProfiling();

    ret = m_VideoEncoder->setCapturePlaneFormat(
        ENCODER_PIXFMT, STREAM_SIZE.width(), STREAM_SIZE.height(),
        2 * 1024 * 1024);
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not set capture plane format");

    ret = m_VideoEncoder->setOutputPlaneFormat(
        V4L2_PIX_FMT_YUV420M, STREAM_SIZE.width(), STREAM_SIZE.height());
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not set output plane format");

    ret = m_VideoEncoder->setBitrate(4 * 1024 * 1024);
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not set bitrate");

    if (ENCODER_PIXFMT == V4L2_PIX_FMT_H264) {
        ret = m_VideoEncoder->setProfile(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
    }
    else {
        ret = m_VideoEncoder->setProfile(V4L2_MPEG_VIDEO_H265_PROFILE_MAIN);
    }
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not set m_VideoEncoderoder profile");

    if (ENCODER_PIXFMT == V4L2_PIX_FMT_H264) {
        ret = m_VideoEncoder->setLevel(V4L2_MPEG_VIDEO_H264_LEVEL_5_0);
        if (ret < 0)
            GP_ORIGINATE_ERROR("Could not set m_VideoEncoderoder level");
    }

    ret = m_VideoEncoder->setRateControlMode(V4L2_MPEG_VIDEO_BITRATE_MODE_CBR);
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not set rate control mode");

    ret = m_VideoEncoder->setIFrameInterval(30);
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not set I-frame interval");

    ret = m_VideoEncoder->setFrameRate(30, 1);
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not set m_VideoEncoderoder framerate");

    ret = m_VideoEncoder->setHWPresetType(V4L2_ENC_HW_PRESET_ULTRAFAST);
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not set m_VideoEncoderoder HW Preset");

    // Query, Export and Map the output plane buffers so that we can read
    // raw data into the buffers
    ret = m_VideoEncoder->output_plane.setupPlane(V4L2_MEMORY_DMABUF, 10, true,
                                                  false);
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not setup output plane");

    // Query, Export and Map the output plane buffers so that we can write
    // m_VideoEncoderoded data from the buffers
    ret = m_VideoEncoder->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 6, true,
                                                   false);
    if (ret < 0)
        GP_ORIGINATE_ERROR("Could not setup capture plane");

    spdlog::info("create video encoder return true\n");
    return true;
}

void ConsumerThread::abort()
{
    m_VideoEncoder->abort();
    m_gotError = true;
}

bool ConsumerThread::encoderCapturePlaneDqCallback(struct v4l2_buffer* v4l2_buf,
                                                   NvBuffer* buffer,
                                                   NvBuffer* shared_buffer,
                                                   void* arg)
{
    ConsumerThread* thiz = (ConsumerThread*)arg;

    if (!v4l2_buf) {
        thiz->abort();
        GP_ORIGINATE_ERROR(
            "Failed to dequeue buffer from encoder capture plane");
    }

    thiz->m_outputFile->write((char*)buffer->planes[0].data,
                              buffer->planes[0].bytesused);

    if (thiz->m_VideoEncoder->capture_plane.qBuffer(*v4l2_buf, NULL) < 0) {
        thiz->abort();
        GP_ORIGINATE_ERROR("Failed to enqueue buffer to encoder capture plane");
        return false;
    }

    // GOT EOS from m_VideoEncoderoder. Stop dqthread.
    if (buffer->planes[0].bytesused == 0) {
        CONSUMER_PRINT("Got EOS, exiting...\n");
        return false;
    }

    return true;
}

std::string CameraRecorder::GetInfo() const
{
    return "CameraRecorder";
}

/*******************************************************************************
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates an BufferOutputStream to output
 *   frames, then performs repeating capture requests for CAPTURE_TIME
 *   seconds before closing the producer and Argus driver.
 ******************************************************************************/
bool CameraRecorder::Execute()
{
    // Create the CameraProvider object and get the core interface.
    UniqueObj<CameraProvider> cameraProvider =
        UniqueObj<CameraProvider>(CameraProvider::create());
    ICameraProvider* iCameraProvider =
        interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        GP_ORIGINATE_ERROR("Failed to create CameraProvider");

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        GP_ORIGINATE_ERROR("No cameras available");

    if (CAMERA_INDEX >= cameraDevices.size()) {
        PRODUCER_PRINT("CAMERA_INDEX out of range. Fall back to 0\n");
        CAMERA_INDEX = 0;
    }

    // Create the capture session using the first device and get the core
    // interface.
    UniqueObj<CaptureSession> captureSession(
        iCameraProvider->createCaptureSession(cameraDevices[CAMERA_INDEX]));
    ICaptureSession* iCaptureSession =
        interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        GP_ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    // Create the OutputStream.
    PRODUCER_PRINT("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_BUFFER));
    IBufferOutputStreamSettings* iStreamSettings =
        interface_cast<IBufferOutputStreamSettings>(streamSettings);
    if (!iStreamSettings)
        GP_ORIGINATE_ERROR(
            "Failed to get IBufferOutputStreamSettings interface");

    // Configure the OutputStream to use the EGLImage BufferType.
    iStreamSettings->setBufferType(BUFFER_TYPE_EGL_IMAGE);

    // Create the OutputStream.
    UniqueObj<OutputStream> outputStream(
        iCaptureSession->createOutputStream(streamSettings.get()));
    IBufferOutputStream* iBufferOutputStream =
        interface_cast<IBufferOutputStream>(outputStream);

    // Allocate native buffers.
    DmaBuffer* nativeBuffers[NUM_BUFFERS];

    for (uint32_t i = 0; i < NUM_BUFFERS; i++) {
        nativeBuffers[i] = DmaBuffer::create(
            STREAM_SIZE, NvBufferColorFormat_NV12,
            DO_CPU_PROCESS ? NvBufferLayout_Pitch : NvBufferLayout_BlockLinear);
        if (!nativeBuffers[i])
            GP_ORIGINATE_ERROR("Failed to allocate NativeBuffer");
    }

    // Create EGLImages from the native buffers.
    EGLImageKHR eglImages[NUM_BUFFERS];
    for (uint32_t i = 0; i < NUM_BUFFERS; i++) {
        eglImages[i] = nativeBuffers[i]->createEGLImage(eglDisplay);
        if (eglImages[i] == EGL_NO_IMAGE_KHR)
            GP_ORIGINATE_ERROR("Failed to create EGLImage");
    }

    // Create the BufferSettings object to configure Buffer creation.
    UniqueObj<BufferSettings> bufferSettings(
        iBufferOutputStream->createBufferSettings());
    IEGLImageBufferSettings* iBufferSettings =
        interface_cast<IEGLImageBufferSettings>(bufferSettings);
    if (!iBufferSettings)
        GP_ORIGINATE_ERROR("Failed to create BufferSettings");

    // Create the Buffers for each EGLImage (and release to stream for initial
    // capture use).
    UniqueObj<Buffer> buffers[NUM_BUFFERS];
    for (uint32_t i = 0; i < NUM_BUFFERS; i++) {
        iBufferSettings->setEGLImage(eglImages[i]);
        iBufferSettings->setEGLDisplay(eglDisplay);
        buffers[i].reset(
            iBufferOutputStream->createBuffer(bufferSettings.get()));
        IBuffer* iBuffer = interface_cast<IBuffer>(buffers[i]);

        // Reference Argus::Buffer and DmaBuffer each other
        iBuffer->setClientData(nativeBuffers[i]);
        nativeBuffers[i]->setArgusBuffer(buffers[i].get());

        if (!interface_cast<IEGLImageBuffer>(buffers[i]))
            GP_ORIGINATE_ERROR("Failed to create Buffer");
        if (iBufferOutputStream->releaseBuffer(buffers[i].get()) != STATUS_OK)
            GP_ORIGINATE_ERROR("Failed to release Buffer for capture use");
    }

    // Launch the FrameConsumer thread to consume frames from the OutputStream.
    PRODUCER_PRINT("Launching consumer thread\n");
    ConsumerThread frameConsumerThread(outputStream.get());
    GP_PROPAGATE_ERROR(frameConsumerThread.initialize());

    // Wait until the consumer is connected to the stream.
    GP_PROPAGATE_ERROR(frameConsumerThread.waitRunning());

    // Create capture request and enable output stream.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest* iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        GP_ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(outputStream.get());

    ISourceSettings* iSourceSettings =
        interface_cast<ISourceSettings>(iRequest->getSourceSettings());
    if (!iSourceSettings)
        GP_ORIGINATE_ERROR("Failed to get ISourceSettings interface");
    iSourceSettings->setFrameDurationRange(Range<uint64_t>(1e9 / DEFAULT_FPS));

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        GP_ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait for CAPTURE_TIME seconds.
    for (int i = 0; i < CAPTURE_TIME && !frameConsumerThread.isInError(); i++)
        sleep(1);

    // Stop the repeating request and wait for idle.
    iCaptureSession->stopRepeat();
    iBufferOutputStream->endOfStream();
    iCaptureSession->waitForIdle();

    // Wait for the consumer thread to complete.
    GP_PROPAGATE_ERROR(frameConsumerThread.shutdown());

    // Destroy the output stream to end the consumer thread.
    outputStream.reset();

    // Destroy the EGLImages.
    for (uint32_t i = 0; i < NUM_BUFFERS; i++)
        NvDestroyEGLImage(NULL, eglImages[i]);

    // Destroy the native buffers.
    for (uint32_t i = 0; i < NUM_BUFFERS; i++)
        delete nativeBuffers[i];

    PRODUCER_PRINT("Done -- exiting.\n");

    return true;
}

void CameraRecorder::printHelp()
{
    printf(
        "Usage: camera_recording [OPTIONS]\n"
        "Options:\n"
        "  -r        Set output resolution WxH [Default 640x480]\n"
        "  -f        Set output filename [Default output.h264]\n"
        "  -t        Set encoder type H264 or H265 [Default H264]\n"
        "  -d        Set capture duration [Default 5 seconds]\n"
        "  -i        Set camera index [Default 0]\n"
        "  -s        Enable profiling\n"
        "  -v        Enable verbose message\n"
        "  -c        Enable demonstration of CPU processing\n"
        "  -h        Print this help\n");
}

bool CameraRecorder::parseCmdline(int argc, char** argv)
{
    int c, w, h;
    bool haveFilename = false;
    while ((c = getopt(argc, argv, "r:f:t:d:i:s::v::c::h")) != -1) {
        switch (c) {
            case 'r':
                if (sscanf(optarg, "%dx%d", &w, &h) != 2)
                    return false;
                STREAM_SIZE.width() = w;
                STREAM_SIZE.height() = h;
                break;
            case 'f':
                OUTPUT_FILENAME = optarg;
                haveFilename = true;
                break;
            case 't':
                if (strcmp(optarg, "H264") == 0)
                    ENCODER_PIXFMT = V4L2_PIX_FMT_H264;
                else if (strcmp(optarg, "H265") == 0) {
                    ENCODER_PIXFMT = V4L2_PIX_FMT_H265;
                    if (!haveFilename)
                        OUTPUT_FILENAME = "output.h265";
                }
                else
                    return false;
                break;
            case 'd':
                CAPTURE_TIME = atoi(optarg);
                break;
            case 'i':
                CAMERA_INDEX = atoi(optarg);
                break;
            case 's':
                DO_STAT = true;
                break;
            case 'v':
                VERBOSE_ENABLE = true;
                break;
            case 'c':
                DO_CPU_PROCESS = true;
                break;
            default:
                return false;
        }
    }
    return true;
}

int CameraRecorder::main(int argc, char* argv[])
{
    spdlog::info("Start camera...\n");

    if (!parseCmdline(argc, argv)) {
        printHelp();
        return EXIT_FAILURE;
    }

    NvApplicationProfiler& profiler =
        NvApplicationProfiler::getProfilerInstance();

    // Get default EGL display
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        spdlog::error("Cannot get EGL display.\n");
        return EXIT_FAILURE;
    }

    if (!Execute())
        return EXIT_FAILURE;

    // Terminate EGL display
    eglTerminate(eglDisplay);

    profiler.stop();
    profiler.printProfilerData(std::cout);

    return EXIT_SUCCESS;
}

}  // namespace GPlayer
