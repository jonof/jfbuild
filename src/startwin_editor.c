#ifndef RENDERTYPEWIN
#error Only for Windows
#endif

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0600

#include "compat.h"
#include "winlayer.h"
#include "build.h"
#include "editor.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>

#include "buildres.h"

#define TAB_CONFIG 0
#define TAB_MESSAGES 1


static HWND startupdlg = NULL;
static HWND pages[2] = { NULL, NULL};
static int mode = TAB_CONFIG;
static struct startwin_settings *settings;
static BOOL quiteventonclose = FALSE;
static int retval = -1;

static void populate_video_modes(BOOL firstTime)
{
    int i, j, mode2d = -1, mode3d = -1;
    int xdim = 0, ydim = 0, bpp = 0, fullscreen = 0;
    int xdim2d = 0, ydim2d = 0;
    char modestr[64];
    int cd[] = { 32, 24, 16, 15, 8, 0 };
    HWND hwnd, hwnd2d;

    hwnd = GetDlgItem(pages[TAB_CONFIG], IDC_VMODE3D);
    hwnd2d = GetDlgItem(pages[TAB_CONFIG], IDC_VMODE2D);
    if (firstTime) {
        getvalidmodes();
        xdim = settings->xdim3d;
        ydim = settings->ydim3d;
        bpp  = settings->bpp3d;
        fullscreen = settings->fullscreen;

        xdim2d = settings->xdim2d;
        ydim2d = settings->ydim2d;
    } else {
        fullscreen = IsDlgButtonChecked(pages[TAB_CONFIG], IDC_FULLSCREEN) == BST_CHECKED;
        i = ComboBox_GetCurSel(hwnd);
        if (i != CB_ERR) i = ComboBox_GetItemData(hwnd, i);
        if (i != CB_ERR) {
            xdim = validmode[i].xdim;
            ydim = validmode[i].ydim;
            bpp  = validmode[i].bpp;
        }

        i = ComboBox_GetCurSel(hwnd2d);
        if (i != CB_ERR) i = ComboBox_GetItemData(hwnd2d, i);
        if (i != CB_ERR) {
            xdim2d = validmode[i].xdim;
            ydim2d = validmode[i].ydim;
        }
    }

    // Find an ideal match.
    mode3d = checkvideomode(&xdim, &ydim, bpp, fullscreen, 1);
    mode2d = checkvideomode(&xdim2d, &ydim2d, 8, fullscreen, 1);
    if (mode2d < 0) {
        mode2d = 0;
    }
    if (mode3d < 0) {
        for (i=0; cd[i]; ) { if (cd[i] >= bpp) i++; else break; }
        for ( ; cd[i]; i++) {
            mode3d = checkvideomode(&xdim, &ydim, cd[i], fullscreen, 1);
            if (mode3d < 0) continue;
            break;
        }
    }

    // Repopulate the lists.
    ComboBox_ResetContent(hwnd);
    ComboBox_ResetContent(hwnd2d);
    for (i=0; i<validmodecnt; i++) {
        if (validmode[i].fs != fullscreen) continue;

        Bsprintf(modestr, "%d x %d %d-bpp", validmode[i].xdim, validmode[i].ydim, validmode[i].bpp);
        j = ComboBox_AddString(hwnd, modestr);
        ComboBox_SetItemData(hwnd, j, i);
        if (i == mode3d) {
            ComboBox_SetCurSel(hwnd, j);
        }

        if (validmode[i].bpp == 8 && validmode[i].xdim >= 640 && validmode[i].ydim >= 480) {
            Bsprintf(modestr, "%d x %d", validmode[i].xdim, validmode[i].ydim);
            j = ComboBox_AddString(hwnd2d, modestr);
            ComboBox_SetItemData(hwnd2d, j, i);
            if (i == mode2d) {
                ComboBox_SetCurSel(hwnd2d, j);
            }
        }
    }
}

static void set_settings(struct startwin_settings *thesettings)
{
    settings = thesettings;
}

static void set_page(int n)
{
    HWND tab = GetDlgItem(startupdlg, IDC_STARTWIN_TABCTL);
    int cur = (int)SendMessage(tab, TCM_GETCURSEL,0,0);

    ShowWindow(pages[cur], SW_HIDE);
    SendMessage(tab, TCM_SETCURSEL, n, 0);
    ShowWindow(pages[n], SW_SHOW);
    mode = n;

    SendMessage(startupdlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(startupdlg, IDC_STARTWIN_TABCTL), TRUE);
}

static void setup_config_mode(void)
{
    set_page(TAB_CONFIG);

    CheckDlgButton(startupdlg, IDC_ALWAYSSHOW, (settings->forcesetup ? BST_CHECKED : BST_UNCHECKED));
    EnableWindow(GetDlgItem(startupdlg, IDC_ALWAYSSHOW), TRUE);

    CheckDlgButton(pages[TAB_CONFIG], IDC_FULLSCREEN, (settings->fullscreen ? BST_CHECKED : BST_UNCHECKED));
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_FULLSCREEN), TRUE);

    populate_video_modes(TRUE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_VMODE3D), TRUE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_VMODE2D), TRUE);

    EnableWindow(GetDlgItem(startupdlg, IDCANCEL), TRUE);
    EnableWindow(GetDlgItem(startupdlg, IDOK), TRUE);
}

static void setup_messages_mode(BOOL allowcancel)
{
    set_page(TAB_MESSAGES);

    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_FULLSCREEN), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_VMODE3D), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_VMODE2D), FALSE);

    EnableWindow(GetDlgItem(startupdlg, IDC_ALWAYSSHOW), FALSE);

    EnableWindow(GetDlgItem(startupdlg, IDCANCEL), allowcancel);
    EnableWindow(GetDlgItem(startupdlg, IDOK), FALSE);
}

