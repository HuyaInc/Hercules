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

#include "Util.h"
#include "Property.h"
#include "CycleCounterStat.h"
#include "Log.h"
#include "Common.h"

#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <map>
#include <utility>
#include <string>

using std::make_pair;

namespace hercules
{

    constexpr size_t kDefaultMaxSize = 600 * 6;
    constexpr size_t kMaxRollbackCount = 60 * 1;
    constexpr int DEFAULT_QUEUE_TIMEOUT_MS = 5;

    template <typename VAL>
    class Queue : public Property
    {
    public:
        Queue()
            : m_name("")
            , m_needReport(false)
            , m_prePushKey(UINT32_MAX)
            , m_prePopTimeMs(0)
            , m_preTimeRef(0)
            , m_maxSize(kDefaultMaxSize)
            , m_pushRollBackCount(0)
        {
        }

        ~Queue()
        {
            logInfo(MIXLOG << traceInfo());
        }

        virtual std::string traceInfo()
        {
            return "name:" + m_name + "," + Property::traceInfo();
        }

        void setName(const std::string &name)
        {
            m_name = name;
        }

        std::string getName() const
        {
            return m_name;
        }

        void setNeedReport(bool need)
        {
            m_needReport = need;
        }

        void setMaxSize(size_t max)
        {
            m_maxSize = max;
        }

