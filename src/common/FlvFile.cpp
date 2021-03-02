// Copyright 2021 HUYA
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "FlvFile.h"
#include "ByteUtil.h"
#include "Amf.h"
#include "Util.h"
#include "Log.h"

#include <iostream>
#include <vector>
#include <string>
namespace hercules
{

    using namespace flv;
    using namespace flvhelper;

    int AudioTag::aacProfile_;
    int AudioTag::sampleRateIndex_;
    int AudioTag::channelConfig_;

    void Tag::init(TagHeader *pHeader, unsigned char *pBuf, uint32_t nLeftLen)
    {
        memcpy(&tagHeader_, pHeader, sizeof(TagHeader));

        tagHeaderData_.data_ = new unsigned char[TAG_HEADER_SIZE];
        memcpy(tagHeaderData_.data_, pBuf, TAG_HEADER_SIZE);

        tagData_.data_ = new unsigned char[tagHeader_.dataSize_ + TAG_HEADER_SIZE];
        memcpy(tagData_.data_, pBuf, tagHeader_.dataSize_ + TAG_HEADER_SIZE);
        tagData_.size_ = tagHeader_.dataSize_ + TAG_HEADER_SIZE;

        if (tagHeader_.tagType_ == TAG_TYPE_VIDEO)
        {
            logDebug(MIXLOG << "tag size" << tagHeader_.dataSize_ + TAG_HEADER_SIZE);
        }
    }

    AudioTag::AudioTag(TagHeader *pHeader, unsigned char *pBuf, uint32_t nLeftLen,
                       FlvFile *pParser)
    {
        init(pHeader, pBuf, nLeftLen);

        unsigned char *pd = tagData_.data_;
        soundFormat_ = (SoundFormat)((pd[0] & 0xf0) >> 4);
        soundRate_ = (SoundRate)((pd[0] & 0x0c) >> 2);
        soundSize_ = (SoundSize)((pd[0] & 0x02) >> 1);
        soundType_ = (SoundType)(pd[0] & 0x01);
        if (soundFormat_ == 10) // AAC
        {
            parseAACTag(pParser);
        }
    }

    int AudioTag::parseAACTag(FlvFile *pParser)
    {
        unsigned char *pd = tagData_.data_;
        int nAACPacketType = pd[1];
        dts_ = tagHeader_.totalTimeStamp_;
        pts_ = dts_;

        if (nAACPacketType == 0)
        {
            parseAudioSpecificConfig(pParser, pd);
        }
        else if (nAACPacketType == 1)
        {
            parseRawAAC(pParser, pd);
        }
        else
        {
        }

        return 1;
    }

    int AudioTag::parseAudioSpecificConfig(FlvFile *pParser,
                                           unsigned char *pTagData)
    {
        unsigned char *pd = tagData_.data_;

        dts_ = tagHeader_.totalTimeStamp_;
        pts_ = dts_;

        aacProfile_ = ((pd[2] & 0xf8) >> 3) - 1;
        sampleRateIndex_ = ((pd[2] & 0x07) << 1) | (pd[3] >> 7);
        channelConfig_ = (pd[3] >> 3) & 0x0f;

        mediaData_.data_ = nullptr;
        mediaData_.size_ = 0;

        return 1;
    }

    int AudioTag::parseRawAAC(FlvFile *pParser, unsigned char *pTagData)
    {
        uint64_t bits = 0;
        int dataSize = tagHeader_.dataSize_ - 2;

        writeU64(&bits, 12, 0xFFF);
        writeU64(&bits, 1, 0);
        writeU64(&bits, 2, 0);
        writeU64(&bits, 1, 1);
        writeU64(&bits, 2, aacProfile_);
        writeU64(&bits, 4, sampleRateIndex_);
        writeU64(&bits, 1, 0);
        writeU64(&bits, 3, channelConfig_);
        writeU64(&bits, 1, 0);
        writeU64(&bits, 1, 0);
        writeU64(&bits, 1, 0);
        writeU64(&bits, 1, 0);
        writeU64(&bits, 13, 7 + dataSize);
        writeU64(&bits, 11, 0x7FF);
        writeU64(&bits, 2, 0);

        mediaData_.size_ = 7 + dataSize;
        mediaData_.data_ = new unsigned char[mediaData_.size_];
        unsigned char p64[8];
        p64[0] = (unsigned char)(bits >> 56);
        p64[1] = (unsigned char)(bits >> 48);
        p64[2] = (unsigned char)(bits >> 40);
        p64[3] = (unsigned char)(bits >> 32);
        p64[4] = (unsigned char)(bits >> 24);
        p64[5] = (unsigned char)(bits >> 16);
        p64[6] = (unsigned char)(bits >> 8);
        p64[7] = (unsigned char)(bits);

        memcpy(mediaData_.data_, p64 + 1, 7);
        memcpy(mediaData_.data_ + 7, pTagData + 2, dataSize);

        return 1;
    }

