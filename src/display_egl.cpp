#include <NvApplicationProfiler.h>
#include <NvVideoEncoder.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include <spdlog/spdlog.h>

#include "Error.h"
#include "Thread.h"
#include "nvmmapi/NvNativeBuffer.h"

#include "display_egl.h"
#include "dma_buffer.h"

namespace GPlayer {

GPDisplayEGL::GPDisplayEGL() : eglDisplay_(EGL_NO_DISPLAY)
{
    Initialize();
}

GPDisplayEGL::~GPDisplayEGL()
{
    Terminate();
}

bool GPDisplayEGL::Initialize()
{
    // Get default EGL display
    eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay_ == EGL_NO_DISPLAY) {
        spdlog::error("Cannot get EGL display.\n");
        return false;
    }

    return true;
}

EGLImageKHR GPDisplayEGL::GetImage(int fd)
{
    EGLImageKHR egl_image = NvEGLImageFromFd(eglDisplay_, fd);
    if (egl_image == NULL)
        spdlog::error("Failed to map dmabuf fd (0x{0:X}) to EGLImage", fd);

    return egl_image;
}

void GPDisplayEGL::Terminate()
{
    // Terminate EGL display
    if (eglDisplay_ && !eglTerminate(eglDisplay_))
        spdlog::error("Failed to terminate EGL display connection\n");
}

}  // namespace GPlayer
