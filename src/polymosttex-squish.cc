/**
 * libsquish bridging interface
 */

#ifdef USE_OPENGL

#include "squish.h"

extern "C" {
#include "glbuild.h"
#include "polymost_priv.h"
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
	
	if (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
		flags |= squish::kDxt1;
	} else if (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
		flags |= squish::kDxt5;
	}
	
	return flags;
}

extern "C" int squish_GetStorageRequirements(int width, int height, int format)
{
	return squish::GetStorageRequirements(width, height, getflags(format));
}

extern "C" int squish_CompressImage(coltype * rgba, int width, int height, unsigned char * output, int format)
{
	squish::CompressImage( (squish::u8 const *) rgba, width, height, output, getflags(format));
	return 0;
}

#endif
