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
#include "Common.h"
#include "Log.h"

#include <map>
#include <thread>
#include <mutex>
#include <utility>
#include <string>

namespace hercules
{

    class ThreadManager : public Singleton<ThreadManager>
    {
        friend class Singleton<ThreadManager>;

    private:
        ThreadManager()
        {
        }

        ~ThreadManager()
        {
        }

    public:
        void registerThread(std::thread::id id, const std::string &name, std::thread *t)
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);

            m_threads[id] = make_pair(name, t);

            logInfo(MIXLOG << "register thread: " << name);
        }

        void unregisterThread(std::thread::id id)
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);

            std::string name = "";
            auto iter = m_threads.find(id);
            if (iter != m_threads.end())
            {
                name = iter->second.first;
            }

            logInfo(MIXLOG << "unregister thread: " << name << ", id: " << id);

            m_threads.erase(id);
        }

        void dumpAllThread()
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);

            logInfo(MIXLOG << "thread num: " << m_threads.size());

            std::map<std::string, int> threadStatistic;

            for (const auto &kv : m_threads)
            {
                threadStatistic[kv.second.first]++;
            }

            for (const auto &kv : threadStatistic)
            {
                logInfo(MIXLOG << "thread: " << kv.first << ", count: " << kv.second);
            }
        }

        void joinAllThread()
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);

            logInfo(MIXLOG << "thread num: " << m_threads.size());

            auto iter = m_threads.begin();
            while (!m_threads.empty())
            {
                std::thread *t = iter->second.second;

                if (t != nullptr)
                {
                    t->join();
                }

                iter = m_threads.erase(iter);
            }
        }

    private:
        std::mutex m_mutex;
        std::map<std::thread::id, std::pair<std::string, std::thread *>> m_threads;
    };

} // namespace hercules
