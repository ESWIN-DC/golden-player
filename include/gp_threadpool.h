#ifndef __GP_THREADPOOL_H__
#define __GP_THREADPOOL_H__

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace GPlayer {
class GPThreadPool {
public:
    GPThreadPool();
    ~GPThreadPool();
    void Post(std::function<void()>& func);
    void Done();

private:
    void InfiniteLoopFunc();

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> function_queue_;
    std::atomic<bool> accept_functions_;
    std::mutex lock_;
    std::condition_variable thread_condition_;
};

}  // namespace GPlayer

#endif  // __GP_THREADPOOL_H__
