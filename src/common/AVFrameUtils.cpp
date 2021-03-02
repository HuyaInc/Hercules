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

#include "AVFrameUtils.h"
#include "FlvHelper.h"
#include "Util.h"
#include "Log.h"

#include <stdint.h>
#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <sstream>

namespace hercules
{
    constexpr uint8_t H264_SPS = 0x67;
    constexpr uint8_t H264_PPS = 0x68;
    constexpr uint8_t H264_SPS_SIZE_1 = 0xE1;

    static unsigned int showU32(unsigned char *pBuf)
    {
        return (pBuf[0] << 24) | (pBuf[1] << 16) | (pBuf[2] << 8) | pBuf[3];
    }

    bool parseAvcConfig(std::string &framePayload, std::string &avcConfig)
    {
        std::stringstream ss;
        int headerSize = 0;
        int parseLen = flvhelper::TAG_HEADER_SIZE + flvhelper::FLV_AVC_HEADER_LEN;
        std::string sps;
        std::string pps;
        while (parseLen < framePayload.size() - 4)
        {
            headerSize = showU32((unsigned char *)framePayload.c_str() + parseLen);

            if (parseLen + 4 + headerSize < framePayload.size() - 4)
            {
                if (sps.empty() && framePayload[parseLen + 4] == H264_SPS)
                {
                    sps.append(framePayload.c_str() + parseLen + 4, headerSize);
                }
                else if (pps.empty() && framePayload[parseLen + 4] == H264_PPS)
                {
                    pps.append(framePayload.c_str() + parseLen + 4, headerSize);
                }
            }
            parseLen += 4;
            parseLen += headerSize;
        }
        logInfo(MIXLOG << "sps size: " << sps.size() << " pps size: " << pps.size());

        if (pps.empty() || sps.empty())
        {
            return false;
        }

        // update avc config 6 byte
        char version = 0x01;
        avcConfig.append(1, version);
        char avcProfile = sps[1];
        avcConfig.append(1, avcProfile);
        char avcCompatibility = sps[2];
        avcConfig.append(1, avcCompatibility);
        char avcLevel = sps[3];
        avcConfig.append(1, avcLevel);
        char reserved = 0xff;
        avcConfig.append(1, reserved);
        char numberSps = H264_SPS_SIZE_1;
        avcConfig.append(1, numberSps);

        // pps
        uint16_t spsSize = htons((uint16_t)sps.size());
        avcConfig.append((const char *)&spsSize, 2);
        avcConfig.append(sps);

        // sps
        char ppsNum = 1;
        avcConfig.append(1, ppsNum);
        uint16_t ppsSize = htons((uint16_t)pps.size());
        avcConfig.append((const char *)&ppsSize, 2);
        avcConfig.append(pps);

        return true;
    };

    bool parseVideoToFlv(std::string &framePayload, std::string &avcConfig)
    {
        if (framePayload.size() < 4)
        {
            return false;
        }

        uint32_t headSize;
        memcpy(&headSize, framePayload.c_str(), 4);
        if (framePayload.size() < 4 + headSize)
        {
            logErr(MIXLOG << "error parse video flv get header fail"
                << ", header offset: " << headSize + 4 
                << ", payload size: " << framePayload.size());
            return false;
        }

        uint32_t offset = 4 + headSize + flvhelper::TAG_HEADER_SIZE;
        if (framePayload.size() < offset)
        {
            logErr(MIXLOG << "offset: " << offset 
                << ", payload size: " << framePayload.size());
            return false;
        }

        std::string avcDecodeConfig(framePayload.c_str() + 4, headSize);
        flvhelper::genFLVVideoHeader(avcDecodeConfig, avcConfig);
        framePayload.erase(0, 4 + headSize);

        uint32_t frameSize = framePayload.size() - 4;

        const unsigned char *prevBuf = (const unsigned char *)framePayload.c_str() + frameSize;
        uint32_t prevTagSize = ((uint32_t)prevBuf[0] << 24) |
                               ((uint32_t)prevBuf[1] << 16) |
                               ((uint32_t)prevBuf[2] << 8) |
                               (prevBuf[3]);
        if (prevTagSize != frameSize)
        {
            logErr(MIXLOG << "flv pretagsize mismatch"
                << ", tagsize: " << frameSize << ", pretagsize: " << prevTagSize);
            framePayload[frameSize] = (frameSize >> 24) & 0xff;
            framePayload[frameSize + 1] = (frameSize >> 16) & 0xff;
            framePayload[frameSize + 2] = (frameSize >> 8) & 0xff;
            framePayload[frameSize + 3] = frameSize & 0xff;
        }

        return true;
    }

