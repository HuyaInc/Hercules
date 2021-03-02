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

#include "AudioResampler.h"
#include "Log.h"

#include <exception>
#include <utility>
#include <string>

using std::exception;

namespace hercules
{

AudioResampler::AudioResampler():
    m_context(nullptr),
    m_audioFifo(nullptr),
    m_inputSamples(0),
    m_nbSamples(1024),  // aac
    m_shouldResample(false),
    m_inited(false),
    m_useResampleCount(0),
    m_resampleCount(0)
{
}

bool AudioResampler::init(
    AVSampleFormat iSampleFmt, int32_t iSampleRate, int32_t iChannelNum,
    AVSampleFormat oSampleFmt, int32_t oSampleRate, int32_t oChannelNum)
{
    if(iSampleFmt == oSampleFmt &&
        iSampleRate == oSampleRate &&
        iChannelNum == oChannelNum)
    {
        logInfo(MIXLOG << traceInfo() << "sample sample_fmt sample_rate channels, no need resample");
        m_shouldResample == false;
        return true;
    }
    m_shouldResample = true;

    m_context = swr_alloc();
    if(m_context == nullptr)
    {
        return false;
    }

    m_iSampleFmt = iSampleFmt;
    m_iSampleRate = iSampleRate;
    m_iChannelNum = iChannelNum;
    m_oSampleFmt = oSampleFmt;
    m_oSampleRate = oSampleRate;
    m_oChannelNum = oChannelNum;

    int64_t input_channel_layout = (m_iChannelNum == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    int64_t output_channel_layout = (m_oChannelNum == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

    av_opt_set_int(m_context, "in_channel_layout", input_channel_layout, 0);
    av_opt_set_int(m_context, "in_sample_rate", m_iSampleRate, 0);
    av_opt_set_sample_fmt(m_context, "in_sample_fmt", m_iSampleFmt, 0);

    av_opt_set_int(m_context, "out_channel_layout", output_channel_layout, 0);
    av_opt_set_int(m_context, "out_sample_rate", m_oSampleRate, 0);
    av_opt_set_sample_fmt(m_context, "out_sample_fmt", m_oSampleFmt, 0);

    int32_t error = swr_init(m_context);
    if(error < 0)
    {
        return false;
    }
    if(m_audioFifo == nullptr)
    {
        m_audioFifo = av_audio_fifo_alloc(
            m_oSampleFmt, m_oChannelNum, m_oSampleRate);
    }

    return true;
}

void AudioResampler::destory()
{
    if(m_context != nullptr)
    {
        swr_free(&m_context);
    }
}

int32_t AudioResampler::resample(void** inputBuffer, int32_t nb_samples, void** outputBuffer)
{
    int32_t outputSamples = static_cast<int>(
        av_rescale_rnd(nb_samples, m_oSampleRate, m_iSampleRate, AV_ROUND_UP));
    return swr_convert(
        m_context, reinterpret_cast<uint8_t**>(outputBuffer), outputSamples,
        const_cast<const uint8_t**>(reinterpret_cast<uint8_t**>(inputBuffer)), nb_samples);
}

void AudioResampler::threadEntry()
{
    logInfo(MIXLOG << traceInfo() << " thread start");
    try
    {
        while (!isStop())
        {
            MediaFrame tMediaFrame;
            if (popMediaFrame(tMediaFrame) && tMediaFrame.getAVFrame() != nullptr)
            {
                logDebug(MIXLOG << traceInfo() << "popMediaFrame success");
                if(!m_inited)
                {
                    logInfo(MIXLOG << traceInfo() << " init resampler");
                    AVFrame* avframe = tMediaFrame.getAVFrame();
                    init((AVSampleFormat)avframe->format, avframe->sample_rate, avframe->channels);
                    m_inited = true;
                }

                if(m_inited)
                {
                    int64_t avframe_pts = (int64_t)tMediaFrame.getPts();
                    tMediaFrame.getAVFrame()->pts = avframe_pts;

                    if (doResample(tMediaFrame.getAVFrame()) != 0)
                    {
                        logErr(MIXLOG << traceInfo() << "resample fail!");
                    }
                }
            }

            if(m_inited)
            {
                MediaFrame mediaFrame;
                if(getResampleFrame(mediaFrame))
                {
                    pushMediaFrame(mediaFrame);
                    dispatch(mediaFrame);
                }
            }
        }
    }
    catch (exception &ex)
    {
        logErr(MIXLOG << traceInfo() << "ex: " << ex.what());
    }
    catch (...)
    {
        logErr(MIXLOG << traceInfo() << "ex:unknown");
    }

    logInfo(MIXLOG << traceInfo() << "thread stop: ");
}


int AudioResampler::doResample(AVFrame* srcFrame)
{
    int32_t outputSamples = static_cast<int>(
        av_rescale_rnd(srcFrame->nb_samples, m_oSampleRate, m_iSampleRate, AV_ROUND_UP));
    int outBufSize = outputSamples * av_get_bytes_per_sample(
            static_cast<AVSampleFormat>(srcFrame->format)) * srcFrame->channels;
    uint8_t* outBuf = reinterpret_cast<uint8_t*>(malloc(outBufSize));
    int resampleSize = resample(reinterpret_cast<void**>(srcFrame->data),
            srcFrame->nb_samples, reinterpret_cast<void **>(&outBuf));
    if(resampleSize <  0)
    {
        return -1;
    }
    logDebug(MIXLOG << traceInfo() << "resampleSize:" << resampleSize);
    av_audio_fifo_write(m_audioFifo, reinterpret_cast<void**>(&outBuf), resampleSize);
    // TODO limit
    m_samples2Dts[m_resampleCount] = srcFrame->pkt_dts;
    m_resampleCount += resampleSize;
    return 0;
}

bool AudioResampler::getResampleFrame(MediaFrame& mediaFrame)
{
    AVFrame* dstFrame = av_frame_alloc();
    // TODO flush encoder
    if(av_audio_fifo_size(m_audioFifo) > m_nbSamples)
    {
        logDebug(MIXLOG << traceInfo() << "fifo size:" << av_audio_fifo_size(m_audioFifo));
        dstFrame->nb_samples = m_nbSamples;
        dstFrame->channels = m_oChannelNum;
        dstFrame->channel_layout = av_get_default_channel_layout(dstFrame->channels);
        dstFrame->format = m_oSampleFmt;
        dstFrame->sample_rate = m_oSampleRate;
        if(m_samples2Dts.empty())
        {
            dstFrame->pkt_dts = ((m_resampleCount * 1000) / m_oSampleRate);
        }
        else
        {
            std::map<uint64_t, uint64_t>::iterator it = m_samples2Dts.lower_bound(m_useResampleCount);
            if(it == m_samples2Dts.end())
            {
                auto rit = m_samples2Dts.rbegin();
                int diff = m_useResampleCount - rit->first;
                int diffMs = (diff * 1000) / m_oSampleRate;
                dstFrame->pkt_dts = rit->second + diffMs;
                if(dstFrame->pkt_dts < 0)
                {
                    dstFrame->pkt_dts = 0;
                }
            }
            else
            {
                int diff = m_useResampleCount - it->first;
                int diffMs = (diff * 1000) / m_oSampleRate;
                dstFrame->pkt_dts = it->second + diffMs;
                if(dstFrame->pkt_dts < 0)
                {
                    dstFrame->pkt_dts = 0;
                }
            }
        }
        dstFrame->pkt_pts = dstFrame->pkt_dts;
        dstFrame->pts = dstFrame->pkt_dts;
        
        m_useResampleCount += m_nbSamples;

        if(av_frame_get_buffer(dstFrame, 0) < 0)
        {
            return false;
        }

        int inputSize = av_audio_fifo_read(m_audioFifo,
            reinterpret_cast<void**>(dstFrame->data), m_nbSamples);
        logDebug(MIXLOG << traceInfo() << "success");
    }
    else
    {
        return false;
    }
    mediaFrame.asAudio();
    mediaFrame.asIFrame();
    int64_t pts = dstFrame->pkt_pts > 0 ? dstFrame->pkt_pts : 0;
    int64_t dts = dstFrame->pkt_dts > 0 ? dstFrame->pkt_dts : 0;

    logDebug(MIXLOG << traceInfo() << "resample success, resample dts: " << dts << ", pts: " << pts);

    mediaFrame.setDts(dts);
    mediaFrame.setPts(pts);
    mediaFrame.asAAC();
    mediaFrame.setAVFrame(dstFrame);
    return true;
}

bool AudioResampler::popMediaFrame(MediaFrame &frame)
{
    return getAudioFrameQueue() != nullptr ?
        getAudioFrameQueue()->pop(frame, DEFAULT_QUEUE_TIMEOUT_MS) : false;
}

void AudioResampler::pushMediaFrame(MediaFrame &frame)
{
    getOutFrameQueue().push(frame.getDts(), frame);
}

void AudioResampler::addSubscriber(const std::string &subscriberName, SubscribeContext *context)
{
    auto itr = m_subscriberMap.find(subscriberName);
    if (itr == m_subscriberMap.end())
    {
        logInfo(MIXLOG << traceInfo() << "subscriber name: " << subscriberName << " insert");
        m_subscriberMap.insert(make_pair(subscriberName, context));
    }
    else
    {
        logInfo(MIXLOG << traceInfo() << "subscriber name: " << subscriberName << " replace");
        itr->second = context;
    }
}

void AudioResampler::delSubscriber(const std::string &subscriberName)
{
    auto itr = m_subscriberMap.find(subscriberName);
    if (m_subscriberMap.end() != itr)
    {
        logInfo(MIXLOG << traceInfo() << "subscriber name: " << subscriberName << " erase");
        m_subscriberMap.erase(itr);
    }
    else
    {
        logInfo(MIXLOG << traceInfo() << "subscriber name: " << subscriberName << " no found");
    }
}

void AudioResampler::dispatch(MediaFrame &frame)
{
    for (const auto &subscriber : m_subscriberMap)
    {
        logDebug(MIXLOG << traceInfo());
        auto context = subscriber.second;
        context->pushAudioFrame(frame);
    }
}

}  // namespace hercules
