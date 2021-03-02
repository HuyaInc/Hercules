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

#include "AudioEncoder.h"
#include "Common.h"
#include "Decoder.h"
#include "Encoder.h"
#include "FFmpegAudioMixer.h"
#include "Job.h"
#include "Log.h"
#include "Lua.h"
#include "MediaFrame.h"
#include "MediaPacket.h"
#include "OpenCVOperator.h"
#include "Property.h"
#include "PublisherWrapper.h"
#include "TimeUse.h"

#include "json/json.h"
#include "opencv2/imgcodecs.hpp"

#include <pthread.h>
#include <string>
#include <vector>

using namespace luabind;
using namespace cv;
using namespace Json;
using std::string;

namespace luabind
{

    template <typename ListType>
    struct default_converter<std::vector<ListType>> : native_converter_base<std::vector<ListType>>
    {
        static int compute_score(lua_State *L, int index)
        {
            return lua_type(L, index) == LUA_TTABLE ? 0 : -1;
        }

        std::vector<ListType> from(lua_State *L, int index)
        {
            std::vector<ListType> list;

            for (luabind::iterator i(luabind::object(luabind::from_stack(L, index))),
                end; i != end; ++i)
            {
                list.push_back(luabind::object_cast<ListType>(*i));
            }

            return list;
        }

        void to(lua_State *L, const std::vector<ListType> &l)
        {
            luabind::object list = luabind::newtable(L);

            for (size_t i = 0; i < l.size(); ++i)
            {
                list[i + 1] = l[i];
            }

            list.push(L);
        }
    };

} // namespace luabind

namespace hercules
{

    typedef Queue<MediaFrame> MediaFrameQueue;
    typedef Queue<MediaPacket> MediaPacketQueue;

    // -------- Help function --------

    void doInfoLog(const std::string &word)
    {
        logInfo(MIXLOG << "[LUA] # " << word);
    }

    void doDebugLog(const std::string &word)
    {
        logDebug(MIXLOG << "[LUA] # " << word);
    }

    void doWarnLog(const std::string &word)
    {
        logWarn(MIXLOG << "[LUA] # " << word);
    }

    void doErrorLog(const std::string &word)
    {
        logErr(MIXLOG << "error:"
                      << "[LUA] # " << word);
    }

    void doLog(const std::string &word)
    {
        logInfo(MIXLOG << "[LUA] # " << word);
    }

    inline uint32_t getRunMs32()
    {
        return TimestampAdjuster::getElapseFromServerStart();
    }

    // ==== STL support ====

    Lua::Lua()
    {
        L = luaL_newstate();
        luaL_openlibs(L);
        luabind::open(L);
        m_lastModTime = -1;
    }

    Lua::~Lua()
    {
        lua_close(L);
    }

    void Lua::bindHelpFunction()
    {
        module(L)
            [
                def("LOG", &doLog),
                def("INFO", &doInfoLog),
                def("DEBUG", &doDebugLog),
                def("WARN", &doWarnLog),
                def("ERROR", &doErrorLog),
                def("getNowMs", &getNowMs),
                def("getNowMs32", &getNowMs32),
                def("getRunMs32", &getRunMs32),
                def("getCWD", &getCWD),
                def("isGif", &isGif)];
    }

    void Lua::bindCommon()
    {
        module(L)
            [class_<Property>("Property")
                 .def(constructor<>())
                 .def("setUid", &Property::setUid)
                 .def("setStreamName", &Property::setStreamName)
                 .def("addTraceInfo", (void (Property::*)(
                    const std::string &, uint64_t, const std::string &)) &
                    Property::addTraceInfo)
                 .def("removeTraceInfo", (void (Property::*)(
                    const std::string &, uint64_t, const std::string &)) &
                    Property::removeTraceInfo)
                 .def("getUid", &Property::getUid)
                 .def("getStreamName", &Property::getStreamName),

             class_<MediaBase>("MediaBase")
                 .def(constructor<>())
                 .def("setDts", &MediaBase::setDts)
                 .def("setPts", &MediaBase::setPts)
                 .def("getDts", &MediaBase::getDts)
                 .def("getPts", &MediaBase::getPts)
                 .def("getStreamId", &MediaBase::getStreamId)
                 .def("setRecvTime", &MediaBase::setRecvTime)
                 .def("getRecvTime", &MediaBase::getRecvTime)
                 .def("asVideo", &MediaBase::asVideo)
                 .def("asAudio", &MediaBase::asAudio)
                 .def("getIdTimestamp", &MediaBase::getIdTimestamp)
                 .def("getInIdTimestamp", &MediaBase::getInIdTimestamp)
                 .def("getIdTimestampStr", &MediaBase::getIdTimestampStr)
                 .def("isVideo", &MediaBase::isVideo)
                 .def("isAudio", &MediaBase::isAudio)
                 .def("isIFrame", &MediaBase::isIFrame)
                 .def("isPFrame", &MediaBase::isPFrame)
                 .def("isBFrame", &MediaBase::isBFrame)
                 .def("isHeaderFrame", &MediaBase::isHeaderFrame)
                 .def("isH264", &MediaBase::isH264)
                 .def("isH265", &MediaBase::isH265)
                 .def("isAAC", &MediaBase::isAAC),

             class_<MediaFrame, bases<MediaBase>>("MediaFrame")
                 .def(constructor<>())
                 .def("getWidth", &MediaFrame::getWidth)
                 .def("getHeight", &MediaFrame::getHeight)
                 .def("setTransparentLayer", &MediaFrame::setTransparentLayer)
                 .def("isTransparentLayer", &MediaFrame::isTransparentLayer)
                 .def("setAlphaRate", &MediaFrame::setAlphaRate)
                 .def("getAlphaRate", &MediaFrame::getAlphaRate),

             class_<MediaPacket, bases<MediaBase>>("MediaPacket")
                 .def(constructor<>())];
    }

