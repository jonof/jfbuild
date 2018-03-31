#ifndef __RESOURCE_startgtk_H__
#define __RESOURCE_startgtk_H__

#include <gio/gio.h>

extern GResource *startgtk_get_resource (void);

extern void startgtk_register_resource (void);
extern void startgtk_unregister_resource (void);

#endif
