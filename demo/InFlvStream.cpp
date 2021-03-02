#include "InFlvStream.h"

using namespace hercules;
using std::string;

InFlvStream::InFlvStream(const string &streamname, const string &filename)
    : m_streamname(streamname), m_filename(filename), m_init(false)
{
}

bool InFlvStream::getAVData(AVData &avData)
{
    if (!m_init)
    {
        if (m_file.init(m_filename) != 0)
        {
            m_file.destory();
            logErr(MIXLOG << "open flv file " << m_filename << " failed.");
            return false;
        }
        m_init = true;
        logInfo(MIXLOG << "open flv file " << m_filename << " successed.");
    }

    flv::Tag *tag = m_file.parseFlv();
    if (tag != nullptr && (tag->tagHeader_.tagType_ == flvhelper::TAG_TYPE_VIDEO ||
                           tag->tagHeader_.tagType_ == flvhelper::TAG_TYPE_SCRIPT ||
                           tag->tagHeader_.tagType_ == flvhelper::TAG_TYPE_AUDIO))
    {
        avData.m_dataType = static_cast<DataType>(tag->tagHeader_.tagType_);
        avData.m_data = std::string((const char*)tag->tagData_.data_, tag->tagData_.size_);
        avData.m_streamName = m_streamname;
        return true;
    }

    return false;
}

