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
#include <inttypes.h>
#include <string.h>
#include <string>

namespace hercules
{

    union int2double
    {
        uint64_t i;
        double d;
    };

    inline unsigned int showU32(unsigned char *pBuf)
    {
        return (pBuf[0] << 24) | (pBuf[1] << 16) | (pBuf[2] << 8) | pBuf[3];
    }

    inline uint64_t showU64(unsigned char *pBuf)
    {
        return (((uint64_t)showU32(pBuf)) << 32) | showU32(pBuf + 4);
    }

    inline double showDouble(unsigned char *pBuf)
    {
        uint64_t num = showU64(pBuf);
        union int2double v;
        v.i = num;
        return v.d;
    }

    inline unsigned int showU24(unsigned char *pBuf)
    {
        return (pBuf[0] << 16) | (pBuf[1] << 8) | (pBuf[2]);
    }

    inline unsigned int showU16(unsigned char *pBuf)
    {
        return (pBuf[0] << 8) | (pBuf[1]);
    }

    inline int32_t show16(unsigned char *pBuf)
    {
        return (pBuf[0] << 8 | pBuf[1]);
    }

    inline unsigned int showU8(unsigned char *pBuf)
    {
        return (pBuf[0]);
    }

    inline void writeU64(uint64_t *x, int length, int value)
    {
        uint64_t mask = (uint64_t)0xFFFFFFFFFFFFFFFF >> (64 - length);
        *x = (*x << length) | ((uint64_t)value & mask);
    }

    inline unsigned int writeU32(unsigned int n)
    {
        unsigned int nn = 0;
        unsigned char *p = (unsigned char *)&n;
        unsigned char *pp = (unsigned char *)&nn;
        pp[0] = p[3];
        pp[1] = p[2];
        pp[2] = p[1];
        pp[3] = p[0];
        return nn;
    }
    struct BinaryData
    {
        unsigned char *data_;
        uint32_t size_;
    };

    inline bool startWith(char *buf1, char *buf2)
    {
        if (buf1 == nullptr || buf2 == nullptr)
        {
            return false;
        }
        int len = strlen(buf2);
        if (len == 0)
        {
            return false;
        }
        if (strlen(buf1) < strlen(buf2))
        {
            return false;
        }
        for (int i = 0; i < len; ++i)
        {
            if (buf1[i] != buf2[i])
            {
                return false;
            }
        }
        return true;
    }

    inline std::string intToStr(uint32_t num)
    {
        std::string str;
        str.append(1, reinterpret_cast<char *>(&num)[0]);
        str.append(1, reinterpret_cast<char *>(&num)[1]);
        str.append(1, reinterpret_cast<char *>(&num)[2]);
        str.append(1, reinterpret_cast<char *>(&num)[3]);
        return str;
    }

} // namespace hercules
