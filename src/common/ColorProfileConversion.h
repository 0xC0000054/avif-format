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

#ifndef COLORPROFILECONVERSION_H
#define COLORPROFILECONVERSION_H

#include "Common.h"
#include "AlphaState.h"
#include "ColorTransfer.h"
#include "ScopedLcms.h"
#include <vector>

class ColorProfileConversion
{
public:

    ColorProfileConversion(
        const FormatRecordPtr formatRecord,
        bool hasAlpha,
        ColorTransferFunction transferFunction,
        bool keepEmbeddedColorProfile);

    ColorProfileConversion(
        const FormatRecordPtr formatRecord,
        bool hasAlpha,
        int hostBitsPerChannel,
        bool keepEmbeddedColorProfile);

    void ConvertRow(void* row, cmsUInt32Number pixelsPerLine, cmsUInt32Number bytesPerLine);

private:

    void ConvertSixteenBitRowToLcms(uint16_t* row, cmsUInt32Number pixelCount);

    void ConvertSixteenBitRowToHost(uint16_t* row, cmsUInt32Number pixelCount);

    void InitializeForRec2020Conversion(bool hasAlpha);

    void InitializeForSRGBConversion(bool hasAlpha, int hostBitsPerChannel);

    ScopedLcmsContext context;
    ScopedLcmsProfile documentProfile;
    ScopedLcmsProfile outputImageProfile;
    ScopedLcmsTransform transform;
    const size_t numberOfChannels;
    const bool isSixteenBitMode;
    std::vector<uint16_t> hostToLcmsLookupTable;
    std::vector<uint16_t> lcmsToHostLookupTable;
};

#endif // !COLORPROFILECONVERSION_H


