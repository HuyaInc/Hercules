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

#include "ByteUtil.h"
#include <string>

namespace hercules
{

    enum AMFDataType
    {
        AMF_DATA_TYPE_NUMBER        = 0x00,
        AMF_DATA_TYPE_BOOL          = 0x01,
        AMF_DATA_TYPE_STRING        = 0x02,
        AMF_DATA_TYPE_OBJECT        = 0x03,
        AMF_DATA_TYPE_NULL          = 0x05,
        AMF_DATA_TYPE_UNDEFINED     = 0x06,
        AMF_DATA_TYPE_REFERENCE     = 0x07,
        AMF_DATA_TYPE_MIXEDARRAY    = 0x08,
        AMF_DATA_TYPE_OBJECT_END    = 0x09,
        AMF_DATA_TYPE_ARRAY         = 0x0a,
        AMF_DATA_TYPE_DATE          = 0x0b,
        AMF_DATA_TYPE_LONG_STRING   = 0x0c,
        AMF_DATA_TYPE_UNSUPPORTED   = 0x0d,
    };

    inline std::string amfGetString(unsigned char *buf, uint32_t size)
    {
        int length = showU16(buf);
        if (length > size)
        {
            return "";
        }
        std::string str(reinterpret_cast<char *>(buf + 2), length);
        return str;
    }
} // namespace hercules
