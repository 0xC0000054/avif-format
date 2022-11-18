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
 // Portions of this file has been adapted from openexrAE
 // These portions are licensed under the 2-Clause BSD License

#include "ColorProfileDetection.h"
#include "lcms2_plugin.h"
#include "libheif/heif.h"
#include <string>
#include <utility>
#include <vector>

namespace
{
    bool ReadColorantTag(cmsHPROFILE profile, cmsTagSignature tag, cmsCIEXYZ& result)
    {
        const cmsCIEXYZ* const profileTag = static_cast<cmsCIEXYZ*>(cmsReadTag(profile, tag));

        if (profileTag != nullptr)
        {
            result.X = profileTag->X;
            result.Y = profileTag->Y;
            result.Z = profileTag->Z;

            return true;
        }

        return false;
    }

    bool ReadMediaWhitePoint(cmsHPROFILE hProfile, cmsCIEXYZ& result)
    {
        cmsCIEXYZ* Tag;

        Tag = (cmsCIEXYZ*)cmsReadTag(hProfile, cmsSigMediaWhitePointTag);

        // If no wp, take D50
        if (Tag == nullptr)
        {
            result = *cmsD50_XYZ();
            return true;
        }

        // V2 display profiles should give D50
        if (cmsGetEncodedICCversion(hProfile) < 0x4000000)
        {
            if (cmsGetDeviceClass(hProfile) == cmsSigDisplayClass)
            {
                result = *cmsD50_XYZ();
                return true;
            }
        }

        // All seems ok
        result = *Tag;
        return true;
    }

    bool ComputeChromaticAdaptation(cmsMAT3* Conversion,
        const cmsCIEXYZ* SourceWhitePoint,
        const cmsCIEXYZ* DestWhitePoint,
        const cmsMAT3* Chad)
    {

        cmsMAT3 Chad_Inv{};
        cmsVEC3 ConeSourceXYZ{}, ConeSourceRGB{};
        cmsVEC3 ConeDestXYZ{}, ConeDestRGB{};
        cmsMAT3 Cone{}, Tmp{};


        Tmp = *Chad;
        if (!_cmsMAT3inverse(&Tmp, &Chad_Inv)) return FALSE;

        _cmsVEC3init(&ConeSourceXYZ, SourceWhitePoint->X,
            SourceWhitePoint->Y,
            SourceWhitePoint->Z);

        _cmsVEC3init(&ConeDestXYZ, DestWhitePoint->X,
            DestWhitePoint->Y,
            DestWhitePoint->Z);

        _cmsMAT3eval(&ConeSourceRGB, Chad, &ConeSourceXYZ);
        _cmsMAT3eval(&ConeDestRGB, Chad, &ConeDestXYZ);

        // Build matrix
        _cmsVEC3init(&Cone.v[0], ConeDestRGB.n[0] / ConeSourceRGB.n[0], 0.0, 0.0);
        _cmsVEC3init(&Cone.v[1], 0.0, ConeDestRGB.n[1] / ConeSourceRGB.n[1], 0.0);
        _cmsVEC3init(&Cone.v[2], 0.0, 0.0, ConeDestRGB.n[2] / ConeSourceRGB.n[2]);


        // Normalize
        _cmsMAT3per(&Tmp, &Cone, Chad);
        _cmsMAT3per(Conversion, &Chad_Inv, &Tmp);

        return TRUE;
    }

    bool AdaptationMatrix(cmsMAT3* r, const cmsMAT3* ConeMatrix, const cmsCIEXYZ* FromIll, const cmsCIEXYZ* ToIll)
    {
        cmsMAT3 LamRigg = { { // Bradford matrix
            {{  0.8951,  0.2664, -0.1614 }},
            {{ -0.7502,  1.7135,  0.0367 }},
            {{  0.0389, -0.0685,  1.0296 }}
        } };

        if (ConeMatrix == nullptr)
        {
            ConeMatrix = &LamRigg;
        }

        return ComputeChromaticAdaptation(r, FromIll, ToIll, ConeMatrix);
    }

    bool AdaptMatrixFromD50(cmsMAT3* r, cmsCIExyY* DestWhitePt)
    {
        cmsCIEXYZ Dn;
        cmsMAT3 Bradford;
        cmsMAT3 Tmp;

        cmsxyY2XYZ(&Dn, DestWhitePt);

        AdaptationMatrix(&Bradford, NULL, cmsD50_XYZ(), &Dn);

        Tmp = *r;
        _cmsMAT3per(r, &Bradford, &Tmp);

        return TRUE;
    }

