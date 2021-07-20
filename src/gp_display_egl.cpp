#include <NvApplicationProfiler.h>
#include <NvVideoEncoder.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "gp_log.h"

#include "Error.h"
#include "NvCudaProc.h"
#include "NvEglRenderer.h"
#include "NvUtils.h"
#include "Thread.h"
#include "nvbuf_utils.h"
#include "nvmmapi/NvNativeBuffer.h"

#include "NvJpegDecoder.h"

#include "dma_buffer.h"
#include "gp_display_egl.h"

namespace GPlayer {

GPDisplayEGLSink::GPDisplayEGLSink()
    : conv_(nullptr), renderer_(nullptr), egl_display_(EGL_NO_DISPLAY)
{
    SetProperties("GPDisplayEGLSink", "GPDisplayEGLSink",
                  BeaderType::EGLDisplaySink, true);
}

GPDisplayEGLSink::~GPDisplayEGLSink()
{
    Terminate();
}

std::string GPDisplayEGLSink::GetInfo() const
{
    return "GPDisplayEGLSink";
}

int GPDisplayEGLSink::Display(int dmabuf_fd)
{
    if (enable_cuda_) {
        // Create EGLImage from dmabuf fd
        EGLImageKHR egl_image = NvEGLImageFromFd(egl_display_, dmabuf_fd);
        if (egl_image == NULL) {
            SPDLOG_ERROR("Failed to map dmabuf fd (0x{:X}) to EGLImage",
                         dmabuf_fd);
            return -1;
        }

        // Running algo process with EGLImage via GPU multi cores
        HandleEGLImage(&egl_image);

        // Destroy EGLImage
        NvDestroyEGLImage(egl_display_, egl_image);
    }

    return renderer_->render(dmabuf_fd);
}

void GPDisplayEGLSink::enableProfiling()
{
    renderer_->enableProfiling();
}

void GPDisplayEGLSink::printProfilingStats()
{
    renderer_->printProfilingStats();
}

bool GPDisplayEGLSink::Initialize(double fps,
                                  bool enable_cuda,
                                  uint32_t x,
                                  uint32_t y,
                                  uint32_t width,
                                  uint32_t height)
{
    if (conv_) {
        delete conv_;
    }
    if (renderer_) {
        delete renderer_;
    }
    if (egl_display_ != EGL_NO_DISPLAY && !eglTerminate(egl_display_))
        SPDLOG_ERROR("Failed to terminate EGL display connection\n");

    renderer_ =
        NvEglRenderer::createEglRenderer("renderer0", width, height, x, y);
    if (!renderer_)
        ERROR_RETURN("Failed to create EGL renderer. Check if X is running");
    renderer_->setFPS(fps);

    if (enable_cuda) {
        // Get defalut EGL display
        egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (egl_display_ == EGL_NO_DISPLAY)
            ERROR_RETURN("Failed to get EGL display connection");

        // Init EGL display connection
        if (!eglInitialize(egl_display_, NULL, NULL))
            ERROR_RETURN("Failed to initialize EGL display connection");
    }

    fps_ = fps;
    enable_cuda_ = enable_cuda;
    x_ = x;
    y_ = y;
    width_ = width;
    height_ = height;

    return true;
}

void GPDisplayEGLSink::Terminate()
{
    if (egl_display_ != EGL_NO_DISPLAY && !eglTerminate(egl_display_))
        SPDLOG_ERROR("Failed to terminate EGL display connection\n");

    if (renderer_) {
        delete renderer_;
    }
}

}  // namespace GPlayer
