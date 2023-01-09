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

#ifndef COLORTRANSFER_H
#define COLORTRANSFER_H

#include <algorithm>
#include <math.h>
#include <libheif/heif.h>

enum class ColorTransferFunction
{
    PQ,
    HLG,
    SMPTE428,
    Clip
};

struct HLGLumaCoefficiants
{
    float red;
    float green;
    float blue;
};

HLGLumaCoefficiants GetHLGLumaCoefficients(heif_color_primaries primaries);

ColorTransferFunction GetTransferFunctionFromNclx(heif_transfer_characteristics transferCharacteristics);

float LinearToPQ(float value, float imageMaxLuminanceLevel);

float PQToLinear(float value, float imageMaxLuminanceLevel);

float LinearToSMPTE428(float value);

float SMPTE428ToLinear(float value);

float LinearToHLG(float value);

float HLGToLinear(float value);

void ApplyHLGOOTF(
    float* rgb,
    const HLGLumaCoefficiants& lumaCoefficiants,
    float displayGamma,
    float nominalPeakBrightness);

void ApplyInverseHLGOOTF(
    float* rgb,
    const HLGLumaCoefficiants& lumaCoefficiants,
    float displayGamma,
    float nominalPeakBrightness);

#endif // !COLORTRANSFER_H