    bool CompareXYValues(const cmsCIExyY& a, const cmsCIExyY& b, cmsFloat64Number tolerance) noexcept
    {
        return fabs(a.x - b.x) < tolerance && fabs(a.y - b.y) < tolerance;
    }

    bool ProfileHasColorantsAndWhitepoint(
        cmsHPROFILE profile,
        const cmsCIExyY& requiredWhitepoint,
        const cmsCIExyYTRIPLE& requiredColorants,
        const cmsFloat64Number& tolerance)
    {
        bool result = false;

        if (cmsGetColorSpace(profile) == cmsSigRgbData)
        {
            cmsCIEXYZTRIPLE colorants{};
            cmsCIEXYZ whitepoint{};

            if (ReadColorantTag(profile, cmsSigRedColorantTag, colorants.Red) &&
                ReadColorantTag(profile, cmsSigGreenColorantTag, colorants.Green) &&
                ReadColorantTag(profile, cmsSigBlueColorantTag, colorants.Blue) &&
                ReadMediaWhitePoint(profile, whitepoint))
            {
                cmsCIExyYTRIPLE xyColorants{};
                cmsCIExyY xyWhitePoint{};

                // make a big matrix so we can run cmsAdaptMatrixFromD50() on it
                // before grabbing the chromaticities
                cmsMAT3 MColorants{};

                MColorants.v[0].n[0] = colorants.Red.X;
                MColorants.v[1].n[0] = colorants.Red.Y;
                MColorants.v[2].n[0] = colorants.Red.Z;

                MColorants.v[0].n[1] = colorants.Green.X;
                MColorants.v[1].n[1] = colorants.Green.Y;
                MColorants.v[2].n[1] = colorants.Green.Z;

                MColorants.v[0].n[2] = colorants.Blue.X;
                MColorants.v[1].n[2] = colorants.Blue.Y;
                MColorants.v[2].n[2] = colorants.Blue.Z;


                cmsXYZ2xyY(&xyWhitePoint, &whitepoint);

                // apparently I have to do this to get the right YXZ values
                // for my chromaticities - anyone know why?
                AdaptMatrixFromD50(&MColorants, &xyWhitePoint);

                // set the colorants back and convert to xyY
                colorants.Red.X = MColorants.v[0].n[0];
                colorants.Red.Y = MColorants.v[1].n[0];
                colorants.Red.Z = MColorants.v[2].n[0];

                colorants.Green.X = MColorants.v[0].n[1];
                colorants.Green.Y = MColorants.v[1].n[1];
                colorants.Green.Z = MColorants.v[2].n[1];

                colorants.Blue.X = MColorants.v[0].n[2];
                colorants.Blue.Y = MColorants.v[1].n[2];
                colorants.Blue.Z = MColorants.v[2].n[2];

                cmsXYZ2xyY(&xyColorants.Red, &colorants.Red);
                cmsXYZ2xyY(&xyColorants.Green, &colorants.Green);
                cmsXYZ2xyY(&xyColorants.Blue, &colorants.Blue);

                const bool whitepointMatches = CompareXYValues(xyWhitePoint, requiredWhitepoint, tolerance);
                const bool redMatches = CompareXYValues(xyColorants.Red, requiredColorants.Red, tolerance);
                const bool greenMatches = CompareXYValues(xyColorants.Green, requiredColorants.Green, tolerance);
                const bool blueMatches = CompareXYValues(xyColorants.Blue, requiredColorants.Blue, tolerance);

                if (whitepointMatches && redMatches && greenMatches && blueMatches)
                {
                    result = true;
                }
            }
        }

        return result;
    }

    bool ProfileHasRec2020ColorantsAndWhitepoint(cmsHPROFILE profile)
    {
        const cmsCIExyY whitepointD65 = { 0.3127, 0.3290, 1.0f }; // D65
        const cmsCIExyYTRIPLE rec2020Primaries =
        {
            { 0.708, 0.292, 1.0 },
            { 0.170, 0.797, 1.0 },
            { 0.131, 0.046, 1.0 }
        };

        return ProfileHasColorantsAndWhitepoint(profile, whitepointD65, rec2020Primaries, 0.01);
    }

