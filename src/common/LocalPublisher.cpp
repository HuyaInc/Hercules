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

#include "LocalPublisher.h"
#include "MediaPacket.h"
#include "Common.h"
#include "MediaPacket.h"
#include "MediaFrame.h"
#include "Log.h"
#include "MixSdk.h"
#include "JobManager.h"
#include "Job.h"

#include <string>

namespace hercules
{

    using std::string;

    int LocalPublisher::connect()
    {
    }

    LocalPublisher::~LocalPublisher()
    {
    }

    void LocalPublisher::threadEntry()
    {
        logInfo(MIXLOG << "local publisher thread start, stream name:" << getStreamName());
        while (!isStop())
        {
            sendVideo();
            sendAudio();
        }
        logInfo(MIXLOG << "local publisher thread stop, stream name:" << getStreamName());
    }

    int LocalPublisher::sendAudio()
    {
        MediaPacket packet;

        if (!getAudioPacket(packet))
        {
            return 0;
        }

        AVData data;
        MediaPacket::mediaPacketToFlvWithHeader(packet, data.m_data);
        data.m_pts = packet.getPts();
        data.m_dts = packet.getDts();
        data.m_dataType = DataType::DATA_TYPE_FLV_AUDIO;
        string sNewHex = dump(data.m_data.c_str(), data.m_data.size());
        Job *job = JobManager::getInstance()->findJob(m_taskId);
        if (job != nullptr)
        {
            job->sendData(data);
        }

        return 0;
    }

    int LocalPublisher::sendVideo()
    {
        MediaPacket packet;

        if (!getVideoPacket(packet))
        {
            return 0;
        }

        logDebug(MIXLOG << "local publisher send video");
        AVData data;
        MediaPacket::mediaPacketToFlvWithHeader(packet, data.m_data);
        data.m_pts = packet.getPts();
        data.m_dts = packet.getDts();
        data.m_dataType = DataType::DATA_TYPE_FLV_VIDEO;
        Job *job = JobManager::getInstance()->findJob(m_taskId);
        if (job != nullptr)
        {
            job->sendData(data);
        }
        return 0;
    }

    bool LocalPublisher::getVideoPacket(MediaPacket &tMediaPacket)
    {
        return getVideoQueue() ? getVideoQueue()->pop(tMediaPacket, 5) : false;
    }

    void LocalPublisher::pushAudioPacket(const MediaPacket &packet)
    {
        if (getAudioQueue())
        {
            getAudioQueue()->push(packet.getDts(), packet);
        }
    }

    bool LocalPublisher::getAudioPacket(MediaPacket &tMediaPacket)
    {
        return getAudioQueue() ? getAudioQueue()->pop(tMediaPacket, 5) : false;
    }

} // namespace hercules
