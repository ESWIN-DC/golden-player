#ifndef __BEADER_H__
#define __BEADER_H__

#include <string>
#include <vector>

#include "gp_data.h"
#include "gp_log.h"

namespace GPlayer {

enum class BeaderDirection { Unknown = 0, Src, Sink };
enum class BeaderType {
    Unknown = 0,
    CameraV4l2Src,
    FileSink,
    IMAGE,
    EGLDisplaySink,
    NvVideoEncoder,
    NvVideoDecoder,
    NvJpegDecoder,
};

class GPPipeline;
class IBeader {
public:
public:
    explicit IBeader() : type_(BeaderType::Unknown), name_("Unknown") {}
    virtual std::string GetInfo() const = 0;
    virtual void SetType(BeaderType type) final { type_ = type; }
    virtual BeaderType GetType() const final { return type_; }
    virtual std::string GetName() const final { return name_; }
    virtual void Link(std::shared_ptr<IBeader> beader) final
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        if (beader->type_ == BeaderType::Unknown) {
            SPDLOG_ERROR("BUGBUG: unkown handler: {}", beader->GetInfo());
        }

        if (beader->pipeline_ != pipeline_) {
            SPDLOG_WARN(
                "BUGBUG: Cannot link beaders [{} {}] from different pipelines.",
                GetInfo(), beader->GetInfo());
        }

        handlers_.push_back(beader);
        SPDLOG_TRACE("{} linked the beader type={} info={} ...", GetInfo(),
                     beader->GetType(), beader->GetInfo());
    }

    virtual void Link(std::vector<std::shared_ptr<IBeader>>& beaders) final
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        std::for_each(beaders.begin(), beaders.end(),
                      [&](std::shared_ptr<IBeader>& beader) {
                          if (beader->type_ == BeaderType::Unknown) {
                              SPDLOG_ERROR("BUGBUG: unkown handler: {}",
                                           beader->GetInfo());
                          }

                          if (beader->pipeline_ != pipeline_) {
                              SPDLOG_WARN(
                                  "BUGBUG: Cannot link beaders [{} {}] from "
                                  "different pipelines.",
                                  GetInfo(), beader->GetInfo());
                          }
                          SPDLOG_TRACE("{} link the beader type={} info={}...",
                                       GetInfo(), beader->GetType(),
                                       beader->GetInfo());
                      });
        handlers_.insert(handlers_.end(), beaders.begin(), beaders.end());
    }

    virtual void Unlink(IBeader* module) final
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if ((*it).get() == module) {
                handlers_.erase(it);
                break;
            }
        }
    }

    virtual void Unlink(BeaderType type) final
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if ((*it)->type_ == type) {
                handlers_.erase(it);
                break;
            }
        }
    }

    virtual std::shared_ptr<IBeader> GetBeader(BeaderType type) final
    {
        std::lock_guard<std::mutex> guard(handlers_mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if ((*it)->type_ == type) {
                return *it;
            }
        }
        return nullptr;
    }

    virtual bool Attach(GPPipeline* pipeline) final
    {
        pipeline_ = pipeline;
        return false;
    }

    virtual bool HasProc() = 0;
    virtual int Proc()
    {
        SPDLOG_CRITICAL("kill the warhog");
        return 0;
    }

private:
    BeaderType type_;
    char name_[32];
    std::vector<std::shared_ptr<IBeader>> handlers_;
    std::mutex handlers_mutex_;
    GPPipeline* pipeline_;
};

}  // namespace GPlayer

#endif  // __BEADER_H__
