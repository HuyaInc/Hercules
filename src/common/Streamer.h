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
#include "CodecUtil.h"
#include "Common.h"
#include "Property.h"
#include "Queue.h"
#include "CycleCounterStat.h"
#include "SubscribeContext.h"
#include "Log.h"

#include <utility>
#include <map>
#include <string>

namespace hercules
{

    constexpr int SIZE_8K = 8 * 1024;

    struct StreamOpt
    {
        StreamOpt()
            : m_publishVideo(true)
            , m_publishAudio(true)
            , m_publishDataStream(false)
            , m_fixUid(true)
            , m_phonyUid(0)
            , m_smooth(true)
        {
        }

        bool m_publishVideo;
        bool m_publishAudio;
        bool m_publishDataStream;
        bool m_fixUid;
        uint64_t m_phonyUid;
        bool m_smooth;

        std::string toString() const
        {
            char buf[SIZE_8K];
            int ret = snprintf(buf, sizeof(buf),
                               "publish_video=%d, publish_audio=%d, publish_data_stream=%d,"
                               " fixUid=%d, phonyUid=%lu, smooth=%d",
                               m_publishVideo,
                               m_publishAudio,
                               m_publishDataStream,
                               m_fixUid,
                               m_phonyUid,
                               m_smooth);

            return std::string(buf, ret);
        }
    };

    class Streamer : public Property
    {
    public:
        Streamer()
            : Property()
            , m_audioFrameId(0)
            , m_videoFrameId(0)
            , m_dataFrameId(0)
            , m_videoCodec(DEFAULT_WIDTH
                , DEFAULT_HEIGHT
                , DEFAULT_FPS
                , DEFAULT_VIDEO_BPS
                , CodecType::H264)
            , m_audioCodec(DEFAULT_CHANNEL_NUM
                , DEFAULT_SAMPLE_RATE
                , DEFAULT_AUDIO_BPS
                , CodecType::AAC)
            , m_audioFpsStat()
            , m_videoFpsStat()
        {
        }

        virtual ~Streamer()
        {
            logInfo(MIXLOG);
        }

        void init(Queue<MediaPacket> &videoQueue, Queue<MediaPacket> &audioQueue)
        {
            /* m_videoQueue = &videoQueue; */
            /* m_audioQueue = &audioQueue; */
        }

        void setOpt(const StreamOpt &opt) { m_opt = opt; }
        StreamOpt getOpt() const { return m_opt; }

        virtual void setParam(const std::string &streamname,
            const std::string &url, uint64_t uid)
        {
            uint64_t fixUid = uid;

            Property::setStreamName(streamname);
            Property::setUid(fixUid);

            m_url = url;
        }

        virtual void setVideoCodec(int width, int height, int fps,
            int kbps, const std::string &codec)
        {
            m_videoCodec = getVideoCodec(width, height, fps, kbps, codec);
        }

        virtual void setAudioCodec(int channels, int sampleRate,
            int kbps, const std::string &codec)
        {
            m_audioCodec = getAudioCodec(channels, sampleRate, kbps, codec);
        }

        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void join() = 0;

        void addSubscriber(const std::string &subscriberName, const SubscribeContext &context)
        {
            if (m_subscriberMap.find(subscriberName) == m_subscriberMap.end())
            {
                logInfo(MIXLOG << "subscriberName=" << subscriberName << " insert");
                m_subscriberMap.insert(make_pair(subscriberName, context));
            }
            else
            {
                logInfo(MIXLOG << "subscriberName=" << subscriberName << " replace");
                m_subscriberMap[subscriberName] = context;
            }

            onNewSubScriber(subscriberName, context);
        }

        virtual void onNewSubScriber(const std::string &subscriberName, 
            const SubscribeContext &context)
        {
        }

        void delSubscriber(const std::string &subscriberName)
        {
            auto itr = m_subscriberMap.find(subscriberName);
            if (m_subscriberMap.end() != itr)
            {
                logInfo(MIXLOG << "subscriberName=" << subscriberName << " erase");
                m_subscriberMap.erase(itr);
            }
            else
            {
                logInfo(MIXLOG << "subscriberName=" << subscriberName << " no found");
            }
        }

        bool hasNoSubscriber()
        {
            return m_subscriberMap.size() == 0;
        }

        size_t subscriberCount()
        {
            return m_subscriberMap.size();
        }

        Queue<MediaPacket> *getVideoQueue() { return &m_videoQueue; }
        Queue<MediaPacket> *getAudioQueue() { return &m_audioQueue; }

        virtual void checkStatus(uint64_t counter) {}

    protected:
        std::string m_url;

        Queue<MediaPacket> m_videoQueue;
        Queue<MediaPacket> m_audioQueue;

        uint64_t m_audioFrameId;
        uint64_t m_videoFrameId;
        uint64_t m_dataFrameId;

        CycleCounterStat<1000> m_audioFpsStat;
        CycleCounterStat<1000> m_videoFpsStat;

        CycleCounterStat<1000> m_dataFpsStat;

        VideoCodec m_videoCodec;
        AudioCodec m_audioCodec;
        std::map<std::string, SubscribeContext> m_subscriberMap;

        StreamOpt m_opt;
        uint32_t m_maxSeiSize;
    };

} // namespace hercules