    int VideoTag::parseH264Tag(FlvFile *file)
    {
        unsigned char *pd = tagData_.data_;
        avcPacketType_ = (AVCPacketType)pd[1];
        compositionTime_ = showU24(pd + 2);

        dts_ = tagHeader_.totalTimeStamp_;
        pts_ = dts_ + compositionTime_;
        return 0;
    }

    int VideoTag::parseH264Configuration(FlvFile *pParser, unsigned char *pTagData)
    {
        unsigned char *pd = pTagData;
        pParser->nalUnitLength_ = (pd[9] & 0x03) + 1;

        int sps_size = 0;
        int pps_size = 0;
        int sps_num = 0;
        int pps_num = 0;
        sps_num = showU8(pd + 10) & 0x1f;
        std::cout << "sps_num:" << sps_num << std::endl;
        sps_size = showU16(pd + TAG_HEADER_SIZE);
        pps_num = showU8(pd + TAG_HEADER_SIZE + (2 + sps_size));
        pps_size = showU16(pd + TAG_HEADER_SIZE + (2 + sps_size) + 1);
        mediaData_.size_ = 4 + sps_size + 4 + pps_size;
        mediaData_.data_ = new unsigned char[mediaData_.size_];
        memcpy(mediaData_.data_, &h264StartCode, 4);
        memcpy(mediaData_.data_ + 4, pd + TAG_HEADER_SIZE + 2, sps_size);
        memcpy(mediaData_.data_ + 4 + sps_size, &h264StartCode, 4);
        memcpy(mediaData_.data_ + 4 + sps_size + 4, pd + TAG_HEADER_SIZE + 2 + sps_size + 2 + 1,
               pps_size);
        return 1;
    }

    // videotag
    VideoTag::VideoTag(TagHeader *pHeader, unsigned char *pBuf, uint32_t nLeftLen,
                       FlvFile *pParser)
    {
        init(pHeader, pBuf, nLeftLen);

        unsigned char *pd = tagData_.data_ + TAG_HEADER_SIZE;
        frameType_ = FrameType((pd[0] & 0xf0) >> 4);
        codecId_ = CodecId(pd[0] & 0x0f);
        if (tagHeader_.tagType_ == TAG_TYPE_VIDEO && codecId_ == CODEC_ID_AVC)
        {
            parseH264Tag(pParser);
        }
    }

    int VideoTag::parseNalu(FlvFile *pParser, unsigned char *pTagData)
    {
        unsigned char *pd = pTagData;
        int nOffset = 0;

        mediaData_.data_ = new unsigned char[tagHeader_.dataSize_ + 10];
        mediaData_.size_ = 0;

        nOffset = 5;
        while (1)
        {
            if (nOffset >= tagHeader_.dataSize_)
                break;

            int nNaluLen;
            switch (pParser->nalUnitLength_)
            {
            case 4:
                nNaluLen = showU32(pd + nOffset);
                break;
            case 3:
                nNaluLen = showU24(pd + nOffset);
                break;
            case 2:
                nNaluLen = showU16(pd + nOffset);
                break;
            default:
                nNaluLen = showU8(pd + nOffset);
            }
            memcpy(mediaData_.data_ + mediaData_.size_, &h264StartCode, 4);
            memcpy(mediaData_.data_ + mediaData_.size_ + 4,
                   pd + nOffset + pParser->nalUnitLength_, nNaluLen);
            mediaData_.size_ += (4 + nNaluLen);
            nOffset += (pParser->nalUnitLength_ + nNaluLen);
        }

        return 1;
    }

    ScriptTag::ScriptTag(TagHeader *pHeader, unsigned char *pBuf,
                         uint32_t nLeftLen, FlvFile *pParser)
    {
        logDebug(MIXLOG << "script tag");
        init(pHeader, pBuf, nLeftLen);

        unsigned char *pd = tagData_.data_ + TAG_HEADER_SIZE;
        int i = 0;
        int type = pd[i++];
        std::string key;
        std::string value;
        if (type == AMF_DATA_TYPE_STRING)
        {
            key = amfGetString(pd + i, nLeftLen - i);
        }
        else
        {
            return;
        }
        i += 2;
        i += key.length();

        if (key == "onMetaData")
        {
            parseMetadata(pd + i, nLeftLen - i, "onMetadata");
        }
        this->show();
    }

