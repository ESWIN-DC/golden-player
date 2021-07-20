#ifndef __GP_VIDEO_DECODER_GROUP__
#define __GP_VIDEO_DECODER_GROUP__

#include <vector>

#include "gp_nvvideo_decoder.h"
#include "gplayer.h"

namespace GPlayer {

class GPVideoDecoderGroup : public IBeader {
    using VideoDecoderGroup = std::tuple<std::shared_ptr<IBeader>,
                                         std::shared_ptr<IBeader>,
                                         std::vector<std::shared_ptr<IBeader>>>;

public:
    explicit GPVideoDecoderGroup(int numOfDecoders = 1);
    ~GPVideoDecoderGroup();

    std::string GetInfo() const override;
    void Process(GPData* data);
    int Proc() override;
    bool HasProc() override { return true; };

    void AddGroup(std::shared_ptr<GPNvVideoDecoder>&& video_beader,
                  std::shared_ptr<GPFileSrc>&& file_src,
                  std::vector<std::shared_ptr<IBeader>>&& display_sinks);

private:
    std::vector<VideoDecoderGroup> beader_groups_;
};

};  // namespace GPlayer

#endif  // __GP_VIDEO_DECODER_GROUP__
