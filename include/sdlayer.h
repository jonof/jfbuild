// SDL interface layer
// for the Build Engine
// by Jonathon Fowler (jonof@edgenetwk.com)

#ifndef __build_interface_layer__
#define __build_interface_layer__ SDL

#include "baselayer.h"

struct sdlappicon {
	unsigned long width,height;
	unsigned long ncolours;
	struct { unsigned char r,g,b,f; } *colourmap;
	unsigned char *pixels;
	unsigned char *mask;
};

#else
#if (__build_interface_layer__ != SDL)
#error "Already using the " __build_interface_layer__ ". Can't now use SDL."
#endif
#endif // __build_interface_layer__