    int ScriptTag::parseMetadata(unsigned char *data, uint32_t size,
                                 std::string key)
    {
        std::string valueString;
        double valueNum;
        int i = 0;
        int valueType = data[i++];
        int valueLen = 0;
        switch (valueType)
        {
        case AMF_DATA_TYPE_MIXEDARRAY:
        {
            int arrLen = showU32(data + i);
            i += 4;
            for (int j = 0; j < arrLen; ++j)
            {
                if (j != 0)
                {
                    ++i;
                }
                key = amfGetString(data + i, size - i);
                i += 2;
                i += key.length();
                valueLen = parseMetadata(data + i, size - i, key);
                i += valueLen;
            }
            break;
        }
        case AMF_DATA_TYPE_STRING:
        {
            valueString = amfGetString(data + i, size - i);
            i += 2;
            i += valueString.length();
            valueLen = valueString.length() + 2;
            break;
        }
        case AMF_DATA_TYPE_BOOL:
        {
            valueNum = data[i++];
            valueLen = 1;
            break;
        }
        case AMF_DATA_TYPE_NUMBER:
        {
            valueNum = showDouble(data + i);
            valueLen = 8;
            i += 8;
            break;
        }
        default:
            break;
        }
        if (key == "duration")
        {
            duration_ = valueNum;
        }
        else if (key == "width")
        {
            width_ = valueNum;
        }
        else if (key == "height")
        {
            height_ = valueNum;
        }
        else if (key == "videodatarate")
        {
            videoDataRate_ = valueNum;
        }
        else if (key == "framerate")
        {
            framerate_ = valueNum;
        }
        else if (key == "videocodecid")
        {
            videoCodecId_ = valueNum;
        }
        else if (key == "audiosamplerate")
        {
            audioSampleRate_ = valueNum;
        }
        else if (key == "audiosamplesize")
        {
            audioSampleSize_ = valueNum;
        }
        else if (key == "stereo")
        {
            stereo_ = valueNum;
        }
        else if (key == "audiocodecid")
        {
            audioCodecId_ = valueNum;
        }
        else if (key == "filesize")
        {
            fileSize_ = valueNum;
        }
        return valueLen;
    }

    void ScriptTag::show()
    {
        logInfo(MIXLOG << "metadata, "
            << ", duration: " << duration_ << ", width:" << width_ 
            << ", height: " << height_ << ", videodatarate" << videoDataRate_
            << ", framerate: " << framerate_ << ", videocodecid" << videoCodecId_
            << ", audiosamplerate: " << audioSampleRate_ 
            << ", audiosamplesize: " << audioSampleSize_ << ", stereo: " << stereo_ 
            << ", audiocodecid: " << audioCodecId_ << ", file size: " << fileSize_);
    }

#define CheckBuffer(x)                  \
    {                                   \
        if ((nBufSize - nOffset) < (x)) \
        {                               \
            *usedLen = nOffset;         \
            return nullptr;                \
        }                               \
    }

#define VIDEO_CODEC_ID_JPEG 1
#define VIDEO_CODEC_ID_H263 2
#define VIDEO_CODEC_ID_SCREEN 3
#define VIDEO_CODEC_ID_VP6 4
#define VIDEO_CODEC_ID_VPA 5
#define VIDEO_CODEC_ID_SCREEN2 6
#define VIDEO_CODEC_ID_AVC 7 // H264

#define VIDEO_FRAME_TYPE_KEYFRAME 1
#define VIDEO_FRAME_TYPE_INTERFRAME 2

#define AUDIO_CODEC_ID_ADPCM 1
#define AUDIO_CODEC_ID_MP3 2
#define AUDIO_CODEC_ID_LINEPCM 3
#define AUDIO_CODEC_ID_AAC 10

#define AVC_PACKET_TYPE_SEQUENCE_HEADER 0
#define AVC_PACKET_TYPE_AVC_NALU 1
#define AVC_PACKET_TYPE_SEQUENCE_END 2

    FlvFile::FlvFile()
    {
        flvHeader_ = nullptr;
        unusedLen_ = 0;
        memset(buf_, 0, sizeof(buf_));
    }

