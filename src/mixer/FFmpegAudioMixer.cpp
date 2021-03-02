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

#include "FFmpegAudioMixer.h"
#include "MediaFrame.h"
#include "Log.h"
#include "ByteUtil.h"
#include "Common.h"

#include <arpa/inet.h>

#include <string>

#define MAX_SHORT_VALUE (32767)
#define MIN_SHORT_VALUE (-32768)

#define CLIP_16BITS(tmp)                \
    {                                   \
        if (tmp > MAX_SHORT_VALUE)      \
            tmp = MAX_SHORT_VALUE;      \
        else if (tmp < MIN_SHORT_VALUE) \
            tmp = MIN_SHORT_VALUE;      \
    }

namespace hercules
{

    void FFmpegAudioMixer::initCodec()
    {
        m_audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        m_audioCodecCtx = avcodec_alloc_context3(m_audioCodec);

        m_audioCodecCtx->bit_rate = DEFAULT_AUDIO_BPS;
        m_audioCodecCtx->sample_rate = DEFAULT_SAMPLE_RATE;
        m_audioCodecCtx->channels = DEFAULT_CHANNEL_NUM;
        m_audioCodecCtx->sample_fmt = m_audioCodec->sample_fmts[0];
        m_audioCodecCtx->channel_layout = av_get_default_channel_layout(m_audioCodecCtx->channels);
        m_audioCodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

        /* open it */
        if (avcodec_open2(m_audioCodecCtx, m_audioCodec, NULL) < 0)
        {
            logErr(MIXLOG << "Could not open audio codec");
            return;
        }
        logInfo(MIXLOG << ", audio enc frame size: " << m_audioCodecCtx->frame_size
            << ", bitrate: " << m_audioCodecCtx->bit_rate 
            << ", samplerate: " << m_audioCodecCtx->sample_rate);

        /* frame containing input raw audio */
        m_audioEncFrame = av_frame_alloc();
        if (!m_audioEncFrame)
        {
            logErr(MIXLOG << "Could not allocate audio frame");
            return;
        }

        m_audioEncFrame->nb_samples = m_audioCodecCtx->frame_size;
        m_audioEncFrame->format = m_audioCodecCtx->sample_fmt;
        m_audioEncFrame->channel_layout = m_audioCodecCtx->channel_layout;
        m_audioEncFrame->sample_rate = m_audioCodecCtx->sample_rate;

        int bufferSize = av_samples_get_buffer_size(NULL, m_audioCodecCtx->channels,
            m_audioCodecCtx->frame_size, m_audioCodecCtx->sample_fmt, 0);

        m_audioSamples = reinterpret_cast<uint8_t *>(av_malloc(bufferSize));

        avcodec_fill_audio_frame(m_audioEncFrame, m_audioCodecCtx->channels,
            m_audioCodecCtx->sample_fmt, reinterpret_cast<const uint8_t *>(m_audioSamples), 
            bufferSize, 0);

        logInfo(MIXLOG 
            << "ouput channels: " << av_get_default_channel_layout(m_audioCodecCtx->channels) 
            << ", fmt: " << m_audioCodecCtx->sample_fmt 
            << ", rate: " << m_audioCodecCtx->sample_rate);
    }

    void FFmpegAudioMixer::mix(std::string &dst, std::string &src)
    {
        if (dst.size() != src.size())
        {
            logErr(MIXLOG << "mix audio fail"
                << ", dst frame size: " << dst.size() 
                << ", src frame size: " << src.size());
            return;
        }
        uint32_t sampleCount = dst.size() / sizeof(int16_t);
        int16_t *pDst = const_cast<int16_t *>(reinterpret_cast<const int16_t *>(dst.c_str()));
        int16_t *pSrc = const_cast<int16_t *>(reinterpret_cast<const int16_t *>(src.c_str()));
        int tmp;
        for (uint32_t i = 0; i < sampleCount; i++)
        {
            tmp = pDst[i] + pSrc[i];
            CLIP_16BITS(tmp);
            pDst[i] = tmp;
        }
    }

    void FFmpegAudioMixer::mixFrame(MediaFrame &src, MediaFrame &dst)
    {
        AVFrame *frameSrc = src.getAVFrame();
        AVFrame *frameDst = dst.getAVFrame();
        if(frameSrc == nullptr || frameDst == nullptr)
        {
            return;
        }
        if (frameSrc->nb_samples != frameDst->nb_samples)
        {
            logErr(MIXLOG << "error: need resample");
            return;
        }
        if(frameSrc->format != frameDst->format)
        {
            logErr(MIXLOG << "error: AVSampleFormat diff");
            return;
        }
        // TODO (resample fix)
        if(frameSrc->channels != frameDst->channels)
        {
            logErr(MIXLOG << "error: channels diff");
            return;
        }
        // TODO adaptive 
        if(static_cast<AVSampleFormat>(frameSrc->format) == AV_SAMPLE_FMT_FLTP)
        {
            uint32_t sampleCount = frameDst->linesize[0] / sizeof(float);
            float *pDst = reinterpret_cast<float *>(frameDst->extended_data[0]);
            float *pSrc = reinterpret_cast<float *>(frameSrc->extended_data[0]);
            for (uint32_t i = 0; i < sampleCount; ++i)
            {
                pDst[i] += pSrc[i];
            }
            pDst = reinterpret_cast<float *>(frameDst->extended_data[1]);
            pSrc = reinterpret_cast<float *>(frameSrc->extended_data[1]);
            for (uint32_t i = 0; i < sampleCount; ++i)
            {
                pDst[i] += pSrc[i];
            }
        }
        else if(static_cast<AVSampleFormat>(frameSrc->format) == AV_SAMPLE_FMT_S16) 
        {
            uint32_t sampleCount = frameDst->linesize[0] / sizeof(int16_t);
            int16_t *pDst = reinterpret_cast<int16_t *>(frameDst->extended_data[0]);
            int16_t *pSrc = reinterpret_cast<int16_t *>(frameSrc->extended_data[0]);
            for (uint32_t i = 0; i < sampleCount; ++i)
            {
                pDst[i] += pSrc[i];
            }
        }
        else
        {
            logErr(MIXLOG << "invalid AVSampleFormat:" << frameSrc->format);
        }
    }

} // namespace hercules
