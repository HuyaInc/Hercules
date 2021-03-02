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

#include "CodecUtil.h"
#include "OneCycleThread.h"
#include "Property.h"
#include "Queue.h"
#include "Queue.h"
#include "CycleCounterStat.h"
#include "SubscribeContext.h"

extern "C"
{
#include "libavutil/opt.h"
#include "libavutil/bswap.h"
}

#include <atomic>
#include <iostream>
#include <map>
#include <string>

struct AVCodecContext;
struct AVPacket;
struct AVFrame;

namespace hercules
{

    class MediaPacket;
    class MediaFrame;

    class AudioDecoder : public OneCycleThread, public Property
    {
    public:
        AudioDecoder();
        ~AudioDecoder();

        void start() { OneCycleThread::startThread("audio decode"); }
        void stop() { OneCycleThread::stopThread(); }
        void join() { OneCycleThread::joinThread(); }

        int init(const std::string &name, Queue<MediaPacket> *);

        void reset();
        int getNum() { return m_numOfDecoded; }

        int doDecode(MediaPacket &tMediaPacket, MediaFrame &tMediaFrame);

        void flushDecoder();

        bool popMediaFrame(MediaFrame &tFrame);
        int popMediaFrameLowerBound(uint64_t timestamp, MediaFrame &tFrame);
        void pushMediaFrame(MediaFrame &tMediaFrame);
        bool popMediaPacket(MediaPacket &tFrame);

        Queue<MediaPacket> *getInVideoQueue() { return m_inAudioPacketQueue; }
        Queue<MediaFrame> *getOutVideoQueue() { return m_outAudioFrameQueue; }

        void addSubscriber(const std::string &subscriberName, SubscribeContext *context);
        void delSubscriber(const std::string &subscriberName);

    private:
        void threadEntry();
        int setupDecoder(const MediaPacket &tMediaPacket);

        void dispatch(MediaFrame &frame);

    private:
        AVCodecContext *m_decodeCtx;
        int m_numOfDecoded;

        CodecHeader m_audioHeader;
        bool m_ready;

        Queue<MediaPacket> *m_inAudioPacketQueue;
        Queue<MediaFrame> *m_outAudioFrameQueue;

        CycleCounterStat<1000> m_decodeFpsStat;
        std::map<std::string, SubscribeContext *> m_subscriberMap;
    };

} // namespace hercules
