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

#include "AudioEncoder.h"
#include "Common.h"
#include "OpenCVOperator.h"
#include "MediaPacket.h"
#include "MediaFrame.h"
#include "Util.h"
#include "Log.h"
#include "Decoder.h"
#include "Job.h"
#include "JobManager.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avc.h"
#include "x264/x264.h"
}

#include <utility>
#include <string>

using std::exception;
using std::make_pair;
using std::string;

namespace hercules
{

    constexpr int AUDIO_ENCODE_FRAME_LOG_INTERVAL = 1000;

#define ADD_X264OPTS(x264opts, param) \
    {                                 \
        if (strlen(x264opts) > 0)     \
        {                             \
            strcat(x264opts, ":");    \
        }                             \
        strcat(x264opts, param);      \
    }

    AudioEncoder::AudioEncoder()
        : OneCycleThread()
        , Property()
        , m_inputVideoFrameQueue(nullptr)
        , m_outputVideoPacketQueue(nullptr)
        , m_encodeCtx(nullptr)
        , m_ready(false)
        , m_codec(DEFAULT_CHANNEL_NUM, DEFAULT_SAMPLE_RATE, DEFAULT_AUDIO_BPS, CodecType::AAC)
        , m_audioHeader()
        , m_numOfEncodedFrame(0)
        , m_lastSendDts(0)
        , m_lastSendPts(0)
        , m_lowLatency(false)
    {
        logInfo(MIXLOG);
    }

    AudioEncoder::~AudioEncoder()
    {
        logInfo(MIXLOG << traceInfo());
        reset();
    }

    int AudioEncoder::init(const string &name, Queue<MediaFrame> &in_queue,
                            Queue<MediaPacket> &out_queue)
    {
        Property::setStreamName(name);

        m_inputVideoFrameQueue = &in_queue;
        m_outputVideoPacketQueue = &out_queue;

        logInfo(MIXLOG << traceInfo());

        return 0;
    }

    void AudioEncoder::reset()
    {
        if (m_encodeCtx != nullptr)
        {
            avcodec_free_context(&m_encodeCtx);
            m_encodeCtx = nullptr;
        }
    }

    bool AudioEncoder::popMediaFrame(MediaFrame &tMediaFrame)
    {
        return getInVideoQueue() ?
            getInVideoQueue()->pop(tMediaFrame, DEFAULT_QUEUE_TIMEOUT_MS) : false;
    }

    void AudioEncoder::pushMediaFrame(MediaFrame &tMediaFrame)
    {
        getInVideoQueue()->push(tMediaFrame.getDts(), tMediaFrame);
    }

    bool AudioEncoder::popMediaPacket(MediaPacket &tMediaPacket)
    {
        return getOutVideoQueue() ?
            getOutVideoQueue()->pop(tMediaPacket, DEFAULT_QUEUE_TIMEOUT_MS) : false;
    }

    void AudioEncoder::pushMediaPacket(MediaPacket &tMediaPacket)
    {
        if (getOutVideoQueue())
        {
            getOutVideoQueue()->push(tMediaPacket.getDts(), tMediaPacket);
        }
    }

    void AudioEncoder::setCodec(int channels, int sampleRate, int kbps, const string &codec)
    {
        std::unique_lock<std::mutex> lockGuard(m_mutex);

        m_codec = getAudioCodec(channels, sampleRate, kbps, codec);

        logInfo(MIXLOG << traceInfo() << ", codec: " << m_codec.print());
        m_ready = false;
    }

    void AudioEncoder::setBitrate(int bitrate)
    {
        m_codec.m_kbps = bitrate;
    }

