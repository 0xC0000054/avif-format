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
 //
 // Portions of this file has been adapted from libavif, https://github.com/AOMediaCodec/libavif
 /*
     Copyright 2019 Joe Drago. All rights reserved.

     Redistribution and use in source and binary forms, with or without
     modification, are permitted provided that the following conditions are met:

     1. Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
     AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
     FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
     SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
     CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
     OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
     OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "YUVCoefficiants.h"
#include <memory>

namespace
{
    struct avifColourPrimariesTable
    {
        heif_color_primaries colourPrimariesEnum;
        const char* name;
        float primaries[8]; // rX, rY, gX, gY, bX, bY, wX, wY
    };
    static const struct avifColourPrimariesTable avifColourPrimariesTables[] = {
        { heif_color_primaries_ITU_R_BT_709_5, "BT.709", { 0.64f, 0.33f, 0.3f, 0.6f, 0.15f, 0.06f, 0.3127f, 0.329f } },
        { heif_color_primaries_ITU_R_BT_470_6_System_M, "BT.470-6 System M", { 0.67f, 0.33f, 0.21f, 0.71f, 0.14f, 0.08f, 0.310f, 0.316f } },
        { heif_color_primaries_ITU_R_BT_470_6_System_B_G, "BT.470-6 System BG", { 0.64f, 0.33f, 0.29f, 0.60f, 0.15f, 0.06f, 0.3127f, 0.3290f } },
        { heif_color_primaries_ITU_R_BT_601_6, "BT.601", { 0.630f, 0.340f, 0.310f, 0.595f, 0.155f, 0.070f, 0.3127f, 0.3290f } },
        { heif_color_primaries_SMPTE_240M, "SMPTE 240M", { 0.630f, 0.340f, 0.310f, 0.595f, 0.155f, 0.070f, 0.3127f, 0.3290f } },
        { heif_color_primaries_generic_film, "Generic film", { 0.681f, 0.319f, 0.243f, 0.692f, 0.145f, 0.049f, 0.310f, 0.316f } },
        { heif_color_primaries_ITU_R_BT_2020_2_and_2100_0, "BT.2020", { 0.708f, 0.292f, 0.170f, 0.797f, 0.131f, 0.046f, 0.3127f, 0.3290f } },
        { heif_color_primaries_SMPTE_ST_428_1, "SMPTE ST 428-1", { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.3333f, 0.3333f } },
        { heif_color_primaries_SMPTE_RP_431_2, "SMPTE RP 431-2", { 0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f, 0.314f, 0.351f } },
        { heif_color_primaries_SMPTE_EG_432_1, "SMPTE EG 432-1 (DCI P3)", { 0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f, 0.3127f, 0.3290f } },
        { heif_color_primaries_EBU_Tech_3213_E, "EBU Tech. 3213-E", { 0.630f, 0.340f, 0.295f, 0.605f, 0.155f, 0.077f, 0.3127f, 0.3290f } }
    };
    static const int avifColourPrimariesTableSize = sizeof(avifColourPrimariesTables) / sizeof(avifColourPrimariesTables[0]);

    void avifNclxColourPrimariesGetValues(heif_color_primaries ancp, float outPrimaries[8]) {
        for (int i = 0; i < avifColourPrimariesTableSize; ++i) {
            if (avifColourPrimariesTables[i].colourPrimariesEnum == ancp) {
                memcpy(outPrimaries, avifColourPrimariesTables[i].primaries, sizeof(avifColourPrimariesTables[i].primaries));
                return;
            }
        }

        // if we get here, the color primaries are unknown. Just return a reasonable default.
        memcpy(outPrimaries, avifColourPrimariesTables[0].primaries, sizeof(avifColourPrimariesTables[0].primaries));
    }

    struct avifMatrixCoefficientsTable
    {
        heif_matrix_coefficients matrixCoefficientsEnum;
        const char* name;
        const float kr;
        const float kb;
    };

    // https://www.itu.int/rec/T-REC-H.273-201612-I/en
    static const struct avifMatrixCoefficientsTable matrixCoefficientsTables[] = {
        //{ AVIF_MATRIX_COEFFICIENTS_IDENTITY, "Identity", 0.0f, 0.0f, }, // Handled elsewhere
        { heif_matrix_coefficients_ITU_R_BT_709_5, "BT.709", 0.2126f, 0.0722f },
        { heif_matrix_coefficients_US_FCC_T47, "FCC USFC 73.682", 0.30f, 0.11f },
        { heif_matrix_coefficients_ITU_R_BT_470_6_System_B_G, "BT.470-6 System BG", 0.299f, 0.114f },
        { heif_matrix_coefficients_ITU_R_BT_601_6, "BT.601", 0.299f, 0.114f },
        { heif_matrix_coefficients_SMPTE_240M, "SMPTE ST 240", 0.212f, 0.087f },
        { heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance, "BT.2020 (non-constant luminance)", 0.2627f, 0.0593f },
        //{ AVIF_MATRIX_COEFFICIENTS_BT2020_CL, "BT.2020 (constant luminance)", 0.2627f, 0.0593f }, // FIXME: It is not an linear transformation.
        //{ AVIF_MATRIX_COEFFICIENTS_SMPTE2085, "ST 2085", 0.0f, 0.0f }, // FIXME: ST2085 can't represent using Kr and Kb.
        //{ AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL, "Chromaticity-derived constant luminance system", 0.0f, 0.0f } // FIXME: It is not an linear transformation.
        //{ AVIF_MATRIX_COEFFICIENTS_ICTCP, "BT.2100-0 ICtCp", 0.0f, 0.0f }, // FIXME: This can't represent using Kr and Kb.
    };

    static const int avifMatrixCoefficientsTableSize = sizeof(matrixCoefficientsTables) / sizeof(matrixCoefficientsTables[0]);

    bool calcYUVInfoFromCICP(const heif_color_profile_nclx* cicp, float coeffs[3]) {
        if (cicp) {
            if (cicp->matrix_coefficients == heif_matrix_coefficients_chromaticity_derived_non_constant_luminance) {
                float primaries[8];
                avifNclxColourPrimariesGetValues(cicp->color_primaries, primaries);
                float const rX = primaries[0];
                float const rY = primaries[1];
                float const gX = primaries[2];
                float const gY = primaries[3];
                float const bX = primaries[4];
                float const bY = primaries[5];
                float const wX = primaries[6];
                float const wY = primaries[7];
                float const rZ = 1.0f - (rX + rY); // (Eq. 34)
                float const gZ = 1.0f - (gX + gY); // (Eq. 35)
                float const bZ = 1.0f - (bX + bY); // (Eq. 36)
                float const wZ = 1.0f - (wX + wY); // (Eq. 37)
                float const kr = (rY * (wX * (gY * bZ - bY * gZ) + wY * (bX * gZ - gX * bZ) + wZ * (gX * bY - bX * gY))) /
                    (wY * (rX * (gY * bZ - bY * gZ) + gX * (bY * rZ - rY * bZ) + bX * (rY * gZ - gY * rZ)));
                // (Eq. 32)
                float const kb = (bY * (wX * (rY * gZ - gY * rZ) + wY * (gX * rZ - rX * gZ) + wZ * (rX * gY - gX * rY))) /
                    (wY * (rX * (gY * bZ - bY * gZ) + gX * (bY * rZ - rY * bZ) + bX * (rY * gZ - gY * rZ)));
                // (Eq. 33)
                coeffs[0] = kr;
                coeffs[2] = kb;
                coeffs[1] = 1.0f - coeffs[0] - coeffs[2];
                return true;
            }
            else {
                for (int i = 0; i < avifMatrixCoefficientsTableSize; ++i) {
                    const struct avifMatrixCoefficientsTable* const table = &matrixCoefficientsTables[i];
                    if (table->matrixCoefficientsEnum == cicp->matrix_coefficients) {
                        coeffs[0] = table->kr;
                        coeffs[2] = table->kb;
                        coeffs[1] = 1.0f - coeffs[0] - coeffs[2];
                        return true;
                    }
                }
            }
        }
        return false;
    }
}

void GetYUVCoefficiants(const heif_color_profile_nclx* colorInfo, YUVCoefficiants& yuvData)
{
    // (As of ISO/IEC 23000-22:2019 Amendment 2)
    // MIAF Section 7.3.6.4 "Colour information property":
    //
    // If a coded image has no associated colour property, the default property is defined as having
    // colour_type equal to 'nclx' with properties as follows:
    // –   colour_primaries equal to 1,
    // –   transfer_characteristics equal to 13,
    // –   matrix_coefficients equal to 5 or 6 (which are functionally identical), and
    // –   full_range_flag equal to 1.
    // Only if the colour information property of the image matches these default values, the colour
    // property may be omitted; all other images shall have an explicitly declared colour space via
    // association with a property of this type.
    //
    // See here for the discussion: https://github.com/AOMediaCodec/av1-avif/issues/77#issuecomment-676526097

    // matrix_coefficients of [5,6] == BT.601:
    float kr = 0.299f;
    float kb = 0.114f;
    float kg = 1.0f - kr - kb;

    float coeffs[3];

    if (calcYUVInfoFromCICP(colorInfo, coeffs))
    {
        kr = coeffs[0];
        kg = coeffs[1];
        kb = coeffs[2];
    }

    yuvData.kr = kr;
    yuvData.kg = kg;
    yuvData.kb = kb;
}
