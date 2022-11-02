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
//
// Portions of this file has been adapted from ffmpeg
// These portions are licensed under the GNU Lesser General Public License version 2.1 or later
//
// Portions of this file have been adapted from Little CMS
// These portions are licensed under the MIT license

#include "ColorProfileGeneration.h"
#include "LibHeifException.h"
#include "ScopedHandleSuite.h"
#include "ScopedHeif.h"
#include "Utilities.h"

#ifdef _MSC_VER
#pragma warning(disable: 5033) // Suppress the 'register' keyword warning.
#endif // __MSC_VER

#include "lcms2.h"

#ifdef _MSC_VER
#pragma warning(default: 5033)
#endif // __MSC_VER

#include <memory>

namespace
{
    namespace detail
    {
        struct mlu_deleter { void operator()(cmsMLU* h) noexcept { if (h) cmsMLUfree(h); } };

        struct tonecurve_deleter { void operator()(cmsToneCurve* h) noexcept { if (h) cmsFreeToneCurve(h); } };
    }

    using ScopedLcmsMLU = std::unique_ptr<cmsMLU, detail::mlu_deleter>;

    using ScopedLcmsToneCurve = std::unique_ptr<cmsToneCurve, detail::tonecurve_deleter>;

    struct ScopedLcmsContext
    {
        ScopedLcmsContext(cmsContext context) : context(context)
        {
        }

        ~ScopedLcmsContext()
        {
            if (context != nullptr)
            {
                cmsDeleteContext(context);
                context = nullptr;
            }
        }

        ScopedLcmsContext(ScopedLcmsContext&& other) noexcept
        {
            context = other.context;
            other.context = nullptr;
        }

        ScopedLcmsContext& operator=(ScopedLcmsContext&& other) noexcept
        {
            context = other.context;
            other.context = nullptr;

            return *this;
        }

        ScopedLcmsContext(const ScopedLcmsContext&) = delete;
        ScopedLcmsContext& operator=(const ScopedLcmsContext&) = delete;

        cmsContext get() const noexcept
        {
            return context;
        }

        explicit operator bool() const noexcept
        {
            return context != nullptr;
        }

    private:

        cmsContext context;
    };

    struct ScopedLcmsProfile
    {
        ScopedLcmsProfile() : profile(nullptr)
        {
        }

        ScopedLcmsProfile(cmsHPROFILE profile) : profile(profile)
        {
        }

        ScopedLcmsProfile(ScopedLcmsProfile&& other) noexcept
        {
            profile = other.profile;
            other.profile = nullptr;
        }

        ScopedLcmsProfile& operator=(ScopedLcmsProfile&& other) noexcept
        {
            profile = other.profile;
            other.profile = nullptr;

            return *this;
        }

        ScopedLcmsProfile(const ScopedLcmsProfile&) = delete;
        ScopedLcmsProfile& operator=(const ScopedLcmsProfile&) = delete;

        ~ScopedLcmsProfile()
        {
            reset();
        }

        cmsHPROFILE get() const noexcept
        {
            return profile;
        }

        void reset()
        {
            if (profile != nullptr)
            {
                cmsCloseProfile(profile);
                profile = nullptr;
            }
        }

        void reset(cmsHPROFILE newProfile)
        {
            reset();

            profile = newProfile;
        }

        explicit operator bool() const noexcept
        {
            return profile != nullptr;
        }

    private:

        cmsHPROFILE profile;
    };

    bool SetProfileDescription(cmsContext context, cmsHPROFILE profile, const wchar_t* const description)
    {
        bool result = false;

        ScopedLcmsMLU copyrightMLU(cmsMLUalloc(context, 1));
        ScopedLcmsMLU descriptionMLU(cmsMLUalloc(context, 1));

        if (copyrightMLU && descriptionMLU)
        {
            if (cmsMLUsetWide(copyrightMLU.get(), "en", "US", L"No copyright, use freely") &&
                cmsMLUsetWide(descriptionMLU.get(), "en", "US", description))
            {
                result = cmsWriteTag(profile, cmsSigCopyrightTag, copyrightMLU.get())
                      && cmsWriteTag(profile, cmsSigProfileDescriptionTag, descriptionMLU.get());
            }
        }

        return result;
    }

