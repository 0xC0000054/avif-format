/*
 * This file is part of avif-format, an AV1 Image (AVIF) file format
 * plug-in for Adobe Photoshop(R).
 *
 * Copyright (c) 2021, 2022, 2023 Nicholas Hayes
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

#ifndef AVIFFORMAT_H
#define AVIFFORMAT_H

#include "Common.h"
#include "ColorTransfer.h"
#include "Utilities.h"

enum class ChromaSubsampling
{
    Yuv420,
    Yuv422,
    Yuv444
};

enum class CompressionSpeed
{
    Fastest,
    Default,
    Slowest
};

enum class ImageBitDepth
{
    Eight,
    Ten,
    Twelve
};

constexpr float displayGammaMin = 1.0f;
constexpr float displayGammaMax = 3.0f;

constexpr int nominalPeakBrightnessMin = 1;
constexpr int nominalPeakBrightnessMax = 10000;

// The PQ default brightness is 80 nits for compatibility with older images and Krita.
// 80 nits is the sRGB reference viewing environment maximum luminance level
// See the 'Screen luminance level' value in the sRGB reference viewing environment table
// https://en.wikipedia.org/wiki/SRGB#Viewing_environment
constexpr int pqDefaultBrightness = 80;

struct HLGOptions
{
    bool applyOOTF;
    float displayGamma;
    int nominalPeakBrightness;
};

struct PQOptions
{
    int nominalPeakBrightness;
};

enum class LoadOptionsHDRFormat : int
{
    Unknown = 0,
    HLG,
    PQ
};

struct LoadUIOptions
{
    LoadOptionsHDRFormat format;
    HLGOptions hlg;
    PQOptions pq;
};

struct SaveUIOptions
{
    int quality;
    ChromaSubsampling chromaSubsampling;
    CompressionSpeed compressionSpeed;
    ImageBitDepth imageBitDepth;
    ColorTransferFunction hdrTransferFunction;
    PQOptions pq;
    bool lossless;
    bool losslessAlpha;
    bool keepColorProfile;
    bool keepExif;
    bool keepXmp;
    bool premultipliedAlpha;
};

struct RevertInfo
{
    int version;

    // Version 0 fields:

    HLGOptions hlg;

    // Version 1 fields:

    PQOptions pq;
    LoadOptionsHDRFormat format;
};

struct Globals
{
    heif_context* context;
    heif_image_handle* imageHandle;
    heif_color_profile_nclx* imageHandleNclxProfile;
    heif_image* image;
    heif_color_profile_type imageHandleProfileType;

    LoadUIOptions loadOptions;
    SaveUIOptions saveOptions;
    bool libheifInitialized;
};

DLLExport MACPASCAL void PluginMain(
    const short selector,
    FormatRecordPtr formatParamBlock,
    intptr_t* data,
    short* result);


OSErr HandleErrorMessage(FormatRecordPtr formatRecord, const char* const message, OSErr fallbackErrorCode);

OSErr NewPIHandle(const FormatRecordPtr formatRecord, int32 size, Handle* handle);
void DisposePIHandle(const FormatRecordPtr formatRecord, Handle handle);
Ptr LockPIHandle(const FormatRecordPtr formatRecord, Handle handle, Boolean moveHigh);
void UnlockPIHandle(const FormatRecordPtr formatRecord, Handle handle);

// Platform-specific UI methods

void DoAbout(const AboutRecordPtr aboutRecord);
bool DoHLGLoadUI(const FormatRecordPtr formatRecord, LoadUIOptions& options);
bool DoPQLoadUI(const FormatRecordPtr formatRecord, LoadUIOptions& options);
bool DoSaveUI(const FormatRecordPtr formatRecord, SaveUIOptions& options);
OSErr ShowErrorDialog(const FormatRecordPtr formatRecord, const char* const message, OSErr fallbackErrorCode);

// File Format API callbacks

OSErr DoReadPrepare(FormatRecordPtr formatRecord);
OSErr DoReadStart(FormatRecordPtr formatRecord, Globals* globals);
OSErr DoReadContinue(FormatRecordPtr formatRecord, Globals* globals);
OSErr DoReadFinish(FormatRecordPtr formatRecord, Globals* globals);

OSErr DoOptionsPrepare(FormatRecordPtr formatRecord);
OSErr DoOptionsStart(FormatRecordPtr formatRecord, Globals* globals);
OSErr DoOptionsContinue();
OSErr DoOptionsFinish();

OSErr DoEstimatePrepare(FormatRecordPtr formatRecord);
OSErr DoEstimateStart(FormatRecordPtr formatRecord);
OSErr DoEstimateContinue();
OSErr DoEstimateFinish();

OSErr DoWritePrepare(FormatRecordPtr formatRecord);
OSErr DoWriteStart(FormatRecordPtr formatRecord, SaveUIOptions& options);
OSErr DoWriteContinue();
OSErr DoWriteFinish(FormatRecordPtr formatRecord, const SaveUIOptions& options);

// Scripting

OSErr ReadScriptParamsOnRead(FormatRecordPtr formatRecord, LoadUIOptions& options, Boolean* showDialog);
OSErr WriteScriptParamsOnRead(FormatRecordPtr formatRecord, const LoadUIOptions& options);

OSErr ReadScriptParamsOnWrite(FormatRecordPtr formatRecord, SaveUIOptions& options, Boolean* showDialog);
OSErr WriteScriptParamsOnWrite(FormatRecordPtr formatRecord, const SaveUIOptions& options);

#endif // !AVIFFORMAT_H
