#ifndef __GPDISPLAY_EGL___
#define __GPDISPLAY_EGL___

#include <queue>

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "NvEglRenderer.h"
#include "NvVideoConverter.h"
#include "NvVideoDecoder.h"

#include "gp_beader.h"
#include "nvosd.h"

namespace GPlayer {

class Display {
};

class GPDisplayEGLSink : public IBeader {
public:
    GPDisplayEGLSink();
    ~GPDisplayEGLSink();

    std::string GetInfo() const override;
    bool HasProc() override { return false; };
    bool Initialize(double fps,
                    bool enable_cuda,
                    uint32_t x = 0,
                    uint32_t y = 0,
                    uint32_t width = 640,
                    uint32_t height = 480);
    void Terminate();
    int Display(int dmabuf_fd);
    void enableProfiling();
    void printProfilingStats();

public:
private:
    double fps_ = 0.0;
    bool enable_cuda_ = false;
    uint32_t x_ = 0, y_ = 0;
    uint32_t width_ = 0, height_ = 0;

    NvVideoConverter* conv_;
    NvEglRenderer* renderer_;
    EGLDisplay egl_display_;

    bool enable_osd;
    bool enable_osd_text;
    char* osd_file_path;
    std::ifstream* osd_file;
    NvOSD_RectParams g_rect[100];
    int g_rect_num;

    char* osd_text;
    NvOSD_TextParams textParams;
};

}  // namespace GPlayer

#endif  // __GPDISPLAY_EGL___
