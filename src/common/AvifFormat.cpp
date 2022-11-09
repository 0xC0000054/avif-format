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
#include "FileIO.h"
#include <stdexcept>

namespace
{
    OSErr CreateGlobals(FormatRecordPtr formatRecord, intptr_t* data)
    {
        Handle handle;

        OSErr e = NewPIHandle(formatRecord, sizeof(Globals), &handle);

        if (e == noErr)
        {
            *data = reinterpret_cast<intptr_t>(handle);
        }

        return e;
    }

    OSErr DoFilterFile(FormatRecordPtr formatRecord)
    {
        constexpr int bufferSize = 50;
        uint8_t buffer[bufferSize] = {};

        // Seek to the start of the file.
        OSErr err = SetFilePosition(formatRecord->dataFork, 0);

        if (err == noErr)
        {
            err = ReadData(formatRecord->dataFork, buffer, bufferSize);

            if (err == noErr)
            {
                try
                {
                    if (!heif_has_compatible_brand(buffer, bufferSize, "avif"))
                    {
                        err = formatCannotRead;
                    }
                }
                catch (const std::bad_alloc&)
                {
                    err = memFullErr;
                }
                catch (...)
                {
                    err = formatCannotRead;
                }
            }
        }

        return err;
    }

    void InitGlobals(Globals* globals)
    {
        globals->context = nullptr;
        globals->imageHandle = nullptr;
        globals->imageHandleNclxProfile = nullptr;
        globals->image = nullptr;
        globals->imageHandleProfileType = heif_color_profile_type_not_present;
        globals->saveOptions.quality = 85;
        globals->saveOptions.chromaSubsampling = ChromaSubsampling::Yuv422;
        globals->saveOptions.compressionSpeed = CompressionSpeed::Default;
        globals->saveOptions.lossless = false;
        globals->saveOptions.losslessAlpha = true;
        globals->saveOptions.imageBitDepth = ImageBitDepth::Twelve; // The save UI will default to 8-bit if the image is 8-bit.
        globals->saveOptions.keepColorProfile = false;
        globals->saveOptions.keepExif = false;
        globals->saveOptions.keepXmp = false;
        globals->saveOptions.premultipliedAlpha = false;
        globals->libheifInitialized = false;
    }
}

void PluginMain(const short selector, FormatRecordPtr formatRecord, intptr_t* data, short* result)
{
    if (selector == formatSelectorAbout)
    {
        DoAbout(reinterpret_cast<AboutRecordPtr>(formatRecord));
        *result = noErr;
    }
    else
    {
        if (formatRecord->HostSupports32BitCoordinates)
        {
            formatRecord->PluginUsing32BitCoordinates = true;
        }

        Globals* globals;
        if (*data == 0)
        {
            *result = CreateGlobals(formatRecord, data);
            if (*result != noErr)
            {
                return;
            }

            globals = reinterpret_cast<Globals*>(LockPIHandle(formatRecord, reinterpret_cast<Handle>(*data), false));
            InitGlobals(globals);
        }
        else
        {
            globals = reinterpret_cast<Globals*>(LockPIHandle(formatRecord, reinterpret_cast<Handle>(*data), false));
        }

        switch (selector)
        {
        case formatSelectorReadPrepare:
            *result = DoReadPrepare(formatRecord);
            break;
        case formatSelectorReadStart:
            *result = DoReadStart(formatRecord, globals);
            break;
        case formatSelectorReadContinue:
            *result = DoReadContinue(formatRecord, globals);
            break;
        case formatSelectorReadFinish:
            *result = DoReadFinish(globals);
            break;

        case formatSelectorOptionsPrepare:
            *result = DoOptionsPrepare(formatRecord);
            break;
        case formatSelectorOptionsStart:
            *result = DoOptionsStart(formatRecord, globals);
            break;
        case formatSelectorOptionsContinue:
            *result = DoOptionsContinue();
            break;
        case formatSelectorOptionsFinish:
            *result = DoOptionsFinish();
            break;

        case formatSelectorEstimatePrepare:
            *result = DoEstimatePrepare(formatRecord);
            break;
        case formatSelectorEstimateStart:
            *result = DoEstimateStart(formatRecord);
            break;
        case formatSelectorEstimateContinue:
            *result = DoEstimateContinue();
            break;
        case formatSelectorEstimateFinish:
            *result = DoEstimateFinish();
            break;

        case formatSelectorWritePrepare:
            *result = DoWritePrepare(formatRecord);
            break;
        case formatSelectorWriteStart:
            *result = DoWriteStart(formatRecord, globals->saveOptions);
            break;
        case formatSelectorWriteContinue:
            *result = DoWriteContinue();
            break;
        case formatSelectorWriteFinish:
            *result = DoWriteFinish(formatRecord, globals->saveOptions);
            break;

        case formatSelectorFilterFile:
            *result = DoFilterFile(formatRecord);
            break;

        default:
            *result = formatBadParameters;
        }

        UnlockPIHandle(formatRecord, reinterpret_cast<Handle>(*data));
    }
}

OSErr HandleErrorMessage(FormatRecordPtr formatRecord, const char* const message, OSErr fallbackErrorCode)
{
    return ShowErrorDialog(formatRecord, message, fallbackErrorCode);
}
