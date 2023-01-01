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

#ifdef _MSC_VER
#pragma warning(push)
// Disable C4505: unreferenced function with internal linkage has been removed
#pragma warning(disable: 4505)
#endif //  _MSC_VER

#include "AvifFormat.h"
#include "HostMetadata.h"
#include "resource.h"
#include "version.h"
#include <aom/aom.h>
#include <libheif/heif.h>
#include <CommCtrl.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <locale.h>
#include <wchar.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif //  _MSC_VER

namespace
{
    // The CenterDialog function was adapted from WinDialogUtils.cpp in the PS6 SDK.
    /*******************************************************************************/
    /* Centers a dialog template 1/3 of the way down on the main screen */

    void CenterDialog(HWND hDlg)
    {
        int  nHeight;
        int  nWidth;
        int  nTitleBits;
        RECT rcDialog;
        RECT rcParent;
        int  xOrigin;
        int  yOrigin;
        int  xScreen;
        int  yScreen;
        HWND hParent = GetParent(hDlg);

        if (hParent == NULL)
            hParent = GetDesktopWindow();

        GetClientRect(hParent, &rcParent);
        ClientToScreen(hParent, (LPPOINT)&rcParent.left);  // point(left,  top)
        ClientToScreen(hParent, (LPPOINT)&rcParent.right); // point(right, bottom)

        // Center on Title: title bar has system menu, minimize,  maximize bitmaps
        // Width of title bar bitmaps - assumes 3 of them and dialog has a sysmenu
        nTitleBits = GetSystemMetrics(SM_CXSIZE);

        // If dialog has no sys menu compensate for odd# bitmaps by sub 1 bitwidth
        if (!(GetWindowLong(hDlg, GWL_STYLE) & WS_SYSMENU))
            nTitleBits -= nTitleBits / 3;

        GetWindowRect(hDlg, &rcDialog);
        nWidth = rcDialog.right - rcDialog.left;
        nHeight = rcDialog.bottom - rcDialog.top;

        xOrigin = std::max(rcParent.right - rcParent.left - nWidth, 0L) / 2
            + rcParent.left - nTitleBits;
        xScreen = GetSystemMetrics(SM_CXSCREEN);
        if (xOrigin + nWidth > xScreen)
            xOrigin = std::max(0, xScreen - nWidth);

        yOrigin = std::max(rcParent.bottom - rcParent.top - nHeight, 0L) / 3
            + rcParent.top;
        yScreen = GetSystemMetrics(SM_CYSCREEN);
        if (yOrigin + nHeight > yScreen)
            yOrigin = std::max(0, yScreen - nHeight);

        SetWindowPos(hDlg, NULL, xOrigin, yOrigin, nWidth, nHeight, SWP_NOZORDER);
    }


    // __ImageBase is a variable provided by the MSVC linker.
    // See "Accessing the current module’s HINSTANCE from a static library" on Raymond Chen's blog:
    // https://devblogs.microsoft.com/oldnewthing/20041025-00/?p=37483
    EXTERN_C IMAGE_DOS_HEADER __ImageBase;

    inline HINSTANCE GetModuleInstanceHandle()
    {
        return reinterpret_cast<HINSTANCE>(&__ImageBase);
    }

    void InitAboutDialog(HWND hDlg) noexcept
    {
        char s[256]{}, format[256]{};

        GetDlgItemTextA(hDlg, ABOUTFORMAT, format, 256);
        _snprintf_s(s, _countof(s), format, VI_VERSION_STR);
        SetDlgItemTextA(hDlg, ABOUTFORMAT, s);

        GetDlgItemTextA(hDlg, IDC_LIBHEIFVERSION, format, 256);
        _snprintf_s(s, _countof(s), format, LIBHEIF_VERSION);
        SetDlgItemTextA(hDlg, IDC_LIBHEIFVERSION, s);

        GetDlgItemTextA(hDlg, IDC_AOMVERSION, format, 256);
        _snprintf_s(s, _countof(s), format, aom_codec_version_str());
        SetDlgItemTextA(hDlg, IDC_AOMVERSION, s);
    }

    INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam) noexcept
    {
        UNREFERENCED_PARAMETER(lParam);

        int cmd;
        LPNMHDR pNmhdr;

        switch (wMsg)
        {
        case WM_INITDIALOG:
            CenterDialog(hDlg);
            InitAboutDialog(hDlg);
            return TRUE;
        case WM_LBUTTONUP:
            EndDialog(hDlg, IDOK);
            break;
        case WM_COMMAND:
            cmd = HIWORD(wParam);

            if (cmd == BN_CLICKED)
            {
                EndDialog(hDlg, IDOK);
            }
            break;
        case WM_NOTIFY:

            pNmhdr = reinterpret_cast<LPNMHDR>(lParam);
            switch (pNmhdr->code)
            {
            case NM_CLICK:          // Fall through to the next case.
            case NM_RETURN:
            {
                if (pNmhdr->idFrom == IDC_PROJECT_HOMEPAGE_LINK)
                {
                    ShellExecuteW(nullptr, L"open", L"https://github.com/0xC0000054/avif-format", nullptr, nullptr, SW_SHOW);
                }
                else if (pNmhdr->idFrom == IDC_CREDITS_LINK)
                {
                    const PNMLINK link = reinterpret_cast<PNMLINK>(lParam);

                    switch (link->item.iLink)
                    {
                    case 0:
                        ShellExecuteW(nullptr, L"open", L"https://github.com/strukturag/libheif", nullptr, nullptr, SW_SHOW);
                        break;
                    case 1:
                        ShellExecuteW(nullptr, L"open", L"https://aomedia.googlesource.com/aom/", nullptr, nullptr, SW_SHOW);
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
            }
            break;
        }

        return FALSE;
    }

    class LoadDialog
    {
    public:

        LoadDialog(const LoadUIOptions& loadOptions)
        {
            options.applyHLGOOTF = loadOptions.applyHLGOOTF;
            options.displayGamma = loadOptions.displayGamma;
            options.nominalPeakBrightness = loadOptions.nominalPeakBrightness;

            ZeroMemory(decimalSeparator, sizeof(decimalSeparator));
            ZeroMemory(thousandsSeparator, sizeof(thousandsSeparator));
            ZeroMemory(lastValidDisplayGammaStr, sizeof(lastValidDisplayGammaStr));
            ZeroMemory(lastValidPeakBrightnessStr, sizeof(lastValidPeakBrightnessStr));
            displayGammaTextUpdating = false;
            peakBrightnessTextUpdating = false;

            WCHAR buffer[4]{};

            // MSDN documents LOCALE_SDECIMAL as requiring at most 4 characters.

            decimalSeparatorLength = GetLocaleInfoW(
                LOCALE_USER_DEFAULT,
                LOCALE_SDECIMAL,
                buffer,
                _countof(buffer)) - 1; // Remove the terminator from the total

            if (decimalSeparatorLength > 0 && decimalSeparatorLength <= 3)
            {
                wmemcpy_s(decimalSeparator, _countof(decimalSeparator) - 1, buffer, decimalSeparatorLength);
            }
            else
            {
                decimalSeparator[0] = '.';
                decimalSeparatorLength = 1;
            }

            // MSDN documents LOCALE_SDECIMAL as requiring at most 4 characters.

            thousandsSeparatorLength = GetLocaleInfoW(
                LOCALE_USER_DEFAULT,
                LOCALE_STHOUSAND,
                buffer,
                _countof(buffer)) - 1; // Remove the terminator from the total

            if (thousandsSeparatorLength > 0 && thousandsSeparatorLength <= 3)
            {
                wmemcpy_s(thousandsSeparator, _countof(thousandsSeparator) - 1, buffer, thousandsSeparatorLength);
            }
            else
            {
                thousandsSeparator[0] = ',';
                thousandsSeparatorLength = 1;
            }

            FormatDisplayGammaFloat(loadOptions.displayGamma);
        }

        const LoadUIOptions& GetLoadOptions() const
        {
            return options;
        }

        static INT_PTR CALLBACK StaticDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
        {
            LoadDialog* dialog;

            if (wMsg == WM_INITDIALOG)
            {
                dialog = reinterpret_cast<LoadDialog*>(lParam);

                SetWindowLongPtr(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(dialog));
            }
            else
            {
                dialog = reinterpret_cast<LoadDialog*>(GetWindowLongPtr(hDlg, DWLP_USER));
            }

            if (dialog != nullptr)
            {
                return dialog->DlgProc(hDlg, wMsg, wParam, lParam);
            }
            else
            {
                return FALSE;
            }
        }

    private:

        void EnableOOTFControls(HWND hDlg, bool enabled)
        {
            EnableWindow(GetDlgItem(hDlg, IDC_DISPLAY_GAMMA_LABEL), enabled);
            EnableWindow(GetDlgItem(hDlg, IDC_DISPLAY_GAMMA_EDIT), enabled);
            EnableWindow(GetDlgItem(hDlg, IDC_PEAK_BRIGHTNESS_LABEL), enabled);
            EnableWindow(GetDlgItem(hDlg, IDC_PEAK_BRIGHTNESS_EDIT), enabled);
            EnableWindow(GetDlgItem(hDlg, IDC_PEAK_BRIGHTNESS_SPIN), enabled);
        }

        void InitializeDialog(HWND hDlg) noexcept
        {
            HWND applyOOTFCheck = GetDlgItem(hDlg, IDC_APPLY_HLG_OOTF);
            HWND displayGammaLabel = GetDlgItem(hDlg, IDC_DISPLAY_GAMMA_LABEL);
            HWND displayGammaEdit = GetDlgItem(hDlg, IDC_DISPLAY_GAMMA_EDIT);
            HWND peakBrightnessLabel = GetDlgItem(hDlg, IDC_PEAK_BRIGHTNESS_LABEL);
            HWND peakBrightnessEdit = GetDlgItem(hDlg, IDC_PEAK_BRIGHTNESS_EDIT);
            HWND peakBrightnessSpin = GetDlgItem(hDlg, IDC_PEAK_BRIGHTNESS_SPIN);

            Button_SetCheck(applyOOTFCheck, options.applyHLGOOTF ? BST_CHECKED : BST_UNCHECKED);

            Edit_SetText(displayGammaEdit, lastValidDisplayGammaStr);

            SendMessage(peakBrightnessSpin, UDM_SETBUDDY, reinterpret_cast<WPARAM>(peakBrightnessEdit), 0);
            SendMessage(peakBrightnessSpin, UDM_SETRANGE, 0, MAKELPARAM(10000, 0));
            SendMessage(peakBrightnessSpin, UDM_SETPOS, 0, static_cast<LPARAM>(options.nominalPeakBrightness));

            if (!options.applyHLGOOTF)
            {
                EnableWindow(displayGammaLabel, false);
                EnableWindow(displayGammaEdit, false);
                EnableWindow(peakBrightnessLabel, false);
                EnableWindow(peakBrightnessEdit, false);
                EnableWindow(peakBrightnessSpin, false);
            }
        }

        INT_PTR CALLBACK DlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam) noexcept
        {
            int item;
            int cmd;
            HWND controlHwnd;

            switch (wMsg)
            {
            case WM_INITDIALOG:
                CenterDialog(hDlg);
                InitializeDialog(hDlg);
                return TRUE;
            case WM_COMMAND:
                item = LOWORD(wParam);
                cmd = HIWORD(wParam);

                if (cmd == BN_CLICKED)
                {
                    controlHwnd = reinterpret_cast<HWND>(lParam);

                    switch (item)
                    {
                    case IDC_APPLY_HLG_OOTF:
                        options.applyHLGOOTF = Button_GetCheck(controlHwnd) == BST_CHECKED;
                        EnableOOTFControls(hDlg, options.applyHLGOOTF);
                        break;
                    case IDOK:
                    case IDCANCEL:
                        EndDialog(hDlg, item);
                        break;
                    default:
                        break;
                    }
                }
                else if (cmd == EN_CHANGE)
                {
                    controlHwnd = reinterpret_cast<HWND>(lParam);

                    if (item == IDC_DISPLAY_GAMMA_EDIT)
                    {
                        OnUpdateDisplayGammaText(controlHwnd);
                    }
                    else if (item == IDC_PEAK_BRIGHTNESS_EDIT)
                    {
                        OnUpdatePeakBrightnessText(hDlg, controlHwnd);
                    }
                }

                break;
            }

            return FALSE;
        }

        void FormatDisplayGammaFloat(float value)
        {
            value += 0.005f; // Ensure that the integer casts round up

            int whole = static_cast<int>(value);
            int fractionalDigits = static_cast<int>(value * 100.0f) % 100;

            // The whole number will be between 1 and 3.
            lastValidDisplayGammaStr[0] = static_cast<WCHAR>(whole + '0');
            wmemcpy_s(&lastValidDisplayGammaStr[1], _countof(lastValidDisplayGammaStr) - 1, decimalSeparator, decimalSeparatorLength);

            int index = 1 + decimalSeparatorLength;

            lastValidDisplayGammaStr[index] = static_cast<WCHAR>((fractionalDigits / 10) + '0');
            lastValidDisplayGammaStr[index + 1] = static_cast<WCHAR>((fractionalDigits % 10) + '0');
        }

        void OnUpdateDisplayGammaText(HWND hwnd)
        {
            if (displayGammaTextUpdating)
            {
                return;
            }

            bool valid = false;
            bool emptyFraction = false;
            bool updateWindowText = false;

            const int length = GetWindowTextLengthW(hwnd);

            WCHAR windowTextBuffer[DisplayGammaBufferSize]{};

            if (length > 0 && length < DisplayGammaBufferSize)
            {
                GetWindowTextW(hwnd, windowTextBuffer, DisplayGammaBufferSize - 1);

                if (std::iswdigit(windowTextBuffer[0]))
                {
                    int whole = windowTextBuffer[0] - '0';

                    if (whole >= 1 && whole <= 3)
                    {
                        if (length == 1)
                        {
                            valid = true;
                        }
                        else
                        {
                            const int lengthWithDecimalPoint = 1 + decimalSeparatorLength;

                            if (length >= lengthWithDecimalPoint &&
                                wcsncmp(&windowTextBuffer[1], decimalSeparator, decimalSeparatorLength) == 0)
                            {
                                int fractionLength = length - lengthWithDecimalPoint;

                                if (fractionLength == 0)
                                {
                                    emptyFraction = true;
                                }
                                else
                                {
                                    bool trimString = false;

                                    if (fractionLength > 2)
                                    {
                                        fractionLength = 2;
                                        trimString = true;
                                    }

                                    bool allInteger = true;

                                    for (int i = 0; i < fractionLength; i++)
                                    {
                                        if (!std::iswdigit(windowTextBuffer[lengthWithDecimalPoint + i]))
                                        {
                                            allInteger = false;
                                            break;
                                        }
                                    }

                                    if (allInteger)
                                    {
                                        valid = true;
                                        if (trimString)
                                        {
                                            windowTextBuffer[lengthWithDecimalPoint + 2] = L'\0';
                                            updateWindowText = true;
                                        }

                                        if (whole == 3)
                                        {
                                            for (int i = 0; i < fractionLength; i++)
                                            {
                                                WCHAR& value = windowTextBuffer[lengthWithDecimalPoint + i];

                                                if (value > L'0')
                                                {
                                                    value = L'0';
                                                    updateWindowText = true;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (valid)
            {
                wcscpy_s(lastValidDisplayGammaStr, windowTextBuffer);
                float temp;

                if (TryParseDisplayGammaText(temp))
                {
                    options.displayGamma = temp;
                }

                if (updateWindowText)
                {
                    displayGammaTextUpdating = true;

                    SetWindowTextW(hwnd, lastValidDisplayGammaStr);

                    displayGammaTextUpdating = false;
                }
            }
            else
            {
                if (!emptyFraction)
                {
                    displayGammaTextUpdating = true;

                    SetWindowTextW(hwnd, lastValidDisplayGammaStr);

                    displayGammaTextUpdating = false;
                }
            }
        }

        void OnUpdatePeakBrightnessText(HWND hDlg, HWND editBoxHwnd)
        {
            if (peakBrightnessTextUpdating)
            {
                return;
            }

            bool valid = false;

            const int length = GetWindowTextLengthW(editBoxHwnd);

            WCHAR windowTextBuffer[PeakBrightnessBufferSize]{};
            int value = 0;

            if (length > 0 && length < PeakBrightnessBufferSize)
            {
                GetWindowTextW(editBoxHwnd, windowTextBuffer, PeakBrightnessBufferSize - 1);

                bool isNumber = true;

                for (int i = 0; i < length; i++)
                {
                    const WCHAR c = windowTextBuffer[i];

                    if (!std::iswdigit(c) && wcsncmp(&windowTextBuffer[i], thousandsSeparator, thousandsSeparatorLength) != 0)
                    {
                        // Unknown character in the string.
                        isNumber = false;
                        break;
                    }
                }

                if (isNumber && TryParsePeakBrightnessText(windowTextBuffer, length, value))
                {
                    if (value < nominalPeakBrightnessMin)
                    {
                        SendMessage(
                            GetDlgItem(hDlg, IDC_PEAK_BRIGHTNESS_SPIN),
                            UDM_SETPOS,
                            0,
                            static_cast<LPARAM>(nominalPeakBrightnessMin));
                    }
                    else if (value > nominalPeakBrightnessMax)
                    {
                        SendMessage(
                            GetDlgItem(hDlg, IDC_PEAK_BRIGHTNESS_SPIN),
                            UDM_SETPOS,
                            0,
                            static_cast<LPARAM>(nominalPeakBrightnessMax));
                    }
                    else
                    {
                        valid = true;
                    }
                }
            }

            if (valid)
            {
                wcscpy_s(lastValidPeakBrightnessStr, windowTextBuffer);

                options.nominalPeakBrightness = value;
            }
            else
            {
                peakBrightnessTextUpdating = true;

                SetWindowTextW(editBoxHwnd, lastValidPeakBrightnessStr);

                peakBrightnessTextUpdating = false;
            }
        }

        bool TryParseDisplayGammaText(float& value)
        {
            const size_t stringLength = wcslen(lastValidDisplayGammaStr);

            bool result = false;

            if (stringLength >= 1 && std::iswdigit(lastValidDisplayGammaStr[0]))
            {
                int whole = lastValidDisplayGammaStr[0] - '0';

                if (whole >= 1 && whole <= 3)
                {
                    const size_t lengthWithDecimalPoint = static_cast<size_t>(1) + decimalSeparatorLength;

                    if (stringLength > lengthWithDecimalPoint &&
                        wcsncmp(&lastValidDisplayGammaStr[1], decimalSeparator, decimalSeparatorLength) == 0 &&
                        whole < 3)
                    {
                        // The "C" locale is used to ensure the decimal separator is always '.' when parsing.
                        static _locale_t locale = _create_locale(LC_ALL, "C");

                        WCHAR parseBuffer[5]{};

                        const size_t fractionLength = stringLength - lengthWithDecimalPoint;

                        parseBuffer[0] = lastValidDisplayGammaStr[0];
                        parseBuffer[1] = L'.';

                        if (fractionLength >= 1 && std::iswdigit(lastValidDisplayGammaStr[lengthWithDecimalPoint]))
                        {
                            parseBuffer[2] = lastValidDisplayGammaStr[lengthWithDecimalPoint];

                            if (fractionLength >= 2 && std::iswdigit(lastValidDisplayGammaStr[lengthWithDecimalPoint + 1]))
                            {
                                parseBuffer[3] = lastValidDisplayGammaStr[lengthWithDecimalPoint + 1];
                            }
                        }
                        else
                        {
                            parseBuffer[2] = L'0';
                        }

                        value = _wcstof_l(parseBuffer, nullptr, locale);
                        result = true;
                    }
                    else
                    {
                        value = static_cast<float>(whole);
                        result = true;
                    }
                }
            }

            return result;
        }

        bool TryParsePeakBrightnessText(const wchar_t* text, size_t length, int& value)
        {
            bool result = false;

            if (length >= 1 && length < static_cast<size_t>(PeakBrightnessBufferSize))
            {
                WCHAR numberBuffer[PeakBrightnessBufferSize]{};
                bool isValid = true;

                for (size_t i = 0, j = 0; i < length; i++)
                {
                    const WCHAR c = text[i];

                    if (std::iswdigit(c))
                    {
                        numberBuffer[j++] = c;
                    }
                    else
                    {
                        if (wcsncmp(&text[i], thousandsSeparator, thousandsSeparatorLength) != 0)
                        {
                            // Unknown character in the string.
                            isValid = false;
                            break;
                        }
                    }
                }

                // The "C" locale is used to ensure the parsing behavior is not affected by the system locale.
                static _locale_t locale = _create_locale(LC_ALL, "C");

                if (isValid)
                {
                    value = _wcstol_l(numberBuffer, nullptr, 10, locale);
                    result = true;
                }
            }

            return result;
        }

        static constexpr int DisplayGammaBufferSize = 12;
        static constexpr int PeakBrightnessBufferSize = 12;

        LoadUIOptions options;
        WCHAR decimalSeparator[4];
        int decimalSeparatorLength;
        WCHAR thousandsSeparator[4];
        int thousandsSeparatorLength;
        WCHAR lastValidDisplayGammaStr[DisplayGammaBufferSize];
        bool displayGammaTextUpdating;
        WCHAR lastValidPeakBrightnessStr[PeakBrightnessBufferSize];
        bool peakBrightnessTextUpdating;
    };


    class SaveDialog
    {
    public:

        SaveDialog(const FormatRecordPtr formatRecord, const SaveUIOptions& saveOptions) :
            hostImageDepth(formatRecord->depth),
            hasColorProfile(HasColorProfileMetadata(formatRecord)),
            hasExif(HasExifMetadata(formatRecord)),
            hasXmp(HasXmpMetadata(formatRecord)),
            hasAlphaChannel(HasAlphaChannel(formatRecord)),
            monochrome(IsMonochromeImage(formatRecord)),
            imageDepthComboEnabled(true),
            colorProfileCheckboxEnabled(hasColorProfile && (formatRecord->depth != 32 || saveOptions.hdrTransferFunction == ColorTransferFunction::Clip)),
            colorProfileChecked(false),
            premultipliedAlphaCheckboxEnabled(hasAlphaChannel && (formatRecord->depth != 32 || saveOptions.hdrTransferFunction == ColorTransferFunction::Clip)),
            premultipliedAlphaChecked(false)
        {
            options.quality = saveOptions.quality;
            // YUV 4:2:0 is used for monochrome images because AOM does not have a YUV 4:0:0 mode.
            options.chromaSubsampling = monochrome ? ChromaSubsampling::Yuv420 : saveOptions.chromaSubsampling;
            options.compressionSpeed = saveOptions.compressionSpeed;
            options.imageBitDepth = formatRecord->depth == 8 ? ImageBitDepth::Eight : saveOptions.imageBitDepth;
            options.hdrTransferFunction = saveOptions.hdrTransferFunction;
            options.lossless = saveOptions.lossless;
            options.losslessAlpha = saveOptions.losslessAlpha;
            options.keepColorProfile = saveOptions.keepColorProfile && hasColorProfile && colorProfileCheckboxEnabled;
            options.keepExif = saveOptions.keepExif && hasExif;
            options.keepXmp = saveOptions.keepXmp && hasXmp;
            options.premultipliedAlpha = saveOptions.premultipliedAlpha && hasAlphaChannel && premultipliedAlphaCheckboxEnabled;
        }

        const SaveUIOptions& GetSaveOptions() const
        {
            return options;
        }

        static INT_PTR CALLBACK StaticDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
        {
            SaveDialog* dialog;

            if (wMsg == WM_INITDIALOG)
            {
                dialog = reinterpret_cast<SaveDialog*>(lParam);

                SetWindowLongPtr(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(dialog));
            }
            else
            {
                dialog = reinterpret_cast<SaveDialog*>(GetWindowLongPtr(hDlg, DWLP_USER));
            }

            if (dialog != nullptr)
            {
                return dialog->DlgProc(hDlg, wMsg, wParam, lParam);
            }
            else
            {
                return FALSE;
            }
        }

    private:

        void EnableLossyCompressionSettings(HWND hDlg, bool enabled)
        {
            EnableWindow(GetDlgItem(hDlg, IDC_QUALITY_SLIDER), enabled);
            EnableWindow(GetDlgItem(hDlg, IDC_QUALITY_EDIT), enabled);
            EnableWindow(GetDlgItem(hDlg, IDC_QUALITY_EDIT_SPIN), enabled);

            if (!monochrome)
            {
                EnableWindow(GetDlgItem(hDlg, IDC_CHROMA_SUBSAMPLING_COMBO), enabled);
            }

            if (hasAlphaChannel)
            {
                EnableWindow(GetDlgItem(hDlg, IDC_PREMULTIPLIED_ALPHA_CHECK), enabled);
            }
        }

        void InitializeDialog(HWND hDlg) noexcept
        {
            const HINSTANCE hInstance = GetModuleInstanceHandle();

            HWND qualitySlider = GetDlgItem(hDlg, IDC_QUALITY);
            HWND qualityEditBox = GetDlgItem(hDlg, IDC_QUALITY_EDIT);
            HWND qualityEditUpDown = GetDlgItem(hDlg, IDC_QUALITY_EDIT_SPIN);
            HWND losslessCheckbox = GetDlgItem(hDlg, IDC_LOSSLESS_CHECK);
            HWND losslessAlphaCheckbox = GetDlgItem(hDlg, IDC_LOSSLESS_ALPHA_CHECK);
            HWND chromaSubsamplingCombo = GetDlgItem(hDlg, IDC_CHROMA_SUBSAMPLING_COMBO);
            HWND keepColorProfileCheckbox = GetDlgItem(hDlg, IDC_KEEP_COLOR_PROFILE_CHECK);
            HWND keepExifCheckbox = GetDlgItem(hDlg, IDC_KEEP_EXIF_CHECK);
            HWND keepXmpCheckbox = GetDlgItem(hDlg, IDC_KEEP_XMP_CHECK);
            HWND premultipliedAlphaCheckbox = GetDlgItem(hDlg, IDC_PREMULTIPLIED_ALPHA_CHECK);
            HWND pixelDepthCombo = GetDlgItem(hDlg, IDC_IMAGE_DEPTH_COMBO);
            HWND hdrTransferCharacteristicsLabel = GetDlgItem(hDlg, IDC_HDR_TRANSFER_CHARACTERISTICS_LABEL);
            HWND hdrTransferCharacteristicsCombo = GetDlgItem(hDlg, IDC_HDR_TRANSFER_CHARACTERISTICS_COMBO);
            HWND hdrInfoLabel = GetDlgItem(hDlg, IDC_HDRINFOLABEL);

            SendMessage(qualitySlider, TBM_SETRANGEMIN, FALSE, 0);
            SendMessage(qualitySlider, TBM_SETRANGEMAX, FALSE, 100);
            SendMessage(qualitySlider, TBM_SETPOS, TRUE, options.quality);
            SendMessage(qualitySlider, TBM_SETBUDDY, FALSE, reinterpret_cast<LPARAM>(qualityEditBox));

            SendMessage(qualityEditBox, EM_LIMITTEXT, 3, 0);
            SetDlgItemInt(hDlg, IDC_QUALITY_EDIT, static_cast<UINT>(options.quality), false);

            SendMessage(qualityEditUpDown, UDM_SETBUDDY, reinterpret_cast<WPARAM>(qualityEditBox), 0);
            SendMessage(qualityEditUpDown, UDM_SETRANGE, 0, MAKELPARAM(100, 0));

            Button_SetCheck(losslessCheckbox, options.lossless);
            EnableLossyCompressionSettings(hDlg, !options.lossless);
            Button_SetCheck(losslessAlphaCheckbox, hasAlphaChannel && options.losslessAlpha);
            EnableWindow(losslessAlphaCheckbox, hasAlphaChannel);

            constexpr int resourceBufferLength = 256;
            TCHAR resourceBuffer[resourceBufferLength]{};

            if (hostImageDepth == 32)
            {
                if (monochrome)
                {
                    if (LoadString(hInstance, IDS_HDR_TRANSFER_CHARACTERISTICS_CLIP, resourceBuffer, resourceBufferLength) > 0)
                    {
                        SendMessage(hdrTransferCharacteristicsCombo, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(resourceBuffer));
                    }

                    ComboBox_SetCurSel(hdrTransferCharacteristicsCombo, 0);
                    EnableWindow(hdrTransferCharacteristicsCombo, FALSE);
                    ShowWindow(hdrInfoLabel, SW_HIDE);
                    options.hdrTransferFunction = ColorTransferFunction::Clip;
                }
                else
                {
                    SendMessage(hdrTransferCharacteristicsCombo, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(TEXT("Rec. 2100 PQ")));
                    SendMessage(hdrTransferCharacteristicsCombo, CB_INSERTSTRING, 1, reinterpret_cast<LPARAM>(TEXT("SMPTE 428-1")));

                    if (LoadString(hInstance, IDS_HDR_TRANSFER_CHARACTERISTICS_CLIP, resourceBuffer, resourceBufferLength) > 0)
                    {
                        SendMessage(hdrTransferCharacteristicsCombo, CB_INSERTSTRING, 2, reinterpret_cast<LPARAM>(resourceBuffer));
                    }

                    int selectedIndex;
                    switch (options.hdrTransferFunction)
                    {
                    case ColorTransferFunction::SMPTE428:
                        selectedIndex = 1;
                        break;
                    case ColorTransferFunction::Clip:
                        selectedIndex = 2;
                        break;
                    case ColorTransferFunction::PQ:
                    default:
                        selectedIndex = 0;
                        break;
                    }

                    ComboBox_SetCurSel(hdrTransferCharacteristicsCombo, selectedIndex);
                }
            }
            else
            {
                EnableWindow(hdrTransferCharacteristicsLabel, FALSE);
                EnableWindow(hdrTransferCharacteristicsCombo, FALSE);
                EnableWindow(hdrInfoLabel, FALSE);
            }

            // Swap the tab order of the Chroma Subsampling combo box and the Default compression speed radio button.
            SetWindowPos(chromaSubsamplingCombo, GetDlgItem(hDlg, IDC_COMPRESSION_SPEED_DEFAULT_RADIO), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            if (monochrome)
            {
                if (LoadString(hInstance, IDS_CHROMA_SUBSAMPLING_400, resourceBuffer, resourceBufferLength) > 0)
                {
                    SendMessage(chromaSubsamplingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(resourceBuffer));
                }
                ComboBox_SetCurSel(chromaSubsamplingCombo, 0);
                EnableWindow(chromaSubsamplingCombo, false);
            }
            else
            {
                std::array<UINT, 3> chromaSubsamplingResIds = { IDS_CHROMA_SUBSAMPLING_420,
                                                                IDS_CHROMA_SUBSAMPLING_422,
                                                                IDS_CHROMA_SUBSAMPLING_444 };

                for (size_t i = 0; i < chromaSubsamplingResIds.size(); i++)
                {
                    UINT id = chromaSubsamplingResIds[i];

                    if (LoadString(hInstance, id, resourceBuffer, resourceBufferLength) > 0)
                    {
                        SendMessage(chromaSubsamplingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(resourceBuffer));
                    }
                }

                int selectedChromaSubsamplingIndex;
                switch (options.chromaSubsampling)
                {
                case ChromaSubsampling::Yuv422:
                    selectedChromaSubsamplingIndex = 1;
                    break;
                case ChromaSubsampling::Yuv444:
                    selectedChromaSubsamplingIndex = 2;
                    break;
                case ChromaSubsampling::Yuv420:
                default:
                    selectedChromaSubsamplingIndex = 0;
                    break;
                }

                ComboBox_SetCurSel(chromaSubsamplingCombo, selectedChromaSubsamplingIndex);
            }

            int selectedCompressionSpeed;
            switch (options.compressionSpeed)
            {
            case CompressionSpeed::Fastest:
                selectedCompressionSpeed = IDC_COMPRESSION_SPEED_FASTEST_RADIO;
                break;
            case CompressionSpeed::Slowest:
                selectedCompressionSpeed = IDC_COMPRESSION_SPEED_SLOWEST_RADIO;
                break;
            case CompressionSpeed::Default:
            default:
                selectedCompressionSpeed = IDC_COMPRESSION_SPEED_DEFAULT_RADIO;
                break;
            }

            CheckRadioButton(hDlg, IDC_COMPRESSION_SPEED_FASTEST_RADIO, IDC_COMPRESSION_SPEED_SLOWEST_RADIO, selectedCompressionSpeed);

            if (colorProfileCheckboxEnabled)
            {
                Button_SetCheck(keepColorProfileCheckbox, options.keepColorProfile);
                EnableWindow(keepColorProfileCheckbox, true);
            }
            else
            {
                Button_SetCheck(keepColorProfileCheckbox, false);
                EnableWindow(keepColorProfileCheckbox, false);
            }

            if (hasExif)
            {
                Button_SetCheck(keepExifCheckbox, options.keepExif);
                EnableWindow(keepExifCheckbox, true);
            }
            else
            {
                Button_SetCheck(keepExifCheckbox, false);
                EnableWindow(keepExifCheckbox, false);
            }

            if (hasXmp)
            {
                Button_SetCheck(keepXmpCheckbox, options.keepXmp);
                EnableWindow(keepXmpCheckbox, true);
            }
            else
            {
                Button_SetCheck(keepXmpCheckbox, false);
                EnableWindow(keepXmpCheckbox, false);
            }

            if (premultipliedAlphaCheckboxEnabled)
            {
                Button_SetCheck(premultipliedAlphaCheckbox, options.premultipliedAlpha);
                EnableWindow(premultipliedAlphaCheckbox, true);
            }
            else
            {
                Button_SetCheck(premultipliedAlphaCheckbox, false);
                EnableWindow(premultipliedAlphaCheckbox, false);
            }

            if (hostImageDepth == 32)
            {
                SendMessage(pixelDepthCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TEXT("10-bit")));
                SendMessage(pixelDepthCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TEXT("12-bit")));

                if (options.hdrTransferFunction == ColorTransferFunction::SMPTE428)
                {
                    // SMPTE 428 only supports 12-bit.
                    options.imageBitDepth = ImageBitDepth::Twelve;
                    ComboBox_SetCurSel(pixelDepthCombo, 1);
                    EnableWindow(pixelDepthCombo, false);
                    imageDepthComboEnabled = false;
                }
                else
                {
                    ComboBox_SetCurSel(pixelDepthCombo, options.imageBitDepth == ImageBitDepth::Ten ? 0 : 1);
                }
            }
            else
            {
                SendMessage(pixelDepthCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TEXT("8-bit")));
                SendMessage(pixelDepthCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TEXT("10-bit")));
                SendMessage(pixelDepthCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TEXT("12-bit")));

                if (hostImageDepth == 8)
                {
                    ComboBox_SetCurSel(pixelDepthCombo, 0);
                }
                else
                {
                    ComboBox_SetCurSel(pixelDepthCombo, options.imageBitDepth == ImageBitDepth::Ten ? 1 : 2);
                }
            }
        }

        INT_PTR CALLBACK DlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam) noexcept
        {
            int item;
            int cmd;
            int value;
            HWND controlHwnd;

            switch (wMsg)
            {
            case WM_INITDIALOG:
                CenterDialog(hDlg);
                InitializeDialog(hDlg);
                return TRUE;
            case WM_COMMAND:
                item = LOWORD(wParam);
                cmd = HIWORD(wParam);

                if (cmd == BN_CLICKED)
                {
                    controlHwnd = reinterpret_cast<HWND>(lParam);

                    switch (item)
                    {
                    case IDC_COMPRESSION_SPEED_FASTEST_RADIO:
                        if (Button_GetCheck(controlHwnd) == BST_CHECKED)
                        {
                            CheckRadioButton(
                                hDlg,
                                IDC_COMPRESSION_SPEED_FASTEST_RADIO,
                                IDC_COMPRESSION_SPEED_SLOWEST_RADIO,
                                IDC_COMPRESSION_SPEED_FASTEST_RADIO);
                            options.compressionSpeed = CompressionSpeed::Fastest;
                        }
                        break;
                    case IDC_COMPRESSION_SPEED_DEFAULT_RADIO:
                        if (Button_GetCheck(controlHwnd) == BST_CHECKED)
                        {
                            CheckRadioButton(
                                hDlg,
                                IDC_COMPRESSION_SPEED_FASTEST_RADIO,
                                IDC_COMPRESSION_SPEED_SLOWEST_RADIO,
                                IDC_COMPRESSION_SPEED_DEFAULT_RADIO);
                            options.compressionSpeed = CompressionSpeed::Default;
                        }
                        break;
                    case IDC_COMPRESSION_SPEED_SLOWEST_RADIO:
                        if (Button_GetCheck(controlHwnd) == BST_CHECKED)
                        {
                            CheckRadioButton(
                                hDlg,
                                IDC_COMPRESSION_SPEED_FASTEST_RADIO,
                                IDC_COMPRESSION_SPEED_SLOWEST_RADIO,
                                IDC_COMPRESSION_SPEED_SLOWEST_RADIO);
                            options.compressionSpeed = CompressionSpeed::Slowest;
                        }
                        break;
                    case IDC_KEEP_COLOR_PROFILE_CHECK:
                        options.keepColorProfile = Button_GetCheck(controlHwnd) == BST_CHECKED;
                        break;
                    case IDC_KEEP_EXIF_CHECK:
                        options.keepExif = Button_GetCheck(controlHwnd) == BST_CHECKED;
                        break;
                    case IDC_KEEP_XMP_CHECK:
                        options.keepXmp = Button_GetCheck(controlHwnd) == BST_CHECKED;
                        break;
                    case IDC_PREMULTIPLIED_ALPHA_CHECK:
                        options.premultipliedAlpha = Button_GetCheck(controlHwnd) == BST_CHECKED;
                        break;
                    case IDC_LOSSLESS_CHECK:
                        options.lossless = Button_GetCheck(controlHwnd) == BST_CHECKED;
                        if (hasAlphaChannel)
                        {
                            EnableWindow(GetDlgItem(hDlg, IDC_LOSSLESS_ALPHA_CHECK), !options.lossless);
                        }
                        EnableLossyCompressionSettings(hDlg, !options.lossless);
                        break;
                    case IDC_LOSSLESS_ALPHA_CHECK:
                        options.losslessAlpha = Button_GetCheck(controlHwnd) == BST_CHECKED;
                        break;
                    case IDOK:
                    case IDCANCEL:
                        EndDialog(hDlg, item);
                        break;
                    default:
                        break;
                    }
                }
                else if (cmd == CBN_SELCHANGE)
                {
                    controlHwnd = reinterpret_cast<HWND>(lParam);

                    value = ComboBox_GetCurSel(controlHwnd);

                    if (item == IDC_CHROMA_SUBSAMPLING_COMBO)
                    {
                        switch (value)
                        {
                        case 0:
                            options.chromaSubsampling = ChromaSubsampling::Yuv420;
                            break;
                        case 1:
                            options.chromaSubsampling = ChromaSubsampling::Yuv422;
                            break;
                        case 2:
                            options.chromaSubsampling = ChromaSubsampling::Yuv444;
                            break;
                        default:
                            options.chromaSubsampling = ChromaSubsampling::Yuv420;
                            break;
                        }
                    }
                    else if (item == IDC_IMAGE_DEPTH_COMBO)
                    {
                        if (hostImageDepth == 32)
                        {
                            options.imageBitDepth = value == 0 ? ImageBitDepth::Ten : ImageBitDepth::Twelve;
                        }
                        else
                        {
                            switch (value)
                            {
                            case 0:
                                options.imageBitDepth = ImageBitDepth::Eight;
                                break;
                            case 1:
                                options.imageBitDepth = ImageBitDepth::Ten;
                                break;
                            case 2:
                                options.imageBitDepth = ImageBitDepth::Twelve;
                                break;
                            default:
                                options.imageBitDepth = hostImageDepth == 8 ? ImageBitDepth::Eight : ImageBitDepth::Twelve;
                                break;
                            }
                        }
                    }
                    else if (item == IDC_HDR_TRANSFER_CHARACTERISTICS_COMBO)
                    {
                        if (value != 2)
                        {
                            if (colorProfileCheckboxEnabled)
                            {
                                colorProfileCheckboxEnabled = false;
                                options.keepColorProfile = false;

                                HWND colorProfileCheck = GetDlgItem(hDlg, IDC_KEEP_COLOR_PROFILE_CHECK);

                                colorProfileChecked = Button_GetCheck(colorProfileCheck) == BST_CHECKED;

                                Button_SetCheck(colorProfileCheck, BST_UNCHECKED);
                                EnableWindow(colorProfileCheck, false);
                            }

                            if (premultipliedAlphaCheckboxEnabled)
                            {
                                premultipliedAlphaCheckboxEnabled = false;
                                options.premultipliedAlpha = false;

                                HWND premultipliedAlphaCheck = GetDlgItem(hDlg, IDC_PREMULTIPLIED_ALPHA_CHECK);

                                premultipliedAlphaChecked = Button_GetCheck(premultipliedAlphaCheck) == BST_CHECKED;

                                Button_SetCheck(premultipliedAlphaCheck, BST_UNCHECKED);
                                EnableWindow(premultipliedAlphaCheck, false);
                            }
                        }
                        else
                        {
                            if (!colorProfileCheckboxEnabled)
                            {
                                colorProfileCheckboxEnabled = true;
                                options.keepColorProfile = colorProfileChecked;

                                HWND colorProfileCheck = GetDlgItem(hDlg, IDC_KEEP_COLOR_PROFILE_CHECK);

                                Button_SetCheck(colorProfileCheck, colorProfileChecked ? BST_CHECKED : BST_UNCHECKED);
                                EnableWindow(colorProfileCheck, true);
                            }

                            if (!premultipliedAlphaCheckboxEnabled)
                            {
                                premultipliedAlphaCheckboxEnabled = true;
                                options.premultipliedAlpha = premultipliedAlphaChecked;

                                HWND premultipliedAlphaCheck = GetDlgItem(hDlg, IDC_PREMULTIPLIED_ALPHA_CHECK);

                                Button_SetCheck(premultipliedAlphaCheck,premultipliedAlphaChecked ? BST_CHECKED : BST_UNCHECKED);
                                EnableWindow(premultipliedAlphaCheck, true);
                            }
                        }

                        if (value == 1)
                        {
                            if (imageDepthComboEnabled)
                            {
                                imageDepthComboEnabled = false;

                                // SMPTE 428 requires 12-bit.
                                options.imageBitDepth = ImageBitDepth::Twelve;

                                HWND imageDepthCombo = GetDlgItem(hDlg, IDC_IMAGE_DEPTH_COMBO);

                                ComboBox_SetCurSel(imageDepthCombo, 1);
                                EnableWindow(imageDepthCombo, false);
                            }

                            options.hdrTransferFunction = ColorTransferFunction::SMPTE428;
                        }
                        else
                        {
                            if (!imageDepthComboEnabled)
                            {
                                imageDepthComboEnabled = true;

                                // The Rec. 2100 and Clip modes support both 10-bit and 12-bit.
                                EnableWindow(GetDlgItem(hDlg, IDC_IMAGE_DEPTH_COMBO), true);
                            }

                            switch (value)
                            {
                            case 2:
                                options.hdrTransferFunction = ColorTransferFunction::Clip;
                                break;
                            case 0:
                            default:
                                options.hdrTransferFunction = ColorTransferFunction::PQ;
                                break;
                            }
                        }
                    }
                }
                else if (item == IDC_QUALITY_EDIT && cmd == EN_CHANGE)
                {
                    BOOL translated;
                    value = static_cast<int>(GetDlgItemInt(hDlg, IDC_QUALITY_EDIT, &translated, true));

                    if (translated && value >= 0 && value <= 100 && options.quality != value)
                    {
                        options.quality = value;
                        SendMessage(GetDlgItem(hDlg, IDC_QUALITY_SLIDER), TBM_SETPOS, TRUE, value);
                    }
                }
                break;
            case WM_HSCROLL:
                controlHwnd = reinterpret_cast<HWND>(lParam);

                switch (LOWORD(wParam))
                {
                case TB_LINEUP:
                case TB_LINEDOWN:
                case TB_PAGEUP:
                case TB_PAGEDOWN:
                case TB_THUMBTRACK:
                case TB_TOP:
                case TB_BOTTOM:
                case TB_ENDTRACK:
                    value = static_cast<int>(SendMessage(controlHwnd, TBM_GETPOS, 0, 0));

                    if (options.quality != value)
                    {
                        options.quality = value;
                        SetDlgItemInt(hDlg, IDC_QUALITY_EDIT, static_cast<UINT>(value), true);
                    }
                    break;
                }
                break;
            }

            return FALSE;
        }

        SaveUIOptions options;
        const int16 hostImageDepth;
        const bool hasColorProfile;
        const bool hasExif;
        const bool hasXmp;
        const bool hasAlphaChannel;
        const bool monochrome;
        bool imageDepthComboEnabled;
        bool colorProfileCheckboxEnabled;
        bool colorProfileChecked;
        bool premultipliedAlphaCheckboxEnabled;
        bool premultipliedAlphaChecked;
    };
}

void DoAbout(const AboutRecordPtr aboutRecord)
{
    PlatformData* platform = static_cast<PlatformData*>(aboutRecord->platformData);

    HWND parent = platform != nullptr ? reinterpret_cast<HWND>(platform->hwnd) : nullptr;

    DialogBoxParam(GetModuleInstanceHandle(), MAKEINTRESOURCE(IDD_ABOUT), parent, AboutDlgProc, 0);
}

bool DoLoadUI(const FormatRecordPtr formatRecord, LoadUIOptions& options)
{
    PlatformData* platform = static_cast<PlatformData*>(formatRecord->platformData);

    HWND parent = platform != nullptr ? reinterpret_cast<HWND>(platform->hwnd) : nullptr;

    LoadDialog dialog(options);

    if (DialogBoxParam(
        GetModuleInstanceHandle(),
        MAKEINTRESOURCE(IDD_LOAD),
        parent,
        LoadDialog::StaticDlgProc,
        reinterpret_cast<LPARAM>(&dialog)) == IDOK)
    {
        const LoadUIOptions& dialogOptions = dialog.GetLoadOptions();

        options.applyHLGOOTF = dialogOptions.applyHLGOOTF;
        options.displayGamma = dialogOptions.displayGamma;
        options.nominalPeakBrightness = dialogOptions.nominalPeakBrightness;

        return true;
    }
    else
    {
        return false;
    }
}

bool DoSaveUI(const FormatRecordPtr formatRecord, SaveUIOptions& options)
{
    PlatformData* platform = static_cast<PlatformData*>(formatRecord->platformData);

    HWND parent = platform != nullptr ? reinterpret_cast<HWND>(platform->hwnd) : nullptr;

    SaveDialog dialog(formatRecord, options);

    if (DialogBoxParam(
        GetModuleInstanceHandle(),
        MAKEINTRESOURCE(IDD_SAVE),
        parent,
        SaveDialog::StaticDlgProc,
        reinterpret_cast<LPARAM>(&dialog)) == IDOK)
    {
        const SaveUIOptions& dialogOptions = dialog.GetSaveOptions();

        options.quality = dialogOptions.quality;
        options.chromaSubsampling = dialogOptions.chromaSubsampling;
        options.compressionSpeed = dialogOptions.compressionSpeed;
        options.lossless = dialogOptions.lossless;
        options.losslessAlpha = dialogOptions.losslessAlpha;
        options.imageBitDepth = dialogOptions.imageBitDepth;
        options.hdrTransferFunction = dialogOptions.hdrTransferFunction;
        options.keepColorProfile = dialogOptions.keepColorProfile;
        options.keepExif = dialogOptions.keepExif;
        options.keepXmp = dialogOptions.keepXmp;
        options.premultipliedAlpha = dialogOptions.premultipliedAlpha;

        return true;
    }
    else
    {
        return false;
    }
}

OSErr ShowErrorDialog(const FormatRecordPtr formatRecord, const char* const message, OSErr fallbackErrorCode)
{
    PlatformData* platformData = static_cast<PlatformData*>(formatRecord->platformData);

    HWND parent = platformData != nullptr ? reinterpret_cast<HWND>(platformData->hwnd) : nullptr;

    if (MessageBoxA(parent, message, "AV1 Image File Format", MB_OK | MB_ICONERROR) == IDOK)
    {
        // Any positive number is a plug-in handled error message.
        return 1;
    }
    else
    {
        return fallbackErrorCode;
    }
}

