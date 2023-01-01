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

#include "MemoryWin.h"
#include <Windows.h>
#include <windowsx.h>

// The following methods were adapted from WinUtilities.cpp in the PS6 SDK:

#define signatureSize	4
static char cSig[signatureSize] = { 'O', 'T', 'O', 'F' };

#pragma warning(disable: 6387)
#pragma warning(disable: 28183)

Handle NewHandle(int32 size)
{
    Handle mHand;
    char* p;

    mHand = (Handle)GlobalAllocPtr(GHND, (sizeof(Handle) + signatureSize));

    if (mHand)
        *mHand = (Ptr)GlobalAllocPtr(GHND, size);

    if (!mHand || !(*mHand))
    {
        return NULL;
    }

    // put the signature after the pointer
    p = (char*)mHand;
    p += sizeof(Handle);
    memcpy(p, cSig, signatureSize);

    return mHand;

}

void DisposeHandle(Handle handle)
{
    if (handle)
    {
        Ptr p;

        p = *handle;

        if (p)
            GlobalFreePtr(p);

        GlobalFreePtr((Ptr)handle);
    }
}

#pragma warning(default: 6387)
#pragma warning(default: 28183)
