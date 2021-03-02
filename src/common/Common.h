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

namespace hercules
{
    // video audio property
    constexpr int DEFAULT_WIDTH = 1920;
    constexpr int DEFAULT_HEIGHT = 1080;
    constexpr int DEFAULT_FPS = 30;
    constexpr int DEFAULT_VIDEO_BPS = 1000;
    constexpr int DEFAULT_CHANNEL_NUM = 2;
    constexpr int DEFAULT_SAMPLE_RATE = 44100;
    constexpr int DEFAULT_AUDIO_BPS = 192;

    // decoder property
    constexpr int DEFAULT_DECODER_EXTRADATA_SIZE = 1024;
    constexpr int DEFAULT_DECODER_THREAD_COUNT = 1;

    // encoder property
    constexpr int MAX_FRAME_METADATA_SIZE = 100;
    constexpr int MIN_FPS = 1;
    constexpr int MAX_FPS = 100;
    constexpr int MIN_BPS = 100;
    constexpr int MAX_BPS = 20000;
} // namespace hercules
