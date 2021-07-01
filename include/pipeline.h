
#ifndef __PIPELINE__
#define __PIPELINE__

#include <list>
#include <memory>

#include "module.h"

namespace GPlayer {

class Pipeline {
private:
public:
    Pipeline();
    bool Add(const std::shared_ptr<IModule>& task);
    bool Insert(const std::shared_ptr<IModule>& task);
    bool Tee(const std::shared_ptr<IModule>& task);
    bool Execute();
    bool Reload();
    bool Terminate();

private:
    std::list<std::shared_ptr<IModule>> tasks_;
};

}  // namespace GPlayer

#endif  // __PIPELINE__
