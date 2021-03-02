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

#include "OpenCVOperator.h"
#include "GifUtil.h"
#include "Log.h"
#include "Common.h"
#include "Util.h"
#include "MediaFrame.h"
#include "CvxText.h"
#include "TimeUse.h"

#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "ft2build.h"
#include "freetype/freetype.h"
#include "freetype/ftbitmap.h"
#include "freetype/ftoutln.h"

#include <wchar.h>
#include <assert.h>
#include <locale.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <chrono>
#include <string>
#include <vector>
#include <algorithm>

using cv::LineTypes;
using cv::Mat;
using cv::Rect;
using cv::Scalar;
using std::string;
using std::vector;

namespace hercules
{

    extern int toWchar(const char *src, wchar_t *&dest, const char *locale = "en_US.utf8");

#define RESIZE_ALGORITHM cv::INTER_LINEAR

    OpenCVOperator::OpenCVOperator()
    {
        try
        {
            m_default_font = new CvxText("simsun.ttc");
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    OpenCVOperator::OpenCVOperator(const std::string &fontFile)
    {
        try
        {
            m_default_font = new CvxText(fontFile);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    OpenCVOperator::~OpenCVOperator()
    {
        logInfo(MIXLOG);
        delete m_default_font;

        for (const auto &kv : m_font_map)
        {
            if (kv.second != nullptr)
            {
                logInfo(MIXLOG << "del font: " << kv.first);
                delete kv.second;
            }
        }
    }

    void OpenCVOperator::setFontType(const string &file)
    {
        if (!isFileExist(file))
        {
            logErr(MIXLOG << "error, font file: " << file << " no exist");
            return;
        }
        if (m_default_font)
        {
            delete m_default_font;
            m_default_font = nullptr;
        }
        try
        {
            m_default_font = new CvxText(file.c_str());
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
        return;
    }

    bool OpenCVOperator::formatCircle(MediaFrame &tMediaFrame, int &radius,
                                      cv::Point &center, int thickness)
    {
        if (center.x == 0 && center.y == 0)
        {
            center.x = tMediaFrame.getWidth() / 2;
            center.y = tMediaFrame.getHeight() / 2;
            radius = tMediaFrame.getWidth() / 2 - thickness - 2;
            if (radius < 0)
            {
                return false;
            }
        }

        int sum = tMediaFrame.getWidth();
        int dec = center.x;
        int min = tMediaFrame.getWidth() - center.x;

        if (min > center.x)
        {
            sum = center.x;
            dec = 0;
            min = center.x;
        }

        if (min > tMediaFrame.getWidth() - center.y)
        {
            sum = tMediaFrame.getWidth();
            dec = center.y;
            min = tMediaFrame.getWidth() - center.y;
        }
        if (min > center.y)
        {
            sum = center.y;
            dec = 0;
            min = center.y;
        }

        int between = sum - dec - radius - thickness;
        if (between < 0)
        {
            radius += between;
        }

        if (radius < 0)
            return false;

        return true;
    }

    int OpenCVOperator::getRGBMat(cv::Mat &YUVMat, cv::Mat &BGRMat)
    {
        if (!YUVMat.empty())
        {
            try
            {
                cv::cvtColor(YUVMat, BGRMat, CV_YUV2BGR_I420);
                return 0;
            }
            catch (cv::Exception &ex)
            {
                logErr(MIXLOG << "cv error, msg: " << ex.what());
            }
            catch (std::exception &ex)
            {
                logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
            }
        }

        return -1;
    }

    int OpenCVOperator::getYUVMat(cv::Mat &YUVMat, cv::Mat &BGRMat)
    {
        if (!BGRMat.empty())
        {
            try
            {
                cv::cvtColor(BGRMat, YUVMat, CV_BGR2YUV_I420);
                return 0;
            }
            catch (cv::Exception &ex)
            {
                logErr(MIXLOG << "cv error, msg: " << ex.what());
            }
            catch (std::exception &ex)
            {
                logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
            }
        }

        return -1;
    }

    void OpenCVOperator::addPolygonAlphaCustom(MediaFrame &tMediaFrame,
                                               const std::vector<std::vector<cv::Point>> contours)
    {
        try
        {
            AVFrame *srcAVFrame = tMediaFrame.getAVFrame();

            if (!tMediaFrame.hasBuffer())
            {
                logErr(MIXLOG << "error, has not buffer");
                return;
            }

            logDebug(MIXLOG << "image width:" << tMediaFrame.getWidth() 
                << ", image height: " << tMediaFrame.getHeight());

            uint8_t *srcBuffer = tMediaFrame.getBuffer();
            int srcW = tMediaFrame.getWidth();
            int srcH = tMediaFrame.getHeight();

            Mat alpha = cv::Mat(srcH, srcW, CV_32FC1);
            alpha.setTo(0);

            cv::fillPoly(alpha, contours, 255, LineTypes::LINE_AA);
            cv::resize(alpha, alpha, cv::Size(srcW, srcH), 0.0, 0.0, RESIZE_ALGORITHM);
            alpha = alpha / 255.0;
            tMediaFrame.setAlpha(alpha);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    void OpenCVOperator::addCircleStroke(MediaFrame &tMediaFrame, cv::Point center, int radius,
                                         const Scalar &color, int thickness)
    {
        try
        {
            int srcW = tMediaFrame.getWidth(), srcH = tMediaFrame.getHeight();
            AVFrame *srcAVFrame = tMediaFrame.getAVFrame();
            Mat srcMat;
            if (tMediaFrame.hasBuffer())
            {
                uint8_t *srcBuffer = tMediaFrame.getBuffer();
                srcMat = cv::Mat(srcH * 3 / 2, srcW, CV_8UC1, srcBuffer);
            }
            else
            {
                logErr(MIXLOG << "error, has not buffer");
                return;
            }

            thickness = thickness / 2 * 2;
            if (thickness == 1)
                thickness = 2;

            if (!formatCircle(tMediaFrame, radius, center, thickness))
            {
                logInfo(MIXLOG << "radius: " << radius
                    << ", thickness: " << thickness
                    << ", x: " << center.x
                    << ", y: " << center.y
                    << ", width: " << tMediaFrame.getWidth()
                    << ", height: " << tMediaFrame.getHeight());

                return;
            }

            radius = radius / 2 * 2;
            if (radius <= 1)
                radius = 2;

            Mat RGBMat;
            if (getRGBMat(srcMat, RGBMat) != 0)
            {
                logErr(MIXLOG << "error, get bgr mat err");
                return;
            }

            circle(RGBMat, center, radius, color, thickness + 2, LineTypes::LINE_AA);

            if (getYUVMat(srcMat, RGBMat) != 0)
            {
                logErr(MIXLOG << "error get yuv err");
                return;
            }

            Mat alpha = cv::Mat(RGBMat.rows, RGBMat.cols, CV_32FC1);
            alpha.setTo(0);
            circle(alpha, center, radius + 2, 255, -1, LineTypes::LINE_AA);
            alpha = alpha / 255.0;
            tMediaFrame.setAlpha(alpha);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    void OpenCVOperator::addCircleAlphaCustom(MediaFrame &tMediaFrame,
                                              const cv::Point &circleCenter, int radius)
    {
        try
        {
            AVFrame *srcAVFrame = tMediaFrame.getAVFrame();

            if (!tMediaFrame.hasBuffer())
            {
                logErr(MIXLOG << "error, has not buffer");
                return;
            }

            uint8_t *srcBuffer = tMediaFrame.getBuffer();
            int srcW = tMediaFrame.getWidth();
            int srcH = tMediaFrame.getHeight();
            Mat srcMat = cv::Mat(srcH * 3 / 2, srcW, CV_8UC1, srcBuffer);

            Mat alpha = cv::Mat(srcH, srcW, CV_32FC1);
            alpha.setTo(0);
            circle(alpha, circleCenter, radius, 255, -1, LineTypes::LINE_AA);
            alpha = alpha / 255.0;
            tMediaFrame.setAlpha(alpha);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    void OpenCVOperator::addCircleYUV(MediaFrame &tMediaFrame, const cv::Scalar color,
        const cv::Point &circleCenter, int radius, int thickness, 
        int line_type)
    {
        try
        {
            AVFrame *srcAVFrame = tMediaFrame.getAVFrame();

            if (!tMediaFrame.hasBuffer())
            {
                logErr(MIXLOG << "error, has not buffer");
                return;
            }

            uint8_t *srcBuffer = tMediaFrame.getBuffer();
            int width = tMediaFrame.getWidth();
            int height = tMediaFrame.getHeight();

            Mat mat_yuv = cv::Mat(height * 3 / 2, width, CV_8UC1, srcBuffer);
            cv::Mat mat_y = mat_yuv(cv::Rect(0, 0, width, height));
            cv::Mat mat_u(height / 2, width / 2, CV_8UC1, mat_y.data + width * height);
            cv::Mat mat_v(height / 2, width / 2, CV_8UC1,
                          mat_y.data + width * height + width * height / 4);

            uchar Y = (uchar)(0.299 * (color.val[2]) + 0.587 * color.val[1] + 0.114 * color.val[0]);
            uchar U = (uchar)(-0.1687 * (color.val[2]) - 0.3313 * color.val[1] +
                              0.5 * color.val[0] + 128);
            uchar V = (uchar)(0.5 * (color.val[2]) - 0.4187 * color.val[1] -
                              0.0813 * color.val[0] + 128);

            int y_thickness = thickness;
            int u_thickness = thickness / 2;
            int v_thickness = thickness / 2;
            if (thickness < 0)
            {
                u_thickness = -1;
                v_thickness = -1;
            }

            cv::circle(mat_y, circleCenter, radius, cv::Scalar(Y, Y, Y), y_thickness, line_type);
            cv::circle(mat_u, cv::Point(circleCenter.x / 2, circleCenter.y / 2), radius / 2,
                       cv::Scalar(U, U, U), u_thickness, line_type);
            cv::circle(mat_v, cv::Point(circleCenter.x / 2, circleCenter.y / 2), radius / 2,
                       cv::Scalar(V, V, V), v_thickness, line_type);

            if (tMediaFrame.isTempLayer() || tMediaFrame.isTransparentLayer())
            {
                cv::Mat &alpha = tMediaFrame.getAlpha();
                circle(alpha, circleCenter, radius, 1.0, -1, LineTypes::LINE_AA);
            }
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg:" << ex.what());
        }
    }

    void OpenCVOperator::createYUV(MediaFrame &tMediaFrame, int w, int h, int y, int u, int v)
    {
        try
        {
            if (w & 1 || h & 1)
            {
                logWarn(MIXLOG << "error, w: " << w << ", h:" << h << " %2 != 0");
                w = w + (w & 1);
                h = h + (h & 1);
            }
            AVFrame *avFrame = av_frame_alloc();
            avFrame->width = w;
            avFrame->height = h;
            avFrame->format = AV_PIX_FMT_YUV420P;
            int yuvFrameSize = avpicture_get_size(AV_PIX_FMT_YUV420P, w, h);
            uint8_t *buffer = reinterpret_cast<uint8_t *>(av_malloc(yuvFrameSize));
            avpicture_fill(reinterpret_cast<AVPicture *>(avFrame), buffer, AV_PIX_FMT_YUV420P, w, h);

            Mat mat = cv::Mat(h + h / 2, w, CV_8UC1, reinterpret_cast<void *>(buffer));
            mat.setTo(y);
            cv::Mat uMat(mat(cv::Rect(0, h, w, h / 4)));
            uMat.setTo(u);
            cv::Mat vMat(mat(cv::Rect(0, h * 5 / 4, w, h / 4)));
            vMat.setTo(v);

            tMediaFrame.setAVFrame(avFrame, buffer, yuvFrameSize);
            tMediaFrame.asVideo();
            tMediaFrame.setWidth(w);
            tMediaFrame.setHeight(h);
            uint32_t timestamp = TimestampAdjuster::getElapseFromServerStart();
            tMediaFrame.setDts(timestamp);
            tMediaFrame.setPts(timestamp);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    bool OpenCVOperator::loadGif(const string &file)
    {
        GifFrameContext ctx;
        bool ret = GifUtil::getInstance()->loadGif(ctx, file);
        if (ret)
        {
            m_gifFrames[file] = ctx;
        }
        return ret;
    }

    void OpenCVOperator::loadLogo(const string &file, MediaFrame &logo)
    {
        try
        {
            bool isAlpha = false;
            cv::Mat mRgbLogo;
            mRgbLogo = cv::imread(file, -1);
            if (mRgbLogo.data == nullptr)
            {
                logErr(MIXLOG << "load log error");
            }
            int w = mRgbLogo.cols, h = mRgbLogo.rows;
            {
                w = w / 2 * 2;
                h = h / 2 * 2;
                cv::resize(mRgbLogo, mRgbLogo, cv::Size(w, h), 0.0, 0.0, RESIZE_ALGORITHM);
            }
            std::vector<cv::Mat> channels;
            cv::split(mRgbLogo, channels);

            if (channels.size() > 3)
            {
                isAlpha = true;
                channels[3].convertTo(channels[3], CV_32FC1);
                channels[3] = channels[3] / 255.0;
                logo.setAlpha(channels[3]);
            }
            AVFrame *avFrame = av_frame_alloc();
            avFrame->width = w;
            avFrame->height = h;
            avFrame->format = AV_PIX_FMT_YUV420P;

            int yuvFrameSize = avpicture_get_size(AV_PIX_FMT_YUV420P, w, h);
            uint8_t *buffer = reinterpret_cast<uint8_t *>(av_malloc(yuvFrameSize));
            avpicture_fill(reinterpret_cast<AVPicture *>(avFrame), buffer, AV_PIX_FMT_YUV420P, w, h);
            cv::Mat mYuvLogo = cv::Mat(h + h / 2, w, CV_8UC1, reinterpret_cast<void *>(buffer));
            mYuvLogo.setTo(0);
            cv::Mat uvMat = mYuvLogo(cv::Rect(0, h, w, h / 2));
            uvMat.setTo(128);

            logo.setAVFrame(avFrame, buffer, yuvFrameSize);
            logo.setWidth(w);
            logo.setHeight(h);

            if (isAlpha)
            {
                cv::cvtColor(mRgbLogo, mYuvLogo, cv::COLOR_BGRA2YUV_I420);
            }
            else
            {
                cv::cvtColor(mRgbLogo, mYuvLogo, cv::COLOR_BGR2YUV_I420);
            }
            mRgbLogo.release();
            logDebug(MIXLOG << "load logo, w:" << w << ", h:" << h);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }

        return;
    }

    void OpenCVOperator::addYUV(MediaFrame &dst, MediaFrame &src, cv::Point &point, int w, int h)
    {
        cv::Rect rect(0, 0, src.getWidth(), src.getHeight());
        std::vector<cv::Point> empty;
        addYUV(dst, src, point, w, h, rect, empty, 1.0);
    }

    void OpenCVOperator::addYUV(MediaFrame &dst, MediaFrame &src,
                                cv::Point &point, int w, int h, cv::Rect &rect)
    {
        std::vector<cv::Point> empty;
        addYUV(dst, src, point, w, h, rect, empty, 1.0);
    }

    void OpenCVOperator::addYUV(MediaFrame &dst, MediaFrame &src, cv::Point &point, int w, int h,
        cv::Rect &rect, const std::vector<cv::Point> clipPolygon, double clipFactor)
    {
        TimeUse t(__FUNCTION__);

        dst.addIdTimestamp(src.getStreamId(), src.getDts());
        auto timeTrace = src.getIdTimeTrace();
        for (const auto &kv : timeTrace)
        {
            for (const auto &key_time : kv.second)
            {
                dst.addIdTimeTrace(src.getStreamId(), key_time.first, key_time.second);
            }
        }
        dst.addIdTimeTrace(src.getStreamId(), TimeTraceKey::MIXED, getNowMs32());

        int x = point.x, y = point.y;

        if (rect.x & 1 || rect.y & 1)
        {
            logWarn(MIXLOG << "warn"<< ", rect xy is odd num, x: " << rect.x << ", y: " << rect.y);
            rect.x = rect.x - (rect.x & 1);
            rect.y = rect.y - (rect.y & 1);
        }

        if (rect.width & 1 || rect.height & 1)
        {
            logWarn(MIXLOG << "warn" << ", rect wh is odd num"
                << ", w: " << rect.width << ", h: " << rect.height);
            rect.width = rect.width - (rect.width & 1);
            rect.height = rect.height - (rect.height & 1);
        }

        w = w - (w & 1);
        h = h - (h & 1);
        x = x - (x & 1);
        y = y - (y & 1);

        int dstW = dst.getWidth(), dstH = dst.getHeight();
        int srcW = src.getWidth(), srcH = src.getHeight();
        AVFrame *dstAVFrame = dst.getAVFrame();
        AVFrame *srcAVFrame = src.getAVFrame();

        if (!(x >= 0 && y >= 0 && x + w <= dstW && y + h <= dstH))
        {
            logWarn(MIXLOG << "warn" << ", put_rect beyond output to adjust"
                << ", x: " << x << ", y: " << y << ", w: " << w << ", h: " << h
                << ", dstw: " << dstW << ", dsth: " << dstH); 

            x = std::min(dstW, std::max(0, x));
            y = std::min(dstH, std::max(0, y));
            w = std::min(dstW - x, w);
            h = std::min(dstH - y, h);
        }

        if (!(rect.x >= 0 && rect.y >= 0 && rect.x + rect.width <= srcW 
            && rect.y + rect.height <= srcH))
        {
            logWarn(MIXLOG << "warn" << ", crop_rect beyond input to adjust"
                << ", x: " << rect.x << ", y: " << rect.y 
                << ", w: " << rect.width << ", h: " << rect.height
                << ", srcw: " << srcW << ", srch: " << srcH);

            rect = cv::Rect(0, 0, srcW, srcH);
        }

        if (srcAVFrame == nullptr || dstAVFrame == nullptr)
        {
            logErr(MIXLOG << "null frame");
            return;
        }

        try
        {
            uint8_t *dstBuffer = dst.getBuffer();
            cv::Mat dstY = cv::Mat(dstH, dstW, CV_8UC1, dstBuffer);
            cv::Mat dstU = cv::Mat(dstH / 2, dstW / 2, CV_8UC1, dstBuffer + dstH * dstW);
            cv::Mat dstV = cv::Mat(dstH / 2, dstW / 2, CV_8UC1,
                dstBuffer + dstH * dstW + dstH / 2 * dstW / 2);

            cv::Mat dstLocY = dstY(cv::Rect(x, y, w, h));
            cv::Mat dstLocU = dstU(cv::Rect(x / 2, y / 2, w / 2, h / 2));
            cv::Mat dstLocV = dstV(cv::Rect(x / 2, y / 2, w / 2, h / 2));

            cv::Mat srcY;
            cv::Mat srcU;
            cv::Mat srcV;

            if (src.hasBuffer())
            {
                uint8_t *srcBuffer = src.getBuffer();
                srcY = cv::Mat(srcH, srcW, CV_8UC1, srcBuffer);
                srcU = cv::Mat(srcH / 2, srcW / 2, CV_8UC1, srcBuffer + srcW * srcH);
                srcV = cv::Mat(srcH / 2, srcW / 2, CV_8UC1,
                    srcBuffer + srcW * srcH + srcW / 2 * srcH / 2);
            }
            else
            {
                srcY = cv::Mat(srcH, srcAVFrame->linesize[0], CV_8UC1,
                               reinterpret_cast<void *>(srcAVFrame->data[0]));
                srcU = cv::Mat(srcH / 2, srcAVFrame->linesize[1], CV_8UC1,
                               reinterpret_cast<void *>(srcAVFrame->data[1]));
                srcV = cv::Mat(srcH / 2, srcAVFrame->linesize[2], CV_8UC1,
                               reinterpret_cast<void *>(srcAVFrame->data[2]));

                srcY = srcY(cv::Rect(0, 0, srcAVFrame->width, srcAVFrame->height));
                srcU = srcU(cv::Rect(0, 0, srcAVFrame->width / 2, srcAVFrame->height / 2));
                srcV = srcV(cv::Rect(0, 0, srcAVFrame->width / 2, srcAVFrame->height / 2));
            }

            cv::Mat srcLocY = srcY(rect);
            cv::Mat srcLocU = srcU(cv::Rect(rect.x / 2, rect.y / 2,
                rect.width / 2, rect.height / 2));
            cv::Mat srcLocV = srcV(cv::Rect(rect.x / 2, rect.y / 2,
                rect.width / 2, rect.height / 2));

            cv::Mat formatSrcY, formatSrcU, formatSrcV;
            resize(srcLocY, formatSrcY, cv::Size(w, h), 0.0, 0.0, RESIZE_ALGORITHM);
            resize(srcLocU, formatSrcU, cv::Size(w / 2, h / 2), 0.0, 0.0, RESIZE_ALGORITHM);
            resize(srcLocV, formatSrcV, cv::Size(w / 2, h / 2), 0.0, 0.0, RESIZE_ALGORITHM);

            const bool clipPolygonMode = !src.isAlpha() && !clipPolygon.empty();
            if (src.isAlpha() || clipPolygonMode)
            {
                float alphaRate = src.getAlphaRate();
                cv::Mat srcMask;
                if (clipPolygonMode)
                    srcMask = cv::Mat(h, w, CV_32FC1, alphaRate);
                else
                    srcMask = src.getAlpha() * alphaRate;
                cv::Mat srcLocMask, small_mask;
                cv::resize(srcMask(rect), srcLocMask, cv::Size(w, h), 0.0, 0.0, RESIZE_ALGORITHM);

                if (!clipPolygon.empty())
                {
                    TimeUse t("clip polygon", 1);

                    int scale = std::max(1, static_cast<int>(clipFactor));
                    std::vector<std::vector<cv::Point>> contours_;
                    contours_.push_back(clipPolygon);
                    std::vector<cv::Point> &points = contours_.back();
                    if (scale > 1)
                    {
                        for (auto &point : points)
                        {
                            point.x *= scale;
                            point.y *= scale;

                            logDebug(MIXLOG << "clip pt.x: " << point.x << ", pt.y: " << point.y);
                        }
                    }

                    Mat alpha = cv::Mat(h * scale, w * scale, CV_32FC1, 0.0f);
                    cv::fillPoly(alpha, contours_, 1.0f, LineTypes::LINE_AA);
                    if (scale > 1)
                        cv::resize(alpha, alpha, cv::Size(w, h), 0.0, 0.0, RESIZE_ALGORITHM);

                    srcLocMask = srcLocMask.mul(alpha);
                }

                resize(srcLocMask, small_mask, cv::Size(w / 2, h / 2), 0.0, 0.0, RESIZE_ALGORITHM);

                cv::Mat fSrcY, fSrcU, fSrcV, fDstY, fDstU, fDstV;
                formatSrcY.convertTo(fSrcY, CV_32FC1);
                formatSrcU.convertTo(fSrcU, CV_32FC1);
                formatSrcV.convertTo(fSrcV, CV_32FC1);
                dstLocY.convertTo(fDstY, CV_32FC1);
                dstLocU.convertTo(fDstU, CV_32FC1);
                dstLocV.convertTo(fDstV, CV_32FC1);

                cv::add(fSrcY.mul(srcLocMask), fDstY.mul(1.0 - srcLocMask), fDstY);
                cv::add(fSrcU.mul(small_mask), fDstU.mul(1.0 - small_mask), fDstU);
                cv::add(fSrcV.mul(small_mask), fDstV.mul(1.0 - small_mask), fDstV);

                if (dst.isTransparentLayer())
                {
                    cv::Mat &fDstA = dst.getAlpha();
                    logDebug(MIXLOG << "dst is transparentLayer"
                        << ", src locmask.w: " << srcLocMask.cols
                        << ", src locmask.h: " << srcLocMask.rows 
                        << ", fdsta.w: " << fDstA.cols << ", fdsta.h: " << fDstA.rows);
                    cv::Mat fDstARange = fDstA(cv::Rect(x, y, w, h));
                    logDebug(MIXLOG << "fdsta range"
                        <<", w: " << fDstARange.cols << ", h: " << fDstARange.rows);
                    cv::add(srcLocMask, fDstARange.mul(1.0 - srcLocMask), fDstARange);
                }

                fDstY.convertTo(dstLocY, CV_8UC1);
                fDstU.convertTo(dstLocU, CV_8UC1);
                fDstV.convertTo(dstLocV, CV_8UC1);
            }
            else
            {
                formatSrcY.copyTo(dstLocY);
                formatSrcU.copyTo(dstLocU);
                formatSrcV.copyTo(dstLocV);

                if (dst.isTransparentLayer())
                {
                    cv::Mat &fDstA = dst.getAlpha();
                    logDebug(MIXLOG << "dst is transparentLayer"
                        << ", fdsta.w:" << fDstA.cols << ", fdsta.h:" << fDstA.rows);
                    cv::Mat fDstARange = fDstA(cv::Rect(x, y, w, h));
                    fDstARange.setTo(1.0);
                }
            }

            avpicture_fill(reinterpret_cast<AVPicture *>(dstAVFrame), dst.getBuffer(),
                           AV_PIX_FMT_YUV420P, dstW, dstH);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv eroor, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
        return;
    }

    void OpenCVOperator::addGif(MediaFrame &dst, const std::string &file, int fps,
                                cv::Point &point, int w, int h)
    {
        if (m_gifFrames.find(file) == m_gifFrames.end())
        {
            loadGif(file);
            return;
        }
        auto &ctx = m_gifFrames[file];
        ++ctx.m_property.m_mixCount;
        uint32_t showDuration = (ctx.m_property.m_mixCount * 1000) / fps;
        uint32_t gifDuration = ctx.m_property.m_idx * ctx.m_property.m_delay;
        int skipFrames = 0;
        while (showDuration > gifDuration)
        {
            ++ctx.m_property.m_idx;
            gifDuration = ctx.m_property.m_idx * ctx.m_property.m_delay;
            ++skipFrames;
            if (skipFrames > 6)
            {
                break;
            }
        }
        int idx = ctx.m_property.m_idx % ctx.m_property.m_imgCount;
        if (ctx.m_imgs.size() > idx)
        {
            MediaFrame &frame = ctx.m_imgs[idx];
            addYUV(dst, frame, point, w, h);
        }
    }

    void OpenCVOperator::addWord(MediaFrame &dst, const std::string &text, cv::Point &point,
        cv::Scalar &color, cv::Scalar &size, float fp, const string &fontName, int leanType,
        float inclination)
    {
        if (text.empty())
            return;

        try
        {
            TimeUse t(__FUNCTION__);
            int dstW = dst.getWidth(), dstH = dst.getHeight();
            uint8_t *dstBuffer = dst.getBuffer();
            cv::Mat dstY = cv::Mat(dstH / 2 * 3, dstW, CV_8UC1, dstBuffer);

            wchar_t *wStr;
            toWchar(text.c_str(), wStr);
            CvxText *font = getFont(fontName);
            font->setFont(nullptr, &size, nullptr, &fp, &leanType, &inclination);
            font->putText(dstY, wStr, point, color, true, dst.isAlpha(), dst.getAlpha());
            delete[] wStr;
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }

        return;
    }

    void OpenCVOperator::addBoldWord(MediaFrame &dst, const std::string &text, cv::Point &point,
        cv::Scalar &color, cv::Scalar &size, float fp, const string &fontName, int bold,
        int leanType, float inclination)
    {
        if (text.empty())
            return;

        try
        {
            TimeUse t(__FUNCTION__);
            int dstW = dst.getWidth(), dstH = dst.getHeight();
            uint8_t *dstBuffer = dst.getBuffer();
            cv::Mat dstY = cv::Mat(dstH / 2 * 3, dstW, CV_8UC1, dstBuffer);

            wchar_t *w_str;
            toWchar(text.c_str(), w_str);
            CvxText *font = getFont(fontName);
            font->setFont(nullptr, &size, nullptr, &fp, &leanType, &inclination);
            font->putBoldText(dstY, w_str, point, color, true, dst.isAlpha(), dst.getAlpha(), bold);
            delete[] w_str;
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }

        return;
    }

    void OpenCVOperator::addBorderWordYUV(MediaFrame &dst, const std::string &text,
        double borderWidth, int fontHeight, cv::Point &point,
        cv::Scalar &fontColor, cv::Scalar &borderColor, cv::Scalar &size, float fp,
        const string &fontName, int leanType, float inclination)
    {
        if (text.empty())
            return;

        try
        {
            TimeUse t(__FUNCTION__);
            int dstW = dst.getWidth(), dstH = dst.getHeight();
            uint8_t *dstBuffer = dst.getBuffer();
            cv::Mat matYUV = cv::Mat(dstH / 2 * 3, dstW, CV_8UC1, dstBuffer);

            logDebug(MIXLOG << "mat yuv"
                << ", rows: " << matYUV.rows << ", cols: " << matYUV.cols
                << ", font height: " << fontHeight << ", border width: " << borderWidth
                << ", point.x: " << point.x << ", point.y: " << point.y
                << ", font color: " 
                << static_cast<int>(fontColor[0]) << ","
                << static_cast<int>(fontColor[1]) << ","
                << static_cast<int>(fontColor[2]) << ","
                << static_cast<int>(fontColor[3])
                << ", border color: "
                << static_cast<int>(borderColor[0]) << ","
                << static_cast<int>(borderColor[1]) << ","
                << static_cast<int>(borderColor[2]) << ","
                << static_cast<int>(borderColor[3]));

            wchar_t *w_str;
            toWchar(text.c_str(), w_str);
            CvxText *font = getFont(fontName);
            font->setFont(nullptr, &size, nullptr, &fp, &leanType, &inclination);
            font->putTextWithBorderYUV(matYUV, fontHeight, w_str, borderWidth,
                                       point, fontColor, borderColor);

            delete[] w_str;
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }

        return;
    }

    CvxText *OpenCVOperator::getFont(const string &fontName)
    {
        CvxText *font = m_default_font;
        if (m_font_map.find(fontName) != m_font_map.end())
        {
            font = m_font_map[fontName];
        }
        else
        {
            const string font_dir = getCWD() + "/../data/font/" + fontName;
            if (isFileExist(font_dir))
            {
                CvxText *new_font = new CvxText(font_dir.c_str());
                m_font_map[fontName] = new_font;
                font = new_font;
            }
        }
        return font;
    }

    void OpenCVOperator::getWordSize(const string &word, cv::Size &size, const string &fontName,
        int font_size, int leanType, float inclination)
    {
        try
        {
            CvxText *font = getFont(fontName);
            float fp = 1.0;
            cv::Scalar scalar(font_size, 0, 0, 0);
            font->setFont(nullptr, &scalar, nullptr, &fp, &leanType, &inclination);
            wchar_t *w_str;
            toWchar(word.c_str(), w_str);
            font->getStrSize(w_str, size.width, size.height);
            delete[] w_str;
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
        return;
    }

    void OpenCVOperator::getBorderWordSize(const string &word, cv::Size &size,
        double borderWidth, const string &fontName, int font_size)
    {
        try
        {
            CvxText *font = getFont(fontName);
            float fp = 1.0;
            cv::Scalar scalar(font_size, 0, 0, 0);
            font->setFont(nullptr, &scalar, nullptr, &fp);
            wchar_t *w_str;
            toWchar(word.c_str(), w_str);
            font->getBorderStrSize(w_str, borderWidth, size.width, size.height);
            delete[] w_str;
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
        return;
    }

    void OpenCVOperator::addLine(MediaFrame &dst, cv::Point &p1, cv::Point &p2,
        cv::Scalar &color, int thickness)
    {
        try
        {
            vector<cv::Mat> dstMatVec = dst.getMatVec();
            if (dstMatVec.size() < 3)
                return;
            cv::line(dstMatVec[0], p1, p2, color, thickness);
            cv::line(dstMatVec[1], cv::Point(p1.x / 2, p1.y / 2),
                     cv::Point(p2.x / 2, p2.y / 2), color, thickness);
            cv::line(dstMatVec[2], cv::Point(p1.x / 2, p1.y / 2),
                     cv::Point(p2.x / 2, p2.y / 2), color, thickness);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    void OpenCVOperator::yuvToRgbAndDumpFile(const std::string &dumpFileName, 
        const MediaFrame &frame)
    {
        if (!frame.hasBuffer())
        {
            return;
        }

        int w = frame.getWidth();
        int h = frame.getHeight();

        logDebug(MIXLOG << "dump file name: " << dumpFileName 
            << ", frame width: " << w << ", frame height: " << h
            << ", frame buffer: " << reinterpret_cast<void *>(frame.getBuffer()) 
            << ", frame size: " << frame.getSize());

        Mat matYUV = cv::Mat(h * 3 / 2, w, CV_8UC1, reinterpret_cast<void *>(frame.getBuffer()));
        cv::Mat matRGB;
        cv::cvtColor(matYUV, matRGB, CV_YUV420p2RGB);

        imwrite(dumpFileName.c_str(), matRGB);
    }

    void OpenCVOperator::yuvDump(const std::string &dumpFileName, const MediaFrame &frame)
    {
        if (!frame.hasBuffer())
        {
            return;
        }

        logDebug(MIXLOG << "dump file name: " << dumpFileName 
            << ", frame buffer: " << reinterpret_cast<void *>(frame.getBuffer()) 
            << ", frame size: " << frame.getSize());

        int fd = open(dumpFileName.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
        if (fd < 0)
        {
            return;
        }

        int nbytes = write(fd, frame.getBuffer(), frame.getSize());
        if (nbytes < 0)
        {
            logErr(MIXLOG << "write yuv to file: " 
                << dumpFileName << " faield, ret: " << nbytes << ", err: " << strerror(errno));
        }

        close(fd);
    }

    void OpenCVOperator::yuvDumpTempLayer(const std::string &dumpFileName,
                                          const MediaFrame &frame, int scaledh, int scaledw)
    {
        if (!frame.hasBuffer())
        {
            return;
        }

        logDebug(MIXLOG << "dump file name=" << dumpFileName << ",frame buffer="
            << reinterpret_cast<void *>(frame.getBuffer()) << ", frame size=" << frame.getSize());

        int fd = open(dumpFileName.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
        if (fd < 0)
        {
            return;
        }

        int nbytes = write(fd, frame.getBuffer(), frame.getSize());
        if (nbytes < 0)
        {
            logErr(MIXLOG << "error, write yuv to file: " << dumpFileName 
                << " faield, ret: " << nbytes << ", err: " << strerror(errno));
        }
        close(fd);

        if (frame.isAlpha() && scaledh != -1 && scaledw != -1)
        {
            scaledh &= 0xfffffffe;
            scaledw &= 0xfffffffe;

            MediaFrame resizeFrame;
            createYUV(resizeFrame, scaledw, scaledh);
            uint8_t *dstBuffer = resizeFrame.getBuffer();
            cv::Mat dstYMat = cv::Mat(scaledh, scaledw, CV_8UC1, dstBuffer);
            cv::Mat dstUMat = cv::Mat(scaledh / 2, scaledw / 2, CV_8UC1,
                                      dstBuffer + scaledh * scaledw);
            cv::Mat dstVMat = cv::Mat(scaledh / 2, scaledw / 2, CV_8UC1,
                                      dstBuffer + scaledh * scaledw + scaledh / 2 * scaledw / 2);

            cv::Mat &alpha = const_cast<MediaFrame &>(frame).getAlpha();
            cv::Mat localYAlpha, localUVAlpha;
            resize(alpha, localYAlpha, cv::Size(scaledw, scaledh), 0, 0, RESIZE_ALGORITHM);
            resize(localYAlpha, localUVAlpha, cv::Size(scaledw / 2, scaledh / 2),
                   0, 0, RESIZE_ALGORITHM);

            cv::Mat yMat;
            cv::Mat uMat;
            cv::Mat vMat;
            dstYMat.convertTo(yMat, CV_32FC1);
            dstUMat.convertTo(uMat, CV_32FC1);
            dstVMat.convertTo(vMat, CV_32FC1);
            yMat = yMat.mul(localYAlpha);
            uMat = uMat.mul(localUVAlpha);
            vMat = vMat.mul(localUVAlpha);
            yMat.convertTo(dstYMat, CV_8UC1);
            uMat.convertTo(dstUMat, CV_8UC1);
            vMat.convertTo(dstVMat, CV_8UC1);

            string dumpAAName = dumpFileName + ".aa.yuv";
            int fd2 = open(dumpAAName.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
            if (fd2 < 0)
            {
                return;
            }
            nbytes = write(fd2, resizeFrame.getBuffer(), resizeFrame.getSize());
            if (nbytes < 0)
            {
                logErr(MIXLOG << "error, write yuv to file: " << dumpFileName
                    << " faield, ret: " << nbytes << ", err: " << strerror(errno));
            }
            close(fd2);

            uint8_t *yuvbuffer = const_cast<MediaFrame &>(frame).getBuffer();
            int h = frame.getHeight();
            int w = frame.getWidth();
            cv::Mat srcYMat = cv::Mat(h, w, CV_8UC1, yuvbuffer);
            cv::Mat srcUMat = cv::Mat(h / 2, w / 2, CV_8UC1, yuvbuffer + w * h);
            cv::Mat srcVMat = cv::Mat(h / 2, w / 2, CV_8UC1, yuvbuffer + w * h + w / 2 * h / 2);

            cv::Mat formatSrcY, formatSrcU, formatSrcV;
            resize(srcYMat, formatSrcY, cv::Size(scaledw, scaledh), 0, 0, RESIZE_ALGORITHM);
            resize(srcUMat, formatSrcU, cv::Size(scaledw / 2, scaledh / 2), 0, 0, RESIZE_ALGORITHM);
            resize(srcVMat, formatSrcV, cv::Size(scaledw / 2, scaledh / 2), 0, 0, RESIZE_ALGORITHM);

            formatSrcY.copyTo(dstYMat);
            formatSrcU.copyTo(dstUMat);
            formatSrcV.copyTo(dstVMat);

            string dumpResizeName = dumpFileName + ".resize.yuv";
            int fd3 = open(dumpResizeName.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
            if (fd3 < 0)
            {
                return;
            }
            nbytes = write(fd3, resizeFrame.getBuffer(), resizeFrame.getSize());
            if (nbytes < 0)
            {
                logErr(MIXLOG << "error, write yuv to file: " << dumpFileName
                    << " faield, ret: " << nbytes << ", err: " << strerror(errno));
            }
            close(fd3);

            MediaFrame oriAFrame;
            createYUV(oriAFrame, w, h);
            uint8_t *oriBuffer = oriAFrame.getBuffer();
            cv::Mat oriYMat = cv::Mat(h, w, CV_8UC1, oriBuffer);
            cv::Mat oriUMat = cv::Mat(h / 2, w / 2, CV_8UC1, oriBuffer + h * w);
            cv::Mat oriVMat = cv::Mat(h / 2, w / 2, CV_8UC1, oriBuffer + h * w + h / 2 * w / 2);

            resize(alpha, localUVAlpha, cv::Size(w / 2, h / 2), 0, 0, RESIZE_ALGORITHM);
            oriYMat.convertTo(yMat, CV_32FC1);
            oriUMat.convertTo(uMat, CV_32FC1);
            oriVMat.convertTo(vMat, CV_32FC1);
            yMat = yMat.mul(alpha);
            uMat = uMat.mul(localUVAlpha);
            vMat = vMat.mul(localUVAlpha);
            yMat.convertTo(oriYMat, CV_8UC1);
            uMat.convertTo(oriUMat, CV_8UC1);
            vMat.convertTo(oriVMat, CV_8UC1);

            string dumpOirAAName = dumpFileName + ".ori.aa.yuv";
            int fd4 = open(dumpOirAAName.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
            if (fd4 < 0)
            {
                return;
            }
            nbytes = write(fd4, oriAFrame.getBuffer(), oriAFrame.getSize());
            if (nbytes < 0)
            {
                logErr(MIXLOG << "error, write yuv to file: " << dumpFileName
                    << " faield, ret: " << nbytes << ", err: " << strerror(errno));
            }
            close(fd4);
        }
    }

    void OpenCVOperator::setAsTempLayer(MediaFrame &tMediaFrame,
                                        MediaFrame &baseFrame, cv::Rect &rect)
    {
        try
        {
            if (!tMediaFrame.hasBuffer() || !baseFrame.hasBuffer())
            {
                logErr(MIXLOG << "error, media frame buffer: " << tMediaFrame.hasBuffer()
                              << ", baseFrame buffer: " << baseFrame.hasBuffer());
                return;
            }

            int baseW = baseFrame.getWidth();
            int baseH = baseFrame.getHeight();
            int dstW = tMediaFrame.getWidth();
            int dstH = tMediaFrame.getHeight();

            if (rect.x & 1 || rect.y & 1)
            {
                logErr(MIXLOG << "error, rect xy is odd num" 
                    << ", x: " << rect.x << ", y: " << rect.y);
                rect.x = rect.x - (rect.x & 1);
                rect.y = rect.y - (rect.y & 1);
            }

            if (rect.width & 1 || rect.height & 1)
            {
                logErr(MIXLOG << "error, rect xy is odd num"
                    << ", w: " << rect.width << ", h: " << rect.height);
                rect.width = rect.width - (rect.width & 1);
                rect.height = rect.height - (rect.height & 1);
            }

            if (!(rect.x >= 0 &&
                  rect.y >= 0 &&
                  rect.x + rect.width <= baseW &&
                  rect.y + rect.height <= baseH))
            {
                logWarn(MIXLOG << "warn" << ", crop_rect beyond input to adjust"
                    << ", x: " << rect.x << ", y: " << rect.y 
                    << ", w: " << rect.width << ", h: " << rect.height
                    << ", srcW: " << baseW << ", srcH: " << baseH);

                return;
            }

            uint8_t *baseBuffer = baseFrame.getBuffer();
            cv::Mat baseY = cv::Mat(baseH, baseW, CV_8UC1, baseBuffer);
            cv::Mat baseU = cv::Mat(baseH / 2, baseW / 2, CV_8UC1,
                                    baseBuffer + baseH * baseW);
            cv::Mat baseV = cv::Mat(baseH / 2, baseW / 2, CV_8UC1,
                                    baseBuffer + baseH * baseW + baseH / 2 * baseW / 2);
            cv::Mat baseLocY = baseY(rect);
            cv::Mat baseLocU = baseU(Rect(rect.x / 2, rect.y / 2, rect.width / 2, rect.height / 2));
            cv::Mat baseLocV = baseV(Rect(rect.x / 2, rect.y / 2, rect.width / 2, rect.height / 2));

            uint8_t *dstBuffer = tMediaFrame.getBuffer();
            cv::Mat dstY = cv::Mat(dstH, dstW, CV_8UC1, dstBuffer);
            cv::Mat dstU = cv::Mat(dstH / 2, dstW / 2, CV_8UC1, dstBuffer + dstH * dstW);
            cv::Mat dstV = cv::Mat(dstH / 2, dstW / 2, CV_8UC1,
                                   dstBuffer + dstH * dstW + dstH / 2 * dstW / 2);

            cv::Mat fitY, fitU, fitV;
            resize(baseLocY, fitY, cv::Size(dstW, dstH), 0.0, 0.0, RESIZE_ALGORITHM);
            resize(baseLocU, fitU, cv::Size(dstW / 2, dstH / 2), 0.0, 0.0, RESIZE_ALGORITHM);
            resize(baseLocV, fitV, cv::Size(dstW / 2, dstH / 2), 0.0, 0.0, RESIZE_ALGORITHM);

            fitY.copyTo(dstY);
            fitU.copyTo(dstU);
            fitV.copyTo(dstV);

            tMediaFrame.setTempLayer(true);
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    void OpenCVOperator::addWordWithBorder(MediaFrame &dst, const std::string &text,
        double borderWidth, cv::Point &point, cv::Scalar &fontColor, cv::Scalar &borderColor,
        cv::Scalar &size, float fp, const string &fontName, int leanType, float inclination)
    {
        if (text.empty())
        {
            return;
        }

        try
        {
            TimeUse t(__FUNCTION__);
            int dstW = dst.getWidth(), dstH = dst.getHeight();
            uint8_t *dstBuffer = dst.getBuffer();
            cv::Mat matYUV = cv::Mat(dstH / 2 * 3, dstW, CV_8UC1, dstBuffer);

            wchar_t *w_str;
            toWchar(text.c_str(), w_str);
            CvxText *font = getFont(fontName);
            font->setFont(nullptr, &size, nullptr, &fp, &leanType, &inclination);
            cv::Point point2 = point;
            font->putTextBorder(matYUV, w_str, borderWidth, point, fontColor, borderColor);
            font->putText(matYUV, w_str, point2, fontColor, true, dst.isAlpha(), dst.getAlpha());

            delete[] w_str;
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << __FUNCTION__ << "cv error, msg: " << ex.what());
        }
    }

    void OpenCVOperator::getWordWithBorderSize(const string &word, cv::Size &result, 
        cv::Point &offset, double borderWidth, cv::Scalar &size, 
        const string &fontName, int leanType, float inclination)
    {
        try
        {
            CvxText *font = getFont(fontName);
            float fp = 1.0;
            font->setFont(nullptr, &size, nullptr, &fp, &leanType, &inclination);
            wchar_t *w_str;
            toWchar(word.c_str(), w_str);
            font->getWordWithBorderSize(w_str, borderWidth, offset.x, offset.y,
                                        result.width, result.height);
            delete[] w_str;
        }
        catch (cv::Exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
        catch (std::exception &ex)
        {
            logErr(MIXLOG << "cv error, msg: " << ex.what());
        }
    }

} // namespace hercules
