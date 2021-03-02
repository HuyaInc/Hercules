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

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_STROKER_H
#include "freetype/ftbitmap.h"
#include "freetype/ftoutln.h"
#include "opencv2/opencv.hpp"

#include <string>
#include <vector>

namespace hercules
{

    struct Vec2
    {
        Vec2() {}
        Vec2(float a, float b)
            : x(a), y(b) {}

        float x, y;
    };

    struct FontRect
    {
        FontRect() {}
        FontRect(float left, float top, float right, float bottom)
            : xmin(left), xmax(right), ymin(top), ymax(bottom) {}

        void include(const Vec2 &r)
        {
            xmin = MIN(xmin, r.x);
            ymin = MIN(ymin, r.y);
            xmax = MAX(xmax, r.x);
            ymax = MAX(ymax, r.y);
        }

        float width() const
        {
            return xmax - xmin + 1;
        }
        float height() const
        {
            return ymax - ymin + 1;
        }

        float xmin, xmax, ymin, ymax;
    };

    struct Span
    {
        Span() {}
        Span(int _x, int _y, int _width, int _coverage)
            : x(_x), y(_y), width(_width), coverage(_coverage)
        {
        }

        int x;
        int y;
        int width;
        int coverage;
    };

    typedef std::vector<Span> Spans;

    class CvxText
    {
    public:
        CvxText &operator=(const CvxText &text)
        {
            m_library = text.m_library;
            m_face = text.m_face;

            return *this;
        }

        CvxText();
        explicit CvxText(const std::string &freeType);
        virtual ~CvxText();

        void setUpFreeType(const std::string &freeType);
        void getFont(int *type, cv::Scalar *size = NULL, bool *underLine = NULL,
            float *diaphaneity = NULL, int *leanType = NULL, float *inclination = NULL);
        void setFont(int *type, cv::Scalar *size = NULL, bool *underLine = NULL,
            float *diaphaneity = NULL, int *leanType = NULL, float *inclination = NULL);
        void restoreFont();
        int putText(cv::Mat img, const wchar_t *text, cv::Point &pos, cv::Scalar color,
            bool isYuv = false, bool isAlpha = false, cv::Mat alpha = cv::Mat());
        int putBoldText(cv::Mat img, const wchar_t *text, cv::Point &pos, cv::Scalar color,
            bool isYuv = false, bool isAlpha = false, 
            cv::Mat alpha = cv::Mat(), int bold = 2);
        int putTextWithBorderYUV(cv::Mat matYuv, int fontHeight, const wchar_t *text,
            double borderWidth, cv::Point &pos, 
            cv::Scalar fontColor, cv::Scalar borderColor);

        int calWidth(const wchar_t *text);
        int calHeight(wchar_t wc);
        void calBorderWidthAndHeight(const wchar_t *text, double borderWidth,
            int &width, int &height);
        void getBorderWordSize(wchar_t ch, double borderWidth, 
            int &width, int &height);
        void getStrSize(const wchar_t *, int &width, int &height);
        void getBorderStrSize(const wchar_t *, double borderWidth, int &width, int &height);

    private:
        void putWChar(cv::Mat img, wchar_t wc, cv::Point &pos, cv::Scalar color,
            bool isYuv = false, bool isAlpha = false, cv::Mat alpha = cv::Mat());
        void putBoldWChar(cv::Mat img, wchar_t wc, cv::Point &pos, cv::Scalar color,
            bool isYuv, int bold, bool isAlpha = false, 
            cv::Mat alpha = cv::Mat());
        void putWCharWithBorderYUV(cv::Mat matYUV, int fontHeight, wchar_t wc,
            double borderWidth, cv::Point &pos, 
            cv::Scalar fontColor, cv::Scalar borderColor);

    public:
        int putTextBorder(cv::Mat matYuv, const wchar_t *text, double borderWidth,
            cv::Point &pos, cv::Scalar fontColor, cv::Scalar borderColor);
        void getWordWithBorderSize(const wchar_t *, double borderWidth,
            int &xOff, int &yOff, int &width, int &height);

    private:
        void putWCharBorder(cv::Mat matYuv, wchar_t wc, double borderWidth,
            cv::Point &pos, cv::Scalar fontColor, cv::Scalar borderColor);
        void getWCharWithBorderSize(wchar_t wc, double borderWidth,
            cv::Point &pos, int &top, int &bottom, int &left, int &right);

    private:
        FT_Library m_library;
        FT_Face m_face;
        FT_Matrix m_matrix;

        int m_fontType;
        cv::Scalar m_fontSize;
        bool m_fontUnderline;
        float m_fontDiaphaneity;

        int m_leanType;
        float m_inclination;
    };

} // namespace hercules
