
#ifndef __GPPIPELINE__
#define __GPPIPELINE__

#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <queue>
#include <thread>

#include "gp_beader.h"

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
    ~GPPipeline();
    bool Add(const std::shared_ptr<IBeader>& element);
    bool Add(const std::vector<std::shared_ptr<IBeader>>& elementList);
    std::vector<std::shared_ptr<IBeader>>& GetBeaderList();
    std::shared_ptr<IBeader> FindBeaderParent(const IBeader& beader,
                                              BeaderType type);
    bool Run();
    bool Reload();
    void Terminate();
    bool AddMessage(const GPMessage& msg);
    bool GetMessage(GPMessage* msg);

private:
    std::vector<std::shared_ptr<IBeader>> elements_;
    std::vector<std::thread> threads_;
    std::deque<GPMessage> messages_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace GPlayer

#endif  // __GPPIPELINE__
