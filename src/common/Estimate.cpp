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

#include "AvifFormat.h"
#include <algorithm>

namespace
{
    int32 EstimateUncompressedSize(const FormatRecordPtr formatRecord)
    {
        VPoint imageSize = GetImageSize(formatRecord);

        const unsigned64 width = static_cast<unsigned64>(imageSize.h);
        const unsigned64 height = static_cast<unsigned64>(imageSize.v);

        unsigned64 imageDataSize = width * height * static_cast<unsigned64>(formatRecord->planes);

        if (formatRecord->depth == 16 || formatRecord->depth == 32)
        {
            imageDataSize *= 2; // 2 bytes per pixel.
        }

        // Assume that the AVIF format overhead is 512 bytes.

        unsigned64 totalSize = 512 + imageDataSize;

        return static_cast<int32>(std::min(totalSize, static_cast<unsigned64>(std::numeric_limits<int32>::max())));
    }
}

OSErr DoEstimatePrepare(FormatRecordPtr formatRecord)
{
    PrintFunctionName();

    OSErr err = noErr;

    if (!HostSupportsRequiredFeatures(formatRecord))
    {
        err = errPlugInHostInsufficient;
    }
    else if (!HostImageModeSupported(formatRecord))
    {
        err = formatBadParameters;
    }

    if (err == noErr)
    {
        formatRecord->maxData = 0;
    }

    return err;
}

OSErr DoEstimateStart(FormatRecordPtr formatRecord)
{
    PrintFunctionName();

    const int32 uncompressedSize = EstimateUncompressedSize(formatRecord);

    formatRecord->minDataBytes = uncompressedSize / 2;
    formatRecord->maxDataBytes = uncompressedSize;
    formatRecord->data = nullptr;

    return noErr;
}

OSErr DoEstimateContinue()
{
    PrintFunctionName();

    return noErr;
}

OSErr DoEstimateFinish()
{
    PrintFunctionName();

    return noErr;
}
