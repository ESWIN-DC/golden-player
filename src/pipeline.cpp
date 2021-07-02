#include <thread>

#include "pipeline.h"

namespace GPlayer {

Pipeline::Pipeline() {}

bool Pipeline::Add(const std::shared_ptr<IBeader>& task)
{
    tasks_.push_back(task);
    return true;
}

bool Pipeline::Insert(const std::shared_ptr<IBeader>& task)
{
    return true;
}

bool Pipeline::Tee(const std::shared_ptr<IBeader>& task)
{
    return true;
}

bool Pipeline::Execute()
{
    std::thread thread;
    thread.join();

    return true;
}

bool Pipeline::Reload()
{
    return true;
}

bool Pipeline::Terminate()
{
    return true;
}

}  // namespace GPlayer