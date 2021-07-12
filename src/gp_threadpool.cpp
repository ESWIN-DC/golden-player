

#include "gp_threadpool.h"

namespace GPlayer {

GPThreadPool::GPThreadPool()
    : function_queue_(), accept_functions_(true), lock_(), thread_condition_()
{
    int num_threads = std::thread::hardware_concurrency();
    for (int i = 0; i < num_threads; i++) {
        threads_.emplace_back(
            std::thread(&GPThreadPool::InfiniteLoopFunc, this));
    }
}

GPThreadPool::~GPThreadPool() {}

void GPThreadPool::Post(std::function<void()>& func)
{
    {
        std::unique_lock<std::mutex> lock(lock_);
        function_queue_.push(func);
    }

    thread_condition_.notify_one();
}

void GPThreadPool::Done()
{
    {
        std::unique_lock<std::mutex> lock(lock_);
        accept_functions_ = false;
    }

    thread_condition_.notify_all();
}

void GPThreadPool::InfiniteLoopFunc()
{
    std::function<void()> func;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(lock_);
            thread_condition_.wait(lock, [this]() {
                return !function_queue_.empty() || !accept_functions_;
            });
            if (!accept_functions_ && function_queue_.empty()) {
                return;
            }
            func = function_queue_.front();
            function_queue_.pop();
        }
        func();
    }
}

}  // namespace GPlayer