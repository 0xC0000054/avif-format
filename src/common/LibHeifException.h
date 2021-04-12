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

#ifndef LIBHEIFEXCEPTION_H
#define LIBHEIFEXCEPTION_H

#include <libheif/heif.h>
#include <stdexcept>

class LibHeifException : public std::runtime_error
{
public:

    LibHeifException(const heif_error& e) : std::runtime_error(e.message), code(e.code), subCode(e.subcode)
    {
    }

    heif_error_code GetErrorCode() const
    {
        return code;
    }

    heif_suberror_code GetSubCode() const
    {
        return subCode;
    }

    static bool IsOutOfMemoryError(const heif_error& e)
    {
        return e.code == heif_error_Memory_allocation_error && e.subcode == heif_suberror_Unspecified;
    }

    static void ThrowIfError(const heif_error& e)
    {
        if (e.code != heif_error_Ok)
        {
            if (IsOutOfMemoryError(e))
            {
                throw std::bad_alloc();
            }
            else
            {
                throw LibHeifException(e);
            }
        }
    }

private:

    heif_error_code code;
    heif_suberror_code subCode;
};

#endif
