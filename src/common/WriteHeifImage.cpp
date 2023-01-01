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

#include "WriteHeifImage.h"
#include "ColorProfileConversion.h"
#include "LibHeifException.h"
#include "OSErrException.h"
#include "PremultipliedAlpha.h"
#include "Utilities.h"
#include <algorithm>
#include <vector>

namespace
{
    ScopedHeifImage CreateHeifImage(int width, int height, heif_colorspace colorspace, heif_chroma chroma)
    {
        heif_image* tempImage;

        LibHeifException::ThrowIfError(heif_image_create(width, height, colorspace, chroma, &tempImage));

        return ScopedHeifImage(tempImage);
    }

    int GetHeifImageBitDepth(ImageBitDepth bitDepth)
    {
        int value;

        switch (bitDepth)
        {
        case ImageBitDepth::Eight:
            value = 8;
            break;
        case ImageBitDepth::Ten:
            value = 10;
            break;
        case ImageBitDepth::Twelve:
            value = 12;
            break;
        default:
            throw OSErrException(formatCannotRead);
        }

        return value;
    }

    heif_chroma GetRGBImageChroma(ImageBitDepth bitDepth, bool hasAlpha)
    {
        heif_chroma chroma;

        switch (bitDepth)
        {
        case ImageBitDepth::Eight:
            chroma = hasAlpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB;
            break;
        case ImageBitDepth::Ten:
        case ImageBitDepth::Twelve:
#ifdef __PIMacPPC__
            chroma = hasAlpha ? heif_chroma_interleaved_RRGGBBAA_BE : heif_chroma_interleaved_RRGGBB_BE;
#else
            chroma = hasAlpha ? heif_chroma_interleaved_RRGGBBAA_LE : heif_chroma_interleaved_RRGGBB_LE;
#endif // __PIMacPPC__
            break;
        default:
            throw OSErrException(formatCannotRead);
        }

        return chroma;
    }

    std::vector<uint16_t> BuildEightBitToHeifImageLookup(int bitDepth)
    {
        std::vector<uint16_t> lookupTable;
        lookupTable.reserve(256);

        const int maxValue = (1 << bitDepth) - 1;
        const float maxValueFloat = static_cast<float>(maxValue);

        for (size_t i = 0; i < lookupTable.capacity(); i++)
        {
            int value = static_cast<int>(((static_cast<float>(i) / 255.0f) * maxValueFloat) + 0.5f);

            if (value < 0)
            {
                value = 0;
            }
            else if (value > maxValue)
            {
                value = maxValue;
            }

            lookupTable.push_back(static_cast<uint16_t>(value));
        }

        return lookupTable;
    }

    std::vector<uint8_t> BuildSixteenBitToEightBitLookup()
    {
        std::vector<uint8_t> lookupTable;
        lookupTable.reserve(32769);

        constexpr int maxValue = std::numeric_limits<uint8_t>::max();
        constexpr float maxValueFloat = static_cast<float>(maxValue);

        for (size_t i = 0; i < lookupTable.capacity(); i++)
        {
            int value = static_cast<int>(((static_cast<float>(i) / 32768.0f) * maxValueFloat) + 0.5f);

            if (value < 0)
            {
                value = 0;
            }
            else if (value > maxValue)
            {
                value = maxValue;
            }

            lookupTable.push_back(static_cast<uint8_t>(value));
        }

        return lookupTable;
    }

    std::vector<uint16_t> BuildSixteenBitToHeifImageLookup(int bitDepth)
    {
        std::vector<uint16_t> lookupTable;
        lookupTable.reserve(32769);

        const int maxValue = (1 << bitDepth) - 1;
        const float maxValueFloat = static_cast<float>(maxValue);

        for (size_t i = 0; i < lookupTable.capacity(); i++)
        {
            int value = static_cast<int>(((static_cast<float>(i) / 32768.0f) * maxValueFloat) + 0.5f);

            if (value < 0)
            {
                value = 0;
            }
            else if (value > maxValue)
            {
                value = maxValue;
            }

            lookupTable.push_back(static_cast<uint16_t>(value));
        }

        return lookupTable;
    }
}

