// Common Start-up Window, Windows variant
// for the Build Engine

// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0600

#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <windowsx.h>
#include <strsafe.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <stdio.h>

#include "startwin.h"
#include "startwin_priv.h"
#include "startwin_win.h"
#include "build.h"
#include "winlayer.h"

static HWND startupdlg;

#define TAB_CONFIG 0
#define TAB_GAME 1
#define TAB_MESSAGES 2
static struct {
    int index;
    HWND hwnd;
} tabs[3];

static BOOL quiteventonclose = FALSE;
static int retval = -1;

static void populate_video_modes(BOOL firsttime)
{
    int i, j, mode2d = -1, mode3d = -1, idx2d = -1, idx3d = -1;
    int xdim3d = 0, ydim3d = 0, bitspp = 0, display = 0, fullsc = 0;
    int xdim2d = 0, ydim2d = 0;
    TCHAR modestr[64];
    int cd[] = { 32, 24, 16, 15, 8, 0 };
    HWND hwnd3d, hwnd2d, hwnddisp;

    hwnd3d = GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_VMODE3D);
    hwnd2d = GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_VMODE2D);
    hwnddisp = GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_DISPLAY);
    if (firsttime) {
        getvalidmodes();
        if (startwin_settings.features.video) {
            xdim3d = startwin_settings.video.xdim;
            ydim3d = startwin_settings.video.ydim;
            bitspp = startwin_settings.video.bpp;
            fullsc = startwin_settings.video.fullscreen;
            display = min(displaycnt-1, max(0, startwin_settings.video.display));

            ComboBox_ResetContent(hwnddisp);
            for (i = 0; i < displaycnt; i++) {
                StringCbPrintf(modestr, sizeof(modestr), TEXT("Display %d - %s"), i, getdisplayname(i));
                j = ComboBox_AddString(hwnddisp, modestr);
                ComboBox_SetItemData(hwnddisp, j, i);
            }
            if (displaycnt < 2) ShowWindow(hwnddisp, SW_HIDE);
        }
        if (startwin_settings.features.editor) {
            xdim2d = startwin_settings.editor.xdim;
            ydim2d = startwin_settings.editor.ydim;
        }
    } else {
        fullsc = IsDlgButtonChecked(tabs[TAB_CONFIG].hwnd, IDC_FULLSCREEN) == BST_CHECKED;
        if (fullsc) {
            i = ComboBox_GetCurSel(hwnddisp);
            if (i >= CB_OKAY) i = ComboBox_GetItemData(hwnddisp, i);
            if (i >= CB_OKAY) display = max(0, i);
        }

        i = ComboBox_GetCurSel(hwnd3d);
        if (i >= CB_OKAY) i = ComboBox_GetItemData(hwnd3d, i);
        if (i >= CB_OKAY) {
            mode3d = i;
            xdim3d = validmode[i].xdim;
            ydim3d = validmode[i].ydim;
            bitspp = validmode[i].bpp;
        }

        i = ComboBox_GetCurSel(hwnd2d);
        if (i >= CB_OKAY) i = ComboBox_GetItemData(hwnd2d, i);
        if (i >= CB_OKAY) {
            mode2d = i;
            xdim2d = validmode[i].xdim;
            ydim2d = validmode[i].ydim;
        }
    }

    // Find an ideal match.
    mode3d = checkvideomode(&xdim3d, &ydim3d, bitspp, SETGAMEMODE_FULLSCREEN(display, fullsc), 1);
    mode2d = checkvideomode(&xdim2d, &ydim2d, 8, SETGAMEMODE_FULLSCREEN(display, fullsc), 1);
    if (mode2d < 0) mode2d = 0;
    for (i=0; mode3d < 0 && cd[i]; i++) {
        mode3d = checkvideomode(&xdim3d, &ydim3d, cd[i], SETGAMEMODE_FULLSCREEN(display, fullsc), 1);
    }
    if (mode3d < 0) mode3d = 0;
    fullsc = validmode[mode3d].fs;
    display = validmode[mode3d].display;

    // Repopulate the lists.
    ComboBox_ResetContent(hwnd3d);
    ComboBox_ResetContent(hwnd2d);
    for (i=0; i<validmodecnt; i++) {
        if (validmode[i].fs != fullsc) continue;
        if (validmode[i].display != display) continue;

        StringCbPrintf(modestr, sizeof(modestr), TEXT("%d x %d %d-bpp"),
            validmode[i].xdim, validmode[i].ydim, validmode[i].bpp);
        j = ComboBox_AddString(hwnd3d, modestr);
        ComboBox_SetItemData(hwnd3d, j, i);
        if (i == mode3d || idx3d < 0) idx3d = j;

        if (validmode[i].bpp == 8 && validmode[i].xdim >= 640 && validmode[i].ydim >= 480) {
            StringCbPrintf(modestr, sizeof(modestr), TEXT("%d x %d"),
                validmode[i].xdim, validmode[i].ydim);
            j = ComboBox_AddString(hwnd2d, modestr);
            ComboBox_SetItemData(hwnd2d, j, i);
            if (i == mode2d || idx2d < 0) idx2d = j;
        }
    }
    if (idx2d >= 0) ComboBox_SetCurSel(hwnd2d, idx2d);
    if (idx3d >= 0) ComboBox_SetCurSel(hwnd3d, idx3d);

    ComboBox_SetCurSel(hwnddisp, validmode[mode3d].display);
    EnableWindow(hwnddisp, validmode[mode3d].fs ? TRUE : FALSE);
    CheckDlgButton(tabs[TAB_CONFIG].hwnd, IDC_FULLSCREEN, validmode[mode3d].fs ? BST_CHECKED : BST_UNCHECKED);
}

