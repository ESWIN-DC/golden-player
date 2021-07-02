#ifndef __MODULE__
#define __MODULE__

#include <string>
#include <vector>

#include "gp_data.h"
#include "gp_error.h"

namespace GPlayer {

class IBead {
public:
    typedef enum {
        Unknown = -1,
        RawBuffer,
        IMAGE,
        Display,
    } Type;

public:
    IBead() : type_(Unknown) {}
    virtual std::string GetInfo() const = 0;
    virtual void AddHandler(IBead* module)
    {
        if (module->type_ == Unknown) {
            SPDLOG_ERROR("BUGBUG: unkown handler");
        }

        handlers_.push_back(module);
    }

    IBead* GetHandler(Type type)
    {
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if ((*it)->type_ == type) {
                return *it;
            }
        }
        return nullptr;
    }

    virtual void Process(GPData* data) = 0;

private:
    Type type_;
    std::vector<IBead*> handlers_;
};

}  // namespace GPlayer

#endif  // __MODULE__
