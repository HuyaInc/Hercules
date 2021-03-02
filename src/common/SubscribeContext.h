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
#include "MediaFrame.h"
#include "Queue.h"

#include <memory>
#include <string>
#include <algorithm>

namespace hercules
{

    typedef std::shared_ptr<Queue<MediaPacket>> packetQueuePtr;
    typedef std::shared_ptr<Queue<MediaFrame>> frameQueuePtr;

    class SubscribeContext
    {
    public:
        SubscribeContext() 
            : m_videoPacketQueue(nullptr)
            , m_audioPacketQueue(nullptr)
            , m_videoFrameQueue(nullptr)
            , m_audioFrameQueue(nullptr)
            , m_audioCnt(0)
            , m_videoCnt(0)
        {
        }

        ~SubscribeContext() {}

        void subscribeVideoPacket();
        void subscribeAudioPacket();
        void subscribeVideoFrame();
        void subscribeAudioFrame();

        void subscribeJobFrame(const std::string &jobId, const std::string &streamName);

        void copy(const SubscribeContext &rhs)
        {
            m_videoPacketQueue = rhs.m_videoPacketQueue;
            m_audioPacketQueue = rhs.m_audioPacketQueue;
            m_videoFrameQueue = rhs.m_videoFrameQueue;
            m_audioFrameQueue = rhs.m_audioFrameQueue;
            m_streamName = rhs.m_streamName;
            m_audioCnt = rhs.m_audioCnt;
            m_videoCnt = rhs.m_videoCnt;
        }

        SubscribeContext(const SubscribeContext &rhs)
        {
            if (this == &rhs)
            {
                return;
            }
            copy(rhs);
        }

        SubscribeContext &operator=(const SubscribeContext &rhs)
        {
            if (this == &rhs)
            {
                return *this;
            }
            copy(rhs);
            return *this;
        }

        Queue<MediaFrame> *getVideoFrameQueue()
        {
            return m_videoFrameQueue.get();
        }

        Queue<MediaFrame> *getAudioFrameQueue()
        {
            return m_audioFrameQueue.get();
        }

        void pushVideoPacket(MediaPacket &packet);
        void pushVideoFrame(MediaFrame &frame);
        void pushAudioPacket(MediaPacket &packet);
        void pushAudioFrame(MediaFrame &frame);

    public:
        packetQueuePtr m_videoPacketQueue;
        packetQueuePtr m_audioPacketQueue;
        frameQueuePtr m_videoFrameQueue;
        frameQueuePtr m_audioFrameQueue;

        std::string m_streamName;
        uint32_t m_audioCnt;
        uint32_t m_videoCnt;
    };

} // namespace hercules
