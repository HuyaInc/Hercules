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

#pragma once

#include <inttypes.h>
#include <string.h>
#include <string>

namespace hercules
{

#define HLS_PREV_TAGSIZE_LEN 4

    namespace flvhelper
    {

        constexpr uint32_t h264StartCode = 0x01000000;
        constexpr uint32_t FLV_HEADER_SIZE = 9;
        constexpr uint32_t TAG_HEADER_SIZE = 11;
        constexpr uint32_t FLV_AVC_HEADER_LEN = 5;

        enum AVCPacketType
        {
            AVC_PACKET_TYPE_SEQUENCE_HEADER,
            AVC_PACKET_TYPE_NALU,
            AVC_PACKET_TYPE_SEQUENCE_END
        };
        enum FrameType
        {
            FRAME_TYPE_KEY = 1, // AVC
            FRAME_TYPE_INTER,   // AVC
            FRAME_TYPE_DI,      // H.263
            FRAME_TYPE_GK,      // SERVER
            FRAME_TYPE_VIDEO_INFO
        };

        enum CodecId
        {
            CODEC_ID_JPEG = 1,
            CODEC_ID_H263,
            CODEC_ID_SCREEN,
            CODEC_ID_VP6,
            CODEC_ID_VP6A,
            CODEC_ID_SCREEN2,
            CODEC_ID_AVC // h264
        };
        enum TagType
        {
            TAG_TYPE_AUDIO = 0X08,
            TAG_TYPE_VIDEO = 0X09,
            TAG_TYPE_SCRIPT = 0X12
        };

        enum SoundFormat
        {
            AUDIO_FORMAT_LINEAR_PCM_PE, // linear PCM, platform endian
            AUDIO_FORMAT_ADPCM,
            AUDIO_FORMAT_MP3,
            AUDIO_FORMAT_LINEAR_PCM_LE, // linear PCM, little endian
            AUDIO_FORMAT_NELLY_16K,
            AUDIO_FORMAT_NELLY_8K,
            AUDIO_FORMAT_NELLY,
            AUDIO_FORMAT_G711A_PCM,
            AUDIO_FORMAT_G711MU_PCM,
            AUDIO_FORMAT_RESERVED,
            AUDIO_FORMAT_AAC,
            AUDIO_FORMAT_SPEEX,
            AUDIO_FORMAT_MP3_8K,
            AUDIO_FORMAT_DSS
        };

        enum SoundRate
        {
            SOUND_RATE_5P5K, // 5.5K
            SOUND_RATE_11K,
            SOUND_RATE_22K,
            SOUND_RATE_44K
        };

        enum SoundSize
        {
            SOUND_SIZE_8BIT,
            SOUND_SIZE_16BIT
        };

        enum SoundType
        {
            SOUND_TYPE_MONO,
            SOUND_TYPE_STEREO
        };

        enum AACPacketType
        {
            AAC_PACKET_TYPE_AAC_SEQUENCE_HEADER, // aac sequence header
            AAC_PACKET_TYPE_AAC_RAW 
        };

        enum GEN_OPTION
        {
            ADD_TAGSIZE = 1,
            NO_TAG_SIZE,
        };

        inline void fillSizeTo24(char sizeBuf[3], uint32_t size)
        {
            sizeBuf[0] = (uint8_t)(size >> 16 & 0xFF);
            sizeBuf[1] = (uint8_t)(size >> 8 & 0xFF);
            sizeBuf[2] = (uint8_t)(size & 0xFF);
        }

        inline void fillSizeTo248(char sizeBuf[HLS_PREV_TAGSIZE_LEN], uint32_t size)
        {
            sizeBuf[0] = (uint8_t)(size >> 16 & 0xFF);
            sizeBuf[1] = (uint8_t)(size >> 8 & 0xFF);
            sizeBuf[2] = (uint8_t)(size & 0xFF);
            sizeBuf[3] = (uint8_t)(size >> 24 & 0xFF);
        }

        inline void fillSizeToUI32(char sizeBuf[HLS_PREV_TAGSIZE_LEN], uint32_t size)
        {
            sizeBuf[0] = (uint8_t)((size >> 24) & 0xFF);
            sizeBuf[1] = (uint8_t)((size >> 16) & 0xFF);
            sizeBuf[2] = (uint8_t)((size >> 8) & 0xFF);
            sizeBuf[3] = (uint8_t)(size & 0xFF);
        }

        inline uint32_t extract248(unsigned char buf[4])
        {
            return (size_t)buf[0] + (((size_t)buf[1]) << 8) +
                   (((size_t)buf[2]) << 16) + (((size_t)buf[3]) << 24);
        }

        // previous tagsize is included
        inline void genDefFLVFileHeader(std::string &ret, GEN_OPTION option = ADD_TAGSIZE)
        {
            // UNUSED(option);
            static char flvhead[13] = {
                0x46, 0x4C, 0x56, 0x01, 0x05, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x09};

            ret.assign(flvhead, sizeof(flvhead));
        }