    FlvFile::~FlvFile()
    {
        logInfo(MIXLOG << "size:" << vpTag_.size());
    }

    int FlvFile::init(const std::string &fileName)
    {
        if (fileName.empty())
        {
            return -1;
        }
        fileName_ = fileName;
        fs_.open(fileName_, std::ios_base::in | std::ios_base::binary);
        if (!fs_ || !fs_.is_open() || !fs_.good())
        {
            return -1;
        }
        return 0;
    }

    int FlvFile::destory()
    {
        fs_.close();
    }

    Tag *FlvFile::parseFlv()
    {
        int readLen = 0;
        int usedLen = 0;
        Tag *tag = nullptr;
        while (tag == nullptr)
        {
            if (unusedLen_ < 4 * 1024 * 1024)
            {
                fs_.read(reinterpret_cast<char *>(buf_) + unusedLen_, sizeof(buf_) - unusedLen_);
                readLen = fs_.gcount();
                unusedLen_ += readLen;
                if (unusedLen_ <= 4)
                {
                    break;
                }
            }

            tag = parse(buf_, unusedLen_, &usedLen);
            if (unusedLen_ != usedLen)
            {
                memmove(buf_, buf_ + usedLen, unusedLen_ - usedLen);
            }
            unusedLen_ -= usedLen;
        }
        return tag;
    }

    Tag *FlvFile::parse(unsigned char *pBuf, int nBufSize, int *usedLen)
    {
        int nOffset = 0;
        if (flvHeader_ == nullptr)
        {
            CheckBuffer(FLV_HEADER_SIZE);
            flvHeader_ = createFlvHeader(pBuf + nOffset);
            nOffset += flvHeader_->headerSize_;
        }
        Tag *tag = nullptr;
        CheckBuffer(TAG_HEADER_SIZE);
        int nPrevSize = showU32(pBuf + nOffset);
        nOffset += 4;
        tag = createTag(pBuf + nOffset, nBufSize - nOffset);
        if (tag == nullptr)
        {
            nOffset -= 4;
            *usedLen = nOffset;
            return tag;
        }
        nOffset += (TAG_HEADER_SIZE + tag->tagHeader_.dataSize_);
        *usedLen = nOffset;
        return tag;
    }

    int FlvFile::printInfo()
    {
        stat();

        logInfo(MIXLOG << "video num: " << sStat_.videoNum_ 
            << " , audio num: " << sStat_.audioNum_
            << ", meta num: " << sStat_.metaNum_ 
            << ", max timestamp: " << sStat_.maxTimeStamp_
            << ", length size: " << sStat_.lengthSize_ 
            << ", tag size: " << vpTag_.size());
        return 1;
    }

    int FlvFile::dumpH264(const std::string &path)
    {
        std::fstream f;
        f.open(path.c_str(), std::ios_base::out | std::ios_base::binary);

        std::vector<Tag *>::iterator it_tag;
        for (it_tag = vpTag_.begin(); it_tag != vpTag_.end(); it_tag++)
        {
            if ((*it_tag)->tagHeader_.tagType_ != TAG_TYPE_VIDEO)
                continue;

            f.write(reinterpret_cast<char *>((*it_tag)->mediaData_.data_),
                    (*it_tag)->mediaData_.size_);
        }
        f.close();

        return 1;
    }

    int FlvFile::dumpAAC(const std::string &path)
    {
        std::fstream f;
        f.open(path.c_str(), std::ios_base::out | std::ios_base::binary);

        std::vector<Tag *>::iterator it_tag;
        for (it_tag = vpTag_.begin(); it_tag != vpTag_.end(); it_tag++)
        {
            if ((*it_tag)->tagHeader_.tagType_ != TAG_TYPE_AUDIO)
                continue;

            AudioTag *pAudioTag = reinterpret_cast<AudioTag *>(*it_tag);
            if (pAudioTag->soundFormat_ != 10)
                continue;

            if (pAudioTag->mediaData_.size_ != 0)
                f.write(reinterpret_cast<char *>((*it_tag)->mediaData_.data_),
                        (*it_tag)->mediaData_.size_);
        }
        f.close();

        return 1;
    }

