#ifndef __DISPLAY_EGL___
#define __DISPLAY_EGL___

#include "module.h"
namespace GPlayer {

class Display {
};

class GPDisplayEGL : public IModule {
public:
    GPDisplayEGL();
    ~GPDisplayEGL();

    std::string GetInfo() const;
    void AddHandler(IModule* module);
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

    std::vector<IModule*> handlers_;
};

}  // namespace GPlayer

#endif  // __DISPLAY_EGL___