static void populate_sound_quality(BOOL firsttime)
{
    int i, j, qual = -1, idx = -1;
    int samplerate = 0, bitspersample = 0, channels = 0;
    TCHAR modestr[64];
    HWND hwnd;

    hwnd = GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_SOUNDQUALITY);
    if (firsttime) {
        samplerate = startwin_settings.audio.samplerate;
        bitspersample = startwin_settings.audio.bitspersample;
        channels = startwin_settings.audio.channels;
        for (i = 0; qual < 0 && startwin_soundqualities[i].frequency; i++) {
            if (startwin_soundqualities[i].frequency == samplerate &&
                startwin_soundqualities[i].samplesize == bitspersample &&
                startwin_soundqualities[i].channels == channels) qual = i;
        }
    } else {
        i = ComboBox_GetCurSel(hwnd);
        if (i >= CB_OKAY) i = ComboBox_GetItemData(hwnd, i);
        if (i >= CB_OKAY) {
            qual = i;
            samplerate = startwin_soundqualities[i].frequency;
            bitspersample = startwin_soundqualities[i].samplesize;
            channels = startwin_soundqualities[i].channels;
        }
    }

    ComboBox_ResetContent(hwnd);
    for (i = 0; startwin_soundqualities[i].frequency; i++) {
        StringCbPrintf(modestr, sizeof(modestr), TEXT("%d kHz, %d-bit, %s"),
            startwin_soundqualities[i].frequency / 1000,
            startwin_soundqualities[i].samplesize,
            startwin_soundqualities[i].channels == 1 ? "Mono" : "Stereo");
        j = ComboBox_AddString(hwnd, modestr);
        ComboBox_SetItemData(hwnd, j, i);
        if (i == qual || idx < 0) idx = j;
    }
    if (idx >= 0) ComboBox_SetCurSel(hwnd, idx);

}

static void populate_game_list(BOOL firsttime)
{
    const struct startwin_datasetfound * datasetp;
    HWND hwnd;
    int sel = -1, oldid = -1, i;
    LVITEM lvi;

    hwnd = GetDlgItem(tabs[TAB_GAME].hwnd, IDC_GAMELIST);

    if (!firsttime) {
        i = ListView_GetNextItem(hwnd, -1, LVNI_SELECTED);
        if (i >= 0) {
            ZeroMemory(&lvi, sizeof(lvi));
            lvi.mask = LVIF_PARAM;
            lvi.iItem = i;
            if (ListView_GetItem(hwnd, &lvi)) oldid = (int)lvi.lParam;
        }
    }

    ListView_DeleteAllItems(hwnd);

    for (i = 0, datasetp = startwin_scan_gamedata(); datasetp; datasetp = datasetp->next) {
        if (!datasetp->complete) continue;

        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask = LVIF_PARAM | LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPTSTR)datasetp->dataset->name;
        lvi.lParam = (LPARAM)datasetp->dataset->id;
        ListView_InsertItem(hwnd, &lvi);

        const struct startwin_datasetfoundfile *grp = startwin_find_dataset_group(datasetp);
        if (grp) ListView_SetItemText(hwnd, i, 1, (LPTSTR)grp->name);

        if (oldid == datasetp->dataset->id) sel = 1;
        else if (sel < 0 && datasetp->dataset->id == startwin_settings.game.gamedataid) sel = 1;
        if (sel == 1) {
            ListView_SetItemState(hwnd, i, LVIS_SELECTED, LVIS_SELECTED);
            sel = 0;
        }
        i++;
    }
    if (sel < 0 && i) ListView_SetItemState(hwnd, 0, LVIS_SELECTED, LVIS_SELECTED);
}

static void set_tab(int n)
{
    HWND tab = GetDlgItem(startupdlg, IDC_STARTWIN_TABCTL);
    int cur = TabCtrl_GetCurSel(tab);

    ShowWindow(tabs[cur].hwnd, SW_HIDE);
    TabCtrl_SetCurSel(tab, tabs[n].index);
    ShowWindow(tabs[n].hwnd, SW_SHOW);

    SendMessage(startupdlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(startupdlg, IDC_STARTWIN_TABCTL), TRUE);
}

struct version_info {
    LPVOID *data;
    TCHAR *filedescription;
    TCHAR *fileversion;
    TCHAR *url;
};

