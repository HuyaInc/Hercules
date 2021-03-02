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

#include "CvxText.h"
#include "Log.h"
#include "Util.h"

#include <fcntl.h>
#include <wchar.h>
#include <assert.h>
#include <locale.h>
#include <ctype.h>
#include <math.h>

#include <string>
#include <algorithm>

#define PI 3.14159265

namespace hercules
{

    using namespace cv;

    static void dumpMat(cv::Mat m)
    {
        logInfo(MIXLOG << "depth: " << m.depth()
                      << ", width: " << m.rows
                      << ", height: " << m.cols
                      << ", channels: " << m.channels());
    }

    void rasterCallback(int y, int count, const FT_Span *const spans, void *const user)
    {
        Spans *sptr = reinterpret_cast<Spans *>(user);

        for (int i = 0; i < count; ++i)
        {
            sptr->push_back(Span(spans[i].x, y, spans[i].len, spans[i].coverage));
        }
    }

    void renderSpans(FT_Library &library, FT_Outline *const outline, Spans *spans)
    {
        FT_Raster_Params params;
        memset(&params, 0, sizeof(params));
        params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
        params.gray_spans = rasterCallback;
        params.user = spans;

        FT_Outline_Render(library, outline, &params);
    }

    CvxText::CvxText() : m_library(nullptr), m_face(nullptr), m_leanType(1), m_inclination(0)
    {
    }

    CvxText::CvxText(const std::string &freeType)
        : m_library(nullptr), m_face(nullptr), m_leanType(1), m_inclination(0)
    {
        assert(!freeType.empty());

        int error;

        if ((error = FT_Init_FreeType(&m_library)) != 0)
        {
            logErr(MIXLOG << "init face fail: " << error);
            return;
        }

        if ((error = FT_New_Face(m_library, freeType.c_str(), 0, &m_face)) != 0)
        {
            logErr(MIXLOG << "new face fail: " << error);
            return;
        }

        for (int i = 0; i < m_face->num_fixed_sizes; i++)
        {
            logDebug(MIXLOG << "font size: " << m_face->available_sizes[i].size / 64);
            return;
        }

        if ((error = FT_Set_Char_Size(m_face, 16 * 64, 0, 0, 0)) != 0)
        {
            logErr(MIXLOG << "set font size fail: " << error);
        }

        restoreFont();
    }

    void CvxText::setUpFreeType(const std::string &freeType)
    {
        assert(!freeType.empty());

        if (FT_Init_FreeType(&m_library))
        {
            throw;
        }

        if (FT_New_Face(m_library, freeType.c_str(), 0, &m_face))
        {
            throw;
        }

        restoreFont();
    }

    CvxText::~CvxText()
    {
        if (m_face)
        {
            FT_Done_Face(m_face);
        }

        if (m_library)
        {
            FT_Done_FreeType(m_library);
        }
    }

    void CvxText::getFont(int *type, Scalar *size, bool *underLine,
                          float *diaphaneity, int *leanType, float *inclination)
    {
        *type = m_fontType;
        *size = m_fontSize;
        *underLine = m_fontUnderline;
        *diaphaneity = m_fontDiaphaneity;
        *leanType = m_leanType;
        *inclination = m_inclination;
    }

