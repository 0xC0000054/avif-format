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

#include "AvifFormat.h"
#include "AvifFormatTerminology.h"

namespace
{
    ChromaSubsampling ChromaSubsamplingFromDescriptor(DescriptorEnumID value)
    {
        switch (value)
        {
        case chromaSubsampling422:
            return ChromaSubsampling::Yuv422;
        case chromaSubsampling444:
            return ChromaSubsampling::Yuv444;
        case chromaSubsampling420:
        default:
            return ChromaSubsampling::Yuv420;
        }
    }

    DescriptorEnumID ChromaSubsamplingToDescriptor(ChromaSubsampling value)
    {
        switch (value)
        {
        case ChromaSubsampling::Yuv422:
            return chromaSubsampling422;
        case ChromaSubsampling::Yuv444:
            return chromaSubsampling444;
        case ChromaSubsampling::Yuv420:
        default:
            return chromaSubsampling420;
        }
    }

    CompressionSpeed CompressionSpeedFromDescriptor(DescriptorEnumID value)
    {
        switch (value)
        {
        case compressionSpeedFastest:
            return CompressionSpeed::Fastest;
        case compressionSpeedSlowest:
            return CompressionSpeed::Slowest;
        case compressionSpeedDefault:
        default:
            return CompressionSpeed::Default;
        }
    }

    DescriptorEnumID CompressionSpeedToDescriptor(CompressionSpeed value)
    {
        switch (value)
        {
        case CompressionSpeed::Fastest:
            return compressionSpeedFastest;
        case CompressionSpeed::Slowest:
            return compressionSpeedSlowest;
        case CompressionSpeed::Default:
        default:
            return compressionSpeedDefault;
        }
    }

    ImageBitDepth ImageBitDepthFromDescriptor(DescriptorEnumID value)
    {
        switch (value)
        {
        case imageBitDepthEight:
            return ImageBitDepth::Eight;
        case imageBitDepthTen:
            return ImageBitDepth::Ten;
        case imageBitDepthTwelve:
        default:
            return ImageBitDepth::Twelve;
        }
    }

    DescriptorEnumID ImageBitDepthToDescriptor(ImageBitDepth value)
    {
        switch (value)
        {
        case ImageBitDepth::Eight:
            return imageBitDepthEight;
        case ImageBitDepth::Ten:
            return imageBitDepthTen;
        case ImageBitDepth::Twelve:
        default:
            return imageBitDepthTwelve;
        }
    }
}

