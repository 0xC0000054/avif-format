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

#ifndef SCOPEDBUFFERSUITE_H
#define SCOPEDBUFFERSUITE_H

#include "Common.h"
#include "OSErrException.h"
#include <stdexcept>

class ScopedBufferSuiteBuffer
{
public:
    explicit ScopedBufferSuiteBuffer()
        : bufferID(), bufferProcs(nullptr), bufferIDValid(false),
          bufferDataPtr(nullptr), allocatedSize(0)
    {
    }

    explicit ScopedBufferSuiteBuffer(BufferProcs* bufferProcs, int32 bufferSize)
        : bufferID(), bufferProcs(bufferProcs), bufferIDValid(false),
          bufferDataPtr(nullptr), allocatedSize(bufferSize)
    {
        OSErrException::ThrowIfError(bufferProcs->allocateProc(bufferSize, &bufferID));
        bufferIDValid = true;
    }

    ScopedBufferSuiteBuffer(ScopedBufferSuiteBuffer&& other) noexcept
        : bufferID(other.bufferID), bufferProcs(other.bufferProcs), bufferIDValid(other.bufferIDValid),
          bufferDataPtr(other.bufferDataPtr), allocatedSize(other.allocatedSize)
    {
        other.bufferDataPtr = nullptr;
        other.bufferIDValid = false;
    }

    ScopedBufferSuiteBuffer& operator=(ScopedBufferSuiteBuffer&& other) noexcept
    {
        bufferID = other.bufferID;
        bufferProcs = other.bufferProcs;
        bufferIDValid = other.bufferIDValid;
        bufferDataPtr = other.bufferDataPtr;
        allocatedSize = other.allocatedSize;

        other.bufferDataPtr = nullptr;
        other.bufferIDValid = false;

        return *this;
    }

    ScopedBufferSuiteBuffer(const ScopedBufferSuiteBuffer&) = delete;
    ScopedBufferSuiteBuffer& operator=(const ScopedBufferSuiteBuffer&) = delete;

    ~ScopedBufferSuiteBuffer()
    {
        reset();
    }

    int32 size() const noexcept
    {
        return allocatedSize;
    }

    void* lock()
    {
        if (!bufferIDValid)
        {
            throw std::runtime_error("Cannot Lock an invalid buffer.");
        }

        if (bufferDataPtr == nullptr)
        {
            bufferDataPtr = bufferProcs->lockProc(bufferID, false);

            if (bufferDataPtr == nullptr)
            {
                throw std::runtime_error("Unable to lock the BufferSuite buffer.");
            }
        }

        return bufferDataPtr;
    }

    bool operator==(std::nullptr_t) const noexcept
    {
        return !bufferIDValid;
    }

    bool operator!=(std::nullptr_t) const noexcept
    {
        return bufferIDValid;
    }

    explicit operator bool() const noexcept
    {
        return bufferIDValid;
    }

private:

    void reset() noexcept
    {
        if (bufferIDValid)
        {
            bufferIDValid = false;
            allocatedSize = 0;
            if (bufferDataPtr != nullptr)
            {
                bufferProcs->unlockProc(bufferID);
                bufferDataPtr = nullptr;
            }
            bufferProcs->freeProc(bufferID);
        }
    }

    const BufferProcs* bufferProcs;
    BufferID bufferID;
    void* bufferDataPtr;
    int32 allocatedSize;
    bool bufferIDValid;
};

#endif