ScopedHeifImage CreateHeifImageGrayEightBit(
    FormatRecordPtr formatRecord,
    AlphaState alphaState,
    const VPoint& imageSize,
    const SaveUIOptions& saveOptions)
{
    const bool hasAlpha = alphaState != AlphaState::None;

    ScopedHeifImage image = CreateHeifImage(imageSize.h, imageSize.v, heif_colorspace_monochrome, heif_chroma_monochrome);

    const int heifImageBitDepth = GetHeifImageBitDepth(saveOptions.imageBitDepth);

    LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_Y, imageSize.h, imageSize.v, heifImageBitDepth));

    int yPlaneStride;
    uint8_t* yPlaneScan0 = heif_image_get_plane(image.get(), heif_channel_Y, &yPlaneStride);

    int alphaPlaneStride = 0;
    uint8_t* alphaPlaneScan0 = nullptr;

    if (hasAlpha)
    {
        LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_Alpha, imageSize.h, imageSize.v, heifImageBitDepth));

        alphaPlaneScan0 = heif_image_get_plane(image.get(), heif_channel_Alpha, &alphaPlaneStride);
    }

    const int32 left = 0;
    const int32 right = imageSize.h;

    if (heifImageBitDepth > 8)
    {
        // The 8-bit data must be converted to 10-bit or 12-bit when writing it to the heif_image.

        std::vector<uint16_t> lookupTable = BuildEightBitToHeifImageLookup(heifImageBitDepth);
        const uint16_t maxValue = static_cast<uint16_t>((1 << heifImageBitDepth) - 1);

        for (int32 y = 0; y < imageSize.v; y++)
        {
            if (formatRecord->abortProc())
            {
                throw OSErrException(userCanceledErr);
            }

            const int32 top = y;
            const int32 bottom = std::min(top + 1, imageSize.v);

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());

            const uint8_t* src = static_cast<const uint8_t*>(formatRecord->data);
            uint16_t* yPlane = reinterpret_cast<uint16_t*>(yPlaneScan0 + ((static_cast<int64_t>(y) * yPlaneStride)));

            if (hasAlpha)
            {
                uint16_t* alphaPlane = reinterpret_cast<uint16_t*>(alphaPlaneScan0 + ((static_cast<int64>(y) * alphaPlaneStride)));

                for (int32 x = 0; x < imageSize.h; x++)
                {
                    uint16_t gray = lookupTable[src[0]];
                    uint16_t alpha = lookupTable[src[1]];

                    if (alphaState == AlphaState::Premultiplied)
                    {
                        if (alpha < maxValue)
                        {
                            if (alpha == 0)
                            {
                                gray = 0;
                            }
                            else
                            {
                                gray = PremultiplyColor(gray, alpha, maxValue);
                            }
                        }
                    }

                    yPlane[0] = gray;
                    alphaPlane[0] = alpha;

                    src += 2;
                    yPlane++;
                    alphaPlane++;
                }
            }
            else
            {
                for (int32 x = 0; x < imageSize.h; x++)
                {
                    yPlane[0] = lookupTable[src[0]];

                    src++;
                    yPlane++;
                }
            }
        }
    }
    else
    {
        constexpr uint8_t maxValue = 255;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            if (formatRecord->abortProc())
            {
                throw OSErrException(userCanceledErr);
            }

            const int32 top = y;
            const int32 bottom = std::min(top + 1, imageSize.v);

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());

            const uint8_t* src = static_cast<const uint8_t*>(formatRecord->data);
            uint8_t* yPlane = yPlaneScan0 + ((static_cast<int64_t>(y) * yPlaneStride));

            if (hasAlpha)
            {
                uint8_t* alphaPlane = alphaPlaneScan0 + ((static_cast<int64_t>(y) * alphaPlaneStride));

                for (int32 x = 0; x < imageSize.h; x++)
                {
                    uint8_t gray = src[0];
                    uint8_t alpha = src[1];

                    if (alphaState == AlphaState::Premultiplied)
                    {
                        if (alpha < maxValue)
                        {
                            if (alpha == 0)
                            {
                                gray = 0;
                            }
                            else
                            {
                                gray = PremultiplyColor(gray, alpha);
                            }
                        }
                    }

                    yPlane[0] = gray;
                    alphaPlane[0] = alpha;

                    src += 2;
                    yPlane++;
                    alphaPlane++;
                }
            }
            else
            {
                for (int32 x = 0; x < imageSize.h; x++)
                {
                    yPlane[0] = src[0];

                    src++;
                    yPlane++;
                }
            }
        }
    }

    return image;
}

