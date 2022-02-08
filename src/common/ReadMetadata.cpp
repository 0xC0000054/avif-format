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

#include "ReadMetadata.h"
#include "ExifParser.h"
#include "LibHeifException.h"
#include "OSErrException.h"
#include "PIProperties.h"
#include "ScopedBufferSuite.h"
#include <stdexcept>
#include <vector>

namespace
{
    bool HasIccProfile(const heif_image_handle* handle)
    {
        switch (heif_image_handle_get_color_profile_type(handle))
        {
        case heif_color_profile_type_prof:
        case heif_color_profile_type_rICC:
            return true;
        case heif_color_profile_type_nclx:
        case heif_color_profile_type_not_present:
        default:
            return false;
        }
    }

    bool TryGetExifItemId(const heif_image_handle* handle, heif_item_id& exifId)
    {
        heif_item_id id;

        if (heif_image_handle_get_list_of_metadata_block_IDs(handle, "Exif", &id, 1) == 1)
        {
            exifId = id;
            return true;
        }

        return false;
    }

    bool TryGetXmpItemId(const heif_image_handle* handle, heif_item_id& xmpId)
    {
        int mimeBlockCount = heif_image_handle_get_number_of_metadata_blocks(handle, "mime");

        if (mimeBlockCount == 0)
        {
            return false;
        }

        std::vector<heif_item_id> ids(mimeBlockCount);

        if (heif_image_handle_get_list_of_metadata_block_IDs(handle, "mime", ids.data(), mimeBlockCount) == mimeBlockCount)
        {
            for (size_t i = 0; i < ids.size(); i++)
            {
                const heif_item_id id = ids[i];
                const char* contentType = heif_image_handle_get_metadata_content_type(handle, id);

                if (strcmp(contentType, "application/rdf+xml") == 0)
                {
                    xmpId = id;
                    return true;
                }
            }
        }

        return false;
    }
}

void ReadExifMetadata(const FormatRecordPtr formatRecord, const heif_image_handle* handle)
{
    heif_item_id exifId;

    if (TryGetExifItemId(handle, exifId))
    {
        const size_t size = heif_image_handle_get_metadata_size(handle, exifId);

        // The EXIF data block has a header that indicates the number of bytes
        // that come before the start of the TIFF header.
        // See ISO/IEC 23008-12:2017 section A.2.1.
        constexpr size_t exifHeaderSize = sizeof(uint32_t);

        if (size > exifHeaderSize && size <= static_cast<size_t>(std::numeric_limits<int32>::max()))
        {
            ScopedBufferSuiteBuffer exifBuffer(formatRecord->bufferProcs, static_cast<int32>(size));
            uint8* exifBlock = static_cast<uint8*>(exifBuffer.Lock());

            LibHeifException::ThrowIfError(heif_image_handle_get_metadata(handle, exifId, exifBlock));

            uint32_t tiffHeaderOffset = (exifBlock[0] << 24) | (exifBlock[1] << 16) | (exifBlock[2] << 8) | exifBlock[3];

            size_t headerStartOffset = static_cast<size_t>(4) + tiffHeaderOffset;
            size_t exifDataLength = size - headerStartOffset;

            if (exifDataLength > 0 &&
                exifDataLength <= static_cast<size_t>(std::numeric_limits<int32>::max()) &&
                CheckTiffFileSignature(exifBlock + headerStartOffset, exifDataLength))
            {
                // Set the EXIF orientation value to top-left, this results in the host treating it as a no-op.
                // The HEIF specification requires that readers ignore the EXIF orientation tag.
                SetExifOrientationToTopLeft(exifBlock + headerStartOffset, exifDataLength);

                Handle complexProperty = formatRecord->handleProcs->newProc(static_cast<int32>(exifDataLength));

                if (complexProperty != nullptr)
                {
                    Ptr ptr = formatRecord->handleProcs->lockProc(complexProperty, false);

                    if (ptr != nullptr)
                    {
                        memcpy(ptr, exifBlock + headerStartOffset, exifDataLength);

                        formatRecord->handleProcs->unlockProc(complexProperty);

                        OSErr err = formatRecord->propertyProcs->setPropertyProc(
                            kPhotoshopSignature,
                            propEXIFData,
                            0,
                            0,
                            complexProperty);

                        // The host takes ownership of the handle if the call succeeds, we dispose the handle if it fails.
                        if (err != noErr)
                        {
                            formatRecord->handleProcs->disposeProc(complexProperty);
                        }
                    }
                    else
                    {
                        formatRecord->handleProcs->disposeProc(complexProperty);
                        throw OSErrException(nilHandleErr);
                    }
                }
                else
                {
                    throw std::bad_alloc();
                }
            }
        }
    }
}

