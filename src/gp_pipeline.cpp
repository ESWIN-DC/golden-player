#include <thread>

#include "gp_pipeline.h"

namespace GPlayer {

GPPipeline::GPPipeline() {}

bool GPPipeline::Add(const std::shared_ptr<IBeader>& task)
{
    tasks_.push_back(task);
    return true;
}

bool GPPipeline::Insert(const std::shared_ptr<IBeader>& task)
{
    return true;
}

bool GPPipeline::Tee(const std::shared_ptr<IBeader>& task)
{
    return true;
}

bool GPPipeline::Execute()
{
    std::thread thread;
    thread.join();

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

}  // namespace GPlayer