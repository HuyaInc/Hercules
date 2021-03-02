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

#include "Singleton.h"
#include "MixSdk.h"

#include "json/json.h"

#include <map>
#include <mutex>
#include <string>

namespace hercules
{

    class Job;

    class JobManager : public Singleton<JobManager>
    {
        friend class Singleton<JobManager>;

    private:
        JobManager();
        ~JobManager();

    public:
        Job *findJob(const std::string &key);
        Job *getOrCreateJob(const std::string &key);
        bool insertJob(const std::string &key, Job *job);
        void removeJob(const std::string &key);
        Job *addTask(const std::string &json, const Json::Value &val, const DataCallback &cb);
        void updateJson(const std::string &json);
        void subscribeJobFrame(const std::string &key,
            const std::string &streamName, SubscribeContext *ctx);

        void checkTimeoutJob();

        int jobCount()
        {
            std::unique_lock<std::mutex> lockGuard(m_jobMapMutex);
            return m_jobMap.size();
        }

    private:
        std::map<std::string, Job *> m_jobMap;
        std::mutex m_jobMapMutex;
    };

} // namespace hercules