        inline void genFLVVideoHeader(const std::string &videoConfig,
                                      std::string &ret, GEN_OPTION option = ADD_TAGSIZE)
        {
            static char tagHead[11] = {
                0x09,             /*TagType*/
                0x00, 0x00, 0x00, /*DataSize*/
                0x00, 0x00, 0x00, /*timestamp*/
                0x00,             /*TimestampExtended*/
                0x00, 0x00, 0x00  /*StreamID*/
            };
            static char avch[5] = {
                0x17,
                0x00,            /*AVCVIDEOPACKET*/
                0x00, 0x00, 0x00 /*AVCDecoderConfigurationRecord*/
            };
            static char tagSizeFeild[HLS_PREV_TAGSIZE_LEN] = {0, 0, 0, 0};

            uint32_t tagbodylen = videoConfig.size() + 5;
            *(tagHead + 1) = (unsigned char)((tagbodylen) >> 16);
            *(tagHead + 2) = (unsigned char)((tagbodylen) >> 8);
            *(tagHead + 3) = (unsigned char)(tagbodylen);
            tagbodylen += 11;

            ret.clear();
            ret.reserve(sizeof(tagHead) + sizeof(avch) + videoConfig.size() + HLS_PREV_TAGSIZE_LEN);
            ret.append(tagHead, sizeof(tagHead));

            ret.append(avch, sizeof(avch));
            ret.append(videoConfig);

            if (option == ADD_TAGSIZE)
            {
                fillSizeToUI32(tagSizeFeild, tagbodylen);
                ret.append(tagSizeFeild, sizeof(tagSizeFeild));
            }
        }

