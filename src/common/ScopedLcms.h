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

#ifndef SCOPEDLCMS_H
#define SCOPEDLCMS_H

#include "lcms2.h"
#include <memory>

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

struct ScopedLcmsTransform
{
    ScopedLcmsTransform() : transform(nullptr)
    {
    }

    ScopedLcmsTransform(cmsHTRANSFORM transform) : transform(transform)
    {
    }

    ScopedLcmsTransform(ScopedLcmsTransform&& other) noexcept
    {
        transform = other.transform;
        other.transform = nullptr;
    }

    ScopedLcmsTransform& operator=(ScopedLcmsTransform&& other) noexcept
    {
        transform = other.transform;
        other.transform = nullptr;

        return *this;
    }

    ScopedLcmsTransform(const ScopedLcmsTransform&) = delete;
    ScopedLcmsTransform& operator=(const ScopedLcmsTransform&) = delete;

    ~ScopedLcmsTransform()
    {
        reset();
    }

    cmsHTRANSFORM get() const noexcept
    {
        return transform;
    }

    void reset()
    {
        if (transform != nullptr)
        {
            cmsDeleteTransform(transform);
            transform = nullptr;
        }
    }

    void reset(cmsHTRANSFORM newTransform)
    {
        reset();

        transform = newTransform;
    }

    explicit operator bool() const noexcept
    {
        return transform != nullptr;
    }

private:

    cmsHTRANSFORM transform;
};

#endif // !SCOPEDLCMS_H
