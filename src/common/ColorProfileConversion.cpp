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

#include "ColorProfileConversion.h"
#include "ColorProfileDetection.h"
#include "ColorProfileGeneration.h"
#include "HostMetadata.h"
#include "ScopedHandleSuite.h"
#include "ScopedLcms.h"

namespace
{
    ScopedLcmsProfile ReadDocumentProfile(cmsContext context, const FormatRecordPtr formatRecord)
    {
        ScopedHandleSuiteLock lock(formatRecord->handleProcs, formatRecord->iCCprofileData);

        return ScopedLcmsProfile(cmsOpenProfileFromMemTHR(context, lock.data(), formatRecord->iCCprofileSize));
    }

    std::vector<uint16_t> BuildHostToLcmsLookup()
    {
        std::vector<uint16_t> lookupTable;
        lookupTable.reserve(32769);

        // Map the image data from Photoshop's 16-bit range [0, 32768] to [0, 65535].
        // Little CMS has a formatter plug-in that adds support for Photoshop's 16-bit range, but we
        // cannot use it because of the GPL license.

        constexpr int maxValue = 65535;
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

            lookupTable.push_back(static_cast<uint16_t>(value));
        }

        return lookupTable;
    }

    std::vector<uint16_t> BuildLcmsToHostLookup()
    {
        std::vector<uint16_t> lookupTable;
        lookupTable.reserve(65536);

        // Map the image data from [0, 65535] to Photoshop's 16-bit range [0, 32768].

        constexpr int maxValue = 32768;
        constexpr float maxValueFloat = static_cast<float>(maxValue);

        for (size_t i = 0; i < lookupTable.capacity(); i++)
        {
            int value = static_cast<int>(((static_cast<float>(i) / 65535.0f) * maxValueFloat) + 0.5f);

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

ColorProfileConversion::ColorProfileConversion(
    const FormatRecordPtr formatRecord,
    bool hasAlpha,
    ColorTransferFunction transferFunction,
    bool keepEmbeddedColorProfile)
    : context(cmsCreateContext(nullptr, nullptr)), documentProfile(), outputImageProfile(), transform(),
      numberOfChannels(hasAlpha ? 4 : 3), isSixteenBitMode(false), hostToLcmsLookupTable(),
      lcmsToHostLookupTable()
{
    bool mayRequireConversion = transferFunction != ColorTransferFunction::Clip || !keepEmbeddedColorProfile;

    if (HasColorProfileMetadata(formatRecord) && mayRequireConversion)
    {
        documentProfile = ReadDocumentProfile(context.get(), formatRecord);

        if (!documentProfile)
        {
            throw ::std::runtime_error("Unable to load the document color profile.");
        }

        if (transferFunction == ColorTransferFunction::Clip)
        {
            // 32-bit documents always need to be converted to sRGB because the 32-bit mode
            // uses linear gamma.
            InitializeForSRGBConversion(hasAlpha, 32);
        }
        else
        {
            if (!IsRec2020ColorProfile(documentProfile.get()))
            {
                InitializeForRec2020Conversion(hasAlpha);
            }
        }
    }
}

ColorProfileConversion::ColorProfileConversion(
    const FormatRecordPtr formatRecord,
    bool hasAlpha,
    int hostBitsPerChannel,
    bool keepEmbeddedColorProfile)
    : context(cmsCreateContext(nullptr, nullptr)), documentProfile(), outputImageProfile(), transform(),
      numberOfChannels(hasAlpha ? 4 : 3), isSixteenBitMode(hostBitsPerChannel == 16), hostToLcmsLookupTable(),
      lcmsToHostLookupTable()
{
    if (HasColorProfileMetadata(formatRecord) && !keepEmbeddedColorProfile)
    {
        documentProfile = ReadDocumentProfile(context.get(), formatRecord);

        if (!documentProfile)
        {
            throw ::std::runtime_error("Unable to load the document color profile.");
        }

        if (!IsSRGBColorProfile(documentProfile.get()))
        {
            InitializeForSRGBConversion(hasAlpha, hostBitsPerChannel);
        }
    }
}

void ColorProfileConversion::ConvertRow(void* row, cmsUInt32Number pixelsPerLine, cmsUInt32Number bytesPerLine)
{
    if (transform)
    {
        constexpr cmsUInt32Number lineCount = 1;
        constexpr cmsUInt32Number bytesPerPlane = 0; // Unused for interleaved data

        if (isSixteenBitMode)
        {
            ConvertSixteenBitRowToLcms(static_cast<uint16_t*>(row), pixelsPerLine);
        }

        cmsDoTransformLineStride(
            transform.get(),
            row,
            row,
            pixelsPerLine,
            lineCount,
            bytesPerLine,
            bytesPerLine,
            bytesPerPlane,
            bytesPerPlane);

        if (isSixteenBitMode)
        {
            ConvertSixteenBitRowToHost(static_cast<uint16_t*>(row), pixelsPerLine);
        }
    }
}

void ColorProfileConversion::ConvertSixteenBitRowToLcms(uint16_t* row, cmsUInt32Number pixelsPerLine)
{
    for (size_t i = 0; i < pixelsPerLine; i++)
    {
        const size_t index = i * numberOfChannels;

        uint16_t r = row[index];
        row[index] = hostToLcmsLookupTable[r];

        uint16_t g = row[index + 1];
        row[index + 1] = hostToLcmsLookupTable[g];

        uint16_t b = row[index + 2];
        row[index + 2] = hostToLcmsLookupTable[b];

        if (numberOfChannels == 4)
        {
            uint16_t a = row[index + 3];
            row[index + 3] = hostToLcmsLookupTable[a];
        }
    }
}

void ColorProfileConversion::ConvertSixteenBitRowToHost(uint16_t* row, cmsUInt32Number pixelsPerLine)
{
    for (size_t i = 0; i < pixelsPerLine; i++)
    {
        const size_t index = i * numberOfChannels;

        uint16_t r = row[index];
        row[index] = lcmsToHostLookupTable[r];

        uint16_t g = row[index + 1];
        row[index + 1] = lcmsToHostLookupTable[g];

        uint16_t b = row[index + 2];
        row[index + 2] = lcmsToHostLookupTable[b];

        if (numberOfChannels == 4)
        {
            uint16_t a = row[index + 3];
            row[index + 3] = lcmsToHostLookupTable[a];
        }
    }
}

void ColorProfileConversion::InitializeForRec2020Conversion(bool hasAlpha)
{
    outputImageProfile = CreateRec2020LinearRGBProfile(context.get());

    if (!outputImageProfile)
    {
        throw ::std::runtime_error("Unable to create a Rec. 2020 color profile for HDR conversion.");
    }

    cmsUInt32Number transformFormat = TYPE_RGB_FLT;
    cmsUInt32Number transformFlags = cmsFLAGS_BLACKPOINTCOMPENSATION;

    if (hasAlpha)
    {
        transformFormat = TYPE_RGBA_FLT;
        transformFlags |= cmsFLAGS_COPY_ALPHA;
    }

    transform.reset(cmsCreateTransformTHR(
        context.get(),
        documentProfile.get(),
        transformFormat,
        outputImageProfile.get(),
        transformFormat,
        INTENT_PERCEPTUAL,
        transformFlags));

    if (!transform)
    {
        throw ::std::runtime_error("Unable to create a color profile transform for HDR conversion.");
    }
}

void ColorProfileConversion::InitializeForSRGBConversion(bool hasAlpha, int hostBitsPerChannel)
{
    outputImageProfile.reset(cmsCreate_sRGBProfileTHR(context.get()));

    if (!outputImageProfile)
    {
        throw ::std::runtime_error("Unable to create a sRGB color profile for the output image.");
    }

    cmsUInt32Number transformFormat;
    cmsUInt32Number transformFlags = cmsFLAGS_BLACKPOINTCOMPENSATION;

    if (hostBitsPerChannel == 8)
    {
        transformFormat = TYPE_RGB_8;

        if (hasAlpha)
        {
            transformFormat = TYPE_RGBA_8;
            transformFlags |= cmsFLAGS_COPY_ALPHA;
        }
    }
    else if (hostBitsPerChannel == 16)
    {
        transformFormat = TYPE_RGB_16;

        if (hasAlpha)
        {
            transformFormat = TYPE_RGBA_16;
            transformFlags |= cmsFLAGS_COPY_ALPHA;
        }

        hostToLcmsLookupTable = BuildHostToLcmsLookup();
        lcmsToHostLookupTable = BuildLcmsToHostLookup();
    }
    else if (hostBitsPerChannel == 32)
    {
        transformFormat = TYPE_RGB_FLT;

        if (hasAlpha)
        {
            transformFormat = TYPE_RGBA_FLT;
            transformFlags |= cmsFLAGS_COPY_ALPHA;
        }
    }
    else
    {
        throw ::std::runtime_error("Unsupported host bit depth, must be 8, 16 or 32.");
    }

    transform.reset(cmsCreateTransformTHR(
        context.get(),
        documentProfile.get(),
        transformFormat,
        outputImageProfile.get(),
        transformFormat,
        INTENT_PERCEPTUAL,
        transformFlags));

    if (!transform)
    {
        throw ::std::runtime_error("Unable to create a color profile transform for the output image conversion.");
    }
}
