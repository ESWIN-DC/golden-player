#ifndef __GP_CIRCULAR_BUFFER__
#define __GP_CIRCULAR_BUFFER__

#include <cstdio>
#include <memory>
#include <mutex>

namespace GPlayer {
template <class T>
class gp_circular_buffer {
public:
    explicit gp_circular_buffer(size_t size)
        : buf_(std::unique_ptr<T[]>(new T[size])), max_size_(size)
    {
    }

    void put(T item)
    {
        buf_[head_] = item;
        if (full_) {
            tail_ = (tail_ + 1) % max_size_;
        }
        head_ = (head_ + 1) % max_size_;
        full_ = head_ == tail_;
    }

    std::size_t put(T* block, size_t length)
    {
        std::size_t to_put = std::min(max_size_ - size(), length);
        std::size_t to_put1 = std::min(max_size_ - head_, to_put);
        std::size_t to_put2 = to_put - to_put1;

        std::memcpy(buf_.get() + head_, block, to_put1 * sizeof(T));
        if (to_put2 > 0) {
            std::memcpy(buf_.get(), block + to_put1, to_put2 * sizeof(T));
        }

        head_ = (head_ + to_put) % max_size_;
        full_ = head_ == tail_;

        return to_put;
    }

    T get()
    {
        if (empty()) {
            return T();
        }

        // Read data and advance the tail (we now have a free space)
        auto val = buf_[tail_];
        full_ = false;
        tail_ = (tail_ + 1) % max_size_;

        return val;
    }

    std::size_t get(T* block, std::size_t length)
    {
        std::size_t to_get = std::min(size(), length);
        std::size_t to_get1 = std::min(max_size_ - tail_, to_get);
        std::size_t to_get2 = to_get - to_get1;

        if (empty()) {
            return 0;
        }

        // Read data and advance the tail
        std::memcpy(block, buf_.get() + tail_, to_get1 * sizeof(T));
        if (to_get2 > 0) {
            std::memcpy(block + to_get1, buf_.get(), to_get2 * sizeof(T));
        }

        full_ = false;
        tail_ = (tail_ + to_get) % max_size_;

        return to_get;
    }

    void drop()
    {
        if (empty()) {
            return;
        }

        full_ = false;
        tail_ = (tail_ + 1) % max_size_;
    }

    std::size_t drop(std::size_t length)
    {
        std::size_t to_drop = std::min(size(), length);

        if (empty()) {
            return 0;
        }

        full_ = false;
        tail_ = (tail_ + to_drop) % max_size_;

        return to_drop;
    }

    std::size_t snap(T* block, std::size_t length)
    {
        size_t to_snap = std::min(size(), length);
        size_t to_snap1 = std::min(max_size_ - tail_, to_snap);
        size_t to_snap2 = to_snap - to_snap1;

        if (empty()) {
            return 0;
        }

        // Read data and advance the tail
        std::memcpy(block, buf_.get() + tail_, to_snap1 * sizeof(T));
        if (to_snap2 > 0) {
            std::memcpy(block + to_snap1, buf_.get(), to_snap2 * sizeof(T));
        }

        return to_snap;
    }

    void reset()
    {
        head_ = tail_;
        full_ = false;
    }

    bool empty() const
    {
        // if head and tail are equal, we are empty
        return (!full_ && (head_ == tail_));
    }

    bool full() const
    {
        // If tail is ahead the head by 1, we are full
        return full_;
    }

    std::size_t capacity() const { return max_size_; }

    std::size_t size() const
    {
        size_t size = max_size_;

        if (!full_) {
            if (head_ >= tail_) {
                size = head_ - tail_;
            }
            else {
                size = max_size_ + head_ - tail_;
            }
        }

        return size;
    }

private:
    std::unique_ptr<T[]> buf_;
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    const std::size_t max_size_;
    bool full_ = 0;
};

}  // namespace GPlayer

#endif  // __GP_CIRCULAR_BUFFER__