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

#ifndef PREMULTIPLIEDALPHA_H
#define PREMULTIPLIEDALPHA_H

#include "Common.h"

float PremultiplyColor(float color, float alpha, float maxValue);

uint8_t PremultiplyColor(uint8_t color, uint8_t alpha);

uint16_t PremultiplyColor(uint16_t color, uint16_t alpha, uint16_t maxValue);

float UnpremultiplyColor(float color, float alpha, float maxValue);

uint8_t UnpremultiplyColor(uint8_t color, uint8_t alpha);

uint16_t UnpremultiplyColor(uint16_t color, uint16_t alpha, uint16_t maxValue);

#endif // PREMULTIPLIEDALPHA_H