    void Lua::bindQueue()
    {
        module(L)
            [class_<MediaFrameQueue, bases<Property>>("MediaFrameQueue")
                 .def(constructor<>())
                 .def("size", &MediaFrameQueue::size)
                 .def("push", &MediaFrameQueue::push)
                 .def("pop", &MediaFrameQueue::pop)
                 .def("pop_tail", &MediaFrameQueue::pop_tail)
                 .def("pop_tail_n", &MediaFrameQueue::pop_tail_n)
                 .def("setName", &MediaFrameQueue::setName)
                 .def("setMaxSize", &MediaFrameQueue::setMaxSize)
                 .def("setNeedReport", &MediaFrameQueue::setNeedReport)
                 .def("pop_by_given_time_ref", &MediaFrameQueue::pop_by_given_time_ref),

             class_<MediaPacketQueue, bases<Property>>("MediaPacketQueue")
                 .def(constructor<>())
                 .def("size", &MediaPacketQueue::size)
                 .def("push", &MediaPacketQueue::push)
                 .def("pop", &MediaPacketQueue::pop)
                 .def("setNeedReport", &MediaPacketQueue::setNeedReport)
                 .def("setMaxSize", &MediaPacketQueue::setMaxSize)];
    }

    void Lua::bindStream()
    {
        module(L)
            [class_<StreamOpt>("StreamOpt")
                 .def(constructor<>())
                 .def_readwrite("m_publishVideo", &StreamOpt::m_publishVideo)
                 .def_readwrite("m_publishAudio", &StreamOpt::m_publishAudio)
                 .def_readwrite("m_publishDataStream", &StreamOpt::m_publishDataStream)
                 .def_readwrite("m_fixUid", &StreamOpt::m_fixUid)
                 .def_readwrite("m_phonyUid", &StreamOpt::m_phonyUid)
                 .def_readwrite("m_smooth", &StreamOpt::m_smooth),

             class_<PublisherWrapper>("PublisherWrapper")
                 .def(constructor<const string &, const string &>())
                 .def("init", &PublisherWrapper::init)
                 .def("start", &PublisherWrapper::start)
                 .def("stop", &PublisherWrapper::stop)
                 .def("join", &PublisherWrapper::join)
                 .def("setUid", &PublisherWrapper::setUid)
                 .def("setStreamName", &PublisherWrapper::setStreamName)
                 .def("getUid", &PublisherWrapper::getUid)
                 .def("getStreamName", &PublisherWrapper::getStreamName)
                 .def("setOpt", &PublisherWrapper::setOpt)
                 .def("setPushParam", &PublisherWrapper::setPushParam)
                 .def("setVideoCodec", &PublisherWrapper::setVideoCodec)
                 .def("setAudioCodec", &PublisherWrapper::setAudioCodec)
                 .def("addTraceInfo", (void (PublisherWrapper::*)(
                    const std::string &, uint64_t, const std::string &)) &
                    PublisherWrapper::addTraceInfo)
                 .def("removeTraceInfo", (void (PublisherWrapper::*)(
                    const std::string &, uint64_t, const std::string &)) &
                    PublisherWrapper::removeTraceInfo)
                 .def("getVideoQueue", &PublisherWrapper::getVideoQueue)
                 .def("getAudioQueue", &PublisherWrapper::getAudioQueue)
        ];
    }

