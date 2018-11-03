// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been added to Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#if defined(POLYMOST) && defined(USE_OPENGL)

// The following files are automatically generated from *.glsl_[fv]s
// sources by way of the included bin2c tool. Consult the Makefile
// for more details, but here is an example of how to use it:
//   make bin2c
//   ./bin2c -text src/namehere.glsl_fs default_namehere_glsl_fs \
//      > src/namehere.glsl_fs_c

#include "polymost.glsl_vs_c"
#include "polymost.glsl_fs_c"
#include "polymostart.glsl_fs_c"
#include "polymostaux.glsl_vs_c"
#include "polymostaux.glsl_fs_c"

#endif
