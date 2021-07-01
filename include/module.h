#ifndef __MODULE__
#define __MODULE__

#include <string>

namespace GPlayer {

class IModule {
public:
    // virtual ~IModule() = 0;
    virtual std::string GetInfo() const = 0;
    // virtual bool Bind() = 0;
};

}  // namespace GPlayer

#endif  // __MODULE__