ScopedHeifImage CreateHeifImageGraySixteenBit(FormatRecordPtr formatRecord, AlphaState alphaState, const VPoint& imageSize, const SaveUIOptions& saveOptions)
{
    const bool hasAlpha = alphaState != AlphaState::None;

    ScopedHeifImage image = CreateHeifImage(imageSize.h, imageSize.v, heif_colorspace_monochrome, heif_chroma_monochrome);

    const int heifImageBitDepth = GetHeifImageBitDepth(saveOptions.imageBitDepth);

    LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_Y, imageSize.h, imageSize.v, heifImageBitDepth));

    int yPlaneStride;
    uint8_t* yPlaneScan0 = heif_image_get_plane(image.get(), heif_channel_Y, &yPlaneStride);

    int alphaPlaneStride = 0;
    uint8_t* alphaPlaneScan0 = nullptr;

    if (hasAlpha)
    {
        LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_Alpha, imageSize.h, imageSize.v, heifImageBitDepth));

        alphaPlaneScan0 = heif_image_get_plane(image.get(), heif_channel_Alpha, &alphaPlaneStride);
    }

    const int32 left = 0;
    const int32 right = imageSize.h;

    if (heifImageBitDepth == 8)
    {
        // The 16-bit data must be converted to 8-bit when writing it to the heif_image.

        std::vector<uint8> lookupTable = BuildSixteenBitToEightBitLookup();
        constexpr uint8_t maxValue = 255;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            if (formatRecord->abortProc())
            {
                throw OSErrException(userCanceledErr);
            }

            const int32 top = y;
            const int32 bottom = std::min(top + 1, imageSize.v);

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());

            const uint16_t* src = static_cast<const uint16_t*>(formatRecord->data);
            uint8_t* yPlane = yPlaneScan0 + ((static_cast<int64_t>(y) * yPlaneStride));

            if (hasAlpha)
            {
                uint8_t* alphaPlane = alphaPlaneScan0 + ((static_cast<int64_t>(y) * alphaPlaneStride));

                for (int32 x = 0; x < imageSize.h; x++)
                {
                    uint8_t gray = lookupTable[src[0]];
                    uint8_t alpha = lookupTable[src[1]];

                    if (alphaState == AlphaState::Premultiplied)
                    {
                        if (alpha < maxValue)
                        {
                            if (alpha == 0)
                            {
                                gray = 0;
                            }
                            else
                            {
                                gray = PremultiplyColor(gray, alpha);
                            }
                        }
                    }

                    yPlane[0] = gray;
                    alphaPlane[0] = alpha;

                    src += 2;
                    yPlane++;
                    alphaPlane++;
                }
            }
            else
            {
                for (int32 x = 0; x < imageSize.h; x++)
                {
                    yPlane[0] = lookupTable[src[0]];

                    src++;
                    yPlane++;
                }
            }
        }
    }
    else
    {
        // The 16-bit data must be converted to 10-bit or 12-bit when writing it to the heif_image.

        std::vector<uint16_t> lookupTable = BuildSixteenBitToHeifImageLookup(heifImageBitDepth);
        const uint16_t maxValue = static_cast<uint16_t>((1 << heifImageBitDepth) - 1);

        for (int32 y = 0; y < imageSize.v; y++)
        {
            if (formatRecord->abortProc())
            {
                throw OSErrException(userCanceledErr);
            }

            const int32 top = y;
            const int32 bottom = std::min(top + 1, imageSize.v);

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());

            const uint16_t* src = static_cast<const uint16*>(formatRecord->data);
            uint16_t* yPlane = reinterpret_cast<uint16*>(yPlaneScan0 + ((static_cast<int64_t>(y) * yPlaneStride)));

            if (hasAlpha)
            {
                uint16_t* alphaPlane = reinterpret_cast<uint16*>(alphaPlaneScan0 + ((static_cast<int64_t>(y) * alphaPlaneStride)));

                for (int32 x = 0; x < imageSize.h; x++)
                {
                    uint16_t gray = lookupTable[src[0]];
                    uint16_t alpha = lookupTable[src[1]];

                    if (alphaState == AlphaState::Premultiplied)
                    {
                        if (alpha < maxValue)
                        {
                            if (alpha == 0)
                            {
                                gray = 0;
                            }
                            else
                            {
                                gray = PremultiplyColor(gray, alpha, maxValue);
                            }
                        }
                    }

                    yPlane[0] = gray;
                    alphaPlane[0] = alpha;

                    src += 2;
                    yPlane++;
                    alphaPlane++;
                }
            }
            else
            {
                for (int32 x = 0; x < imageSize.h; x++)
                {
                    yPlane[0] = lookupTable[src[0]];

                    src++;
                    yPlane++;
                }
            }
        }
    }

    return image;
}

