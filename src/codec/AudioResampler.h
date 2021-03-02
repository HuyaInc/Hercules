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

#include "Queue.h"
#include "MediaFrame.h"
#include "OneCycleThread.h"
#include "SubscribeContext.h"

#include <memory>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/audio_fifo.h"
}

#include <map>
#include <string>

namespace hercules
{

class AudioResampler : public OneCycleThread , public SubscribeContext, public Property
{
public:
    AudioResampler();

    ~AudioResampler() {destory();}

    bool init(AVSampleFormat iSampleFmt, int32_t iSampleRate, int32_t iChannelNum,
        AVSampleFormat oSampleFmt = AV_SAMPLE_FMT_S16, int32_t oSampleRate = 44100,
        int32_t oChannelNum = 2);

    void destory();

    void start() { OneCycleThread::startThread("AudioResampler"); }
    void stop() { OneCycleThread::stopThread(); }
    void join() { OneCycleThread::joinThread(); }

    int32_t resample(void** inputBuffer, int32_t nb_samples, void** outputBuffer);

    int doResample(AVFrame* srcFrame);

    bool popMediaFrame(MediaFrame &);
    void pushMediaFrame(MediaFrame &);

    Queue<MediaFrame>& getOutFrameQueue() { return m_outFrameQueue;}

    void addSubscriber(const std::string &subscriberName, SubscribeContext *context);
    void delSubscriber(const std::string &subscriberName);

    bool getResampleFrame(MediaFrame& mediaFrame);

protected:
    void threadEntry();
    void dispatch(MediaFrame &frame);
private:
    SwrContext* m_context;
    AVSampleFormat m_iSampleFmt;
    int32_t m_iSampleRate;
    int32_t m_iChannelNum;
    AVSampleFormat m_oSampleFmt;
    int32_t m_oSampleRate;
    int32_t m_oChannelNum;
    Queue<MediaFrame> m_inFrameQueue;
    Queue<MediaFrame> m_outFrameQueue;
    bool m_inited;

    AVAudioFifo* m_audioFifo;
    uint64_t m_resampleCount;
    uint64_t m_useResampleCount;

    uint32_t m_inputSamples;
    uint32_t m_nbSamples;
    bool m_shouldResample;
    // nb_samples => dts
    std::map<uint64_t, uint64_t> m_samples2Dts;
    std::map<std::string, SubscribeContext *> m_subscriberMap;
};

}  // namespace hercules
