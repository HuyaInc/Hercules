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

#include <stdint.h>

#include <atomic>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>

namespace hercules
{

    enum class MediaType
    {
        UNKNOWN = -1,
        VIDEO = 1,
        AUDIO = 2,
        APP = 10,
        DATA = 11,
    };

    typedef uint32_t TIMESTAMP;

    static std::string MediaType2Str(MediaType type)
    {
        switch (type)
        {
        case MediaType::UNKNOWN:
            return "UNKNOWN";
        case MediaType::VIDEO:
            return "VIDEO";
        case MediaType::AUDIO:
            return "AUDIO";
        case MediaType::APP:
            return "APP";
        default:
            return "UNKNOWN";
        }

        return "UNKNOWN";
    }

    enum class FrameType
    {
        UNKNOWN = -1,
        I = 1,
        P = 2,
        B = 3,
        HEADER = 4,

        CLEAN_QUEUE = 10,
    };

    static std::string FrameType2Str(FrameType type)
    {
        switch (type)
        {
        case FrameType::UNKNOWN:
            return "UNKNOWN";
        case FrameType::I:
            return "I";
        case FrameType::P:
            return "P";
        case FrameType::B:
            return "B";
        case FrameType::HEADER:
            return "HEADER";
        case FrameType::CLEAN_QUEUE:
            return "CLEAN_QUEUE";
        default:
            return "UNKNOWN";
        }

        return "UNKNOWN";
    }

    enum class CodecType
    {
        UNKNOWN = -1,
        H264 = 7,
        OGG_OPUS = 9,
        AAC = 10,
        H265 = 12
    };

    static std::string CodecType2Str(CodecType type)
    {
        switch (type)
        {
        case CodecType::UNKNOWN:
            return "UNKNOWN";
        case CodecType::H264:
            return "H264";
        case CodecType::AAC:
            return "AAC";
        case CodecType::OGG_OPUS:
            return "OGG_OPUS";
        case CodecType::H265:
            return "H265";
        default:
            return "UNKNOWN";
        }

        return "UNKNOWN";
    }

    enum class TimeTraceKey
    {
        RECV = 2,
        DELIVER = 3,
        DECODE = 6,
        MIXED = 10,
        ENCODE = 14,
        SEND = 18,
    };

    static std::string TimeTraceKey2Str(TimeTraceKey type)
    {
        switch (type)
        {
        case TimeTraceKey::RECV:
            return "RECV";
        case TimeTraceKey::DELIVER:
            return "DELIVER";
        case TimeTraceKey::DECODE:
            return "DECODE";
        case TimeTraceKey::MIXED:
            return "MIXED";
        case TimeTraceKey::ENCODE:
            return "ENCODE";
        case TimeTraceKey::SEND:
            return "SEND";
        default:
            return "UNKNOWN";
        }

        return "UNKNOWN";
    }

    class MediaBase
    {
    public:
        MediaBase() : m_mediaType(MediaType::UNKNOWN),
                      m_frameType(FrameType::UNKNOWN),
                      m_codecType(CodecType::UNKNOWN),
                      m_dts(0),
                      m_pts(0),
                      m_globalId(m_globalIncrId.fetch_add(1)),
                      m_frameId(0),
                      m_streamId(0),
                      m_audioSampleRate(0),
                      m_audioChannels(0)
        {
        }

        virtual ~MediaBase() {}

        MediaType getMediaType() const { return m_mediaType; }
        void setMediaType(MediaType type) { m_mediaType = type; }

        void asVideo() { setMediaType(MediaType::VIDEO); }
        void asAudio() { setMediaType(MediaType::AUDIO); }
        void asApp() { setMediaType(MediaType::APP); }

        bool isVideo() const { return MediaType::VIDEO == m_mediaType; }
        bool isAudio() const { return MediaType::AUDIO == m_mediaType; }
        bool isApp() const { return MediaType::APP == m_mediaType; }

        FrameType getFrameType() const { return m_frameType; }
        void setFrameType(const FrameType &type) { m_frameType = type; }

