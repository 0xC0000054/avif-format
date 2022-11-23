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

#include "YUVDecode.h"
#include "ColorTransfer.h"
#include "PremultipliedAlpha.h"
#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>

void DecodeY8RowToGray8(
    const uint8_t* yPlane,
    uint8_t* grayRow,
    int32 rowWidth,
    const YUVLookupTables& tables)
{
    uint8_t* dstPtr = grayRow;

    constexpr float rgbMaxChannel = 255.0f;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack Y into unorm
        const uint8_t unormY = yPlane[x];

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];

        dstPtr[0] = static_cast<uint8_t>(0.5f + (Y * rgbMaxChannel));

        dstPtr++;
    }
}

void DecodeY8RowToGrayAlpha8(
    const uint8_t* yPlane,
    const uint8_t* alphaPlane,
    bool alphaPremultiplied,
    uint8_t* grayaRow,
    int32 rowWidth,
    const YUVLookupTables& tables)
{
    uint8_t* dstPtr = grayaRow;

    constexpr float rgbMaxChannel = 255.0f;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack Y into unorm
        const uint8_t unormY = yPlane[x];
        const uint8_t unormA = alphaPlane[x];

        // Convert unorm to float
        float Y = tables.unormFloatTableY[unormY];

        if (alphaPremultiplied)
        {
            if (unormA < tables.yuvMaxChannel)
            {
                if (unormA == 0)
                {
                    Y = 0;
                }
                else
                {
                    const float A = tables.unormFloatTableAlpha[unormA];

                    Y = UnpremultiplyColor(Y, A, 1.0f);
                }
            }
        }

        dstPtr[0] = static_cast<uint8_t>(0.5f + (Y * rgbMaxChannel));
        dstPtr[1] = unormA;

        dstPtr += 2;
    }
}

void DecodeY16RowToGray16(
    const uint16_t* yPlane,
    uint16_t* grayRow,
    int32 rowWidth,
    const YUVLookupTables& tables)
{
    uint16_t* dstPtr = grayRow;

    const uint16_t yuvMaxChannel = static_cast<uint16_t>(tables.yuvMaxChannel);
    constexpr float rgbMaxChannel = 32768.0f;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack Y into unorm
        const uint16_t unormY = std::min(yPlane[x], yuvMaxChannel);

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];

        dstPtr[0] = static_cast<uint16_t>(0.5f + (Y * rgbMaxChannel));

        dstPtr++;
    }
}

void DecodeY16RowToGrayAlpha16(
    const uint16_t* yPlane,
    const uint16_t* alphaPlane,
    bool alphaPremultiplied,
    uint16_t* grayaRow,
    int32 rowWidth,
    const YUVLookupTables& tables)
{
    uint16_t* dstPtr = grayaRow;

    const uint16_t yuvMaxChannel = static_cast<uint16_t>(tables.yuvMaxChannel);
    constexpr float rgbMaxChannel = 32768.0f;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack Y into unorm
        const uint16_t unormY = std::min(yPlane[x], yuvMaxChannel);
        const uint16_t unormA = std::min(alphaPlane[x], yuvMaxChannel);

        // Convert unorm to float
        float Y = tables.unormFloatTableY[unormY];
        const float A = tables.unormFloatTableAlpha[unormA];

        if (alphaPremultiplied)
        {
            if (unormA < tables.yuvMaxChannel)
            {
                if (unormA == 0)
                {
                    Y = 0;
                }
                else
                {
                    Y = UnpremultiplyColor(Y, A, 1.0f);
                }
            }
        }

        dstPtr[0] = static_cast<uint16_t>(0.5f + (Y * rgbMaxChannel));
        dstPtr[1] = static_cast<uint16_t>(0.5f + (A * rgbMaxChannel));

        dstPtr += 2;
    }
}

void DecodeY16RowToGray32(
    const uint16_t* yPlane,
    float* grayRow,
    int32 rowWidth,
    const YUVLookupTables& tables,
    ColorTransferFunction transferFunction)
{
    float* dstPtr = grayRow;

    const uint16_t yuvMaxChannel = static_cast<uint16_t>(tables.yuvMaxChannel);

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack Y into unorm
        const uint16_t unormY = std::min(yPlane[x], yuvMaxChannel);

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];

        dstPtr[0] = TransferFunctionToLinear(Y, transferFunction);

        dstPtr++;
    }
}

