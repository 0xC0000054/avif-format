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
    constexpr float pqMaxLuminanceLevel = 10000.0f;

    inline float LinearToPQ(float value)
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

        // We have to adjust for the difference in the maximum luminance level between
        // sRGB and PQ, otherwise the image is too bright.
        const float x = powf(value * (srgbMaxLuminanceLevel / pqMaxLuminanceLevel), m1);
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

    inline float LinearToSMPTE428(float value)
    {
        if (value < 0.0f)
        {
            return 0.0f;
        }

        return powf(value * 48.0f / 52.37f, 1.0f / 2.6f);
    }

    inline float SMPTE428ToLinear(float value)
    {
        if (value < 0.0f)
        {
            return 0.0f;
        }

        // The following code is equivalent to (powf(value, 2.6f) * 52.37f) / 48.0f
        // but it removes the need to perform division at runtime.
        return powf(value, 2.6f) * (52.37f / 48.0f);
    }

    inline float LinearToHLG(float value)
    {
        if (value < 0.0f)
        {
            return 0.0f;
        }

        // These constants are from the ITU-R BT.2100 specification:
        // https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-E.pdf
        constexpr float a = 0.17883277f;
        constexpr float b = 0.28466892f;
        constexpr float c = 0.55991073f;

        if (value > (1.0f / 12.0f))
        {
            value = a * logf(value * 12.0f - b) + c;
        }
        else
        {
            value = sqrtf(value * 3.0f);
        }

        return value;
    }

    inline float HLGToLinear(float value)
    {
        if (value < 0.0f)
        {
            return 0.0f;
        }

        // These constants are from the ITU-R BT.2100 specification:
        // https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-E.pdf
        constexpr float a = 0.17883277f;
        constexpr float b = 0.28466892f;
        constexpr float c = 0.55991073f;

        if (value > 0.5f)
        {
            value = (expf((value - c) / a) + b) / 12.0f;
        }
        else
        {
            // Equivalent to powf(value, 2.0f) / 3.0f
            value = (value * value) * (1.0f / 3.0f);
        }

        return value;
    }
}

HLGLumaCoefficiants GetHLGLumaCoefficients(heif_color_primaries primaries)
{
    switch (primaries)
    {
    case heif_color_primaries_ITU_R_BT_709_5:
        return HLGLumaCoefficiants{ 0.2126f, 0.7152f, 0.0722f };
    case heif_color_primaries_ITU_R_BT_470_6_System_B_G:
    case heif_color_primaries_ITU_R_BT_601_6:
        return HLGLumaCoefficiants{ 0.299f, 0.587f, 0.114f };
    case heif_color_primaries_ITU_R_BT_2020_2_and_2100_0:
        return HLGLumaCoefficiants{ 0.2627f, 0.6780f, 0.0593f };
    default:
        throw std::runtime_error("Unsupported color primaries for the HLG Luma Coefficients ");
    }
}

ColorTransferFunction GetTransferFunctionFromNclx(heif_transfer_characteristics transferCharacteristics)
{
    ColorTransferFunction transferFunction;

    switch (transferCharacteristics)
    {
    case heif_transfer_characteristic_ITU_R_BT_2100_0_PQ:
        transferFunction = ColorTransferFunction::PQ;
        break;
    case heif_transfer_characteristic_ITU_R_BT_2100_0_HLG:
        transferFunction = ColorTransferFunction::HLG;
        break;
    case heif_transfer_characteristic_SMPTE_ST_428_1:
        transferFunction = ColorTransferFunction::SMPTE428;
        break;
    default:
        throw std::runtime_error("Unsupported NCLX transfer characteristic.");
    }

    return transferFunction;
}

float TransferFunctionToLinear(float value, ColorTransferFunction transferFunction)
{
    switch (transferFunction)
    {
    case ColorTransferFunction::PQ:
        return PQToLinear(value);
    case ColorTransferFunction::HLG:
        return HLGToLinear(value);
    case ColorTransferFunction::SMPTE428:
        return SMPTE428ToLinear(value);
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
    case ColorTransferFunction::HLG:
        return LinearToHLG(value);
    case ColorTransferFunction::SMPTE428:
        return LinearToSMPTE428(value);
    case ColorTransferFunction::Clip:
        return value;
    default:
        throw std::runtime_error("Unsupported color transfer function.");
    }
}

void ApplyHLGOOTF(
    float* rgb,
    const HLGLumaCoefficiants& lumaCoefficiants,
    float displayGamma,
    float nominalPeakBrightness)
{
    const float luma = (rgb[0] * lumaCoefficiants.red) + (rgb[1] * lumaCoefficiants.green) + (rgb[2] * lumaCoefficiants.blue);

    const float factor = nominalPeakBrightness * powf(luma, displayGamma - 1.0f);

    rgb[0] *= factor;
    rgb[1] *= factor;
    rgb[2] *= factor;
}

void ApplyInverseHLGOOTF(
    float* rgb,
    const HLGLumaCoefficiants& lumaCoefficiants,
    float displayGamma,
    float nominalPeakBrightness)
{
    const float luma = (rgb[0] * lumaCoefficiants.red) + (rgb[1] * lumaCoefficiants.green) + (rgb[2] * lumaCoefficiants.blue);

    const float factor = powf(luma / nominalPeakBrightness, (displayGamma - 1.0f) / displayGamma) / nominalPeakBrightness;

    rgb[0] *= factor;
    rgb[1] *= factor;
    rgb[2] *= factor;
}
