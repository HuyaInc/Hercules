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

#include "TimeUse.h"
#include "Common.h"
#include "Util.h"
#include "Log.h"

#include "luabind/adopt_policy.hpp"
#include "luabind/luabind.hpp"
#include "luabind/function.hpp"
#include "luabind/class.hpp"
#include "luabind/lua_include.hpp"
#include "opencv2/opencv.hpp"
#include "json/json.h"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <string>

struct lua_State;

namespace hercules
{

    class Lua
    {
    public:
        Lua();
        ~Lua();

        void bindHelpFunction();
        void bindCommon();
        void bindQueue();
        void bindStream();
        void bindOpenCV();
        void bindEncoder();
        void bindAudioEncoder();
        void bindDecoder();
        void bindAudioMixer();
        void bindSubscribeContext();
        void bindFFmpegAudioMixer();

        void initScript(const std::string &script);
        void reloadScript();

        int start(const std::string &jobKey, const std::string &name, const std::string &sJson);
        int stop();
        int update(const std::string &jobKey, const std::string &name, const std::string &sJson);

        int process();

        int gc(int what, int data) { return lua_gc(L, what, data); }

        luabind::object parseJson(const std::string &);
        luabind::object processJson(Json::Value &tValue);

        std::string getLuaFileName()
        {
            return m_script;
        }

        template <class... T>
        int doFunc(const std::string &sFunc, T... args)
        {
            try
            {
                reloadScript();
                luabind::call_function<void>(L, sFunc.c_str(), args...);
            }
            catch (luabind::error &ex)
            {
                luabindErrorHandler(L);

                return -1;
            }
            catch (std::exception &ex)
            {
                luabindErrorHandler(L);

                return -1;
            }
            return 0;
        }

        int getFileLastModTime(time_t &mtime)
        {
            struct stat f_stat;
            int ret = stat(m_script.c_str(), &f_stat);
            mtime = f_stat.st_mtime;

            return ret;
        }

        int luabindErrorHandler(lua_State *L);

    private:
        lua_State *L;
        std::string m_script;
        time_t m_lastModTime;

        std::string m_jobKey;
    };

} // namespace hercules
