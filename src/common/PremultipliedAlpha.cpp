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

float PremultiplyColor(float color, float alpha, float maxValue)
{
    return color * alpha / maxValue;
}

uint8_t PremultiplyColor(uint8_t color, uint8_t alpha)
{
    constexpr float maxValueFloat = 255.0f;

    const float value = PremultiplyColor(static_cast<float>(color), static_cast<float>(alpha), maxValueFloat);

    return static_cast<uint8_t>(std::min(roundf(value), maxValueFloat));
}

uint16_t PremultiplyColor(uint16_t color, uint16_t alpha, uint16_t maxValue)
{
    const float maxValueFloat = static_cast<float>(maxValue);

    const float value = PremultiplyColor(static_cast<float>(color), static_cast<float>(alpha), maxValueFloat);

    return static_cast<uint16_t>(std::min(roundf(value), maxValueFloat));
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
