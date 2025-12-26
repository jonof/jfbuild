// Windows interface layer
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __build_interface_layer__
#define __build_interface_layer__ WIN

#ifdef __cplusplus
extern "C" {
#endif

intptr_t win_gethwnd(void);
intptr_t win_gethinstance(void);

void win_setmaxrefreshfreq(unsigned frequency);
unsigned win_getmaxrefreshfreq(void);

#ifdef __cplusplus
}
#endif

#include "baselayer.h"

#else
#if (__build_interface_layer__ != WIN)
#error "Already using the " __build_interface_layer__ ". Can't now use Windows."
#endif
#endif // __build_interface_layer__

