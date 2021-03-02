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

#include "SubscribeContext.h"
#include "MediaFrame.h"
#include "MediaPacket.h"
#include "JobManager.h"
#include "Log.h"

#include <memory>
#include <string>

namespace hercules
{

    using std::make_pair;
    using std::make_shared;
    using std::string;

    void SubscribeContext::pushVideoPacket(MediaPacket &packet)
    {
        if (m_videoPacketQueue)
        {
            m_videoPacketQueue->push(packet.getDts(), packet);
        }
    }
    void SubscribeContext::pushVideoFrame(MediaFrame &frame)
    {
        if (m_videoFrameQueue)
        {
            m_videoFrameQueue->push(frame.getDts(), frame);
        }
    }

    void SubscribeContext::pushAudioPacket(MediaPacket &packet)
    {

    }

    void SubscribeContext::pushAudioFrame(MediaFrame &frame)
    {
        ++m_audioCnt;
        logDebug(MIXLOG << "streamName: " << m_streamName << ", audioCnt: "
                      << m_audioCnt << ", " << std::hex << this);
        if (m_audioFrameQueue)
        {
            logDebug(MIXLOG);
            m_audioFrameQueue->push(frame.getDts(), frame);
        }
    }

    void SubscribeContext::subscribeVideoPacket()
    {
        if (m_videoPacketQueue)
        {
            return;
        }
        m_videoPacketQueue = make_shared<Queue<MediaPacket>>();
    }

    void SubscribeContext::subscribeJobFrame(const std::string &jobId, const std::string &streamName)
    {
        m_streamName = streamName;
        logInfo(MIXLOG << "streamName: " << m_streamName << ", audioCnt:"
                      << m_audioCnt << ", " << std::hex << this);
        JobManager::getInstance()->subscribeJobFrame(jobId, streamName, this);
    }

    void SubscribeContext::subscribeAudioPacket()
    {
        if (m_audioPacketQueue)
        {
            return;
        }
        m_audioPacketQueue = make_shared<Queue<MediaPacket>>();
    }

    void SubscribeContext::subscribeVideoFrame()
    {
        if (m_videoFrameQueue)
        {
            return;
        }
        m_videoFrameQueue = make_shared<Queue<MediaFrame>>();
    }

    void SubscribeContext::subscribeAudioFrame()
    {
        if (m_audioFrameQueue)
        {
            return;
        }
        m_audioFrameQueue = make_shared<Queue<MediaFrame>>();
    }

} // namespace hercules
