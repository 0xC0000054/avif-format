/*
 * This file is part of avif-format, an AV1 Image (AVIF) file format
 * plug-in for Adobe Photoshop(R).
 *
 * Copyright (c) 2021 Nicholas Hayes
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

VPoint GetImageSize(const FormatRecordPtr formatRecord)
{
    VPoint imageSize;

    if (formatRecord->HostSupports32BitCoordinates && formatRecord->PluginUsing32BitCoordinates)
    {
        imageSize.h = formatRecord->imageSize32.h;
        imageSize.v = formatRecord->imageSize32.v;
    }
    else
    {
        imageSize.h = formatRecord->imageSize.h;
        imageSize.v = formatRecord->imageSize.v;
    }

    return imageSize;
}

void SetRect(FormatRecordPtr formatRecord, int32 top, int32 left, int32 bottom, int32 right)
{
    if (formatRecord->HostSupports32BitCoordinates && formatRecord->PluginUsing32BitCoordinates)
    {
        formatRecord->theRect32.top = top;
        formatRecord->theRect32.left = left;
        formatRecord->theRect32.bottom = bottom;
        formatRecord->theRect32.right = right;
    }
    else
    {
        formatRecord->theRect.top = static_cast<int16>(top);
        formatRecord->theRect.left = static_cast<int16>(left);
        formatRecord->theRect.bottom = static_cast<int16>(bottom);
        formatRecord->theRect.right = static_cast<int16>(right);
    }
}
