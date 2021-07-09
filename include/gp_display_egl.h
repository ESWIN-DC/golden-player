#ifndef __GPDISPLAY_EGL___
#define __GPDISPLAY_EGL___

#include "gp_beader.h"
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
    // EGL renderer
    NvEglRenderer* renderer_;

    // CUDA processing
    // bool enable_cuda;
    EGLDisplay egl_display_;

    std::vector<IBeader*> handlers_;
};

}  // namespace GPlayer

#endif  // __GPDISPLAY_EGL___
