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

#include "MediaPacket.h"
#include "Common.h"
#include "FlvFile.h"

#include <string>
#include <algorithm>

namespace hercules
{
    using flvhelper::AACPacketType;
    using flvhelper::AVCPacketType;
    using flvhelper::SoundFormat;
    using flvhelper::TagType;
    using std::string;

#define FLV_SOUND_FORMAT_AAC 10
#define FLV_AVC_KEY_FRAME 0x17
#define FLV_AVC_INTER_FRAME 0x27
#define AAC_44100_S16_STEREO 0XAF

    std::atomic<uint64_t> MediaPacket::m_avpacketRefCount(0);

    MediaPacket::MediaPacket()
        : MediaBase()
        , m_packet(nullptr)
        , m_ref(nullptr)
    {
    }

    MediaPacket::~MediaPacket()
    {
        destroy();
    }

    MediaPacket::MediaPacket(const MediaPacket &rhs)
    {
        if (this == &rhs)
        {
            return;
        }

        copy(rhs);
    }

    MediaPacket &MediaPacket::operator=(const MediaPacket &rhs)
    {
        if (this == &rhs)
        {
            return *this;
        }

        destroy();
        copy(rhs);

        return *this;
    }

    void MediaPacket::appendPreTagSize(std::string &tag)
    {
        uint32_t pre_tag_size = tag.size();

        tag.append(1, static_cast<char>((pre_tag_size >> 24) & 0xFF));
        tag.append(1, static_cast<char>((pre_tag_size >> 16) & 0xFF));
        tag.append(1, static_cast<char>((pre_tag_size >> 8) & 0xFF));
        tag.append(1, static_cast<char>((pre_tag_size)&0xFF));
    }

    int MediaPacket::parseFlvTagHeader(const uint8_t *data, int size,
        MediaType &mediaType, uint32_t &dataSize, uint32_t &timeStamp, uint32_t &streamId)
    {
        if (size < 11)
        {
            return -1;
        }

        uint8_t tagType = data[0] & 0x1F;

        mediaType = MediaType::UNKNOWN;
        if (tagType == TagType::TAG_TYPE_AUDIO)
        {
            mediaType = MediaType::AUDIO;
        }
        else if (tagType == TagType::TAG_TYPE_VIDEO)
        {
            mediaType = MediaType::VIDEO;
        }
        else
        {
            return -1;
        }

        dataSize = (data[1] << 16) | (data[2] << 8) | data[3];
        timeStamp = (data[7] << 24) | (data[4] << 16) | (data[5] << 8) | data[6];
        streamId = (data[8] << 16) | (data[9] << 8) | data[10];

        if (streamId != 0)
        {
            return -1;
        }

        return 0;
    }

    int MediaPacket::genMediaPacketFromFlvWithHeader(uint32_t timestamp,
        const uint8_t *data, int size, MediaPacket &packet)
    {
        MediaType mediaType = MediaType::UNKNOWN;
        uint32_t dataSize = 0;
        uint32_t streamId = 0xFFFFFFFF;
        uint32_t ignore_timestamp_in_flv = 0;

        if (parseFlvTagHeader(data, size, mediaType, dataSize,
            ignore_timestamp_in_flv, streamId) < 0)
        {
            return -1;
        }

        return genMediaPacketFromFlvWithoutHeader(mediaType, timestamp,
            data + 11, size - 11, packet);
    }

    int MediaPacket::genMediaPacketFromFlvWithHeader(const uint8_t *data,
        int size, MediaPacket &packet)
    {
        MediaType mediaType = MediaType::UNKNOWN;
        uint32_t dataSize = 0;
        uint32_t streamId = 0xFFFFFFFF;
        uint32_t timestamp = 0;

        if (parseFlvTagHeader(data, size, mediaType, dataSize, timestamp, streamId) < 0)
        {
            return -1;
        }

        return genMediaPacketFromFlvWithoutHeader(mediaType, timestamp, data + 11, size - 11,
            packet);
    }

