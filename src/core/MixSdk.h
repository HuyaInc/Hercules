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

#include "OneCycleThread.h"
#include "Queue.h"
#include "MediaPacket.h"
#include "SubscribeContext.h"
#include "Log.h"
#include "FlvFile.h"
#include "Singleton.h"

#include "json/json.h"

#include <functional>
#include <vector>
#include <inttypes.h>
#include <memory>
#include <mutex>
#include <map>
#include <string>

class Decoder;
class OpenCVOperator;
class Encoder;

namespace hercules
{

    enum DataType
    {
        DATA_TYPE_FLV_AUDIO = 0X08,
        DATA_TYPE_FLV_VIDEO = 0X09,
        DATA_TYPE_FLV_SCRIPT = 0X12
    };

    enum FlvTagType
    {
        FLV_AUDIO_TAG = 0X08,
        FLV_VIDEO_TAG = 0X09,
        FLV_SCRIPT_TAG = 0X12
    };

    struct AVData
    {
        explicit AVData(DataType type = DATA_TYPE_FLV_VIDEO) : m_dataType(type)
        {
        }

        DataType m_dataType;
        std::string m_data;
        uint32_t m_dts;
        uint32_t m_pts;
        std::string m_streamName;
    };

    enum ErrorCode
    {
        EC_SUCCESS = 0,
        EC_ERROR = 1,
        EC_JSON_ERROR = 100
    };

    typedef std::function<void(const AVData &data)> DataCallback;

    class MixTask : public OneCycleThread
    {
    public:
        MixTask() {}
        virtual ~MixTask() {}
        virtual int init(const std::string &fontFile, const DataCallback &cb) = 0;
        virtual void destroy() = 0;
        virtual void updateJson(const std::string &json) = 0;
        virtual int addAVData(AVData &data)
        {
            logDebug(MIXLOG << "MixTask addAVData");
        }

        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void join() = 0;
        virtual bool canStop() = 0;
    };

    class MixTaskManager : public Singleton<MixTaskManager>
    {
        friend class Singleton<MixTaskManager>;

    private:
        MixTaskManager() {}
        ~MixTaskManager() {}

    public:
        int init(const std::string &fontFile, const LogCallback &logCb,
            LogLevel logLevel = LOG_LEVEL_INFO);
        void destroy();

        const std::string &fontFile() { return m_fontFile; }
        int stopTask(const std::string &taskKey);
        MixTask *addTask(const std::string &task, const DataCallback &dataCb);
        void stopAll();

    private:
        std::map<std::string, MixTask *> m_tasks;
        std::mutex m_taskMutex;
        std::string m_fontFile;
    };

} // namespace hercules