    int FlvFile::dumpFlv(const std::string &path)
    {
        std::fstream f;
        f.open(path.c_str(), std::ios_base::out | std::ios_base::binary);

        // write flv-header
        f.write(reinterpret_cast<char *>(flvHeader_->data_),
                flvHeader_->headerSize_);
        unsigned int nLastTagSize = 0;

        // write flv-tag
        std::vector<Tag *>::iterator it_tag;
        for (it_tag = vpTag_.begin(); it_tag < vpTag_.end(); it_tag++)
        {
            unsigned int nn = writeU32(nLastTagSize);
            f.write(reinterpret_cast<char *>(&nn), 4);

            // check duplicate start code
            if ((*it_tag)->tagHeader_.tagType_ == TAG_TYPE_AUDIO &&
                *((*it_tag)->tagData_.data_ + 1) == 0x01)
            {
                bool duplicate = false;
                unsigned char *pStartCode = (*it_tag)->tagData_.data_ + 5 +
                                            nalUnitLength_;
                unsigned nalu_len = 0;
                unsigned char *p_nalu_len = (unsigned char *)&nalu_len;
                switch (nalUnitLength_)
                {
                case 4:
                    nalu_len = showU32((*it_tag)->tagData_.data_ + 5);
                    break;
                case 3:
                    nalu_len = showU24((*it_tag)->tagData_.data_ + 5);
                    break;
                case 2:
                    nalu_len = showU16((*it_tag)->tagData_.data_ + 5);
                    break;
                default:
                    nalu_len = showU8((*it_tag)->tagData_.data_ + 5);
                    break;
                }

                unsigned char *pStartCodeRecord = pStartCode;
                int i;
                for (i = 0; i <
                            (*it_tag)->tagHeader_.dataSize_ - 5 - nalUnitLength_ - 4;
                     ++i)
                {
                    if (pStartCode[i] == 0x00 && pStartCode[i + 1] == 0x00 &&
                        pStartCode[i + 2] == 0x00 && pStartCode[i + 3] == 0x01)
                    {
                        if (pStartCode[i + 4] == 0x67)
                        {
                            i += 4;
                            continue;
                        }
                        else if (pStartCode[i + 4] == 0x68)
                        {
                            i += 4;
                            continue;
                        }
                        else if (pStartCode[i + 4] == 0x06)
                        {
                            i += 4;
                            continue;
                        }
                        else
                        {
                            i += 4;
                            duplicate = true;
                            break;
                        }
                    }
                }

                if (duplicate)
                {
                    nalu_len -= i;
                    (*it_tag)->tagHeader_.dataSize_ -= i;
                    unsigned char *p =
                        (unsigned char *)&((*it_tag)->tagHeader_.dataSize_);
                    (*it_tag)->tagHeaderData_.data_[1] = p[2];
                    (*it_tag)->tagHeaderData_.data_[2] = p[1];
                    (*it_tag)->tagHeaderData_.data_[3] = p[0];

                    f.write(reinterpret_cast<char *>((*it_tag)->tagHeaderData_.data_),
                        TAG_HEADER_SIZE);

                    switch (nalUnitLength_)
                    {
                    case 4:
                        *((*it_tag)->tagData_.data_ + 5) = p_nalu_len[3];
                        *((*it_tag)->tagData_.data_ + 6) = p_nalu_len[2];
                        *((*it_tag)->tagData_.data_ + 7) = p_nalu_len[1];
                        *((*it_tag)->tagData_.data_ + 8) = p_nalu_len[0];
                        break;
                    case 3:
                        *((*it_tag)->tagData_.data_ + 5) = p_nalu_len[2];
                        *((*it_tag)->tagData_.data_ + 6) = p_nalu_len[1];
                        *((*it_tag)->tagData_.data_ + 7) = p_nalu_len[0];
                        break;
                    case 2:
                        *((*it_tag)->tagData_.data_ + 5) = p_nalu_len[1];
                        *((*it_tag)->tagData_.data_ + 6) = p_nalu_len[0];
                        break;
                    default:
                        *((*it_tag)->tagData_.data_ + 5) = p_nalu_len[0];
                        break;
                    }
                    f.write(reinterpret_cast<char *>((*it_tag)->tagData_.data_),
                            pStartCode - (*it_tag)->tagData_.data_);
                    f.write(reinterpret_cast<char *>(pStartCode + i),
                            (*it_tag)->tagHeader_.dataSize_ -
                                (pStartCode - (*it_tag)->tagData_.data_));
                }
                else
                {
                    f.write(reinterpret_cast<char *>((*it_tag)->tagHeaderData_.data_),
                         TAG_HEADER_SIZE);
                    f.write(reinterpret_cast<char *>((*it_tag)->tagData_.data_),
                        (*it_tag)->tagHeader_.dataSize_);
                }
            }
            else
            {
                f.write(reinterpret_cast<char *>(
                    (*it_tag)->tagHeaderData_.data_), TAG_HEADER_SIZE);
                f.write(reinterpret_cast<char *>((*it_tag)->tagData_.data_),
                    (*it_tag)->tagHeader_.dataSize_);
            }

            nLastTagSize = TAG_HEADER_SIZE + (*it_tag)->tagHeader_.dataSize_;
        }
        unsigned int nn = writeU32(nLastTagSize);
        f.write(reinterpret_cast<char *>(&nn), 4);

        f.close();

        return 1;
    }

