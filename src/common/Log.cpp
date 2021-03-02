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

#include "Log.h"
#include "Util.h"

extern "C"
{
#include "libavutil/avutil.h"
}

#include <mutex>
#include <string>

namespace hercules
{

    constexpr int FFMPEG_LOG_BUFFER_SIZE = 10240;
    static LogLevel g_logLevel = LOG_LEVEL_INFO;

    void defaultLog(const std::string &str)
    {
    }

    static LogCallback logCb = defaultLog;
    static std::mutex logMutex;

    static void ffmpegLog(void *ptr, int level, const char *fmt, va_list vl)
    {
        char buf[FFMPEG_LOG_BUFFER_SIZE] = {0};
        int nbytes = vsnprintf(buf, sizeof(buf), fmt, vl);
        mixlog(MIXLOG << std::string("[ffmpeg]") << buf);
    }

    void mixlog(const std::string &str)
    {
        if (logCb)
        {
            std::unique_lock<std::mutex> lock(logMutex);
            logCb(std::string("[mixsdk]") + str);
        }
    }
    
    static std::string logLevelToStr(LogLevel logLevel)
    {
        switch(logLevel)
        {
            case LOG_LEVEL_DEBUG:
                return "[debug]";
            case LOG_LEVEL_INFO:
                return "[info]";
            case LOG_LEVEL_WARNING:
                return "[warn]";
            case LOG_LEVEL_ERROR:
                return "[err]";
            default:
                return "[unknown]";
        }
    }

    void mixlog(const MixLog &ss, LogLevel logLevel)
    {
        if(logLevel >= g_logLevel && logCb)
        {
            mixlog(logLevelToStr(logLevel) + ss.str());
        }
    }

    void logDebug(const MixLog& ss)
    {
        mixlog(ss, LOG_LEVEL_DEBUG);
    }
    void logInfo(const MixLog& ss)
    {
        mixlog(ss, LOG_LEVEL_INFO);
    }
    void logWarn(const MixLog& ss)
    {
        mixlog(ss, LOG_LEVEL_WARNING);
    }
    void logErr(const MixLog& ss)
    {
        mixlog(ss, LOG_LEVEL_ERROR);
    }

    void setLogCb(const LogCallback &cb)
    {
        if (cb)
        {
            logCb = cb;
        }
    }

    void initLog(LogLevel logLevel)
    {
        av_log_set_callback(ffmpegLog);
        g_logLevel = logLevel;
        int ffmpegLogLevel = AV_LOG_INFO;
        switch(logLevel)
        {
            case LOG_LEVEL_DEBUG:
                ffmpegLogLevel = AV_LOG_DEBUG;
            break;
                ffmpegLogLevel = AV_LOG_INFO;
            break;
            case LOG_LEVEL_WARNING:
                ffmpegLogLevel = AV_LOG_WARNING;
                break;
            case LOG_LEVEL_ERROR:
                ffmpegLogLevel = AV_LOG_ERROR;
                break;
            default:
                break;
        }
        av_log_set_level(ffmpegLogLevel);
    }
}// namespace hercules
