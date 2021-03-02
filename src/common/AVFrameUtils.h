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
#include <string>

namespace hercules
{
    bool parseAvcConfig(std::string &framePayload, std::string &avcConfig);
    bool parseVideoToFlv(std::string &framePayload, std::string &avcConfig);
    bool parseYUV(std::string &framePayload, std::string &yuvHeader);
    uint32_t getYUVTimestamp(const uint8_t *data, int len);
    uint32_t getVideoFrameType(const std::string &framePayload);
    bool isVideoIFrame(const std::string &framePayload);
    bool isH265VideoFrame(const std::string &framePayload);

    bool parseAudioToFlv(std::string &framePayload, std::string &aacConfig);
    bool parseAdtsAudioToFlv(std::string &framePayload, uint32_t dts, std::string &aacConfig);
    void extractFlvVideoHeader(const std::string &flvTag, std::string &rawContent,
                                   bool addHeaderSize = true);
    void extractFlvAudioHeader(const std::string &flvTag, std::string &rawContent);

} // namespace hercules
