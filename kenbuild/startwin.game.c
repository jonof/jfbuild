#ifndef RENDERTYPEWIN
#error Only for Windows
#endif

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0600

#include "compat.h"
#include "winlayer.h"
#include "build.h"
#include "mmulti.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>

#ifndef EM_SHOWBALLOONTIP
typedef struct _tagEDITBALLOONTIP {
    DWORD cbStruct;
    LPCWSTR pszTitle;
    LPCWSTR pszText;
    INT ttiIcon;
} EDITBALLOONTIP,*PEDITBALLOONTIP;
#define EM_SHOWBALLOONTIP 0x1503
#define EM_HIDEBALLOONTIP 0x1504
#define TTI_NONE 0
#endif

#include "gameres.h"

#define TAB_CONFIG 0
#define TAB_MESSAGES 1

static struct {
    int fullscreen;
    int xdim3d, ydim3d, bpp3d;
    int forcesetup;

    int multiargc;
    char const * *multiargv;
    char *multiargstr;
} settings;

static HWND startupdlg = NULL;
static HWND pages[2] = { NULL, NULL};
static int done = -1, mode = TAB_CONFIG, quiteventonclose = 1;

static void populateVideoModes(BOOL firstTime)
{
    int i, j, mode3d, fullscreen;
    int xdim, ydim, bpp = 8;
    char buf[64];
    HWND hwnd;

    fullscreen = IsDlgButtonChecked(pages[TAB_CONFIG], IDC_FULLSCREEN) == BST_CHECKED;

    hwnd = GetDlgItem(pages[TAB_CONFIG], IDC_3DVMODE);
    if (firstTime) {
        xdim = settings.xdim3d;
        ydim = settings.ydim3d;
        bpp  = settings.bpp3d;
    } else {
        i = ComboBox_GetCurSel(hwnd);
        if (i != CB_ERR) i = ComboBox_GetItemData(hwnd, i);
        if (i != CB_ERR) {
            xdim = validmode[i].xdim;
            ydim = validmode[i].ydim;
            bpp  = validmode[i].bpp;
        }
    }
    mode3d = checkvideomode(&xdim, &ydim, bpp, fullscreen, 1);
    if (mode3d < 0) {
        int i, cd[] = { 32, 24, 16, 15, 8, 0 };
        for (i=0; cd[i]; ) { if (cd[i] >= bpp) i++; else break; }
        for ( ; cd[i]; i++) {
            mode3d = checkvideomode(&xdim, &ydim, cd[i], fullscreen, 1);
            if (mode3d < 0) continue;
            break;
        }
    }

    ComboBox_ResetContent(hwnd);
    for (i=0; i<validmodecnt; i++) {
        if (validmode[i].fs != fullscreen) continue;

        Bsprintf(buf, "%d x %d %d-bpp", validmode[i].xdim, validmode[i].ydim, validmode[i].bpp);
        j = ComboBox_AddString(hwnd, buf);
        ComboBox_SetItemData(hwnd, j, i);
        if (i == mode3d) {
            ComboBox_SetCurSel(hwnd, j);
        }
    }
}

static void setPage(int n)
{
    HWND tab = GetDlgItem(startupdlg, IDC_STARTWIN_TABCTL);
    int cur = (int)SendMessage(tab, TCM_GETCURSEL,0,0);

    ShowWindow(pages[cur], SW_HIDE);
    SendMessage(tab, TCM_SETCURSEL, n, 0);
    ShowWindow(pages[n], SW_SHOW);
    mode = n;

    SendMessage(startupdlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(startupdlg, IDC_STARTWIN_TABCTL), TRUE);
}

static void setupPlayerHostsField(void)
{
    if (IsDlgButtonChecked(pages[TAB_CONFIG], IDC_PEERTOPEER) == BST_UNCHECKED) {
        SetDlgItemText(pages[TAB_CONFIG], IDC_HOSTSLISTLABEL, TEXT("Host address:"));
    } else {
        SetDlgItemText(pages[TAB_CONFIG], IDC_HOSTSLISTLABEL, TEXT("Player addresses:"));
    }
}

