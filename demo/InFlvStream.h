#pragma once
#include "FlvFile.h"
#include "MixSdk.h"
#include <string>

class InFlvStream
{
public:
    InFlvStream(const std::string &streamname, const std::string& filename);
    bool getAVData(hercules::AVData &data);

private:
    std::string m_streamname;
    std::string m_filename;
    bool m_init;
    hercules::flv::FlvFile m_file; //not copyable
};
