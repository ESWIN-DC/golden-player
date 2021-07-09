#ifndef __GPBEADER_H__
#define __GPBEADER_H__

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
    EGLDisplaySink,
    NvVideoEncoder,
    NvVideoDecoder,
    NvJpegDecoder,
};

class GPPipeline;
class IBeader {
public:
    explicit IBeader();
    virtual std::string GetInfo() const = 0;
    virtual void SetType(BeaderType type) final;
    virtual BeaderType GetType() const final;
    virtual std::string GetName() const final;
    virtual void Link(const std::shared_ptr<IBeader>& beader) final;
    virtual void Link(
        const std::vector<std::shared_ptr<IBeader>>& beaders) final;
    virtual void Unlink(IBeader* module) final;
    virtual void Unlink(BeaderType type) final;
    virtual std::shared_ptr<IBeader> GetBeader(BeaderType type) final;
    virtual bool Attach(GPPipeline* pipeline) final;
    virtual bool HasProc() = 0;
    virtual int Proc();

private:
    BeaderType type_;
    char name_[32];
    std::vector<std::shared_ptr<IBeader>> beaders_;
    std::mutex mutex_;
    GPPipeline* pipeline_;
};

}  // namespace GPlayer

#endif  // __GPBEADER_H__
