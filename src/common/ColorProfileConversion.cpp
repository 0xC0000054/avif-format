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
}

ColorProfileConversion::ColorProfileConversion(
    const FormatRecordPtr formatRecord,
    AlphaState alphaState,
    ColorTransferFunction transferFunction)
    : context(cmsCreateContext(nullptr, nullptr)), documentProfile(), rec2020Profile(), transform()
{
    if (transferFunction != ColorTransferFunction::Clip && HasColorProfileMetadata(formatRecord))
    {
        documentProfile = ReadDocumentProfile(context.get(), formatRecord);

        if (documentProfile && !IsRec2020ColorProfile(documentProfile.get()))
        {
            rec2020Profile = CreateRec2020LinearRGBProfile(context.get());

            if (!rec2020Profile)
            {
                throw ::std::runtime_error("Unable to create a Rec. 2020 color profile for HDR conversion.");
            }

            cmsUInt32Number format = TYPE_RGB_FLT;
            cmsUInt32Number transformFlags = cmsFLAGS_BLACKPOINTCOMPENSATION;

            if (alphaState != AlphaState::None)
            {
                format = TYPE_RGBA_FLT;
                transformFlags |= cmsFLAGS_COPY_ALPHA;
            }

            transform.reset(cmsCreateTransformTHR(
                context.get(),
                documentProfile.get(),
                format,
                rec2020Profile.get(),
                format,
                INTENT_PERCEPTUAL,
                transformFlags));

            if (!transform)
            {
                throw ::std::runtime_error("Unable to create a color profile transform for HDR conversion.");
            }
        }
    }
}

void ColorProfileConversion::ConvertRow(void* row, cmsUInt32Number pixelsPerLine, cmsUInt32Number bytesPerLine)
{
    if (transform)
    {
        constexpr cmsUInt32Number lineCount = 1;
        constexpr cmsUInt32Number bytesPerPlane = 0; // Unused for interleaved data

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
    }
}
