#include <Argus/Argus.h>
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

#include "display.h"
#include "dma_buffer.h"

using namespace Argus;
using namespace ArgusSamples;

namespace GPlayer {

GPDisplay::GPDisplay() : eglDisplay_(EGL_NO_DISPLAY) {}

bool GPDisplay::Create()
{
    // Get default EGL display
    eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay_ == EGL_NO_DISPLAY) {
        spdlog::error("Cannot get EGL display.\n");
        return false;
    }

    return true;
}

void GPDisplay::Start() {}
void GPDisplay::Stop() {}

void GPDisplay::Terminate()
{
    // Terminate EGL display
    eglTerminate(eglDisplay_);
}

}  // namespace GPlayer