static int get_version_info(struct version_info *vi)
{
    TCHAR exefn[1024], key[64];
    int fvisiz;
    LPVOID fvidata;
    LPWORD tl;
    UINT ulen;

    if (!GetModuleFileName(NULL, exefn, Barraylen(exefn))) return 0;
    if (!(fvisiz = GetFileVersionInfoSize(exefn, NULL))) return 0;
    if (!(fvidata = malloc(fvisiz))) return 0;
    do {
        if (!GetFileVersionInfo(exefn, 0, fvisiz, fvidata)) break;
        if (!VerQueryValue(fvidata, TEXT("\\VarFileInfo\\Translation"), (LPVOID*)&tl, &ulen)) break;

        StringCbPrintf(key, sizeof(key), TEXT("\\StringFileInfo\\%04x%04x\\%s"), tl[0], tl[1], "FileDescription");
        VerQueryValue(fvidata, key, (LPVOID*)&vi->filedescription, &ulen);
        StringCbPrintf(key, sizeof(key), TEXT("\\StringFileInfo\\%04x%04x\\%s"), tl[0], tl[1], "FileVersion");
        VerQueryValue(fvidata, key, (LPVOID*)&vi->fileversion, &ulen);
        StringCbPrintf(key, sizeof(key), TEXT("\\StringFileInfo\\%04x%04x\\%s"), tl[0], tl[1], "URL");
        VerQueryValue(fvidata, key, (LPVOID*)&vi->url, &ulen);
        vi->data = fvidata;

        return 1;
    } while(0);

    free(fvidata);
    return 0;
}

struct configure_enum_data {
    RECT search;
    int hide;
    int offset;
};

static BOOL CALLBACK configure_enum(HWND hwnd, LPARAM lParam) {
    struct configure_enum_data *ceo = (struct configure_enum_data *)lParam;
    RECT rect;

    GetWindowRect(hwnd, &rect);
    if (PtInRect(&ceo->search, *(LPPOINT)&rect)) {
        rect.top += ceo->offset;
        ScreenToClient(tabs[TAB_CONFIG].hwnd, (LPPOINT)&rect);
        SetWindowPos(hwnd, NULL, rect.left, rect.top, 0, 0,
            (ceo->hide ? SWP_HIDEWINDOW : SWP_SHOWWINDOW) | SWP_NOZORDER | SWP_NOSIZE);
    }

    return TRUE;
}

static void configure(BOOL firsttime)
{
    HWND hwnd;

    if (firsttime) {
        struct version_info vi = {0};
        if (get_version_info(&vi)) {
            TCHAR str[64];

            if (vi.filedescription) {
                StringCbPrintf(str, sizeof(str), TEXT("%s"), vi.filedescription);
                SetWindowText(GetDlgItem(startupdlg, IDC_STARTWIN_APPTITLE), str);
            }
            if (vi.fileversion) {
                StringCbPrintf(str, sizeof(str), TEXT("Version %s"), vi.fileversion);
                SetWindowText(GetDlgItem(startupdlg, IDC_STARTWIN_APPVERSION), str);
            }
            if (vi.url) {
                StringCbPrintf(str, sizeof(str), TEXT("<A HREF=\"%s\">Website</A>"), vi.url);
                SetWindowText(GetDlgItem(startupdlg, IDC_STARTWIN_APPLINK), str);
            }
            free(vi.data);
        }
        return;
    }

    int offset = 0;
    const struct {
        int id;
        int enabled;
    } views[] = {
        { IDC_CONFIG_FEATURE_VIDEO,   startwin_settings.features.video   },
        { IDC_CONFIG_FEATURE_EDITOR,  startwin_settings.features.editor  },
        { IDC_CONFIG_FEATURE_AUDIO,   startwin_settings.features.audio   },
        { IDC_CONFIG_FEATURE_INPUT,   startwin_settings.features.input   },
        { IDC_CONFIG_FEATURE_NETWORK, startwin_settings.features.network },
    };
    for (size_t i=0; i<Barraylen(views); i++) {
        struct configure_enum_data oper = { .offset = offset };
        GetWindowRect(GetDlgItem(tabs[TAB_CONFIG].hwnd, views[i].id), &oper.search);
        if (!views[i].enabled) {
            offset -= oper.search.bottom - oper.search.top;
            oper.hide = 1;
        }
        EnumChildWindows(tabs[TAB_CONFIG].hwnd, configure_enum, (LPARAM)&oper);
    }

    hwnd = GetDlgItem(startupdlg, IDC_STARTWIN_TABCTL);

    tabs[TAB_CONFIG].index = TabCtrl_InsertItem(hwnd, 0, (&(TCITEM) {
        .mask = TCIF_TEXT | TCIF_PARAM,
        .pszText = TEXT("Configuration"),
        .lParam = TAB_CONFIG,
    }));
    tabs[TAB_MESSAGES].index++;

    if (startwin_settings.features.game) {
        tabs[TAB_GAME].index = TabCtrl_InsertItem(hwnd, 1, (&(TCITEM) {
            .mask = TCIF_TEXT | TCIF_PARAM,
            .pszText = TEXT("Game"),
            .lParam = TAB_GAME,
        }));
        tabs[TAB_MESSAGES].index++;
    }
}

