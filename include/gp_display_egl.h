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

class GPDisplayEGL : public IBeader {
public:
    GPDisplayEGL();
    ~GPDisplayEGL();

    std::string GetInfo() const override;
    bool HasProc() override { return false; };
    bool Initialize(int fps,
                    bool enable_cuda,
                    uint32_t width,
                    uint32_t height,
                    uint32_t x = 0,
                    uint32_t y = 0);
    void Terminate();
    int Display(bool enable_cuda, int dmabuf_fd);
    void enableProfiling();
    void printProfilingStats();

public:
    EGLImageKHR GetImage(int fd);

private:
    NvVideoConverter* conv_;
    NvEglRenderer* renderer_;

    // CUDA processing
    // bool enable_cuda;
    EGLDisplay egl_display_;

    std::vector<IBeader*> handlers_;

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