    void CvxText::setFont(int *type, Scalar *size, bool *underLine,
                          float *diaphaneity, int *leanType, float *inclination)
    {
        if (type)
        {
            if (type >= 0)
            {
                m_fontType = *type;
            }
        }

        if (size)
        {
            m_fontSize.val[0] = fabs(size->val[0]);
            m_fontSize.val[1] = fabs(size->val[1]);
            m_fontSize.val[2] = fabs(size->val[2]);
            m_fontSize.val[3] = fabs(size->val[3]);
        }

        if (underLine)
        {
            m_fontUnderline = *underLine;
        }

        if (diaphaneity)
        {
            m_fontDiaphaneity = *diaphaneity;
        }

        if (leanType && inclination)
        {
            m_leanType = *leanType;
            m_inclination = *inclination;
        }

        float angle = m_inclination * PI / 180;

        switch (m_leanType)
        {
        case 1:
        {
            m_matrix.xx = 0x10000L;
            m_matrix.xy = 0x10000L * tan(angle);
            m_matrix.yx = 0;
            m_matrix.yy = 0x10000L;
            break;
        }
        case 2:
        {
            m_matrix.xx = 0x10000L;
            m_matrix.xy = 0x10000L * sin(angle);
            m_matrix.yx = 0;
            m_matrix.yy = 0x10000L * cos(angle);
            break;
        }
        default:
        {
            m_leanType = 1;
            m_inclination = 0;
            logInfo(MIXLOG << "lean type not support, use default, reset"
                << ", m_leanType: " << m_leanType
                << ", m_inclination: " << m_inclination);
            break;
        }
        }

        int error = 0;
        if (( error =
            FT_Set_Char_Size(m_face, static_cast<int>(m_fontSize.val[0] * 64), 0, 0, 0)) != 0)
        {
            logErr(MIXLOG << "set font size fail: " << error);
        }

        FT_Set_Transform(m_face, &m_matrix, 0);
    }

    void CvxText::restoreFont()
    {
        m_fontType = 0;

        m_fontSize.val[0] = 20;
        m_fontSize.val[1] = 0.5;
        m_fontSize.val[2] = 0.2;
        m_fontSize.val[3] = 0;

        m_fontUnderline = false;

        m_fontDiaphaneity = 0.9f;

        m_leanType = 1;
        m_inclination = 0;

        m_matrix.xx = 0x10000L;
        m_matrix.xy = 0;
        m_matrix.yx = 0;
        m_matrix.yy = 0x10000L;

        FT_Set_Pixel_Sizes(m_face, static_cast<int>(m_fontSize.val[0]), 0);
        FT_Set_Transform(m_face, &m_matrix, 0);
    }

    int CvxText::putText(Mat img, const wchar_t *text, Point &pos, Scalar color,
                         bool isYuv, bool isAlpha, cv::Mat alpha)
    {
        if (img.data == nullptr)
        {
            return -1;
        }

        if (text == nullptr)
        {
            return -1;
        }

        int i = 0;
        for (i = 0; text[i] != '\0'; ++i)
        {
            putWChar(img, text[i], pos, color, isYuv, isAlpha, alpha);
        }

        return i;
    }

    int CvxText::putBoldText(cv::Mat img, const wchar_t *text, cv::Point &pos, cv::Scalar color,
                             bool isYuv, bool isAlpha, cv::Mat alpha, int bold)
    {
        if (img.data == nullptr)
        {
            return -1;
        }

        if (text == nullptr)
        {
            return -1;
        }

        int i = 0;
        for (i = 0; text[i] != '\0'; ++i)
        {
            putBoldWChar(img, text[i], pos, color, isYuv, bold, isAlpha, alpha);
        }

        return i;
    }

    int CvxText::putTextWithBorderYUV(Mat matYuv, int fontHeight, const wchar_t *text,
        double borderWidth, Point &pos, Scalar fontColor, Scalar borderColor)
    {
        if (matYuv.data == nullptr)
        {
            return -1;
        }

        if (text == nullptr)
        {
            return -1;
        }

        int i = 0;
        for (i = 0; text[i] != '\0'; ++i)
        {
            putWCharWithBorderYUV(matYuv, fontHeight, text[i], borderWidth, pos, 
                fontColor, borderColor);
        }

        return i;
    }

    int CvxText::calHeight(wchar_t wc)
    {
        FT_Outline_Embolden(&(m_face->glyph->outline), -24);
        FT_Load_Char(m_face, wc, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT);
        FT_GlyphSlot slot = m_face->glyph;

        int high = 0;
        if (m_leanType == 1)
        {
            high = slot->bitmap.rows;
        }
        else
        {
            high = (slot->bitmap.rows) * cos(m_inclination * PI / 180);
        }

        return high;
    }