static void setup_config_mode(void)
{
    CheckDlgButton(startupdlg, IDC_ALWAYSSHOW, (startwin_settings.alwaysshow ? BST_CHECKED : BST_UNCHECKED));
    EnableWindow(GetDlgItem(startupdlg, IDC_ALWAYSSHOW), TRUE);

    if (startwin_settings.features.video || startwin_settings.features.editor) {
        if (startwin_settings.features.video) {
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_VMODE3D), TRUE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_FULLSCREEN), TRUE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_DISPLAY), TRUE);
        }
        if (startwin_settings.features.editor) {
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_VMODE2D), TRUE);
        }
        populate_video_modes(TRUE);
    }
    if (startwin_settings.features.audio) {
        EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_SOUNDQUALITY), TRUE);
        populate_sound_quality(TRUE);
    }
    if (startwin_settings.features.input) {
        EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_USEMOUSE), TRUE);
        CheckDlgButton(tabs[TAB_CONFIG].hwnd, IDC_USEMOUSE, (startwin_settings.input.mouse ?
            BST_CHECKED : BST_UNCHECKED));
        EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_USEJOYSTICK), TRUE);
        CheckDlgButton(tabs[TAB_CONFIG].hwnd, IDC_USEJOYSTICK, (startwin_settings.input.controller ?
            BST_CHECKED : BST_UNCHECKED));
    }
    if (startwin_settings.features.network) {
        if (!startwin_settings.network.netoverride) {
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_SINGLEPLAYER), TRUE);
            CheckRadioButton(tabs[TAB_CONFIG].hwnd, IDC_SINGLEPLAYER, IDC_HOSTMULTIPLAYER, IDC_SINGLEPLAYER);

            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_JOINMULTIPLAYER), TRUE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_HOSTFIELD), FALSE);

            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_HOSTMULTIPLAYER), TRUE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERS), FALSE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERSUD), FALSE);
            SetDlgItemInt(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERS, 2, TRUE);
            SendDlgItemMessage(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERSUD, UDM_SETPOS, 0, 2);
            SendDlgItemMessage(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERSUD, UDM_SETRANGE, 0, MAKELPARAM(MAXPLAYERS, 2));
        } else {
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_SINGLEPLAYER), FALSE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_JOINMULTIPLAYER), FALSE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_HOSTFIELD), FALSE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_HOSTMULTIPLAYER), FALSE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERS), FALSE);
            EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERSUD), FALSE);
        }
    }
    if (startwin_settings.features.game) {
        EnableWindow(GetDlgItem(tabs[TAB_GAME].hwnd, IDC_GAMELIST), TRUE);
        EnableWindow(GetDlgItem(tabs[TAB_GAME].hwnd, IDC_CHOOSEIMPORT), TRUE);
        EnableWindow(GetDlgItem(tabs[TAB_GAME].hwnd, IDC_IMPORTINFO), TRUE);
        populate_game_list(TRUE);
    }

    if (startwin_settings.features.game && !startwin_settings.game.gamedataid) set_tab(TAB_GAME);
    else set_tab(TAB_CONFIG);

    EnableWindow(GetDlgItem(startupdlg, IDCANCEL), TRUE);
    EnableWindow(GetDlgItem(startupdlg, IDOK), TRUE);
}

static void setup_messages_mode(BOOL allowcancel)
{
    set_tab(TAB_MESSAGES);

    for (int i=IDC_CONFIG_FEATURE_VIDEO; i<IDC_CONFIG_FEATURE_MAX; i++)
        EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, i), FALSE);

    EnableWindow(GetDlgItem(tabs[TAB_GAME].hwnd, IDC_GAMELIST), FALSE);
    EnableWindow(GetDlgItem(tabs[TAB_GAME].hwnd, IDC_CHOOSEIMPORT), FALSE);
    EnableWindow(GetDlgItem(tabs[TAB_GAME].hwnd, IDC_IMPORTINFO), FALSE);

    EnableWindow(GetDlgItem(startupdlg, IDC_ALWAYSSHOW), FALSE);

    EnableWindow(GetDlgItem(startupdlg, IDCANCEL), allowcancel);
    EnableWindow(GetDlgItem(startupdlg, IDOK), FALSE);
}

static void fullscreen_clicked(void)
{
    populate_video_modes(FALSE);
}

static void display_changed(void)
{
    populate_video_modes(FALSE);
}

static void multiplayerradio_clicked(int sender)
{
    EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_HOSTFIELD), (sender == IDC_JOINMULTIPLAYER));
    EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERS), (sender == IDC_HOSTMULTIPLAYER));
    EnableWindow(GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERSUD), (sender == IDC_HOSTMULTIPLAYER));

    CheckRadioButton(tabs[TAB_CONFIG].hwnd, IDC_SINGLEPLAYER, IDC_HOSTMULTIPLAYER, sender);
}