OSErr ReadScriptParamsOnWrite(FormatRecordPtr formatRecord, SaveUIOptions& options, Boolean* showDialog)
{
    OSErr error = noErr;

    if (showDialog)
    {
        *showDialog = true;
    }

    if (DescriptorSuiteIsAvailable(formatRecord))
    {
        DescriptorKeyID				key = 0;
        DescriptorTypeID			type = 0;
        int32						flags = 0;
        DescriptorKeyIDArray		array =
        {
            keyQuality,
            keyCompressionSpeed,
            keyLosslessCompression,
            keyChromaSubsampling,
            keyKeepColorProfile,
            keyKeepEXIF,
            keyKeepXMP,
            keyImageBitDepth,
            NULLID
        };

        ReadDescriptorProcs* readProcs = formatRecord->descriptorParameters->readDescriptorProcs;

        PIReadDescriptor token = readProcs->openReadDescriptorProc(formatRecord->descriptorParameters->descriptor, array);
        if (token != nullptr)
        {
            DescriptorEnumID enumValue;
            Boolean boolValue;
            int32 intValue;

            while (readProcs->getKeyProc(token, &key, &type, &flags))
            {
                switch (key)
                {
                case keyQuality:
                    if (readProcs->getIntegerProc(token, &intValue) == noErr)
                    {
                        options.quality = intValue;
                    }
                    break;
                case keyCompressionSpeed:
                    if (readProcs->getEnumeratedProc(token, &enumValue) == noErr)
                    {
                        options.compressionSpeed = CompressionSpeedFromDescriptor(enumValue);
                    }
                    break;
                case keyLosslessCompression:
                    if (readProcs->getBooleanProc(token, &boolValue) == noErr)
                    {
                        options.lossless = boolValue;
                    }
                    break;
                case keyChromaSubsampling:
                    if (readProcs->getEnumeratedProc(token, &enumValue) == noErr)
                    {
                        options.chromaSubsampling = ChromaSubsamplingFromDescriptor(enumValue);
                    }
                    break;
                case keyKeepColorProfile:
                    if (readProcs->getBooleanProc(token, &boolValue) == noErr)
                    {
                        options.keepColorProfile = boolValue;
                    }
                    break;
                case keyKeepEXIF:
                    if (readProcs->getBooleanProc(token, &boolValue) == noErr)
                    {
                        options.keepExif = boolValue;
                    }
                    break;
                case keyKeepXMP:
                    if (readProcs->getBooleanProc(token, &boolValue) == noErr)
                    {
                        options.keepXmp = boolValue;
                    }
                    break;
                case keyPremultipliedAlpha:
                    if (readProcs->getBooleanProc(token, &boolValue) == noErr)
                    {
                        options.premultipliedAlpha = boolValue;
                    }
                    break;
                case typeImageBitDepth:
                    if (readProcs->getEnumeratedProc(token, &enumValue) == noErr)
                    {
                        options.imageBitDepth = ImageBitDepthFromDescriptor(enumValue);
                    }
                    break;
                }
            }

            error = readProcs->closeReadDescriptorProc(token); // closes & disposes.

            // Dispose the parameter block descriptor:
            formatRecord->handleProcs->disposeProc(formatRecord->descriptorParameters->descriptor);
            formatRecord->descriptorParameters->descriptor = nullptr;

            if (showDialog != nullptr)
            {
                *showDialog = formatRecord->descriptorParameters->playInfo == plugInDialogDisplay;
            }
        }
    }

    return error;
}

OSErr WriteScriptParamsOnWrite(FormatRecordPtr formatRecord, const SaveUIOptions& options)
{
    OSErr error = noErr;

    if (DescriptorSuiteIsAvailable(formatRecord))
    {
        WriteDescriptorProcs* writeProcs = formatRecord->descriptorParameters->writeDescriptorProcs;

        PIWriteDescriptor token = writeProcs->openWriteDescriptorProc();
        if (token != nullptr)
        {
            DescriptorEnumID enumValue;

            if (options.lossless)
            {
                writeProcs->putBooleanProc(token, keyLosslessCompression, options.lossless);
            }
            else
            {
                // Lossless compression overrides the quality and chroma sub-sampling settings.
                writeProcs->putIntegerProc(token, keyQuality, options.quality);

                if (options.chromaSubsampling != ChromaSubsampling::Yuv422)
                {
                    enumValue = ChromaSubsamplingToDescriptor(options.chromaSubsampling);
                    writeProcs->putEnumeratedProc(token, keyChromaSubsampling, typeChromaSubsampling, enumValue);
                }
            }

            if (options.compressionSpeed != CompressionSpeed::Default)
            {
                enumValue = CompressionSpeedToDescriptor(options.compressionSpeed);
                writeProcs->putEnumeratedProc(token, keyCompressionSpeed, typeCompressionSpeed, enumValue);
            }

            if (options.keepColorProfile)
            {
                writeProcs->putBooleanProc(token, keyKeepColorProfile, options.keepColorProfile);
            }

            if (options.keepExif)
            {
                writeProcs->putBooleanProc(token, keyKeepEXIF, options.keepExif);
            }

            if (options.keepXmp)
            {
                writeProcs->putBooleanProc(token, keyKeepXMP, options.keepXmp);
            }

            if (options.premultipliedAlpha)
            {
                writeProcs->putBooleanProc(token, keyPremultipliedAlpha, options.premultipliedAlpha);
            }

            enumValue = ImageBitDepthToDescriptor(options.imageBitDepth);
            writeProcs->putEnumeratedProc(token, keyImageBitDepth, typeImageBitDepth, enumValue);

            error = writeProcs->closeWriteDescriptorProc(token, &formatRecord->descriptorParameters->descriptor);
        }
    }

    return error;
}