void DecodeY16RowToGrayAlpha32(
    const uint16_t* yPlane,
    const uint16_t* alphaPlane,
    bool alphaPremultiplied,
    float* grayaRow,
    int32 rowWidth,
    const YUVLookupTables& tables,
    ColorTransferFunction transferFunction)
{
    float* dstPtr = grayaRow;

    const uint16_t yuvMaxChannel = static_cast<uint16_t>(tables.yuvMaxChannel);

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack Y into unorm
        uint16_t unormY = std::min(yPlane[x], yuvMaxChannel);
        const uint16_t unormA = std::min(alphaPlane[x], yuvMaxChannel);

        if (alphaPremultiplied)
        {
            if (unormA < tables.yuvMaxChannel)
            {
                if (unormA == 0)
                {
                    unormY = 0;
                }
                else
                {
                    unormY = UnpremultiplyColor(unormY, unormA, yuvMaxChannel);
                }
            }
        }

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];
        const float A = tables.unormFloatTableAlpha[unormA];

        dstPtr[0] = TransferFunctionToLinear(Y, transferFunction);
        dstPtr[1] = A;

        dstPtr += 2;
    }
}

void DecodeYUV8RowToRGB8(
    const uint8_t* yPlane,
    const uint8_t* uPlane,
    const uint8_t* vPlane,
    uint8_t* rgbRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables)
{
    const float kr = yuvCoefficiants.kr;
    const float kg = yuvCoefficiants.kg;
    const float kb = yuvCoefficiants.kb;

    constexpr float rgbMaxChannel = 255.0f;

    uint8_t* dstPtr = rgbRow;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack YUV into unorm
        const int32_t uvI = x >> xChromaShift;
        const uint8_t unormY = yPlane[x];
        const uint8_t unormU = uPlane[uvI];
        const uint8_t unormV = vPlane[uvI];

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];
        const float Cb = tables.unormFloatTableUV[unormU];
        const float Cr = tables.unormFloatTableUV[unormV];

        float R = Y + (2 * (1 - kr)) * Cr;
        float B = Y + (2 * (1 - kb)) * Cb;
        float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);

        R = std::clamp(R, 0.0f, 1.0f);
        G = std::clamp(G, 0.0f, 1.0f);
        B = std::clamp(B, 0.0f, 1.0f);

        dstPtr[0] = static_cast<uint8_t>(0.5f + (R * rgbMaxChannel));
        dstPtr[1] = static_cast<uint8_t>(0.5f + (G * rgbMaxChannel));
        dstPtr[2] = static_cast<uint8_t>(0.5f + (B * rgbMaxChannel));

        dstPtr += 3;
    }
}

void DecodeYUV8RowToRGBA8(
    const uint8_t* yPlane,
    const uint8_t* uPlane,
    const uint8_t* vPlane,
    const uint8_t* alphaPlane,
    bool alphaPremultiplied,
    uint8_t* rgbRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables)
{
    const float kr = yuvCoefficiants.kr;
    const float kg = yuvCoefficiants.kg;
    const float kb = yuvCoefficiants.kb;

    constexpr uint8_t yuvMaxChannel = 255;
    constexpr float rgbMaxChannel = 255.0f;

    uint8_t* dstPtr = rgbRow;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack YUV into unorm
        const int32_t uvI = x >> xChromaShift;
        const uint8_t unormY = yPlane[x];
        const uint8_t unormU = uPlane[uvI];
        const uint8_t unormV = vPlane[uvI];
        const uint8_t unormA = alphaPlane[x];

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];
        const float Cb = tables.unormFloatTableUV[unormU];
        const float Cr = tables.unormFloatTableUV[unormV];

        float R = Y + (2 * (1 - kr)) * Cr;
        float B = Y + (2 * (1 - kb)) * Cb;
        float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);

        R = std::clamp(R, 0.0f, 1.0f);
        G = std::clamp(G, 0.0f, 1.0f);
        B = std::clamp(B, 0.0f, 1.0f);

        if (alphaPremultiplied)
        {
            if (unormA < yuvMaxChannel)
            {
                if (unormA == 0)
                {
                    R = 0;
                    G = 0;
                    B = 0;
                }
                else
                {
                    const float A = tables.unormFloatTableAlpha[unormA];

                    R = UnpremultiplyColor(R, A, 1.0f);
                    G = UnpremultiplyColor(G, A, 1.0f);
                    B = UnpremultiplyColor(B, A, 1.0f);
                }
            }
        }

        dstPtr[0] = static_cast<uint8_t>(0.5f + (R * rgbMaxChannel));
        dstPtr[1] = static_cast<uint8_t>(0.5f + (G * rgbMaxChannel));
        dstPtr[2] = static_cast<uint8_t>(0.5f + (B * rgbMaxChannel));
        dstPtr[3] = unormA;

        dstPtr += 4;
    }
}