static void setupRunMode(void)
{
    CheckDlgButton(startupdlg, IDC_ALWAYSSHOW, (settings.forcesetup ? BST_CHECKED : BST_UNCHECKED));
    EnableWindow(GetDlgItem(startupdlg, IDC_ALWAYSSHOW), TRUE);

    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_3DVMODE), TRUE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_FULLSCREEN), TRUE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_SINGLEPLAYER), TRUE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_MULTIPLAYER), TRUE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_NUMPLAYERS), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_NUMPLAYERSUD), TRUE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_PEERTOPEER), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_HOSTSLIST), FALSE);

    populateVideoModes(TRUE);
    CheckDlgButton(pages[TAB_CONFIG], IDC_FULLSCREEN, (settings.fullscreen ? BST_CHECKED : BST_UNCHECKED));

    CheckRadioButton(pages[TAB_CONFIG], IDC_SINGLEPLAYER, IDC_MULTIPLAYER, IDC_SINGLEPLAYER);
    SetDlgItemInt(pages[TAB_CONFIG], IDC_NUMPLAYERS, 2, TRUE);
    SendDlgItemMessage(pages[TAB_CONFIG], IDC_NUMPLAYERSUD, UDM_SETPOS, 0, 2);
    SendDlgItemMessage(pages[TAB_CONFIG], IDC_NUMPLAYERSUD, UDM_SETRANGE, 0, MAKELPARAM(MAXPLAYERS, 2));
    setupPlayerHostsField();

    EnableWindow(GetDlgItem(startupdlg, IDCANCEL), TRUE);
    EnableWindow(GetDlgItem(startupdlg, IDOK), TRUE);

    setPage(TAB_CONFIG);
}

static void setupMessagesMode(int allowCancel)
{
    setPage(TAB_MESSAGES);

    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_3DVMODE), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_FULLSCREEN), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_SINGLEPLAYER), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_MULTIPLAYER), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_NUMPLAYERS), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_NUMPLAYERSUD), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_PEERTOPEER), FALSE);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_HOSTSLIST), FALSE);

    EnableWindow(GetDlgItem(startupdlg, IDC_ALWAYSSHOW), FALSE);

    EnableWindow(GetDlgItem(startupdlg, IDCANCEL), allowCancel);
    EnableWindow(GetDlgItem(startupdlg, IDOK), FALSE);
}

static void multiplayerModeClicked(int sender)
{
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_PEERTOPEER), (sender == IDC_MULTIPLAYER));
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_HOSTSLIST), (sender == IDC_MULTIPLAYER));

    CheckRadioButton(pages[TAB_CONFIG], IDC_SINGLEPLAYER, IDC_MULTIPLAYER, sender);
    setupPlayerHostsField();

    BOOL enableNumPlayers = (sender == IDC_MULTIPLAYER) && (IsDlgButtonChecked(pages[TAB_CONFIG], IDC_PEERTOPEER) == BST_UNCHECKED);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_NUMPLAYERS), enableNumPlayers);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_NUMPLAYERSUD), enableNumPlayers);
}

static void peerToPeerModeClicked(void)
{
    BOOL enableNumPlayers = (IsDlgButtonChecked(pages[TAB_CONFIG], IDC_PEERTOPEER) == BST_UNCHECKED);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_NUMPLAYERS), enableNumPlayers);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDC_NUMPLAYERSUD), enableNumPlayers);
    setupPlayerHostsField();
}

