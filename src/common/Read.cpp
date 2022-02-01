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
#include "LibHeifException.h"
#include "OSErrException.h"
#include "ReadMetadata.h"
#include "PremultipliedAlpha.h"
#include "ScopedHeif.h"
#include <memory>

namespace
{
    int64_t heif_reader_get_position(void* userData)
    {
        int64_t position;

        if (GetFilePosition(reinterpret_cast<intptr_t>(userData), position) == noErr)
        {
            return position;
        }
        else
        {
            return -1;
        }
    }

    int heif_reader_read(void* data, size_t size, void* userData)
    {
        if (ReadData(reinterpret_cast<intptr_t>(userData), data, size) == noErr)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }

    int heif_reader_seek(int64_t position, void* userData)
    {
        if (SetFilePosition(reinterpret_cast<intptr_t>(userData), position) == noErr)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }

    heif_reader_grow_status heif_reader_wait_for_file_size(int64_t target_size, void* userData)
    {
        int64 size;

        if (GetFileSize(reinterpret_cast<intptr_t>(userData), size) == noErr)
        {
            return target_size > size ? heif_reader_grow_status_size_beyond_eof : heif_reader_grow_status_size_reached;
        }
        else
        {
            return heif_reader_grow_status_size_beyond_eof;
        }
    }

    static struct heif_reader readerCallbacks
    {
        1,
        heif_reader_get_position,
        heif_reader_read,
        heif_reader_seek,
        heif_reader_wait_for_file_size
    };

    ScopedHeifImage DecodeImage(heif_image_handle* imageHandle, heif_colorspace colorSpace, heif_chroma chroma)
    {
        heif_image* tempImage;

        LibHeifException::ThrowIfError(heif_decode_image(imageHandle, &tempImage, colorSpace, chroma, nullptr));

        return ScopedHeifImage(tempImage);
    }

    ScopedHeifImageHandle GetPrimaryImageHandle(heif_context* context)
    {
        heif_image_handle* imageHandle;

        LibHeifException::ThrowIfError(heif_context_get_primary_image_handle(context, &imageHandle));

        return ScopedHeifImageHandle(imageHandle);
    }
}

OSErr DoReadPrepare(FormatRecordPtr formatRecord)
{
    PrintFunctionName();

    OSErr err = noErr;

    if (HostSupportsRequiredFeatures(formatRecord))
    {
        formatRecord->maxData /= 2;
    }
    else
    {
        err = errPlugInHostInsufficient;
    }

    return err;
}

