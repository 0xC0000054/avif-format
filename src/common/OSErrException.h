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

#ifndef OSERREXCEPTION_H
#define OSERREXCEPTION_H

#include "PITypes.h"
#include <stdexcept>

class OSErrException : public std::exception
{
public:
    explicit OSErrException(OSErr err) noexcept : error(err)
    {
    }

    OSErr GetErrorCode() const noexcept
    {
        return error;
    }

    static void ThrowIfError(OSErr err)
    {
        if (err != noErr)
        {
            throw OSErrException(err);
        }
    }

private:
    OSErr error;
};

#endif // !OSERREXCEPTION_H
