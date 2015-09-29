#include "a.h"

#if defined _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#elif defined __linux || defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__ || defined __APPLE__
# include <sys/mman.h>
# include <unistd.h>
# include <stdio.h>
# include <errno.h>
#endif

void makeasmwriteable(void)
{
#ifndef ENGINE_USING_A_C
    extern int dep_begin, dep_end;

# if defined _WIN32
    DWORD oldprot;
    TCHAR msg[1024+64], msgbuf[1024];
    if (!VirtualProtect((LPVOID)&dep_begin, (SIZE_T)&dep_end - (SIZE_T)&dep_begin, PAGE_EXECUTE_READWRITE, &oldprot)) {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)msgbuf, 1024, NULL);
        wsprintf(msg, "Error making code writeable: %s", msgbuf);
        MessageBox(0, msg, "BUILD engine", MB_OK|MB_ICONSTOP);
        return;
    }

# elif defined __linux || defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__ || defined __APPLE__
    int pagesize;
    size_t dep_begin_page;
    pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize == -1) {
        fputs("Error getting system page size\n", stderr);
        return;
    }
    dep_begin_page = ((size_t)&dep_begin) & ~(pagesize-1);
    if (mprotect((void *)dep_begin_page, (size_t)&dep_end - dep_begin_page, PROT_READ|PROT_WRITE) < 0) {
        fprintf(stderr, "Error making code writeable (errno=%d)\n", errno);
        return;
    }

# else
#  error Dont know how to unprotect the self-modifying assembly on this platform!
# endif

#endif  //ENGINE_USING_A_C
}