static void cancelbutton_clicked(void)
{
    retval = STARTWIN_CANCEL;
    quitevent = quitevent || quiteventonclose;
}

static void startbutton_clicked(void)
{
    int i;
    HWND hwnd;

    if (startwin_settings.features.video) {
        hwnd = GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_VMODE3D);
        i = ComboBox_GetCurSel(hwnd);
        if (i >= CB_OKAY) i = ComboBox_GetItemData(hwnd, i);
        if (i >= CB_OKAY) {
            startwin_settings.video.xdim = validmode[i].xdim;
            startwin_settings.video.ydim = validmode[i].ydim;
            startwin_settings.video.bpp = validmode[i].bpp;
            startwin_settings.video.fullscreen = validmode[i].fs;
            startwin_settings.video.display = validmode[i].display;
        }
    }
    if (startwin_settings.features.editor) {
        hwnd = GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_VMODE2D);
        i = ComboBox_GetCurSel(hwnd);
        if (i >= CB_OKAY) i = ComboBox_GetItemData(hwnd, i);
        if (i >= CB_OKAY) {
            startwin_settings.editor.xdim = validmode[i].xdim;
            startwin_settings.editor.ydim = validmode[i].ydim;
        }
    }
    if (startwin_settings.features.input) {
        startwin_settings.input.mouse = IsDlgButtonChecked(tabs[TAB_CONFIG].hwnd, IDC_USEMOUSE) == BST_CHECKED;
        startwin_settings.input.controller = IsDlgButtonChecked(tabs[TAB_CONFIG].hwnd, IDC_USEJOYSTICK) == BST_CHECKED;
    }
    if (startwin_settings.features.audio) {
        hwnd = GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_SOUNDQUALITY);
        i = ComboBox_GetCurSel(hwnd);
        if (i >= CB_OKAY) i = ComboBox_GetItemData(hwnd, i);
        if (i >= CB_OKAY) {
            startwin_settings.audio.samplerate = startwin_soundqualities[i].frequency;
            startwin_settings.audio.bitspersample = startwin_soundqualities[i].samplesize;
            startwin_settings.audio.channels = startwin_soundqualities[i].channels;
        }
    }
    if (startwin_settings.features.network) {
        startwin_settings.network.numplayers = 0;
        startwin_settings.network.joinhost = 0;
        if (IsDlgButtonChecked(tabs[TAB_CONFIG].hwnd, IDC_SINGLEPLAYER) == BST_CHECKED) {
            startwin_settings.network.numplayers = 1;
        } else if (IsDlgButtonChecked(tabs[TAB_CONFIG].hwnd, IDC_JOINMULTIPLAYER) == BST_CHECKED) {
            int joinhostlen, wcharlen;
            WCHAR *wcharstr;

            startwin_settings.network.numplayers = 2;

            hwnd = GetDlgItem(tabs[TAB_CONFIG].hwnd, IDC_HOSTFIELD);
            wcharlen = GetWindowTextLengthW(hwnd) + 1;
            wcharstr = (WCHAR *)malloc(wcharlen * sizeof(WCHAR));
            GetWindowTextW(hwnd, wcharstr, wcharlen);

            joinhostlen = WideCharToMultiByte(CP_UTF8, 0, wcharstr, -1, NULL, 0, NULL, NULL);
            startwin_settings.network.joinhost = (char *)malloc(joinhostlen + 1);
            WideCharToMultiByte(CP_UTF8, 0, wcharstr, -1, startwin_settings.network.joinhost, joinhostlen, NULL, NULL);

            free(wcharstr);
        } else if (IsDlgButtonChecked(tabs[TAB_CONFIG].hwnd, IDC_HOSTMULTIPLAYER) == BST_CHECKED) {
            startwin_settings.network.numplayers = (int)GetDlgItemInt(tabs[TAB_CONFIG].hwnd, IDC_NUMPLAYERS, NULL, TRUE);
        }
    }
    if (startwin_settings.features.game) {
        // Get the chosen game entry.
        hwnd = GetDlgItem(tabs[TAB_GAME].hwnd, IDC_GAMELIST);
        i = ListView_GetNextItem(hwnd, -1, LVNI_SELECTED);
        if (i >= 0) {
            LVITEM lvi;
            ZeroMemory(&lvi, sizeof(lvi));
            lvi.mask = LVIF_PARAM;
            lvi.iItem = i;
            if (ListView_GetItem(hwnd, &lvi)) startwin_settings.game.gamedataid = (int)lvi.lParam;
        }
    }

    startwin_settings.alwaysshow = IsDlgButtonChecked(startupdlg, IDC_ALWAYSSHOW) == BST_CHECKED;

    retval = STARTWIN_RUN;
}

struct importstatus_data {
    HWND hwndDlg;
    BOOL cancelled;
};

