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

#ifndef AVIFFORMATTERMINOLOGY_H
#define AVIFFORMATTERMINOLOGY_H

#include "PITerminology.h"
#include "PIActions.h"

#ifndef NULLID
#define NULLID 0
#endif

// Dictionary (aete) resources:

#define vendorName			"0xC0000054"
#define plugInAETEComment "AV1 Image format module"

#define plugInSuiteID		'av1F'
#define plugInClassID		plugInSuiteID
#define plugInEventID		typeNull // must be this

// keyQuality is defined in PITerminology.h
#define keyCompressionSpeed 'av1S'
#define keyLosslessCompression 'av1L'
#define keyChromaSubsampling 'av1C'
#define keyKeepColorProfile 'kpmC'
#define keyKeepEXIF 'kpmE'
#define keyKeepXMP 'kpmX'
#define keyLosslessAlpha 'losA'
#define keyPremultipliedAlpha 'pmAl'
#define keyImageBitDepth 'av1B'

#define typeCompressionSpeed 'coSp'

#define compressionSpeedFastest 'csP0'
#define compressionSpeedDefault 'csP1'
#define compressionSpeedSlowest 'csP2'

#define typeChromaSubsampling 'chSu'

#define chromaSubsampling420 'chS0'
#define chromaSubsampling422 'chS1'
#define chromaSubsampling444 'chS2'

#define typeImageBitDepth 'imBd'

#define imageBitDepthEight 'iBd0'
#define imageBitDepthTen 'iBd1'
#define imageBitDepthTwelve 'iBd2'

#endif
