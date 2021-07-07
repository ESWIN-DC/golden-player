
#ifndef __GPPIPELINE__
#define __GPPIPELINE__

#include <list>
#include <memory>

#include "beader.h"

namespace GPlayer {

enum class GPMessageType {
    ERROR = -1,
    STATE_CHANGED,
    STATE_READY,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_MAX,
};

struct GPMessage {
    GPMessageType type;
};

class GPPipeline {
private:
public:
    GPPipeline();
    bool Add(const std::shared_ptr<IBeader>& element);
    bool Add(std::vector<std::shared_ptr<IBeader>>& elementList);
    bool Insert(const std::shared_ptr<IBeader>& element);
    bool Tee(const std::shared_ptr<IBeader>& element);
    bool Run();
    bool Reload();
    bool Terminate();

private:
    bool GetMessage(GPMessage* msg);

private:
    std::vector<std::shared_ptr<IBeader>> elements_;
};

}  // namespace GPlayer

#endif  // __GPPIPELINE__
