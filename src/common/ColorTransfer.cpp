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

#include "ColorTransfer.h"
#include <stdexcept>

namespace
{
    // The sRGB reference viewing environment has a maximum luminance level of 80 nits
    // See the 'Screen luminance level' value in the sRGB reference viewing environment table
    // https://en.wikipedia.org/wiki/SRGB#Viewing_environment
    constexpr float srgbMaxLuminanceLevel = 80.0f;
    // PQ (SMPTE ST 2084) has a maximum luminance level of 10000 nits
    // https://en.wikipedia.org/wiki/Perceptual_quantizer
    constexpr float pqMaxLuminanceLevel = 10000;

    inline float LinearToPQ(float normalizedLinearValue)
    {
        // These constant values are taken from the Perceptual quantizer article on Wikipedia
        // https://en.wikipedia.org/wiki/Perceptual_quantizer
        constexpr float m1 = 2610.0f / 16384.0f;
        constexpr float m2 = 2523.0f / 4096.0f * 128.0f;
        constexpr float c1 = 3424.0f / 4096.0f; // c3 - c2 + 1
        constexpr float c2 = 2413.0f / 4096.0f * 32.0f;
        constexpr float c3 = 2392.0f / 4096.0f * 32.0f;

        if (normalizedLinearValue < 0.0f)
        {
            return 0.0f;
        }

        const float x = powf(normalizedLinearValue, m1);
        const float pq = powf((c1 + c2 * x) / (1.0f + c3 * x), m2);

        return pq;
    }

    inline float PQToLinear(float value)
    {
        // These constant values are taken from the Perceptual quantizer article on Wikipedia
        // https://en.wikipedia.org/wiki/Perceptual_quantizer
        constexpr float m1 = 2610.0f / 16384.0f;
        constexpr float m2 = 2523.0f / 4096.0f * 128.0f;
        constexpr float c1 = 3424.0f / 4096.0f; // c3 - c2 + 1
        constexpr float c2 = 2413.0f / 4096.0f * 32.0f;
        constexpr float c3 = 2392.0f / 4096.0f * 32.0f;

        if (value < 0.0f)
        {
            return 0.0f;
        }

        const float x = powf(value, 1.0f / m2);
        const float normalizedLinear = powf(std::max(x - c1, 0.0f) / (c2 - c3 * x), 1.0f / m1);

        // We have to adjust for the difference in the maximum luminance level between
        // PQ and sRGB, otherwise the image is too dark.
        return normalizedLinear * (pqMaxLuminanceLevel / srgbMaxLuminanceLevel);
    }
}

float TransferFunctionToLinear(float value, ColorTransferFunction transferFunction)
{
    switch (transferFunction)
    {
    case ColorTransferFunction::PQ:
        return PQToLinear(value);
    default:
        throw std::runtime_error("Unsupported color transfer function.");
    }
}

float LinearToTransferFunction(float value, ColorTransferFunction transferFunction)
{
    switch (transferFunction)
    {
    case ColorTransferFunction::PQ:
        return LinearToPQ(value);
    default:
        throw std::runtime_error("Unsupported color transfer function.");
    }
}
