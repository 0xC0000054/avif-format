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

#ifndef COLORPROFILEGENERATION_H
#define COLORPROFILEGENERATION_H

#include "Common.h"
#include "ScopedLcms.h"

ScopedLcmsProfile CreateRec2020LinearRGBProfile(cmsContext context);

void SetIccProfileFromNclx(FormatRecord* formatRecord, const heif_color_profile_nclx* nclx);

#endif // !COLORPROFILEGENERATION_H
