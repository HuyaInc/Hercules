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

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
#include "libavformat/avio.h"
#include "libavutil/avassert.h"
}

#include <deque>
#include <string>

namespace hercules
{
    class FFmpegAudioMixer
    {
    public:
        FFmpegAudioMixer() {}
        ~FFmpegAudioMixer() {}

        void initCodec();
        void mixFrame(MediaFrame &src, MediaFrame &dst);

    private:
        void mix(std::string &dst, std::string &src);

    private:
        AVCodec *m_audioCodec;
        AVCodecContext *m_audioCodecCtx;
        AVFrame *m_audioEncFrame;
        uint8_t *m_audioSamples;
    };
} // namespace hercules
