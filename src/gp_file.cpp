#include "gp_file.h"

namespace GPlayer {

GPFile::GPFile(std::string filepath) : filepath_(filepath)
{
    outfile_ = new std::ofstream(filepath);
}

GPFile::~GPFile()
{
    delete outfile_;
}

std::string GPFile::GetInfo() const
{
    return "GPFile: " + filepath_;
}

void GPFile::AddHandler(IModule* module) {}

void GPFile::Process(GPBuffer* buffer)
{
    outfile_->write(static_cast<const char*>(buffer->GetData()),
                    buffer->GetLength());
}

}  // namespace GPlayer
