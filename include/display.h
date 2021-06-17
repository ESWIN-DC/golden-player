#ifndef __DISPLAY___
#define __DISPLAY___

namespace GPlayer {

class Display {
};

class GPDisplay : public Display {
private:
    GPDisplay();

public:
    bool Create();
    void Start();
    void Stop();
    void Terminate();

private:
    EGLDisplay eglDisplay_;
};

}  // namespace GPlayer

#endif  // __DISPLAY___
