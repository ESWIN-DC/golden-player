#ifndef __GP_DATA__
#define __GP_DATA__

#include <cstdint>

namespace GPlayer {

class GPBuffer {
public:
    GPBuffer(void* data, uint32_t length) : data_(data), length_(length) {}

    void* GetData() const { return data_; }
    uint32_t GetLength() const { return length_; }

private:
    GPBuffer();

private:
    void* data_;
    uint32_t length_;
};

class GPEGLImage {
public:
    bool enable_cuda;
    int render_dmabuf_fd;
};

class GPData {
public:
    typedef enum { BUFFER, IMAGE } DataType;

public:
    GPData(GPBuffer* buffer) : type_(BUFFER), gpbuffer(buffer) {}
    GPData(GPEGLImage* image) : type_(IMAGE), eglImage(image) {}

    operator GPBuffer*() const
    {
        if (type_ == BUFFER) {
            return gpbuffer;
        }
        return nullptr;
    }

    operator GPEGLImage*() const
    {
        if (type_ == IMAGE) {
            return eglImage;
        }
        return nullptr;
    }

private:
    DataType type_;
    union {
        GPBuffer* gpbuffer;
        GPEGLImage* eglImage;
    };
};

}  // namespace GPlayer

#endif  // __GP_DATA__
