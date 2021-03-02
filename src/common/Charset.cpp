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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>
#include <iostream>

namespace hercules
{
    int toWchar(const char *src, wchar_t *&dest, const char *locale = "en_US.utf8")
    {
        if (src == nullptr)
        {
          return 0;
        }
        setlocale(LC_CTYPE, locale);
        unsigned int w_size = mbstowcs(nullptr, src, 0) + 1;
        if (w_size == 0)
        {
          dest = nullptr;
          return -1;
        }
        
        dest = new wchar_t[w_size];
        if (!dest)
        {
          return -1;
        }

        size_t ret = mbstowcs(dest, src, strlen(src) + 1);
        if (ret <= 0)
        {
          return -1;
        }

        return 0;
    }

} // namespace hercules