    ScopedLcmsProfile BuildGrayProfile(
        cmsContext context,
        cmsFloat64Number whitepointX,
        cmsFloat64Number whitepointY,
        cmsToneCurve* toneCurve,
        const wchar_t* const description)
    {
        cmsCIExyY whitepoint = { whitepointX, whitepointY, 1.0 };

        ScopedLcmsProfile profile;

        profile.reset(cmsCreateGrayProfileTHR(context, &whitepoint, toneCurve));

        if (!SetProfileDescription(context, profile.get(), description))
        {
            profile.reset();
        }

        return profile;
    }

    ScopedLcmsProfile BuildRGBProfile(
        cmsContext context,
        const cmsCIExyY* whitepoint,
        const cmsCIExyYTRIPLE* primaries,
        cmsToneCurve* toneCurve,
        const wchar_t* const description)
    {
        ScopedLcmsProfile profile;

        cmsToneCurve* rgbToneCurve[3]{};
        rgbToneCurve[0] = rgbToneCurve[1] = rgbToneCurve[2] = toneCurve;

        profile.reset(cmsCreateRGBProfileTHR(context, whitepoint, primaries, rgbToneCurve));

        if (!SetProfileDescription(context, profile.get(), description))
        {
            profile.reset();
        }

        return profile;
    }

    void SaveColorProfileToHandle(cmsHPROFILE profile, FormatRecord* formatRecord)
    {
        cmsUInt32Number profileSize = 0;

        cmsSaveProfileToMem(profile, nullptr, &profileSize);

        if (profileSize > 0 && profileSize <= static_cast<cmsUInt32Number>(std::numeric_limits<int32>::max()))
        {
            ScopedHandleSuiteHandle handle(formatRecord->handleProcs, static_cast<int32>(profileSize));

            ScopedHandleSuiteLock lock = handle.Lock();

            if (cmsSaveProfileToMem(profile, lock.Data(), &profileSize))
            {
                lock.Unlock();

                // Ownership of the handle is transfered to the host through the iCCprofileData field.
                formatRecord->iCCprofileData = handle.Release();
                formatRecord->iCCprofileSize = static_cast<int32>(profileSize);
            }
        }
    }

    void SetCICPTag(
        cmsHPROFILE profile,
        heif_color_primaries primaries,
        heif_transfer_characteristics transferCharacteristics,
        heif_matrix_coefficients matrixCoefficients,
        bool fullRange)
    {
#if LCMS_VERSION >= 2140
        cmsVideoSignalType cicp{};
        cicp.ColourPrimaries = static_cast<cmsUInt8Number>(primaries);
        cicp.TransferCharacteristics = static_cast<cmsUInt8Number>(transferCharacteristics);
        cicp.MatrixCoefficients = static_cast<cmsUInt8Number>(matrixCoefficients);
        cicp.VideoFullRangeFlag = static_cast<cmsUInt8Number>(fullRange);

        cmsWriteTag(profile, cmsSigcicpTag, &cicp);
#endif // LCMS_VERSION >= 2140
    }
}

