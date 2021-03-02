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

#include "Decoder.h"
#include "OpenCVOperator.h"
#include "Common.h"
#include "MediaPacket.h"
#include "MediaFrame.h"
#include "Log.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "x264/x264.h"
}

#include <utility>
#include <string>

namespace hercules
{

#define ADD_X264OPTS(x264opts, param) \
    {                                 \
        if (strlen(x264opts) > 0)     \
        {                             \
            strcat(x264opts, ":");    \
        }                             \
        strcat(x264opts, param);      \
    }

    using std::exception;
    using std::string;

    Decoder::Decoder() 
        : OneCycleThread()
        , Property()
        , m_decodeCtx(NULL)
        , m_numOfDecoded(0)
        , m_videoHeader()
        , m_ready(false)
        , m_inVideoPacketQueue(NULL)
        , m_outVideoFrameQueue(NULL)
    {
        logInfo(MIXLOG);
    }

    int Decoder::init(const string &name, Queue<MediaPacket> *inVideoQueue)
    {
        Property::setStreamName(name);
        m_inVideoPacketQueue = inVideoQueue;
        m_ready = false;

        av_register_all();
        avcodec_register_all();
        return 0;
    }

    Decoder::~Decoder()
    {
        reset();
        logInfo(MIXLOG << traceInfo());
    }

    void Decoder::reset()
    {
        if (m_decodeCtx != NULL)
        {
            avcodec_free_context(&m_decodeCtx);
            m_decodeCtx = NULL;
        }
    }

    void Decoder::threadEntry()
    {
        logInfo(MIXLOG << "thread start:" << traceInfo());

        try
        {
            while (!isStop())
            {
                MediaPacket tMediaPacket;
                if (!popMediaPacket(tMediaPacket))
                {
                    continue;
                }
                logDebug(MIXLOG << "decoder get video pakcet");

                tMediaPacket.getAVPacket()->pts = tMediaPacket.getPts();
                tMediaPacket.getAVPacket()->dts = tMediaPacket.getDts();

                bool skip = false;
                if (tMediaPacket.isHeaderFrame() || tMediaPacket.hasGlobalHeader())
                {
                    if (m_ready)
                    {
                        auto curHeader = getHeader(tMediaPacket);
                        if (m_videoHeader == curHeader)
                        {
                            logInfo(MIXLOG << traceInfo() << " recv a same avc header, ignore");
                            skip = true;
                            if (tMediaPacket.isHeaderFrame())
                            {
                                continue;
                            }
                        }
                    }

                    if (!skip)
                    {
                        m_ready = false;
                        logInfo(MIXLOG << traceInfo() 
                            << ", reconfig decoder packet:" << tMediaPacket.print() 
                            << ", frameid:" << tMediaPacket.getFrameId());

                        if (setupDecoder(tMediaPacket) == 0)
                        {
                            m_ready = true;

                            uint64_t realUid = tMediaPacket.getStreamId();
                            uint64_t uid = Property::getUid();
                            if (realUid != uid)
                            {
                                Property::setUid(realUid);
                                logInfo(MIXLOG << traceInfo() 
                                    << ", fix uid from: " << uid << " to: " << realUid);
                            }
                        }
                    }

                    if (tMediaPacket.isHeaderFrame())
                    {
                        continue;
                    }
                }

                if (m_ready)
                {
                    MediaFrame tMediaFrame;
                    tMediaFrame.asVideo();
                    tMediaFrame.setCodecType(tMediaPacket.getCodecType());
                    tMediaFrame.setStreamId(tMediaPacket.getStreamId());
                    tMediaFrame.setStreamName(tMediaPacket.getStreamName());
                    tMediaFrame.setIdTimeTrace(tMediaPacket.getIdTimeTrace());
                    tMediaFrame.setFrameId(tMediaPacket.getFrameId());

                    int ret = doDecode(tMediaPacket, tMediaFrame);
                    ++m_numOfDecoded;

                    if (ret != 0)
                    {
                        continue;
                    }

                    if (tMediaPacket.isIFrame())
                    {
                        logInfo(MIXLOG << traceInfo() << " decode success"
                            << ", pts: " << tMediaFrame.getAVFrame()->pkt_pts 
                            << ", dts: " << tMediaFrame.getAVFrame()->pkt_dts 
                            << ", in video queue size: " << getInVideoQueue()->size() 
                            << ", packet: " << tMediaPacket.print()
                            << ", frame: " << tMediaFrame.print());
                    }

                    dispatch(tMediaFrame);
                }
                else
                {
                    logErr(MIXLOG << traceInfo() << " decoder no ready");
                }
            }
        }
        catch (exception &ex)
        {
            logErr(MIXLOG << traceInfo() << "ex: " << ex.what());
        }
        catch (...)
        {
            logErr(MIXLOG << traceInfo() << "ex: unknown");
        }

        logInfo(MIXLOG << "thread stop: " << traceInfo());
    }