void DecodeYUV16RowToRGB16(
    const uint16_t* yPlane,
    const uint16_t* uPlane,
    const uint16_t* vPlane,
    uint16_t* rgbRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables)
{
    const float kr = yuvCoefficiants.kr;
    const float kg = yuvCoefficiants.kg;
    const float kb = yuvCoefficiants.kb;

    const uint16_t yuvMaxChannel = static_cast<uint16_t>(tables.yuvMaxChannel);
    constexpr float rgbMaxChannel = 32768.0f;

    uint16_t* dstPtr = rgbRow;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack YUV into unorm
        const int32_t uvI = x >> xChromaShift;
        const uint16_t unormY = std::min(yPlane[x], yuvMaxChannel);
        const uint16_t unormU = std::min(uPlane[uvI], yuvMaxChannel);
        const uint16_t unormV = std::min(vPlane[uvI], yuvMaxChannel);

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];
        const float Cb = tables.unormFloatTableUV[unormU];
        const float Cr = tables.unormFloatTableUV[unormV];

        float R = Y + (2 * (1 - kr)) * Cr;
        float B = Y + (2 * (1 - kb)) * Cb;
        float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);

        R = std::clamp(R, 0.0f, 1.0f);
        G = std::clamp(G, 0.0f, 1.0f);
        B = std::clamp(B, 0.0f, 1.0f);

        dstPtr[0] = static_cast<uint16_t>(0.5f + (R * rgbMaxChannel));
        dstPtr[1] = static_cast<uint16_t>(0.5f + (G * rgbMaxChannel));
        dstPtr[2] = static_cast<uint16_t>(0.5f + (B * rgbMaxChannel));

        dstPtr += 3;
    }
}

void DecodeYUV16RowToRGBA16(
    const uint16_t* yPlane,
    const uint16_t* uPlane,
    const uint16_t* vPlane,
    const uint16_t* alphaPlane,
    bool alphaPremultiplied,
    uint16_t* rgbaRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables)
{
    const float kr = yuvCoefficiants.kr;
    const float kg = yuvCoefficiants.kg;
    const float kb = yuvCoefficiants.kb;

    const uint16_t yuvMaxChannel = static_cast<uint16_t>(tables.yuvMaxChannel);
    constexpr float rgbMaxChannel = 32768.0f;

    uint16_t* dstPtr = rgbaRow;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack YUV into unorm
        const int32_t uvI = x >> xChromaShift;
        const uint16_t unormY = std::min(yPlane[x], yuvMaxChannel);
        const uint16_t unormU = std::min(uPlane[uvI], yuvMaxChannel);
        const uint16_t unormV = std::min(vPlane[uvI], yuvMaxChannel);
        const uint16_t unormA = std::min(alphaPlane[x], yuvMaxChannel);

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];
        const float Cb = tables.unormFloatTableUV[unormU];
        const float Cr = tables.unormFloatTableUV[unormV];

        float R = Y + (2 * (1 - kr)) * Cr;
        float B = Y + (2 * (1 - kb)) * Cb;
        float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);

        R = std::clamp(R, 0.0f, 1.0f);
        G = std::clamp(G, 0.0f, 1.0f);
        B = std::clamp(B, 0.0f, 1.0f);
        const float A = tables.unormFloatTableAlpha[unormA];

        if (alphaPremultiplied)
        {
            if (unormA < tables.yuvMaxChannel)
            {
                if (unormA == 0)
                {
                    R = 0;
                    G = 0;
                    B = 0;
                }
                else
                {
                    R = UnpremultiplyColor(R, A, 1.0f);
                    G = UnpremultiplyColor(G, A, 1.0f);
                    B = UnpremultiplyColor(B, A, 1.0f);
                }
            }
        }

        dstPtr[0] = static_cast<uint16_t>(0.5f + (R * rgbMaxChannel));
        dstPtr[1] = static_cast<uint16_t>(0.5f + (G * rgbMaxChannel));
        dstPtr[2] = static_cast<uint16_t>(0.5f + (B * rgbMaxChannel));
        dstPtr[3] = static_cast<uint16_t>(0.5f + (A * rgbMaxChannel));

        dstPtr += 4;
    }
}

