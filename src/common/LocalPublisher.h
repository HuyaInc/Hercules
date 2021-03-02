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

#include "OneCycleThread.h"
#include "Queue.h"
#include "srs_librtmp.hpp"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <string.h>
#include <iostream>
#include "Streamer.h"

namespace hercules
{

    class MediaPacket;
    class MediaFrame;

    class LocalPublisher : public OneCycleThread, public Streamer
    {
    public:
        explicit LocalPublisher(const std::string &taskId) 
            : OneCycleThread()
            , m_taskId(taskId)
        {
        }

        ~LocalPublisher();

        bool getVideoPacket(MediaPacket &);
        bool getAudioPacket(MediaPacket &);
        void pushAudioPacket(const MediaPacket &packet);

        void start()
        {
            connect();
            OneCycleThread::startThread();
        }

        void stop()
        {
            OneCycleThread::stopThread();
        }

        void join()
        {
            OneCycleThread::joinThread();
        }

    protected:
        void threadEntry();

        int connect();
        int sendVideo();
        int sendAudio();

        std::string m_taskId;
    };

} // namespace hercules
