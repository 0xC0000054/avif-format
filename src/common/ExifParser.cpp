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

#include "ExifParser.h"
#include <bit>
#include <limits>

namespace
{
    static const char bigEndianTiffSignature[] = "MM\0*";
    static const char littleEndianTiffSignature[] = "II*\0";

    uint32 ReadLong(const uint8* data, size_t startOffset, bool bigEndian)
    {
        uint32 result;

        if (bigEndian)
        {
            result = (data[startOffset] << 24) | (data[startOffset + 1] << 16) | (data[startOffset + 2] << 8) | data[startOffset + 3];
        }
        else
        {
            result = data[startOffset] | (data[startOffset + 1] << 8) | (data[startOffset + 2] << 16) | (data[startOffset + 3] << 24);
        }

        return result;
    }

    uint16 ReadShort(const uint8* data, size_t startOffset, bool bigEndian)
    {
        uint16 result;

        if (bigEndian)
        {
            result = (data[startOffset] << 8) | data[startOffset + 1];
        }
        else
        {
            result = data[startOffset] | (data[startOffset + 1] << 8);
        }

        return result;
    }

    bool TryReadLong(const uint8* data, size_t startOffset, size_t dataLength, bool bigEndian, uint32& result)
    {
        if ((startOffset + sizeof(uint32)) > dataLength)
        {
            result = 0;
            return false;
        }

        result = ReadLong(data, startOffset, bigEndian);
        return true;
    }

    bool TryReadShort(const uint8* data, size_t startOffset, size_t dataLength, bool bigEndian, uint16& result)
    {
        if ((startOffset + sizeof(uint16)) > dataLength)
        {
            result = 0;
            return false;
        }

        result = ReadShort(data, startOffset, bigEndian);
        return true;
    }

    void WriteShort(uint8* data, size_t startOffset, bool bigEndian, uint16 value)
    {
        if constexpr(std::endian::native == std::endian::little)
        {
            if (bigEndian)
            {
                data[startOffset] = static_cast<uint8>((value >> 8) & 0xff);
                data[startOffset + 1] = static_cast<uint8>(value & 0xff);
            }
            else
            {
                data[startOffset] = static_cast<uint8>(value & 0xff);
                data[startOffset + 1] = static_cast<uint8>((value >> 8) & 0xff);
            }
        }
        else
        {
            if (bigEndian)
            {
                data[startOffset] = static_cast<uint8>(value & 0xff);
                data[startOffset + 1] = static_cast<uint8>((value >> 8) & 0xff);
            }
            else
            {
                data[startOffset] = static_cast<uint8>((value >> 8) & 0xff);
                data[startOffset + 1] = static_cast<uint8>(value & 0xff);
            }
        }
    }
}

bool CheckTiffFileSignature(const uint8* data, size_t dataLength)
{
    // We must have at least 4 bytes to check the TIFF file signature.
    if (dataLength < 4)
    {
        return false;
    }

    return memcmp(data, bigEndianTiffSignature, 4) == 0 || memcmp(data, littleEndianTiffSignature, 4) == 0;
}

void SetExifOrientationToTopLeft(uint8* data, size_t dataLength)
{
    if (CheckTiffFileSignature(data, dataLength))
    {
        size_t offset = 4;
        const size_t length = dataLength;

        const bool bigEndian = memcmp(data, bigEndianTiffSignature, 4) == 0;
        uint32 ifdOffset;

        if (TryReadLong(data, offset, length, bigEndian, ifdOffset) &&
            ifdOffset < static_cast<uint32>(std::numeric_limits<int32>::max()))
        {
            offset = ifdOffset;

            uint16 directoryEntryCount;

            if (offset < length &&
                TryReadShort(data, offset, length, bigEndian, directoryEntryCount))
            {
                offset += sizeof(uint16);

                constexpr size_t tiffIfdEntrySize = 12;

                const size_t directoryEntryLengthInBytes = static_cast<size_t>(directoryEntryCount) * tiffIfdEntrySize;

                if ((offset + directoryEntryLengthInBytes) <= length)
                {
                    constexpr uint16 orientationTag = 274;
                    constexpr uint16 orientationType = 3; // SHORT - 16-bit unsigned integer
                    constexpr uint16 orientationItemCount = 1;

                    for (uint16 i = 0; i < directoryEntryCount; i++)
                    {
                        uint16 entryTag = ReadShort(data, offset, bigEndian);
                        offset += sizeof(uint16);

                        uint16 entryType = ReadShort(data, offset, bigEndian);;
                        offset += sizeof(uint16);

                        uint32 entryItemCount = ReadLong(data, offset, bigEndian);
                        offset += sizeof(uint32);

                        size_t entryOffsetFieldLocation = offset;
                        offset += sizeof(uint32);

                        if (entryTag == orientationTag &&
                            entryType == orientationType &&
                            entryItemCount == orientationItemCount)
                        {
                            constexpr uint16 orientationTopLeft = 1;

                            // The value is packed into the IFD entry offset field.
                            WriteShort(data, entryOffsetFieldLocation, bigEndian, orientationTopLeft);

                            break;
                        }
                    }
                }
            }
        }
    }
}