        inline void genFLVFileTail(std::string &ret)
        {
            int tagbodylen = 0;
            char tagHead[11] = {0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            tagbodylen = 5;
            *(tagHead + 1) = (unsigned char)((tagbodylen) >> 16);
            *(tagHead + 2) = (unsigned char)((tagbodylen) >> 8);
            *(tagHead + 3) = (unsigned char)(tagbodylen);
            tagbodylen += 11;
            uint8_t avch[5] = {0x27, 2, 0, 0, 0};
            uint8_t tagSizeFeild[4] = {
                (unsigned char)((tagbodylen) >> 24),
                (unsigned char)((tagbodylen) >> 16),
                (unsigned char)((tagbodylen) >> 8),
                (unsigned char)(tagbodylen)};
            ret.append(tagHead, tagHead + 11);
            ret.append((const char *)avch, (const char *)avch + 5);
            ret.append(tagSizeFeild, tagSizeFeild + 4);
        }

        // audioSpeacificConfig
        inline void genFlvAudioHeader(const std::string &audioConfig,
                                      std::string &ret, GEN_OPTION option = ADD_TAGSIZE)
        {
            char aTagHead[11] = {
                0x08,             /*TagType*/
                0x00, 0x00, 0x00, /*DataSize*/
                0x00, 0x00, 0x00, /*timestamp*/
                0x00,             /*TimestampExtended*/
                0x00, 0x00, 0x00  /*StreamID*/
            };

            uint32_t aTagBodyLen = audioConfig.size();
            *(aTagHead + 1) = (unsigned char)((aTagBodyLen) >> 16);
            *(aTagHead + 2) = (unsigned char)((aTagBodyLen) >> 8);
            *(aTagHead + 3) = (unsigned char)(aTagBodyLen);

            aTagBodyLen += 11;
            ret.clear();
            ret.reserve(sizeof(aTagHead) + audioConfig.size() + 4);
            ret.append(aTagHead, aTagHead + 11);
            ret.append(audioConfig);

            if (option == ADD_TAGSIZE)
            {
                char atagSizeFeild[4];
                fillSizeToUI32(atagSizeFeild, aTagBodyLen);
                ret.append(atagSizeFeild, sizeof(atagSizeFeild));
            }
        }

        // audio data has no FLV header
        inline void genFlvAudioData(std::string &ret, const char *data, uint32_t len,
                                    uint32_t audioStamp, GEN_OPTION option = ADD_TAGSIZE)
        {
            // send audio tag
            unsigned char tagHead[13] = {
                0x08,             /*TagType*/
                0x00, 0x00, 0x00, /*DataSize*/
                0x00, 0x00, 0x00, /*timestamp*/
                0x00,             /*TimestampExtended*/
                0x00, 0x00, 0x00, /*StreamID*/
                0xaf, 0x01        /**/
            };
            uint32_t tagBodyLen = len + 2;

            fillSizeTo24(reinterpret_cast<char *>(tagHead) + 1, len + 2);
            fillSizeTo248(reinterpret_cast<char *>(tagHead) + 4, audioStamp);

            tagBodyLen += 11;

            ret.clear();
            ret.reserve(sizeof(tagHead) + len + 4);
            ret.append(reinterpret_cast<char *>(tagHead), 13);
            ret.append(data, len);

            if (option == ADD_TAGSIZE)
            {
                char tagSizeFeild[4];
                fillSizeToUI32(tagSizeFeild, sizeof(tagHead) + len);
                ret.append(tagSizeFeild, tagSizeFeild + 4);
            }
        }

        inline uint32_t addAmfString(unsigned char *buff, const char *data, uint16_t len)
        {
            // string type
            *buff = 0x02;
            // string len
            *(buff + 1) = (unsigned char)(len >> 8);
            *(buff + 2) = (unsigned char)(len);
            // string content
            memcpy(buff + 3, data, len);

            return len + 3;
        }

        inline uint32_t addAmfStringNoType(unsigned char *buff, const char *data, uint16_t len)
        {
            // string type
            // buff = 0x02;
            // string len
            *(buff + 0) = (unsigned char)(len >> 8);
            *(buff + 1) = (unsigned char)(len);
            // string content
            memcpy(buff + 2, data, len);

            return len + 2;
        }

        inline uint32_t addAmfNumber(unsigned char *buff, double number)
        {
            char *src = reinterpret_cast<char *>(&number);
            buff[0] = 0x00; // number

            *(buff + 1) = src[7];
            *(buff + 2) = src[6];
            *(buff + 3) = src[5];
            *(buff + 4) = src[4];
            *(buff + 5) = src[3];
            *(buff + 6) = src[2];
            *(buff + 7) = src[1];
            *(buff + 8) = src[0];

            return 9;
        }

        // audio data has no FLV header
        inline void genSimpleMetadata(std::string &ret, uint32_t videoWidth, uint32_t videoHeight,
            uint32_t frameRate, uint16_t videoCodecId, uint32_t videoDataRate, 
            const std::string codecName, GEN_OPTION option = ADD_TAGSIZE)
        {
            unsigned char buff[200] = "";
            uint32_t offset = 0;
            // script tag tag
            char tagHead[11] = {
                0x12,             /*TagType*/
                0x00, 0x00, 0x00, /*DataSize*/
                0x00, 0x00, 0x00, /*timestamp*/
                0x00,             /*TimestampExtended*/
                0x00, 0x00, 0x00  /*StreamID*/
            };

            memset(buff, 0, sizeof(buff));
            offset += addAmfString(buff, "onMetaData", sizeof("onMetaData") - 1);
            buff[offset++] = 0x08;   // ECMA array type
            buff[offset + 3] = 0x07;
            offset += 4;

            offset += addAmfStringNoType(buff + offset, "duration", sizeof "duration" - 1);
            offset += addAmfNumber(buff + offset, 0);

            offset += addAmfStringNoType(buff + offset, "width", sizeof "width" - 1);
            offset += addAmfNumber(buff + offset, videoWidth);

            offset += addAmfStringNoType(buff + offset, "height", sizeof "height" - 1);
            offset += addAmfNumber(buff + offset, videoHeight);

            offset += addAmfStringNoType(buff + offset, "framerate", sizeof "framerate" - 1);
            offset += addAmfNumber(buff + offset, frameRate);

            offset += addAmfStringNoType(buff + offset, "videocodecid", sizeof "videocodecid" - 1);
            offset += addAmfNumber(buff + offset, videoCodecId);

            offset += addAmfStringNoType(buff + offset, "videodatarate", 
                sizeof "videodatarate" - 1);
            offset += addAmfNumber(buff + offset, videoDataRate);

            offset += addAmfStringNoType(buff + offset, "codecname", sizeof "codecname" - 1);
            offset += addAmfString(buff + offset, codecName.c_str(), codecName.size());

            buff[offset] = 0x00;
            offset += 2; /* empty string is the last element */
            buff[offset] = 0x09;
            offset += 1; /* an object ends with 0x09 */

            fillSizeTo24(tagHead + 1, offset);

            ret.clear();
            ret.reserve(sizeof(tagHead) + offset + 2);
            ret.append(tagHead, sizeof(tagHead));
            ret.append(reinterpret_cast<char *>(buff), offset);

            if (option == ADD_TAGSIZE)
            {
                char tagSizeFeild[4];
                fillSizeToUI32(tagSizeFeild, sizeof(tagHead) + offset);
                ret.append(tagSizeFeild, tagSizeFeild + 4);
            }
        }

        // only for flvTag
        inline void updateFLVTagTimeStamp(std::string &flvTag, uint32_t timestamp)
        {
            if (flvTag.size() < 7)
            {
                return;
            }
            flvTag[4] = (uint8_t)(timestamp >> 16 & 0xFF);
            flvTag[5] = (uint8_t)(timestamp >> 8 & 0xFF);
            flvTag[6] = (uint8_t)(timestamp & 0xFF);
            flvTag[7] = (uint8_t)(timestamp >> 24 & 0xFF);
        }

    } // namespace flvhelper
} // namespace hercules