    void Lua::bindOpenCV()
    {
        module(L)
            [class_<cv::Mat>("Mat")
                 .def(constructor<>()),

             class_<cv::Point>("Point")
                 .def(constructor<>())
                 .def(constructor<int, int>())
                 .def_readwrite("x", &Point::x)
                 .def_readwrite("y", &Point::y),

             class_<cv::Size>("Size")
                 .def(constructor<>())
                 .def(constructor<int, int>())
                 .def_readwrite("width", &Size::width)
                 .def_readwrite("height", &Size::height),

             class_<cv::Rect>("Rect")
                 .def(constructor<>())
                 .def(constructor<int, int, int, int>()),

             class_<cv::Scalar>("Scalar")
                 .def(constructor<>())
                 .def(constructor<double, double, double, double>()),

             class_<OpenCVOperator>("OpenCVOperator")
                 .def(constructor<>())
                 .def("addWord", &OpenCVOperator::addWord)
                 .def("addBoldWord", &OpenCVOperator::addBoldWord)
                 .def("addBorderWordYUV", &OpenCVOperator::addBorderWordYUV)
                 .def("createYUV", &OpenCVOperator::createYUV)
                 .def("addYUV", (void (OpenCVOperator::*)(MediaFrame &, MediaFrame &, 
                    cv::Point &, int, int)) & OpenCVOperator::addYUV)
                 .def("addYUV", (void (OpenCVOperator::*)(
                    MediaFrame &, MediaFrame &, cv::Point &, int, int, cv::Rect &)) &
                    OpenCVOperator::addYUV)
                 .def("addYUV", (void (OpenCVOperator::*)(
                    MediaFrame &, MediaFrame &, cv::Point &, int, int, cv::Rect &,
                    const std::vector<cv::Point>, double)) &
                    OpenCVOperator::addYUV)
                 .def("addGif", &OpenCVOperator::addGif)
                 .def("loadLogo", &OpenCVOperator::loadLogo)
                 .def("loadGif", (void (OpenCVOperator::*)(const string &)) & 
                    OpenCVOperator::loadGif)
                 .def("setFontType", &OpenCVOperator::setFontType)
                 .def("getWordSize", &OpenCVOperator::getWordSize)
                 .def("getBorderWordSize", &OpenCVOperator::getBorderWordSize)
                 .def("addLine", &OpenCVOperator::addLine)
                 .def("addCircleStroke", &OpenCVOperator::addCircleStroke)
                 .def("addCircleYUV", &OpenCVOperator::addCircleYUV)
                 .def("addCircleAlphaCustom", &OpenCVOperator::addCircleAlphaCustom)
                 .def("yuvToRgbAndDumpFile", &OpenCVOperator::yuvToRgbAndDumpFile)
                 .def("yuvDump", &OpenCVOperator::yuvDump)
                 .def("setAsTempLayer", &OpenCVOperator::setAsTempLayer)
                 .def("yuvDumpTempLayer", &OpenCVOperator::yuvDumpTempLayer)
                 .def("addPolygonAlphaCustom", &OpenCVOperator::addPolygonAlphaCustom)
                 .def("addWordWithBorder", &OpenCVOperator::addWordWithBorder)
                 .def("getWordWithBorderSize", &OpenCVOperator::getWordWithBorderSize)];
    }

    void Lua::bindEncoder()
    {
        module(L)
            [class_<Encoder, bases<Property>>("Encoder")
                 .def(constructor<>())
                 .def("init", &Encoder::init)
                 .def("start", &Encoder::start)
                 .def("join", &Encoder::join)
                 .def("setCodec", &Encoder::setCodec)
                 .def("setLowLatency", &Encoder::setLowLatency)
                 .def("pushMediaFrame", &Encoder::pushMediaFrame)
                 .def("stop", &Encoder::stop)];
    }

    void Lua::bindAudioEncoder()
    {
        module(L)
            [class_<AudioEncoder, bases<Property>>("AudioEncoder")
                 .def(constructor<>())
                 .def("init", &AudioEncoder::init)
                 .def("start", &AudioEncoder::start)
                 .def("join", &AudioEncoder::join)
                 .def("setCodec", &AudioEncoder::setCodec)
                 .def("setLowLatency", &AudioEncoder::setLowLatency)
                 .def("pushMediaFrame", &AudioEncoder::pushMediaFrame)
                 .def("stop", &AudioEncoder::stop)];
    }

