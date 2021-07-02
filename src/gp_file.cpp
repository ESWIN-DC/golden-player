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

void GPFile::AddBeader(IBeader* module) {}

void GPFile::Process(GPData* data)
{
    GPBuffer* buffer = *data;
    outfile_->write(static_cast<const char*>(buffer->GetData()),
                    buffer->GetLength());
}

}  // namespace GPlayer
