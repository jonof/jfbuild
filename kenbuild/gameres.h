// resource ids
#define IDD_STARTWIN                1000
#define IDC_STARTWIN_TABCTL         1001
#define IDC_STARTWIN_BITMAP         1002
#define IDC_ALWAYSSHOW              1003

#define IDD_PAGE_CONFIG             1100
#define IDC_FULLSCREEN              1101
#define IDC_VMODE3D                 1102
#define IDC_SINGLEPLAYER            1103
#define IDC_JOINMULTIPLAYER         1104
#define IDC_HOSTMULTIPLAYER         1105
#define IDC_HOSTFIELD               1106
#define IDC_NUMPLAYERS              1107
#define IDC_NUMPLAYERSUD            1108

#define IDD_PAGE_MESSAGES           1200
#define IDC_MESSAGES                1201

#define IDI_ICON       100
#define IDB_BMP        200

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#ifdef RC_INVOKED
#ifndef __DATE__
#define __DATE__ "0000-00-00"
#endif
#ifndef __TIME__
#define __TIME__ "00:00:00"
#endif
#endif
