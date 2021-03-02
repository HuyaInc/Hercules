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

#include "MediaPacket.h"

#include <stdint.h>
#include <string.h>

#include <string>

namespace hercules
{

    constexpr int MAX_CONFIG_LEN = 4096;

    struct CodecHeader
    {
        CodecHeader() : m_codecType(CodecType::UNKNOWN),
                        m_len(0)
        {
            memset(m_config, 0, sizeof(m_config));
        }

        bool valid()
        {
            return m_codecType != CodecType::UNKNOWN && m_len > 0;
        }

        bool operator==(const CodecHeader &rhs)
        {
            return m_codecType == rhs.m_codecType &&
                   m_len == rhs.m_len &&
                   memcmp(m_config, rhs.m_config, m_len) == 0;
        }

        bool operator!=(const CodecHeader &rhs)
        {
            return !operator==(rhs);
        }

        CodecType m_codecType;
        size_t m_len;
        uint8_t m_config[MAX_CONFIG_LEN];
    };

    struct VideoCodec
    {
        int m_width;
        int m_height;
        int m_fps;
        int m_kbps;
        CodecType m_codecType;

        VideoCodec(int w, int h, int fps, int kbps,
                   CodecType codecType = CodecType::H264)
            : m_width(w),
              m_height(h),
              m_fps(fps),
              m_kbps(kbps),
              m_codecType(codecType)
        {
        }

        std::string print()
        {
            std::ostringstream os;

            os << "width: " << m_width << ", height: " << m_height
                << ", fps: " << m_fps << ", kbps: " << m_kbps
                << ", codecType: " << CodecType2Str(m_codecType);

            return os.str();
        }
    };

    struct AudioCodec
    {
        int m_channels;
        int m_sampleRate;
        int m_kbps;
        CodecType m_codecType;

        AudioCodec(int channels, int sampleRate, int kbps,
            CodecType codecType = CodecType::AAC)
            : m_channels(channels), m_sampleRate(sampleRate), m_kbps(kbps), m_codecType(codecType)
        {
        }

        std::string print()
        {
            std::ostringstream os;

            os << "channels: " << m_channels << ", sample rate: " << m_sampleRate 
                << ", kbps: " << m_kbps << ", codec type: " << CodecType2Str(m_codecType);

            return os.str();
        }
    };

    static VideoCodec getVideoCodec(int width, int height, int fps, int kbps, const std::string &codec = "")
    {
        CodecType codecType = CodecType::H264;
        if (codec == "h265" || codec == "hevc" || codec == "H265" || codec == "HEVC")
        {
            codecType = CodecType::H265;
        }

        return VideoCodec(width, height, fps, kbps, codecType);
    }

    static AudioCodec getAudioCodec(int channels, int sampleRate, int kbps, const std::string &codec = "")
    {
        CodecType codecType = CodecType::AAC;

        if (codec == "opus" || codec == "OPUS")
        {
            codecType = CodecType::OGG_OPUS;
        }

        return AudioCodec(channels, sampleRate, kbps, codecType);
    }

    static CodecHeader getHeader(const MediaPacket &tMediaPacket)
    {
        CodecHeader avcHeader;
        avcHeader.m_codecType = tMediaPacket.getCodecType();

        if (tMediaPacket.isVideo())
        {
            if (tMediaPacket.isHeaderFrame())
            {
                memcpy(avcHeader.m_config, tMediaPacket.data(), tMediaPacket.size());
                avcHeader.m_len = tMediaPacket.size();
            }
            else if (tMediaPacket.hasGlobalHeader())
            {
                std::string avc = tMediaPacket.getGlobalHeader();
                memcpy(avcHeader.m_config, avc.data(), avc.size());
                avcHeader.m_len = avc.size();
            }
        }
        else if (tMediaPacket.isAudio())
        {
            if (tMediaPacket.isHeaderFrame())
            {
                memcpy(avcHeader.m_config, tMediaPacket.data(), tMediaPacket.size());
                avcHeader.m_len = tMediaPacket.size();
            }
        }

        return avcHeader;
    }

} // namespace hercules
