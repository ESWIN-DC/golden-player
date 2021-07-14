
#include <string>
#include <vector>

#include "gp_beader.h"
#include "gp_data.h"
#include "gp_log.h"

namespace GPlayer {

IBeader::IBeader()
    : name_("Unknown"),
      description_(""),
      type_(BeaderType::Unknown),
      is_passive_(false)
{
}

void IBeader::SetProperties(const std::string& name,
                            const std::string& description,
                            BeaderType type,
                            bool is_passive)
{
    name_ = name;
    description_ = description;
    type_ = type;
    is_passive_ = is_passive;
}

BeaderType IBeader::GetType() const
{
    return type_;
}

bool IBeader::IsPassive() const
{
    return is_passive_;
}

std::string IBeader::GetName() const
{
    return name_;
}

std::string IBeader::GetDescription() const
{
    return description_;
}

void IBeader::Link(const std::shared_ptr<IBeader>& beader)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (beader->type_ == BeaderType::Unknown) {
        SPDLOG_ERROR("BUGBUG: unkown handler: {}", beader->GetInfo());
    }

    if (beader->pipeline_ != pipeline_) {
        SPDLOG_WARN(
            "BUGBUG: Cannot link beaders [{} {}] from different pipelines.",
            GetInfo(), beader->GetInfo());
    }

    if (beader->GetBeader(type_).get() == this) {
        SPDLOG_ERROR("BUGBUG: Cannot link beads to each other.");
        return;
    }

    beaders_.emplace_back(beader);
    SPDLOG_TRACE("{} linked the beader type={} info={} ...", GetInfo(),
                 beader->GetType(), beader->GetInfo());
}

void IBeader::Link(const std::vector<std::shared_ptr<IBeader>>& beaders)
{
    std::lock_guard<std::mutex> guard(mutex_);
    std::for_each(
        beaders.begin(), beaders.end(),
        [&](const std::shared_ptr<IBeader>& beader) {
            if (beader->type_ == BeaderType::Unknown) {
                SPDLOG_ERROR("BUGBUG: unkown handler: {}", beader->GetInfo());
            }

            if (beader->pipeline_ != pipeline_) {
                SPDLOG_WARN(
                    "BUGBUG: Cannot link beaders [{} {}] from "
                    "different pipelines.",
                    GetInfo(), beader->GetInfo());
            }
            SPDLOG_TRACE("{} link the beader type={} info={}...", GetInfo(),
                         beader->GetType(), beader->GetInfo());
        });
    beaders_.insert(beaders_.end(), beaders.begin(), beaders.end());
}

void IBeader::Unlink(IBeader* module)
{
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto it = beaders_.begin(); it != beaders_.end(); ++it) {
        if ((*it).get() == module) {
            beaders_.erase(it);
            break;
        }
    }
}

void IBeader::Unlink(BeaderType type)
{
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto it = beaders_.begin(); it != beaders_.end(); ++it) {
        if ((*it)->type_ == type) {
            beaders_.erase(it);
            break;
        }
    }
}

std::shared_ptr<IBeader> IBeader::GetBeader(BeaderType type)
{
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto it = beaders_.begin(); it != beaders_.end(); ++it) {
        if ((*it)->type_ == type) {
            return *it;
        }
    }
    return nullptr;
}

bool IBeader::Attach(GPPipeline* pipeline)
{
    pipeline_ = pipeline;
    return false;
}

int IBeader::Proc()
{
    SPDLOG_CRITICAL("kill the warhog");
    return 0;
}

}  // namespace GPlayer
