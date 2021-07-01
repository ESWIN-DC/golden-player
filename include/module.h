#ifndef __MODULE__
#define __MODULE__

#include <string>

#include "gp_buffer.h"

namespace GPlayer {

class IModule {
public:
    // virtual ~IModule() = 0;
    virtual std::string GetInfo() const = 0;
    virtual void AddHandler(IModule* module) = 0;
    virtual void Process(GPBuffer* buffer) = 0;
};

}  // namespace GPlayer

#endif  // __MODULE__