#include "gp_filesrc.h"
#include "gp_log.h"

namespace GPlayer {

GPFileSrc::GPFileSrc(std::string filepath) : filepath_(filepath)
{
    inputfile_ =
        new std::ifstream(filepath, std::ifstream::in | std::ifstream::binary);

    SetProperties("GPFileSrc", "GPFileSrc", BeaderType::FileSrc, true);

    if (inputfile_->is_open()) {
        SPDLOG_TRACE("Open input file {}", filepath);
    }
    else {
        SPDLOG_CRITICAL("Failed to open input file {}", filepath);
    }
}

GPFileSrc::~GPFileSrc()
{
    inputfile_->close();
    delete inputfile_;
}

std::string GPFileSrc::GetInfo() const
{
    return "GPFileSrc: " + filepath_;
}

void GPFileSrc::Process(GPData* data)
{
    GPBuffer* buffer = *data;
    inputfile_->read(reinterpret_cast<char*>(buffer->GetData()),
                     buffer->GetLength());
}

std::basic_istream<char>& GPFileSrc::Read(char* buffer, std::streamsize count)
{
    return inputfile_->read(buffer, count);
}

}  // namespace GPlayer
