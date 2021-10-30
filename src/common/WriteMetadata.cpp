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

#include "WriteMetadata.h"
#include "HostMetadata.h"
#include "LibHeifException.h"
#include "ScopedBufferSuite.h"
#include "ScopedHeif.h"

namespace
{
    void SetNclxColorProfile(
        heif_image* image,
        heif_color_primaries primaries,
        heif_transfer_characteristics transferCharacteristics,
        heif_matrix_coefficients matrixCoefficients)
    {
        ScopedHeifNclxProfile nclxProfile(heif_nclx_color_profile_alloc());

        if (nclxProfile == nullptr)
        {
            throw std::bad_alloc();
        }

        nclxProfile->version = 1;
        nclxProfile->color_primaries = primaries;
        nclxProfile->transfer_characteristics = transferCharacteristics;
        nclxProfile->matrix_coefficients = matrixCoefficients;
        nclxProfile->full_range_flag = true;

        LibHeifException::ThrowIfError(heif_image_set_nclx_color_profile(image, nclxProfile.get()));
    }

    void SetIccColorProfile(const FormatRecordPtr formatRecord, heif_image* image, const SaveUIOptions& saveOptions)
    {
        const int32 dataSize = formatRecord->handleProcs->getSizeProc(formatRecord->iCCprofileData);

        if (dataSize > 0)
        {
            ScopedHandleSuiteLock lock(formatRecord->handleProcs, formatRecord->iCCprofileData);

            Ptr data = lock.Data();

            if (data != nullptr)
            {
                LibHeifException::ThrowIfError(heif_image_set_raw_color_profile(
                    image,
                    "prof",
                    data,
                    static_cast<size_t>(dataSize)));

                if (saveOptions.lossless)
                {
                    SetNclxColorProfile(
                        image,
                        heif_color_primaries_unspecified,
                        heif_transfer_characteristic_unspecified,
                        heif_matrix_coefficients_RGB_GBR);
                }
            }
        }
    }

    bool GetExifDataWithHeader(const FormatRecordPtr formatRecord, ScopedBufferSuiteBuffer& buffer)
    {
        bool result = false;

        ScopedHandleSuiteHandle exif = GetExifMetadata(formatRecord);

        if (exif != nullptr)
        {
            const int32 exifSize = exif.GetSize();

            if (exifSize > 0)
            {
                // The EXIF data block has a header that indicates the number of bytes
                // that come before the start of the TIFF header.
                // See ISO/IEC 23008-12:2017 section A.2.1.
                const int64 exifSizeWithHeader = static_cast<int64>(exifSize) + 4;

                if (exifSizeWithHeader <= std::numeric_limits<int32>::max())
                {
                    ScopedHandleSuiteLock lock = exif.Lock();
                    void* exifDataPtr = lock.Data();

                    if (exifDataPtr != nullptr)
                    {
                        buffer.Reset(static_cast<int32>(exifSizeWithHeader));

                        uint8* destinationBuffer = static_cast<uint8*>(buffer.Lock());

                        uint32_t* tiffHeaderOffset = reinterpret_cast<uint32_t*>(destinationBuffer);
                        *tiffHeaderOffset = 0;

                        memcpy(destinationBuffer + sizeof(uint32_t), exifDataPtr, static_cast<size_t>(exifSize));
                        result = true;
                    }
                }
            }
        }

        return result;
    }
}

void AddColorProfileToImage(const FormatRecordPtr formatRecord, heif_image* image, const SaveUIOptions& saveOptions)
{
    if (saveOptions.keepColorProfile && HasColorProfileMetadata(formatRecord))
    {
        SetIccColorProfile(formatRecord, image, saveOptions);
    }
    else
    {
        const heif_color_primaries primaries = heif_color_primaries_ITU_R_BT_709_5;
        const heif_transfer_characteristics transferCharacteristics = heif_transfer_characteristic_IEC_61966_2_1;
        heif_matrix_coefficients matrixCoefficients = heif_matrix_coefficients_ITU_R_BT_601_6;

        if (saveOptions.lossless)
        {
            matrixCoefficients = heif_matrix_coefficients_RGB_GBR;
        }

        SetNclxColorProfile(image, primaries, transferCharacteristics, matrixCoefficients);
    }
}

void AddExifMetadata(const FormatRecordPtr formatRecord, heif_context* context, heif_image_handle* imageHandle)
{
    ScopedBufferSuiteBuffer exif(formatRecord->bufferProcs);

    if (GetExifDataWithHeader(formatRecord, exif))
    {
        const int32 bufferSize = exif.GetSize();
        void* ptr = exif.Lock();

        if (ptr != nullptr)
        {
            LibHeifException::ThrowIfError(heif_context_add_exif_metadata(context, imageHandle, ptr, bufferSize));
        }
    }
}

void AddXmpMetadata(const FormatRecordPtr formatRecord, heif_context* context, heif_image_handle* imageHandle)
{
    ScopedHandleSuiteHandle xmp = GetXmpMetadata(formatRecord);

    if (xmp != nullptr)
    {
        const int32 xmpSize = xmp.GetSize();

        if (xmpSize > 0)
        {
            ScopedHandleSuiteLock lock = xmp.Lock();
            void* ptr = lock.Data();

            if (ptr != nullptr)
            {
                LibHeifException::ThrowIfError(heif_context_add_XMP_metadata(context, imageHandle, ptr, xmpSize));
            }
        }
    }
}