        void asIFrame() { setFrameType(FrameType::I); }
        void asPFrame() { setFrameType(FrameType::P); }
        void asBFrame() { setFrameType(FrameType::B); }
        void asHeaderFrame() { setFrameType(FrameType::HEADER); }
        void asCleanQueueFrame() { setFrameType(FrameType::CLEAN_QUEUE); }

        bool isIFrame() const { return m_frameType == FrameType::I; }
        bool isPFrame() const { return m_frameType == FrameType::P; }
        bool isBFrame() const { return m_frameType == FrameType::B; }
        bool isHeaderFrame() const { return m_frameType == FrameType::HEADER; }
        bool isCleanQueueFrame() const { return m_frameType == FrameType::CLEAN_QUEUE; }

        CodecType getCodecType() const { return m_codecType; }
        void setCodecType(const CodecType &type) { m_codecType = type; }

        void asH264() { setCodecType(CodecType::H264); }
        void asH265() { setCodecType(CodecType::H265); }
        void asAAC() { setCodecType(CodecType::AAC); }
        void asOGG_OPUS() { setCodecType(CodecType::OGG_OPUS); }

        bool isH264() const { return m_codecType == CodecType::H264; }
        bool isH265() const { return m_codecType == CodecType::H265; }
        bool isAAC() const { return m_codecType == CodecType::AAC; }
        bool isOGG_OPUS() const { return m_codecType == CodecType::OGG_OPUS; }

        TIMESTAMP getDts() const { return m_dts; }
        void setDts(TIMESTAMP dts) { m_dts = dts; }

        TIMESTAMP getPts() const { return m_pts; }
        void setPts(TIMESTAMP pts) { m_pts = pts; }

        uint32_t getRecvTime() const { return m_recvTime; }
        void setRecvTime(uint32_t recvTime) { m_recvTime = recvTime; }

        uint64_t getGlobalId() const { return m_globalId; }

        uint64_t getFrameId() const { return m_frameId; }
        void setFrameId(uint64_t id) { m_frameId = id; }

        uint64_t getStreamId() const { return m_streamId; }
        void setStreamId(uint64_t id) { m_streamId = id; }

        void setStreamName(const std::string &streamName) { m_streamName = streamName; }
        std::string getStreamName() const { return m_streamName; }

        uint32_t getAudioSampleRate() const { return m_audioSampleRate; }
        void setAudioSampleRate(uint32_t rate) { m_audioSampleRate = rate; }

        uint8_t getAudioChannels() const { return m_audioChannels; }
        void setAudioChannels(uint8_t channel) { m_audioChannels = channel; }

        void addIdTimestamp(uint64_t id, uint32_t timestamp)
        {
            m_idTimestamp[id] = timestamp;
        }
        void setIdTimestamp(const std::map<uint64_t, uint32_t> &idTimestamp)
        {
            m_idTimestamp = idTimestamp;
        }
        std::map<uint64_t, uint32_t> getIdTimestamp() const { return m_idTimestamp; }
        uint32_t getInIdTimestamp(uint64_t id) const
        {
            auto iter = m_idTimestamp.find(id);
            if (iter == m_idTimestamp.end())
            {
                return 0;
            }

            return iter->second;
        }

        std::string getIdTimestampStr() const
        {
            std::ostringstream os;
            size_t count = 0;
            for (const auto &kv : m_idTimestamp)
            {
                os << kv.first << ":" << kv.second;

                ++count;
                if (count != m_idTimestamp.size())
                {
                    os << "|";
                }
            }

            return os.str();
        }

        void setIdVolume(const std::map<uint64_t, uint32_t> &idVolume) { m_idVolume = idVolume; }
        std::map<uint64_t, uint32_t> getIdVolume() const { return m_idVolume; }

        void addIdTimeTrace(uint64_t id, const TimeTraceKey &key, uint32_t timestamp)
        {
            m_idTimeTrace[id][key] = timestamp;
        }
        void addAllIdTimeTrace(const TimeTraceKey &key, uint32_t timestamp)
        {
            for (auto &kv : m_idTimeTrace)
            {
                kv.second[key] = timestamp;
            }
        }

