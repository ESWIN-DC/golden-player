#include <NvApplicationProfiler.h>
#include <NvVideoEncoder.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include <spdlog/spdlog.h>

#include "Error.h"
#include "NvCudaProc.h"
#include "NvEglRenderer.h"
#include "NvUtils.h"
#include "Thread.h"
#include "nvbuf_utils.h"
#include "nvmmapi/NvNativeBuffer.h"

#include "NvJpegDecoder.h"

#include "display_egl.h"
#include "dma_buffer.h"

namespace GPlayer {

GPDisplayEGL::GPDisplayEGL() : egl_display_(EGL_NO_DISPLAY) {}

GPDisplayEGL::~GPDisplayEGL()
{
    Terminate();
}

std::string GPDisplayEGL::GetInfo() const
{
    return "GPDisplayEGL";
}

void GPDisplayEGL::Display(bool enable_cuda, int dmabuf_fd)
{
    if (enable_cuda) {
        // Create EGLImage from dmabuf fd
        EGLImageKHR egl_image = NvEGLImageFromFd(egl_display_, dmabuf_fd);
        if (egl_image == NULL) {
            SPDLOG_ERROR("Failed to map dmabuf fd (0x%X) to EGLImage",
                         dmabuf_fd);
            return;
        }

        // Running algo process with EGLImage via GPU multi cores
        HandleEGLImage(&egl_image);

        // Destroy EGLImage
        NvDestroyEGLImage(egl_display_, egl_image);
    }

    renderer_->render(dmabuf_fd);
}

void GPDisplayEGL::enableProfiling()
{
    renderer_->enableProfiling();
}

void GPDisplayEGL::printProfilingStats()
{
    renderer_->printProfilingStats();
}

bool GPDisplayEGL::Initialize(int fps,
                              bool enable_cuda,
                              uint32_t width,
                              uint32_t height)
{
    // Create EGL renderer
    renderer_ =
        NvEglRenderer::createEglRenderer("renderer0", width, height, 0, 0);
    if (!renderer_)
        ERROR_RETURN("Failed to create EGL renderer");
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

    return true;
}

EGLImageKHR GPDisplayEGL::GetImage(int fd)
{
    EGLImageKHR egl_image = NvEGLImageFromFd(egl_display_, fd);
    if (egl_image == NULL)
        SPDLOG_ERROR("Failed to map dmabuf fd (0x{0:X}) to EGLImage", fd);

    return egl_image;
}

void GPDisplayEGL::Terminate()
{
    // Terminate EGL display
    if (egl_display_ && !eglTerminate(egl_display_))
        SPDLOG_ERROR("Failed to terminate EGL display connection\n");
}

}  // namespace GPlayer
