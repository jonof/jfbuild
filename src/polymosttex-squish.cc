/**
 * libsquish bridging interface
 */

#include "build.h"

#if USE_POLYMOST && USE_OPENGL

#include "squish.h"

extern "C" {
#include "glbuild.h"
extern int gltexcomprquality;
}

static int getflags(int format)
{
	int flags;

	switch (gltexcomprquality) {
		case 1: flags = squish::kColourClusterFit;		// slow, but also quite good looking
			break;
		case 2: flags = squish::kColourIterativeClusterFit;	// slower
			break;
		default: flags = squish::kColourRangeFit;		// fast, but worse quality
			break;
	}

	if (format == 1) {
		flags |= squish::kDxt1;
	} else if (format == 5) {
		flags |= squish::kDxt5;
	}

	return flags;
}

extern "C" int squish_GetStorageRequirements(int width, int height, int format)
{
	return squish::GetStorageRequirements(width, height, getflags(format));
}

extern "C" int squish_CompressImage(void * rgba, int width, int height, unsigned char * output, int format)
{
	squish::CompressImage( (squish::u8 const *) rgba, width, height, output, getflags(format));
	return 0;
}

#endif	//USE_OPENGL