ScopedHeifImage CreateHeifImageGrayThirtyTwoBit(
    FormatRecordPtr formatRecord,
    AlphaState alphaState,
    const VPoint& imageSize,
    const SaveUIOptions& saveOptions)
{
    const bool hasAlpha = alphaState != AlphaState::None;

    ScopedHeifImage image = CreateHeifImage(imageSize.h, imageSize.v, heif_colorspace_monochrome, heif_chroma_monochrome);

    const int heifImageBitDepth = GetHeifImageBitDepth(saveOptions.imageBitDepth);

    LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_Y, imageSize.h, imageSize.v, heifImageBitDepth));

    int yPlaneStride;
    uint8_t* yPlaneScan0 = heif_image_get_plane(image.get(), heif_channel_Y, &yPlaneStride);

    int alphaPlaneStride = 0;
    uint8_t* alphaPlaneScan0 = nullptr;

    if (hasAlpha)
    {
        LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_Alpha, imageSize.h, imageSize.v, heifImageBitDepth));

        alphaPlaneScan0 = heif_image_get_plane(image.get(), heif_channel_Alpha, &alphaPlaneStride);
    }

    const int32 left = 0;
    const int32 right = imageSize.h;

    const float heifImageMaxValue = static_cast<float>((1 << heifImageBitDepth) - 1);
    const ColorTransferFunction transferFunction = saveOptions.hdrTransferFunction;

    for (int32 y = 0; y < imageSize.v; y++)
    {
        if (formatRecord->abortProc())
        {
            throw OSErrException(userCanceledErr);
        }

        const int32 top = y;
        const int32 bottom = std::min(top + 1, imageSize.v);

        SetRect(formatRecord, top, left, bottom, right);

        OSErrException::ThrowIfError(formatRecord->advanceState());

        const float* src = static_cast<const float*>(formatRecord->data);
        uint16_t* yPlane = reinterpret_cast<uint16*>(yPlaneScan0 + ((static_cast<int64_t>(y) * yPlaneStride)));

        if (hasAlpha)
        {
            uint16_t* alphaPlane = reinterpret_cast<uint16*>(alphaPlaneScan0 + ((static_cast<int64_t>(y) * alphaPlaneStride)));

            for (int32 x = 0; x < imageSize.h; x++)
            {
                float gray = src[0];
                float alpha = std::clamp(src[1], 0.0f, 1.0f);

                if (alphaState == AlphaState::Premultiplied)
                {
                    if (alpha < 1.0f)
                    {
                        if (alpha == 0)
                        {
                            gray = 0;
                        }
                        else
                        {
                            gray = PremultiplyColor(std::clamp(gray, 0.0f, 1.0f), alpha, 1.0f);
                        }
                    }
                }

                const float transferCurveGray = LinearToTransferFunction(gray, transferFunction);

                yPlane[0] = static_cast<uint16_t>(std::clamp(transferCurveGray * heifImageMaxValue, 0.0f, heifImageMaxValue));
                alphaPlane[0] = static_cast<uint16_t>(std::clamp(alpha * heifImageMaxValue, 0.0f, heifImageMaxValue));

                src += 2;
                yPlane++;
                alphaPlane++;
            }
        }
        else
        {
            for (int32 x = 0; x < imageSize.h; x++)
            {
                const float gray = std::clamp(src[0], 0.0f, 1.0f);

                const float transferCurveGray = LinearToTransferFunction(gray, transferFunction);

                yPlane[0] = static_cast<uint16_t>(std::clamp(transferCurveGray * heifImageMaxValue, 0.0f, heifImageMaxValue));

                src++;
                yPlane++;
            }
        }
    }

    return image;
}

