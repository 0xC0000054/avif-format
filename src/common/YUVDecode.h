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

#ifndef YUVDECODE_H
#define YUVDECODE_H

#include "YUVCoefficiants.h"
#include "YUVLookupTables.h"

void DecodeY8RowToGray8(
    const uint8_t* yPlane,
    uint8_t* grayRow,
    int32 rowWidth,
    const YUVLookupTables& tables);

void DecodeY8RowToGrayAlpha8(
    const uint8_t* yPlane,
    const uint8_t* alphaPlane,
    bool alphaPremultiplied,
    uint8_t* grayaRow,
    int32 rowWidth,
    const YUVLookupTables& tables);

void DecodeY16RowToGray16(
    const uint16_t* yPlane,
    uint16_t* grayRow,
    int32 rowWidth,
    const YUVLookupTables& tables);

void DecodeY16RowToGrayAlpha16(
    const uint16_t* yPlane,
	const uint16_t* alphaPlane,
    bool alphaPremultiplied,
    uint16_t* grayaRow,
    int32 rowWidth,
    const YUVLookupTables& tables);

void DecodeYUV8RowToRGB8(
    const uint8_t* yPlane,
    const uint8_t* uPlane,
    const uint8_t* vPlane,
    uint8_t* rgbRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables);

void DecodeYUV8RowToRGBA8(
    const uint8_t* yPlane,
    const uint8_t* uPlane,
    const uint8_t* vPlane,
    const uint8_t* alphaPlane,
    bool alphaPremultiplied,
    uint8_t* rgbaRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables);

void DecodeYUV16RowToRGB16(
    const uint16_t* yPlane,
    const uint16_t* uPlane,
    const uint16_t* vPlane,
    uint16_t* rgbRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables);

void DecodeYUV16RowToRGBA16(
    const uint16_t* yPlane,
    const uint16_t* uPlane,
    const uint16_t* vPlane,
    const uint16_t* alphaRow,
    bool alphaPremultiplied,
    uint16_t* rgbaRow,
    int32 rowWidth,
    int32 xChromaShift,
    const YUVCoefficiants& yuvCoefficiants,
    const YUVLookupTables& tables);

#endif // !YUVDECODE_H
