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

#include "AudioMixer.h"

#include <memory>

namespace hercules
{

    class AudioMixerWrapper
    {
    public:
        AudioMixerWrapper(bool dumpFile, int statsIntervalInFrameNum)
        {
            m_audioMixer = std::make_shared<AudioMixer>(dumpFile, statsIntervalInFrameNum);
        }

        ~AudioMixerWrapper()
        {
        }

        AudioMixerWrapper(const AudioMixerWrapper &rhs)
        {
            m_audioMixer = rhs.m_audioMixer;
        }

        AudioMixerWrapper &operator=(const AudioMixerWrapper &rhs)
        {
            m_audioMixer = rhs.m_audioMixer;
            return *this;
        }

        std::shared_ptr<AudioMixer> operator->()
        {
            return m_audioMixer;
        }

        AudioMixer *getRawPtr()
        {
            return m_audioMixer ? m_audioMixer.get() : NULL;
        }

        std::shared_ptr<AudioMixer> getPtr()
        {
            return m_audioMixer;
        }

    public:
        std::shared_ptr<AudioMixer> m_audioMixer;
    };

} // namespace hercules
