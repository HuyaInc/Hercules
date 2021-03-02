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
#include <string>
#include <sstream>
#include <functional>
#include <iostream>

namespace hercules
{
    enum LogLevel
    {
        LOG_LEVEL_DEBUG,
        LOG_LEVEL_INFO,
        LOG_LEVEL_WARNING,
        LOG_LEVEL_ERROR
    };

    typedef std::function<void(const std::string &)> LogCallback;

    void initLog(LogLevel level = LOG_LEVEL_INFO);
    void mixlog(const std::string &str);
    void setLogCb(const LogCallback &cb);

    class MixLog
    {
    public:
        MixLog() {}
        std::stringstream m_ss;
        template <class T>
        MixLog &operator<<(const T &t)
        {
            m_ss << t;
            return *this;
        };
        std::string str() const
        {
            return m_ss.str();
        }
    };

    void mixlog(const MixLog &ss, LogLevel logLevel = LOG_LEVEL_INFO);
    void logDebug(const MixLog& ss);
    void logInfo(const MixLog& ss);
    void logWarn(const MixLog& ss);
    void logErr(const MixLog& ss);

#define MIXLOG MixLog() << __FILE__ << "#" << __LINE__ << ":" << __FUNCTION__ << " "

} // namespace hercules