    void AudioEncoder::threadEntry()
    {
        logInfo(MIXLOG << traceInfo() << " thread start");

        try
        {
            while (!isStop())
            {
                MediaFrame tMediaFrame;
                if (!popMediaFrame(tMediaFrame) || tMediaFrame.getAVFrame() == nullptr)
                {
                    continue;
                }

                tMediaFrame.setCodecType(m_codec.m_codecType);

                if (isStop())
                {
                    break;
                }

                {
                    std::unique_lock<std::mutex> lockGuard(m_mutex);
                    if (!isReady())
                    {
                        if (setupEncoder() == 0)
                        {
                            setReady();
                        }
                    }
                }

                int64_t avframe_pts = (int64_t)tMediaFrame.getPts();
                tMediaFrame.getAVFrame()->pts = avframe_pts;

                insertFrameMetadata(avframe_pts, tMediaFrame);

                AVPacket *avpacket = av_packet_alloc();
                if (doEncode(tMediaFrame.getAVFrame(), avpacket) != 0)
                {
                    logErr(MIXLOG << traceInfo() << " encode fail!");
                    av_packet_free(&avpacket);
                    continue;
                }
                else if (tMediaFrame.getFrameId() % AUDIO_ENCODE_FRAME_LOG_INTERVAL == 0)
                {
                    logDebug(MIXLOG << traceInfo() << " encode success!");
                }

                MediaPacket tMediaPacket;
                tMediaPacket.asAudio();
                tMediaPacket.asIFrame();
                int64_t pts = avpacket->pts > 0 ? avpacket->pts : 0;
                int64_t dts = avpacket->dts > 0 ? avpacket->dts : 0;

                logDebug(MIXLOG << "streamname: " << m_streamName 
                    << ", avframe pts: " << avframe_pts 
                    << ", encoded dts: " << dts << ", pts: " << pts);

                tMediaPacket.setDts(dts);
                tMediaPacket.setPts(pts);
                getFrameMetadata(pts, tMediaPacket);
                tMediaPacket.addAllIdTimeTrace(TimeTraceKey::ENCODE, getNowMs32());
                m_lastSendDts = dts;
                m_lastSendPts = pts;

                uint8_t *side_data = nullptr;
                int side_data_size = 0;

                logDebug(MIXLOG << traceInfo() << tMediaFrame.print() << ", encode success"
                    << ", encode num: " << m_numOfEncodedFrame 
                    << ", pts: " << pts << ", dts: " << dts 
                    << ", size: " << avpacket->size 
                    << ", input video queue size: " << getInVideoQueue()->size() 
                    << ", ouput video queue size: " << getOutVideoQueue()->size());

                tMediaPacket.asAAC();
                tMediaPacket.setAVPacket(avpacket);
                pushMediaPacket(tMediaPacket);

                ++m_numOfEncodedFrame;

                m_encodeFpsStat.incr(1);
            }
        }
        catch (exception &ex)
        {
            logErr(MIXLOG << traceInfo() << ", ex: " << ex.what());
        }
        catch (...)
        {
            logErr(MIXLOG << traceInfo() << ", ex:unknown");
        }

        logInfo(MIXLOG << "thread stop: " << traceInfo());
    }

