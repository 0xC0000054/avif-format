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
 //
 // Portions of this file has been adapted from libavif, https://github.com/AOMediaCodec/libavif
 /*
     Copyright 2019 Joe Drago. All rights reserved.

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

#include "YUVLookupTables.h"
#include <stdexcept>

namespace
{
#define AVIF_CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

    // Limited -> Full
    // Plan: subtract limited offset, then multiply by ratio of FULLSIZE/LIMITEDSIZE (rounding), then clamp.
    // RATIO = (FULLY - 0) / (MAXLIMITEDY - MINLIMITEDY)
    // -----------------------------------------
    // ( ( (v - MINLIMITEDY)                    | subtract limited offset
    //     * FULLY                              | multiply numerator of ratio
    //   ) + ((MAXLIMITEDY - MINLIMITEDY) / 2)  | add 0.5 (half of denominator) to round
    // ) / (MAXLIMITEDY - MINLIMITEDY)          | divide by denominator of ratio
    // AVIF_CLAMP(v, 0, FULLY)                  | clamp to full range
    // -----------------------------------------
#define LIMITED_TO_FULL(MINLIMITEDY, MAXLIMITEDY, FULLY)                                                 \
        v = (((v - MINLIMITEDY) * FULLY) + ((MAXLIMITEDY - MINLIMITEDY) / 2)) / (MAXLIMITEDY - MINLIMITEDY); \
        v = AVIF_CLAMP(v, 0, FULLY)


    constexpr int avifLimitedToFullY(int depth, int v)
    {
        switch (depth) {
        case 8:
            LIMITED_TO_FULL(16, 235, 255);
            break;
        case 10:
            LIMITED_TO_FULL(64, 940, 1023);
            break;
        case 12:
            LIMITED_TO_FULL(256, 3760, 4095);
            break;
        case 16:
            LIMITED_TO_FULL(1024, 60160, 65535);
            break;
        default:
            throw std::runtime_error("The image has an unsupported bit depth, must be 8, 10, 12 or 16.");
        }
        return v;
    }

    int avifLimitedToFullUV(int depth, int v)
    {
        switch (depth) {
        case 8:
            LIMITED_TO_FULL(16, 240, 255);
            break;
        case 10:
            LIMITED_TO_FULL(64, 960, 1023);
            break;
        case 12:
            LIMITED_TO_FULL(256, 3840, 4095);
            break;
        case 16:
            LIMITED_TO_FULL(1024, 61440, 65535);
            break;
        default:
            throw std::runtime_error("The image has an unsupported bit depth, must be 8, 10, 12 or 16.");
        }
        return v;
    }

#undef AVIF_CLAMP
#undef LIMITED_TO_FULL
}

YUVLookupTables::YUVLookupTables(const heif_color_profile_nclx* nclx, int32_t bitDepth, bool monochrome, bool hasAlpha)
    : yuvMaxChannel((1 << bitDepth) - 1)
{
    if (bitDepth != 8 &&
        bitDepth != 10 &&
        bitDepth != 12 &&
        bitDepth != 16)
    {
        throw std::runtime_error("The image has an unsupported bit depth, must be 8, 10, 12 or 16.");
    }

    // (As of ISO/IEC 23000-22:2019 Amendment 2)
    // MIAF Section 7.3.6.4 "Colour information property":
    //
    // If a coded image has no associated colour property, the default property is defined as having
    // colour_type equal to 'nclx' with properties as follows:
    // –   colour_primaries equal to 1,
    // –   transfer_characteristics equal to 13,
    // –   matrix_coefficients equal to 5 or 6 (which are functionally identical), and
    // –   full_range_flag equal to 1.
    // Only if the colour information property of the image matches these default values, the colour
    // property may be omitted; all other images shall have an explicitly declared colour space via
    // association with a property of this type.
    //
    // See here for the discussion: https://github.com/AOMediaCodec/av1-avif/issues/77#issuecomment-676526097
    const bool fullRange = nclx ? nclx->full_range_flag != 0 : true;
    const heif_matrix_coefficients matrixCoefficient = nclx ? nclx->matrix_coefficients : heif_matrix_coefficients_ITU_R_BT_601_6;

    const int count = 1 << bitDepth;
    const bool isColorImage = !monochrome;
    const bool isIdentityMatrix = isColorImage && matrixCoefficient == heif_matrix_coefficients_RGB_GBR;
    const float yuvMaxChannelFloat = static_cast<float>(yuvMaxChannel);

    unormFloatTableY = std::make_unique<float[]>(count);
    if (isColorImage)
    {
        unormFloatTableUV = std::make_unique<float[]>(count);
    }

    if (hasAlpha)
    {
        unormFloatTableAlpha = std::make_unique<float[]>(count);
    }

    for (int i = 0; i < count; ++i)
    {
        int unormY = i;
        int unormUV = i;

        if (!fullRange)
        {
            unormY = avifLimitedToFullY(bitDepth, unormY);
            if (isColorImage)
            {
                unormUV = avifLimitedToFullUV(bitDepth, unormUV);
            }
        }

        unormFloatTableY[i] = static_cast<float>(unormY) / yuvMaxChannelFloat;

        if (isColorImage)
        {
            if (isIdentityMatrix)
            {
                unormFloatTableUV[i] = unormFloatTableY[i];
            }
            else
            {
                unormFloatTableUV[i] = static_cast<float>(unormUV) / yuvMaxChannelFloat - 0.5f;
            }
        }

        if (hasAlpha)
        {
            unormFloatTableAlpha[i] = static_cast<float>(i) / yuvMaxChannelFloat;
        }
    }
}
