#ifndef __osxbits_h__
#define __osxbits_h__

char *osx_gethomedir(void);
char *osx_getappdir(void);
char *osx_getsupportdir(int global);

char * wmosx_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice);

#endif
