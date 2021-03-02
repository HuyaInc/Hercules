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

#include "MediaFrame.h"
#include "Log.h"
#include "Common.h"

extern "C"
{
#include "libavutil/atomic.h"
#include "libavutil/buffer.h"
#include "libavutil/buffer_internal.h"
}

#include <algorithm>

namespace hercules
{

    std::atomic<uint64_t> MediaFrame::m_avframeRefCount(0);
    std::atomic<uint64_t> MediaFrame::m_avframeSize(0);

    MediaFrame::MediaFrame() 
        : MediaBase()
        , m_width(0)
        , m_height(0)
        , m_size(0)
        , m_frame(nullptr)
        , m_ref(nullptr)
        , m_frameBuffer(nullptr)
        , m_isAlpha(false)
        , m_alphaMat()
        , m_alphaRate(1.0f)
        , m_isTempLayer(false)
        , m_isTransparentLayer(false)
    {
    }

    void MediaFrame::setAlphaRate(double rate)
    {
        if (!isAlpha())
        {
            logInfo(MIXLOG << "is not alpha frame");
            return;
        }

        if (rate > 1)
            return;

        m_alphaRate = rate;
    }

    void MediaFrame::setTempLayer(bool bTempLayer)
    {
        m_isTempLayer = bTempLayer;

        if (bTempLayer)
        {
            m_isAlpha = false;
        }
    }

    MediaFrame::~MediaFrame()
    {
        destroy();
    }

    void MediaFrame::copy(const MediaFrame &rhs)
    {
        MediaBase::copy(rhs);

        m_width = rhs.m_width;
        m_height = rhs.m_height;
        m_size = rhs.m_size;
        m_frame = rhs.m_frame;
        m_ref = rhs.m_ref;
        m_frameBuffer = rhs.m_frameBuffer;
        m_isAlpha = rhs.m_isAlpha;
        if (m_isAlpha)
        {
            rhs.m_alphaMat.copyTo(m_alphaMat);
        }
        ref();
    }

    void MediaFrame::destroy()
    {
        MediaBase::destroy();

        if (unref() == 1)
        {
            if (m_frameBuffer != nullptr)
            {
                av_free(m_frameBuffer);
            }

            if (m_isAlpha)
            {
                m_alphaMat.release();
            }

            av_frame_free(&m_frame);

            delete m_ref;
            m_ref = nullptr;

            m_avframeRefCount.fetch_add(-1);
            m_avframeSize.fetch_add(-m_size);
        }
    }

    MediaFrame::MediaFrame(const MediaFrame &rhs)
    {
        if (this == &rhs)
        {
            return;
        }

        copy(rhs);
    }

    MediaFrame &MediaFrame::operator=(const MediaFrame &rhs)
    {
        if (this == &rhs)
        {
            return *this;
        }

        destroy();
        copy(rhs);

        return *this;
    }

    void MediaFrame::setTransparentLayer(bool b)
    {
        if (b)
        {
            if (m_isTempLayer)
            {
                logErr(MIXLOG << "error, is already temp layer");
                return;
            }

            m_isTransparentLayer = true;
            m_isAlpha = true;
            m_alphaMat = cv::Mat(getHeight(), getWidth(), CV_32FC1);
            m_alphaMat.setTo(0);
        }
        else
        {
            m_isTransparentLayer = false;
        }
    }

} // namespace hercules
