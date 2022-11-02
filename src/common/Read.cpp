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
#include "ColorProfileGeneration.h"
#include "FileIO.h"
#include "LibHeifException.h"
#include "OSErrException.h"
#include "ReadHeifImage.h"
#include "ReadMetadata.h"
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

    ScopedHeifNclxProfile GetNclxColorProfile(const heif_image* image)
    {
        heif_color_profile_nclx* nclxProfile;

        heif_error err = heif_image_get_nclx_color_profile(image, &nclxProfile);

        if (err.code != heif_error_Ok)
        {
            switch (err.code)
            {
            case heif_error_Color_profile_does_not_exist:
                nclxProfile = nullptr;
                break;
            default:
                throw LibHeifException(err);
            }
        }

        return ScopedHeifNclxProfile(nclxProfile);
    }

    ScopedHeifNclxProfile GetNclxColorProfile(const heif_image_handle* image)
    {
        heif_color_profile_nclx* nclxProfile;

        heif_error err = heif_image_handle_get_nclx_color_profile(image, &nclxProfile);

        if (err.code != heif_error_Ok)
        {
            switch (err.code)
            {
            case heif_error_Color_profile_does_not_exist:
                nclxProfile = nullptr;
                break;
            default:
                throw LibHeifException(err);
            }
        }

        return ScopedHeifNclxProfile(nclxProfile);
    }

    AlphaState GetAlphaState(const heif_image_handle* imageHandle)
    {
        AlphaState alphaState = AlphaState::None;

        if (heif_image_handle_has_alpha_channel(imageHandle))
        {
            if (heif_image_handle_is_premultiplied_alpha(imageHandle))
            {
                alphaState = AlphaState::Premultiplied;
            }
            else
            {
                alphaState = AlphaState::Straight;
            }
        }

        return alphaState;
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
    globals->libheifInitialized = false;

    OSErr err = noErr;

    try
    {
        LibHeifException::ThrowIfError(heif_init(nullptr));
        globals->libheifInitialized = true;

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

        ScopedHeifImage image = DecodeImage(primaryImage.get(), heif_colorspace_undefined, heif_chroma_undefined);

        const heif_colorspace colorSpace = heif_image_get_colorspace(image.get());
        const heif_chroma chroma = heif_image_get_chroma_format(image.get());

        if (colorSpace == heif_colorspace_monochrome)
        {
            if (chroma != heif_chroma_monochrome)
            {
                throw std::runtime_error("Unsupported chroma format for a monochrome image.");
            }

            switch (lumaBitsPerPixel)
            {
            case 8:
                formatRecord->imageMode = plugInModeGrayScale;
                formatRecord->depth = 8;
                break;
            case 10:
            case 12:
                formatRecord->imageMode = plugInModeGray16;
                formatRecord->depth = 16;
                break;
            default:
                throw OSErrException(formatCannotRead);
            }
            formatRecord->planes = hasAlpha ? 2 : 1;
        }
        else if (colorSpace == heif_colorspace_RGB)
        {
            if (chroma != heif_chroma_444)
            {
                throw std::runtime_error("Unsupported chroma format for a RGB image.");
            }

            switch (lumaBitsPerPixel)
            {
            case 8:
                formatRecord->imageMode = plugInModeRGBColor;
                formatRecord->depth = 8;
                break;
            case 10:
            case 12:
                formatRecord->imageMode = plugInModeRGB48;
                formatRecord->depth = 16;
                break;
            default:
                throw OSErrException(formatCannotRead);
            }
            formatRecord->planes = hasAlpha ? 4 : 3;
        }
        else if (colorSpace == heif_colorspace_YCbCr)
        {
            if (chroma != heif_chroma_420 && chroma != heif_chroma_422 && chroma != heif_chroma_444)
            {
                throw std::runtime_error("Unsupported chroma format for a YCbCr image.");
            }

            switch (lumaBitsPerPixel)
            {
            case 8:
                formatRecord->imageMode = plugInModeRGBColor;
                formatRecord->depth = 8;
                break;
            case 10:
            case 12:
                formatRecord->imageMode = plugInModeRGB48;
                formatRecord->depth = 16;
                break;
            default:
                throw OSErrException(formatCannotRead);
            }
            formatRecord->planes = hasAlpha ? 4 : 3;
        }
        else
        {
            throw std::runtime_error("Unsupported image color space, expected monochrome, RGB or YCbCr.");
        }

        if (hasAlpha && formatRecord->transparencyPlane)
        {
            // Transparency data is always the last plane in the image.
            formatRecord->transparencyPlane = formatRecord->planes - 1;
        }

        // The context, image handle and image must remain valid until DoReadFinish is called.
        // The image data and meta-data will be set in DoReadContinue.
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

    OSErr err = noErr;

    try
    {
        ScopedHeifNclxProfile imageNclxProfile = GetNclxColorProfile(globals->image);

        const AlphaState alphaState = GetAlphaState(globals->imageHandle);

        switch (formatRecord->imageMode)
        {
        case plugInModeGrayScale:
            ReadHeifImageGrayEightBit(globals->image, alphaState, imageNclxProfile.get(), formatRecord);
            break;
        case plugInModeGray16:
            ReadHeifImageGraySixteenBit(globals->image, alphaState, imageNclxProfile.get(), formatRecord);
            break;
        case plugInModeRGBColor:
            ReadHeifImageRGBEightBit(globals->image, alphaState, imageNclxProfile.get(), formatRecord);
            break;
        case plugInModeRGB48:
            ReadHeifImageRGBSixteenBit(globals->image, alphaState, imageNclxProfile.get(), formatRecord);
            break;
        default:
            throw OSErrException(formatCannotRead);
        }

        SetRect(formatRecord, 0, 0, 0, 0);
        formatRecord->data = nullptr;

        if (HandleSuiteIsAvailable(formatRecord))
        {
            if (PropertySuiteIsAvailable(formatRecord))
            {
                ReadExifMetadata(formatRecord, globals->imageHandle);
                ReadXmpMetadata(formatRecord, globals->imageHandle);
            }

            if (formatRecord->canUseICCProfiles)
            {
                const heif_color_profile_type imageHandleProfileType = heif_image_handle_get_color_profile_type(globals->imageHandle);

                if (imageHandleProfileType == heif_color_profile_type_prof ||
                    imageHandleProfileType == heif_color_profile_type_rICC)
                {
                    ReadIccProfileMetadata(formatRecord, globals->imageHandle);
                }
                else
                {
                    if (imageNclxProfile)
                    {
                        SetIccProfileFromNclx(formatRecord, imageNclxProfile.get());
                    }
                    else if (imageHandleProfileType == heif_color_profile_type_nclx)
                    {
                        ScopedHeifNclxProfile imageHandleNclxProfile = GetNclxColorProfile(globals->imageHandle);

                        SetIccProfileFromNclx(formatRecord, imageHandleNclxProfile.get());
                    }
                }
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

    if (globals->libheifInitialized)
    {
        heif_deinit();
        globals->libheifInitialized = false;
    }

    return noErr;
}