    int CvxText::calWidth(const wchar_t *text)
    {
        if (text == nullptr)
        {
            return 0;
        }

        int width = 0;
        int space = m_fontSize.val[0] * m_fontSize.val[1];
        int sep = m_fontSize.val[0] * m_fontSize.val[2];

        for (int i = 0; text[i] != '\0'; ++i)
        {
            FT_Load_Char(m_face, text[i], FT_LOAD_RENDER);
            FT_GlyphSlot slot = m_face->glyph;

            width += (slot->advance.x >> 6) ? (slot->advance.x >> 6) : space + sep;
        }

        if (m_leanType == 1)
        {
            width += calHeight(text[0]) * tan(m_inclination * PI / 180);
        }
        else
        {
            width += calHeight(text[0]) * sin(m_inclination * PI / 180);
        }

        return width;
    }

    void CvxText::calBorderWidthAndHeight(const wchar_t *text, double borderWidth, 
        int &width, int &height)
    {
        width = 0;
        height = 0;

        if (text == nullptr)
        {
            return;
        }

        for (int i = 0; text[i] != '\0'; ++i)
        {
            int w = 0;
            int h = 0;
            getBorderWordSize(text[i], borderWidth, w, h);
            width += w;
            height = max(h, height);
        }
    }

    void CvxText::getBorderWordSize(wchar_t wc, double borderWidth,
        int &width, int &height)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(m_face, (FT_ULong)wc);

        if (FT_Load_Glyph(m_face, glyph_index, FT_LOAD_FORCE_AUTOHINT) != 0)
        {
            return;
        }

