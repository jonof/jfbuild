#ifndef __gtkbits_h__
#define __gtkbits_h__

extern void wmgtk_init(int *argc, char ***argv);
extern void wmgtk_exit(void);
extern int wmgtk_idle(void *);
extern int wmgtk_msgbox(const char *name, const char *msg);
extern int wmgtk_ynbox(const char *name, const char *msg);

#endif
