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

#include "ByteUtil.h"
#include "FlvHelper.h"

#include <vector>
#include <cinttypes>
#include <iostream>
#include <map>
#include <fstream>
#include <string>

namespace hercules
{

    namespace flv
    {
        constexpr int DEFAULT_BUFFER_SIZE = 4 * 1024 * 1024;

        class FlvFile;

        class TagHeader
        {
        public:
            flvhelper::TagType tagType_; // 1byte
            uint32_t dataSize_;          // 3byte
            uint32_t timeStamp_;         // 3byte
            uint8_t timeStampEx_;        // 1byte
            uint32_t streamId_;          // 3byte
            uint32_t totalTimeStamp_;    // calc
            TagHeader() : dataSize_(0), timeStamp_(0), timeStampEx_(0), streamId_(0),
                          totalTimeStamp_(0)
            {
            }
            ~TagHeader()
            {
            }
        };

        class Tag
        {
        public:
            Tag() : prevTagLen_(0)
            {
            }
            void init(TagHeader *header, unsigned char *data, uint32_t size);

            TagHeader tagHeader_;      // tagheader
            int64_t dts_;              // dts
            int64_t pts_;              // pts
            BinaryData tagHeaderData_; // tagheader
            BinaryData tagData_;       // tagdata
            BinaryData mediaData_;     // mediadata
            uint32_t prevTagLen_;      // prev tag length
        };

        class VideoTag : public Tag
        {
        public:
            VideoTag(TagHeader *header, unsigned char *data, uint32_t size,
                     FlvFile *parser);
            flvhelper::FrameType frameType_;
            flvhelper::CodecId codecId_;
            // h264
            flvhelper::AVCPacketType avcPacketType_; // 1byte
            uint32_t compositionTime_;               // 3byte

            int parseH264Tag(FlvFile *pParser);
            int parseH264Configuration(FlvFile *pParser, unsigned char *pTagDamZa);
            int parseNalu(FlvFile *pParser, unsigned char *pTagData);
        };

        class AudioTag : public Tag
        {
        public:
            AudioTag(TagHeader *header, unsigned char *data, uint32_t size,
                     FlvFile *parser);
            int parseAACTag(FlvFile *parser);
            int parseAudioSpecificConfig(FlvFile *pParser, unsigned char *pTagData);
            int parseRawAAC(FlvFile *pParser, unsigned char *pTagData);
            flvhelper::SoundFormat soundFormat_; // 4bit
            flvhelper::SoundRate soundRate_;     // 2bit
            flvhelper::SoundSize soundSize_;     // 1bit soundsize
            flvhelper::SoundType soundType_;     // 1bit soundtype

            flvhelper::AACPacketType aacPacketType_;
            static int aacProfile_;
            static int sampleRateIndex_;
            static int channelConfig_;
        };

        class ScriptTag : public Tag
        {
        public:
            ScriptTag(TagHeader *header, unsigned char *data, uint32_t size,
                      FlvFile *parser);

            void show();
            int parseMetadata(unsigned char *data, uint32_t size, std::string key);
            double duration_;
            double width_;
            double height_;
            double videoDataRate_;
            double framerate_;
            double videoCodecId_;
            double audioSampleRate_;
            double audioSampleSize_;
            bool stereo_;
            double audioCodecId_;
            double fileSize_;
        };

        class FlvHeader
        {
        public:
            char *signature_; // "FLV" (0X46 0X4C 0X66) 3byte
            uint8_t version_; // 1byte
            uint8_t flags_;   // 1byte
            bool haveVideo_;
            bool haveAudio_;
            uint32_t headerSize_;
            unsigned char *data_;
        };

        class FlvFile
        {
        public:
            FlvFile();
            virtual ~FlvFile();

            int init(const std::string &fileName);
            int destory();

            Tag *parseFlv();
            Tag *parse(unsigned char *pBuf, int nBufSize, int *usedLen);
            int printInfo();
            int dumpH264(const std::string &path);
            int dumpAAC(const std::string &path);
            int dumpFlv(const std::string &path);
            int decodeH264();
            int decodeAAC();
            Tag *createTag(unsigned char *pBuf, int nLeftLen);

            std::string flvHeader() { return flvHeaderStr_; }

        private:
            struct FlvStat
            {
                int metaNum_;
                int videoNum_;
                int audioNum_;
                int maxTimeStamp_;
                int lengthSize_;

                FlvStat() : metaNum_(0), videoNum_(0), audioNum_(0)
                    , maxTimeStamp_(0), lengthSize_(0)
                {
                }
                ~FlvStat()
                {
                }
            };

        private:
            FlvHeader *createFlvHeader(unsigned char *pBuf);
            int destroyFlvHeader(FlvHeader *pHeader);
            int destroyTag(Tag *pTag);
            int stat();
            int statVideo(Tag *pTag);
            int IsUserDataTag(Tag *pTag);

        private:
            std::string flvHeaderStr_;
            FlvHeader *flvHeader_;
            std::vector<Tag *> vpTag_;
            FlvStat sStat_;

        public:
            std::vector<int> seekPos_;
            int nalUnitLength_;
            ScriptTag *metadata_;
            std::fstream fs_;
            std::string fileName_;
            int unusedLen_;
            unsigned char buf_[DEFAULT_BUFFER_SIZE];
        };
    } // namespace flv
} //  namespace hercules
