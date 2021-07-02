#ifndef __DISPLAY_EGL___
#define __DISPLAY_EGL___

#include "bead.h"
namespace GPlayer {

class Display {
};

class GPDisplayEGL : public IBead {
public:
    GPDisplayEGL();
    ~GPDisplayEGL();

    std::string GetInfo() const;
    void AddHandler(IBead* module);
    void Process(GPData* data);

private:
    bool Initialize();
    void Terminate();

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

    std::vector<IBead*> handlers_;
};

}  // namespace GPlayer

#endif  // __DISPLAY_EGL___
