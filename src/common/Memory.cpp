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

#if __PIWin__
#include "MemoryWin.h"
#else
#include "LowMem.h"
#endif // __PIWin__

// The following methods were adapted from the Host*Handle methods in the PS6 SDK:

OSErr NewPIHandle(const FormatRecordPtr formatRecord, int32 size, Handle* handle)
{
    if (handle == nullptr)
    {
        return nilHandleErr;
    }

    if (HandleSuiteIsAvailable(formatRecord))
    {
        *handle = formatRecord->handleProcs->newProc(size);
    }
    else
    {
        *handle = NewHandle(size);
    }

    return *handle != nullptr ? noErr : memFullErr;
}

void DisposePIHandle(const FormatRecordPtr formatRecord, Handle handle)
{
    if (HandleSuiteIsAvailable(formatRecord))
    {
        formatRecord->handleProcs->disposeProc(handle);
    }
    else
    {
        DisposeHandle(handle);
    }
}

Ptr LockPIHandle(const FormatRecordPtr formatRecord, Handle handle, Boolean moveHigh)
{
    if (HandleSuiteIsAvailable(formatRecord))
    {
        return formatRecord->handleProcs->lockProc(handle, moveHigh);
    }
    else
    {
        // Use OS routines:

#ifdef __PIMac__
        if (moveHigh)
            MoveHHi(handle);
        HLock(handle);

        return *handle; // dereference and return pointer

#else // Windows

        return (Ptr)GlobalLock(handle);

#endif
    }
}

void UnlockPIHandle(const FormatRecordPtr formatRecord, Handle handle)
{
    if (HandleSuiteIsAvailable(formatRecord))
    {
        formatRecord->handleProcs->unlockProc(handle);
    }
    else
    {
        // Use OS routines:
#ifdef __PIMac__

        HUnlock(handle);

#else // Windows

        GlobalUnlock(handle);

#endif
    }
}