        if (m_face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            Spans spans;
            renderSpans(m_library, &m_face->glyph->outline, &spans);

            Spans outlineSpans;

            FT_Fixed radius = borderWidth * 64;
            if (radius <= 0)
                radius = 1.0 * 64;

            FT_Stroker stroker;
            FT_Stroker_New(m_library, &stroker);
            FT_Stroker_Set(stroker,
                           radius,
                           FT_STROKER_LINECAP_ROUND,
                           FT_STROKER_LINEJOIN_ROUND,
                           0);

            FT_Glyph glyph;

            if (FT_Get_Glyph(m_face->glyph, &glyph) == 0)
            {
                FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);

                if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
                {
                    FT_Outline *o = &(reinterpret_cast<FT_OutlineGlyph>(glyph)->outline);
                    renderSpans(m_library, o, &outlineSpans);
                }

                FT_Stroker_Done(stroker);
                FT_Done_Glyph(glyph);

                if (!spans.empty())
                {
                    FontRect rect(spans.front().x, spans.front().y, 
                        spans.front().x, spans.front().y);

                    for (Spans::iterator s = spans.begin();
                         s != spans.end(); ++s)
                    {
                        rect.include(Vec2(s->x, s->y));
                        rect.include(Vec2(s->x + s->width - 1, s->y));
                    }

                    for (Spans::iterator s = outlineSpans.begin();
                         s != outlineSpans.end(); ++s)
                    {
                        rect.include(Vec2(s->x, s->y));
                        rect.include(Vec2(s->x + s->width - 1, s->y));
                    }

                    width = rect.width() + 2;
                    height = rect.height();
                }
            }
        }
    }

    void CvxText::getStrSize(const wchar_t *str, int &width, int &height)
    {
        if (!str)
        {
            return;
        }

        width = calWidth(str);
        height = calHeight(str[0]);
    }

    void CvxText::getBorderStrSize(const wchar_t *str, double borderWidth,
        int &width, int &height)
    {
        if (!str)
        {
            return;
        }

        calBorderWidthAndHeight(str, borderWidth, width, height);
    }

    void CvxText::putWChar(Mat img, wchar_t wc, Point &pos, Scalar color,
        bool isYuv, bool isAlpha /*= false*/, cv::Mat alpha /* = cv::Mat()*/)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(m_face, (FT_ULong)wc);
        FT_Load_Glyph(m_face, glyph_index, FT_LOAD_FORCE_AUTOHINT);
        FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);

        FT_GlyphSlot slot = m_face->glyph;

        int rows = slot->bitmap.rows;
        int cols = slot->bitmap.width;

        int nImgHeight = img.rows;
        int nImgWidth = img.cols;

        int locX = pos.x + slot->bitmap_left;
        int locY = pos.y + slot->bitmap.rows - slot->bitmap_top;

        int nWidth = img.cols;
        int nHight = img.rows * 2 / 3;
        Mat imgY = img(Rect(0, 0, nWidth, nHight));
        Mat imgU(nHight / 2, nWidth / 2, CV_8UC1, imgY.data + nHight * nWidth);
        Mat imgV(nHight / 2, nWidth / 2, CV_8UC1,
                 imgY.data + nHight * nWidth + nHight * nWidth / 4);

        if (isYuv)
        {
            nImgHeight = nImgHeight * 2 / 3;
        }

        double colorAlpha = color.val[3];

        uchar Y = (uchar)(rgb2y(color.val[2], color.val[1], color.val[0]));
        uchar U = (uchar)(rgb2u(color.val[2], color.val[1], color.val[0]));
        uchar V = (uchar)(rgb2v(color.val[2], color.val[1], color.val[0]));

        for (int i = 0; i < rows; ++i)
        {
            for (int j = 0; j < cols; ++j)
            {
                int off = i * slot->bitmap.pitch + j;

                if (slot->bitmap.buffer[off])
                {
                    int r = locY - (rows - 1 - i);
                    int c = locX + j;
                    int value = (slot->bitmap.buffer[off]);
                    float diaphaneity = value / 255.0 * m_fontDiaphaneity * colorAlpha;

                    if (r >= 0 && r < nImgHeight && c >= 0 && c < nImgWidth)
                    {
                        switch (img.channels())
                        {
                        case 1:
                        {
                            if (!isYuv)
                            {
                                img.ptr<uchar>(r)[c] = uchar(img.ptr<uchar>(r)[c] 
                                    * (1 - diaphaneity) + color.val[0] * diaphaneity);
                            }
                            else
                            {
                                imgY.ptr<uchar>(r)[c] = uchar(imgY.ptr<uchar>(r)[c] 
                                    * (1 - diaphaneity) + Y * diaphaneity);
                                imgU.ptr<uchar>(r / 2)[c / 2] = uchar(imgU.ptr<uchar>(r / 2)[c / 2] 
                                    * (1 - diaphaneity) + U * diaphaneity);
                                imgV.ptr<uchar>(r / 2)[c / 2] = uchar(imgV.ptr<uchar>(r / 2)[c / 2] 
                                    * (1 - diaphaneity) + V * diaphaneity);
                            }
                        }
                        break;

                        case 3:
                        {
                            Mat_<Vec3b> _I = img;
                            _I(r, c)[0] = uchar(_I(r, c)[0] 
                                * (1 - diaphaneity) + color.val[2] * diaphaneity);
                            _I(r, c)[1] = uchar(_I(r, c)[1] 
                                * (1 - diaphaneity) + color.val[1] * diaphaneity);
                            _I(r, c)[2] = uchar(_I(r, c)[2] 
                                * (1 - diaphaneity) + color.val[0] * diaphaneity);
                            break;
                        }
                        }

                        if (isAlpha && m_fontDiaphaneity > alpha.ptr<float>(r)[c])
                        {
                            alpha.ptr<float>(r)[c] = m_fontDiaphaneity;
                        }
                    }
                }
            }
        }

        int space = m_fontSize.val[0] * m_fontSize.val[1];
        int sep = m_fontSize.val[0] * m_fontSize.val[2];

        pos.x += (slot->advance.x >> 6) ? (slot->advance.x >> 6) : space + sep;
    }

    void CvxText::putBoldWChar(Mat img, wchar_t wc, Point &pos, Scalar color, bool isYuv,
        int bold, bool isAlpha /*= false*/, cv::Mat alpha /*= cv::Mat()*/)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(m_face, (FT_ULong)wc);
        FT_Load_Glyph(m_face, glyph_index, FT_LOAD_FORCE_AUTOHINT);
        FT_GlyphSlot slot = m_face->glyph;

        FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);

        FT_Bitmap_Embolden(m_library, &slot->bitmap, bold, bold);

        int rows = slot->bitmap.rows;
        int cols = slot->bitmap.width;

        int nImgHeight = img.rows;
        int nImgWidth = img.cols;

        int locX = pos.x + slot->bitmap_left;
        int locY = pos.y + slot->bitmap.rows - slot->bitmap_top;

        int nWidth = img.cols;
        int nHeight = img.rows * 2 / 3;
        Mat imgY = img(Rect(0, 0, nWidth, nHeight));
        Mat imgU(nHeight / 2, nWidth / 2, CV_8UC1, imgY.data + nHeight * nWidth);
        Mat imgV(nHeight / 2, nWidth / 2, CV_8UC1,
                 imgY.data + nHeight * nWidth + nHeight * nWidth / 4);

        if (isYuv)
        {
            nImgHeight = nImgHeight * 2 / 3;
        }

        uchar Y = (uchar)(rgb2y(color.val[2], color.val[1], color.val[0]));
        uchar U = (uchar)(rgb2u(color.val[2], color.val[1], color.val[0]));
        uchar V = (uchar)(rgb2v(color.val[2], color.val[1], color.val[0]));

        for (int i = 0; i < rows; ++i)
        {
            for (int j = 0; j < cols; ++j)
            {
                int off = i * slot->bitmap.pitch + j;

                if (slot->bitmap.buffer[off])
                {
                    int r = locY - (rows - 1 - i);
                    int c = locX + j;
                    int value = (slot->bitmap.buffer[off]);
                    float diaphaneity = value / 255.0 * m_fontDiaphaneity;

                    if (r >= 0 && r < nImgHeight && c >= 0 && c < nImgWidth)
                    {
                        switch (img.channels())
                        {
                        case 1:
                        {
                            if (!isYuv)
                            {
                                img.ptr<uchar>(r)[c] = uchar(img.ptr<uchar>(r)[c] 
                                    * (1 - diaphaneity) + color.val[0] * diaphaneity);
                            }
                            else
                            {
                                imgY.ptr<uchar>(r)[c] = uchar(imgY.ptr<uchar>(r)[c] 
                                    * (1 - diaphaneity) + Y * diaphaneity);
                                imgU.ptr<uchar>(r / 2)[c / 2] = uchar(imgU.ptr<uchar>(r / 2)[c / 2] 
                                    * (1 - diaphaneity) + U * diaphaneity);
                                imgV.ptr<uchar>(r / 2)[c / 2] = uchar(imgV.ptr<uchar>(r / 2)[c / 2] 
                                    * (1 - diaphaneity) + V * diaphaneity);
                            }
                        }
                        break;

                        case 3:
                        {
                            Mat_<Vec3b> _I = img;
                            _I(r, c)[0] = uchar(_I(r, c)[0] 
                                * (1 - diaphaneity) + color.val[2] * diaphaneity);
                            _I(r, c)[1] = uchar(_I(r, c)[1] 
                                * (1 - diaphaneity) + color.val[1] * diaphaneity);
                            _I(r, c)[2] = uchar(_I(r, c)[2] 
                                * (1 - diaphaneity) + color.val[0] * diaphaneity);
                            break;
                        }
                        }

                        if (isAlpha && m_fontDiaphaneity > alpha.ptr<float>(r)[c])
                        {
                            alpha.ptr<float>(r)[c] = m_fontDiaphaneity;
                        }
                    }
                }
            }
        }

        int space = m_fontSize.val[0] * m_fontSize.val[1];
        int sep = m_fontSize.val[0] * m_fontSize.val[2];

        pos.x += (slot->advance.x >> 6) ? (slot->advance.x >> 6) : space + sep;
    }

    void CvxText::putWCharWithBorderYUV(Mat matYUV, int fontHeight, wchar_t wc,
        double borderWidth, Point &pos, Scalar fontColor, Scalar borderColor)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(m_face, (FT_ULong)wc);

        if (FT_Load_Glyph(m_face, glyph_index, FT_LOAD_FORCE_AUTOHINT) != 0)
        {
            return;
        }

        if (m_face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            Spans spans;
            renderSpans(m_library, &m_face->glyph->outline, &spans);

            Spans outlineSpans;

            FT_Fixed radius = borderWidth * 64;
            if (radius <= 0)
                radius = 1.0 * 64;

            FT_Stroker stroker;
            FT_Stroker_New(m_library, &stroker);
            FT_Stroker_Set(stroker,
                           radius,
                           FT_STROKER_LINECAP_ROUND,
                           FT_STROKER_LINEJOIN_ROUND,
                           0);

            FT_Glyph glyph;

            if (FT_Get_Glyph(m_face->glyph, &glyph) == 0)
            {
                FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);

                if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
                {
                    FT_Outline *o = &(reinterpret_cast<FT_OutlineGlyph>(glyph)->outline);
                    renderSpans(m_library, o, &outlineSpans);
                }

                FT_Stroker_Done(stroker);
                FT_Done_Glyph(glyph);

                int outW = matYUV.cols;
                int outH = matYUV.rows * 2 / 3;
                Mat matY = matYUV(Rect(0, 0, outW, outH));
                Mat matU(outH / 2, outW / 2, CV_8UC1, matY.data + outH * outW);
                Mat matV(outH / 2, outW / 2, CV_8UC1, matY.data + outH * outW + outH * outW / 4);

                if (!spans.empty())
                {
                    FontRect rect(spans.front().x, spans.front().y, 
                        spans.front().x, spans.front().y);

                    for (Spans::iterator s = spans.begin();
                         s != spans.end(); ++s)
                    {
                        rect.include(Vec2(s->x, s->y));
                        rect.include(Vec2(s->x + s->width - 1, s->y));
                    }

                    for (Spans::iterator s = outlineSpans.begin();
                         s != outlineSpans.end(); ++s)
                    {
                        rect.include(Vec2(s->x, s->y));
                        rect.include(Vec2(s->x + s->width - 1, s->y));
                    }

                    int imgWidth = rect.width();
                    int imgHeight = rect.height();
                    int imgSize = imgWidth * imgHeight;

                    uchar borderY = (uchar)(
                        rgb2y(borderColor.val[2], borderColor.val[1], borderColor.val[0]));
                    uchar borderU = (uchar)(
                        rgb2u(borderColor.val[2], borderColor.val[1], borderColor.val[0]));
                    uchar borderV = (uchar)(
                        rgb2v(borderColor.val[2], borderColor.val[1], borderColor.val[0]));

                    int y = pos.y + (fontHeight - rect.height()) / 2;

                    for (Spans::iterator s = outlineSpans.begin(); s != outlineSpans.end(); ++s)
                    {
                        int c = 0;
                        for (int w = 0; w < s->width; ++w)
                        {
                            int r = y + (imgHeight - 1 - (s->y - rect.ymin));
                            int c = pos.x + s->x - rect.xmin + w;
                            float diaphaneity = s->coverage / 255.0 * m_fontDiaphaneity;
                            matY.ptr<uchar>(r)[c] = uchar(matY.ptr<uchar>(r)[c] 
                                * (1 - diaphaneity) + borderY * diaphaneity);
                            matU.ptr<uchar>(r / 2)[c / 2] = uchar(matU.ptr<uchar>(r / 2)[c / 2] 
                                * (1 - diaphaneity) + borderU * diaphaneity);
                            matV.ptr<uchar>(r / 2)[c / 2] = uchar(matV.ptr<uchar>(r / 2)[c / 2] 
                                * (1 - diaphaneity) + borderV * diaphaneity);
                        }
                    }

                    uchar fontY = (uchar)(
                        rgb2y(fontColor.val[2], fontColor.val[1], fontColor.val[0]));
                    uchar fontU = (uchar)(
                        rgb2u(fontColor.val[2], fontColor.val[1], fontColor.val[0]));
                    uchar fontV = (uchar)(
                        rgb2v(fontColor.val[2], fontColor.val[1], fontColor.val[0]));

                    for (Spans::iterator s = spans.begin(); s != spans.end(); ++s)
                    {
                        for (int w = 0; w < s->width; ++w)
                        {
                            int r = y + (imgHeight - 1 - (s->y - rect.ymin));
                            int c = pos.x + s->x - rect.xmin + w;

                            float diaphaneity = s->coverage / 255.0 * m_fontDiaphaneity;
                            matY.ptr<uchar>(r)[c] = uchar(matY.ptr<uchar>(r)[c] 
                                * (1 - diaphaneity) + fontY * diaphaneity);
                            matU.ptr<uchar>(r / 2)[c / 2] = uchar(matU.ptr<uchar>(r / 2)[c / 2] 
                                * (1 - diaphaneity) + fontU * diaphaneity);
                            matV.ptr<uchar>(r / 2)[c / 2] = uchar(matV.ptr<uchar>(r / 2)[c / 2] 
                                * (1 - diaphaneity) + fontV * diaphaneity);
                        }
                    }

                    int space = m_fontSize.val[0] * m_fontSize.val[1];
                    int sep = m_fontSize.val[0] * m_fontSize.val[2];

                    pos.x += imgWidth + 2;
                }
            }
        }
    }

    int CvxText::putTextBorder(Mat matYUV, const wchar_t *text, double borderWidth,
        Point &pos, Scalar fontColor, Scalar borderColor)
    {
        if (matYUV.data == nullptr)
        {
            return -1;
        }

        if (text == nullptr)
        {
            return -1;
        }

        int i = 0;
        for (i = 0; text[i] != '\0'; ++i)
        {
            putWCharBorder(matYUV, text[i], borderWidth, pos, fontColor, borderColor);
        }

        return i;
    }

    void CvxText::getWCharWithBorderSize(wchar_t wc, double borderWidth,
        cv::Point &pos, int &top, int &bottom, int &left, int &right)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(m_face, (FT_ULong)wc);

        if (FT_Load_Glyph(m_face, glyph_index, FT_LOAD_FORCE_AUTOHINT) != 0)
        {
            return;
        }

        if (m_face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            Spans outlineSpans;

            FT_Fixed radius = borderWidth * 64;
            if (radius <= 0)
                radius = 1.0 * 64;

            FT_Stroker stroker;
            FT_Stroker_New(m_library, &stroker);
            FT_Stroker_Set(stroker,
                           radius,
                           FT_STROKER_LINECAP_ROUND,
                           FT_STROKER_LINEJOIN_ROUND,
                           0);

            FT_Glyph glyph;

            FT_GlyphSlot slot = m_face->glyph;
            if (FT_Get_Glyph(m_face->glyph, &glyph) == 0)
            {
                FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);

                if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
                {
                    FT_Outline *o = &(reinterpret_cast<FT_OutlineGlyph>(glyph)->outline);
                    renderSpans(m_library, o, &outlineSpans);
                }

                FT_Stroker_Done(stroker);
                FT_Done_Glyph(glyph);

                if (!outlineSpans.empty())
                {
                    FontRect rect(outlineSpans.front().x, outlineSpans.front().y,
                                  outlineSpans.front().x, outlineSpans.front().y);

                    for (Spans::iterator s = outlineSpans.begin();
                         s != outlineSpans.end(); ++s)
                    {
                        rect.include(Vec2(s->x, s->y));
                        rect.include(Vec2(s->x + s->width - 1, s->y));
                    }

                    for (Spans::iterator s = outlineSpans.begin(); s != outlineSpans.end(); ++s)
                    {
                        left = std::min(left, pos.x + s->x);
                        right = std::max(right, pos.x + s->x + s->width);
                        top = std::min(top, pos.y - s->y);
                        bottom = std::max(bottom, pos.y - s->y);
                    }
                    int space = m_fontSize.val[0] * m_fontSize.val[1];
                    int sep = m_fontSize.val[0] * m_fontSize.val[2];

                    pos.x += (slot->advance.x >> 6) ? (slot->advance.x >> 6) : space + sep;
                }
            }
        }
    }

    void CvxText::putWCharBorder(Mat matYUV, wchar_t wc, double borderWidth,
        Point &pos, Scalar fontColor, Scalar borderColor)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(m_face, (FT_ULong)wc);

        if (FT_Load_Glyph(m_face, glyph_index, FT_LOAD_FORCE_AUTOHINT) != 0)
        {
            return;
        }
        FT_GlyphSlot slot = m_face->glyph;
        if (m_face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            Spans outlineSpans;

            FT_Fixed radius = borderWidth * 64;
            if (radius <= 0)
                radius = 1.0 * 64;

            FT_Stroker stroker;
            FT_Stroker_New(m_library, &stroker);
            FT_Stroker_Set(stroker,
                           radius,
                           FT_STROKER_LINECAP_ROUND,
                           FT_STROKER_LINEJOIN_ROUND,
                           0);

            FT_Glyph glyph;

            if (FT_Get_Glyph(m_face->glyph, &glyph) == 0)
            {
                FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);

                if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
                {
                    FT_Outline *o = &(reinterpret_cast<FT_OutlineGlyph>(glyph)->outline);
                    renderSpans(m_library, o, &outlineSpans);
                }

                FT_Stroker_Done(stroker);
                FT_Done_Glyph(glyph);

                int outW = matYUV.cols;
                int outH = matYUV.rows * 2 / 3;
                Mat matY = matYUV(Rect(0, 0, outW, outH));
                Mat matU(outH / 2, outW / 2, CV_8UC1, matY.data + outH * outW);
                Mat matV(outH / 2, outW / 2, CV_8UC1, matY.data + outH * outW + outH * outW / 4);

                if (!outlineSpans.empty())
                {
                    FontRect rect(outlineSpans.front().x, outlineSpans.front().y,
                                  outlineSpans.front().x, outlineSpans.front().y);

                    for (Spans::iterator s = outlineSpans.begin();
                         s != outlineSpans.end(); ++s)
                    {
                        rect.include(Vec2(s->x, s->y));
                        rect.include(Vec2(s->x + s->width - 1, s->y));
                    }

                    int imgWidth = rect.width();
                    int imgHeight = rect.height();
                    int imgSize = imgWidth * imgHeight;

                    uchar borderY = (uchar)(
                        rgb2y(borderColor.val[2], borderColor.val[1], borderColor.val[0]));
                    uchar borderU = (uchar)(
                        rgb2u(borderColor.val[2], borderColor.val[1], borderColor.val[0]));
                    uchar borderV = (uchar)(
                        rgb2v(borderColor.val[2], borderColor.val[1], borderColor.val[0]));

                    for (Spans::iterator s = outlineSpans.begin(); s != outlineSpans.end(); ++s)
                    {
                        int c = 0;
                        for (int w = 0; w < s->width; ++w)
                        {
                            int r = pos.y - s->y;
                            int c = pos.x + s->x + w;
                            float diaphaneity = s->coverage / 255.0 * m_fontDiaphaneity;
                            matY.ptr<uchar>(r)[c] = uchar(matY.ptr<uchar>(r)[c] 
                                * (1 - diaphaneity) + borderY * diaphaneity);
                            matU.ptr<uchar>(r / 2)[c / 2] = uchar(matU.ptr<uchar>(r / 2)[c / 2] 
                                * (1 - diaphaneity) + borderU * diaphaneity);
                            matV.ptr<uchar>(r / 2)[c / 2] = uchar(matV.ptr<uchar>(r / 2)[c / 2] 
                                * (1 - diaphaneity) + borderV * diaphaneity);
                        }
                    }
                }
            }
        }
        int space = m_fontSize.val[0] * m_fontSize.val[1];
        int sep = m_fontSize.val[0] * m_fontSize.val[2];

        pos.x += (slot->advance.x >> 6) ? (slot->advance.x >> 6) : space + sep;
    }

    void CvxText::getWordWithBorderSize(const wchar_t *text, double borderWidth,
        int &xOff, int &yOff, int &width, int &height)
    {
        if (text == nullptr)
        {
            return;
        }

        width = 0;
        height = 0;

        int top = 0, bottom = 0, left = 0, right = 0;
        cv::Point pos;
        for (int i = 0; text[i] != '\0'; ++i)
        {
            getWCharWithBorderSize(text[i], borderWidth, pos, top, bottom, left, right);
        }
        xOff = left;
        yOff = top;
        width = right - left;
        height = bottom - top;
    }

} // namespace hercules
