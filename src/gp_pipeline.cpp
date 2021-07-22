#include <thread>

#include "gp_log.h"
#include "gp_pipeline.h"

namespace GPlayer {

GPPipeline::GPPipeline()
{
    spdlog::set_level(spdlog::level::trace);
}

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
    elements_.emplace_back(element);
    element->Attach(shared_from_this());
    return true;
}

bool GPPipeline::Add(const std::vector<std::shared_ptr<IBeader>>& elementList)
{
    elements_.insert(elements_.end(), elementList.begin(), elementList.end());
    std::for_each(
        elements_.begin(), elements_.end(),
        [&](std::shared_ptr<IBeader>& element) {
            element->Attach(shared_from_this());
            SPDLOG_TRACE(
                "The beader [type={} info={}] attached to pipeline ...",
                element->GetType(), element->GetInfo());
        });
    return true;
}

std::vector<std::shared_ptr<IBeader>>& GPPipeline::GetBeaderList()
{
    return elements_;
}

std::shared_ptr<IBeader> GPPipeline::FindBeaderParent(const IBeader& beader,
                                                      BeaderType type)
{
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto it = elements_.begin(); it != elements_.end(); ++it) {
        if ((*it)->HasChild(beader) && (*it)->GetType() == type) {
            return (*it);
        }
    }

    return nullptr;
}

bool GPPipeline::Run()
{
    std::for_each(elements_.begin(), elements_.end(),
                  [&](std::shared_ptr<IBeader>& beader) {
                      if (beader->HasProc()) {
                          threads_.emplace_back(
                              std::thread(&IBeader::Proc, beader.get()));
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
    messages_.emplace_back(msg);
    cv_.notify_all();
}

bool GPPipeline::AddMessage(const GPMessage& msg)
{
    std::lock_guard<std::mutex> guard(mutex_);
    messages_.emplace_back(msg);
    cv_.notify_all();
    return true;
}

bool GPPipeline::GetMessage(GPMessage* msg)
{
    using namespace std::chrono_literals;

    std::unique_lock<std::mutex> guard(mutex_);
    cv_.wait_for(guard, 100ms, [&] { return !messages_.empty(); });
    if (!messages_.empty()) {
        *msg = messages_.front();
        messages_.pop_back();
        return true;
    }
    return false;
}

}  // namespace GPlayer
