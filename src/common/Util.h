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

#include "Log.h"

#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <sstream>
#include <deque>
#include <map>
#include <time.h>

namespace hercules
{

    constexpr int ONE_MIN = 60;

    std::string bin2str(const void *buf, size_t len, const std::string &sSep = "", size_t lines = 0);

    std::string bin2str(const std::string &sBinData, const std::string &sSep = "", size_t lines = 0);

    inline bool isFileExist(const std::string &file)
    {
        struct stat sb;
        return lstat(file.c_str(), &sb) != -1;
    }

    struct RGBA
    {
        RGBA() : r(0), g(0), b(0), a(0)
        {
        }
        uint8_t r;
        uint8_t g;
        uint8_t b;
        float a;
    };

    // #ffffffff
    inline RGBA getRGBA(const std::string &color)
    {
        RGBA rgba;
        if (color.size() < 9)
        {
            return rgba;
        }
        const char *str = color.c_str();
        uint32_t tmp = strtol(str + 1, nullptr, 16) & 0xffffffff;
        rgba.r = tmp >> 24;
        rgba.g = (tmp >> 16) & 0xff;
        rgba.b = (tmp >> 8) & 0xff;
        rgba.a = static_cast<float>(tmp & 0xff) / 255;
        return rgba;
    }

    // magic num from wiki
    inline uint8_t rgb2y(uint8_t r, uint8_t g, uint8_t b)
    {
        return 0.299 * r + 0.587 * g + 0.114 * b;
    }

    inline uint8_t rgb2u(uint8_t r, uint8_t g, uint8_t b)
    {
        return -0.1687 * r - 0.3313 * g + 0.5 * b + 128;
    }

    inline uint8_t rgb2v(uint8_t r, uint8_t g, uint8_t b)
    {
        return 0.5 * r - 0.4187 * g - 0.0813 * b + 128;
    }

    inline std::string getCWD()
    {
        char *cwd = get_current_dir_name();

        std::string ret = ".";
        if (cwd != nullptr)
        {
            ret.assign(cwd);
            free(cwd);
        }

        return ret;
    }

    inline uint64_t getNow()
    {
        timeval tv;
        gettimeofday(&tv, nullptr);

        return tv.tv_sec;
    }

    inline uint64_t getNowMs()
    {
        timeval tv;
        gettimeofday(&tv, nullptr);

        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }

    inline uint64_t getNowUs()
    {
        timeval tv;
        gettimeofday(&tv, NULL);

        return tv.tv_sec * 1000000 + tv.tv_usec;
    }

    inline uint64_t clockGetNowMs()
    {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        return now.tv_nsec / 1000000 + now.tv_sec * 1000;
    }

    inline std::string getNowStr()
    {
        char timeStr[128];
        time_t now = getNow();
        struct tm tt;
        localtime_r(&now, &tt);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tt);

        return std::string(timeStr);
    }

    inline uint64_t clockGetNs()
    {
        struct timespec tv;
        clock_gettime(CLOCK_REALTIME, &tv);

        return tv.tv_sec * 1000000000UL + tv.tv_nsec;
    }

    inline double clockGetMs()
    {
        uint64_t ns = clockGetNs();

        return static_cast<double>(ns) / 1000000.0;
    }

    inline uint32_t getNowMs32()
    {
        return (uint32_t)getNowMs();
    }

    class TimestampAdjuster
    {
    public:
        TimestampAdjuster()
            : m_dtsBase(0),
              m_timeMsBase(0)
        {
        }

        void setName(const std::string &name)
        {
            m_name = name;
        }

        ~TimestampAdjuster()
        {
        }

        uint32_t getDts(uint32_t dts, bool updateDtsBase = false)
        {
            bool log = false;

            if (m_timeMsBase == 0 || m_dtsBase == 0 || updateDtsBase)
            {
                m_timeMsBase = getNowMs() - m_serverStartTimeMs;
                m_dtsBase = dts;
                log = true;
            }

            if (dts <= m_dtsBase)
            {
                log = true;
            }

            uint32_t fixDts = dts - m_dtsBase + m_timeMsBase;

            if (log)
            {
                logDebug(MIXLOG << "TimestampAdjuster"
                    << ", name: " << m_name 
                    << ", m_serverStartTimeMs: " << m_serverStartTimeMs 
                    << ", updateDtsBase: " << updateDtsBase 
                    << ", dts:"  << dts << ", time base: " << m_timeMsBase 
                    << ", m_dtsBase: " << m_dtsBase 
                    << ", fix: " << fixDts);
            }

            return fixDts;
        }

        static uint32_t getElapseFromServerStart()
        {
            return getNowMs() - m_serverStartTimeMs;
        }

        static uint32_t m_serverStartTimeMs;

    private:
        std::string m_name;
        uint32_t m_dtsBase;
        uint32_t m_timeMsBase;
    };

    class ScopeLog
    {
    public:
        ScopeLog() {}
        ~ScopeLog()
        {
            logInfo(MIXLOG << os.str());
        }

        template <typename T>
        ScopeLog &operator<<(const T &t)
        {
            os << t;
            return *this;
        }

    private:
        std::ostringstream os;
    };

    class IdTimestampTrace
    {
    public:
        IdTimestampTrace()
        {
        }

        void push(const std::string &type, uint32_t dts,
                  const std::map<uint64_t, uint32_t> &idTime)
        {
            char buf[1024] = {0};

            int nbytes = 0;
            nbytes = snprintf(buf + nbytes, sizeof(buf),
                              "type:%s|dts:%u, src id time:{", type.c_str(), dts);

            for (const auto &kv : idTime)
            {
                nbytes += snprintf(buf + nbytes, sizeof(buf) - nbytes,
                                   "(%lu:%u) ", kv.first, kv.second);
            }

            nbytes += snprintf(buf + nbytes, sizeof(buf) - nbytes, "}");

            m_traceQueue.emplace_back(buf);
        }

        void logPrintAndClean(const std::string &prefix)
        {
            int count = 0;
            for (const auto &item : m_traceQueue)
            {
                if (++count > ONE_MIN)
                {
                    break;
                }

                logInfo(MIXLOG << prefix << "|" << item);
            }

            m_traceQueue.clear();
        }

    private:
        std::deque<std::string> m_traceQueue;
    };

    template <typename T>
    T rangeLimit(const T &val, const T &l, const T &r)
    {
        if (val < l)
        {
            return l;
        }

        if (val > r)
        {
            return r;
        }

        return val;
    }

#define return_if(r, ...)                                                                        \
    if (r)                                                                                       \
    {                                                                                            \
        logErr(MIXLOG << __VA_ARGS__);                                                           \
        logErr(MIXLOG << __FILE__ << " " << __LINE__                                             \
                      << " error no:" << errno << " error msg:" << strerror(errno));             \
        return -1;                                                                               \
    }

    inline void sleepms(int ms)
    {
        timeval timeout;
        timeout.tv_sec = ms / 1000;
        timeout.tv_usec = (ms - timeout.tv_sec * 1000) * 1000;
        /* cout << "sleep for " << ms << "ms" << endl; */
        select(0, nullptr, nullptr, nullptr, &timeout);
    }

    inline std::string dump(const void *buffer, size_t len)
    {
        std::string sHexStr = bin2str(buffer, len, " ", 30);
        return sHexStr;
    }

} // namespace hercules
