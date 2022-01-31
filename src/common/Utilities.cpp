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

#include "AvifFormat.h"

namespace
{
    // Define the required suite versions and minimum callback routine counts here.
    // This allows the plug-in to work in 3rd party hosts that do not have access to the post-6.0 Photoshop SDKs.

    constexpr int16 RequiredBufferProcsVersion = 2;
    constexpr int16 RequiredBufferProcsCount = 5;

    constexpr int16 RequiredDescriptorParametersVerson = 0;
    constexpr int16 RequiredReadDescriptorProcsVersion = 0;
    constexpr int16 RequiredReadDescriptorProcsCount = 18;
    constexpr int16 RequiredWriteDescriptorProcsVersion = 0;
    constexpr int16 RequiredWriteDescriptorProcsCount = 16;

    constexpr int16 RequiredHandleProcsVersion = 1;
    constexpr int16 RequiredHandleProcsCount = 6;

    constexpr int16 RequiredPropertyProcsVersion = 1;
    constexpr int16 RequiredPropertyProcsCount = 2;

    // Adapted from PIUtilities.cpp in the Photoshop 6 SDK

    //-------------------------------------------------------------------------------
    //
    //	HostBufferProcsAvailable
    //
    //	Determines whether the BufferProcs callback is available.
    //
    //	Inputs:
    //		BufferProcs* procs	Pointer to BufferProcs structure.
    //
    //	Outputs:
    //
    //		returns TRUE				If the BufferProcs callback is available.
    //		returns FALSE				If the BufferProcs callback is absent.
    //
    //-------------------------------------------------------------------------------

    bool HostBufferProcsAvailable(const BufferProcs* procs)
    {
#if DEBUG_BUILD
        if (procs != nullptr)
        {
            DebugOut("bufferProcsVersion=%d numBufferProcs=%d allocateProc=%p lockProc=%p unlockProc=%p freeProc=%p spaceProc=%p",
                procs->bufferProcsVersion,
                procs->numBufferProcs,
                procs->allocateProc,
                procs->freeProc,
                procs->lockProc,
                procs->unlockProc,
                procs->spaceProc);
        }
        else
        {
            DebugOut("BufferProcs == nullptr");
        }
#endif // DEBUG_BUILD

        bool available = true;		// assume docInfo are available

        // We want to check for this stuff in a logical order, going from things
        // that should always be present to things that "may" be present.  It's
        // always a danger checking things that "may" be present because some
        // hosts may not leave them NULL if unavailable, instead pointing to
        // other structures to save space.  So first we'll check the main
        // proc pointer, then the version, the number of routines, then some
        // of the actual routines:

        if (procs == nullptr)
        {
            available = false;
        }
        else if (procs->bufferProcsVersion != RequiredBufferProcsVersion)
        {
            available = false;
        }
        else if (procs->numBufferProcs < RequiredBufferProcsCount)
        {
            available = false;
        }
        else if (procs->allocateProc == nullptr ||
            procs->lockProc == nullptr ||
            procs->unlockProc == nullptr ||
            procs->freeProc == nullptr ||
            procs->spaceProc == nullptr)
        {
            available = false;
        }

        return available;

    } // end HostBufferProcsAvailable

    //-------------------------------------------------------------------------------
    //
    //	HostDescriptorAvailable
    //
    //	Determines whether the PIDescriptorParameters callback is available.
    //
    //	The Descriptor Parameters suite are callbacks designed for
    //	scripting and automation.  See PIActions.h.
    //
    //	Inputs:
    //		PIDescriptorParameters* procs	Pointer to Descriptor Parameters suite.
    //
    //	Outputs:
    //
    //		returns TRUE					If PIDescriptorParameters is available.
    //		returns FALSE					If PIDescriptorParameters is absent.
    //
    //-------------------------------------------------------------------------------

