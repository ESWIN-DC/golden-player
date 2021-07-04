#ifndef __BEADER_H__
#define __BEADER_H__

#include <string>
#include <vector>

#include "gp_data.h"
#include "gp_error.h"

namespace GPlayer {

class IBeader {
public:
    typedef enum {
        Unknown = -1,
        RawBuffer,
        IMAGE,
        Display,
        Encoder,
        JpegDecoder,
    } Type;

public:
    IBeader() : type_(Unknown) {}
    virtual std::string GetInfo() const = 0;
    virtual void AddBeader(IBeader* module)
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        if (module->type_ == Unknown) {
            SPDLOG_ERROR("BUGBUG: unkown handler");
        }

        handlers_.push_back(module);
    }

    void RemoveBeader(IBeader* module)
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if ((*it) == module) {
                handlers_.erase(it);
                break;
            }
        }
    }

    void RemoveBeader(Type type)
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if ((*it)->type_ == type) {
                handlers_.erase(it);
                break;
            }
        }
    }

    IBeader* GetBeader(Type type)
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if ((*it)->type_ == type) {
                return *it;
            }
        }
        return nullptr;
    }

private:
    Type type_;
    std::vector<IBeader*> handlers_;
    std::mutex handlers_mutex_;
};

}  // namespace GPlayer

#endif  // __BEADER_H__
