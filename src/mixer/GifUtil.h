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

#include "MediaFrame.h"
#include "ThreadPool.h"
#include "Singleton.h"
#include "Common.h"

#include "opencv2/opencv.hpp"

#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <set>

namespace hercules
{

    struct GifProperty
    {
        GifProperty() : m_delay(100), m_idx(0), m_mixCount(0) {}
        int m_imgCount;
        int m_width;
        int m_height;
        uint32_t m_delay;

        uint32_t m_idx;
        uint32_t m_mixCount;
    };

    template <typename T>
    struct GifContext
    {
        bool isValid() const
        {
            return m_imgs.size() > 0;
        }
        GifProperty m_property;
        std::vector<T> m_imgs;
    };

    typedef GifContext<cv::Mat> GifCVContext;
    typedef GifContext<MediaFrame> GifFrameContext;

    bool isGif(const std::string &filename);

    class GifUtil : public Singleton<GifUtil>
    {
    public:
        GifUtil() {}
        ~GifUtil() {}

        bool parseGif(const std::string &filename, GifCVContext &ctx);

        bool loadGif(GifFrameContext &ctx, const std::string &filename, bool reload = false);

        void loadGif(GifCVContext &cvCtx, GifFrameContext &frameCtx);

        // filename => MediaFrame
        std::map<std::string, GifFrameContext> m_gifFrames;
        std::set<std::string> m_loadTask;

        std::mutex m_gifMutex;
        std::mutex m_taskMutex;
    };

} // namespace hercules