    int AudioEncoder::setupEncoder()
    {
        reset();

        string encoder_name = "libfdk_aac";
        AVCodec *avcodec = avcodec_find_encoder_by_name(encoder_name.c_str());

        if (avcodec == nullptr)
        {
            logErr(MIXLOG << traceInfo() << ", can't find avcodec: " << encoder_name);
            return -1;
        }

        m_encodeCtx = avcodec_alloc_context3(avcodec);
        return_if(!m_encodeCtx, "allocate encode context fail");

        m_encodeCtx->codec_id = AV_CODEC_ID_AAC;
        m_encodeCtx->codec_type = AVMEDIA_TYPE_AUDIO;

        m_encodeCtx->bit_rate = m_codec.m_kbps * 1000;
        m_encodeCtx->sample_fmt = AV_SAMPLE_FMT_S16;
        m_encodeCtx->sample_rate = m_codec.m_sampleRate;
        m_encodeCtx->channel_layout = AV_CH_LAYOUT_STEREO;
        m_encodeCtx->channels = m_codec.m_channels;
        m_encodeCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        logInfo(MIXLOG << traceInfo() 
            << ", channels: " << m_encodeCtx->channels 
            << ", sampleRate: " << m_encodeCtx->sample_rate 
            << ", kbps: " << m_encodeCtx->bit_rate);

        int ret = avcodec_open2(m_encodeCtx, m_encodeCtx->codec, nullptr);
        if (ret < 0)
        {
            reset();
            logErr(MIXLOG << traceInfo() << ", open encode codec fail, ret:" << ret);
            return -1;
        }

        AVIOContext *pb;
        uint8_t *P = nullptr;
        ret = avio_open_dyn_buf(&pb);
        if (ret == 0)
        {
            logInfo(MIXLOG << traceInfo() << ", header:" 
                << bin2str(m_encodeCtx->extradata, m_encodeCtx->extradata_size, " "));
            avio_write(pb, m_encodeCtx->extradata, m_encodeCtx->extradata_size);
            m_audioHeader.m_len = avio_close_dyn_buf(pb, &P);
            memcpy(m_audioHeader.m_config, P, m_audioHeader.m_len);
            av_freep(&P);
        }

        if (m_audioHeader.m_len == 0)
        {
            logErr(MIXLOG << traceInfo() 
                << ", setup audio encoder get aac config fail");
            return -1;
        }

        string sNewHex = dump(m_audioHeader.m_config, m_audioHeader.m_len);
        logInfo(MIXLOG << traceInfo() << ", dump new aac header: "
            << sNewHex << ", header len: " << m_audioHeader.m_len);

        MediaPacket tMediaPacket;
        tMediaPacket.asHeaderFrame();
        tMediaPacket.setDts(0);
        tMediaPacket.setPts(0);
        tMediaPacket.asAudio();
        tMediaPacket.asAAC();

        AVPacket *avPacket = av_packet_alloc();
        size_t avpacket_buf_size = m_audioHeader.m_len;
        uint8_t *avpacket_buf = reinterpret_cast<uint8_t *>(av_malloc(avpacket_buf_size));
        memcpy(avpacket_buf, m_audioHeader.m_config, m_audioHeader.m_len);
        av_packet_from_data(avPacket, avpacket_buf, avpacket_buf_size);
        tMediaPacket.setAVPacket(avPacket);
        pushMediaPacket(tMediaPacket);

        m_numOfEncodedFrame = 0;

        return 0;
    }

    int AudioEncoder::doEncode(AVFrame *tFrame, AVPacket *tPacket)
    {
        if (m_encodeCtx == nullptr)
        {
            return -1;
        }

        int got_frame = 0;
        int ret = avcodec_encode_audio2(m_encodeCtx, tPacket, tFrame, &got_frame);

        if (ret < 0)
        {
            logErr(MIXLOG << traceInfo() << ", encode failed ret: " << ret);
            ret = -1;
        }

        // TODO maybe eagin
        if (!got_frame)
        {
            logErr(MIXLOG << traceInfo() << ", no got frame");
            ret = -1;
        }

        return ret;
    }

    void AudioEncoder::insertFrameMetadata(int64_t key, const MediaFrame &tMediaFrame)
    {
        FrameMetadata frameMetadata(tMediaFrame.getIdTimestamp(), tMediaFrame.getIdTimeTrace());
        m_frameMetadata.insert(make_pair(key, frameMetadata));

        if (m_frameMetadata.size() >= MAX_FRAME_METADATA_SIZE)
        {
            m_frameMetadata.erase(m_frameMetadata.begin());
        }
    }

    bool AudioEncoder::getFrameMetadata(int64_t key, MediaPacket &tMediaPacket)
    {
        auto iter = m_frameMetadata.find(key);

        if (iter == m_frameMetadata.end())
        {
            return false;
        }

        tMediaPacket.setIdTimeTrace(iter->second.m_idTimeTrace);
        tMediaPacket.setIdTimestamp(iter->second.m_idTimestamp);

        m_frameMetadata.erase(m_frameMetadata.begin(), iter);

        return true;
    }

} // namespace hercules