    static Boolean HostDescriptorAvailable(const PIDescriptorParameters* procs)
    {

        Boolean available = TRUE;		// assume procs are available
        Boolean newerVersion = FALSE;	// assume we're running under correct version

                                        // We want to check for this stuff in a logical order, going from things
                                        // that should always be present to things that "may" be present.  It's
                                        // always a danger checking things that "may" be present because some
                                        // hosts may not leave them nullptr if unavailable, instead pointing to
                                        // other structures to save space.  So first we'll check the main
                                        // proc pointer, then the version, the number of routines, then some
                                        // of the actual routines:

        if (procs == nullptr)
        {
            available = FALSE;
        }
        else if (procs->descriptorParametersVersion < RequiredDescriptorParametersVerson)
        {
            available = FALSE;
        }
        else if (procs->descriptorParametersVersion > RequiredDescriptorParametersVerson)
        {
            available = FALSE;
            newerVersion = TRUE;
        }
        else if (procs->readDescriptorProcs == nullptr || procs->writeDescriptorProcs == nullptr)
        {
            available = FALSE;
        }
        else if (procs->readDescriptorProcs->readDescriptorProcsVersion < RequiredReadDescriptorProcsVersion)
        {
            available = FALSE;
        }
        else if (procs->readDescriptorProcs->readDescriptorProcsVersion > RequiredReadDescriptorProcsVersion)
        {
            available = FALSE;
            newerVersion = TRUE;
        }
        else if (procs->readDescriptorProcs->numReadDescriptorProcs < RequiredReadDescriptorProcsCount)
        {
            available = FALSE;
        }
        else if (procs->writeDescriptorProcs->writeDescriptorProcsVersion < RequiredWriteDescriptorProcsVersion)
        {
            available = FALSE;
        }
        else if (procs->writeDescriptorProcs->writeDescriptorProcsVersion > RequiredWriteDescriptorProcsVersion)
        {
            available = FALSE;
            newerVersion = TRUE;
        }
        else if (procs->writeDescriptorProcs->numWriteDescriptorProcs < RequiredWriteDescriptorProcsCount)
        {
            available = FALSE;
        }
        else if (procs->readDescriptorProcs->openReadDescriptorProc == nullptr ||
                 procs->readDescriptorProcs->closeReadDescriptorProc == nullptr ||
                 procs->readDescriptorProcs->getKeyProc == nullptr ||
                 procs->readDescriptorProcs->getIntegerProc == nullptr ||
                 procs->readDescriptorProcs->getFloatProc == nullptr ||
                 procs->readDescriptorProcs->getUnitFloatProc == nullptr ||
                 procs->readDescriptorProcs->getBooleanProc == nullptr ||
                 procs->readDescriptorProcs->getTextProc == nullptr ||
                 procs->readDescriptorProcs->getAliasProc == nullptr ||
                 procs->readDescriptorProcs->getEnumeratedProc == nullptr ||
                 procs->readDescriptorProcs->getClassProc == nullptr ||
                 procs->readDescriptorProcs->getSimpleReferenceProc == nullptr ||
                 procs->readDescriptorProcs->getObjectProc == nullptr ||
                 procs->readDescriptorProcs->getCountProc == nullptr ||
                 procs->readDescriptorProcs->getStringProc == nullptr ||
                 procs->readDescriptorProcs->getPinnedIntegerProc == nullptr ||
                 procs->readDescriptorProcs->getPinnedFloatProc == nullptr ||
                 procs->readDescriptorProcs->getPinnedUnitFloatProc == nullptr)
        {
            available = FALSE;
        }
        else if (procs->writeDescriptorProcs->openWriteDescriptorProc == nullptr ||
                 procs->writeDescriptorProcs->closeWriteDescriptorProc == nullptr ||
                 procs->writeDescriptorProcs->putIntegerProc == nullptr ||
                 procs->writeDescriptorProcs->putFloatProc == nullptr ||
                 procs->writeDescriptorProcs->putUnitFloatProc == nullptr ||
                 procs->writeDescriptorProcs->putBooleanProc == nullptr ||
                 procs->writeDescriptorProcs->putTextProc == nullptr ||
                 procs->writeDescriptorProcs->putAliasProc == nullptr ||
                 procs->writeDescriptorProcs->putEnumeratedProc == nullptr ||
                 procs->writeDescriptorProcs->putClassProc == nullptr ||
                 procs->writeDescriptorProcs->putSimpleReferenceProc == nullptr ||
                 procs->writeDescriptorProcs->putObjectProc == nullptr ||
                 procs->writeDescriptorProcs->putCountProc == nullptr ||
                 procs->writeDescriptorProcs->putStringProc == nullptr ||
                 procs->writeDescriptorProcs->putScopedClassProc == nullptr ||
                 procs->writeDescriptorProcs->putScopedObjectProc == nullptr)
        {
            available = FALSE;
        }

        return available;

    } // end HostDescriptorAvailable

    //-------------------------------------------------------------------------------
    //
    //	HostHandleProcsAvailable
    //
    //	Determines whether the HandleProcs callback is available.
    //
    //	The HandleProcs are cross-platform master pointers that point to
    //	pointers that point to data that is allocated in the Photoshop
    //	virtual memory structure.  They're reference counted and
    //	managed more efficiently than the operating system calls.
    //
    //	WARNING:  Do not mix operating system handle creation, deletion,
    //			  and sizing routines with these callback routines.  They
    //			  operate differently, allocate memory differently, and,
    //			  while you won't crash, you can cause memory to be
    //			  allocated on the global heap and never deallocated.
    //
    //	Inputs:
    //		HandeProcs* procs	Pointer to HandleProcs structure.
    //
    //	Outputs:
    //
    //		returns TRUE				If the HandleProcs callback is available.
    //		returns FALSE				If the HandleProcs callback is absent.
    //
    //-------------------------------------------------------------------------------

