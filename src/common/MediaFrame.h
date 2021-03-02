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

#include "MediaBase.h"
#include "opencv2/opencv.hpp"

extern "C"
{
#include "libavcodec/avcodec.h"
}

#include <atomic>
#include <string>
#include <vector>
#include <algorithm>

struct AVFrame;

namespace hercules
{

    class MediaFrame : public MediaBase
    {
    public:
        MediaFrame();
        ~MediaFrame();

        void copy(const MediaFrame &);
        void destroy();

        MediaFrame(const MediaFrame &rhs);
        MediaFrame &operator=(const MediaFrame &rhs);

        AVFrame *getAVFrame() { return m_frame; }

        void setAVFrame(AVFrame *frame, uint8_t *frameBuffer = nullptr, uint64_t size = 0)
        {
            ref();
            m_frame = av_frame_clone(frame);
            av_frame_free(&frame);

            m_frameBuffer = frameBuffer;
            m_size = size;
            m_avframeSize.fetch_add(m_size);
            m_avframeRefCount.fetch_add(1);
        }

        bool isAlpha() const { return m_isAlpha; }
        void setAlpha(cv::Mat &mat)
        {
            if (!m_isTempLayer || !m_isTransparentLayer)
            {
                m_isAlpha = true;
                mat.copyTo(m_alphaMat);
            }
        }

        cv::Mat &getAlpha() { return m_alphaMat; }

        void setAlphaRate(double rate);
        double getAlphaRate() { return m_alphaRate; }

        void setTempLayer(bool bTempLayer);
        bool isTempLayer() const { return m_isTempLayer; }

        void setTransparentLayer(bool b);
        bool isTransparentLayer() { return m_isTransparentLayer; }

        bool hasBuffer() const { return m_frameBuffer != nullptr; }

        uint8_t *getBuffer() const { return m_frameBuffer; }
        uint64_t getSize() const { return m_size; }

        void setWidth(int w) { m_width = w; }
        void setHeight(int h) { m_height = h; }

        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }

        std::string print()
        {
            std::ostringstream os;
            os << MediaBase::print()
               << ",width:" << m_width
               << ",height:" << m_height;

            return os.str();
        }

        std::vector<cv::Mat> getMatVec()
        {
            std::vector<cv::Mat> result;
            int srcH = getHeight(), srcW = getWidth();
            if (hasBuffer())
            {
                uint8_t *srcBuffer = getBuffer();
                result.emplace_back(cv::Mat(srcH, srcW, CV_8UC1, srcBuffer));
                result.emplace_back(cv::Mat(srcH / 2, srcW / 2, CV_8UC1, srcBuffer + srcW * srcH));
                result.emplace_back(cv::Mat(srcH / 2, srcW / 2, CV_8UC1,
                    srcBuffer + srcW * srcH + srcW / 2 * srcH / 2));
            }
            else
            {
                ::AVFrame *srcAVFrame = getAVFrame();
                result.emplace_back(cv::Mat(srcH, srcAVFrame->linesize[0], CV_8UC1,
                    reinterpret_cast<void *>(srcAVFrame->data[0])));
                result.emplace_back(cv::Mat(srcH / 2, srcAVFrame->linesize[1], CV_8UC1,
                    reinterpret_cast<void *>(srcAVFrame->data[1])));
                result.emplace_back(cv::Mat(srcH / 2, srcAVFrame->linesize[2], CV_8UC1,
                    reinterpret_cast<void *>(srcAVFrame->data[2])));
            }
            return result;
        }

    private:
        int ref()
        {
            if (m_ref == nullptr)
            {
                m_ref = new std::atomic<int>(0);
            }

            return m_ref->fetch_add(1);
        }

        int unref()
        {
            if (m_ref == nullptr)
            {
                return -1;
            }

            return m_ref->fetch_add(-1);
        }

    public:
        static std::atomic<uint64_t> m_avframeRefCount;
        static std::atomic<uint64_t> m_avframeSize;

    private:
        int m_width;
        int m_height;
        uint64_t m_size;
        AVFrame *m_frame;
        std::atomic<int> *m_ref;
        uint8_t *m_frameBuffer;
        bool m_isAlpha;
        cv::Mat m_alphaMat;
        double m_alphaRate;
        bool m_isTempLayer;
        bool m_isTransparentLayer;
    };

} // namespace hercules
