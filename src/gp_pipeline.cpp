#include <thread>

#include "gp_pipeline.h"

namespace GPlayer {

GPPipeline::GPPipeline() {}

bool GPPipeline::Add(const std::shared_ptr<IBeader>& element)
{
    elements_.push_back(element);
    return true;
}

bool GPPipeline::Add(std::vector<std::shared_ptr<IBeader>>& elementList)
{
    elements_.insert(elements_.end(), elementList.begin(), elementList.end());
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
    GPMessage msg;

    while (GetMessage(&msg)) {
        if (msg.type == GPMessageType::ERROR) {
            break;
        }
        else if (msg.type == GPMessageType::STATE_CHANGED) {
        }
        else {
        }
    };

    return true;
}

bool GPPipeline::Reload()
{
    return true;
}

bool GPPipeline::Terminate()
{
    return true;
}

bool GPPipeline::GetMessage(GPMessage* msg)
{
    return true;
}

}  // namespace GPlayer
