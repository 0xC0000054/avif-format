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

#include "AvifFormat.h"
#include "FileIO.h"
#include "LibHeifException.h"
#include "OSErrException.h"
#include "ScopedBufferSuite.h"
#include "ScopedHeif.h"
#include "WriteMetadata.h"
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

    ScopedHeifEncoder GetDefaultAV1Encoder(heif_context* context)
    {
        heif_encoder* tempEncoder;

        LibHeifException::ThrowIfError(heif_context_get_encoder_for_format(context, heif_compression_AV1, &tempEncoder));

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

    std::string GetUnsupportedEncoderMessage(const char* encoderName)
    {
        const int requiredLength = std::snprintf(nullptr, 0, "Unsupported AV1 encoder %s.", encoderName);

        if (requiredLength <= 0)
        {
            return "Unsupported AV1 encoder.";
        }

        const size_t lengthWithTerminator = static_cast<size_t>(requiredLength) + 1;

        auto buffer = std::make_unique<char[]>(lengthWithTerminator);

        const int writtenLength = std::snprintf(
            buffer.get(),
            lengthWithTerminator,
            "Unsupported AV1 encoder %s.",
            encoderName);

        return std::string(buffer.get(), buffer.get() + writtenLength);
    }

    void EncodeAndSaveImage(
        const FormatRecordPtr formatRecord,
        heif_context* context,
        heif_image* image,
        const SaveUIOptions& saveOptions)
    {
        formatRecord->progressProc(50, 100);

        AddColorProfileToImage(formatRecord, image, saveOptions);

        ScopedHeifEncoder encoder = GetDefaultAV1Encoder(context);

        if (formatRecord->depth == 8 && saveOptions.lossless)
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
        }

        const char* encoderName = heif_encoder_get_name(encoder.get());

        if (_strnicmp(encoderName, "AOM", 3) == 0)
        {
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
        }
        else
        {
            throw std::runtime_error(GetUnsupportedEncoderMessage(encoderName));
        }

        unsigned int threadCount = std::thread::hardware_concurrency();

        if (threadCount >= 1 && threadCount <= 16)
        {
            heif_encoder_set_parameter_integer(encoder.get(), "threads", static_cast<int>(threadCount));
        }

        ScopedHeifEncodingOptions encodingOptions(heif_encoding_options_alloc());

        if (encodingOptions == nullptr)
        {
            throw std::bad_alloc();
        }

        encodingOptions->save_two_colr_boxes_when_ICC_and_nclx_available = true;

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

    ScopedHeifImage CreateHeifImage(int width, int height, heif_colorspace colorspace, heif_chroma chroma)
    {
        heif_image* tempImage;

        LibHeifException::ThrowIfError(heif_image_create(width, height, colorspace, chroma, &tempImage));

        return ScopedHeifImage(tempImage);
    }

    heif_chroma GetHeifImageChroma(ImageBitDepth bitDepth, bool hasAlpha)
    {
        heif_chroma chroma;

        switch (bitDepth)
        {
        case ImageBitDepth::Eight:
            chroma = hasAlpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB;
            break;
        case ImageBitDepth::Ten:
        case ImageBitDepth::Twelve:
#ifdef __PIMacPPC__
            chroma = hasAlpha ? heif_chroma_interleaved_RRGGBBAA_BE : heif_chroma_interleaved_RRGGBB_BE;
#else
            chroma = hasAlpha ? heif_chroma_interleaved_RRGGBBAA_LE : heif_chroma_interleaved_RRGGBB_LE;
#endif // __PIMacPPC__
            break;
        default:
            throw OSErrException(formatCannotRead);
        }

        return chroma;
    }

    int GetHeifImageBitDepth(ImageBitDepth bitDepth)
    {
        int value;

        switch (bitDepth)
        {
        case ImageBitDepth::Eight:
            value = 8;
            break;
        case ImageBitDepth::Ten:
            value = 10;
            break;
        case ImageBitDepth::Twelve:
            value = 12;
            break;
        default:
            throw OSErrException(formatCannotRead);
        }

        return value;
    }

    std::vector<uint16> BuildEightBitToHeifImageLookup(int bitDepth)
    {
        std::vector<uint16> lookupTable;
        lookupTable.reserve(256);

        const int maxValue = (1 << bitDepth) - 1;
        const float maxValueFloat = static_cast<float>(maxValue);

        for (size_t i = 0; i < lookupTable.capacity(); i++)
        {
            int value = static_cast<int>(((static_cast<float>(i) / 255.0f) * maxValueFloat) + 0.5f);

            if (value < 0)
            {
                value = 0;
            }
            else if (value > maxValue)
            {
                value = maxValue;
            }

            lookupTable.push_back(static_cast<uint16>(value));
        }

        return lookupTable;
    }

    std::vector<uint8> BuildSixteenBitToEightBitLookup()
    {
        std::vector<uint8> lookupTable;
        lookupTable.reserve(32769);

        constexpr int maxValue = std::numeric_limits<uint8>::max();
        constexpr float maxValueFloat = static_cast<float>(maxValue);

        for (size_t i = 0; i < lookupTable.capacity(); i++)
        {
            int value = static_cast<int>(((static_cast<float>(i) / 32768.0f) * maxValueFloat) + 0.5f);

            if (value < 0)
            {
                value = 0;
            }
            else if (value > maxValue)
            {
                value = maxValue;
            }

            lookupTable.push_back(static_cast<uint8>(value));
        }

        return lookupTable;
    }

    std::vector<uint16> BuildSixteenBitToHeifImageLookup(int bitDepth)
    {
        std::vector<uint16> lookupTable;
        lookupTable.reserve(32769);

        const int maxValue = (1 << bitDepth) - 1;
        const float maxValueFloat = static_cast<float>(maxValue);

        for (size_t i = 0; i < lookupTable.capacity(); i++)
        {
            int value = static_cast<int>(((static_cast<float>(i) / 32768.0f) * maxValueFloat) + 0.5f);

            if (value < 0)
            {
                value = 0;
            }
            else if (value > maxValue)
            {
                value = maxValue;
            }

            lookupTable.push_back(static_cast<uint16>(value));
        }

        return lookupTable;
    }

    void ConvertEightBitDataToHeifImage(
        FormatRecordPtr formatRecord,
        const VPoint& imageSize,
        uint8_t* heifImageData,
        int heifImageStride,
        int heifImageBitDepth)
    {
        const int32 left = 0;
        const int32 right = imageSize.h;

        const int32 channelCount = formatRecord->planes;
        const int32 rowLength = imageSize.h * channelCount;

        if (heifImageBitDepth > 8)
        {
            // The 8-bit data must be converted to 10-bit or 12-bit when writing it to the heif_image.

            std::vector<uint16> lookupTable = BuildEightBitToHeifImageLookup(heifImageBitDepth);

            for (int32 y = 0; y < imageSize.v; y++)
            {
                if (formatRecord->abortProc())
                {
                    throw OSErrException(userCanceledErr);
                }

                const int32 top = y;
                const int32 bottom = std::min(top + 1, imageSize.v);

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());

                const uint8* src = static_cast<const uint8*>(formatRecord->data);
                uint16* dst = reinterpret_cast<uint16*>(heifImageData + ((static_cast<int64>(y) * heifImageStride)));

                for (int32 x = 0; x < rowLength; x += channelCount)
                {
                    switch (channelCount)
                    {
                    case 3:
                        dst[x] = lookupTable[src[x]];
                        dst[x + 1] = lookupTable[src[x + 1]];
                        dst[x + 2] = lookupTable[src[x + 2]];
                        break;
                    case 4:
                        dst[x] = lookupTable[src[x]];
                        dst[x + 1] = lookupTable[src[x + 1]];
                        dst[x + 2] = lookupTable[src[x + 2]];
                        dst[x + 3] = lookupTable[src[x + 3]];
                        break;
                    default:
                        throw OSErrException(formatBadParameters);
                    }
                }
            }
        }
        else
        {
            for (int32 y = 0; y < imageSize.v; y++)
            {
                if (formatRecord->abortProc())
                {
                    throw OSErrException(userCanceledErr);
                }

                const int32 top = y;
                const int32 bottom = std::min(top + 1, imageSize.v);

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());

                const uint8* src = static_cast<const uint8*>(formatRecord->data);
                uint8* dst = heifImageData + ((static_cast<int64>(y) * heifImageStride));

                for (int32 x = 0; x < rowLength; x += channelCount)
                {
                    switch (channelCount)
                    {
                    case 3:
                        dst[x] = src[x];
                        dst[x + 1] = src[x + 1];
                        dst[x + 2] = src[x + 2];
                        break;
                    case 4:
                        dst[x] = src[x];
                        dst[x + 1] = src[x + 1];
                        dst[x + 2] = src[x + 2];
                        dst[x + 3] = src[x + 3];
                        break;
                    default:
                        throw OSErrException(formatBadParameters);
                    }
                }
            }
        }
    }

    void ConvertSixteenBitDataToHeifImage(
        FormatRecordPtr formatRecord,
        const VPoint& imageSize,
        uint8_t* heifImageData,
        int heifImageStride,
        int heifImageBitDepth)
    {
        const int32 left = 0;
        const int32 right = imageSize.h;

        const int32 channelCount = formatRecord->planes;
        const int32 rowLength = imageSize.h * channelCount;

        if (heifImageBitDepth == 8)
        {
            // The 16-bit data must be converted to 8-bit when writing it to the heif_image.

            std::vector<uint8> lookupTable = BuildSixteenBitToEightBitLookup();

            for (int32 y = 0; y < imageSize.v; y++)
            {
                if (formatRecord->abortProc())
                {
                    throw OSErrException(userCanceledErr);
                }

                const int32 top = y;
                const int32 bottom = std::min(top + 1, imageSize.v);

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());

                const uint16* src = static_cast<const uint16*>(formatRecord->data);
                uint8* dst = heifImageData + ((static_cast<int64>(y) * heifImageStride));

                for (int32 x = 0; x < rowLength; x += channelCount)
                {
                    switch (channelCount)
                    {
                    case 3:
                        dst[x] = lookupTable[src[x]];
                        dst[x + 1] = lookupTable[src[x + 1]];
                        dst[x + 2] = lookupTable[src[x + 2]];
                        break;
                    case 4:
                        dst[x] = lookupTable[src[x]];
                        dst[x + 1] = lookupTable[src[x + 1]];
                        dst[x + 2] = lookupTable[src[x + 2]];
                        dst[x + 3] = lookupTable[src[x + 3]];
                        break;
                    default:
                        throw OSErrException(formatBadParameters);
                    }
                }
            }
        }
        else
        {
            // The 16-bit data must be converted to 10-bit or 12-bit when writing it to the heif_image.

            std::vector<uint16> lookupTable = BuildSixteenBitToHeifImageLookup(heifImageBitDepth);

            for (int32 y = 0; y < imageSize.v; y++)
            {
                if (formatRecord->abortProc())
                {
                    throw OSErrException(userCanceledErr);
                }

                const int32 top = y;
                const int32 bottom = std::min(top + 1, imageSize.v);

                SetRect(formatRecord, top, left, bottom, right);

                OSErrException::ThrowIfError(formatRecord->advanceState());

                const uint16* src = static_cast<const uint16*>(formatRecord->data);
                uint16* dst = reinterpret_cast<uint16*>(heifImageData + ((static_cast<int64>(y) * heifImageStride)));

                for (int32 x = 0; x < rowLength; x += channelCount)
                {
                    switch (channelCount)
                    {
                    case 3:
                        dst[x] = lookupTable[src[x]];
                        dst[x + 1] = lookupTable[src[x + 1]];
                        dst[x + 2] = lookupTable[src[x + 2]];
                        break;
                    case 4:
                        dst[x] = lookupTable[src[x]];
                        dst[x + 1] = lookupTable[src[x + 1]];
                        dst[x + 2] = lookupTable[src[x + 2]];
                        dst[x + 3] = lookupTable[src[x + 3]];
                        break;
                    default:
                        throw OSErrException(formatBadParameters);
                    }
                }
            }
        }
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

    try
    {
        formatRecord->progressProc(0, 100);

        ScopedHeifContext context(heif_context_alloc());

        if (context == nullptr)
        {
            throw std::bad_alloc();
        }

        const VPoint imageSize = GetImageSize(formatRecord);
        const bool hasAlpha = formatRecord->planes == 4;

        const heif_colorspace colorSpace = heif_colorspace_RGB;
        const heif_chroma chroma = GetHeifImageChroma(options.imageBitDepth, hasAlpha);
        const int heifImageBitDepth = GetHeifImageBitDepth(options.imageBitDepth);

        ScopedHeifImage image = CreateHeifImage(imageSize.h, imageSize.v, colorSpace, chroma);

        LibHeifException::ThrowIfError(heif_image_add_plane(
            image.get(),
            heif_channel_interleaved,
            imageSize.h,
            imageSize.v,
            heifImageBitDepth));

        formatRecord->planes = hasAlpha ? 4 : 3;
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
            ScopedBufferSuiteBuffer buffer(formatRecord->bufferProcs, static_cast<int32>(rowBytes));

            formatRecord->data = buffer.Lock();
            formatRecord->rowBytes = static_cast<int32>(rowBytes);

            // The image data is read into a temporary buffer, one row at a time.
            // The host application may use a different stride value than the heif_image.

            int heifImageStride;
            uint8_t* heifImageData = heif_image_get_plane(image.get(), heif_channel_interleaved, &heifImageStride);

            if (formatRecord->depth == 8)
            {
                ConvertEightBitDataToHeifImage(
                    formatRecord,
                    imageSize,
                    heifImageData,
                    heifImageStride,
                    heifImageBitDepth);
            }
            else
            {
                ConvertSixteenBitDataToHeifImage(
                    formatRecord,
                    imageSize,
                    heifImageData,
                    heifImageStride,
                    heifImageBitDepth);
            }
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
