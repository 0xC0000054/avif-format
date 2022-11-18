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
#include "PremultipliedAlpha.h"
#include "ScopedBufferSuite.h"
#include "ScopedHeif.h"
#include "WriteHeifImage.h"
#include "WriteMetadata.h"
#include <algorithm>
#include <thread>
#include <vector>

namespace
{
    ScopedHeifImageHandle EncodeImage(
        heif_context* context,
        heif_image* image,
        heif_encoder* encoder,
        const heif_encoding_options* options)
    {
        heif_image_handle* encodedImageHandle;

        LibHeifException::ThrowIfError(heif_context_encode_image(context, image, encoder, options, &encodedImageHandle));

        return ScopedHeifImageHandle(encodedImageHandle);
    }

    ScopedHeifEncoder GetAOMEncoder(heif_context* context)
    {
        heif_encoder* tempEncoder;

        const heif_encoder_descriptor* aomEncoderDescriptor;

        if (heif_context_get_encoder_descriptors(context, heif_compression_AV1, "aom", &aomEncoderDescriptor, 1) != 1)
        {
            throw std::runtime_error("Unable to get the AOM encoder descriptor.");
        }

        LibHeifException::ThrowIfError(heif_context_get_encoder(context, aomEncoderDescriptor, &tempEncoder));

        return ScopedHeifEncoder(tempEncoder);
    }

    heif_error heif_writer_write(
        heif_context* /*context*/,
        const void* data,
        size_t size,
        void* userdata)
    {
        static heif_error Success = { heif_error_Ok, heif_suberror_Unspecified, "Success" };
        static heif_error WriteError = { heif_error_Encoding_error, heif_suberror_Cannot_write_output_data, "Write error" };

        return WriteData(reinterpret_cast<intptr_t>(userdata), data, size) == noErr ? Success : WriteError;
    }

    void WriteEncodedImage(const FormatRecordPtr formatRecord, heif_context* context)
    {
        static heif_writer writer = { 1, heif_writer_write };

        LibHeifException::ThrowIfError(heif_context_write(context, &writer, reinterpret_cast<void*>(formatRecord->dataFork)));
    }

    void EncodeAndSaveImage(
        const FormatRecordPtr formatRecord,
        heif_context* context,
        heif_image* image,
        const SaveUIOptions& saveOptions)
    {
        formatRecord->progressProc(50, 100);

        AddColorProfileToImage(formatRecord, image, saveOptions);

        ScopedHeifEncoder encoder = GetAOMEncoder(context);

        if (saveOptions.lossless)
        {
            heif_encoder_set_lossy_quality(encoder.get(), 100);
            heif_encoder_set_lossless(encoder.get(), true);
            heif_encoder_set_parameter(encoder.get(), "chroma", "444");
        }
        else
        {
            heif_encoder_set_lossy_quality(encoder.get(), saveOptions.quality);
            heif_encoder_set_lossless(encoder.get(), false);

            switch (saveOptions.chromaSubsampling)
            {
            case ChromaSubsampling::Yuv420:
                heif_encoder_set_parameter(encoder.get(), "chroma", "420");
                break;
            case ChromaSubsampling::Yuv422:
                heif_encoder_set_parameter(encoder.get(), "chroma", "422");
                break;
            case ChromaSubsampling::Yuv444:
                heif_encoder_set_parameter(encoder.get(), "chroma", "444");
                break;
            default:
                throw OSErrException(formatBadParameters);
            }

            if (HasAlphaChannel(formatRecord) && saveOptions.losslessAlpha)
            {
                heif_encoder_set_parameter_integer(encoder.get(), "alpha-quality", 100);
                heif_encoder_set_parameter_boolean(encoder.get(), "lossless-alpha", true);
            }
        }

        switch (saveOptions.compressionSpeed)
        {
        case CompressionSpeed::Fastest:
            heif_encoder_set_parameter_integer(encoder.get(), "speed", 6);
            heif_encoder_set_parameter_boolean(encoder.get(), "realtime", true);
            break;
        case CompressionSpeed::Slowest:
            heif_encoder_set_parameter_integer(encoder.get(), "speed", 1);
            break;
        case CompressionSpeed::Default:
            heif_encoder_set_parameter_integer(encoder.get(), "speed", 4);
            break;
        default:
            throw OSErrException(formatBadParameters);
        }

        const unsigned int threadCount = std::clamp(std::thread::hardware_concurrency(), 1U, 16U);
        heif_encoder_set_parameter_integer(encoder.get(), "threads", static_cast<int>(threadCount));

        ScopedHeifEncodingOptions encodingOptions(heif_encoding_options_alloc());

        if (encodingOptions == nullptr)
        {
            throw std::bad_alloc();
        }

        encodingOptions->save_two_colr_boxes_when_ICC_and_nclx_available = true;
        encodingOptions->macOS_compatibility_workaround_no_nclx_profile = false;

        // Check if cancellation has been requested before staring the encode.
        // Unfortunately, most encoders do not provide a way to cancel an encode that is in progress.
        if (formatRecord->abortProc())
        {
            throw OSErrException(userCanceledErr);
        }

        ScopedHeifImageHandle encodedImageHandle = EncodeImage(
            context,
            image,
            encoder.get(),
            encodingOptions.get());

        formatRecord->progressProc(75, 100);
        if (formatRecord->abortProc())
        {
            throw OSErrException(userCanceledErr);
        }

        if (saveOptions.keepExif)
        {
            AddExifMetadata(formatRecord, context, encodedImageHandle.get());
        }

        if (saveOptions.keepXmp)
        {
            AddXmpMetadata(formatRecord, context, encodedImageHandle.get());
        }

        WriteEncodedImage(formatRecord, context);

        formatRecord->progressProc(100, 100);
    }