    bool ProfileHasSRGBColorantsAndWhitepoint(cmsHPROFILE profile)
    {
        const cmsCIExyY whitepointD65 = { 0.3127, 0.3290, 1.0f }; // D65
        const cmsCIExyYTRIPLE srgbPrimaries =
        {
            { 0.6400, 0.3300, 1.0 },
            { 0.3000, 0.6000, 1.0 },
            { 0.1500, 0.0600, 1.0 }
        };

        return ProfileHasColorantsAndWhitepoint(profile, whitepointD65, srgbPrimaries, 0.01);
    }

    constexpr std::pair<const wchar_t*, size_t> MakeDescriptionEntry(const wchar_t* text)
    {
        return std::make_pair(text, std::char_traits<wchar_t>::length(text));
    }

    bool ProfileHasRec2020Description(cmsHPROFILE profile)
    {
        const std::vector<std::pair<const wchar_t*, size_t>> entries =
        {
            // This name should cover all of Elle Stone's Rec. 2020 ICC profiles.
            // These profiles use the file name as the profile description, e.g. Rec2020-elle-V4-g10.icc.
            MakeDescriptionEntry(L"Rec2020-elle-V"),
            // The ICC profiles generated by colorist use this.
            MakeDescriptionEntry(L"Colorist BT. 2020"),
            // The beta Rec. 2020 profile from the International Color Consortium
            MakeDescriptionEntry(L"ITU-R BT. 2020 Reference Display")
        };

        constexpr cmsUInt32Number descriptionBufferSize = 256;
        wchar_t descriptionBuffer[descriptionBufferSize]{};

        const size_t charsWritten = cmsGetProfileInfo(
            profile,
            cmsInfoDescription,
            "en",
            "US",
            descriptionBuffer,
            descriptionBufferSize - 1) / sizeof(wchar_t);

        if (charsWritten > 0)
        {
            for (const auto& entry : entries)
            {
                if (charsWritten >= entry.second)
                {
                    if (std::wcsncmp(descriptionBuffer, entry.first, entry.second) == 0)
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool ProfileHasSRGBDescription(cmsHPROFILE profile)
    {
        constexpr cmsUInt32Number descriptionBufferSize = 256;
        wchar_t descriptionBuffer[descriptionBufferSize]{};

        const size_t charsWritten = cmsGetProfileInfo(
            profile,
            cmsInfoDescription,
            "en",
            "US",
            descriptionBuffer,
            descriptionBufferSize - 1) / sizeof(wchar_t);

        if (charsWritten > 0)
        {
            constexpr const wchar_t* const srgbProfileName = L"sRGB";
            constexpr size_t srgbProfileNameLength = std::char_traits<wchar_t>::length(srgbProfileName);

            if (charsWritten >= srgbProfileNameLength)
            {
                if (std::wcsncmp(descriptionBuffer, srgbProfileName, srgbProfileNameLength) == 0)
                {
                    return true;
                }
            }
        }

        return false;
    }
}

bool IsRec2020ColorProfile(cmsHPROFILE profile)
{
    bool result = false;

    if (profile != nullptr)
    {
        // The CICP tag is checked first as it is the most accurate method.
        if (cmsIsTag(profile, cmsSigcicpTag))
        {
            cmsVideoSignalType* tag = static_cast<cmsVideoSignalType*>(cmsReadTag(profile, cmsSigcicpTag));

            result = tag->ColourPrimaries == static_cast<cmsUInt8Number>(heif_color_primaries_ITU_R_BT_2020_2_and_2100_0);
        }
        else
        {
            result = ProfileHasRec2020Description(profile) || ProfileHasRec2020ColorantsAndWhitepoint(profile);
        }
    }

    return result;
}

bool IsSRGBColorProfile(cmsHPROFILE profile)
{
    bool result = false;

    if (profile != nullptr)
    {
        // The CICP tag is checked first as it is the most accurate method.
        if (cmsIsTag(profile, cmsSigcicpTag))
        {
            cmsVideoSignalType* tag = static_cast<cmsVideoSignalType*>(cmsReadTag(profile, cmsSigcicpTag));

            result = tag->ColourPrimaries == static_cast<cmsUInt8Number>(heif_color_primaries_ITU_R_BT_709_5)
                  && tag->TransferCharacteristics == static_cast<cmsUInt8Number>(heif_transfer_characteristic_IEC_61966_2_1);
        }
        else
        {
            result = ProfileHasSRGBDescription(profile) || ProfileHasSRGBColorantsAndWhitepoint(profile);
        }
    }

    return result;
}
