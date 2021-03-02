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
#include "CycleCounterStat.h"

extern "C"
{
#include "libavutil/opt.h"
#include "libavutil/bswap.h"
#include "libavformat/avformat.h"
}

#include <atomic>
#include <iostream>
#include <string>
#include <map>

struct AVCodec;
struct AVCodecContext;
struct AVPacket;
struct AVFrame;

namespace hercules
{

    class MediaFrame;
    class MediaPacket;
    class Decoder;

    class Encoder : public OneCycleThread, public Property
    {
    public:
        Encoder();
        ~Encoder();

        void start() { OneCycleThread::startThread("encoder"); }
        void stop() { OneCycleThread::stopThread(); }
        void join() { OneCycleThread::joinThread(); }

        int init(const std::string &name, Queue<MediaFrame> &in_queue, 
            Queue<MediaPacket> &out_queue);

        bool popMediaFrame(MediaFrame &tMediaFrame);
        void pushMediaFrame(MediaFrame &tMediaFrame);
        void pushMediaPacket(MediaPacket &tMediaFrame);
        bool popMediaPacket(MediaPacket &tMediaFrame);

        void setCodec(int width, int height, int fps,
            int kbps, const std::string &codec);
        void setBitrate(int bitrate);
        void setLowLatency(bool lowLatency) { m_lowLatency = lowLatency; }

    private:
        struct FrameMetadata
        {
            FrameMetadata(const std::map<uint64_t, uint32_t> &idTimestamp,
                          const std::map<uint64_t, std::map<TimeTraceKey, uint32_t>> &idTimeTrace)
                : m_idTimestamp(idTimestamp), m_idTimeTrace(idTimeTrace)
            {
            }

            std::map<uint64_t, uint32_t> m_idTimestamp;
            std::map<uint64_t, std::map<TimeTraceKey, uint32_t>> m_idTimeTrace;
        };

        std::map<int64_t, FrameMetadata> m_frameMetadata;

        void insertFrameMetadata(int64_t key, const MediaFrame &tMediaFrame);
        bool getFrameMetadata(int64_t key, MediaPacket &tMediaPacket);

    private:
        Queue<MediaFrame> *getInVideoQueue() { return m_inputVideoFrameQueue; }
        Queue<MediaPacket> *getOutVideoQueue() { return m_outputVideoPacketQueue; }

        bool isReady() { return m_ready; }
        void setReady() { m_ready = true; }

        int doEncode(AVFrame *tFrame, AVPacket *tPacket);
        int setupEncoder();

        void reset();

    protected:
        void threadEntry();

    private:
        Queue<MediaFrame> *m_inputVideoFrameQueue;
        Queue<MediaPacket> *m_outputVideoPacketQueue;

        AVCodecContext *m_encodeCtx;
        bool m_ready;
        VideoCodec m_codec;
        CodecHeader m_videoHeader;
        int m_numOfEncodedFrame;
        std::mutex m_mutex;

        CycleCounterStat<1000> m_encodeFpsStat;
        uint32_t m_lastSendDts;
        uint32_t m_lastSendPts;

        bool m_lowLatency;

        Decoder *m_copyDecoder;
    };

} // namespace hercules
