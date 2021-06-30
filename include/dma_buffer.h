#ifndef __DMABUFFER__
#define __DMABUFFER__

#include "nvmmapi/NvNativeBuffer.h"

using namespace Argus;
using namespace ArgusSamples;
namespace GPlayer {

static const int NUM_BUFFERS = 10;  // This value is tricky.
// Too small value will impact the FPS.
static Size2D<uint32_t> STREAM_SIZE(640, 480);

// Helper class to map NvNativeBuffer to Argus::Buffer and vice versa.
// A reference to DmaBuffer will be saved as client data in each Argus::Buffer.
// Also DmaBuffer will keep a reference to corresponding Argus::Buffer.
// This class also extends NvBuffer to act as a share buffer between Argus and
// V4L2 encoder.
class DmaBuffer : public NvNativeBuffer, public NvBuffer {
public:
    // Always use this static method to create DmaBuffer
    static DmaBuffer* create(const Argus::Size2D<uint32_t>& size,
                             NvBufferColorFormat colorFormat,
                             NvBufferLayout layout = NvBufferLayout_Pitch)
    {
        DmaBuffer* buffer = new DmaBuffer(size);
        if (!buffer)
            return NULL;

        if (NvBufferCreate(&buffer->m_fd, size.width(), size.height(), layout,
                           colorFormat)) {
            delete buffer;
            return NULL;
        }

        buffer->planes[0].fd =
            buffer->m_fd;  // save the DMABUF fd in NvBuffer structure
        buffer->planes[0].bytesused =
            1;  // byteused must be non-zero for a valid buffer.

        return buffer;
    }

    // Help function to convert Argus Buffer to DmaBuffer
    static DmaBuffer* fromArgusBuffer(Buffer* buffer)
    {
        IBuffer* iBuffer = interface_cast<IBuffer>(buffer);
        const DmaBuffer* dmabuf =
            static_cast<const DmaBuffer*>(iBuffer->getClientData());

        return const_cast<DmaBuffer*>(dmabuf);
    }

    // Return DMA buffer handle
    int getFd() const { return m_fd; }

    // Get and set reference to Argus buffer
    void setArgusBuffer(Buffer* buffer) { m_buffer = buffer; }
    Buffer* getArgusBuffer() const { return m_buffer; }

private:
    DmaBuffer(const Argus::Size2D<uint32_t>& size)
        : NvNativeBuffer(size), NvBuffer(0, 0), m_buffer(NULL)
    {
    }

    Buffer* m_buffer;  // Reference to Argus::Buffer
};

}  // namespace GPlayer

#endif  // __DMABUFFER__