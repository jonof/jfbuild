#ifndef __osxbits_h__
#define __osxbits_h__

char *osx_gethomedir(void);
char *osx_getappdir(void);
char *osx_getsupportdir(int global);

int osx_msgbox(const char *name, const char *msg);
int osx_ynbox(const char *name, const char *msg);

#endif
