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

#ifndef READHEIFIMAGE_H
#define READHEIFIMAGE_H

#include "AvifFormat.h"
#include "AlphaState.h"

void ReadHeifImageGrayEightBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord);

void ReadHeifImageRGBEightBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord);

void ReadHeifImageGraySixteenBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord);

void ReadHeifImageRGBSixteenBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord);

void ReadHeifImageGrayThirtyTwoBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    FormatRecordPtr formatRecord);

void ReadHeifImageRGBThirtyTwoBit(
    const heif_image* image,
    AlphaState alphaState,
    const heif_color_profile_nclx* nclxProfile,
    const LoadUIOptions& loadOptions,
    FormatRecordPtr formatRecord);

#endif // !READHEIFIMAGE_H
