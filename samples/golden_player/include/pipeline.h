
#ifndef __PIPELINE__
#define __PIPELINE__

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
    bool Run();
    bool Reload();
    bool Terminate();
};

}  // namespace GPlayer

#endif  // __PIPELINE__
