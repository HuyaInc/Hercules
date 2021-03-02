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

#include "Encoder.h"
#include "Common.h"
#include "OpenCVOperator.h"
#include "MediaPacket.h"
#include "MediaFrame.h"
#include "Util.h"
#include "Log.h"
#include "Decoder.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avc.h"
#include "x264/x264.h"
}

#include <utility>
#include <string>

#define ADD_X264OPTS(x264opts, param) \
    {                                 \
        if (strlen(x264opts) > 0)     \
        {                             \
            strcat(x264opts, ":");    \
        }                             \
        strcat(x264opts, param);      \
    }

using namespace std;

namespace hercules
{

    Encoder::Encoder()
        : OneCycleThread()
        , Property()
        , m_inputVideoFrameQueue(nullptr)
        , m_outputVideoPacketQueue(nullptr)
        , m_encodeCtx(nullptr)
        , m_ready(false)
        , m_codec(DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS, DEFAULT_VIDEO_BPS, CodecType::H264)
        , m_videoHeader()
        , m_numOfEncodedFrame(0)
        , m_lastSendDts(0)
        , m_lastSendPts(0)
        , m_lowLatency(true)
        , m_copyDecoder(nullptr)
    {
        logInfo(MIXLOG);
    }

    Encoder::~Encoder()
    {
        logInfo(MIXLOG << traceInfo());
        reset();
    }

    int Encoder::init(const string &name, Queue<MediaFrame> &inQueue, Queue<MediaPacket> &outQueue)
    {
        Property::setStreamName(name);

        m_inputVideoFrameQueue = &inQueue;
        m_outputVideoPacketQueue = &outQueue;
        logInfo(MIXLOG << traceInfo());
        return 0;
    }

    void Encoder::reset()
    {
        if (m_encodeCtx != nullptr)
        {
            avcodec_free_context(&m_encodeCtx);
            m_encodeCtx = nullptr;
        }
    }

    bool Encoder::popMediaFrame(MediaFrame &tMediaFrame)
    {
        return getInVideoQueue() ?
            getInVideoQueue()->pop(tMediaFrame, DEFAULT_QUEUE_TIMEOUT_MS) : false;
    }

    void Encoder::pushMediaFrame(MediaFrame &tMediaFrame)
    {
        getInVideoQueue()->push(tMediaFrame.getDts(), tMediaFrame);
    }

    bool Encoder::popMediaPacket(MediaPacket &tMediaPacket)
    {
        return getOutVideoQueue() ?
            getOutVideoQueue()->pop(tMediaPacket, DEFAULT_QUEUE_TIMEOUT_MS) : false;
    }

    void Encoder::pushMediaPacket(MediaPacket &tMediaPacket)
    {
        if (getOutVideoQueue())
        {
            getOutVideoQueue()->push(tMediaPacket.getDts(), tMediaPacket);
        }
    }

    void Encoder::setCodec(int width, int height, int fps,
        int kbps, const string &codec)
    {
        std::unique_lock<std::mutex> lockGuard(m_mutex);

        m_codec = getVideoCodec(width, height, rangeLimit(fps, MIN_FPS, MAX_FPS),
            rangeLimit(kbps, MIN_BPS, MAX_BPS), codec);

        logInfo(MIXLOG << traceInfo() << "codec:" << m_codec.print());
        m_ready = false;
    }

    void Encoder::setBitrate(int bitrate)
    {
        m_codec.m_kbps = bitrate;
    }

    void Encoder::threadEntry()
    {
        logInfo(MIXLOG << "thread start: " << traceInfo());

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
                    logErr(MIXLOG << traceInfo() << " encode fail");
                    av_packet_free(&avpacket);
                    continue;
                }
                else if (tMediaFrame.isIFrame())
                {
                    logInfo(MIXLOG << traceInfo() << " encode success");
                }

