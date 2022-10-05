/*
 * This file is part of avif-format, an AV1 Image (AVIF) file format
 * plug-in for Adobe Photoshop(R).
 *
 * Copyright (c) 2021, 2022 Nicholas Hayes
 *
 * avif-format is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * avif-format is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with avif-format.  If not, see <http://www.gnu.org/licenses/>.
 */
 // Portions of this file has been adapted from libavif, https://github.com/AOMediaCodec/libavif
 /*
     Copyright 2020 Joe Drago. All rights reserved.

     Redistribution and use in source and binary forms, with or without
     modification, are permitted provided that the following conditions are met:

     1. Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
     AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
     FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
     SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
     CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
     OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
     OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PremultipliedAlpha.h"
#include <algorithm>

void PremultiplyAlpha(uint8_t* data, int width, int height, int stride, int bitDepth)
{
    constexpr int channelCount = 4;
    const int rowLength = width * channelCount;

    if (bitDepth > 8)
    {
        const int maxValue = (1 << bitDepth) - 1;
        const float maxValueF = static_cast<float>(maxValue);

        for (int y = 0; y < height; y++)
        {
            uint16_t* row = reinterpret_cast<uint16_t*>(data + (static_cast<int64>(y) * stride));

            for (int x = 0; x < rowLength; x += channelCount)
            {
                uint16_t alpha = row[x + 3];

                if (alpha == maxValue)
                {
                    continue;
                }
                else
                {
                    switch (alpha)
                    {
                    case 0:
                        row[x] = 0;
                        row[x + 1] = 0;
                        row[x + 2] = 0;
                        break;
                    default:
                        const float alphaF = static_cast<float>(alpha);

                        row[x] = static_cast<uint16_t>(PremultiplyColor(static_cast<float>(row[x]), alphaF, maxValueF));
                        row[x + 1] = static_cast<uint16_t>(PremultiplyColor(static_cast<float>(row[x + 1]), alphaF, maxValueF));
                        row[x + 2] = static_cast<uint16_t>(PremultiplyColor(static_cast<float>(row[x + 2]), alphaF, maxValueF));
                        break;
                    }
                }
            }
        }
    }
    else
    {
        constexpr int maxValue = 255;
        constexpr float maxValueF = 255.0f;

        for (int y = 0; y < height; y++)
        {
            uint8_t* row = data + (static_cast<int64>(y) * stride);

            for (int x = 0; x < rowLength; x += channelCount)
            {
                uint8_t alpha = row[x + 3];

                if (alpha == maxValue)
                {
                    continue;
                }
                else
                {
                    switch (alpha)
                    {
                    case 0:
                        row[x] = 0;
                        row[x + 1] = 0;
                        row[x + 2] = 0;
                        break;
                    default:
                        const float alphaF = static_cast<float>(alpha);

                        row[x] = static_cast<uint8_t>(PremultiplyColor(static_cast<float>(row[x]), alphaF, maxValueF));
                        row[x + 1] = static_cast<uint8_t>(PremultiplyColor(static_cast<float>(row[x + 1]), alphaF, maxValueF));
                        row[x + 2] = static_cast<uint8_t>(PremultiplyColor(static_cast<float>(row[x + 2]), alphaF, maxValueF));
                        break;
                    }
                }
            }
        }
    }
}

float PremultiplyColor(float color, float alpha, float maxValue)
{
    return roundf(color * alpha / maxValue);
}

float UnpremultiplyColor(float color, float alpha, float maxValue)
{
    return std::min(color * maxValue / alpha, maxValue);
}

uint8_t UnpremultiplyColor(uint8_t color, uint8_t alpha)
{
    constexpr float maxValueFloat = 255.0f;

    const float value = UnpremultiplyColor(static_cast<float>(color), static_cast<float>(alpha), maxValueFloat);

    return static_cast<uint8_t>(std::min(roundf(value), maxValueFloat));
}

uint16_t UnpremultiplyColor(uint16_t color, uint16_t alpha, uint16_t maxValue)
{
    const float maxValueFloat = static_cast<float>(maxValue);

    const float value = UnpremultiplyColor(static_cast<float>(color), static_cast<float>(alpha), maxValueFloat);

    return static_cast<uint16_t>(std::min(roundf(value), maxValueFloat));
}
