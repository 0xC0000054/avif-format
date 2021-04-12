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

#include "AvifFormat.h"

OSErr DoOptionsPrepare(FormatRecordPtr formatRecord)
{
    PrintFunctionName();

    OSErr err = noErr;

    if (!HostSupportsRequiredFeatures(formatRecord))
    {
        err = errPlugInHostInsufficient;
    }
    else if (!HostImageModeSupported(formatRecord))
    {
        err = formatBadParameters;
    }

    if (err == noErr)
    {
        formatRecord->maxData = 0;
    }

    return err;
}

OSErr DoOptionsStart(FormatRecordPtr formatRecord, Globals* globals)
{
    PrintFunctionName();

    formatRecord->data = nullptr;
    SetRect(formatRecord, 0, 0, 0, 0);

    Boolean showDialog;
    ReadScriptParamsOnWrite(formatRecord, globals->saveOptions, &showDialog);

    OSErr err = noErr;

    if (showDialog)
    {
        if (DoSaveUI(formatRecord, globals->saveOptions))
        {
            WriteScriptParamsOnWrite(formatRecord, globals->saveOptions);
        }
        else
        {
            err = userCanceledErr;
        }
    }

    return err;
}

OSErr DoOptionsContinue()
{
    PrintFunctionName();

    return noErr;
}

OSErr DoOptionsFinish()
{
    PrintFunctionName();

    return noErr;
}
