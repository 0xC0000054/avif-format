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

#ifndef SCOPEDHANDLESUITE_H
#define SCOPEDHANDLESUITE_H

#include "AvifFormat.h"
#include <stdexcept>

class ScopedHandleSuiteLock
{
public:
    ScopedHandleSuiteLock(const HandleProcs* handleProcs, Handle handle)
        : handleProcs(handleProcs), handle(handle),
          ptr(handleProcs->lockProc(handle, false))
    {
    }

    ~ScopedHandleSuiteLock()
    {
        if (ptr != nullptr)
        {
            handleProcs->unlockProc(handle);
            ptr = nullptr;
        }
    }

    Ptr Data() const noexcept
    {
        return ptr;
    }

private:
    const HandleProcs* const handleProcs;
    Handle handle;
    Ptr ptr;
};

class ScopedHandleSuiteHandle
{
public:
    ScopedHandleSuiteHandle(const HandleProcs* handleProcs, Handle handle) noexcept
        : handleProcs(handleProcs), handle(handle)
    {
    }

    ~ScopedHandleSuiteHandle()
    {
        if (handle != nullptr)
        {
            handleProcs->disposeProc(handle);
            handle = nullptr;
        }
    }

    int32 GetSize() const
    {
        int32 size = 0;

        if (handle != nullptr)
        {
            size = handleProcs->getSizeProc(handle);
        }

        return size;
    }

    ScopedHandleSuiteLock Lock() const
    {
        if (handle == nullptr)
        {
            throw std::runtime_error("Cannot Lock an invalid handle.");
        }

        return ScopedHandleSuiteLock(handleProcs, handle);
    }

    bool operator==(std::nullptr_t) const noexcept
    {
        return handle == nullptr;
    }

    bool operator!=(std::nullptr_t) const noexcept
    {
        return handle != nullptr;
    }

    explicit operator bool() const noexcept
    {
        return handle != nullptr;
    }

private:
    const HandleProcs* const handleProcs;
    Handle handle;
};

#endif // !SCOPEDHANDLESUITE_H
