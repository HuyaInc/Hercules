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

#include "Job.h"
#include "Lua.h"
#include "Util.h"
#include "Log.h"
#include "Decoder.h"
#include "JobManager.h"
#include "AVFrameUtils.h"
#include "Common.h"
#include "AudioDecoder.h"

#include <string>
#include <set>

namespace hercules
{

    using std::exception;
    using std::string;

    Job::Job()
        : m_preUpdateTimeMs(getNowMs())
    {
    }

    Job::~Job()
    {
        JobManager::getInstance()->removeJob(m_key);
        for (auto decoder : m_decoders)
        {
            delete decoder.second;
        }
        m_decoders.clear();
    }

    int Job::init(const string &key, const string &name, const string &scriptName,
                  const DataCallback &cb)
    {
        m_key = key;
        m_name = name;
        m_script = scriptName;
        m_dataCb = cb;
        logInfo(MIXLOG << "script name: " + scriptName);
        return 0;
    }

    int Job::init(const std::string &fontFile, const DataCallback &cb)
    {
        logInfo(MIXLOG << "init datacb task id: " << m_key);
        m_dataCb = cb;
    }

    int Job::addAVData(AVData &data)
    {
        if (isStop())
        {
            return EC_ERROR;
        }
        int ret = EC_SUCCESS;
        switch (data.m_dataType)
        {
        case DATA_TYPE_FLV_AUDIO:
        {
            ret = addAudioData(data);
        }
        break;
        case DATA_TYPE_FLV_VIDEO:
        {
            ret = addVideoData(data);
        }
        break;
        default:
            ret = EC_SUCCESS;
            break;
        }
        return ret;
    }

    int Job::addAudioData(AVData &data)
    {
        logDebug(MIXLOG << data.m_streamName);
        int ret = EC_SUCCESS;
        AudioDecoderCtx *decoderCtx = nullptr;
        auto it = m_audioDecoders.find(data.m_streamName);
        if (it == m_audioDecoders.end())
        {
            logInfo(MIXLOG << "new decoder ctx");
            decoderCtx = new AudioDecoderCtx();
            m_audioDecoders[data.m_streamName] = decoderCtx;
            decoderCtx->m_decoder.init(m_key, &(decoderCtx->m_packetQueue));
            decoderCtx->m_resampler.subscribeAudioFrame();
            decoderCtx->m_decoder.addSubscriber(m_key, &(decoderCtx->m_resampler));

            if (m_subCtxMap.find(data.m_streamName) != m_subCtxMap.end())
            {
                logInfo(MIXLOG << "decoder ctx add subscriber" << data.m_streamName);
                decoderCtx->m_resampler.addSubscriber(m_key, m_subCtxMap[data.m_streamName]);
            }
            decoderCtx->m_decoder.setStreamName(data.m_streamName);
            decoderCtx->m_resampler.setStreamName(data.m_streamName);
            decoderCtx->m_decoder.start();
            decoderCtx->m_resampler.start();
        }
        else
        {
            decoderCtx = m_audioDecoders[data.m_streamName];
        }
        MediaPacket packet;
        ret = MediaPacket::genMediaPacketFromFlvWithHeader(
            reinterpret_cast<const uint8_t *>(data.m_data.data()), data.m_data.size(), packet);
        if (ret == -1)
        {
            ret = EC_ERROR;
            return ret;
        }
        packet.setFrameId((decoderCtx->m_frameId)++);
        if (decoderCtx->m_packetQueue.push(packet.getDts(), packet))
        {
            logDebug(MIXLOG << "push success dts: " << packet.getDts() 
                << ", pts: " << packet.getPts() 
                << ", diff: " << packet.getPts() - packet.getDts() 
                << ", frameid: " << packet.getFrameId());
        }
        else
        {
            logErr(MIXLOG << "error " << "push error");
        }
        return ret;
    }

