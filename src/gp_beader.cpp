
#include <string>
#include <utility>
#include <vector>

#include "gp_beader.h"
#include "gp_data.h"
#include "gp_log.h"
#include "gp_pipeline.h"

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
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    if (beader->type_ == BeaderType::Unknown) {
        SPDLOG_ERROR("BUGBUG: unkown handler: {}", beader->GetInfo());
    }

    if (beader->pipeline_ != pipeline_) {
        SPDLOG_WARN(
            "BUGBUG: Cannot link beaders [{} {}] from different pipelines.",
            GetInfo(), beader->GetInfo());
    }

    if (beader->GetChild(type_).get() == this) {
        SPDLOG_ERROR("BUGBUG: Cannot link the beaders to each other.");
        return;
    }

    if (beader->IsPassive()) {
        child_beaders_.emplace_back(beader);
        SPDLOG_TRACE("{} linked the beader type={} info={} ...", GetInfo(),
                     beader->GetType(), beader->GetInfo());
        return;
    }

    std::vector<std::shared_ptr<IBeader>> beaders = pipeline_->GetBeaderList();
    for (auto it = beaders.begin(); it != beaders.end(); ++it) {
        if ((*it).get() == this) {
            std::lock_guard<std::recursive_mutex> guard(beader->mutex_);
            beader->child_beaders_.emplace_back(*it);
            SPDLOG_TRACE("{} reverse linked the beader type={} info={} ...",
                         GetInfo(), beader->GetType(), beader->GetInfo());
            return;
        }
    }

    SPDLOG_TRACE("{} is unable to link the beader type={} info={} ...",
                 GetInfo(), beader->GetType(), beader->GetInfo());
}

void IBeader::Link(const std::vector<std::shared_ptr<IBeader>>& beaders)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    std::for_each(
        beaders.begin(), beaders.end(),
        [&](const std::shared_ptr<IBeader>& beader) { Link(beader); });
}

void IBeader::Unlink(IBeader* beader)
{
    Unlink(beader->GetType());
}

void IBeader::Unlink(BeaderType type)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    for (auto it = child_beaders_.begin(); it != child_beaders_.end(); ++it) {
        if ((*it)->type_ == type && (*it)->IsPassive()) {
            child_beaders_.erase(it);
            return;
        }
    }

    SPDLOG_ERROR("{} is unable to unlink the beader type={} ...", GetInfo(),
                 type);
}

std::shared_ptr<IBeader> IBeader::GetChild(BeaderType type)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    for (auto it = child_beaders_.begin(); it != child_beaders_.end(); ++it) {
        if ((*it)->type_ == type) {
            // SPDLOG_TRACE("Found child beader [type = {}] for beader [{}].",
            //              (*it)->type_, type_);
            return *it;
        }
    }

    SPDLOG_TRACE("No found beader [type = {}] in beader [{}].", type, type_);
    return nullptr;
}

std::vector<std::shared_ptr<IBeader>> IBeader::GetChildren(BeaderType type)
{
    std::vector<std::shared_ptr<IBeader>> beaders;
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    for (auto it = child_beaders_.begin(); it != child_beaders_.end(); ++it) {
        if ((*it)->type_ == type) {
            // SPDLOG_TRACE("Found child beader [type = {}] for beader [{}].",
            //              (*it)->type_, type_);
            beaders.emplace_back(*it);
        }
    }

    SPDLOG_TRACE("found {} beaders [type = {}] in beader [{}].", beaders.size(),
                 type_);
    return beaders;
}

bool IBeader::HasChild(const IBeader& beader)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    for (auto it = child_beaders_.begin(); it != child_beaders_.end(); ++it) {
        if ((*it).get() == &beader) {
            SPDLOG_TRACE("Found child beader [type = {}] for beader [{}].",
                         (*it)->type_, type_);
            return true;
        }
    }

    SPDLOG_TRACE("No found child beader [type = {}] in beader [{}].",
                 beader.type_, type_);
    return false;
}

std::shared_ptr<IBeader> IBeader::FindParent(BeaderType type)
{
    if (pipeline_) {
        return pipeline_->FindBeaderParent(*this, type);
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
