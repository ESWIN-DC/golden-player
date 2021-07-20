
#include "gp_video_decoder_group.h"
#include "gp_log.h"
#include "gplayer.h"

namespace GPlayer {

GPVideoDecoderGroup::GPVideoDecoderGroup(int numOfDecoders)
{
    SetProperties("GPVideoDecoderGroup", "GPVideoDecoderGroup",
                  BeaderType::NvVideoDecoder);

    for (int i = 0; i < numOfDecoders; i++) {
        // auto decoder = std::make_shared<GPNvVideoDecoder>();
        // decoders_.emplace_back(decoder);
    }
}

GPVideoDecoderGroup::~GPVideoDecoderGroup() {}

std::string GPVideoDecoderGroup::GetInfo() const
{
    std::string info = "VideoDecoderGroup: ";
    for (auto&& d : beader_groups_) {
        auto& [video_beader, file_src, display_sinks] = d;

        info += "[";
        info += video_beader->GetInfo();
        info += "] ";
    }

    return info;
}

void GPVideoDecoderGroup::Process(GPData* data)
{
    // std::lock_guard<std::mutex> guard(buffer_lock_);
    // GPBuffer* buffer = *data;

    // size_t put_size = buffer_.put(buffer->GetData(), buffer->GetLength());
    // if (put_size < buffer->GetLength()) {
    //     SPDLOG_WARN("Buffer full!");
    // }
    // // SPDLOG_CRITICAL("Buffer received!");
    // buffer_condition_.notify_one();
}

int GPVideoDecoderGroup::Proc()
{
    std::for_each(beader_groups_.begin(), beader_groups_.end(),
                  [&](VideoDecoderGroup& d) {
                      auto& [video_beader, file_src, display_sinks] = d;
                      video_beader->Proc();
                  });
    return 0;
}

void GPVideoDecoderGroup::AddGroup(
    std::shared_ptr<GPNvVideoDecoder>&& video_beader,
    std::shared_ptr<GPFileSrc>&& file_src,
    std::vector<std::shared_ptr<IBeader>>&& display_sinks)
{
    beader_groups_.push_back(
        std::make_tuple(video_beader, file_src, display_sinks));
}

}  // namespace GPlayer