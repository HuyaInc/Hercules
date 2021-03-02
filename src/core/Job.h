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

#include "Common.h"
#include "OneCycleThread.h"
#include "Util.h"
#include "MediaBase.h"
#include "Log.h"
#include "ThreadQueue.h"
#include "MixSdk.h"
#include "Decoder.h"
#include "AudioDecoder.h"
#include "AudioResampler.h"

#include <vector>
#include <queue>
#include <map>
#include <string>

namespace hercules
{

    constexpr int kJobTimeoutMs = 10000;

    class Lua;

    struct DecoderCtx
    {
        DecoderCtx() : m_frameId(0)
        {
        }
        Decoder m_decoder;
        Queue<MediaPacket> m_packetQueue;
        uint32_t m_frameId;
    };

    struct AudioDecoderCtx
    {
        AudioDecoderCtx() : m_frameId(0)
        {
        }
        AudioDecoder m_decoder;
        Queue<MediaPacket> m_packetQueue;
        uint32_t m_frameId;
        AudioResampler m_resampler;
    };

    class Job : public MixTask
    {
    public:
        Job();
        ~Job();

        int init(const std::string &key, const std::string &name,
                 const std::string &scriptName, const DataCallback &cb);

        int init(const std::string &fontFile, const DataCallback &cb);

        void destroy() {}

        bool isTimeout() const { return m_preUpdateTimeMs + kJobTimeoutMs < getNowMs(); }

        void updateJson(const std::string &sJson) { pushJson(sJson); }

        void stopDecoder();

        void pushJson(const std::string &sJson)
        {
            getJsonQueue().push_back(sJson);
            m_preUpdateTimeMs = getNowMs();
        }

        void start()
        {
            logInfo(MIXLOG << "job start: " << m_name);
            OneCycleThread::startThread("luaJob:" + m_name);
        }

        bool canStop()
        {
            return isTimeout();
        }

        void stop()
        {
            logInfo(MIXLOG << "job stop: " << m_key);
            OneCycleThread::stopThread();
        }

        void join()
        {
            OneCycleThread::joinThread();
        }

        void subscribeJobFrame(const std::string &streamName, SubscribeContext *ctx);

        int addAVData(AVData &data);
        void sendData(const AVData &data);

    private:
        int addVideoData(AVData &data);
        int addAudioData(AVData &data);

        ThreadQueue<std::string> &getJsonQueue() { return m_jsonQueue; }
        void threadEntry();
        void luaJob();

    private:
        std::string m_key;
        std::string m_name;
        std::string m_script;
        ThreadQueue<std::string> m_jsonQueue;

        uint64_t m_preUpdateTimeMs;

        std::mutex m_decoderMutex;
        std::map<std::string, DecoderCtx *> m_decoders;
        std::map<std::string, AudioDecoderCtx *> m_audioDecoders;
        std::mutex m_subMutex;
        std::map<std::string, SubscribeContext *> m_subCtxMap;

        DataCallback m_dataCb;
    };

} // namespace hercules