static void fullscreen_clicked(void)
{
    populate_video_modes(FALSE);
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

    hwnd = GetDlgItem(pages[TAB_CONFIG], IDC_VMODE3D);
    i = ComboBox_GetCurSel(hwnd);
    if (i != CB_ERR) i = ComboBox_GetItemData(hwnd, i);
    if (i != CB_ERR) {
        settings->xdim3d = validmode[i].xdim;
        settings->ydim3d = validmode[i].ydim;
        settings->bpp3d  = validmode[i].bpp;
        settings->fullscreen = validmode[i].fs;
    }

    hwnd = GetDlgItem(pages[TAB_CONFIG], IDC_VMODE2D);
    i = ComboBox_GetCurSel(hwnd);
    if (i != CB_ERR) i = ComboBox_GetItemData(hwnd, i);
    if (i != CB_ERR) {
        settings->xdim2d = validmode[i].xdim;
        settings->ydim2d = validmode[i].ydim;
    }

    settings->forcesetup = IsDlgButtonChecked(startupdlg, IDC_ALWAYSSHOW) == BST_CHECKED;

    retval = STARTWIN_RUN;
}

static INT_PTR CALLBACK ConfigPageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG: {
            EnableThemeDialogTexture(hwndDlg, ETDT_ENABLETAB);
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FULLSCREEN:
                    fullscreen_clicked();
                    return TRUE;
                default: break;
            }
            break;
        default: break;
    }
    return FALSE;
}

static INT_PTR CALLBACK MessagesPageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_CTLCOLORSTATIC:
            if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_MESSAGES))
                return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
            break;
    }
    return FALSE;
}

static INT_PTR CALLBACK startup_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG: {
            HWND hwnd;
            RECT r, rdlg, chrome, rtab, rcancel, rstart;
            int xoffset = 0, yoffset = 0;

            {
                TCITEM tab;

                hwnd = GetDlgItem(hwndDlg, IDC_STARTWIN_TABCTL);

                // Add tabs to the tab control
                ZeroMemory(&tab, sizeof(tab));
                tab.mask = TCIF_TEXT;
                tab.pszText = TEXT("Configuration");
                TabCtrl_InsertItem(hwnd, 0, &tab);
                tab.pszText = TEXT("Messages");
                TabCtrl_InsertItem(hwnd, 1, &tab);

                // Work out the position and size of the area inside the tab control for the pages.
                ZeroMemory(&r, sizeof(r));
                GetClientRect(hwnd, &r);
                TabCtrl_AdjustRect(hwnd, FALSE, &r);

                // Create the pages and position them in the tab control, but hide them.
                pages[TAB_CONFIG] = CreateDialog((HINSTANCE)win_gethinstance(),
                    MAKEINTRESOURCE(IDD_PAGE_CONFIG), hwnd, ConfigPageProc);
                SetWindowPos(pages[TAB_CONFIG], NULL, r.left,r.top,r.right,r.bottom, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOSIZE);

                pages[TAB_MESSAGES] = CreateDialog((HINSTANCE)win_gethinstance(),
                    MAKEINTRESOURCE(IDD_PAGE_MESSAGES), hwnd, MessagesPageProc);
                SetWindowPos(pages[TAB_MESSAGES], NULL, r.left,r.top,r.right,r.bottom, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOSIZE);

                SendMessage(hwndDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwndDlg, IDOK), TRUE);
            }
            return TRUE;
        }

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            int cur;
            if (nmhdr->idFrom != IDC_STARTWIN_TABCTL) break;
            cur = (int)SendMessage(nmhdr->hwndFrom, TCM_GETCURSEL,0,0);
            switch (nmhdr->code) {
                case TCN_SELCHANGING: {
                    if (cur < 0 || !pages[cur]) break;
                    ShowWindow(pages[cur],SW_HIDE);
                    return TRUE;
                }
                case TCN_SELCHANGE: {
                    if (cur < 0 || !pages[cur]) break;
                    ShowWindow(pages[cur],SW_SHOW);
                    return TRUE;
                }
            }
            break;
        }

        case WM_CLOSE:
            cancelbutton_clicked();
            return TRUE;

        case WM_DESTROY:
            if (pages[TAB_CONFIG]) {
                DestroyWindow(pages[TAB_CONFIG]);
                pages[TAB_CONFIG] = NULL;
            }

            if (pages[TAB_MESSAGES]) {
                DestroyWindow(pages[TAB_MESSAGES]);
                pages[TAB_MESSAGES] = NULL;
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
    icc.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);
    startupdlg = CreateDialog((HINSTANCE)win_gethinstance(), MAKEINTRESOURCE(IDD_STARTWIN), NULL, startup_dlgproc);
    if (!startupdlg) {
        return -1;
    }

    quiteventonclose = TRUE;
    setup_messages_mode(FALSE);
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

    edctl = GetDlgItem(pages[TAB_MESSAGES], IDC_MESSAGES);
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

int startwin_idle(void *v)
{
    if (!startupdlg || !IsWindow(startupdlg)) return 0;
    if (IsDialogMessage(startupdlg, (MSG*)v)) return 1;
    return 0;
}

int startwin_run(struct startwin_settings *settings)
{
    MSG msg;

    if (!startupdlg) return 1;

    set_settings(settings);
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

    setup_messages_mode(FALSE);
    set_settings(NULL);

    return retval;
}