ScopedHeifImage CreateHeifImageRGBEightBit(
    FormatRecordPtr formatRecord,
    AlphaState alphaState,
    const VPoint& imageSize,
    const SaveUIOptions& saveOptions)
{
    const bool hasAlpha = alphaState != AlphaState::None;

    const heif_chroma chroma = GetRGBImageChroma(saveOptions.imageBitDepth, hasAlpha);

    ScopedHeifImage image = CreateHeifImage(imageSize.h, imageSize.v, heif_colorspace_RGB, chroma);

    const int heifImageBitDepth = GetHeifImageBitDepth(saveOptions.imageBitDepth);

    LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_interleaved, imageSize.h, imageSize.v, heifImageBitDepth));

    int heifImageStride;
    uint8_t* heifImageData = heif_image_get_plane(image.get(), heif_channel_interleaved, &heifImageStride);

    const int32 left = 0;
    const int32 right = imageSize.h;

    ColorProfileConversion converter(formatRecord, hasAlpha, 8, saveOptions.keepColorProfile);

    if (heifImageBitDepth > 8)
    {
        // The 8-bit data must be converted to 10-bit or 12-bit when writing it to the heif_image.

        std::vector<uint16_t> lookupTable = BuildEightBitToHeifImageLookup(heifImageBitDepth);
        const uint16_t maxValue = static_cast<uint16_t>((1 << heifImageBitDepth) - 1);

        for (int32 y = 0; y < imageSize.v; y++)
        {
            if (formatRecord->abortProc())
            {
                throw OSErrException(userCanceledErr);
            }

            const int32 top = y;
            const int32 bottom = std::min(top + 1, imageSize.v);

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());

            converter.ConvertRow(
                formatRecord->data,
                static_cast<cmsUInt32Number>(imageSize.h),
                static_cast<cmsUInt32Number>(formatRecord->rowBytes));

            const uint8_t* src = static_cast<const uint8_t*>(formatRecord->data);
            uint16_t* yPlane = reinterpret_cast<uint16_t*>(heifImageData + ((static_cast<int64_t>(y) * heifImageStride)));

            for (int32 x = 0; x < imageSize.h; x++)
            {
                if (hasAlpha)
                {
                    uint16_t r = lookupTable[src[0]];
                    uint16_t g = lookupTable[src[1]];
                    uint16_t b = lookupTable[src[2]];
                    uint16_t a = lookupTable[src[3]];

                    if (alphaState == AlphaState::Premultiplied)
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
                                r = PremultiplyColor(r, a, maxValue);
                                g = PremultiplyColor(g, a, maxValue);
                                b = PremultiplyColor(b, a, maxValue);
                            }
                        }
                    }

                    yPlane[0] = r;
                    yPlane[1] = g;
                    yPlane[2] = b;
                    yPlane[3] = a;

                    src += 4;
                    yPlane += 4;
                }
                else
                {
                    yPlane[0] = lookupTable[src[0]];
                    yPlane[1] = lookupTable[src[1]];
                    yPlane[2] = lookupTable[src[2]];

                    src += 3;
                    yPlane += 3;
                }
            }
        }
    }
    else
    {
        constexpr uint8_t maxValue = 255;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            if (formatRecord->abortProc())
            {
                throw OSErrException(userCanceledErr);
            }

            const int32 top = y;
            const int32 bottom = std::min(top + 1, imageSize.v);

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());

            converter.ConvertRow(
                formatRecord->data,
                static_cast<cmsUInt32Number>(imageSize.h),
                static_cast<cmsUInt32Number>(formatRecord->rowBytes));

            const uint8_t* src = static_cast<const uint8_t*>(formatRecord->data);
            uint8_t* yPlane = heifImageData + ((static_cast<int64_t>(y) * heifImageStride));

            for (int32 x = 0; x < imageSize.h; x++)
            {
                if (hasAlpha)
                {
                    uint8_t r = src[0];
                    uint8_t g = src[1];
                    uint8_t b = src[2];
                    uint8_t a = src[3];

                    if (alphaState == AlphaState::Premultiplied)
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
                                r = PremultiplyColor(r, a);
                                g = PremultiplyColor(g, a);
                                b = PremultiplyColor(b, a);
                            }
                        }
                    }

                    yPlane[0] = r;
                    yPlane[1] = g;
                    yPlane[2] = b;
                    yPlane[3] = a;

                    src += 4;
                    yPlane += 4;
                }
                else
                {
                    yPlane[0] = src[0];
                    yPlane[1] = src[1];
                    yPlane[2] = src[2];

                    src += 3;
                    yPlane += 3;
                }
            }
        }
    }

    return image;
}