    int Job::addVideoData(AVData &data)
    {
        int ret = EC_SUCCESS;
        DecoderCtx *decoderCtx = nullptr;
        auto it = m_decoders.find(data.m_streamName);
        if (it == m_decoders.end())
        {
            logInfo(MIXLOG << "new decoderCtx");
            decoderCtx = new DecoderCtx();
            m_decoders[data.m_streamName] = decoderCtx;
            decoderCtx->m_decoder.init(m_key, &(decoderCtx->m_packetQueue));
            if (m_subCtxMap.find(data.m_streamName) != m_subCtxMap.end())
            {
                logInfo(MIXLOG << "decoderCtx addSubscriber");
                decoderCtx->m_decoder.addSubscriber(m_key, m_subCtxMap[data.m_streamName]);
            }
            decoderCtx->m_decoder.setStreamName(data.m_streamName);
            decoderCtx->m_decoder.start();
        }
        else
        {
            decoderCtx = m_decoders[data.m_streamName];
        }
        MediaPacket packet;
        ret = MediaPacket::genMediaPacketFromFlvWithHeader(
            reinterpret_cast<const uint8_t *>(data.m_data.data()), data.m_data.size(), packet);
        if (ret == -1)
        {
            ret = EC_ERROR;
            return ret;
        }
        packet.setFrameId((decoderCtx->m_frameId)++);
        logDebug(MIXLOG << "frametype:" << static_cast<int>(packet.getFrameType()) 
            << ", frameid:" << packet.getFrameId() << ", ret" << ret);
        if (packet.isIFrame() && !packet.isHeaderFrame())
        {
            std::string avcConfig;
            if (parseAvcConfig(data.m_data, avcConfig))
            {
                logInfo(MIXLOG << "parse avcconfig success");
                packet.setGlobalHeader(avcConfig);
            }
            else
            {
                logWarn(MIXLOG << "parse avcconfig fail");
            }
        }
        if (decoderCtx->m_packetQueue.push(packet.getDts(), packet))
        {
            logDebug(MIXLOG << "push success"
                << ", dts: " << packet.getDts() << ", pts: " << packet.getPts() 
                << ", diff: " << packet.getPts() - packet.getDts() 
                << ", frameid: " << packet.getFrameId());
        }
        else
        {
            logErr(MIXLOG << "push error");
        }
        return ret;
    }

    void Job::threadEntry()
    {
        luaJob();
    }

    void Job::luaJob()
    {
        Lua *lua = NULL;
        string name = m_name;
        string key = m_key;

        logInfo(MIXLOG << "thread start: " << key << ", name: " << name);

        try
        {
            lua = new Lua;
            lua->initScript(m_script);

            string sJson;
            while (!isStop())
            {
                if (getJsonQueue().pop_front(sJson, 100))
                {
                    break;
                }
            }

            logInfo(MIXLOG << name << " is stop:" << isStop());
            logInfo(MIXLOG << name << " init first json: " << sJson);

            if (!isStop())
            {
                if (0 != lua->start(key, name, sJson))
                {
                    logErr(MIXLOG << "error" 
                        << ", key: " << key << ", name: " << name << ", start fail");
                    return;
                }
            }

            while (!isStop())
            {
                if (getJsonQueue().pop_front(sJson, 10))
                {
                    if (lua->update(key, name, sJson) != 0)
                    {
                        logErr(MIXLOG << "error"
                            << ", key: " << key << ", name: " << name << ", update fail");

                        break;
                    }
                }

                if (lua->process() != 0)
                {
                    logErr(MIXLOG << "error"
                        << ", key: " << key << ", name: " << name << ", process fail");

                    break;
                }
            }

            stopDecoder();
            lua->stop();
        }
        catch (exception &ex)
        {
            logErr(MIXLOG << "error: " << ", ex: " << ex.what());
        }
        catch (...)
        {
            logErr(MIXLOG << "error: " << ", ex:unknown");
        }

        logInfo(MIXLOG << "thread stop: " << key << ", name: " << name);

        if (lua)
        {
            delete lua;
        }
    }

    void Job::stopDecoder()
    {
        std::set<DecoderCtx *> deleteDecoders;
        std::set<AudioDecoderCtx *> deleteAudioDecoders;

        {
            std::unique_lock<std::mutex> lock(m_decoderMutex);
            for (const auto &decoder : m_decoders)
            {
                decoder.second->m_decoder.stop();
                deleteDecoders.insert(decoder.second);
            }
            for (const auto &decoder : m_audioDecoders)
            {
                decoder.second->m_decoder.stop();
                deleteAudioDecoders.insert(decoder.second);
            }
            m_decoders.clear();
            m_audioDecoders.clear();
        }

        for (const auto &decoder : deleteDecoders)
        {
            decoder->m_decoder.join();
            delete decoder;
        }
        for (const auto &decoder : deleteAudioDecoders)
        {
            decoder->m_decoder.join();
            delete decoder;
        }
    }

    void Job::sendData(const AVData &data)
    {
        if (!isStop())
        {
            if (m_dataCb)
            {
                m_dataCb(data);
            }
        }
    }

    void Job::subscribeJobFrame(const std::string &streamName, SubscribeContext *ctx)
    {
        logInfo(MIXLOG << "job subscribe job frame task id: " + m_key);
        std::unique_lock<std::mutex> lock(m_subMutex);
        m_subCtxMap[streamName] = ctx;

        auto videoDecoder = m_decoders.find(streamName);
        if (videoDecoder != m_decoders.end())
        {
            logInfo(MIXLOG << "decoder add subscriber: " << ctx->m_streamName);
            m_decoders[streamName]->m_decoder.addSubscriber(m_key, ctx);
        }

        auto audioDecoder = m_audioDecoders.find(streamName);
        if (audioDecoder != m_audioDecoders.end())
        {
            logInfo(MIXLOG << "audio decoder add subscriber: " << ctx->m_streamName);
            m_audioDecoders[streamName]->m_decoder.addSubscriber(m_key, ctx);
        }
    }

} // namespace hercules
