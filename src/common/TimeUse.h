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
#include "Log.h"

#include <string>
namespace hercules
{

    class TimeUse
    {
    public:
        explicit TimeUse(const std::string &name, int overLimitTimeuse = 5)
            : m_function(name), m_overLimitTimeUse(overLimitTimeuse)
        {
            m_start = getNowMs();
        }

        TimeUse()
        {
            m_start = getNowMs();
        }

        ~TimeUse()
        {
            m_end = getNowMs();
            int64_t diff = m_end - m_start;

            if (diff >= m_overLimitTimeUse)
            {
                logInfo(MIXLOG << "timeuse: " << diff << ", ms: " << m_function);
            }
        }

        int64_t trackTime()
        {
            int64_t lTimeNow = getNowMs();
            return lTimeNow - m_start;
        }

        void setFuncName(const std::string &sname)
        {
            m_function = sname;
        }

    protected:
        int64_t m_start;
        int64_t m_end;
        std::string m_function;
        int m_overLimitTimeUse;
    };

    class TimeTrace : public TimeUse
    {
    public:
        TimeTrace()
        {
            m_function = "";
            m_overLimitTimeUse = 0;
        }
    };

} // namespace hercules
