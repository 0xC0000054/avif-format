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

    ScopedHandleSuiteLock(ScopedHandleSuiteLock&& other) noexcept
        : handleProcs(other.handleProcs), handle(other.handle),  ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    ScopedHandleSuiteLock& operator=(ScopedHandleSuiteLock&& other) noexcept
    {
        handleProcs = other.handleProcs;
        handle = other.handle;
        ptr = other.ptr;

        other.ptr = nullptr;

        return *this;
    }

    ScopedHandleSuiteLock(const ScopedHandleSuiteLock&) = delete;
    ScopedHandleSuiteLock& operator=(const ScopedHandleSuiteLock&) = delete;

    ~ScopedHandleSuiteLock()
    {
        unlock();
    }

    Ptr data() const
    {
        if (ptr == nullptr)
        {
            throw std::runtime_error("The locked data pointer is invalid.");
        }

        return ptr;
    }

    void unlock()
    {
        if (ptr != nullptr)
        {
            handleProcs->unlockProc(handle);
            ptr = nullptr;
        }
    }

private:
    const HandleProcs* handleProcs;
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

    ScopedHandleSuiteHandle(const HandleProcs* handleProcs, int32 size)
        : handleProcs(handleProcs), handle(handleProcs->newProc(size))
    {
        if (handle == nullptr)
        {
            // Failed to allocate the handle, this is treated as an out of memory error.
            throw std::bad_alloc();
        }
    }

    ScopedHandleSuiteHandle(ScopedHandleSuiteHandle&& other) noexcept
        : handleProcs(other.handleProcs), handle(other.handle)
    {
        other.handle = nullptr;
    }

    ScopedHandleSuiteHandle& operator=(ScopedHandleSuiteHandle&& other) noexcept
    {
        handleProcs = other.handleProcs;
        handle = other.handle;

        other.handle = nullptr;

        return *this;
    }

    ScopedHandleSuiteHandle(const ScopedHandleSuiteHandle&) = delete;
    ScopedHandleSuiteHandle& operator=(const ScopedHandleSuiteHandle&) = delete;

    ~ScopedHandleSuiteHandle()
    {
        if (handle != nullptr)
        {
            handleProcs->disposeProc(handle);
            handle = nullptr;
        }
    }

    Handle get() const noexcept
    {
        return handle;
    }

    int32 size() const
    {
        int32 allocatedSize = 0;

        if (handle != nullptr)
        {
            allocatedSize = handleProcs->getSizeProc(handle);
        }

        return allocatedSize;
    }

    ScopedHandleSuiteLock lock() const
    {
        if (handle == nullptr)
        {
            throw std::runtime_error("Cannot Lock an invalid handle.");
        }

        return ScopedHandleSuiteLock(handleProcs, handle);
    }

    // Returns the underlying handle and releases the ownership.
    Handle release()
    {
        Handle existing = handle;
        handle = nullptr;

        return existing;
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
    const HandleProcs* handleProcs;
    Handle handle;
};

#endif // !SCOPEDHANDLESUITE_H