    AlphaState GetAlphaState(const FormatRecordPtr formatRecord, const SaveUIOptions& saveOptions)
    {
        AlphaState alphaState = AlphaState::None;

        if (HasAlphaChannel(formatRecord))
        {
            // The premultiplied alpha conversion can cause colors to drift, so it is
            // disabled for lossless compression.
            if (saveOptions.premultipliedAlpha && !saveOptions.lossless)
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

OSErr DoWritePrepare(FormatRecordPtr formatRecord)
{
    PrintFunctionName();

    formatRecord->maxData /= 2;

    return noErr;
}

OSErr DoWriteStart(FormatRecordPtr formatRecord, SaveUIOptions& options)
{
    PrintFunctionName();

    OSErr err = noErr;

    ReadScriptParamsOnWrite(formatRecord, options, nullptr);

    if (formatRecord->depth == 32 && options.hdrTransferFunction == ColorTransferFunction::SMPTE428)
    {
        // SMPTE 428 requires 12-bit.
        options.imageBitDepth = ImageBitDepth::Twelve;
    }

    bool libheifInitialized = false;

    try
    {
        LibHeifException::ThrowIfError(heif_init(nullptr));
        libheifInitialized = true;

        formatRecord->progressProc(0, 100);

        ScopedHeifContext context(heif_context_alloc());

        if (context == nullptr)
        {
            throw std::bad_alloc();
        }

        const VPoint imageSize = GetImageSize(formatRecord);
        const AlphaState alphaState = GetAlphaState(formatRecord, options);

        formatRecord->planeBytes = (formatRecord->depth + 7) / 8;
        formatRecord->loPlane = 0;
        formatRecord->hiPlane = formatRecord->planes - 1;
        formatRecord->colBytes = static_cast<int16>(formatRecord->planes * formatRecord->planeBytes);

        formatRecord->progressProc(25, 100);

        const unsigned64 rowBytes = static_cast<unsigned64>(imageSize.h) * static_cast<unsigned64>(formatRecord->colBytes);

        if (rowBytes > std::numeric_limits<int32>::max())
        {
            throw std::bad_alloc();
        }
        else
        {
            formatRecord->rowBytes = static_cast<int32>(rowBytes);
        }

        ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, formatRecord->rowBytes);

        formatRecord->data = buffer.lock();

        ScopedHeifImage image;

        if (IsMonochromeImage(formatRecord))
        {
            switch (formatRecord->depth)
            {
            case 8:
                image = CreateHeifImageGrayEightBit(formatRecord, alphaState, imageSize, options);
                break;
            case 16:
                image = CreateHeifImageGraySixteenBit(formatRecord, alphaState, imageSize, options);
                break;
            case 32:
                image = CreateHeifImageGrayThirtyTwoBit(formatRecord, alphaState, imageSize, options);
                break;
            default:
                throw OSErrException(formatBadParameters);
            }
        }
        else
        {
            switch (formatRecord->depth)
            {
            case 8:
                image = CreateHeifImageRGBEightBit(formatRecord, alphaState, imageSize, options);
                break;
            case 16:
                image = CreateHeifImageRGBSixteenBit(formatRecord, alphaState, imageSize, options);
                break;
            case 32:
                image = CreateHeifImageRGBThirtyTwoBit(formatRecord, alphaState, imageSize, options);
                break;
            default:
                throw OSErrException(formatBadParameters);
            }
        }

        if (alphaState == AlphaState::Premultiplied)
        {
            heif_image_set_premultiplied_alpha(image.get(), true);
        }

        EncodeAndSaveImage(formatRecord, context.get(), image.get(), options);
    }
    catch (const std::bad_alloc&)
    {
        err = memFullErr;
    }
    catch (const LibHeifException& e)
    {
        err = HandleErrorMessage(formatRecord, e.what(), writErr);
    }
    catch (const OSErrException& e)
    {
        err = e.GetErrorCode();
    }
    catch (const std::exception& e)
    {
        err = HandleErrorMessage(formatRecord, e.what(), writErr);
    }
    catch (...)
    {
        err = writErr;
    }

    if (libheifInitialized)
    {
        heif_deinit();
    }

    formatRecord->data = nullptr;
    SetRect(formatRecord, 0, 0, 0, 0);

    return err;
}

OSErr DoWriteContinue()
{
    PrintFunctionName();

    return noErr;
}

OSErr DoWriteFinish(FormatRecordPtr formatRecord, const SaveUIOptions& options)
{
    PrintFunctionName();

    WriteScriptParamsOnWrite(formatRecord, options);
    return noErr;
}
