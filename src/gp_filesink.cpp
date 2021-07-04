#include "gp_filesink.h"

namespace GPlayer {

GPFileSink::GPFileSink(std::string filepath) : filepath_(filepath)
{
    outfile_ = new std::ofstream(filepath);
    SetType(FileSink);
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
    outfile_->write(static_cast<const char*>(buffer->GetData()),
                    buffer->GetLength());
}

}  // namespace GPlayer
