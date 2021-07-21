#ifndef __GPBEADER_H__
#define __GPBEADER_H__

#include <string>
#include <vector>

#include "gp_data.h"
#include "gp_log.h"

namespace GPlayer {

// enum class BeaderDirection { Downstream = 1, Upstream = 2, All = 3 };

enum class BeaderType {
    Unknown = 0,
    SocketServerSrc,
    SocketClientSrc,
    CameraV4l2Src,
    FileSrc,
    FileSink,
    EGLDisplaySink,
    NvVideoEncoder,
    NvVideoDecoder,
    NvJpegDecoder,
};

class GPPipeline;
class IBeader : public std::enable_shared_from_this<IBeader> {
public:
    explicit IBeader();
    virtual std::string GetInfo() const = 0;
    virtual void SetProperties(const std::string& name = "",
                               const std::string& description = "",
                               BeaderType type = BeaderType::Unknown,
                               bool isPassive = true) final;
    virtual std::string GetName() const final;
    virtual std::string GetDescription() const final;
    virtual BeaderType GetType() const final;
    virtual bool IsPassive() const final;
    virtual bool Link(const std::shared_ptr<IBeader>& beader) final;
    virtual void Link(
        const std::vector<std::shared_ptr<IBeader>>& beaders) final;
    virtual void Unlink(const std::shared_ptr<IBeader>& beader) final;
    virtual void Unlink(BeaderType type) final;
    virtual std::shared_ptr<IBeader> GetChild(BeaderType type) final;
    virtual std::vector<std::shared_ptr<IBeader>> GetChildren(BeaderType type);
    virtual bool HasChild(const IBeader& beader) final;
    virtual std::shared_ptr<IBeader> FindParent(BeaderType type) final;
    virtual bool Attach(const std::shared_ptr<GPPipeline>& pipeline) final;
    virtual bool HasProc() = 0;
    virtual int Proc();

private:
    std::string name_;
    std::string description_;
    BeaderType type_;
    bool is_passive_;
    std::vector<std::shared_ptr<IBeader>> child_beaders_;
    std::recursive_mutex mutex_;
    std::weak_ptr<GPPipeline> pipeline_;
};

}  // namespace GPlayer

#endif  // __GPBEADER_H__
