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

#include <stdint.h>

#include <mutex>
#include <string>
#include <set>
#include <map>

namespace hercules
{

    class Property
    {
    public:
        struct TraceInfo
        {
            TraceInfo()
                : m_jobKey("")
                , m_traceUid(0)
                , m_traceStreamName("")
            {
            }

            TraceInfo(const std::string &jobKey, uint64_t uid, const std::string &streamName)
                : m_jobKey(jobKey), m_traceUid(uid), m_traceStreamName(streamName)
            {
            }

            std::string m_jobKey;
            uint64_t m_traceUid;
            std::string m_traceStreamName;

            bool operator==(const TraceInfo &rhs) const
            {
                return m_jobKey == rhs.m_jobKey &&
                       m_traceUid == rhs.m_traceUid &&
                       m_traceStreamName == rhs.m_traceStreamName;
            }

            bool operator<(const TraceInfo &rhs) const
            {
                return m_jobKey < rhs.m_jobKey ||
                       m_traceUid < rhs.m_traceUid ||
                       m_traceStreamName < rhs.m_traceStreamName;
            }
        };

        Property()
            : m_uid(UINT64_MAX)
            , m_appid(UINT32_MAX)
            , m_streamName("")
            , m_propertyMutex()
        {
        }

        Property(const Property &rhs)
        {
            m_uid = rhs.m_uid;
            m_appid = rhs.m_appid;
            m_streamName = rhs.m_streamName;
            m_traceInfos = rhs.m_traceInfos;
        }

        virtual ~Property() {}

        std::map<std::string, std::string> getDimensionMap()
        {
            return {{"anchorUid", std::to_string(m_uid)},{"streamName", m_streamName},};
        }

        void setTraceInfos(const std::set<TraceInfo> &traceInfos)
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            m_traceInfos = traceInfos;
        }

        std::set<TraceInfo> getTraceInfos()
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            return m_traceInfos;
        };

        void setUid(uint64_t uid)
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            m_uid = uid;
        }
        uint64_t getUid()
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            return m_uid;
        }

        void setAppid(uint32_t appid) { m_appid = appid; }
        uint32_t getAppid() const { return m_appid; }

        void setStreamName(const std::string &streamName)
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            m_streamName = streamName;
        }

        std::string getStreamName()
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            return m_streamName;
        }

        void addTraceInfo(const TraceInfo &traceInfo)
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            m_traceInfos.insert(traceInfo);
        }
        void addTraceInfo(const std::string &jobKey, uint64_t traceUid,
                          const std::string &traceStreamName)
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            m_traceInfos.insert({jobKey, traceUid, traceStreamName});
        }

        void removeTraceInfo(const TraceInfo &traceInfo)
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            m_traceInfos.erase(traceInfo);
        }
        void removeTraceInfo(const std::string &jobKey, uint64_t traceUid,
                             const std::string &traceStreamName)
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            m_traceInfos.erase({jobKey, traceUid, traceStreamName});
        }

        virtual std::string traceInfo()
        {
            std::unique_lock<std::mutex> lockGuard(m_propertyMutex);
            return traceInfoWithLock();
        }

        virtual std::string traceInfoWithLock() const
        {
            std::string ret = "stream:" + m_streamName +
                              ",appid:" + std::to_string(m_appid) +
                              ",uid:" + std::to_string(m_uid) +
                              ",traceInfo:{";

            int i = 0;
            for (const auto &traceInfo : m_traceInfos)
            {
                if (i != 0)
                {
                    ret += ",";
                }

                ret += "[jobKey:" + traceInfo.m_jobKey +
                       ",traceUid:" + std::to_string(traceInfo.m_traceUid) +
                       ",traceStreamName:" + traceInfo.m_traceStreamName + "]";
            }

            ret += "}";

            return ret;
        }

    protected:
        uint64_t m_uid;
        uint32_t m_appid;
        std::string m_streamName;

        std::mutex m_propertyMutex;
        std::set<TraceInfo> m_traceInfos;
    };

} // namespace hercules