        size_t size()
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);
            return m_queue.size();
        }

        bool empty()
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);
            return m_queue.empty();
        }

        bool push(uint32_t key, const VAL &val)
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);
            m_maxKey = m_maxKey > key ? m_maxKey : key;

            incrQueueSize();
            incrPushAll();

            bool forceClean = false;

            if (m_prePushKey != UINT32_MAX)
            {
                if (key < m_prePushKey && !val.isCleanQueueFrame())
                {
                    mixlog(MIXLOG << "error: " << traceInfo() << ", key rollback: "
                                  << m_prePushKey << " -> " << key);
                    incrPushFailed();

                    ++m_pushRollBackCount;

                    if (m_pushRollBackCount == kMaxRollbackCount)
                    {
                        m_pushRollBackCount = 0;
                        forceClean = true;

                        mixlog(MIXLOG << traceInfo() << ", rollback count -> " 
                            << kMaxRollbackCount << ", force clean");
                    }
                    else
                    {
                        return false;
                    }
                }
            }

            if (forceClean || val.isCleanQueueFrame())
            {
                m_prePushKey = UINT32_MAX;
                m_prePopTimeMs = 0;
                m_preTimeRef = 0;

                m_queueSizeCounterStat.resetCount(0);
                m_queueSizeCounterStat.resetAllCount(0);

                mixlog(MIXLOG << traceInfo() << ", clean queue because recv CLEAN_QUEUE_FRAME");
            }

            incrPushSuccess();

            m_prePushKey = key;

            m_queue.insert(make_pair(key, val));

            while (m_queue.size() >= m_maxSize)
            {
                mixlog(MIXLOG << "error:" << traceInfo()
                              << ", queue size too big: " << m_queue.size());
                m_queue.erase(m_queue.begin());
            }

            m_cond.notify_one();

            return true;
        }

        bool pop(VAL &val, int timeoutInMs = 0)
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);

            if (m_queue.empty())
            {
                if (timeoutInMs == 0)
                {
                    return false;
                }
                else if (timeoutInMs == -1)
                {
                    m_cond.wait(lockGuard);
                }
                else
                {
                    if (m_cond.wait_for(lockGuard, std::chrono::microseconds(timeoutInMs))
                         == std::cv_status::timeout)
                    {
                        return false;
                    }
                }
            }

            if (m_queue.empty())
            {
                return false;
            }

            m_prePopTimeMs = getNowMs();
            val = m_queue.begin()->second;

            assert(!m_queue.empty());

            m_queue.erase(m_queue.begin());

            return true;
        }

        bool pop_tail(VAL &val)
        {
            return pop_tail_n(val, 0);
        }

        bool pop_tail_n(VAL &val, int n)
        {
            std::unique_lock<std::mutex> lockGuard(m_mutex);

            if (m_queue.empty())
            {
                return false;
            }

            m_prePopTimeMs = getNowMs();

            int index = n;
            if (index >= m_queue.size())
            {
                index = m_queue.size() - 1;
            }

            if (index < 0)
            {
                index = 0;
            }

            auto iter = m_queue.rbegin();
            for (int i = 0; i != index; ++i)
            {
                ++iter;
            }
            val = iter->second;

            assert(!m_queue.empty());

            m_queue.erase(--iter.base(), m_queue.end());

            return true;
        }

        bool pop_by_given_time_ref(uint32_t timeRef, VAL &val)
        {
            uint32_t now_ms = getNowMs();

            std::unique_lock<std::mutex> lockGuard(m_mutex);

            incrPopAll();

            if (m_queue.empty())
            {
                incrPopNothing();
                return false;
            }

            uint32_t fixedTime = timeRef;
            if (fixedTime <= m_preTimeRef)
            {
                if (m_prePopTimeMs == 0)
                {
                    fixedTime = 0;
                }
                else
                {
                    fixedTime = m_preTimeRef + now_ms - m_prePopTimeMs;
                }
            }

            chooseFrame(fixedTime, val);

            m_prePopTimeMs = now_ms;
            m_preTimeRef = timeRef;

            return true;
        }

        void chooseFrame(uint32_t fixedTime, VAL &val)
        {
            if (fixedTime == 0)
            {
#if 0
            auto iter = m_queue.begin();
            for (size_t i = 0; i < m_queue.size() / 2; ++i)
            {
                ++iter;
            }

            val = iter->second;
            m_queue.erase(m_queue.begin(), iter);
#else
                auto iter = m_queue.rbegin();
                val = iter->second;
                m_queue.clear();
#endif

                incrPopTail();
            }
            else
            {
                auto iter = m_queue.upper_bound(fixedTime);

                if (iter == m_queue.end())
                {
                    incrPopTail();
                    val = m_queue.rbegin()->second;
                    m_queue.clear();
                }
                else
                {
                    incrPopNormal();
                    val = iter->second;
                    m_queue.erase(m_queue.begin(), iter);
                }
            }
        }

    private:
        void incrPushAll();
        void incrPushFailed();
        void incrPushSuccess();
        void incrPopAll();
        void incrPopNormal();
        void incrPopTail();
        void incrPopNothing();
        void incrQueueSize();

    private:
        std::string m_name;
        std::map<uint32_t, VAL> m_queue;
        std::mutex m_mutex;
        std::condition_variable m_cond;

        bool m_needReport;

        uint32_t m_prePushKey;
        uint32_t m_prePopTimeMs;
        uint32_t m_preTimeRef;
        size_t m_maxSize;

        uint32_t m_pushRollBackCount;

        uint32_t m_maxKey;

        CycleCounterStat<1000> m_pushAllCounterStat;
        CycleCounterStat<1000> m_pushSuccessCounterStat;
        CycleCounterStat<1000> m_pushFailedCounterStat;

        CycleCounterStat<1000> m_popAllCounterStat;
        CycleCounterStat<1000> m_popNormalCounterStat;
        CycleCounterStat<1000> m_popTailCounterStat;
        CycleCounterStat<1000> m_popNothingCounterStat;

        CycleCounterStat<1000> m_queueSizeCounterStat;
    };

    template <typename T>
    void Queue<T>::incrPushAll()
    {
        if (!m_needReport)
            return;

        if (m_pushAllCounterStat.incr(1))
        {
            auto dimension = Property::getDimensionMap();
            auto traceInfos = Property::getTraceInfos();

            for (const auto &traceInfo : traceInfos)
            {
                dimension["traceUid"] = std::to_string(traceInfo.m_traceUid);
                dimension["traceStreamName"] = traceInfo.m_traceStreamName;
            }
        }
    }

    template <typename T>
    void Queue<T>::incrPushSuccess()
    {
        if (!m_needReport)
            return;

        if (m_pushSuccessCounterStat.incr(1))
        {
            auto dimension = Property::getDimensionMap();
            auto traceInfos = Property::getTraceInfos();

            for (const auto &traceInfo : traceInfos)
            {
                dimension["traceUid"] = std::to_string(traceInfo.m_traceUid);
                dimension["traceStreamName"] = traceInfo.m_traceStreamName;
            }
        }
    }

    template <typename T>
    void Queue<T>::incrPushFailed()
    {
        if (!m_needReport)
            return;

        if (m_pushFailedCounterStat.incr(1))
        {
            auto dimension = Property::getDimensionMap();
            auto traceInfos = Property::getTraceInfos();

            for (const auto &traceInfo : traceInfos)
            {
                dimension["traceUid"] = std::to_string(traceInfo.m_traceUid);
                dimension["traceStreamName"] = traceInfo.m_traceStreamName;
            }
        }
    }

    template <typename T>
    void Queue<T>::incrPopAll()
    {
        if (!m_needReport)
            return;

        if (m_popAllCounterStat.incr(1))
        {
            auto dimension = Property::getDimensionMap();
            auto traceInfos = Property::getTraceInfos();

            for (const auto &traceInfo : traceInfos)
            {
                dimension["traceUid"] = std::to_string(traceInfo.m_traceUid);
                dimension["traceStreamName"] = traceInfo.m_traceStreamName;
            }
        }
    }

    template <typename T>
    void Queue<T>::incrPopNormal()
    {
        if (!m_needReport)
            return;

        if (m_popNormalCounterStat.incr(1))
        {
            auto dimension = Property::getDimensionMap();
            auto traceInfos = Property::getTraceInfos();

            for (const auto &traceInfo : traceInfos)
            {
                dimension["traceUid"] = std::to_string(traceInfo.m_traceUid);
                dimension["traceStreamName"] = traceInfo.m_traceStreamName;
            }
        }
    }

    template <typename T>
    void Queue<T>::incrPopTail()
    {
        if (!m_needReport)
            return;

        if (m_popTailCounterStat.incr(1))
        {
            auto dimension = Property::getDimensionMap();
            auto traceInfos = Property::getTraceInfos();

            for (const auto &traceInfo : traceInfos)
            {
                dimension["traceUid"] = std::to_string(traceInfo.m_traceUid);
                dimension["traceStreamName"] = traceInfo.m_traceStreamName;
            }
        }
    }

    template <typename T>
    void Queue<T>::incrPopNothing()
    {
        if (!m_needReport)
            return;

        if (m_popNothingCounterStat.incr(1))
        {
            auto dimension = Property::getDimensionMap();
            auto traceInfos = Property::getTraceInfos();

            for (const auto &traceInfo : traceInfos)
            {
                dimension["traceUid"] = std::to_string(traceInfo.m_traceUid);
                dimension["traceStreamName"] = traceInfo.m_traceStreamName;
            }
        }
    }

    template <typename T>
    void Queue<T>::incrQueueSize()
    {
        if (!m_needReport)
            return;

        if (m_queueSizeCounterStat.incr(1))
        {
            auto dimension = Property::getDimensionMap();
            auto traceInfos = Property::getTraceInfos();

            for (const auto &traceInfo : traceInfos)
            {
                dimension["traceUid"] = std::to_string(traceInfo.m_traceUid);
                dimension["traceStreamName"] = traceInfo.m_traceStreamName;
            }
        }
    }
} // namespace hercules
