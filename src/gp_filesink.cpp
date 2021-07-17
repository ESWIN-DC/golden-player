#include "gp_filesink.h"

namespace GPlayer {

GPFileSink::GPFileSink(std::string filepath) : filepath_(filepath)
{
    SetProperties("GPFileSink", "GPFileSink", BeaderType::FileSink, true);

    outfile_ = new std::ofstream(filepath);
    if (outfile_->is_open()) {
        SPDLOG_TRACE("Open output file {}", filepath);
    }
    else {
        SPDLOG_CRITICAL("Failed to open output file {}", filepath);
    }
}

GPFileSink::~GPFileSink()
{
    outfile_->close();
    delete outfile_;
}

std::string GPFileSink::GetInfo() const
{
    return "GPFileSink: " + filepath_;
}

void GPFileSink::Process(GPData* data)
{
    GPBuffer* buffer = *data;
    outfile_->write(reinterpret_cast<const char*>(buffer->GetData()),
                    buffer->GetLength());
}

}  // namespace GPlayer