    bool parseYUV(std::string &framePayload, std::string &yuvHeader)
    {
        if (framePayload.size() < 4)
        {
            return false;
        }

        uint32_t headSize;
        memcpy(&headSize, framePayload.c_str(), 4);
        if (framePayload.size() < 4 + headSize)
        {
            logErr(MIXLOG << "parse yuv get header fail"
                << ", header offset: " << headSize + 4
                << ", payload size: " << framePayload.size());
            return false;
        }

        yuvHeader.assign(framePayload.c_str() + 4, headSize);
        framePayload.erase(0, 4 + headSize);

        return true;
    }

    uint32_t getYUVTimestamp(const uint8_t *data, int len)
    {
        if (len < 4)
        {
            return (uint32_t)-1;
        }

        uint32_t headSize;
        memcpy(&headSize, data, 4);
        if (len < 4 + headSize || headSize < 4)
        {
            logErr(MIXLOG << "get yuv timestamp fail"
                << ", header offset: " << headSize + 4 
                << ", firstPacket size: " << len);
            return (uint32_t)-1;
        }

        return (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    }

    uint32_t getVideoFrameType(const std::string &framePayload)
    {
        return framePayload.empty() ?
            0 : (framePayload[flvhelper::TAG_HEADER_SIZE] & 0xF0) >> 4;
    }

    bool isVideoIFrame(const std::string &framePayload)
    {
        return (getVideoFrameType(framePayload) == 0x1);
    }

    void extractFlvVideoHeader(const std::string &flvTag,
                               std::string &rawContent, bool addHeaderSize)
    {
        rawContent.clear();
        if (flvTag.size() < flvhelper::TAG_HEADER_SIZE + flvhelper::TAG_HEADER_SIZE + 4)
        {
            return;
        }

        const char *tagBuf = flvTag.data();
        uint32_t headerSize =
            flvTag.size() - flvhelper::TAG_HEADER_SIZE - flvhelper::FLV_AVC_HEADER_LEN - 4;

        if (addHeaderSize)
        {
            rawContent.reserve(4 + headerSize);
            rawContent.append(reinterpret_cast<char *>(&headerSize), sizeof(headerSize));
        }
        else
        {
            rawContent.reserve(headerSize);
        }
        rawContent.append(
            tagBuf + flvhelper::TAG_HEADER_SIZE + flvhelper::FLV_AVC_HEADER_LEN, headerSize);

        std::ostringstream os;
        os << std::hex;
        for (std::string::const_iterator it = flvTag.begin(); it != flvTag.end(); ++it)
        {
            os << ((unsigned char)*it) << " ";
        }
    }

    void extractFlvAudioHeader(const std::string &flvTag, std::string &rawContent)
    {
        rawContent.clear();
        if (flvTag.size() < flvhelper::TAG_HEADER_SIZE + 4)
        {
            return;
        }

        const char *tagBuf = flvTag.data();
        uint32_t headerSize = flvTag.size() - flvhelper::TAG_HEADER_SIZE - 4;

        rawContent.reserve(4 + headerSize);
        rawContent.append(reinterpret_cast<char *>(&headerSize), sizeof(headerSize));
        rawContent.append(tagBuf + flvhelper::TAG_HEADER_SIZE, headerSize);

        std::ostringstream os;
        os << std::hex;
        for (std::string::const_iterator it = flvTag.begin(); it != flvTag.end(); ++it)
        {
            os << ((unsigned char)*it) << " ";
        }
    }

} // namespace hercules