static INT_PTR CALLBACK importstatus_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG:
            SetWindowLongPtr(hwndDlg, DWLP_USER, lParam);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDCANCEL: {
                    struct importstatus_data *status = (struct importstatus_data *)GetWindowLongPtr(hwndDlg, DWLP_USER);
                    status->cancelled = TRUE;
                    return TRUE;
                }
            }
            break;

        default: break;
    }

    return FALSE;
}

static void import_progress(void *data, const char *path)
{
    struct importstatus_data *status = (struct importstatus_data *)data;
    SetDlgItemText(status->hwndDlg, IDC_IMPORTSTATUS_TEXT, path);
}

static int import_cancelled(void *data)
{
    struct importstatus_data *status = (struct importstatus_data *)data;
    MSG msg;

    // One iteration of the a modal dialogue box event loop.
    msg.message = WM_NULL;
    if (!status->cancelled) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                status->cancelled = TRUE;
            } else if (!IsDialogMessage(status->hwndDlg, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    return status->cancelled;
}

static void chooseimport_clicked(void)
{
    BROWSEINFO info;
    LPITEMIDLIST item;
    TCHAR displayname[MAX_PATH] = {0};
    char filename[BMAX_PATH+1] = "";

    ZeroMemory(&info, sizeof(info));
    info.hwndOwner = startupdlg;
    info.pszDisplayName = displayname;
    info.lpszTitle = "Select a folder to search for game data.";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON;
    item = SHBrowseForFolder(&info);
    if (item != NULL) {
        if (SHGetPathFromIDList(item, filename)) {
            struct importstatus_data status = {0};
            struct startwin_import_meta meta = {
                (void *)&status,
                0,
                import_progress,
                import_cancelled
            };

            status.hwndDlg = CreateDialogParam((HINSTANCE)win_gethinstance(), MAKEINTRESOURCE(IDD_IMPORTSTATUS),
                startupdlg, importstatus_dlgproc, (LPARAM)&status);

            EnableWindow(startupdlg, FALSE);

            if (startwin_import_path(filename, &meta) >= STARTWIN_IMPORT_OK) {
                populate_game_list(FALSE);
            }

            EnableWindow(startupdlg, TRUE);
            DestroyWindow(status.hwndDlg);
        }
        CoTaskMemFree(item);
    }
}

static INT_PTR CALLBACK importinfo_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;

    switch (uMsg) {
        case WM_INITDIALOG: {
            SetDlgItemText(hwndDlg, IDC_IMPORTINFO_HEADER, startwin_settings.game.moreinfobrief);
            SetDlgItemText(hwndDlg, IDC_IMPORTINFO_TEXT, startwin_settings.game.moreinfodetail);
            if (!startwin_settings.game.demourl) ShowWindow(GetDlgItem(hwndDlg, IDCONTINUE), SW_HIDE);

            {
                // Set the font of the header text.
                HFONT hfont = CreateFont(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    TEXT("MS Shell Dlg"));
                if (hfont) {
                    HWND hwnd = GetDlgItem(hwndDlg, IDC_IMPORTINFO_HEADER);
                    SendMessage(hwnd, WM_SETFONT, (WPARAM)hfont, FALSE);
                }
            }
            return TRUE;
        }

        case WM_DESTROY:
            // Dispose of the font used for the header text.
            {
                HWND hwnd = GetDlgItem(hwndDlg, IDC_IMPORTINFO_HEADER);
                HFONT hfont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
                if (hfont) {
                    DeleteObject(hfont);
                }
            }
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDCONTINUE:
                    EndDialog(hwndDlg, 1);
                    return TRUE;
                case IDOK:
                    EndDialog(hwndDlg, 0);
                    return TRUE;
            }
            break;

        default: break;
    }

    return FALSE;
}

static void importinfo_clicked(void)
{
    if (DialogBox((HINSTANCE)win_gethinstance(), MAKEINTRESOURCE(IDD_IMPORTINFO), startupdlg, importinfo_dlgproc) == 1) {
        ShellExecute(startupdlg, "open", startwin_settings.game.demourl, NULL, NULL, SW_SHOW);
    }
}

static INT_PTR CALLBACK ConfigPageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;

    switch (uMsg) {
        case WM_INITDIALOG: {
            EnableThemeDialogTexture(hwndDlg, ETDT_ENABLETAB);
            SendDlgItemMessage(hwndDlg, IDC_NUMPLAYERSUD, UDM_SETBUDDY, (WPARAM)GetDlgItem(hwndDlg, IDC_NUMPLAYERS), 0);
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FULLSCREEN:
                    fullscreen_clicked();
                    return TRUE;
                case IDC_DISPLAY:
                    display_changed();
                    return TRUE;

                case IDC_SINGLEPLAYER:
                case IDC_JOINMULTIPLAYER:
                case IDC_HOSTMULTIPLAYER:
                    multiplayerradio_clicked(LOWORD(wParam));
                    return TRUE;
                default: break;
            }
            break;
        default: break;
    }
    return FALSE;
}

