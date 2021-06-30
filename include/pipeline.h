
#ifndef __PIPELINE__
#define __PIPELINE__

#include <list>
#include <memory>

#include "task.h"

namespace GPlayer {

class Pipeline {
private:
public:
    Pipeline();
    bool Add(const std::shared_ptr<ITask>& task);
    bool Insert(const std::shared_ptr<ITask>& task);
    bool Tee(const std::shared_ptr<ITask>& task);
    bool Execute();
    bool Reload();
    bool Terminate();

private:
    std::list<std::shared_ptr<ITask>> tasks_;
};

}  // namespace GPlayer

#endif  // __PIPELINE__
