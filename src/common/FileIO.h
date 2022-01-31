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

#ifndef FILEIO_H
#define FILEIO_H

#include "Common.h"

OSErr GetFilePosition(intptr_t refNum, int64& position);

OSErr GetFileSize(intptr_t refNum, int64& size);

OSErr ReadData(intptr_t refNum, void* buffer, size_t size);

OSErr SetFilePosition(intptr_t refNum, int64 position);

OSErr WriteData(intptr_t refNum, const void* buffer, size_t size);

#endif // !FILEIO_H
