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

#ifndef SCOPEDHEIF_H
#define SCOPEDHEIF_H

#include "libheif/heif.h"
#include <memory>

namespace detail
{
    struct context_deleter { void operator()(heif_context* h) noexcept { if (h) heif_context_free(h); } };

    struct encoder_deleter { void operator()(heif_encoder* h) noexcept { if (h) heif_encoder_release(h); } };

    struct encoding_options_deleter { void operator()(heif_encoding_options* h) noexcept { if (h) heif_encoding_options_free(h); } };

    struct image_handle_deleter { void operator()(heif_image_handle* h) noexcept { if (h) heif_image_handle_release(h); } };

    struct image_deleter { void operator()(heif_image* h) noexcept { if (h) heif_image_release(h); } };

    struct nclx_profile_deleter { void operator()(heif_color_profile_nclx* h) noexcept { if (h) heif_nclx_color_profile_free(h); } };
}

using ScopedHeifContext = std::unique_ptr<heif_context, detail::context_deleter>;

using ScopedHeifEncoder = std::unique_ptr<heif_encoder, detail::encoder_deleter>;

using ScopedHeifEncodingOptions = std::unique_ptr<heif_encoding_options, detail::encoding_options_deleter>;

using ScopedHeifImageHandle = std::unique_ptr<heif_image_handle, detail::image_handle_deleter>;

using ScopedHeifImage = std::unique_ptr<heif_image, detail::image_deleter>;

using ScopedHeifNclxProfile = std::unique_ptr<heif_color_profile_nclx, detail::nclx_profile_deleter>;

#endif
