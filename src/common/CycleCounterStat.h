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

#include <math.h>

#include <string>
#include <vector>
#include <deque>
#include <utility>

namespace hercules
{

    template <typename T>
    static double calAvg(const std::vector<T> &input)
    {
        double ret = 0;
        if (input.empty())
        {
            return ret;
        }

        double sum = 0;
        for (const auto &i : input)
        {
            sum += i;
        }

        return sum / input.size();
    }

    template <typename T>
    static double calVar(const std::vector<T> &input)
    {
        double ret = 0;
        if (input.empty())
        {
            return ret;
        }

        double avg = calAvg(input);

        for (const auto &i : input)
        {
            double diff = i - avg;
            if (i < avg)
            {
                diff = avg - i;
            }

            ret += (diff * diff);
        }

        return ret;
    }

    template <typename T>
    static double calStdDev(const std::vector<T> &input)
    {
        return sqrt(calVar(input));
    }

    struct DataStat
    {
        DataStat()
            : avg(0.0), dev(0.0)
        {
        }

        double avg;
        double dev;
    };

    template <int CYCLE_IN_MS = 1000>
    class CycleCounterStat
    {
    public:
        CycleCounterStat()
            : m_preCalculateTimeInMs(getNowMs())
            , m_preIncrTimeInMs(getNowMs())
            , m_count(0)
            , m_allCount(0)
        {
        }

        ~CycleCounterStat()
        {
        }

        void setStreamName(const std::string &streamName) { m_streamName = streamName; }
        std::string getStreamName() const { return m_streamName; }
        uint64_t getAllCount() const { return m_allCount; }

        bool incr(uint64_t delta)
        {
            uint64_t nowMs = getNowMs();

            m_preIncrTimeInMs = nowMs;
            m_count += delta;
            m_allCount += delta;

            if (nowMs - m_preCalculateTimeInMs >= CYCLE_IN_MS)
            {
                m_count = 0.0;
                m_preCalculateTimeInMs = nowMs;

                return true;
            }

            return false;
        }

        void resetCount(uint64_t count) { m_count = count; }
        void resetAllCount(uint64_t allCount) { m_allCount = allCount; }

        void saveHistory(uint64_t time, uint64_t val)
        {
            m_historyVal.push_back(std::make_pair(time, val));

            if (m_historyVal.size() >= 60)
            {
                m_historyVal.pop_front();
            }
        }

        bool stat(DataStat &stat)
        {
            if (m_historyVal.empty())
            {
                return false;
            }

            std::vector<uint64_t> input;
            input.reserve(m_historyVal.size());
            for (const auto &val : m_historyVal)
            {
                input.push_back(val.second);
            }

            stat.avg = calAvg(input);
            stat.dev = calStdDev(input);

            return true;
        }

        const uint64_t getElapseMsBetweenIncr() const
        {
            return getNowMs() - m_preIncrTimeInMs;
        }

    private:
        std::string m_streamName;
        uint64_t m_preIncrTimeInMs;
        uint64_t m_preCalculateTimeInMs;
        uint64_t m_count;
        uint64_t m_allCount;

        std::deque<std::pair<uint64_t, uint64_t>> m_historyVal;
    };

} // namespace hercules
