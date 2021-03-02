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

#include "RtmpPublisher.h"
#include "MediaPacket.h"
#include "Common.h"
#include "MediaPacket.h"
#include "MediaFrame.h"
#include "Log.h"

#include <string>

namespace hercules
{

    using std::make_pair;
    using std::string;

    constexpr int FLV_AUDIO_TYPE = 8;
    constexpr int FLV_VIDEO_TYPE = 9;
    constexpr int QUEUE_TIMEOUT_MS = 5;

    int RtmpPublisher::connect()
    {
        if (m_srsRtmpContext)
        {
            srs_rtmp_destroy(m_srsRtmpContext);
            m_srsRtmpContext = nullptr;
        }

        logInfo(MIXLOG << "rtmp url: " << m_url);

        m_srsRtmpContext = srs_rtmp_create(m_url.c_str());

        int ret = 0;

        // RTMP C0/C1/C2
        if ((ret = srs_rtmp_handshake(m_srsRtmpContext)) != 0)
        {
            logErr(MIXLOG << "error, handshake failed");
            goto destory;
        }

        // NetStream.Connect
        if ((ret = srs_rtmp_connect_app(m_srsRtmpContext)) != 0)
        {
            logErr(MIXLOG << "error, connect failed");
            goto destory;
        }

        // NetStream.Publish
        if ((ret = srs_rtmp_publish_stream(m_srsRtmpContext)) != 0)
        {
            logErr(MIXLOG << "error, publish failed");
            goto destory;
        }

        logInfo(MIXLOG << "connect stream success");
        return 0;

    destory:

        if (m_srsRtmpContext)
        {
            srs_rtmp_destroy(m_srsRtmpContext);
            m_srsRtmpContext = nullptr;
        }
        return -1;
    }

    RtmpPublisher::~RtmpPublisher()
    {
        if (m_srsRtmpContext)
        {
            srs_rtmp_destroy(m_srsRtmpContext);
            m_srsRtmpContext = nullptr;
        }

        logInfo(MIXLOG << "~RtmpPublisher");
    }

    void RtmpPublisher::threadEntry()
    {
        logInfo(MIXLOG << "RtmpPublisher thread start, streamName: " << getStreamName());
        while (!m_stop)
        {
            while (m_srsRtmpContext == nullptr)
            {
                connect();
                continue;
            }

            int ret = sendVideo();

            if (ret != 0)
            {
                connect();
            }

            ret = sendAudio();

            if (ret != 0)
            {
                connect();
            }
        }

        logInfo(MIXLOG << "RtmpPublisher thread exit, streamName: " << getStreamName());
    }

    int RtmpPublisher::sendAudio()
    {
        MediaPacket packet;

        if (!getAudioPacket(packet))
        {
            return 0;
        }

        string flv;
        MediaPacket::mediaPacketToFlvWithoutHeader(packet, flv);

        char *rtmpPacket = reinterpret_cast<char *>(malloc(flv.size()));
        memcpy(rtmpPacket, flv.data(), flv.size());
        int ret = srs_rtmp_write_packet(
            m_srsRtmpContext, FLV_AUDIO_TYPE, packet.getDts(), rtmpPacket, flv.size());

        if (ret != 0)
        {
            logErr(MIXLOG << "error, send rtmp packet failed");
        }
        else
        {
            logDebug(MIXLOG << "send audio success, " << packet.print());
        }

        return ret;
    }

    int RtmpPublisher::sendVideo()
    {
        MediaPacket packet;

        if (!getVideoPacket(packet))
        {
            return 0;
        }

        string flv;
        MediaPacket::mediaPacketToFlvWithoutHeader(packet, flv);

        char *rtmpPacket = reinterpret_cast<char *>(malloc(flv.size()));
        memcpy(rtmpPacket, flv.data(), flv.size());
        int ret = srs_rtmp_write_packet(
            m_srsRtmpContext, FLV_VIDEO_TYPE, packet.getDts(), rtmpPacket, flv.size());

        if (ret != 0)
        {
            logErr(MIXLOG << "error, send rtmp packet failed");
        }
        else
        {
            logDebug(MIXLOG << "send video success, " << packet.print());
        }

        return ret;
    }

    bool RtmpPublisher::getVideoPacket(MediaPacket &tMediaPacket)
    {
        return getVideoQueue() ? getVideoQueue()->pop(tMediaPacket, QUEUE_TIMEOUT_MS) : false;
    }

    void RtmpPublisher::pushAudioPacket(const MediaPacket &packet)
    {
        if (getAudioQueue())
        {
            getAudioQueue()->push(packet.getDts(), packet);
        }
    }

    bool RtmpPublisher::getAudioPacket(MediaPacket &tMediaPacket)
    {
        return getAudioQueue() ? getAudioQueue()->pop(tMediaPacket, QUEUE_TIMEOUT_MS) : false;
    }

} // namespace hercules
