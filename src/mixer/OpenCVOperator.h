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

#include "GifUtil.h"

#include "opencv2/opencv.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace hercules
{

    class CvxText;
    class MediaFrame;

    class OpenCVOperator
    {
    public:
        OpenCVOperator();
        ~OpenCVOperator();
        explicit OpenCVOperator(const std::string &fontFile);

        void createYUV(MediaFrame &tFrame, int w, int h, 
            int y = 237, int u = 128, int v = 128);

        void addYUV(MediaFrame &dst, MediaFrame &src, cv::Point &point, int w, int h);

        void addYUV(MediaFrame &dst, MediaFrame &src, cv::Point &point, int w, int h, 
            cv::Rect &rect);

        void addYUV(MediaFrame &dst, MediaFrame &src, cv::Point &point, int w, int h, 
            cv::Rect &rect, const std::vector<cv::Point> clipPolygon, double clipFactor);

        void addGif(MediaFrame &dst, const std::string &file, int fps, 
            cv::Point &point, int w, int h);

        void addWord(MediaFrame &dst, const std::string &text, cv::Point &point, cv::Scalar &color,
            cv::Scalar &size, float fp, const std::string &fontName, int leanType, 
            float inclination);

        void addBoldWord(MediaFrame &dst, const std::string &text, cv::Point &point, 
            cv::Scalar &color, cv::Scalar &size, float fp, const std::string &fontName, 
            int bold, int leanType, float inclination);

        void addBorderWordYUV(MediaFrame &dst, const std::string &text, double borderWidth,
            int fontHeight, cv::Point &point, cv::Scalar &fontColor, cv::Scalar &borderColor,
            cv::Scalar &size, float fp, const std::string &fontName, int leanType, 
            float inclination);

        void setFontType(const std::string &file);

        void getWordSize(const std::string &, cv::Size &result, const std::string &fontName,
            int font_size, int leanType = 1, float inclination = 0);

        void getBorderWordSize(const std::string &, cv::Size &result, double borderWidth,
            const std::string &fontName, int font_size);

        void addLine(MediaFrame &dst, cv::Point &p1, cv::Point &p2, cv::Scalar &color, 
            int thickness);

        bool loadGif(const std::string &file);

        void loadLogo(const std::string &file, MediaFrame &logo);

        bool formatCircle(MediaFrame &tMediaFrame, int &radius, cv::Point &center, int thickness);

        void addPolygonAlphaCustom(MediaFrame &tMediaFrame,
            const std::vector<std::vector<cv::Point>> contours);

        void addCircleStroke(MediaFrame &tMediaFrame, cv::Point center,
            int radius, const cv::Scalar &color, int thickness);

        void addCircleYUV(MediaFrame &tMediaFrame, const cv::Scalar color,
            const cv::Point &circleCenter, int radius, int thickness,
            int line_type);

        void addCircleAlphaCustom(MediaFrame &tMediaFrame, const cv::Point &circleCenter,
            int radius);

        void yuvToRgbAndDumpFile(const std::string &dumpFileName, const MediaFrame &frame);

        void yuvDump(const std::string &dumpFileName, const MediaFrame &frame);

        void yuvDumpTempLayer(const std::string &dumpFileName,
            const MediaFrame &frame, int scaledh, int scaledw);

        void setAsTempLayer(MediaFrame &tMediaFrame, MediaFrame &baseFrame, cv::Rect &rect);

        void addWordWithBorder(MediaFrame &dst, const std::string &text, double borderWidth,
            cv::Point &point, cv::Scalar &fontColor, cv::Scalar &borderColor, cv::Scalar &size,
            float fp, const std::string &fontName, int leanType, float inclination);

        void getWordWithBorderSize(const std::string &word, cv::Size &result, cv::Point &offset,
            double borderWidth, cv::Scalar &size, const std::string &fontName, 
            int leanType, float inclination);

    private:
        int getYUVMat(cv::Mat &YUVMat, cv::Mat &BGRMat);
        int getRGBMat(cv::Mat &YUVMat, cv::Mat &BGRMat);
        CvxText *getFont(const std::string &);

    private:
        OpenCVOperator(const OpenCVOperator &);

    private:
        CvxText *m_default_font;
        std::map<std::string, CvxText *> m_font_map;

        // filename => MediaFrame
        std::map<std::string, GifFrameContext> m_gifFrames;
    };

} // namespace hercules
