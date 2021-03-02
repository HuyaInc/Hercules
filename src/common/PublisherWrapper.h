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

#include "RtmpPublisher.h"
#include "LocalPublisher.h"
#include "Log.h"

#include <memory>
#include <string>
#include <set>

namespace hercules
{

    class PublisherWrapper
    {
    public:
        explicit PublisherWrapper(const std::string &type, const std::string &taskId)
        {
            if (type == "rtmp")
            {
                m_publisher = std::shared_ptr<Streamer>(new RtmpPublisher());
            }
            else if (type == "local")
            {
                m_publisher = std::shared_ptr<Streamer>(new LocalPublisher(taskId));
            }
        }

        ~PublisherWrapper()
        {
            logInfo(MIXLOG);
        }

        void init(Queue<MediaPacket> &videoQueue, Queue<MediaPacket> &audioQueue)
        {
            m_publisher->init(videoQueue, audioQueue);
        }

        void setPushParam(const std::string &name, const std::string url, uint64_t uid)
        {
            m_publisher->setParam(name, url, uid);
        }

        void setOpt(const StreamOpt &opt)
        {
            m_publisher->setOpt(opt);
        }

        void setVideoCodec(int width, int height, int fps,
            int kbps, const std::string &codec)
        {
            m_publisher->setVideoCodec(width, height, fps, kbps, codec);
        }

        void setAudioCodec(int channels, int sampleRate,
            int kbps, const std::string &codec)
        {
            m_publisher->setAudioCodec(channels, sampleRate, kbps, codec);
        }

        void setUid(uint64_t uid) { m_publisher->setUid(uid); }
        uint64_t getUid() const { return m_publisher->getUid(); }

        void setAppid(uint32_t appid) { m_publisher->setAppid(appid); }
        uint32_t getAppid() const { return m_publisher->getAppid(); }

        void setStreamName(const std::string &streamName) { m_publisher->setStreamName(streamName);}
        std::string getStreamName() const { return m_publisher->getStreamName(); }

        void addTraceInfo(const std::string &jobKey,
            uint64_t traceUid, const std::string &traceStreamName)
        {
            m_publisher->addTraceInfo(jobKey, traceUid, traceStreamName);
        }
        void addTraceInfo(const Property::TraceInfo &traceInfo)
        {
            m_publisher->addTraceInfo(traceInfo);
        }
        std::set<Property::TraceInfo> getTraceInfos() { return m_publisher->getTraceInfos(); }

        void removeTraceInfo(const Property::TraceInfo &traceInfo)
        {
            m_publisher->removeTraceInfo(traceInfo);
        }
        void removeTraceInfo(const std::string &jobKey,
            uint64_t traceUid, const std::string &traceStreamName)
        {
            m_publisher->removeTraceInfo(jobKey, traceUid, traceStreamName);
        }

        std::string traceInfo() const { return m_publisher->traceInfo(); }

        void start() { m_publisher->start(); }
        void stop() { m_publisher->stop(); }
        void join() { m_publisher->join(); }

        std::shared_ptr<Streamer> getPtr()
        {
            return m_publisher;
        }

        Queue<MediaPacket> &getVideoQueue()
        {
            return *m_publisher->getVideoQueue();
        }

        Queue<MediaPacket> &getAudioQueue()
        {
            return *m_publisher->getAudioQueue();
        }

    private:
        std::shared_ptr<Streamer> m_publisher;
    };

} // namespace hercules