ScopedHeifImage CreateHeifImageRGBSixteenBit(
    FormatRecordPtr formatRecord,
    AlphaState alphaState,
    const VPoint& imageSize,
    const SaveUIOptions& saveOptions)
{
    const bool hasAlpha = alphaState != AlphaState::None;

    const heif_chroma chroma = GetRGBImageChroma(saveOptions.imageBitDepth, hasAlpha);

    ScopedHeifImage image = CreateHeifImage(imageSize.h, imageSize.v, heif_colorspace_RGB, chroma);

    const int heifImageBitDepth = GetHeifImageBitDepth(saveOptions.imageBitDepth);

    LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_interleaved, imageSize.h, imageSize.v, heifImageBitDepth));

    int heifImageStride;
    uint8_t* heifImageData = heif_image_get_plane(image.get(), heif_channel_interleaved, &heifImageStride);

    const int32 left = 0;
    const int32 right = imageSize.h;

    ColorProfileConversion converter(formatRecord, hasAlpha, 16, saveOptions.keepColorProfile);

    if (heifImageBitDepth == 8)
    {
        // The 16-bit data must be converted to 8-bit when writing it to the heif_image.

        std::vector<uint8_t> lookupTable = BuildSixteenBitToEightBitLookup();
        constexpr int maxValue = 255;

        for (int32 y = 0; y < imageSize.v; y++)
        {
            if (formatRecord->abortProc())
            {
                throw OSErrException(userCanceledErr);
            }

            const int32 top = y;
            const int32 bottom = std::min(top + 1, imageSize.v);

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());

            converter.ConvertRow(
                formatRecord->data,
                static_cast<cmsUInt32Number>(imageSize.h),
                static_cast<cmsUInt32Number>(formatRecord->rowBytes));

            const uint16_t* src = static_cast<const uint16_t*>(formatRecord->data);
            uint8_t* yPlane = heifImageData + ((static_cast<int64>(y) * heifImageStride));

            for (int32 x = 0; x < imageSize.h; x++)
            {
                if (hasAlpha)
                {
                    uint8_t r = lookupTable[src[0]];
                    uint8_t g = lookupTable[src[1]];
                    uint8_t b = lookupTable[src[2]];
                    uint8_t a = lookupTable[src[3]];

                    if (alphaState == AlphaState::Premultiplied)
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
                                r = PremultiplyColor(r, a);
                                g = PremultiplyColor(g, a);
                                b = PremultiplyColor(b, a);
                            }
                        }
                    }

                    yPlane[0] = r;
                    yPlane[1] = g;
                    yPlane[2] = b;
                    yPlane[3] = a;

                    src += 4;
                    yPlane += 4;
                }
                else
                {
                    yPlane[0] = lookupTable[src[0]];
                    yPlane[1] = lookupTable[src[1]];
                    yPlane[2] = lookupTable[src[2]];

                    src += 3;
                    yPlane += 3;
                }
            }
        }
    }
    else
    {
        // The 16-bit data must be converted to 10-bit or 12-bit when writing it to the heif_image.

        std::vector<uint16_t> lookupTable = BuildSixteenBitToHeifImageLookup(heifImageBitDepth);
        const uint16_t maxValue = static_cast<uint16_t>((1 << heifImageBitDepth) - 1);

        for (int32 y = 0; y < imageSize.v; y++)
        {
            if (formatRecord->abortProc())
            {
                throw OSErrException(userCanceledErr);
            }

            const int32 top = y;
            const int32 bottom = std::min(top + 1, imageSize.v);

            SetRect(formatRecord, top, left, bottom, right);

            OSErrException::ThrowIfError(formatRecord->advanceState());

            converter.ConvertRow(
                formatRecord->data,
                static_cast<cmsUInt32Number>(imageSize.h),
                static_cast<cmsUInt32Number>(formatRecord->rowBytes));

            const uint16_t* src = static_cast<const uint16_t*>(formatRecord->data);
            uint16_t* yPlane = reinterpret_cast<uint16_t*>(heifImageData + ((static_cast<int64_t>(y) * heifImageStride)));

            for (int32 x = 0; x < imageSize.h; x++)
            {
                if (hasAlpha)
                {
                    uint16_t r = lookupTable[src[0]];
                    uint16_t g = lookupTable[src[1]];
                    uint16_t b = lookupTable[src[2]];
                    uint16_t a = lookupTable[src[3]];

                    if (alphaState == AlphaState::Premultiplied)
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
                                r = PremultiplyColor(r, a, maxValue);
                                g = PremultiplyColor(g, a, maxValue);
                                b = PremultiplyColor(b, a, maxValue);
                            }
                        }
                    }

                    yPlane[0] = r;
                    yPlane[1] = g;
                    yPlane[2] = b;
                    yPlane[3] = a;

                    src += 4;
                    yPlane += 4;
                }
                else
                {
                    yPlane[0] = lookupTable[src[0]];
                    yPlane[1] = lookupTable[src[1]];
                    yPlane[2] = lookupTable[src[2]];

                    src += 3;
                    yPlane += 3;
                }
            }
        }
    }

    return image;
}