static INT_PTR CALLBACK GamePageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;

    switch (uMsg) {
        case WM_INITDIALOG: {
            LVCOLUMN lvc;
            HWND hwnd;

            hwnd = GetDlgItem(hwndDlg, IDC_GAMELIST);
            ListView_SetExtendedListViewStyle(hwnd, LVS_EX_FULLROWSELECT);

            ZeroMemory(&lvc, sizeof(lvc));
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
            lvc.fmt = LVCFMT_LEFT;

            lvc.iSubItem = 0;
            lvc.pszText = TEXT("Game name");
            lvc.cx = 220;
            ListView_InsertColumn(hwnd, 0, &lvc);

            lvc.iSubItem = 1;
            lvc.pszText = TEXT("File name");
            lvc.cx = 150;
            ListView_InsertColumn(hwnd, 1, &lvc);

            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_CHOOSEIMPORT:
                    chooseimport_clicked();
                    return TRUE;
                case IDC_IMPORTINFO:
                    importinfo_clicked();
                    return TRUE;
            }
            break;
        default: break;
    }
    return FALSE;
}

static INT_PTR CALLBACK MessagesPageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;

    switch (uMsg) {
        case WM_CTLCOLORSTATIC:
            if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_MESSAGES))
                return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
            break;
    }
    return FALSE;
}

static INT_PTR CALLBACK StartupDlgProg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG: {
            HWND hwnd;
            RECT r;

            {
                hwnd = GetDlgItem(hwndDlg, IDC_STARTWIN_TABCTL);

                // At least one tab needs to exist for the rect adjustment to work.
                tabs[TAB_CONFIG].index = -1; // Initially excluded from the tab control.
                tabs[TAB_GAME].index = -1; // Initially excluded from the tab control.
                tabs[TAB_MESSAGES].index = TabCtrl_InsertItem(hwnd, 0, (&(TCITEM) {
                    .mask = TCIF_TEXT | TCIF_PARAM,
                    .pszText = TEXT("Messages"),
                    .lParam = TAB_MESSAGES,
                }));

                // Work out the position and size of the area inside the tab control for the tabs.
                ZeroMemory(&r, sizeof(r));
                GetClientRect(hwnd, &r);
                TabCtrl_AdjustRect(hwnd, FALSE, &r);

                // Create the tabs and position them in the tab control, but hide them.
                tabs[TAB_CONFIG].hwnd = CreateDialog((HINSTANCE)win_gethinstance(),
                    MAKEINTRESOURCE(IDD_PAGE_CONFIG), hwnd, ConfigPageProc);
                SetWindowPos(tabs[TAB_CONFIG].hwnd, NULL, r.left,r.top,r.right,r.bottom, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOSIZE);

                tabs[TAB_GAME].hwnd = CreateDialog((HINSTANCE)win_gethinstance(),
                    MAKEINTRESOURCE(IDD_PAGE_GAME), hwnd, GamePageProc);
                SetWindowPos(tabs[TAB_GAME].hwnd, NULL, r.left,r.top,r.right,r.bottom, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOSIZE);

                tabs[TAB_MESSAGES].hwnd = CreateDialog((HINSTANCE)win_gethinstance(),
                    MAKEINTRESOURCE(IDD_PAGE_MESSAGES), hwnd, MessagesPageProc);
                SetWindowPos(tabs[TAB_MESSAGES].hwnd, NULL, r.left,r.top,r.right,r.bottom, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOSIZE);

                SendMessage(hwndDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwndDlg, IDOK), TRUE);
            }
            {
                // Set the font of the application title.
                HFONT hfont = CreateFont(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    TEXT("MS Shell Dlg"));
                if (hfont) {
                    hwnd = GetDlgItem(hwndDlg, IDC_STARTWIN_APPTITLE);
                    SendMessage(hwnd, WM_SETFONT, (WPARAM)hfont, FALSE);
                }
            }
            return TRUE;
        }

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->idFrom == IDC_STARTWIN_TABCTL) {
                int cur = TabCtrl_GetCurSel(nmhdr->hwndFrom);
                if (cur < 0) break;

                TCITEM item = { .mask = TCIF_PARAM };
                TabCtrl_GetItem(nmhdr->hwndFrom, cur, &item);
                if (!tabs[item.lParam].hwnd) break;

                switch (nmhdr->code) {
                    case TCN_SELCHANGING:
                        ShowWindow(tabs[item.lParam].hwnd, SW_HIDE);
                        return TRUE;
                    case TCN_SELCHANGE:
                        ShowWindow(tabs[item.lParam].hwnd, SW_SHOW);
                        return TRUE;
                }
            }
            if (nmhdr->idFrom == IDC_STARTWIN_APPLINK) {
                PNMLINK pNMLink = (PNMLINK)lParam;

                if (nmhdr->code != NM_CLICK && nmhdr->code != NM_RETURN) {
                    break;
                }
                if (pNMLink->item.iLink == 0) {
                    ShellExecuteW(hwndDlg, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOW);
                }
            }
            break;
        }

        case WM_CLOSE:
            cancelbutton_clicked();
            return TRUE;

        case WM_DESTROY:
            if (tabs[TAB_CONFIG].hwnd) DestroyWindow(tabs[TAB_CONFIG].hwnd);
            if (tabs[TAB_GAME].hwnd) DestroyWindow(tabs[TAB_GAME].hwnd);
            if (tabs[TAB_MESSAGES].hwnd) DestroyWindow(tabs[TAB_MESSAGES].hwnd);
            tabs[TAB_CONFIG].hwnd = NULL;
            tabs[TAB_CONFIG].index = -1;
            tabs[TAB_GAME].hwnd = NULL;
            tabs[TAB_GAME].index = -1;
            tabs[TAB_MESSAGES].hwnd = NULL;
            tabs[TAB_MESSAGES].index = -1;

            // Dispose of the font used for the application title.
            {
                HWND hwnd = GetDlgItem(hwndDlg, IDC_STARTWIN_APPTITLE);
                HFONT hfont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
                if (hfont) {
                    DeleteObject(hfont);
                }
            }

            startupdlg = NULL;
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDCANCEL:
                    cancelbutton_clicked();
                    return TRUE;
                case IDOK: {
                    startbutton_clicked();
                    return TRUE;
                }
            }
            break;

        default: break;
    }

    return FALSE;
}


