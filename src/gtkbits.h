#ifndef __gtkbits_h__
#define __gtkbits_h__

extern void gtkbuild_init(int *argc, char ***argv);
extern void gtkbuild_exit(void);
extern int gtkbuild_main(void);
extern int gtkbuild_msgbox(const char *name, const char *msg);
extern int gtkbuild_ynbox(const char *name, const char *msg);

#endif
