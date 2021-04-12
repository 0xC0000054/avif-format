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

#ifndef COMMON_H
#define COMMON_H

#if defined(_MSC_VER)
#pragma warning(push)
// Disable uninitialized variable warnings in the SDK headers.
#pragma warning(disable: 26495)
// Suppress C4121: 'FormatRecord': alignment of a member was sensitive to packing
#pragma warning(disable: 4121)
#endif // _MSC_VER)

#include "PIDefines.h"
#include "PITypes.h"
#include "PIFormat.h"
#include "PIAbout.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// PITypes.h may define a few C++ reserved keywords
#if defined(false)
#undef false
#endif // defined(false)

#if defined(true)
#undef true
#endif // defined(true)

#include "libheif/heif.h"

#if defined(DEBUG) || defined(_DEBUG)
#define DEBUG_BUILD 1
#else
#define DEBUG_BUILD 0
#endif

#if DEBUG_BUILD
void DebugOut(const char* fmt, ...) noexcept;
#else
#define DebugOut(fmt, ...)
#endif // DEBUG_BUILD

#define PrintFunctionName() DebugOut(__FUNCTION__)

#endif // !COMMON_H
