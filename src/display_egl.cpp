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

GPDisplayEGL::GPDisplayEGL() : egl_display_(EGL_NO_DISPLAY)
{
    Initialize();
}

GPDisplayEGL::~GPDisplayEGL()
{
    Terminate();
}

std::string GPDisplayEGL::GetInfo() const
{
    return "GPDisplayEGL";
}

void GPDisplayEGL::AddBeader(IBeader* module)
{
    handlers_.push_back(module);
}

void GPDisplayEGL::Process(GPData* data)
{
    GPEGLImage* renderer_data = *data;

    if (renderer_data->enable_cuda) {
        // Create EGLImage from dmabuf fd
        EGLImageKHR egl_image =
            NvEGLImageFromFd(egl_display_, renderer_data->render_dmabuf_fd);
        if (egl_image == NULL) {
            SPDLOG_ERROR("Failed to map dmabuf fd (0x%X) to EGLImage",
                         renderer_data->render_dmabuf_fd);
            return;
        }

        // Running algo process with EGLImage via GPU multi cores
        HandleEGLImage(&egl_image);

        // Destroy EGLImage
        NvDestroyEGLImage(egl_display_, egl_image);
    }

    renderer_->render(renderer_data->render_dmabuf_fd);
}

bool GPDisplayEGL::Initialize()
{
    // Get default EGL display
    egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display_ == EGL_NO_DISPLAY) {
        spdlog::error("Cannot get EGL display.\n");
        return false;
    }

    return true;
}

EGLImageKHR GPDisplayEGL::GetImage(int fd)
{
    EGLImageKHR egl_image = NvEGLImageFromFd(egl_display_, fd);
    if (egl_image == NULL)
        spdlog::error("Failed to map dmabuf fd (0x{0:X}) to EGLImage", fd);

    return egl_image;
}

void GPDisplayEGL::Terminate()
{
    // Terminate EGL display
    if (egl_display_ && !eglTerminate(egl_display_))
        spdlog::error("Failed to terminate EGL display connection\n");
}

}  // namespace GPlayer