        void setIdTimeTrace(const std::map<uint64_t, std::map<TimeTraceKey, uint32_t>> &trace)
        {
            m_idTimeTrace = trace;
        }
        std::map<uint64_t, std::map<TimeTraceKey, uint32_t>> getIdTimeTrace() const
        {
            return m_idTimeTrace;
        }

        std::string getIdTimeTraceStr() const
        {
            std::ostringstream os;
            size_t count = 0;
            for (const auto &kv : m_idTimeTrace)
            {
                os << kv.first << ":(";

                size_t subSount = 0;
                int timeElapse = 0;
                uint32_t preTime = 0;
                for (const auto &key_time : kv.second)
                {
                    os << TimeTraceKey2Str(key_time.first) << "->" << key_time.second;

                    ++subSount;

                    if (preTime != 0)
                    {
                        int diff = (key_time.second - preTime);
                        timeElapse += diff;
                        os << "(" << diff << ")";
                    }

                    preTime = key_time.second;

                    if (subSount != kv.second.size())
                    {
                        os << ",";
                    }
                }

                os << "), elapse:" << timeElapse;

                ++count;
                if (count != m_idTimeTrace.size())
                {
                    os << "|";
                }
            }

            return os.str();
        }

        std::string getIdVolumeStr() const
        {
            uint32_t count = 0;
            std::ostringstream os;
            for (const auto &id2volume : m_idVolume)
            {
                os << "(" << id2volume.first << ", " << id2volume.second << ")";
                ++count;
                if (count != m_idVolume.size())
                {
                    os << ", ";
                }
            }

            return os.str();
        }

    protected:
        virtual void copy(const MediaBase &rhs)
        {
            m_mediaType = rhs.m_mediaType;
            m_frameType = rhs.m_frameType;
            m_codecType = rhs.m_codecType;
            m_dts = rhs.m_dts;
            m_pts = rhs.m_pts;
            m_recvTime = rhs.m_recvTime;
            m_globalId = rhs.m_globalId;
            m_frameId = rhs.m_frameId;
            m_streamId = rhs.m_streamId;
            m_streamName = rhs.m_streamName;

            m_audioSampleRate = rhs.m_audioSampleRate;
            m_audioChannels = rhs.m_audioChannels;
            m_idTimestamp = rhs.m_idTimestamp;
            m_idTimeTrace = rhs.m_idTimeTrace;
            m_idVolume = rhs.m_idVolume;
        }

        virtual void destroy()
        {
        }

        virtual std::string print() const
        {
            std::ostringstream os;
            os << MediaType2Str(m_mediaType) << ", "
               << FrameType2Str(m_frameType) << ", "
               << CodecType2Str(m_codecType)
               << ", dts: " << m_dts
               << ", pts: " << m_pts
               << ", frameId: " << m_frameId
               << ", streamId: " << m_streamId
               << ", streamName: " << m_streamName
               << ", idTimestamp: " << getIdTimestampStr()
               << ", idTimeTrace: " << getIdTimeTraceStr()
               << ", idVolume: " << getIdVolumeStr();

            return os.str();
        }

    protected:
        MediaType m_mediaType;
        FrameType m_frameType;
        CodecType m_codecType;
        TIMESTAMP m_dts;
        TIMESTAMP m_pts;
        uint32_t m_recvTime;
        uint64_t m_globalId;
        uint64_t m_frameId;
        uint64_t m_streamId;
        std::string m_streamName;

        uint32_t m_audioSampleRate;
        uint8_t m_audioChannels;
        std::map<uint64_t, uint32_t> m_idVolume;

        std::map<uint64_t, uint32_t> m_idTimestamp;
        std::map<uint64_t, std::map<TimeTraceKey, uint32_t>> m_idTimeTrace;

        static std::atomic<uint64_t> m_globalIncrId;
    };

} // namespace hercules
