
#ifndef __PIPELINE__
#define __PIPELINE__

#include <list>
#include <memory>

#include "beader.h"

namespace GPlayer {

class Pipeline {
private:
public:
    Pipeline();
    bool Add(const std::shared_ptr<IBeader>& task);
    bool Insert(const std::shared_ptr<IBeader>& task);
    bool Tee(const std::shared_ptr<IBeader>& task);
    bool Execute();
    bool Reload();
    bool Terminate();

private:
    std::list<std::shared_ptr<IBeader>> tasks_;
};

}  // namespace GPlayer

#endif  // __PIPELINE__
