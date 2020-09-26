#ifndef __gtkbits_h__
#define __gtkbits_h__

extern void wmgtk_init(int *argc, char ***argv);
extern void wmgtk_exit(void);
extern int wmgtk_idle(void *);
extern void wmgtk_setapptitle(const char *name);
extern int wmgtk_msgbox(const char *name, const char *msg);
extern int wmgtk_ynbox(const char *name, const char *msg);
extern int wmgtk_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice);

#endif