static INT_PTR CALLBACK ConfigPageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG: {
            EnableThemeDialogTexture(hwndDlg, ETDT_ENABLETAB);
            SendDlgItemMessage(hwndDlg, IDC_NUMPLAYERSUD, UDM_SETBUDDY, (WPARAM)GetDlgItem(hwndDlg, IDC_NUMPLAYERS), 0);
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FULLSCREEN:
                    populateVideoModes(FALSE);
                    return TRUE;
                case IDC_SINGLEPLAYER:
                case IDC_MULTIPLAYER:
                    multiplayerModeClicked(LOWORD(wParam));
                    return TRUE;
                case IDC_PEERTOPEER:
                    peerToPeerModeClicked();
                    return TRUE;
                case IDC_HOSTSLIST:
                    if (HIWORD(wParam) == EN_SETFOCUS) {
                        WCHAR text[100];
                        EDITBALLOONTIP tip = {
                            sizeof(EDITBALLOONTIP),
                            0,
                            text,
                            TTI_NONE
                        };
                        if (IsDlgButtonChecked(hwndDlg, IDC_PEERTOPEER) == BST_CHECKED) {
                            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, "Leave empty to host the game.", -1, text, 100);
                        } else {
                            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, "List each address in order. Use "
                                "* to indicate this machine's position.", -1, text, 100);
                        }
                        SendDlgItemMessage(hwndDlg, IDC_HOSTSLIST, EM_SHOWBALLOONTIP, 0, (LPARAM)&tip);
                        return TRUE;
                    } else if (HIWORD(wParam) == EN_SETFOCUS) {
                        SendDlgItemMessage(hwndDlg, IDC_HOSTSLIST, EM_HIDEBALLOONTIP, 0, 0);
                        return TRUE;
                    }
                    break;
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
            done = STARTWIN_CANCEL;
            quitevent = quitevent || quiteventonclose;
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
                    done = STARTWIN_CANCEL;
                    quitevent = quitevent || quiteventonclose;
                    return TRUE;
                case IDOK: {
                    int i;
                    HWND hwnd;

                    hwnd = GetDlgItem(pages[TAB_CONFIG], IDC_3DVMODE);
                    i = ComboBox_GetCurSel(hwnd);
                    if (i != CB_ERR) i = ComboBox_GetItemData(hwnd, i);
                    if (i != CB_ERR) {
                        settings.xdim3d = validmode[i].xdim;
                        settings.ydim3d = validmode[i].ydim;
                        settings.bpp3d  = validmode[i].bpp;
                    }

                    settings.fullscreen = IsDlgButtonChecked(pages[TAB_CONFIG], IDC_FULLSCREEN) == BST_CHECKED;

                    if (IsDlgButtonChecked(pages[TAB_CONFIG], IDC_MULTIPLAYER) == BST_CHECKED) {
                        int argstrlen, wcharlen, nparams;
                        char *argstrptr;
                        WCHAR *wcharstr;
                        const char *delimiters = " ,\n\t";

                        // Get the user's hosts string in wide chars.
                        hwnd = GetDlgItem(pages[TAB_CONFIG], IDC_HOSTSLIST);
                        wcharlen = GetWindowTextLengthW(hwnd) + 1;
                        wcharstr = (WCHAR *)malloc(wcharlen * sizeof(WCHAR));
                        GetWindowTextW(hwnd, wcharstr, wcharlen);

                        // Figure out the length of the string in UTF-8, plus the networking type parameter.
                        argstrlen = strlen("-n:xx") + 1;
                        argstrlen += WideCharToMultiByte(CP_UTF8, 0, wcharstr, -1, NULL, 0, NULL, NULL) + 1;

                        settings.multiargstr = (char *)malloc(argstrlen);
                        argstrptr = settings.multiargstr;

                        // Prepare the networking type parameter.
                        if (IsDlgButtonChecked(pages[TAB_CONFIG], IDC_PEERTOPEER) == BST_CHECKED) {
                            strcpy(argstrptr, "-n1");
                        } else {
                            int nplayers = (int)GetDlgItemInt(pages[TAB_CONFIG], IDC_NUMPLAYERS, NULL, TRUE);
                            nplayers = max(2, min(MAXPLAYERS, nplayers));
                            sprintf(argstrptr, "-n0:%d", nplayers);
                        }
                        argstrptr += strlen(argstrptr);
                        *argstrptr = ' ';
                        argstrptr++;

                        // Append the text the user gave, converted to UTF-8.
                        WideCharToMultiByte(CP_UTF8, 0, wcharstr, -1, argstrptr, argstrlen - strlen("-n:xx") - 1, NULL, NULL);

                        // Count how big the argv array needs to be by looking for token separators.
                        // This might be over-estimating, but that's fine.
                        nparams = 1;
                        for (argstrptr = settings.multiargstr; *argstrptr; argstrptr++) {
                            if (strchr(delimiters, *argstrptr)) nparams++;
                        }

                        // Now prepare the parameter list.
                        settings.multiargc = 0;
                        settings.multiargv = (const char * *)malloc(nparams * sizeof(char *));
                        for (argstrptr = strtok(settings.multiargstr, delimiters);
                                argstrptr; argstrptr = strtok(NULL, delimiters)) {
                            if (*argstrptr) {
                                settings.multiargv[settings.multiargc++] = argstrptr;
                            }
                        }

                        free(wcharstr);
 
                        done = STARTWIN_RUN_MULTI;
                    } else {
                        settings.multiargc = 0;
                        settings.multiargv = NULL;
                        settings.multiargstr = NULL;

                        done = STARTWIN_RUN;
                    }

                    settings.forcesetup = IsDlgButtonChecked(hwndDlg, IDC_ALWAYSSHOW) == BST_CHECKED;

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
    icc.dwICC = ICC_TAB_CLASSES | ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&icc);
    startupdlg = CreateDialog((HINSTANCE)win_gethinstance(), MAKEINTRESOURCE(IDD_STARTWIN), NULL, startup_dlgproc);
    if (startupdlg) {
        setupMessagesMode(FALSE);
        return 0;
    }
    return -1;
}

