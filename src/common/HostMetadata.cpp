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

#include "HostMetadata.h"
#include "PIProperties.h"

namespace
{
    Handle GetComplexProperty(const FormatRecordPtr formatRecord, int32 propertyKey)
    {
        Handle handle = nullptr;

        if (formatRecord->propertyProcs->getPropertyProc(kPhotoshopSignature, propertyKey, 0, nullptr, &handle) != noErr)
        {
            handle = nullptr;
        }

        return handle;
    }
}

ScopedHandleSuiteHandle GetExifMetadata(const FormatRecordPtr formatRecord)
{
    Handle exifHandle = nullptr;

    if (HandleSuiteIsAvailable(formatRecord) && PropertySuiteIsAvailable(formatRecord))
    {
        exifHandle = GetComplexProperty(formatRecord, propEXIFData);
    }

    return ScopedHandleSuiteHandle(formatRecord->handleProcs, exifHandle);
}

ScopedHandleSuiteHandle GetXmpMetadata(const FormatRecordPtr formatRecord)
{
    Handle xmpHandle = nullptr;

    if (HandleSuiteIsAvailable(formatRecord) && PropertySuiteIsAvailable(formatRecord))
    {
        xmpHandle = GetComplexProperty(formatRecord, propXMP);
    }

    return ScopedHandleSuiteHandle(formatRecord->handleProcs, xmpHandle);
}

bool HasColorProfileMetadata(const FormatRecordPtr formatRecord)
{
    return (HandleSuiteIsAvailable(formatRecord) &&
            formatRecord->canUseICCProfiles &&
            formatRecord->iCCprofileData != nullptr &&
            formatRecord->iCCprofileSize > 0);
}

bool HasExifMetadata(const FormatRecordPtr formatRecord)
{
    bool result = false;

    ScopedHandleSuiteHandle exif = GetExifMetadata(formatRecord);

    if (exif != nullptr)
    {
        result = exif.size() > 0;
    }

    return result;
}

bool HasXmpMetadata(const FormatRecordPtr formatRecord)
{
    bool result = false;

    ScopedHandleSuiteHandle xmp = GetXmpMetadata(formatRecord);

    if (xmp != nullptr)
    {
        result = xmp.size() > 0;
    }

    return result;
}
