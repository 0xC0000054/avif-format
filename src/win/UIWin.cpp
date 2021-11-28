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

    class SaveDialog
    {
    public:

        SaveDialog(const FormatRecordPtr formatRecord, const SaveUIOptions& saveOptions) :
            hostImageDepth(formatRecord->depth),
            hasColorProfile(HasColorProfileMetadata(formatRecord)),
            hasExif(HasExifMetadata(formatRecord)),
            hasXmp(HasXmpMetadata(formatRecord)),
            losslessCheckboxEnabled(formatRecord->depth == 8)
        {
            options.quality = saveOptions.quality;
            options.chromaSubsampling = saveOptions.chromaSubsampling;
            options.compressionSpeed = saveOptions.compressionSpeed;
            options.imageBitDepth = formatRecord->depth == 8 ? ImageBitDepth::Eight : saveOptions.imageBitDepth;
            options.lossless = saveOptions.lossless && losslessCheckboxEnabled;
            options.keepColorProfile = saveOptions.keepColorProfile && hasColorProfile;
            options.keepExif = saveOptions.keepExif && hasExif;
            options.keepXmp = saveOptions.keepXmp && hasXmp;
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
            EnableWindow(GetDlgItem(hDlg, IDC_CHROMA_SUBSAMPLING_COMBO), enabled);
        }

        void InitializeDialog(HWND hDlg) noexcept
        {
            const HINSTANCE hInstance = GetModuleInstanceHandle();

            HWND qualitySlider = GetDlgItem(hDlg, IDC_QUALITY);
            HWND qualityEditBox = GetDlgItem(hDlg, IDC_QUALITY_EDIT);
            HWND qualityEditUpDown = GetDlgItem(hDlg, IDC_QUALITY_EDIT_SPIN);
            HWND losslessCheckbox = GetDlgItem(hDlg, IDC_LOSSLESS_CHECK);
            HWND chromaSubsamplingCombo = GetDlgItem(hDlg, IDC_CHROMA_SUBSAMPLING_COMBO);
            HWND keepColorProfileCheckbox = GetDlgItem(hDlg, IDC_KEEP_COLOR_PROFILE_CHECK);
            HWND keepExifCheckbox = GetDlgItem(hDlg, IDC_KEEP_EXIF_CHECK);
            HWND keepXmpCheckbox = GetDlgItem(hDlg, IDC_KEEP_XMP_CHECK);
            HWND pixelDepthCombo = GetDlgItem(hDlg, IDC_IMAGE_DEPTH_COMBO);

            SendMessage(qualitySlider, TBM_SETRANGEMIN, FALSE, 0);
            SendMessage(qualitySlider, TBM_SETRANGEMAX, FALSE, 100);
            SendMessage(qualitySlider, TBM_SETPOS, TRUE, options.quality);
            SendMessage(qualitySlider, TBM_SETBUDDY, FALSE, reinterpret_cast<LPARAM>(qualityEditBox));

            SendMessage(qualityEditBox, EM_LIMITTEXT, 3, 0);
            SetDlgItemInt(hDlg, IDC_QUALITY_EDIT, static_cast<UINT>(options.quality), false);

            SendMessage(qualityEditUpDown, UDM_SETBUDDY, reinterpret_cast<WPARAM>(qualityEditBox), 0);
            SendMessage(qualityEditUpDown, UDM_SETRANGE, 0, MAKELPARAM(100, 0));

            // The AVIF format only supports 10-bit and 12-bit data, so saving a 16-bit image may be lossy.
            if (hostImageDepth == 8)
            {
                Button_SetCheck(losslessCheckbox, options.lossless);
                EnableWindow(losslessCheckbox, true);
                EnableLossyCompressionSettings(hDlg, !options.lossless);
            }
            else
            {
                Button_SetCheck(losslessCheckbox, false);
                EnableWindow(losslessCheckbox, false);
            }

            // Swap the tab order of the Chroma Subsampling combo box and the Default compression speed radio button.
            SetWindowPos(chromaSubsamplingCombo, GetDlgItem(hDlg, IDC_COMPRESSION_SPEED_DEFAULT_RADIO), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            std::array<UINT, 3> chromaSubsamplingResIds = { IDS_CHROMA_SUBSAMPLING_420,
                                                            IDS_CHROMA_SUBSAMPLING_422,
                                                            IDS_CHROMA_SUBSAMPLING_444 };

            constexpr int resourceBufferLength = 256;
            TCHAR resourceBuffer[resourceBufferLength]{};

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

            if (hasColorProfile)
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
                    case IDC_LOSSLESS_CHECK:
                        options.lossless = Button_GetCheck(controlHwnd) == BST_CHECKED;
                        EnableLossyCompressionSettings(hDlg, !options.lossless);
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

                        if (hostImageDepth == 8)
                        {
                            // Lossless mode is only supported when the host image depth and output
                            // image depth are both 8-bits-per-channel.

                            if (options.imageBitDepth != ImageBitDepth::Eight)
                            {
                                if (losslessCheckboxEnabled)
                                {
                                    losslessCheckboxEnabled = false;
                                    HWND losslessCheck = GetDlgItem(hDlg, IDC_LOSSLESS_CHECK);

                                    if (Button_GetCheck(losslessCheck) == BST_CHECKED)
                                    {
                                        Button_SetCheck(losslessCheck, BST_UNCHECKED);
                                        options.lossless = false;
                                        EnableLossyCompressionSettings(hDlg, true);
                                    }

                                    EnableWindow(losslessCheck, false);
                                }
                            }
                            else
                            {
                                if (!losslessCheckboxEnabled)
                                {
                                    losslessCheckboxEnabled = true;

                                    EnableWindow(GetDlgItem(hDlg, IDC_LOSSLESS_CHECK), true);
                                }
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
        bool losslessCheckboxEnabled; // Used to track state when changing the save bit-depth.
    };
}

void DoAbout(const AboutRecordPtr aboutRecord)
{
    PlatformData* platform = static_cast<PlatformData*>(aboutRecord->platformData);

    HWND parent = platform != nullptr ? reinterpret_cast<HWND>(platform->hwnd) : nullptr;

    DialogBoxParam(GetModuleInstanceHandle(), MAKEINTRESOURCE(IDD_ABOUT), parent, AboutDlgProc, 0);
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
        options.imageBitDepth = dialogOptions.imageBitDepth;
        options.keepColorProfile = dialogOptions.keepColorProfile;
        options.keepExif = dialogOptions.keepExif;
        options.keepXmp = dialogOptions.keepXmp;

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

