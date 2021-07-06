#ifndef __GP_DATA__
#define __GP_DATA__

#include <cstdint>
#include <cstring>
#include <memory>

namespace GPlayer {

class GPBuffer {
public:
    GPBuffer(uint8_t* data, uint32_t length, bool clone = false)
        : data_(data), length_(length), cloned_(clone)
    {
        if (cloned_) {
            data_ = new uint8_t[length];
            std::memcpy(data_, data, length);
        }
    }

    ~GPBuffer()
    {
        if (cloned_) {
            delete[] data_;
        }
    }

    std::shared_ptr<GPBuffer> clone()
    {
        std::shared_ptr<GPBuffer> newClone =
            std::make_shared<GPBuffer>(data_, length_, true);
        return newClone;
    }

    uint8_t* GetData() const { return data_; }
    uint32_t GetLength() const { return length_; }

private:
    GPBuffer();

private:
    uint8_t* data_;
    uint32_t length_;
    bool cloned_;
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
