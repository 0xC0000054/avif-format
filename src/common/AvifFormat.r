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

//-------------------------------------------------------------------------------
//	Definitions -- Required by include files.
//-------------------------------------------------------------------------------

// The About box and resources are created in PIUtilities.r.
// You can easily override them, if you like.

#define plugInName			"AV1 Image"


//-------------------------------------------------------------------------------
//	Set up included files for Macintosh and Windows.
//-------------------------------------------------------------------------------

#include "PIDefines.h"

#if __PIMac__
    #include "Types.r"
    #include "SysTypes.r"
    #include "PIGeneral.r"
    #include "PIUtilities.r"
#elif defined(__PIWin__)
    #define Rez
    #include "PIGeneral.h"
    #include "PIUtilities.r"
#endif

#include "AvifFormatTerminology.h"

//-------------------------------------------------------------------------------
//	PiPL resource
//-------------------------------------------------------------------------------

resource 'PiPL' (ResourceID, plugInName " PiPL", purgeable)
{
    {
        Kind { ImageFormat },
        Name { plugInName },
        Version { (latestFormatVersion << 16) | latestFormatSubVersion },

        #ifdef __PIMac__
            #if defined(__arm64__)
                CodeMacARM64 { "PluginMain" },
            #elif defined(__x86_64__)
                CodeMacIntel64 { "PluginMain" },
            #elif defined(__i386__)
                CodeMacIntel32 { "PluginMain" },
            #endif
        #else
            #if defined(_M_ARM64)
                CodeWin64ARM { "PluginMain" },
            #elif defined(_M_AMD64)
                CodeWin64X86 { "PluginMain" },
            #else
                CodeWin32X86 { "PluginMain" },
            #endif
        #endif

        HasTerminology { plugInClassID,
                         plugInEventID,
                         ResourceID,
                         vendorName " " plugInName },

        SupportedModes
        {
            noBitmap,
            doesSupportGrayScale,
            noIndexedColor,
            doesSupportRGBColor,
            noCMYKColor,
            noHSLColor,
            noHSBColor,
            noMultichannel,
            noDuotone,
            noLABColor
        },

        EnableInfo { "in (PSHOP_ImageMode, RGBMode, RGB48Mode, GrayScaleMode, Gray16Mode, RGB96Mode, Gray32Mode)" },

        PlugInMaxSize { 1073741824, 1073741824 },

        FormatMaxSize { { 32767, 32767 } },

        FormatMaxChannels { { 0, 2, 0, 4, 0, 0, 0, 0, 0, 0, 2, 4 } },

        FmtFileType { 'AV1F', '8BIM' },
        //ReadTypes { { '8B1F', '    ' } },
        //FilteredTypes { { '8B1F', '    ' } },
        ReadExtensions { { 'AVIF' } },
        WriteExtensions { { 'AVIF' } },
        //FilteredExtensions { { 'AVIF' } },
        FormatFlags { fmtDoesNotSaveImageResources,
                      fmtCanRead,
                      fmtCanWrite,
                      fmtCanWriteIfRead,
                      fmtCanWriteTransparency,
                      fmtCannotCreateThumbnail }
        }
    };
}

resource 'aete' (ResourceID, plugInName " dictionary", purgeable)
{
    1, 0, english, roman,									/* aete version and language specifiers */
    {
        vendorName,											/* vendor suite name */
        "AVIF format plug-in",							/* optional description */
        plugInSuiteID,										/* suite ID */
        1,													/* suite code, must be 1 */
        1,													/* suite level, must be 1 */
        {},													/* structure for filters */
        {													/* non-filter plug-in class here */
            vendorName " avifFormat",						/* unique class name */
            plugInClassID,									/* class ID, must be unique or Suite ID */
            plugInAETEComment,								/* optional description */
            {												/* define inheritance */
                "<Inheritance>",							/* must be exactly this */
                keyInherits,								/* must be keyInherits */
                classFormat,								/* parent: Format, Import, Export */
                "parent class format",						/* optional description */
                flagsSingleProperty,						/* if properties, list below */

                /* Load dialog parameters */

                "apply HLG OOTF",
                keyApplyHLGOOTF,
                typeBoolean,
                "",
                flagsSingleProperty,

                "display gamma",
                keyDisplayGamma,
                typeFloat,                            /* This should be a float32, but the PS scripting API only supports float64 */
                "",
                flagsSingleProperty,

                "nominal peak brightness",
                keyNominalPeakBrightness,
                typeInteger,
                "",
                flagsSingleProperty,

                /* Save dialog parameters */

                "quality",
                keyQuality,
                typeInteger,
                "",
                flagsSingleProperty,

                "compression speed",
                keyCompressionSpeed,
                typeCompressionSpeed,
                "",
                flagsEnumeratedParameter,

                "lossless compression",
                keyLosslessCompression,
                typeBoolean,
                "",
                flagsSingleProperty,

                "chroma sub-sampling",
                keyChromaSubsampling,
                typeChromaSubsampling,
                "",
                flagsEnumeratedParameter,

                 "color profile",
                keyKeepColorProfile,
                typeBoolean,
                "",
                flagsSingleProperty,

                "EXIF",
                keyKeepEXIF,
                typeBoolean,
                "",
                flagsSingleProperty,

                "XMP",
                keyKeepXMP,
                typeBoolean,
                "",
                flagsSingleProperty,

                 "lossless alpha",
                keyLosslessAlpha,
                typeBoolean,
                "",
                flagsSingleProperty,

                "premultiplied alpha",
                keyPremultipliedAlpha,
                typeBoolean,
                "",
                flagsSingleProperty,

                "image depth",
                keyImageBitDepth,
                typeImageBitDepth,
                "",
                flagsEnumeratedParameter,

                "HDR transfer function",
                keyHDRTransferFunction,
                typeHDRTransferFunction,
                "",
                flagsEnumeratedParameter,
            },
            {}, /* elements (not supported) */
            /* class descriptions */
        },
        {}, /* comparison ops (not supported) */
        {   /* enumerations */

            typeCompressionSpeed,
            {
                "fastest",
                compressionSpeedFastest,
                "",

                "default",
                compressionSpeedDefault,
                "",

                "slowest",
                compressionSpeedSlowest,
                ""
            },
            typeChromaSubsampling,
            {
                "4:2:0",
                chromaSubsampling420,
                "YUV 4:2:0 (best compression)",

                "4:2:2",
                chromaSubsampling422,
                "YUV 4:2:2",

                "4:4:4",
                chromaSubsampling444,
                "YUV 4:4:4 (best quality)"
            },
            typeImageBitDepth,
            {
                "8-bit",
                imageBitDepthEight,
                "",

                "10-bit",
                imageBitDepthTen,
                "",

                "12-bit",
                imageBitDepthTwelve,
                ""
            },
            typeHDRTransferFunction,
            {
                "PQ",
                hdrTransferFunctionPQ,
                "Rec. 2100 PQ",
                /* Reserved for future use
                "HLG",
                hdrTransferFunctionHLG,
                "Rec. 2100 HLG",
                */
                "SMPTE-428",
                hdrTransferFunctionSMPTE428,
                "SMPTE 428-1",
                "Clip",
                hdrTransferFunctionClip,
                "None, clip",
            }
        }
    }
};