    int MediaPacket::genMediaPacketFromFlvWithoutHeader(MediaType mediaType,
        uint64_t dts, const uint8_t *data, int size, MediaPacket &packet)
    {
        if (mediaType == MediaType::VIDEO)
        {
            if (size < 5)
            {
                return -1;
            }

            packet.asVideo();

            uint8_t frameType = (data[0] & 0xF0) >> 4;
            uint8_t codecId = (data[0] & 0x0F);

            if (codecId == 7)
            {
                packet.asH264();
            }
            else if (codecId == 12)
            {
                packet.asH265();
            }

            uint8_t packetType = data[1];

            int32_t compositionTime = 0;

            if (packetType == 0)
            {
                packet.asHeaderFrame();
            }
            else
            {
                uint32_t cts = (data[2] << 16) | (data[3] << 8) | data[4];
                compositionTime = (cts + 0xFF800000) ^ 0xFF800000;
                if (frameType == 0x01)
                {
                    packet.asIFrame();
                }
                else if (frameType == 0x02)
                {
                    packet.asPFrame();

                    if (compositionTime != 0)
                    {
                        packet.asBFrame();
                    }
                }
            }

            packet.setDts(dts);
            packet.setPts(dts + compositionTime);

            AVPacket *avPacket = av_packet_alloc();
            size_t rawSize = size - 5;
            uint8_t *rawData = reinterpret_cast<uint8_t *>(
                av_mallocz(rawSize + AV_INPUT_BUFFER_PADDING_SIZE));
            memcpy(rawData, const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(data)) + 5,
                rawSize);
            av_packet_from_data(avPacket, rawData, rawSize);

            packet.setAVPacket(avPacket);

            return 0;
        }
        else if (mediaType == MediaType::AUDIO)
        {
            if (size < 2)
            {
                return -1;
            }

            packet.asAudio();
            packet.setDts(dts);
            packet.setPts(dts);

            size_t rawSize = 0;
            uint8_t *rawData = nullptr;
            AVPacket *avPacket = av_packet_alloc();
            uint8_t soundFormat = (data[0] & 0xF0) >> 4;
            if (soundFormat == SoundFormat::AUDIO_FORMAT_AAC) // opus
            {
                if (size < 2)
                {
                    return -1;
                }

                uint8_t soundRate = (data[0] & 0x0C) >> 2;
                switch (soundRate)
                {
                case 0:
                    packet.setAudioSampleRate(5500);
                    break;
                case 1:
                    packet.setAudioSampleRate(11000);
                    break;
                case 2:
                    packet.setAudioSampleRate(22050);
                    break;
                case 3:
                    packet.setAudioSampleRate(44100);
                    break;
                }

                uint8_t soundSize = (data[0] & 0x02) >> 1;
                uint8_t soundType = data[0] & 0x01;
                switch (soundType)
                {
                case 0:
                    packet.setAudioChannels(1);
                    break;
                case 1:
                    packet.setAudioChannels(2);
                    break;
                }

                uint8_t packetType = data[1];

                if (packetType == 0)
                {
                    packet.asHeaderFrame();
                }
                else
                {
                    packet.asIFrame();
                }

                packet.asAAC();
                rawSize = size - 2;
                rawData = reinterpret_cast<uint8_t *>(av_malloc(rawSize));
                memcpy(rawData, const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(data)) + 2,
                    rawSize);
            }

            av_packet_from_data(avPacket, rawData, rawSize);

            packet.setAVPacket(avPacket);

