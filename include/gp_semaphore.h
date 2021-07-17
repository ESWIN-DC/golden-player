#ifndef __GP_SMAPHORE__
#define __GP_SMAPHORE__

#include <condition_variable>
#include <mutex>

namespace GPlayer {

class GPSemaphore {
public:
    GPSemaphore(std::size_t count = 0) : count_(count) {}

    void release()
    {
        std::lock_guard<std::mutex> lock(lock_);
        count_++;
        cv_.notify_one();
    }
    void acquire()
    {
        std::unique_lock<std::mutex> lock(lock_);
        cv_.wait(lock, [&]() { return count_ != 0; });
        count_--;
    }

private:
    std::mutex lock_;
    std::condition_variable cv_;
    std::size_t count_;
};

}  // namespace GPlayer

#endif  // __GP_SMAPHORE__