ScopedHeifImage CreateHeifImageRGBThirtyTwoBit(
    FormatRecordPtr formatRecord,
    AlphaState alphaState,
    const VPoint& imageSize,
    const SaveUIOptions& saveOptions)
{
    const bool hasAlpha = alphaState != AlphaState::None;

    const heif_chroma chroma = GetRGBImageChroma(saveOptions.imageBitDepth, hasAlpha);

    ScopedHeifImage image = CreateHeifImage(imageSize.h, imageSize.v, heif_colorspace_RGB, chroma);

    const int heifImageBitDepth = GetHeifImageBitDepth(saveOptions.imageBitDepth);

    LibHeifException::ThrowIfError(heif_image_add_plane(image.get(), heif_channel_interleaved, imageSize.h, imageSize.v, heifImageBitDepth));

    int heifImageStride;
    uint8_t* heifImageData = heif_image_get_plane(image.get(), heif_channel_interleaved, &heifImageStride);

    const int32 left = 0;
    const int32 right = imageSize.h;

    const float heifImageMaxValue = static_cast<float>((1 << heifImageBitDepth) - 1);
    const ColorTransferFunction transferFunction = saveOptions.hdrTransferFunction;

    ColorProfileConversion converter(formatRecord, hasAlpha, transferFunction, saveOptions.keepColorProfile);

    for (int32 y = 0; y < imageSize.v; y++)
    {
        if (formatRecord->abortProc())
        {
            throw OSErrException(userCanceledErr);
        }

        const int32 top = y;
        const int32 bottom = std::min(top + 1, imageSize.v);

        SetRect(formatRecord, top, left, bottom, right);

        OSErrException::ThrowIfError(formatRecord->advanceState());

        converter.ConvertRow(
            formatRecord->data,
            static_cast<cmsUInt32Number>(imageSize.h),
            static_cast<cmsUInt32Number>(formatRecord->rowBytes));

        const float* src = static_cast<const float*>(formatRecord->data);
        uint16_t* yPlane = reinterpret_cast<uint16_t*>(heifImageData + ((static_cast<int64_t>(y) * heifImageStride)));

        for (int32 x = 0; x < imageSize.h; x++)
        {
            float r = src[0];
            float g = src[1];
            float b = src[2];

            if (hasAlpha)
            {
                const float a = std::clamp(src[3], 0.0f, 1.0f);

                if (alphaState == AlphaState::Premultiplied)
                {
                    if (a < 1.0f)
                    {
                        if (a == 0)
                        {
                            r = 0;
                            g = 0;
                            b = 0;
                        }
                        else
                        {
                            r = PremultiplyColor(std::clamp(r, 0.0f, 1.0f), a, 1.0f);
                            g = PremultiplyColor(std::clamp(g, 0.0f, 1.0f), a, 1.0f);
                            b = PremultiplyColor(std::clamp(b, 0.0f, 1.0f), a, 1.0f);
                        }
                    }
                }

                const float tranferCurveR = LinearToTransferFunction(r, transferFunction);
                const float tranferCurveG = LinearToTransferFunction(g, transferFunction);
                const float tranferCurveB = LinearToTransferFunction(b, transferFunction);

                yPlane[0] = static_cast<uint16_t>(std::clamp(tranferCurveR * heifImageMaxValue, 0.0f, heifImageMaxValue));
                yPlane[1] = static_cast<uint16_t>(std::clamp(tranferCurveG * heifImageMaxValue, 0.0f, heifImageMaxValue));
                yPlane[2] = static_cast<uint16_t>(std::clamp(tranferCurveB * heifImageMaxValue, 0.0f, heifImageMaxValue));
                yPlane[3] = static_cast<uint16_t>(std::clamp(a * heifImageMaxValue, 0.0f, heifImageMaxValue));

                src += 4;
                yPlane += 4;
            }
            else
            {
                const float tranferCurveR = LinearToTransferFunction(r, transferFunction);
                const float tranferCurveG = LinearToTransferFunction(g, transferFunction);
                const float tranferCurveB = LinearToTransferFunction(b, transferFunction);

                yPlane[0] = static_cast<uint16_t>(std::clamp(tranferCurveR * heifImageMaxValue, 0.0f, heifImageMaxValue));
                yPlane[1] = static_cast<uint16_t>(std::clamp(tranferCurveG * heifImageMaxValue, 0.0f, heifImageMaxValue));
                yPlane[2] = static_cast<uint16_t>(std::clamp(tranferCurveB * heifImageMaxValue, 0.0f, heifImageMaxValue));

                src += 3;
                yPlane += 3;
            }
        }
    }

    return image;
}