                MediaPacket tMediaPacket;
                tMediaPacket.asVideo();
                int64_t pts = avpacket->pts;
                int64_t dts = avpacket->dts;

                logDebug(MIXLOG << traceInfo() << ", avframe_pts: " << avframe_pts 
                    << ", encoded dts: " << dts << ", pts: " << pts);

                tMediaPacket.setDts(dts);
                tMediaPacket.setPts(pts);
                getFrameMetadata(pts, tMediaPacket);
                tMediaPacket.addAllIdTimeTrace(TimeTraceKey::ENCODE, getNowMs32());
                m_lastSendDts = dts;
                m_lastSendPts = pts;

                string pictTypeStr = "UNKNOWN";
                int pictType = -1;
                uint8_t *sideData = nullptr;
                int sideDataSize = 0;

                sideData = av_packet_get_side_data(avpacket, 
                    AV_PKT_DATA_QUALITY_STATS, &sideDataSize);
                if (sideData && sideDataSize > 4)
                {
                    pictType = sideData[4];
                    switch (pictType)
                    {
                    case AV_PICTURE_TYPE_I:
                        pictTypeStr = "[I]";
                        break;

                    case AV_PICTURE_TYPE_P:
                        pictTypeStr = "[P]";
                        break;

                    case AV_PICTURE_TYPE_B:
                        pictTypeStr = "[B]";
                        break;

                    default:
                        pictTypeStr = "[UNKNOWN]";
                        break;
                    }
                }
                
                bool print = false;
                if (m_codec.m_codecType == CodecType::H264)
                {
                    if (pictType == AV_PICTURE_TYPE_I)
                    {
                        print = true;
                        tMediaFrame.asIFrame();
                        tMediaPacket.asIFrame();
                    }
                    else if (pictType == AV_PICTURE_TYPE_P)
                    {
                        tMediaFrame.asPFrame();
                        tMediaPacket.asPFrame();
                    }
                    else if (pictType == AV_PICTURE_TYPE_B)
                    {
                        tMediaFrame.asBFrame();
                        tMediaPacket.asBFrame();
                    }
                    else
                    {
                        logErr(MIXLOG << traceInfo() << ", unknown pict type");
                    }
                }
                else if (m_codec.m_codecType == CodecType::H265)
                {
                    // TODO IMP
                }

                if(print)
                {
                    logInfo(MIXLOG << traceInfo() << ", " << tMediaFrame.print()
                        << ", input video queue size: " << getInVideoQueue()->size()
                        << ", ouput video queue size: " << getOutVideoQueue()->size()
                        << ", pictType: " << pictType 
                        << ", pictTypeStr: " << pictTypeStr 
                        << ", encode success"
                        << ", encodeNum: " << m_numOfEncodedFrame 
                        << ", pts: " << pts << " dts: " << dts 
                        << ", size: " << avpacket->size);
                }

