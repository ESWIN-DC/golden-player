#ifndef __GP_CONFIGURATION___
#define __GP_CONFIGURATION___

#include <string>

#include "nlohmann/json.hpp"
namespace GPlayer {
class GPConfiguration {
public:
    bool SetConfiguration(const std::string& configuration);
    bool GetConfiguration(std::string& configuration);
    void SetKey(const std::string& key, const std::string& value);
    bool GetKey(const std::string& key, auto& value);

private:
    nlohmann::json configuration_;
};

}  // namespace GPlayer

#endif  // __GP_CONFIGURATION___
