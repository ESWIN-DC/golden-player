#ifndef __DISPLAY_EGL___
#define __DISPLAY_EGL___

namespace GPlayer {

class Display {
};

class GPDisplayEGL : public Display {
private:
    GPDisplayEGL();
    ~GPDisplayEGL();
    bool Initialize();
    void Terminate();

public:
    EGLImageKHR GetImage(int fd);

private:
    EGLDisplay eglDisplay_;
};

}  // namespace GPlayer

#endif  // __DISPLAY_EGL___