int startwin_close(void)
{
    if (!startupdlg) return 1;
    quiteventonclose = 0;
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
    if (!startupdlg) return 1;
    SetWindowText(startupdlg, str);
    return 0;
}

int startwin_idle(void *v)
{
    if (!startupdlg || !IsWindow(startupdlg)) return 0;
    if (IsDialogMessage(startupdlg, (MSG*)v)) return 1;
    return 0;
}

extern int xdimgame, ydimgame, bppgame, forcesetup;

int startwin_run(void)
{
    MSG msg;

    if (!startupdlg) return 1;

    done = -1;

    settings.fullscreen = fullscreen;
    settings.xdim3d = xdimgame;
    settings.ydim3d = ydimgame;
    settings.bpp3d = bppgame;
    settings.forcesetup = forcesetup;

    setupRunMode();

    while (done < 0) {
        switch (GetMessage(&msg, NULL, 0,0)) {
            case 0: done = STARTWIN_CANCEL; break;    //WM_QUIT
            case -1: return -1;     // error
            default:
                 if (IsWindow(startupdlg) && IsDialogMessage(startupdlg, &msg)) break;
                 TranslateMessage(&msg);
                 DispatchMessage(&msg);
                 break;
        }
    }

    setupMessagesMode(done == STARTWIN_RUN_MULTI);

    if (done) {
        fullscreen = settings.fullscreen;
        xdimgame = settings.xdim3d;
        ydimgame = settings.ydim3d;
        bppgame = settings.bpp3d;
        forcesetup = settings.forcesetup;

        if (done == STARTWIN_RUN_MULTI) {
            if (!initmultiplayersparms(settings.multiargc, settings.multiargv)) {
                done = STARTWIN_RUN;
            }
            free(settings.multiargv);
            free(settings.multiargstr);
        }
    }

    return done;
}

