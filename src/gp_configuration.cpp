
#include "nlohmann/json.hpp"

#include "gp_configuration.h"
#include "gp_log.h"

namespace GPlayer {

bool GPConfiguration::SetConfiguration(const std::string& configuration)
{
    using json = nlohmann::json;

    json result = json::parse(configuration, nullptr, false);
    if (result.is_discarded()) {
        SPDLOG_ERROR("parsing unsuccessful: '{}'", configuration);
        return false;
    }

    configuration_ = result;
    SPDLOG_TRACE("parsed value: \n'{}'", result.dump(4));

    return true;
}

bool GPConfiguration::GetConfiguration(std::string& configuration)
{
    std::string json_string = configuration_.dump(4);
    configuration = json_string;
    SPDLOG_TRACE("configuration = \n'{}'", json_string);
    return true;
}

void GPConfiguration::SetKey(const std::string& key, const std::string& value)
{
    configuration_[key] = value;
}

bool GPConfiguration::GetKey(const std::string& key, auto& value)
{
    value = configuration_[key];
    return true;
}

}  // namespace GPlayer