                if (m_codec.m_codecType == CodecType::H264)
                {
                    tMediaPacket.asH264();
                }
                else if (m_codec.m_codecType == CodecType::H265)
                {
                    // TODO IMP
                }
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
            logErr(MIXLOG << traceInfo() << ", ex: unknown");
        }

        logInfo(MIXLOG << "thread stop: " << traceInfo());
    }

    int Encoder::setupEncoder()
    {
        reset();

        string encoderName = "libx264";
        if (m_codec.m_codecType == CodecType::H265)
        {
            // TODO IMP
        }

        AVCodec *avcodec = avcodec_find_encoder_by_name(encoderName.c_str());

        if (avcodec == nullptr)
        {
            logErr(MIXLOG << traceInfo() << " can't find avcodec: " << encoderName);
            return -1;
        }

        m_encodeCtx = avcodec_alloc_context3(avcodec);
        return_if(!m_encodeCtx, "allocate encode context fail");

        m_encodeCtx->time_base = (AVRational){1, 1000};
        m_encodeCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        m_encodeCtx->width = m_codec.m_width;
        m_encodeCtx->height = m_codec.m_height;
        m_encodeCtx->pix_fmt = AV_PIX_FMT_YUV420P;

        if (m_lowLatency)
        {
            m_encodeCtx->gop_size = m_codec.m_fps * 1;
            m_encodeCtx->max_b_frames = 0;
        }
        else
        {
            m_encodeCtx->gop_size = m_codec.m_fps * 3;

            if (m_codec.m_codecType == CodecType::H264)
            {
                m_encodeCtx->max_b_frames = 1;
            }
        }

        logInfo(MIXLOG << traceInfo() << ", width: " << m_codec.m_width << ", height: " 
            << m_codec.m_height << ", fps: " << m_codec.m_fps);

        int bit_rate = m_codec.m_kbps * 1024;
        m_encodeCtx->bit_rate = bit_rate;
        m_encodeCtx->rc_min_rate = bit_rate;
        m_encodeCtx->rc_max_rate = bit_rate;
        m_encodeCtx->bit_rate_tolerance = bit_rate;
        m_encodeCtx->rc_buffer_size = bit_rate * 9 / 20;
        m_encodeCtx->rc_initial_buffer_occupancy = m_encodeCtx->rc_buffer_size * 4 / 5;
        m_encodeCtx->qcompress = 1.0;

        logInfo(MIXLOG << "encoder bitrate: " << m_encodeCtx->bit_rate);

        if (m_copyDecoder != nullptr)
        {
            logInfo(MIXLOG << "copy decoder1");
            AVCodecContext *decoder = m_copyDecoder->getDecodeContext();
            if (decoder != nullptr)
            {
                logInfo(MIXLOG << "copy decoder"
                    << ", bitrate:" << decoder->bit_rate 
                    << ", gop size:" << decoder->gop_size);
                m_encodeCtx->width = decoder->width;
                m_encodeCtx->height = decoder->height;
                m_encodeCtx->pix_fmt = decoder->pix_fmt;
            }
        }

        if (m_lowLatency)
        {
            av_opt_set(m_encodeCtx->priv_data, "mbtree", "0", 0);
            av_opt_set(m_encodeCtx->priv_data, "rc-lookahead", "0", 0);
        }

        if (m_codec.m_codecType == CodecType::H264)
        {
            // fps
            char x264opts[512] = {0};
            char param[16] = {0};
            snprintf(param, sizeof(param), "fps=%u/%u", m_codec.m_fps, 1);

            ADD_X264OPTS(x264opts, param);
            ADD_X264OPTS(x264opts, "annexb=0");
            ADD_X264OPTS(x264opts, "aq-mode=0");
            ADD_X264OPTS(x264opts, "psy=0");
            ADD_X264OPTS(x264opts, "psnr=1");

            if (m_lowLatency)
            {
                av_opt_set(m_encodeCtx->priv_data, "preset", "ultrafast", 0);
                av_opt_set(m_encodeCtx->priv_data, "tune", "zerolatency", 0);
            }
            else
            {
                av_opt_set(m_encodeCtx->priv_data, "preset", "faster", 0);
                av_opt_set(m_encodeCtx->priv_data, "mbtree", "0", 0);
                av_opt_set(m_encodeCtx->priv_data, "rc-lookahead", "5", 0);
            }

            m_encodeCtx->thread_count = 16;
            av_opt_set(m_encodeCtx->priv_data, "x264opts", x264opts, 0);
        }
        else if (m_codec.m_codecType == CodecType::H265)
        {
            // TODO IMP
        }

        av_opt_set(m_encodeCtx->priv_data, "b-pyramid", "0", 0);
        logInfo(MIXLOG << "bitrate:" << m_encodeCtx->bit_rate);
        if (avcodec_open2(m_encodeCtx, m_encodeCtx->codec, nullptr) < 0)
        {
            reset();
            logErr(MIXLOG << traceInfo() << "|open encode codec fail!");
            return -1;
        }

        if (m_codec.m_codecType == CodecType::H265)
        {
            // TODO IMP
        }
        else
        {
            uint8_t startCode[4] = {0, 0, 0, 1};
            uint8_t *p = m_encodeCtx->extradata;
            uint32_t size;

            while (reinterpret_cast<uint8_t *>(p) - m_encodeCtx->extradata
                < m_encodeCtx->extradata_size)
            {
                size = av_be2ne32(*reinterpret_cast<uint32_t *>(p));
                memcpy(reinterpret_cast<void *>(p), reinterpret_cast<void *>(startCode),
                       sizeof(startCode));
                p += sizeof(size) + size;
            }

            AVIOContext *pb;
            uint8_t *P = nullptr;
            int ret = avio_open_dyn_buf(&pb);
            if (ret == 0)
            {
                logInfo(MIXLOG << traceInfo() << ", header:" 
                    << bin2str(m_encodeCtx->extradata, m_encodeCtx->extradata_size, " "));
                ff_isom_write_avcc(pb, m_encodeCtx->extradata, m_encodeCtx->extradata_size);
                m_videoHeader.m_len = avio_close_dyn_buf(pb, &P);
                memcpy(m_videoHeader.m_config, P, m_videoHeader.m_len);
                av_freep(&P);
            }
        }

        if (m_videoHeader.m_len == 0)
        {
            logErr(MIXLOG << traceInfo() << "|setupEncoder Get Video Config Fail.");
            return -1;
        }

        string sNewHex = dump(m_videoHeader.m_config, m_videoHeader.m_len);
        logInfo(MIXLOG << traceInfo() << "|dump new avc header:" << sNewHex);

        MediaPacket tMediaPacket;
        tMediaPacket.asHeaderFrame();
        tMediaPacket.setDts(m_lastSendDts);
        tMediaPacket.setPts(m_lastSendPts);
        tMediaPacket.asVideo();
        if (m_codec.m_codecType == CodecType::H264)
        {
            tMediaPacket.asH264();
        }
        else
        {
            // TODO IMP
        }

        AVPacket *avPacket = av_packet_alloc();
        size_t avpacket_buf_size = m_videoHeader.m_len;
        uint8_t *avpacket_buf = reinterpret_cast<uint8_t *>(av_malloc(avpacket_buf_size));
        memcpy(avpacket_buf, m_videoHeader.m_config, m_videoHeader.m_len);
        av_packet_from_data(avPacket, avpacket_buf, avpacket_buf_size);
        tMediaPacket.setAVPacket(avPacket);
        pushMediaPacket(tMediaPacket);

        // XXX:
        m_numOfEncodedFrame = 0;

        return 0;
    }

    int Encoder::doEncode(AVFrame *tFrame, AVPacket *tPacket) // frame - > packet
    {
        if (m_encodeCtx == nullptr)
        {
            return -1;
        }

        int gotFrame = 0;
        int ret = avcodec_encode_video2(m_encodeCtx, tPacket, tFrame, &gotFrame);

        if (ret < 0)
        {
            logErr(MIXLOG << traceInfo() << " encode failed! ret:" << ret);
            ret = -1;
        }

        if (!gotFrame)
        {
            logErr(MIXLOG << traceInfo() << " no got frame");
            ret = -1;
        }

        return ret;
    }

    void Encoder::insertFrameMetadata(int64_t key, const MediaFrame &tMediaFrame)
    {
        FrameMetadata frameMetadata(tMediaFrame.getIdTimestamp(), tMediaFrame.getIdTimeTrace());
        m_frameMetadata.insert(make_pair(key, frameMetadata));

        if (m_frameMetadata.size() >= 100)
        {
            m_frameMetadata.erase(m_frameMetadata.begin());
        }
    }

    bool Encoder::getFrameMetadata(int64_t key, MediaPacket &tMediaPacket)
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