    int Decoder::setupDecoder(const MediaPacket &tMediaPacket)
    {
        m_videoHeader = getHeader(tMediaPacket);

        logInfo(MIXLOG << "setup decoder: " << traceInfo());

        reset();

        string decoder_name = "h264";
        if (m_videoHeader.m_codecType == CodecType::H265)
        {
            decoder_name = "hevc";
        }

        AVCodec *avcodec = avcodec_find_decoder_by_name(decoder_name.c_str());

        if (avcodec == NULL)
        {
            logErr(MIXLOG << traceInfo()
                << "can't find avcodec: " << decoder_name);
            return -1;
        }

        m_decodeCtx = avcodec_alloc_context3(avcodec);

        // extradata
        m_decodeCtx->extradata_size = DEFAULT_DECODER_EXTRADATA_SIZE;
        m_decodeCtx->extradata = reinterpret_cast<uint8_t *>(
            av_malloc(m_decodeCtx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE));
        memset(m_decodeCtx->extradata, 0, 
            m_decodeCtx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);

        // set avcDecoder ConfigurationRecord
        memcpy(reinterpret_cast<void *>(m_decodeCtx->extradata),
               m_videoHeader.m_config, m_videoHeader.m_len);
        m_decodeCtx->extradata_size = m_videoHeader.m_len;

        m_decodeCtx->thread_count = DEFAULT_DECODER_THREAD_COUNT;

        int ret = avcodec_open2(m_decodeCtx, m_decodeCtx->codec, NULL);
        if (ret < 0)
        {
            logErr(MIXLOG << traceInfo() << "open codec failed");
            return ret;
        }
        else
        {
            logInfo(MIXLOG << traceInfo() << "open codec success");
        }

        return 0;
    }

    bool Decoder::popMediaPacket(MediaPacket &tMediaPacket)
    {
        return getInVideoQueue() ? 
            getInVideoQueue()->pop(tMediaPacket, DEFAULT_QUEUE_TIMEOUT_MS) : false;
    }

    void Decoder::pushMediaFrame(MediaFrame &tMediaFrame)
    {
        if (getOutVideoQueue())
        {
            getOutVideoQueue()->push(tMediaFrame.getDts(), tMediaFrame);
        }
    }

    bool Decoder::popMediaFrame(MediaFrame &tFrame)
    {
        return getOutVideoQueue() ? 
            getOutVideoQueue()->pop(tFrame, DEFAULT_QUEUE_TIMEOUT_MS) : false;
    }

    int Decoder::doDecode(MediaPacket &tMediaPacket, MediaFrame &tMediaFrame)
    {
        int got_frame = 0;

        logDebug(MIXLOG << "deocder bitrate:" << m_decodeCtx->bit_rate);

        AVFrame *avframe = av_frame_alloc();
        int ret = avcodec_decode_video2(m_decodeCtx, avframe, &got_frame, 
            tMediaPacket.getAVPacket());

        // TODO EAGIN
        if (!got_frame)
        {
            av_frame_free(&avframe);
            logErr(MIXLOG << traceInfo() << " doDecode fail, packet type"
                << static_cast<int>(tMediaPacket.getFrameType()) 
                << "frameid:" << tMediaPacket.getFrameId() << " ret:" << ret);
            return -1;
        }
        else if (tMediaPacket.isIFrame())
        {
            logInfo(MIXLOG << traceInfo() << " doDecode success"
                << ", packet type: " << static_cast<int>(tMediaPacket.getFrameType()) 
                << ", frameid: " << tMediaPacket.getFrameId() 
                << ", ret: " << ret << ", dts: " << tMediaPacket.getDts());
        }

        tMediaFrame.setAVFrame(avframe);
        tMediaFrame.setWidth(tMediaFrame.getAVFrame()->width);
        tMediaFrame.setHeight(tMediaFrame.getAVFrame()->height);
        int cts = tMediaPacket.getPts() - tMediaPacket.getDts();
        tMediaFrame.setDts(tMediaPacket.getDts());
        tMediaFrame.setPts(tMediaFrame.getDts() + cts);
        tMediaFrame.setFrameId(tMediaPacket.getFrameId());
        tMediaFrame.addIdTimeTrace(tMediaFrame.getStreamId(), TimeTraceKey::DECODE, getNowMs32());

        return 0;
    }

    void Decoder::addSubscriber(const std::string &subscriberName, SubscribeContext *context)
    {
        if (m_subscriberMap.find(subscriberName) == m_subscriberMap.end())
        {
            logInfo(MIXLOG << "subscriber name " << subscriberName << " insert");
            m_subscriberMap.insert(make_pair(subscriberName, context));
        }
        else
        {
            logInfo(MIXLOG << "subscriber name " << subscriberName << " replace");
            m_subscriberMap[subscriberName] = context;
        }
    }

    void Decoder::delSubscriber(const std::string &subscriberName)
    {
        auto itr = m_subscriberMap.find(subscriberName);
        if (m_subscriberMap.end() != itr)
        {
            logInfo(MIXLOG << "subscriber name" << subscriberName << " erase");
            m_subscriberMap.erase(itr);
        }
        else
        {
            logInfo(MIXLOG << "subscriber name" << subscriberName << " no found");
        }
    }

    void Decoder::dispatch(MediaFrame &frame)
    {
        for (auto &subscriber : m_subscriberMap)
        {
            logInfo(MIXLOG << " stream name: " << m_streamName << ", dts: " << frame.getDts());
            auto context = subscriber.second;
            context->pushVideoFrame(frame);
        }
    }

    AVCodecContext *Decoder::getDecodeContext()
    {
        if (m_ready)
        {
            return m_decodeCtx;
        }
        return nullptr;
    }

} // namespace hercules
