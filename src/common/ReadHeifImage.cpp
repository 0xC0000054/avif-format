/*
 * This file is part of avif-format, an AV1 Image (AVIF) file format
 * plug-in for Adobe Photoshop(R).
 *
 * Copyright (c) 2021, 2022, 2023 Nicholas Hayes
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

#include "ReadHeifImage.h"
#include "ColorTransfer.h"
#include "PremultipliedAlpha.h"
#include "ScopedBufferSuite.h"
#include "Utilities.h"
#include "YUVDecode.h"
#include <vector>

namespace
{
    void SetupFormatRecord(
        FormatRecordPtr formatRecord,
        const VPoint& imageSize)
    {
        formatRecord->loPlane = 0;
        formatRecord->hiPlane = formatRecord->planes - 1;
        formatRecord->planeBytes = (formatRecord->depth + 7) / 8;
        formatRecord->colBytes = static_cast<int16>(formatRecord->planes * formatRecord->planeBytes);

        uint64_t rowBytes = static_cast<uint64_t>(imageSize.h) * static_cast<uint64_t>(formatRecord->colBytes);

        if (rowBytes > std::numeric_limits<int32>::max())
        {
            // The image stride in bytes exceeds the range of an int32, this is treated
            // as an out of memory error.
            throw std::bad_alloc();
        }

        formatRecord->rowBytes = static_cast<int32>(rowBytes);
    }

    void GetChromaShift(heif_chroma chroma, int32& xChromaShift, int32& yChromaShift)
    {
        switch (chroma)
        {
        case heif_chroma_monochrome:
        case heif_chroma_444:
            xChromaShift = 0;
            yChromaShift = 0;
            break;
        case heif_chroma_422:
            xChromaShift = 1;
            yChromaShift = 0;
            break;
        case heif_chroma_420:
            xChromaShift = 1;
            yChromaShift = 1;
            break;
        case heif_chroma_undefined:
        case heif_chroma_interleaved_RGB:
        case heif_chroma_interleaved_RGBA:
        case heif_chroma_interleaved_RRGGBB_BE:
        case heif_chroma_interleaved_RRGGBBAA_BE:
        case heif_chroma_interleaved_RRGGBB_LE:
        case heif_chroma_interleaved_RRGGBBAA_LE:
        default:
            xChromaShift = 0;
            yChromaShift = 0;
            break;
        }
    }

    void ReadHeifImageYUVEightBit(
        const heif_image* image,
        AlphaState alphaState,
        const heif_color_profile_nclx* nclxProfile,
        FormatRecordPtr formatRecord)
    {
        const heif_chroma chroma = heif_image_get_chroma_format(image);

        constexpr int lumaBitsPerPixel = 8;

        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Cb) != lumaBitsPerPixel ||
            heif_image_get_bits_per_pixel_range(image, heif_channel_Cr) != lumaBitsPerPixel)
        {
            throw std::runtime_error("The chroma channel bit depth does not match the main image.");
        }

        const VPoint imageSize = GetImageSize(formatRecord);
        const bool hasAlpha = alphaState != AlphaState::None;

        SetupFormatRecord(formatRecord, imageSize);

        int yPlaneStride;
        const uint8_t* yPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Y, &yPlaneStride);

        int cbPlaneStride;
        const uint8_t* cbPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Cb, &cbPlaneStride);

        int crPlaneStride;
        const uint8_t* crPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Cr, &crPlaneStride);

        ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

        formatRecord->data = buffer.lock();

        const int32 left = 0;
        const int32 right = imageSize.h;

        YUVCoefficiants yuvCoefficiants{};

        GetYUVCoefficiants(nclxProfile, yuvCoefficiants);

        const YUVLookupTables tables(nclxProfile, lumaBitsPerPixel, false, hasAlpha);

        int32 xChromaShift, yChromaShift;

        GetChromaShift(chroma, xChromaShift, yChromaShift);

        if (hasAlpha)
        {
            if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != lumaBitsPerPixel)
            {
                throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
            }

            int alphaStride;
            const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
            const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

            for (int32 y = 0; y < imageSize.v; y++)
            {
                const int32 uvJ = y >> yChromaShift;
                const uint8_t* srcY = yPlaneScan0 + (static_cast<int64>(y) * yPlaneStride);
                const uint8_t* srcCb = cbPlaneScan0 + (static_cast<int64>(uvJ) * cbPlaneStride);
                const uint8_t* srcCr = crPlaneScan0 + (static_cast<int64>(uvJ) * crPlaneStride);

                const uint8_t* srcAlpha = alphaScan0 + (static_cast<int64>(y) * alphaStride);
                uint8_t* dst = static_cast<uint8_t*>(formatRecord->data);

                DecodeYUV8RowToRGBA8(srcY, srcCb, srcCr, srcAlpha, alphaPremultiplied,
                    dst, imageSize.h, xChromaShift, yuvCoefficiants, tables);

                const int32 top = y;
                const int32 bottom = y + 1;

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());
            }
        }
        else
        {
            for (int32 y = 0; y < imageSize.v; y++)
            {
                const int32 uvJ = y >> yChromaShift;
                const uint8_t* srcY = yPlaneScan0 + (static_cast<int64>(y) * yPlaneStride);
                const uint8_t* srcCb = cbPlaneScan0 + (static_cast<int64>(uvJ) * cbPlaneStride);
                const uint8_t* srcCr = crPlaneScan0 + (static_cast<int64>(uvJ) * crPlaneStride);

                uint8_t* dst = static_cast<uint8_t*>(formatRecord->data);

                DecodeYUV8RowToRGB8(srcY, srcCb, srcCr, dst, imageSize.h, xChromaShift,
                    yuvCoefficiants, tables);

                const int32 top = y;
                const int32 bottom = y + 1;

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());
            }
        }
    }

    void ReadHeifImageYUVSixteenBit(
        const heif_image* image,
        AlphaState alphaState,
        const heif_color_profile_nclx* nclxProfile,
        FormatRecordPtr formatRecord)
    {
        const heif_chroma chroma = heif_image_get_chroma_format(image);

        const int lumaBitsPerPixel = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Cb) != lumaBitsPerPixel ||
            heif_image_get_bits_per_pixel_range(image, heif_channel_Cr) != lumaBitsPerPixel)
        {
            throw std::runtime_error("The chroma channel bit depth does not match the main image.");
        }

        const VPoint imageSize = GetImageSize(formatRecord);
        const bool hasAlpha = alphaState != AlphaState::None;

        SetupFormatRecord(formatRecord, imageSize);
        formatRecord->maxData = 32768;

        int yPlaneStride;
        const uint8_t* yPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Y, &yPlaneStride);

        int cbPlaneStride;
        const uint8_t* cbPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Cb, &cbPlaneStride);

        int crPlaneStride;
        const uint8_t* crPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Cr, &crPlaneStride);

        ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

        formatRecord->data = buffer.lock();

        const int32 left = 0;
        const int32 right = imageSize.h;

        YUVCoefficiants yuvCoefficiants{};

        GetYUVCoefficiants(nclxProfile, yuvCoefficiants);

        const YUVLookupTables tables(nclxProfile, lumaBitsPerPixel, false, hasAlpha);

        int32 xChromaShift, yChromaShift;

        GetChromaShift(chroma, xChromaShift, yChromaShift);

        if (hasAlpha)
        {
            if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != lumaBitsPerPixel)
            {
                throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
            }

            int alphaStride;
            const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
            const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

            for (int32 y = 0; y < imageSize.v; y++)
            {
                const int32 uvJ = y >> yChromaShift;
                const uint16_t* srcY = reinterpret_cast<const uint16_t*>(yPlaneScan0 + (static_cast<int64>(y) * yPlaneStride));
                const uint16_t* srcCb = reinterpret_cast<const uint16_t*>(cbPlaneScan0 + (static_cast<int64>(uvJ) * cbPlaneStride));
                const uint16_t* srcCr = reinterpret_cast<const uint16_t*>(crPlaneScan0 + (static_cast<int64>(uvJ) * crPlaneStride));

                const uint16_t* srcAlpha = reinterpret_cast<const uint16_t*>(alphaScan0 + (static_cast<int64>(y) * alphaStride));
                uint16_t* dst = static_cast<uint16_t*>(formatRecord->data);

                DecodeYUV16RowToRGBA16(srcY, srcCb, srcCr, srcAlpha, alphaPremultiplied,
                    dst, imageSize.h, xChromaShift, yuvCoefficiants, tables);

                const int32 top = y;
                const int32 bottom = y + 1;

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());
            }
        }
        else
        {
            for (int32 y = 0; y < imageSize.v; y++)
            {
                const int32 uvJ = y >> yChromaShift;
                const uint16_t* srcY = reinterpret_cast<const uint16_t*>(yPlaneScan0 + (static_cast<int64>(y) * yPlaneStride));
                const uint16_t* srcCb = reinterpret_cast<const uint16_t*>(cbPlaneScan0 + (static_cast<int64>(uvJ) * cbPlaneStride));
                const uint16_t* srcCr = reinterpret_cast<const uint16_t*>(crPlaneScan0 + (static_cast<int64>(uvJ) * crPlaneStride));

                uint16_t* dst = static_cast<uint16_t*>(formatRecord->data);

                DecodeYUV16RowToRGB16(srcY, srcCb, srcCr, dst, imageSize.h, xChromaShift,
                    yuvCoefficiants, tables);

                const int32 top = y;
                const int32 bottom = y + 1;

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());
            }
        }
    }

    void ReadHeifImageYUVThirtyTwoBit(
        const heif_image* image,
        AlphaState alphaState,
        const heif_color_profile_nclx* nclxProfile,
        FormatRecordPtr formatRecord,
        ColorTransferFunction transferFunction,
        const LoadUIOptions& loadOptions)
    {
        const heif_chroma chroma = heif_image_get_chroma_format(image);

        const int lumaBitsPerPixel = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Cb) != lumaBitsPerPixel ||
            heif_image_get_bits_per_pixel_range(image, heif_channel_Cr) != lumaBitsPerPixel)
        {
            throw std::runtime_error("The chroma channel bit depth does not match the main image.");
        }

        const VPoint imageSize = GetImageSize(formatRecord);
        const bool hasAlpha = alphaState != AlphaState::None;

        SetupFormatRecord(formatRecord, imageSize);

        int yPlaneStride;
        const uint8_t* yPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Y, &yPlaneStride);

        int cbPlaneStride;
        const uint8_t* cbPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Cb, &cbPlaneStride);

        int crPlaneStride;
        const uint8_t* crPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_Cr, &crPlaneStride);

        ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

        formatRecord->data = buffer.lock();

        const int32 left = 0;
        const int32 right = imageSize.h;

        YUVCoefficiants yuvCoefficiants{};

        GetYUVCoefficiants(nclxProfile, yuvCoefficiants);

        const YUVLookupTables tables(nclxProfile, lumaBitsPerPixel, false, hasAlpha);

        int32 xChromaShift, yChromaShift;

        GetChromaShift(chroma, xChromaShift, yChromaShift);

        HLGLumaCoefficiants hlgLumaCoefficiants{};

        if (transferFunction == ColorTransferFunction::HLG && loadOptions.hlg.applyOOTF)
        {
            hlgLumaCoefficiants = GetHLGLumaCoefficients(nclxProfile->color_primaries);
        }

        if (hasAlpha)
        {
            if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != lumaBitsPerPixel)
            {
                throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
            }

            int alphaStride;
            const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
            const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

            for (int32 y = 0; y < imageSize.v; y++)
            {
                const int32 uvJ = y >> yChromaShift;
                const uint16_t* srcY = reinterpret_cast<const uint16_t*>(yPlaneScan0 + (static_cast<int64>(y) * yPlaneStride));
                const uint16_t* srcCb = reinterpret_cast<const uint16_t*>(cbPlaneScan0 + (static_cast<int64>(uvJ) * cbPlaneStride));
                const uint16_t* srcCr = reinterpret_cast<const uint16_t*>(crPlaneScan0 + (static_cast<int64>(uvJ) * crPlaneStride));

                const uint16_t* srcAlpha = reinterpret_cast<const uint16_t*>(alphaScan0 + (static_cast<int64>(y) * alphaStride));
                float* dst = static_cast<float*>(formatRecord->data);

                DecodeYUV16RowToRGBA32(srcY, srcCb, srcCr, srcAlpha, alphaPremultiplied,
                    dst, imageSize.h, xChromaShift, yuvCoefficiants, tables, transferFunction, loadOptions, hlgLumaCoefficiants);

                const int32 top = y;
                const int32 bottom = y + 1;

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());
            }
        }
        else
        {
            for (int32 y = 0; y < imageSize.v; y++)
            {
                const int32 uvJ = y >> yChromaShift;
                const uint16_t* srcY = reinterpret_cast<const uint16_t*>(yPlaneScan0 + (static_cast<int64>(y) * yPlaneStride));
                const uint16_t* srcCb = reinterpret_cast<const uint16_t*>(cbPlaneScan0 + (static_cast<int64>(uvJ) * cbPlaneStride));
                const uint16_t* srcCr = reinterpret_cast<const uint16_t*>(crPlaneScan0 + (static_cast<int64>(uvJ) * crPlaneStride));

                float* dst = static_cast<float*>(formatRecord->data);

                DecodeYUV16RowToRGB32(srcY, srcCb, srcCr, dst, imageSize.h, xChromaShift,
                    yuvCoefficiants, tables, transferFunction, loadOptions, hlgLumaCoefficiants);

                const int32 top = y;
                const int32 bottom = y + 1;

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());
            }
        }
    }

    ::std::vector<float> BuildUnormToFloatLookupTable(int bitDepth)
    {
        const size_t count = static_cast<size_t>(1) << bitDepth;
        const float maxValue = static_cast<float>(count - 1);

        ::std::vector<float> table(count);

        for (size_t i = 0; i < count; i++)
        {
            table[i] = static_cast<float>(i) / maxValue;
        }

        return table;
    }
}

void ReadHeifImageGrayEightBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord)
{
    const VPoint imageSize = GetImageSize(formatRecord);
    const bool hasAlpha = alphaState != AlphaState::None;

    SetupFormatRecord(formatRecord, imageSize);

    constexpr int lumaBitsPerPixel = 8;

    int grayStride;
    const uint8_t* grayScan0 = heif_image_get_plane_readonly(image, heif_channel_Y, &grayStride);

    ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

    formatRecord->data = buffer.lock();

    const int32 left = 0;
    const int32 right = imageSize.h;

    const YUVLookupTables tables(nclxProfile, lumaBitsPerPixel, true, hasAlpha);

    if (hasAlpha)
    {
        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != lumaBitsPerPixel)
        {
            throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
        }

        int alphaStride;
        const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
        const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint8_t* srcY = grayScan0 + (static_cast<int64>(y) * grayStride);
            const uint8_t* srcAlpha = alphaScan0 + (static_cast<int64>(y) * alphaStride);
            uint8_t* dst = static_cast<uint8_t*>(formatRecord->data);

            DecodeY8RowToGrayAlpha8(srcY, srcAlpha, alphaPremultiplied, dst, imageSize.h, tables);

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
    else
    {
        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint8_t* srcY = grayScan0 + (static_cast<int64>(y) * grayStride);
            uint8_t* dst = static_cast<uint8_t*>(formatRecord->data);

            DecodeY8RowToGray8(srcY, dst, imageSize.h, tables);

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
}

void ReadHeifImageGraySixteenBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord)
{
    const VPoint imageSize = GetImageSize(formatRecord);
    const bool hasAlpha = alphaState != AlphaState::None;

    SetupFormatRecord(formatRecord, imageSize);
    formatRecord->maxValue = 32768;

    const int lumaBitsPerPixel = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

    int grayStride;
    const uint8_t* grayScan0 = heif_image_get_plane_readonly(image, heif_channel_Y, &grayStride);

    ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

    formatRecord->data = buffer.lock();

    const int32 left = 0;
    const int32 right = imageSize.h;

    const YUVLookupTables tables(nclxProfile, lumaBitsPerPixel, true, hasAlpha);

    if (hasAlpha)
    {
        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != lumaBitsPerPixel)
        {
            throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
        }

        int alphaStride;
        const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
        const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint16_t* srcGray = reinterpret_cast<const uint16_t*>(grayScan0 + (static_cast<int64>(y) * grayStride));
            const uint16_t* srcAlpha = reinterpret_cast<const uint16_t*>(alphaScan0 + (static_cast<int64>(y) * alphaStride));
            uint16_t* dst = static_cast<uint16_t*>(formatRecord->data);

            DecodeY16RowToGrayAlpha16(srcGray, srcAlpha, alphaPremultiplied, dst, imageSize.h, tables);

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
    else
    {
        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint16_t* srcGray = reinterpret_cast<const uint16_t*>(grayScan0 + (static_cast<int64>(y) * grayStride));
            uint16_t* dst = static_cast<uint16_t*>(formatRecord->data);

            DecodeY16RowToGray16(srcGray, dst, imageSize.h, tables);

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
}

void ReadHeifImageRGBEightBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord)
{
    const heif_colorspace colorspace = heif_image_get_colorspace(image);

    // The image color space can be either YCbCr or RGB.
    if (colorspace == heif_colorspace_YCbCr)
    {
        ReadHeifImageYUVEightBit(image, alphaState, nclxProfile, formatRecord);
        return;
    }
    else if (colorspace != heif_colorspace_RGB)
    {
        throw std::runtime_error("Unsupported image color space, expected RGB.");
    }

    const VPoint imageSize = GetImageSize(formatRecord);
    const bool hasAlpha = alphaState != AlphaState::None;

    const int redBitsPerPixel = heif_image_get_bits_per_pixel_range(image, heif_channel_R);

    if (redBitsPerPixel != 8)
    {
        throw std::runtime_error("Unsupported RGB channel bit depth, expected 8 bits-per-channel.");
    }

    if (heif_image_get_bits_per_pixel_range(image, heif_channel_G) != redBitsPerPixel ||
        heif_image_get_bits_per_pixel_range(image, heif_channel_B) != redBitsPerPixel)
    {
        throw std::runtime_error("The color channel bit depths do not match.");
    }

    SetupFormatRecord(formatRecord, imageSize);

    constexpr uint8_t rgbMaxValue = 255;

    int rPlaneStride;
    const uint8_t* rPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_R, &rPlaneStride);

    int gPlaneStride;
    const uint8_t* gPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_G, &gPlaneStride);

    int bPlaneStride;
    const uint8_t* bPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_B, &bPlaneStride);

    ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

    formatRecord->data = buffer.lock();

    const int32 left = 0;
    const int32 right = imageSize.h;

    if (hasAlpha)
    {
        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != redBitsPerPixel)
        {
            throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
        }

        int alphaStride;
        const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
        const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint8_t* srcR = rPlaneScan0 + (static_cast<int64>(y) * rPlaneStride);
            const uint8_t* srcG = gPlaneScan0 + (static_cast<int64>(y) * gPlaneStride);
            const uint8_t* srcB = bPlaneScan0 + (static_cast<int64>(y) * bPlaneStride);
            const uint8_t* srcAlpha = alphaScan0 + (static_cast<int64>(y) * alphaStride);

            uint8_t* dst = static_cast<uint8_t*>(formatRecord->data);

            for (int32 x = 0; x < imageSize.h; x++)
            {
                uint8_t r = *srcR;
                uint8_t g = *srcG;
                uint8_t b = *srcB;
                uint8_t a = *srcAlpha;

                if (alphaPremultiplied)
                {
                    if (a < rgbMaxValue)
                    {
                        if (a == 0)
                        {
                            r = 0;
                            g = 0;
                            b = 0;
                        }
                        else
                        {
                            r = UnpremultiplyColor(r, a);
                            g = UnpremultiplyColor(g, a);
                            b = UnpremultiplyColor(b, a);
                        }
                    }
                }

                dst[0] = r;
                dst[1] = g;
                dst[2] = b;
                dst[3] = a;

                srcR++;
                srcG++;
                srcB++;
                srcAlpha++;
                dst += 4;
            }

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
    else
    {
        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint8_t* srcR = rPlaneScan0 + (static_cast<int64>(y) * rPlaneStride);
            const uint8_t* srcG = gPlaneScan0 + (static_cast<int64>(y) * gPlaneStride);
            const uint8_t* srcB = bPlaneScan0 + (static_cast<int64>(y) * bPlaneStride);

            uint8_t* dst = static_cast<uint8_t*>(formatRecord->data);

            for (int32 x = 0; x < imageSize.h; x++)
            {
                dst[0] = *srcR;
                dst[1] = *srcG;
                dst[2] = *srcB;

                srcR++;
                srcG++;
                srcB++;
                dst += 3;
            }

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
}

void ReadHeifImageRGBSixteenBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord)
{
    const heif_colorspace colorspace = heif_image_get_colorspace(image);

    // The image color space can be either YCbCr or RGB.
    if (colorspace == heif_colorspace_YCbCr)
    {
        ReadHeifImageYUVSixteenBit(image, alphaState, nclxProfile, formatRecord);
        return;
    }
    else if (colorspace != heif_colorspace_RGB)
    {
        throw std::runtime_error("Unsupported image color space, expected RGB.");
    }

    const VPoint imageSize = GetImageSize(formatRecord);
    const bool hasAlpha = alphaState != AlphaState::None;

    const int redBitsPerPixel = heif_image_get_bits_per_pixel_range(image, heif_channel_R);

    if (heif_image_get_bits_per_pixel_range(image, heif_channel_G) != redBitsPerPixel ||
        heif_image_get_bits_per_pixel_range(image, heif_channel_B) != redBitsPerPixel)
    {
        throw std::runtime_error("The color channel bit depths do not match.");
    }

    const uint16_t maxValue = (1 << redBitsPerPixel) - 1;

    SetupFormatRecord(formatRecord, imageSize);
    formatRecord->maxValue = maxValue;

    int rPlaneStride;
    const uint8_t* rPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_R, &rPlaneStride);

    int gPlaneStride;
    const uint8_t* gPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_G, &gPlaneStride);

    int bPlaneStride;
    const uint8_t* bPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_B, &bPlaneStride);

    ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

    formatRecord->data = buffer.lock();

    const int32 left = 0;
    const int32 right = imageSize.h;

    if (hasAlpha)
    {
        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != redBitsPerPixel)
        {
            throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
        }

        int alphaStride;
        const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
        const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint16_t* srcR = reinterpret_cast<const uint16_t*>(rPlaneScan0 + (static_cast<int64>(y) * rPlaneStride));
            const uint16_t* srcG = reinterpret_cast<const uint16_t*>(gPlaneScan0 + (static_cast<int64>(y) * gPlaneStride));
            const uint16_t* srcB = reinterpret_cast<const uint16_t*>(bPlaneScan0 + (static_cast<int64>(y) * bPlaneStride));
            const uint16_t* srcAlpha = reinterpret_cast<const uint16_t*>(alphaScan0 + (static_cast<int64>(y) * alphaStride));

            uint16_t* dst = static_cast<uint16_t*>(formatRecord->data);

            for (int32 x = 0; x < imageSize.h; x++)
            {
                uint16_t r = *srcR & maxValue;
                uint16_t g = *srcG & maxValue;
                uint16_t b = *srcB & maxValue;
                uint16_t a = *srcAlpha & maxValue;

                if (alphaPremultiplied)
                {
                    if (a < maxValue)
                    {
                        if (a == 0)
                        {
                            r = 0;
                            g = 0;
                            b = 0;
                        }
                        else
                        {
                            r = UnpremultiplyColor(r, a, maxValue);
                            g = UnpremultiplyColor(g, a, maxValue);
                            b = UnpremultiplyColor(b, a, maxValue);
                        }
                    }
                }

                dst[0] = r;
                dst[1] = g;
                dst[2] = b;
                dst[3] = a;

                srcR++;
                srcG++;
                srcB++;
                srcAlpha++;
                dst += 4;
            }

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
    else
    {
        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint16_t* srcR = reinterpret_cast<const uint16_t*>(rPlaneScan0 + (static_cast<int64>(y) * rPlaneStride));
            const uint16_t* srcG = reinterpret_cast<const uint16_t*>(gPlaneScan0 + (static_cast<int64>(y) * gPlaneStride));
            const uint16_t* srcB = reinterpret_cast<const uint16_t*>(bPlaneScan0 + (static_cast<int64>(y) * bPlaneStride));

            uint16_t* dst = static_cast<uint16_t*>(formatRecord->data);

            for (int32 x = 0; x < imageSize.h; x++)
            {
                dst[0] = *srcR & maxValue;
                dst[1] = *srcG & maxValue;
                dst[2] = *srcB & maxValue;

                srcR++;
                srcG++;
                srcB++;
                dst += 3;
            }

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
}

void ReadHeifImageGrayThirtyTwoBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    const LoadUIOptions& loadOptions,
    FormatRecordPtr formatRecord)
{
    if (nclxProfile == nullptr)
    {
        throw std::runtime_error("The nclxProfile is null.");
    }

    const VPoint imageSize = GetImageSize(formatRecord);
    const bool hasAlpha = alphaState != AlphaState::None;

    SetupFormatRecord(formatRecord, imageSize);

    const int lumaBitsPerPixel = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

    int grayStride;
    const uint8_t* grayScan0 = heif_image_get_plane_readonly(image, heif_channel_Y, &grayStride);

    ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

    formatRecord->data = buffer.lock();

    const int32 left = 0;
    const int32 right = imageSize.h;

    const YUVLookupTables tables(nclxProfile, lumaBitsPerPixel, true, hasAlpha);
    const ColorTransferFunction transferFunction = GetTransferFunctionFromNclx(nclxProfile->transfer_characteristics);

    if (hasAlpha)
    {
        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != lumaBitsPerPixel)
        {
            throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
        }

        int alphaStride;
        const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
        const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint16_t* srcGray = reinterpret_cast<const uint16_t*>(grayScan0 + (static_cast<int64>(y) * grayStride));
            const uint16_t* srcAlpha = reinterpret_cast<const uint16_t*>(alphaScan0 + (static_cast<int64>(y) * alphaStride));
            float* dst = static_cast<float*>(formatRecord->data);

            DecodeY16RowToGrayAlpha32(
                srcGray,
                srcAlpha,
                alphaPremultiplied,
                dst,
                imageSize.h,
                tables,
                transferFunction,
                loadOptions);

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
    else
    {
        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint16_t* srcGray = reinterpret_cast<const uint16_t*>(grayScan0 + (static_cast<int64>(y) * grayStride));
            float* dst = static_cast<float*>(formatRecord->data);

            DecodeY16RowToGray32(srcGray, dst, imageSize.h, tables, transferFunction, loadOptions);

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
}

void ReadHeifImageRGBThirtyTwoBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    const LoadUIOptions& loadOptions,
    FormatRecordPtr formatRecord)
{
    if (nclxProfile == nullptr)
    {
        throw std::runtime_error("The nclxProfile is null.");
    }

    const ColorTransferFunction transferFunction = GetTransferFunctionFromNclx(nclxProfile->transfer_characteristics);

    const heif_colorspace colorspace = heif_image_get_colorspace(image);

    // The image color space can be either YCbCr or RGB.
    if (colorspace == heif_colorspace_YCbCr)
    {
        ReadHeifImageYUVThirtyTwoBit(image, alphaState, nclxProfile, formatRecord, transferFunction, loadOptions);
        return;
    }
    else if (colorspace != heif_colorspace_RGB)
    {
        throw std::runtime_error("Unsupported image color space, expected RGB.");
    }

    const VPoint imageSize = GetImageSize(formatRecord);
    const bool hasAlpha = alphaState != AlphaState::None;

    const int redBitsPerPixel = heif_image_get_bits_per_pixel_range(image, heif_channel_R);

    if (heif_image_get_bits_per_pixel_range(image, heif_channel_G) != redBitsPerPixel ||
        heif_image_get_bits_per_pixel_range(image, heif_channel_B) != redBitsPerPixel)
    {
        throw std::runtime_error("The color channel bit depths do not match.");
    }

    SetupFormatRecord(formatRecord, imageSize);

    int rPlaneStride;
    const uint8_t* rPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_R, &rPlaneStride);

    int gPlaneStride;
    const uint8_t* gPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_G, &gPlaneStride);

    int bPlaneStride;
    const uint8_t* bPlaneScan0 = heif_image_get_plane_readonly(image, heif_channel_B, &bPlaneStride);

    ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

    formatRecord->data = buffer.lock();

    const int32 left = 0;
    const int32 right = imageSize.h;

    const ::std::vector<float> unormToFloatTable = BuildUnormToFloatLookupTable(redBitsPerPixel);

    HLGLumaCoefficiants hlgLumaCoefficiants{};

    if (transferFunction == ColorTransferFunction::HLG && loadOptions.hlg.applyOOTF)
    {
        hlgLumaCoefficiants = GetHLGLumaCoefficients(nclxProfile->color_primaries);
    }

    if (hasAlpha)
    {
        if (heif_image_get_bits_per_pixel_range(image, heif_channel_Alpha) != redBitsPerPixel)
        {
            throw std::runtime_error("The alpha channel bit depth does not match the main image channels.");
        }

        int alphaStride;
        const uint8_t* alphaScan0 = heif_image_get_plane_readonly(image, heif_channel_Alpha, &alphaStride);
        const bool alphaPremultiplied = alphaState == AlphaState::Premultiplied;

        const uint16_t rgbMaxValue = (1 << redBitsPerPixel) - 1;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint16_t* srcR = reinterpret_cast<const uint16_t*>(rPlaneScan0 + (static_cast<int64>(y) * rPlaneStride));
            const uint16_t* srcG = reinterpret_cast<const uint16_t*>(gPlaneScan0 + (static_cast<int64>(y) * gPlaneStride));
            const uint16_t* srcB = reinterpret_cast<const uint16_t*>(bPlaneScan0 + (static_cast<int64>(y) * bPlaneStride));
            const uint16_t* srcAlpha = reinterpret_cast<const uint16_t*>(alphaScan0 + (static_cast<int64>(y) * alphaStride));

            float* dst = static_cast<float*>(formatRecord->data);

            for (int32 x = 0; x < imageSize.h; x++)
            {
                uint16_t unormR = *srcR;
                uint16_t unormG = *srcG;
                uint16_t unormB = *srcB;
                const uint16_t unormA = *srcAlpha;

                if (alphaPremultiplied)
                {
                    if (unormA < rgbMaxValue)
                    {
                        if (unormA == 0)
                        {
                            unormR = 0;
                            unormG = 0;
                            unormB = 0;
                        }
                        else
                        {
                            unormR = UnpremultiplyColor(unormR, unormA, rgbMaxValue);
                            unormG = UnpremultiplyColor(unormG, unormA, rgbMaxValue);
                            unormB = UnpremultiplyColor(unormB, unormA, rgbMaxValue);
                        }
                    }
                }

                const float r = unormToFloatTable[unormR];
                const float g = unormToFloatTable[unormG];
                const float b = unormToFloatTable[unormB];
                const float a = unormToFloatTable[unormA];

                switch (transferFunction)
                {
                case ColorTransferFunction::PQ:
                    dst[0] = PQToLinear(r, static_cast<float>(loadOptions.pq.nominalPeakBrightness));
                    dst[1] = PQToLinear(g, static_cast<float>(loadOptions.pq.nominalPeakBrightness));
                    dst[2] = PQToLinear(b, static_cast<float>(loadOptions.pq.nominalPeakBrightness));
                    break;
                case ColorTransferFunction::HLG:
                    dst[0] = HLGToLinear(r);
                    dst[1] = HLGToLinear(g);
                    dst[2] = HLGToLinear(b);

                    if (loadOptions.hlg.applyOOTF)
                    {
                        ApplyHLGOOTF(
                            dst,
                            hlgLumaCoefficiants,
                            loadOptions.hlg.displayGamma,
                            static_cast<float>(loadOptions.hlg.nominalPeakBrightness));
                    }
                    break;
                case ColorTransferFunction::SMPTE428:
                    dst[0] = SMPTE428ToLinear(r);
                    dst[1] = SMPTE428ToLinear(g);
                    dst[2] = SMPTE428ToLinear(b);
                    break;
                default:
                    throw std::runtime_error("Unsupported color transfer function.");
                }

                dst[3] = a;

                srcR++;
                srcG++;
                srcB++;
                srcAlpha++;
                dst += 4;
            }

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
    else
    {
        for (int32 y = 0; y < imageSize.v; y++)
        {
            const uint16_t* srcR = reinterpret_cast<const uint16_t*>(rPlaneScan0 + (static_cast<int64>(y) * rPlaneStride));
            const uint16_t* srcG = reinterpret_cast<const uint16_t*>(gPlaneScan0 + (static_cast<int64>(y) * gPlaneStride));
            const uint16_t* srcB = reinterpret_cast<const uint16_t*>(bPlaneScan0 + (static_cast<int64>(y) * bPlaneStride));

            float* dst = static_cast<float*>(formatRecord->data);

            for (int32 x = 0; x < imageSize.h; x++)
            {
                const uint16_t unormR = *srcR;
                const uint16_t unormG = *srcG;
                const uint16_t unormB = *srcB;

                const float r = unormToFloatTable[unormR];
                const float g = unormToFloatTable[unormG];
                const float b = unormToFloatTable[unormB];

                switch (transferFunction)
                {
                case ColorTransferFunction::PQ:
                    dst[0] = PQToLinear(r, static_cast<float>(loadOptions.pq.nominalPeakBrightness));
                    dst[1] = PQToLinear(g, static_cast<float>(loadOptions.pq.nominalPeakBrightness));
                    dst[2] = PQToLinear(b, static_cast<float>(loadOptions.pq.nominalPeakBrightness));
                    break;
                case ColorTransferFunction::HLG:
                    dst[0] = HLGToLinear(r);
                    dst[1] = HLGToLinear(g);
                    dst[2] = HLGToLinear(b);

                    if (loadOptions.hlg.applyOOTF)
                    {
                        ApplyHLGOOTF(
                            dst,
                            hlgLumaCoefficiants,
                            loadOptions.hlg.displayGamma,
                            static_cast<float>(loadOptions.hlg.nominalPeakBrightness));
                    }
                    break;
                case ColorTransferFunction::SMPTE428:
                    dst[0] = SMPTE428ToLinear(r);
                    dst[1] = SMPTE428ToLinear(g);
                    dst[2] = SMPTE428ToLinear(b);
                    break;
                default:
                    throw std::runtime_error("Unsupported color transfer function.");
                }

                srcR++;
                srcG++;
                srcB++;
                dst += 3;
            }

            const int32 top = y;
            const int32 bottom = y + 1;

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());
        }
    }
}
