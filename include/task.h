#ifndef __TASK__
#define __TASK__

#include <string>

namespace GPlayer {

class ITask {
public:
    // virtual ~ITask() = 0;
    virtual std::string GetInfo() const = 0;
    // virtual bool Bind() = 0;
};

}  // namespace GPlayer

#endif  // __TASK__