    bool HostHandleProcsAvailable(const HandleProcs* procs) noexcept
    {
#if DEBUG_BUILD
        if (procs != nullptr)
        {
            DebugOut("handleProcsVersion=%d numHandleProcs=%d newProc=%p disposeProc=%p getSizeProc=%p setSizeProc=%p lockProc=%p unlockProc=%p",
                procs->handleProcsVersion,
                procs->numHandleProcs,
                procs->newProc,
                procs->disposeProc,
                procs->getSizeProc,
                procs->setSizeProc,
                procs->lockProc,
                procs->unlockProc);
        }
        else
        {
            DebugOut("HandleProcs == nullptr");
        }
#endif // DEBUG_BUILD

        Boolean available = true;		// assume docInfo are available

        // We want to check for this stuff in a logical order, going from things
        // that should always be present to things that "may" be present.  It's
        // always a danger checking things that "may" be present because some
        // hosts may not leave them NULL if unavailable, instead pointing to
        // other structures to save space.  So first we'll check the main
        // proc pointer, then the version, the number of routines, then some
        // of the actual routines:

        if (procs == nullptr)
        {
            available = false;
        }
        else if (procs->handleProcsVersion != RequiredHandleProcsVersion)
        {
            available = false;
        }
        else if (procs->numHandleProcs < RequiredHandleProcsCount)
        {
            available = false;
        }
        else if (procs->newProc == nullptr ||
                 procs->disposeProc == nullptr ||
                 procs->getSizeProc == nullptr ||
                 procs->setSizeProc == nullptr ||
                 procs->lockProc == nullptr ||
                 procs->unlockProc == nullptr)
        {
            available = false;
        }

        return available;

    } // end HostHandleProcsAvailable

    //-------------------------------------------------------------------------------
    //
    //	HostPropertyProcsAvailable
    //
    //	Determines whether the Property suite of callbacks is available.
    //
    //	The Property suite callbacks are two callbacks, GetProperty and
    //	SetProperty, that manage a list of different data elements.  See
    //	PIProperties.h.
    //
    //	Inputs:
    //		PropertyProcs* procs	Pointer to PropertyProcs structure.
    //
    //	Outputs:
    //
    //		returns TRUE				If the Property suite is available.
    //		returns FALSE				If the Property suite is absent.
    //
    //-------------------------------------------------------------------------------

    bool HostPropertyProcsAvailable(const PropertyProcs* procs) noexcept
    {
#if DEBUG_BUILD
        if (procs != nullptr)
        {
            DebugOut("propertyProcsVersion=%d numPropertyProcs=%d getPropertyProc=%p setPropertyProc=%p",
                procs->propertyProcsVersion,
                procs->numPropertyProcs,
                procs->getPropertyProc,
                procs->setPropertyProc);
        }
        else
        {
            DebugOut("PropertyProcs == nullptr");
        }
#endif // DEBUG_BUILD

        bool available = true;

        if (procs == nullptr)
        {
            available = false;
        }
        else if (procs->propertyProcsVersion != RequiredPropertyProcsVersion)
        {
            available = false;
        }
        else if (procs->numPropertyProcs < RequiredPropertyProcsCount)
        {
            available = false;
        }
        else if (procs->getPropertyProc == nullptr ||
                 procs->setPropertyProc == nullptr)
        {
            available = false;
        }

        return available;
    }
}

bool DescriptorSuiteIsAvailable(const FormatRecordPtr formatRecord)
{
    static bool descriptorSuiteAvailable = HostDescriptorAvailable(formatRecord->descriptorParameters);

    return descriptorSuiteAvailable;
}

bool HandleSuiteIsAvailable(const FormatRecordPtr formatRecord)
{
    static bool handleSuiteAvailable = HostHandleProcsAvailable(formatRecord->handleProcs);

    return handleSuiteAvailable;
}

bool HostImageModeSupported(const FormatRecordPtr formatRecord)
{
    switch (formatRecord->imageMode)
    {
    case plugInModeRGBColor:
    case plugInModeRGB48:
        return (formatRecord->depth == 8 || formatRecord->depth == 16);
    default:
        return false;
    }
}

bool HostSupportsRequiredFeatures(const FormatRecordPtr formatRecord)
{
    return formatRecord->advanceState != nullptr && HostBufferProcsAvailable(formatRecord->bufferProcs);
}

bool PropertySuiteIsAvailable(const FormatRecordPtr formatRecord)
{
    static bool propertySuiteAvailable = HostPropertyProcsAvailable(formatRecord->propertyProcs);

    return propertySuiteAvailable;
}