void DecodeYUV16RowToRGB32(
    const uint16_t* yPlane,
    const uint16_t* uPlane,
    const uint16_t* vPlane,
    float* rgbRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables,
    ColorTransferFunction transferFunction,
    const LoadUIOptions& loadOptions,
    const HLGLumaCoefficiants& hlgLumaCoefficiants)
{
    const float kr = yuvCoefficiants.kr;
    const float kg = yuvCoefficiants.kg;
    const float kb = yuvCoefficiants.kb;

    const uint16_t yuvMaxChannel = static_cast<uint16_t>(tables.yuvMaxChannel);

    float* dstPtr = rgbRow;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack YUV into unorm
        const int32_t uvI = x >> xChromaShift;
        const uint16_t unormY = std::min(yPlane[x], yuvMaxChannel);
        const uint16_t unormU = std::min(uPlane[uvI], yuvMaxChannel);
        const uint16_t unormV = std::min(vPlane[uvI], yuvMaxChannel);

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];
        const float Cb = tables.unormFloatTableUV[unormU];
        const float Cr = tables.unormFloatTableUV[unormV];

        float R = Y + (2 * (1 - kr)) * Cr;
        float B = Y + (2 * (1 - kb)) * Cb;
        float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);

        R = std::clamp(R, 0.0f, 1.0f);
        G = std::clamp(G, 0.0f, 1.0f);
        B = std::clamp(B, 0.0f, 1.0f);

        dstPtr[0] = TransferFunctionToLinear(R, transferFunction);
        dstPtr[1] = TransferFunctionToLinear(G, transferFunction);
        dstPtr[2] = TransferFunctionToLinear(B, transferFunction);

        if (transferFunction == ColorTransferFunction::HLG && loadOptions.applyHLGOOTF)
        {
            ApplyHLGOOTF(dstPtr, hlgLumaCoefficiants, loadOptions.displayGamma, loadOptions.nominalPeakBrightness);
        }

        dstPtr += 3;
    }
}

void DecodeYUV16RowToRGBA32(
    const uint16_t* yPlane,
    const uint16_t* uPlane,
    const uint16_t* vPlane,
    const uint16_t* alphaPlane,
    bool alphaPremultiplied,
    float* rgbaRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables,
    ColorTransferFunction transferFunction,
    const LoadUIOptions& loadOptions,
    const HLGLumaCoefficiants& hlgLumaCoefficiants)
{
    const float kr = yuvCoefficiants.kr;
    const float kg = yuvCoefficiants.kg;
    const float kb = yuvCoefficiants.kb;

    const uint16_t yuvMaxChannel = static_cast<uint16_t>(tables.yuvMaxChannel);

    float* dstPtr = rgbaRow;

    for (int32 x = 0; x < rowWidth; ++x)
    {
        // Unpack YUV into unorm
        const int32_t uvI = x >> xChromaShift;
        const uint16_t unormY = std::min(yPlane[x], yuvMaxChannel);
        const uint16_t unormU = std::min(uPlane[uvI], yuvMaxChannel);
        const uint16_t unormV = std::min(vPlane[uvI], yuvMaxChannel);
        const uint16_t unormA = std::min(alphaPlane[x], yuvMaxChannel);

        // Convert unorm to float
        const float Y = tables.unormFloatTableY[unormY];
        const float Cb = tables.unormFloatTableUV[unormU];
        const float Cr = tables.unormFloatTableUV[unormV];

        float R = Y + (2 * (1 - kr)) * Cr;
        float B = Y + (2 * (1 - kb)) * Cb;
        float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);

        R = std::clamp(R, 0.0f, 1.0f);
        G = std::clamp(G, 0.0f, 1.0f);
        B = std::clamp(B, 0.0f, 1.0f);
        const float A = tables.unormFloatTableAlpha[unormA];

        if (alphaPremultiplied)
        {
            if (unormA < tables.yuvMaxChannel)
            {
                if (unormA == 0)
                {
                    R = 0;
                    G = 0;
                    B = 0;
                }
                else
                {
                    R = UnpremultiplyColor(R, A, 1.0f);
                    G = UnpremultiplyColor(G, A, 1.0f);
                    B = UnpremultiplyColor(B, A, 1.0f);
                }
            }
        }

        dstPtr[0] = TransferFunctionToLinear(R, transferFunction);
        dstPtr[1] = TransferFunctionToLinear(G, transferFunction);
        dstPtr[2] = TransferFunctionToLinear(B, transferFunction);
        dstPtr[3] = A;

        if (transferFunction == ColorTransferFunction::HLG && loadOptions.applyHLGOOTF)
        {
            ApplyHLGOOTF(dstPtr, hlgLumaCoefficiants, loadOptions.displayGamma, loadOptions.nominalPeakBrightness);
        }

        dstPtr += 4;
    }
}
