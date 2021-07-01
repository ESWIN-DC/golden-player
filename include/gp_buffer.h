#ifndef __GP_BUFFER__
#define __GP_BUFFER__

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

}  // namespace GPlayer

#endif  // __GP_BUFFER__
