#include <thread>

#include "gp_pipeline.h"

namespace GPlayer {

GPPipeline::GPPipeline() {}
GPPipeline::~GPPipeline()
{
    std::for_each(threads_.begin(), threads_.end(), [&](std::thread& thread) {
        if (thread.joinable()) {
            thread.join();
        }
    });
}

bool GPPipeline::Add(const std::shared_ptr<IBeader>& element)
{
    elements_.push_back(element);
    return true;
}

bool GPPipeline::Add(std::vector<std::shared_ptr<IBeader>>& elementList)
{
    elements_.insert(elements_.end(), elementList.begin(), elementList.end());
    std::for_each(elements_.begin(), elements_.end(),
                  [&](std::shared_ptr<IBeader>& element) {
                      element->Attach(this);
                      SPDLOG_INFO(
                          "Pipeline beaders changed: type={} info={}...",
                          element->GetType(), element->GetInfo());
                  });
    return true;
}

bool GPPipeline::Insert(const std::shared_ptr<IBeader>& element)
{
    return true;
}

bool GPPipeline::Tee(const std::shared_ptr<IBeader>& element)
{
    return true;
}

bool GPPipeline::Run()
{
    std::for_each(
        elements_.begin(), elements_.end(),
        [&](std::shared_ptr<IBeader>& beader) {
            if (beader->HasProc()) {
                threads_.push_back(std::thread(&IBeader::Proc, beader.get()));
            }
        });

    return true;
}

bool GPPipeline::Reload()
{
    return true;
}

void GPPipeline::Terminate()
{
    std::lock_guard<std::mutex> guard(mutex_);
    GPMessage msg = {GPMessageType::ERROR};
    messages_.push_back(msg);
    cv_.notify_all();
}

bool GPPipeline::AddMessage(const GPMessage& msg)
{
    std::lock_guard<std::mutex> guard(mutex_);
    messages_.push_back(msg);
    cv_.notify_all();
    return true;
}

bool GPPipeline::GetMessage(GPMessage* msg)
{
    using namespace std::chrono_literals;

    std::unique_lock<std::mutex> guard(mutex_);
    cv_.wait_for(guard, 100ms, [&] { return !messages_.empty(); });
    *msg = messages_.front();
    messages_.pop_back();
    return true;
}

}  // namespace GPlayer