OSErr DoReadStart(FormatRecordPtr formatRecord, Globals* globals)
{
    PrintFunctionName();

    globals->context = nullptr;
    globals->imageHandle = nullptr;
    globals->image = nullptr;

    OSErr err = noErr;

    try
    {
        ScopedHeifContext context(heif_context_alloc());

        if (context == nullptr)
        {
            throw std::bad_alloc();
        }

        // Seek to the start of the file.
        OSErrException::ThrowIfError(SetFilePosition(formatRecord->dataFork, 0));

        LibHeifException::ThrowIfError(heif_context_read_from_reader(
            context.get(),
            &readerCallbacks,
            reinterpret_cast<void*>(formatRecord->dataFork),
            nullptr));

        ScopedHeifImageHandle primaryImage = GetPrimaryImageHandle(context.get());

        const int width = heif_image_handle_get_width(primaryImage.get());
        const int height = heif_image_handle_get_height(primaryImage.get());
        const bool hasAlpha = heif_image_handle_has_alpha_channel(primaryImage.get());
        const int lumaBitsPerPixel = heif_image_handle_get_luma_bits_per_pixel(primaryImage.get());

        const heif_colorspace colorSpace = heif_colorspace_RGB;
        heif_chroma chroma;

        switch (lumaBitsPerPixel)
        {
        case 8:
            formatRecord->imageMode = plugInModeRGBColor;
            formatRecord->depth = 8;
            formatRecord->planeBytes = 1;
            chroma = hasAlpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB;
            break;
        case 10:
        case 12:
            formatRecord->imageMode = plugInModeRGB48;
            formatRecord->depth = 16;
            formatRecord->planeBytes = 2;
            formatRecord->maxValue = (1 << lumaBitsPerPixel) - 1;
#ifdef __PIMacPPC__
            chroma = hasAlpha ? heif_chroma_interleaved_RRGGBBAA_BE : heif_chroma_interleaved_RRGGBB_BE;
#else
            chroma = hasAlpha ? heif_chroma_interleaved_RRGGBBAA_LE : heif_chroma_interleaved_RRGGBB_LE;
#endif // __PIMacPPC__
            break;
        default:
            throw OSErrException(formatCannotRead);
        }

        if (formatRecord->HostSupports32BitCoordinates && formatRecord->PluginUsing32BitCoordinates)
        {
            formatRecord->imageSize32.h = width;
            formatRecord->imageSize32.v = height;
        }
        else
        {
            if (width > std::numeric_limits<int16>::max() || height > std::numeric_limits<int16>::max())
            {
                // The image is larger that the maximum value of a 16-bit signed integer.
                throw OSErrException(formatCannotRead);
            }

            formatRecord->imageSize.h = static_cast<int16>(width);
            formatRecord->imageSize.v = static_cast<int16>(height);
        }

        ScopedHeifImage image = DecodeImage(primaryImage.get(), colorSpace, chroma);

        int stride;
        uint8_t* data = heif_image_get_plane(image.get(), heif_channel_interleaved, &stride);

        if (hasAlpha && heif_image_handle_is_premultiplied_alpha(primaryImage.get()))
        {
            UnpremultiplyAlpha(data, width, height, stride, lumaBitsPerPixel);
        }

        formatRecord->data = data;
        formatRecord->planes = hasAlpha ? 4 : 3;
        formatRecord->loPlane = 0;
        formatRecord->hiPlane = formatRecord->planes - 1;
        formatRecord->colBytes = static_cast<int16>(formatRecord->planes * formatRecord->planeBytes);
        formatRecord->rowBytes = stride;

        if (hasAlpha && formatRecord->transparencyPlane != 0)
        {
            formatRecord->transparencyPlane = 3;
        }

        SetRect(formatRecord, 0, 0, height, width);

        // The context, image handle and image must remain valid until DoReadFinish is called.
        // The host will read the image data when DoReadStart returns, and the meta-data will be set in DoReadContinue.
        globals->context = context.release();
        globals->imageHandle = primaryImage.release();
        globals->image = image.release();
    }
    catch (const std::bad_alloc&)
    {
        err = memFullErr;
    }
    catch (const LibHeifException& e)
    {
        err = HandleErrorMessage(formatRecord, e.what(), readErr);
    }
    catch (const OSErrException& e)
    {
        err = e.GetErrorCode();
    }
    catch (const std::exception& e)
    {
        err = HandleErrorMessage(formatRecord, e.what(), readErr);
    }
    catch (...)
    {
        err = readErr;
    }

    return err;
}

OSErr DoReadContinue(FormatRecordPtr formatRecord, Globals* globals)
{
    PrintFunctionName();

    formatRecord->data = nullptr;
    SetRect(formatRecord, 0, 0, 0, 0);

    OSErr err = noErr;

    try
    {
        if (HandleSuiteIsAvailable(formatRecord))
        {
            if (PropertySuiteIsAvailable(formatRecord))
            {
                ReadExifMetadata(formatRecord, globals->imageHandle);
                ReadXmpMetadata(formatRecord, globals->imageHandle);
            }

            if (formatRecord->canUseICCProfiles)
            {
                ReadIccProfileMetadata(formatRecord, globals->imageHandle);
            }
        }
    }
    catch (const std::bad_alloc&)
    {
        err = memFullErr;
    }
    catch (const LibHeifException& e)
    {
        err = HandleErrorMessage(formatRecord, e.what(), readErr);
    }
    catch (const OSErrException& e)
    {
        err = e.GetErrorCode();
    }
    catch (const std::exception& e)
    {
        err = HandleErrorMessage(formatRecord, e.what(), readErr);
    }
    catch (...)
    {
        err = readErr;
    }

    return err;
}

OSErr DoReadFinish(Globals* globals)
{
    PrintFunctionName();

    if (globals->image != nullptr)
    {
        heif_image_release(globals->image);
        globals->image = nullptr;
    }

    if (globals->imageHandle != nullptr)
    {
        heif_image_handle_release(globals->imageHandle);
        globals->imageHandle = nullptr;
    }

    if (globals->context != nullptr)
    {
        heif_context_free(globals->context);
        globals->context = nullptr;
    }

    return noErr;
}
