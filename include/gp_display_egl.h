#ifndef __GPDISPLAY_EGL___
#define __GPDISPLAY_EGL___

#include "beader.h"
namespace GPlayer {

class Display {
};

class GPDisplayEGL : public IBeader {
public:
    GPDisplayEGL();
    ~GPDisplayEGL();

    std::string GetInfo() const;
    bool Initialize(int fps, bool enable_cuda, uint32_t width, uint32_t height);
    void Terminate();
    void Display(bool enable_cuda, int dmabuf_fd);
    void enableProfiling();
    void printProfilingStats();

public:
    EGLImageKHR GetImage(int fd);

private:
    // EGL renderer
    NvEglRenderer* renderer_;
    // int render_dmabuf_fd;
    // int fps;

    // CUDA processing
    // bool enable_cuda;
    EGLDisplay egl_display_;
    // EGLImageKHR egl_image;

    // MJPEG decoding
    // NvJPEGDecoder* jpegdec_;

    std::vector<IBeader*> handlers_;
};

}  // namespace GPlayer

#endif  // __GPDISPLAY_EGL___
