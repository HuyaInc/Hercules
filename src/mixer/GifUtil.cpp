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

#include "GifUtil.h"
#include "Common.h"
#include "FreeImage.h"

#include "opencv2/opencv.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace hercules
{

    constexpr int GIF_HEADER_LEN = 7; // GIF89A GIF87A + 1

    static bool FreeImage_GetMetadataEx(FREE_IMAGE_MDMODEL model,
        FIBITMAP *dib, const char *key, FREE_IMAGE_MDTYPE type, FITAG **tag)
    {
        if (FreeImage_GetMetadata(model, dib, key, tag))
        {
            if (FreeImage_GetTagType(*tag) == type)
            {
                return true;
            }
        }
        return false;
    }

    static bool imDecodeGif(const std::string &filename, GifCVContext &gifCtx)
    {
        // load the FreeImage function lib
        FreeImage_Initialise(true);
        FREE_IMAGE_FORMAT fif = FIF_GIF;
        FIBITMAP *fiBmp = FreeImage_Load(fif, filename.c_str(), GIF_DEFAULT);
        FIMULTIBITMAP *pGIF = FreeImage_OpenMultiBitmap(fif, filename.c_str(), 
            0, 1, 0, GIF_PLAYBACK);
        int gifImgCnt = FreeImage_GetPageCount(pGIF);

        int delayTime = 0;
        FITAG *tag;
        if (FreeImage_GetMetadataEx(FIMD_ANIMATION, fiBmp, "FrameTime", FIDT_LONG, &tag))
        {
            delayTime = *reinterpret_cast<int *>(const_cast<void *>(FreeImage_GetTagValue(tag)));
        }

        FIBITMAP *pFrame;
        int width, height;
        width = FreeImage_GetWidth(fiBmp);
        height = FreeImage_GetHeight(fiBmp);

        gifCtx.m_property.m_imgCount = gifImgCnt;
        gifCtx.m_property.m_width = width;
        gifCtx.m_property.m_height = height;
        gifCtx.m_property.m_delay = delayTime < 20 ? 100 : delayTime;

        RGBQUAD *ptrPalette = new RGBQUAD;
        for (int curFrame = 0; curFrame < gifImgCnt; ++curFrame)
        {
            cv::Mat mat = cv::Mat(cv::Size(width, height), CV_8UC3);
            pFrame = FreeImage_LockPage(pGIF, curFrame);
            unsigned char *ptrImgDataPerLine;
            for (int i = 0; i < height; i++)
            {
                ptrImgDataPerLine = mat.data + i * mat.step[0];
                for (int j = 0; j < width; j++)
                {
                    FreeImage_GetPixelColor(pFrame, j, height - i, ptrPalette);
                    ptrImgDataPerLine[3 * j] = ptrPalette->rgbBlue;
                    ptrImgDataPerLine[3 * j + 1] = ptrPalette->rgbGreen;
                    ptrImgDataPerLine[3 * j + 2] = ptrPalette->rgbRed;
                }
            }
            gifCtx.m_imgs.push_back(mat);
            FreeImage_UnlockPage(pGIF, pFrame, 1);
        }

        delete ptrPalette;
        FreeImage_CloseMultiBitmap(pGIF, GIF_PLAYBACK);
        FreeImage_Unload(fiBmp);
        FreeImage_DeInitialise();
        return true;
    }

    bool GifUtil::parseGif(const std::string &filename, GifCVContext &ctx)
    {
        if (isGif(filename))
        {
            return imDecodeGif(filename, ctx);
        }
        return false;
    }

    bool isGif(const std::string &filename)
    {
        if (filename.empty())
        {
            return false;
        }
        std::ifstream fp(filename, std::ios::binary);
        if (fp.is_open())
        {
            char contents[GIF_HEADER_LEN] = {0};
            fp.read(contents, sizeof(contents) - 1);
            if ((contents[0] == 'G' && contents[1] == 'I' && contents[2] == 'F') &&
                (contents[3] == '8' && (contents[4] == '9' || contents[4] == '7') &&
                 contents[5] == 'a'))
            {
                fp.close();
                return true;
            }
        }
        fp.close();
        return false;
    }

    bool GifUtil::loadGif(GifFrameContext &ctx, const std::string &filename, bool reload)
    {
        if (filename.empty())
        {
            return false;
        }

        {
            std::unique_lock<std::mutex> lock(m_taskMutex);
            if (m_loadTask.find(filename) != m_loadTask.end())
            {
                return false;
            }
        }

        {
            std::unique_lock<std::mutex> lock(m_gifMutex);
            if (!reload && m_gifFrames.find(filename) != m_gifFrames.end())
            {
                ctx = m_gifFrames[filename];
                m_gifFrames.erase(filename);
                return true;
            }
        }

        ThreadPool::getInstance()->enqueue([filename, this]() {
            {
                std::unique_lock<std::mutex> lock(m_taskMutex);
                if (m_loadTask.find(filename) != m_loadTask.end())
                {
                    return;
                }
                m_loadTask.insert(filename);
            }
            GifCVContext gifCVCtx;
            if (parseGif(filename, gifCVCtx))
            {
                GifFrameContext gifFrameCtx;
                loadGif(gifCVCtx, gifFrameCtx);
                {
                    std::unique_lock<std::mutex> lock(m_gifMutex);
                    m_gifFrames[filename] = gifFrameCtx;
                }
                {
                    std::unique_lock<std::mutex> lock(m_taskMutex);
                    m_loadTask.erase(filename);
                }
            }
            else
            {
                {
                    std::unique_lock<std::mutex> lock(m_taskMutex);
                    m_loadTask.erase(filename);
                }
            }
        });
        return false;
    }

    void GifUtil::loadGif(GifCVContext &cvCtx, GifFrameContext &frameCtx)
    {
        frameCtx.m_property = cvCtx.m_property;
        for (cv::Mat &mRgbLogo : cvCtx.m_imgs)
        {
            try
            {
                MediaFrame logo;
                bool isAlpha = false;
                int w = mRgbLogo.cols, h = mRgbLogo.rows;
                {
                    w = w / 2 * 2;
                    h = h / 2 * 2;
                    cv::resize(mRgbLogo, mRgbLogo, cv::Size(w, h), 0.0, 0.0, cv::INTER_LINEAR);
                }
                std::vector<cv::Mat> channels;
                cv::split(mRgbLogo, channels);

                if (channels.size() > 3)
                {
                    isAlpha = true;
                    channels[3].convertTo(channels[3], CV_32FC1); // float
                    channels[3] = channels[3] / 255.0;
                    logo.setAlpha(channels[3]);
                }
                AVFrame *avFrame = av_frame_alloc();
                avFrame->width = w;
                avFrame->height = h;
                avFrame->format = AV_PIX_FMT_YUV420P;

                int yuvFrameSize = avpicture_get_size(AV_PIX_FMT_YUV420P, w, h);
                uint8_t *buffer = reinterpret_cast<uint8_t *>(av_malloc(yuvFrameSize));
                avpicture_fill(reinterpret_cast<AVPicture *>(avFrame), 
                    buffer, AV_PIX_FMT_YUV420P, w, h);
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
                frameCtx.m_imgs.push_back(logo);
            }
            catch (cv::Exception &ex)
            {
                logErr(MIXLOG << "CV ERROR! msg:" << ex.what());
            }
            catch (std::exception &ex)
            {
                logErr(MIXLOG << __FUNCTION__ << "ERROR! msg:" << ex.what());
            }
        }

        return;
    }

} // namespace hercules