    int FlvFile::stat()
    {
        for (int i = 0; i < vpTag_.size(); i++)
        {
            switch (vpTag_[i]->tagHeader_.tagType_)
            {
            case TAG_TYPE_AUDIO:
                sStat_.audioNum_++;
                break;
            case TAG_TYPE_VIDEO:
                statVideo(vpTag_[i]);
                break;
            case TAG_TYPE_SCRIPT:
                sStat_.metaNum_++;
                break;
            default:
                break;
            }
        }
        return 1;
    }

    int FlvFile::statVideo(Tag *pTag)
    {
        sStat_.videoNum_++;
        sStat_.maxTimeStamp_ = pTag->tagHeader_.timeStamp_;

        if (pTag->tagData_.data_[0] == 0x17 && pTag->tagData_.data_[1] == 0x00)
        {
            sStat_.lengthSize_ = (pTag->tagData_.data_[9] & 0x03) + 1;
        }

        return 0;
    }

    FlvHeader *FlvFile::createFlvHeader(unsigned char *pBuf)
    {
        FlvHeader *pHeader = new FlvHeader;
        pHeader->version_ = pBuf[3];
        pHeader->haveAudio_ = (pBuf[4] >> 2) & 0x01;
        pHeader->haveVideo_ = (pBuf[4] >> 0) & 0x01;
        pHeader->headerSize_ = showU32(pBuf + 5);

        pHeader->data_ = new unsigned char[pHeader->headerSize_];
        memcpy(pHeader->data_, pBuf, pHeader->headerSize_);
        flvHeaderStr_ = std::string(reinterpret_cast<char *>(pBuf), pHeader->headerSize_);
        return pHeader;
    }

    int FlvFile::destroyFlvHeader(FlvHeader *header)
    {
        if (header == nullptr)
            return 0;

        delete header->data_;
        return 1;
    }

    Tag *FlvFile::createTag(unsigned char *pBuf, int nLeftLen)
    {
        TagHeader header;
        header.tagType_ = (TagType)showU8(pBuf + 0);
        header.dataSize_ = showU24(pBuf + 1);
        header.timeStamp_ = showU24(pBuf + 4);
        header.timeStampEx_ = showU8(pBuf + 7);
        header.streamId_ = showU24(pBuf + 8);
        header.totalTimeStamp_ = (unsigned int)((header.timeStampEx_ << 24)) +
                                 header.timeStamp_;
        if ((header.dataSize_ + TAG_HEADER_SIZE) > nLeftLen)
        {
            logDebug(MIXLOG << "tag size error data size: "
                          << header.dataSize_ + TAG_HEADER_SIZE << " left len: " << nLeftLen);
            return nullptr;
        }

        Tag *pTag;
        switch (header.tagType_)
        {
        case TAG_TYPE_VIDEO:
            pTag = new VideoTag(&header, pBuf, nLeftLen, this);
            break;
        case TAG_TYPE_AUDIO:
            pTag = new AudioTag(&header, pBuf, nLeftLen, this);
            break;
        case TAG_TYPE_SCRIPT:
            pTag = new ScriptTag(&header, pBuf, nLeftLen, this);
            metadata_ = reinterpret_cast<ScriptTag *>(pTag);
            break;
        default:
            pTag = new Tag();
            pTag->init(&header, pBuf, nLeftLen);
        }

        return pTag;
    }

    int FlvFile::destroyTag(Tag *pTag)
    {
        if (pTag->mediaData_.data_ != nullptr)
            delete[] pTag->mediaData_.data_;
        if (pTag->tagData_.data_ != nullptr)
            delete[] pTag->tagData_.data_;
        if (pTag->tagHeaderData_.data_ != nullptr)
            delete[] pTag->tagHeaderData_.data_;

        return 1;
    }

} // namespace hercules