    void Lua::bindDecoder()
    {
        module(L)
            [class_<Decoder, bases<Property>>("Decoder")
                 .def(constructor<>())
                 .def("init", &Decoder::init)
                 .def("start", &Decoder::start)
                 .def("join", &Decoder::join)
                 .def("getNum", &Decoder::getNum)
                 .def("stop", &Decoder::stop)];
    }

    void Lua::bindSubscribeContext()
    {
        module(L)
            [class_<SubscribeContext>("SubscribeContext")
                 .def(constructor<>())
                 .def("subscribeVideoPacket", &SubscribeContext::subscribeVideoPacket)
                 .def("subscribeAudioPacket", &SubscribeContext::subscribeAudioPacket)
                 .def("subscribeVideoFrame", &SubscribeContext::subscribeVideoFrame)
                 .def("subscribeAudioFrame", &SubscribeContext::subscribeAudioFrame)
                 .def("getVideoFrameQueue", &SubscribeContext::getVideoFrameQueue)
                 .def("getAudioFrameQueue", &SubscribeContext::getAudioFrameQueue)
                 .def("subscribeJobFrame", &SubscribeContext::subscribeJobFrame)
        ];
    }

    void Lua::bindFFmpegAudioMixer()
    {
        module(L)
            [class_<FFmpegAudioMixer>("FFmpegAudioMixer")
                 .def(constructor<>())
                 .def("mixFrame", &FFmpegAudioMixer::mixFrame)];
    }

    void Lua::initScript(const string &script)
    {
        m_script = script;

        bindHelpFunction();
        bindCommon();
        bindQueue();
        bindStream();
        bindOpenCV();
        bindEncoder();
        bindAudioEncoder();
        bindDecoder();
        bindSubscribeContext();
        bindFFmpegAudioMixer();

        reloadScript();
    }

    void Lua::reloadScript()
    {
        time_t mtime = 0;
        if (getFileLastModTime(mtime) != 0)
        {
            return;
        }

        if (m_lastModTime != mtime)
        {
            m_lastModTime = mtime;

            luaL_dofile(L, m_script.c_str());
        }
    }

    int Lua::start(const string &jobKey, const string &name, const string &sJson)
    {
        m_jobKey = jobKey;

        luabind::object oTable = parseJson(sJson);
        oTable["job_key"] = jobKey;
        oTable["name"] = name;
        return doFunc("start", oTable);
    }

    int Lua::update(const string &jobKey, const string &name, const string &sJson)
    {
        luabind::object oTable = parseJson(sJson);
        oTable["job_key"] = jobKey;
        oTable["name"] = name;
        return doFunc("update", oTable);
    }

    int Lua::process()
    {
        return doFunc("process");
    }

    int Lua::stop()
    {
        return doFunc("stop");
    }

    luabind::object Lua::processJson(Json::Value &tValue)
    {
        luabind::object cur = newtable(L);
        Json::Value::Members mems = tValue.getMemberNames();
        for (auto itr = mems.begin(); itr != mems.end(); itr++)
        {
            string name = *itr;
            luabind::object element;
            if (tValue[name].type() == Json::objectValue)
            {
                element = processJson(tValue[name]);
                cur[name] = element;
            }
            else if (tValue[name].type() == Json::arrayValue)
            {
                element = newtable(L);
                for (int i = 0; i < tValue[name].size(); i++)
                {
                    element[i + 1] = processJson(tValue[name][i]);
                }
                cur[name] = element;
            }
            else if (tValue[name].isString())
            {
                cur[name] = tValue[name].asString();
            }
            else if (tValue[name].isDouble())
            {
                cur[name] = tValue[name].asDouble();
            }
            else if (tValue[name].isInt())
            {
                cur[name] = tValue[name].asInt();
            }
            else if (tValue[name].isBool())
            {
                cur[name] = tValue[name].asBool();
            }
            else if (tValue[name].asUInt64())
            {
                double doubleVale = tValue[name].asUInt64();
                cur[name] = doubleVale;
            }
            else
            {
            }
        }

        return cur;
    }

    luabind::object Lua::parseJson(const string &sJson)
    {
        Json::Reader tReader;
        Json::Value tResult;
        tReader.parse(sJson, tResult);
        return processJson(tResult);
    }

    int Lua::luabindErrorHandler(lua_State *L)
    {
        luabind::object msg(luabind::from_stack(L, -1));

        std::string traceback = luabind::call_function<std::string>(
            luabind::globals(L)["debug"]["traceback"]);
        traceback = std::string("lua> ") + traceback;

        logErr(MIXLOG << "error: jobKey:" << m_jobKey 
            << " traceback:" << traceback << " err:" << msg);
        return 1;
    }

} // namespace hercules
