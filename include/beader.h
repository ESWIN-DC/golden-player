#ifndef __BEADER_H__
#define __BEADER_H__

#include <string>
#include <vector>

#include "gp_data.h"
#include "gp_log.h"

namespace GPlayer {

enum class BeaderType {
    Unknown = -1,
    CameraV4l2Src,
    FileSink,
    IMAGE,
    EGLDisplaySink,
    NvVideoEncoder,
    NvVideoDecoder,
    NvJpegDecoder,
};

class IBeader {
public:
public:
    IBeader() : type_(BeaderType::Unknown), name_("Unknown") {}
    virtual std::string GetInfo() const = 0;

    void SetType(BeaderType type) { type_ = type; }
    BeaderType GetType() const { return type_; }
    std::string GetName() const { return name_; }
    virtual void AddBeader(IBeader* module)
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        if (module->type_ == BeaderType::Unknown) {
            SPDLOG_ERROR("BUGBUG: unkown handler: {}", module->GetInfo());
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

    void RemoveBeader(BeaderType type)
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if ((*it)->type_ == type) {
                handlers_.erase(it);
                break;
            }
        }
    }

    IBeader* GetBeader(BeaderType type)
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
    BeaderType type_;
    char name_[32];
    std::vector<IBeader*> handlers_;
    std::mutex handlers_mutex_;
};

}  // namespace GPlayer

#endif  // __BEADER_H__