            return 0;
        }

        return -1;
    }

    int MediaPacket::mediaPacketToFlvWithoutHeader(const MediaPacket &packet, 
        string &flvWithoutHeader)
    {
        if (packet.isVideo())
        {
            uint8_t videoTagHeader[5] = {0};
            if (packet.isIFrame() || packet.isHeaderFrame())
            {
                // default h264
                videoTagHeader[0] = FLV_AVC_KEY_FRAME;
            }
            else
            {
                // default h264
                videoTagHeader[0] = FLV_AVC_INTER_FRAME;
            }

            if (!packet.isHeaderFrame())
            {
                videoTagHeader[1] = AVCPacketType::AVC_PACKET_TYPE_NALU;
            }

            // if (packet.isBFrame())
            {
                uint32_t compositionTime = packet.getPts() - packet.getDts();

                videoTagHeader[2] = (compositionTime & 0xFF0000) >> 16;
                videoTagHeader[3] = (compositionTime & 0x00FF00) >> 8;
                videoTagHeader[4] = (compositionTime & 0xFF);
            }

            flvWithoutHeader.append((const char *)videoTagHeader, sizeof(videoTagHeader));

            flvWithoutHeader.append(packet.m_sei);

            flvWithoutHeader.append((const char *)packet.data(),
                packet.size() - packet.m_sei.size());

            return 0;
        }
        else if (packet.isAudio())
        {
            uint8_t audioTagHeader[2] = {0};

            audioTagHeader[0] = AAC_44100_S16_STEREO;

            if (!packet.isHeaderFrame())
            {
                audioTagHeader[1] = AACPacketType::AAC_PACKET_TYPE_AAC_RAW;
            }

            flvWithoutHeader.append((const char *)audioTagHeader, sizeof(audioTagHeader));

            flvWithoutHeader.append((const char *)packet.data(), packet.size());

            return 0;
        }

        return -1;
    }

    int MediaPacket::mediaPacketToFlvWithHeader(const MediaPacket &packet, string &flvWithHeader)
    {
        uint8_t flvHeader[flvhelper::TAG_HEADER_SIZE] = {0};
        if (packet.isVideo())
        {
            // type: video
            flvHeader[0] = TagType::TAG_TYPE_VIDEO;

            // size
            uint32_t size = packet.size() + 5;
            flvHeader[1] = (size & 0x00FF0000) >> 16;
            flvHeader[2] = (size & 0x0000FF00) >> 8;
            flvHeader[3] = size & 0xFF;

            // timestamp
            uint32_t timestamp = packet.getDts();
            flvHeader[4] = (timestamp & 0x00FF0000) >> 16;
            flvHeader[5] = (timestamp & 0x0000FF00) >> 8;
            flvHeader[6] = timestamp & 0xFF;
            flvHeader[7] = (timestamp & 0xFF000000) >> 24;

            // stream id
            flvHeader[8] = 0x00;
            flvHeader[9] = 0x00;
            flvHeader[10] = 0x00;

            flvWithHeader.append((const char *)flvHeader, sizeof(flvHeader));
        }
        else if (packet.isAudio())
        {
            // type: audio
            flvHeader[0] = TagType::TAG_TYPE_AUDIO;

            // size
            uint32_t size = packet.size() + 2;
            flvHeader[1] = (size & 0x00FF0000) >> 16;
            flvHeader[2] = (size & 0x0000FF00) >> 8;
            flvHeader[3] = size & 0xFF;

            // timestamp
            uint32_t timestamp = packet.getDts();
            flvHeader[4] = (timestamp & 0x00FF0000) >> 16;
            flvHeader[5] = (timestamp & 0x0000FF00) >> 8;
            flvHeader[6] = timestamp & 0xFF;
            flvHeader[7] = (timestamp & 0xFF000000) >> 24;

            // stream id
            flvHeader[8] = 0x00;
            flvHeader[9] = 0x00;
            flvHeader[10] = 0x00;

            flvWithHeader.append((const char *)flvHeader, sizeof(flvHeader));
        }

        string flvTag;
        if (mediaPacketToFlvWithoutHeader(packet, flvTag) != 0)
        {
            return -1;
        }

        flvWithHeader.append(flvTag);
        appendPreTagSize(flvWithHeader);

        return 0;
    }

    void MediaPacket::copy(const MediaPacket &rhs)
    {
        MediaBase::copy(rhs);

        m_packet = rhs.m_packet;
        m_globalHeader = rhs.m_globalHeader;
        m_ref = rhs.m_ref;
        m_sei = rhs.m_sei;
        m_forward = rhs.m_forward;

        ref();
    }

    void MediaPacket::destroy()
    {
        MediaBase::destroy();

        if (unref() == 1)
        {
            if (m_packet != nullptr)
            {
                av_packet_free(&m_packet);
            }

            delete m_ref;
            m_ref = nullptr;

            m_avpacketRefCount.fetch_add(-1);
        }
    }

} // namespace hercules