void ReadIccProfileMetadata(const FormatRecordPtr formatRecord, const heif_image_handle* handle)
{
    if (HasIccProfile(handle))
    {
        const size_t iccProfileLength = heif_image_handle_get_raw_color_profile_size(handle);

        if (iccProfileLength > 0 && iccProfileLength <= static_cast<size_t>(std::numeric_limits<int32>::max()))
        {
            Handle iccProfile = formatRecord->handleProcs->newProc(static_cast<int32>(iccProfileLength));

            if (iccProfile != nullptr)
            {
                Ptr ptr = formatRecord->handleProcs->lockProc(iccProfile, false);

                if (ptr != nullptr)
                {
                    heif_error error = heif_image_handle_get_raw_color_profile(handle, ptr);

                    formatRecord->handleProcs->unlockProc(iccProfile);

                    if (error.code == heif_error_Ok)
                    {
                        formatRecord->iCCprofileData = iccProfile;
                        formatRecord->iCCprofileSize = static_cast<int32>(iccProfileLength);
                    }
                    else
                    {
                        formatRecord->handleProcs->disposeProc(iccProfile);

                        if (LibHeifException::IsOutOfMemoryError(error))
                        {
                            throw std::bad_alloc();
                        }
                        else
                        {
                            throw LibHeifException(error);
                        }
                    }
                }
                else
                {
                    formatRecord->handleProcs->disposeProc(iccProfile);
                    throw OSErrException(nilHandleErr);
                }
            }
            else
            {
                throw std::bad_alloc();
            }
        }
    }
}

void ReadXmpMetadata(const FormatRecordPtr formatRecord, const heif_image_handle* handle)
{
    heif_item_id xmpId;

    if (TryGetXmpItemId(handle, xmpId))
    {
        const size_t xmpDataLength = heif_image_handle_get_metadata_size(handle, xmpId);

        if (xmpDataLength > 0 && xmpDataLength <= static_cast<size_t>(std::numeric_limits<int32>::max()))
        {
            Handle complexProperty = formatRecord->handleProcs->newProc(static_cast<int32>(xmpDataLength));

            if (complexProperty != nullptr)
            {
                Ptr ptr = formatRecord->handleProcs->lockProc(complexProperty, false);

                if (ptr != nullptr)
                {
                    heif_error error = heif_image_handle_get_metadata(handle, xmpId, ptr);

                    formatRecord->handleProcs->unlockProc(complexProperty);

                    if (error.code == heif_error_Ok)
                    {
                        OSErr err = formatRecord->propertyProcs->setPropertyProc(
                            kPhotoshopSignature,
                            propXMP,
                            0,
                            0,
                            complexProperty);

                        // The host takes ownership of the handle if the call succeeds, we dispose the handle if it fails.
                        if (err != noErr)
                        {
                            formatRecord->handleProcs->disposeProc(complexProperty);
                        }
                    }
                    else
                    {
                        formatRecord->handleProcs->disposeProc(complexProperty);

                        if (LibHeifException::IsOutOfMemoryError(error))
                        {
                            throw std::bad_alloc();
                        }
                        else
                        {
                            throw LibHeifException(error);
                        }
                    }
                }
                else
                {
                    formatRecord->handleProcs->disposeProc(complexProperty);
                    throw OSErrException(nilHandleErr);
                }
            }
            else
            {
                throw std::bad_alloc();
            }
        }
    }
}
