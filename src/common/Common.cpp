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

#include "Common.h"
#include <stdio.h>
#include <stdarg.h>

#if DEBUG_BUILD
void DebugOut(const char* fmt, ...) noexcept
{
#if __PIWin__
    va_list argp;
    char dbg_out[4096] = {};

    va_start(argp, fmt);
    vsprintf_s(dbg_out, fmt, argp);
    va_end(argp);

    OutputDebugStringA(dbg_out);
    OutputDebugStringA("\n");
#else
#error "Debug output has not been configured for this platform."
#endif // __PIWin__
}
#endif // DEBUG_BUILD
