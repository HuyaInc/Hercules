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

#include "ThreadManager.h"
#include "Util.h"
#include "Util.h"
#include "Log.h"

#include <sys/prctl.h>
#include <atomic>
#include <thread>
#include <string>

namespace hercules
{

    class OneCycleThread
    {
    public:
        OneCycleThread()
            : m_stop(false)
            , m_name("")
            , m_thread(nullptr)
            , m_id(std::thread::id())
            , m_preTimerMs(getNowMs())
            , m_timerCounter(0)
        {
        }

        virtual ~OneCycleThread()
        {
            stopThread();
            joinThread();
            logInfo(MIXLOG << "name: " << m_name);
        }

        virtual void startThread(const std::string &name = "")
        {
            if (m_thread != nullptr)
            {
                return;
            }

            m_stop = false;
            m_name = name;
#if 0
        if (!name.empty())
        {
            prctl(PR_SET_NAME, (uint64_t)(name.c_str()), 0, 0);
        }
#endif

            logInfo(MIXLOG << "start thread: " << name);
            m_thread = new std::thread(&OneCycleThread::threadEntry, this);
            m_id = m_thread->get_id();
            ThreadManager::getInstance()->registerThread(m_id, name, m_thread);
        }

        virtual void stopThread()
        {
            m_stop = true;
        }

        virtual void joinThread()
        {
            if (m_thread != nullptr)
            {
                m_thread->join();

                ThreadManager::getInstance()->unregisterThread(m_id);

                delete m_thread;
                m_thread = nullptr;
            }
        }

        std::thread::id id()
        {
            return m_id;
        }

        bool isStop() { return m_stop; }

        virtual void threadEntry() = 0;
        void checkTimer()
        {
            uint64_t nowMs = getNowMs();

            if (nowMs - m_preTimerMs >= 1000)
            {
                oneSecondTimeout(m_timerCounter++);
                m_preTimerMs = nowMs;
            }
        }

        virtual void oneSecondTimeout(uint64_t counter)
        {
        
        }

    protected:
        std::atomic<bool> m_stop;
        std::string m_name;
        std::thread *m_thread;
        std::thread::id m_id;

        uint64_t m_preTimerMs;
        uint64_t m_timerCounter;
    };

} // namespace hercules