int startwin_open(void)
{
    INITCOMMONCONTROLSEX icc;

    if (startupdlg) return 1;

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_UPDOWN_CLASS | ICC_LISTVIEW_CLASSES | ICC_LINK_CLASS;
    InitCommonControlsEx(&icc);
    startupdlg = CreateDialog((HINSTANCE)win_gethinstance(), MAKEINTRESOURCE(IDD_STARTWIN), NULL, StartupDlgProg);
    if (!startupdlg) {
        return -1;
    }

    quiteventonclose = TRUE;
    configure(TRUE);
    setup_messages_mode(TRUE);
    return 0;
}

int startwin_close(void)
{
    if (!startupdlg) return 1;

    quiteventonclose = FALSE;
    DestroyWindow(startupdlg);
    startupdlg = NULL;

    return 0;
}

int startwin_puts(const char *buf)
{
    const char *p = NULL, *q = NULL;
    char workbuf[1024];
    static int newline = 0;
    int curlen, linesbefore, linesafter;
    HWND edctl;
    int vis;

    if (!startupdlg) return 1;

    edctl = GetDlgItem(tabs[TAB_MESSAGES].hwnd, IDC_MESSAGES);
    if (!edctl) return -1;

    vis = ((int)SendMessage(GetDlgItem(startupdlg, IDC_STARTWIN_TABCTL), TCM_GETCURSEL,0,0) == TAB_MESSAGES);

    if (vis) SendMessage(edctl, WM_SETREDRAW, FALSE,0);
    curlen = SendMessage(edctl, WM_GETTEXTLENGTH, 0,0);
    SendMessage(edctl, EM_SETSEL, (WPARAM)curlen, (LPARAM)curlen);
    linesbefore = SendMessage(edctl, EM_GETLINECOUNT, 0,0);
    p = buf;
    while (*p) {
        if (newline) {
            SendMessage(edctl, EM_REPLACESEL, 0, (LPARAM)"\r\n");
            newline = 0;
        }
        q = p;
        while (*q && *q != '\n') q++;
        memcpy(workbuf, p, q-p);
        if (*q == '\n') {
            if (!q[1]) {
                newline = 1;
                workbuf[q-p] = 0;
            } else {
                workbuf[q-p] = '\r';
                workbuf[q-p+1] = '\n';
                workbuf[q-p+2] = 0;
            }
            p = q+1;
        } else {
            workbuf[q-p] = 0;
            p = q;
        }
        SendMessage(edctl, EM_REPLACESEL, 0, (LPARAM)workbuf);
    }
    linesafter = SendMessage(edctl, EM_GETLINECOUNT, 0,0);
    SendMessage(edctl, EM_LINESCROLL, 0, linesafter-linesbefore);
    if (vis) SendMessage(edctl, WM_SETREDRAW, TRUE,0);
    return 0;
}

int startwin_settitle(const char *str)
{
    if (startupdlg) {
        SetWindowText(startupdlg, str);
    }
    return 0;
}

int startwin_idle(void *msg)
{
    if (!msg) return 0;
    if (!startupdlg || !IsWindow(startupdlg)) return 0;
    if (IsDialogMessage(startupdlg, (MSG*)msg)) return 1;
    return 0;
}

int startwin_run(void)
{
    MSG msg;

    if (!startupdlg) return 1;

    configure(FALSE);
    setup_config_mode();

    while (retval < 0) {
        switch (GetMessage(&msg, NULL, 0,0)) {
            case 0: retval = STARTWIN_CANCEL; break;    //WM_QUIT
            case -1: return -1;     // error
            default:
                 if (IsWindow(startupdlg) && IsDialogMessage(startupdlg, &msg)) break;
                 TranslateMessage(&msg);
                 DispatchMessage(&msg);
                 break;
        }
    }

    setup_messages_mode(startwin_settings.features.network &&
        (startwin_settings.network.numplayers > 1));

    return retval;
}

