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

#include "FileIOWin.h"
#include <algorithm>

OSErr GetFilePositionNative(intptr_t refNum, int64& position)
{
    LARGE_INTEGER distanceToMove;
    distanceToMove.QuadPart = 0;

    LARGE_INTEGER currentFilePosition;

    if (!SetFilePointerEx(reinterpret_cast<HANDLE>(refNum), distanceToMove, &currentFilePosition, FILE_CURRENT))
    {
        return readErr;
    }

    position = currentFilePosition.QuadPart;
    return noErr;
}

OSErr GetFileSizeNative(intptr_t refNum, int64& size)
{
    LARGE_INTEGER currentFileSize;

    if (!GetFileSizeEx(reinterpret_cast<HANDLE>(refNum), &currentFileSize))
    {
        return readErr;
    }

    size = currentFileSize.QuadPart;
    return noErr;
}

OSErr ReadDataNative(intptr_t refNum, void* buffer, size_t size)
{
    size_t totalBytesRead = 0;

    HANDLE hFile = reinterpret_cast<HANDLE>(refNum);

    uint8_t* dataBuffer = static_cast<uint8_t*>(buffer);

    while (totalBytesRead < size)
    {
        DWORD bytesToRead = static_cast<DWORD>(std::min(size - totalBytesRead, static_cast<size_t>(2147483648)));

        DWORD bytesRead;

        if (!ReadFile(hFile, dataBuffer + totalBytesRead, bytesToRead, &bytesRead, nullptr))
        {
            return readErr;
        }

        if (bytesRead == 0)
        {
            return eofErr;
        }

        totalBytesRead += bytesRead;
    }

    return noErr;
}

OSErr SetFilePositionNative(intptr_t refNum, int64 position)
{
    LARGE_INTEGER distanceToMove;
    distanceToMove.QuadPart = position;

    if (!SetFilePointerEx(reinterpret_cast<HANDLE>(refNum), distanceToMove, nullptr, FILE_BEGIN))
    {
        return readErr;
    }

    return noErr;
}

OSErr WriteDataNative(intptr_t refNum, const void* buffer, size_t size)
{
    size_t totalBytesWritten = 0;

    HANDLE hFile = reinterpret_cast<HANDLE>(refNum);

    const uint8_t* dataBuffer = static_cast<const uint8_t*>(buffer);

    while (totalBytesWritten < size)
    {
        DWORD bytesToWrite = static_cast<DWORD>(std::min(size - totalBytesWritten, static_cast<size_t>(2147483648)));

        DWORD bytesWritten;

        if (!WriteFile(hFile, dataBuffer + totalBytesWritten, bytesToWrite, &bytesWritten, nullptr))
        {
            return writErr;
        }

        if (bytesWritten != bytesToWrite)
        {
            return writErr;
        }

        totalBytesWritten += bytesWritten;
    }

    return noErr;
}
