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
#include "JobManager.h"
#include "Log.h"
#include "ThreadPool.h"

#include "json/json.h"

#include <set>
#include <utility>
#include <string>

namespace hercules
{

    using std::set;
    using std::string;

    Job *JobManager::addTask(const std::string &json, const Json::Value &val, 
        const DataCallback &cb)
    {
        Job *job = JobManager::getInstance()->findJob(val["task_id"].asString());
        logInfo(MIXLOG << "fix json: " << json);
        std::string taskKey = val["task_id"].asString();
        if (job == NULL)
        {
            logInfo(MIXLOG << "new job, key: " << val["task_id"]);

            job = new Job();
            job->init(val["task_id"].asString(), val["output_stream"]["streamname"].asString(), 
                val["task_file"].asString(), cb);
            insertJob(taskKey, job);

            string sJson = json;
            Job *rawJob = findJob(taskKey);
            if (rawJob)
            {
                rawJob->pushJson(sJson);
                rawJob->start();
            }
            else
            {
                logInfo(MIXLOG << "media lua" << val["task_id"].asString() << " start fail");
            }
        }
        else
        {
            Job *rawJob = JobManager::getInstance()->findJob(taskKey);
            if (rawJob)
            {
                logInfo(MIXLOG << "update job: " << taskKey);
                rawJob->pushJson(json);
            }
            else
            {
                logErr(MIXLOG << "update failed: " << taskKey);
            }
        }
        return job;
    }

    void JobManager::updateJson(const std::string &json)
    {
        Json::Reader tReader;
        Json::Value tValue;

        if (!tReader.parse(json, tValue))
        {
            logErr(MIXLOG << "error invalid json");
            return;
        }
        std::string taskKey = tValue["task_id"].asString();

        Job *rawJob = JobManager::getInstance()->findJob(taskKey);
        if (rawJob)
        {
            logInfo(MIXLOG << "update job: " << taskKey);
            rawJob->pushJson(json);
        }
        else
        {
            logErr(MIXLOG << "update failed: " << taskKey);
        }
    }

    JobManager::JobManager()
    {
    }

    JobManager::~JobManager()
    {
    }

    Job *JobManager::findJob(const std::string &key)
    {
        std::unique_lock<std::mutex> lockGuard(m_jobMapMutex);

        auto iter = m_jobMap.find(key);

        if (iter == m_jobMap.end())
        {
            return NULL;
        }

        return iter->second;
    }

    Job *JobManager::getOrCreateJob(const std::string &key)
    {
        std::unique_lock<std::mutex> lockGuard(m_jobMapMutex);

        auto iter = m_jobMap.find(key);

        Job *job = NULL;
        if (iter == m_jobMap.end())
        {
            job = new Job();
            m_jobMap.insert(make_pair(key, job));
        }
        else
        {
            job = iter->second;
        }

        return job;
    }

    bool JobManager::insertJob(const std::string &key, Job *job)
    {
        logInfo(MIXLOG << "insert job: " << key);

        std::unique_lock<std::mutex> lockGuard(m_jobMapMutex);

        auto ret = m_jobMap.insert(make_pair(key, job));

        return ret.second;
    }

    void JobManager::removeJob(const std::string &key)
    {
        logInfo(MIXLOG << "remove job: " << key);

        std::unique_lock<std::mutex> lockGuard(m_jobMapMutex);

        m_jobMap.erase(key);
    }

    void JobManager::checkTimeoutJob()
    {
        set<Job *> deleteJobs;
        {
            std::unique_lock<std::mutex> lockGuard(m_jobMapMutex);

            auto iter = m_jobMap.begin();
            while (iter != m_jobMap.end())
            {
                const string &key = iter->first;
                logDebug(MIXLOG << "job: " << key);
                Job *job = iter->second;

                if (job == NULL)
                {
                    iter = m_jobMap.erase(iter);
                    continue;
                }

                if (job->isTimeout())
                {
                    logInfo(MIXLOG << "job: " << key << ", timeout");
                    job->stop();
                    iter = m_jobMap.erase(iter);
                    deleteJobs.insert(job);
                    continue;
                }

                logDebug(MIXLOG << "job: " << key << ", still running...");
                ++iter;
            }
        }

        for (const auto &job : deleteJobs)
        {
            job->joinThread();
            if (job != NULL)
            {
                job->joinThread();
                delete job;
            }
        }
    }

    void JobManager::subscribeJobFrame(const std::string &key, const std::string &streamName,
                                       SubscribeContext *ctx)
    {
        logInfo(MIXLOG << "job manager, subscribe job, frame job key:" << key);
        Job *job = findJob(key);
        if (job == nullptr)
        {
            return;
        }
        job->subscribeJobFrame(streamName, ctx);
    }

} // namespace hercules
