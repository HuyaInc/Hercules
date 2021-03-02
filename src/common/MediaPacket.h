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
#include "Util.h"

extern "C"
{
#include "libavcodec/avcodec.h"
}

#include <algorithm>
#include <string>

namespace hercules
{

    class MediaPacket : public MediaBase
    {
    public:
        MediaPacket();
        ~MediaPacket();

        MediaPacket(const MediaPacket &rhs);
        MediaPacket &operator=(const MediaPacket &rhs);

        static void appendPreTagSize(std::string &tag);

        static int parseFlvTagHeader(const uint8_t *data, int size, MediaType &mediaType,
            uint32_t &dataSize, uint32_t &timeStamp, uint32_t &streamId);

        static int genMediaPacketFromFlvWithHeader(const uint8_t *data,
            int size, MediaPacket &packet);

        static int genMediaPacketFromFlvWithHeader(uint32_t timestamp,
            const uint8_t *data, int size, MediaPacket &packet);

        static int genMediaPacketFromFlvWithoutHeader(MediaType mediaType,
            uint64_t dts, const uint8_t *data, int size, MediaPacket &packet);
            
        static int mediaPacketToFlvWithoutHeader(const MediaPacket &packet,
            std::string &flvWithoutHeader);
            
        static int mediaPacketToFlvWithHeader(const MediaPacket &packet, std::string &flvWithHeader);

        void setForward(const std::string &str) { m_forward = str; }
        const std::string &getForward() const { return m_forward; }

        AVPacket *getAVPacket() const { return m_packet; }
        void setAVPacket(AVPacket *packet)
        {
            ref();
            m_packet = packet;

            m_avpacketRefCount.fetch_add(1);
        }

        void setGlobalHeader(const std::string &config) { m_globalHeader = config; }
        std::string getGlobalHeader() const { return m_globalHeader; }
        bool hasGlobalHeader() const { return !m_globalHeader.empty(); }

        uint8_t *data() const
        {
            if (m_packet == nullptr)
            {
                return nullptr;
            }

            return m_packet->data;
        }

        int size() const
        {
            if (m_packet == nullptr)
            {
                return 0;
            }

            return m_packet->size + m_sei.size();
        }

        std::string print() const
        {
            std::ostringstream os;
            os << MediaBase::print() 
               << ", decode config: " << bin2str(m_globalHeader, " ")
               << ", packet={";

            if (data() == nullptr)
            {
                os << "nullptr";
            }
            else
            {
                int sizeLimit = isVideo() ? 64 : 16;
                sizeLimit = size() > sizeLimit ? sizeLimit : size();

                os << ", size: " << size() << ", bin: " << bin2str(data(), sizeLimit, " ");
            }
            os << "}";

            if (!m_globalHeader.empty())
            {
                int sizeLimit = isVideo() ? 64 : 16;
                sizeLimit = size() > sizeLimit ? sizeLimit : size();

                os << ", size: " << size() << ", header: "
                   << bin2str(m_globalHeader.c_str(), sizeLimit, " ");
            }

            os << "}";

            return os.str();
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
        static std::atomic<uint64_t> m_avpacketRefCount;

    private:
        void copy(const MediaPacket &rhs);
        void destroy();

    private:
        AVPacket *m_packet;
        std::string m_globalHeader;
        std::atomic<int> *m_ref;

        std::string m_forward;

    public:
        std::string m_sei;
    };

} // namespace hercules