void SetIccProfileFromNclx(FormatRecord* formatRecord, const heif_color_profile_nclx* nclx)
{
    if (nclx == nullptr)
    {
        return;
    }

    heif_color_primaries primaries = nclx->color_primaries;
    heif_transfer_characteristics transferCharacteristics = nclx->transfer_characteristics;
    heif_matrix_coefficients matrixCoefficients = nclx->matrix_coefficients;
    const bool fullRange = nclx->full_range_flag != 0;

    if (primaries == heif_color_primaries_unspecified)
    {
        primaries = heif_color_primaries_ITU_R_BT_709_5;
    }

    if (transferCharacteristics == heif_transfer_characteristic_unspecified)
    {
        transferCharacteristics = heif_transfer_characteristic_IEC_61966_2_1;
    }

    if (matrixCoefficients == heif_matrix_coefficients_unspecified)
    {
        matrixCoefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
    }

    // The 32-bits-per-channel image modes always operate in linear color.
    const bool linear = transferCharacteristics == heif_transfer_characteristic_linear || formatRecord->depth == 32;

    ScopedLcmsContext context(cmsCreateContext(nullptr, nullptr));

    if (context)
    {
        if (IsMonochromeImage(formatRecord))
        {
            ScopedLcmsToneCurve toneCurve;
            const wchar_t* description = nullptr;

            if (linear)
            {
                toneCurve.reset(cmsBuildGamma(context.get(), 1.0));
                description = L"Linear Grayscale Profile";
            }
            else if (transferCharacteristics == heif_transfer_characteristic_IEC_61966_2_1)
            {
                cmsFloat64Number Parameters[5]
                {
                    2.4,
                    1 / 1.055,
                    1 - 1 / 1.055,
                    1 / 12.92,
                    12.92 * 0.0031308,
                };

                toneCurve.reset(cmsBuildParametricToneCurve(context.get(), 4, Parameters));
                description = L"Grayscale (sRGB TRC)";
            }
            else if (transferCharacteristics == heif_transfer_characteristic_ITU_R_BT_709_5)
            {
                cmsFloat64Number Parameters[5]
                {
                    1.0 / 0.45,
                    1.0 / 1.099296826809442,
                    1.0 - 1 / 1.099296826809442,
                    1.0 / 4.5,
                    4.5 * 0.018053968510807,
                };

                toneCurve.reset(cmsBuildParametricToneCurve(context.get(), 4, Parameters));
                description = L"Grayscale (Rec. 709 TRC)";
            }

            if (toneCurve && description != nullptr)
            {
                ScopedLcmsProfile profile = BuildGrayProfile(
                    context.get(),
                    nclx->color_primary_white_x,
                    nclx->color_primary_white_y,
                    toneCurve.get(),
                    description);

                if (profile)
                {
                    SetCICPTag(profile.get(), primaries, transferCharacteristics, matrixCoefficients, fullRange);
                    SaveColorProfileToHandle(profile.get(), formatRecord);
                }
            }
        }
        else
        {
            ScopedLcmsProfile profile;

            if (primaries == heif_color_primaries_ITU_R_BT_709_5)
            {
                const cmsCIExyY whitepoint = { 0.3127, 0.3290, 1.0f }; // D65
                const cmsCIExyYTRIPLE rgbPrimaries =
                {
                    { 0.6400, 0.3300, 1.0 },
                    { 0.3000, 0.6000, 1.0 },
                    { 0.1500, 0.0600, 1.0 }
                };

                ScopedLcmsToneCurve toneCurve;
                const wchar_t* description = nullptr;

                if (linear)
                {
                    toneCurve.reset(cmsBuildGamma(context.get(), 1.0));
                    description = L"sRGB IEC 61966-2-1 (Linear RGB Profile)";
                }
                else if (transferCharacteristics == heif_transfer_characteristic_IEC_61966_2_1)
                {
                    cmsFloat64Number Parameters[5]
                    {
                        2.4,
                        1 / 1.055,
                        1 - 1 / 1.055,
                        1 / 12.92,
                        12.92 * 0.0031308,
                    };

                    toneCurve.reset(cmsBuildParametricToneCurve(context.get(), 4, Parameters));
                    description = L"sRGB IEC 61966-2-1";
                }
                else if (transferCharacteristics == heif_transfer_characteristic_ITU_R_BT_709_5)
                {
                    cmsFloat64Number Parameters[5]
                    {
                        1.0 / 0.45,
                        1.0 / 1.099296826809442,
                        1.0 - 1 / 1.099296826809442,
                        1.0 / 4.5,
                        4.5 * 0.018053968510807,
                    };

                    toneCurve.reset(cmsBuildParametricToneCurve(context.get(), 4, Parameters));
                    description = L"Rec. 709";
                }

                if (toneCurve && description != nullptr)
                {
                    profile = BuildRGBProfile(
                        context.get(),
                        &whitepoint,
                        &rgbPrimaries,
                        toneCurve.get(),
                        description);
                }
            }

            if (profile)
            {
                SetCICPTag(profile.get(), primaries, transferCharacteristics, matrixCoefficients, fullRange);
                SaveColorProfileToHandle(profile.get(), formatRecord);
            }
        }
    }
}
