#include "pipeline.h"

namespace GPlayer {

Pipeline::Pipeline() {}

bool Pipeline::Add(const std::shared_ptr<ITask>& task)
{
    tasks_.push_back(task);
    return true;
}

bool Pipeline::Insert(const std::shared_ptr<ITask>& task)
{
    return true;
}

bool Pipeline::Tee(const std::shared_ptr<ITask>& task)
{
    return true;
}

bool Pipeline::Run()
{
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