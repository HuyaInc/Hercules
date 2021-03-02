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

#include "MixSdk.h"
#include "ThreadPool.h"
#include "Decoder.h"
#include "OpenCVOperator.h"
#include "Encoder.h"
#include "AVFrameUtils.h"
#include "Log.h"
#include "JobManager.h"

#include "json/json.h"

#include <string>

namespace hercules
{

    MixTask *MixTaskManager::addTask(const std::string &task, const DataCallback &dataCb)
    {
        if (m_fontFile.empty())
        {
            return nullptr;
        }
        Json::Reader tReader;
        Json::Value tValue;

        if (!tReader.parse(task, tValue))
        {
            logErr(MIXLOG << "error, invalid json");
            return nullptr;
        }

        if (tValue["task_type"] == "lua")
        {
            MixTask *job = reinterpret_cast<MixTask *>(
                JobManager::getInstance()->addTask(task, tValue, dataCb));
            m_tasks[tValue["task_id"].asString()] = job;
            return job;
        }
        return nullptr;
    }

    int MixTaskManager::init(const std::string &fontFile, const LogCallback &logCb, LogLevel logLevel)
    {
        if (fontFile.empty())
        {
            return EC_ERROR;
        }
        m_fontFile = fontFile;
        if (logCb)
        {
            setLogCb(logCb);
        }
        initLog(logLevel);

        av_register_all();
        avcodec_register_all();

        return EC_SUCCESS;
    }

    int MixTaskManager::stopTask(const std::string &taskKey)
    {
        int ret = EC_SUCCESS;

        if (m_tasks.find(taskKey) == m_tasks.end())
        {
            logErr(MIXLOG << "task not exist, " << taskKey);
            return ret;
        }
        m_tasks[taskKey]->stop();
        m_tasks.erase(taskKey);
        JobManager::getInstance()->removeJob(taskKey);
        return ret;
    }

    void MixTaskManager::stopAll()
    {
        for (auto task : m_tasks)
        {
            stopTask(task.first);
        }
    }
} // namespace hercules
