/**
 * libsquish/rg_etc1 bridging interface
 */

#include "build.h"

#if USE_POLYMOST && USE_OPENGL

#include "polymosttexcompress.h"

#include "squish.h"
#include "rg_etc1.h"

extern "C" {
#include "glbuild.h"
extern int gltexcomprquality;
}

static int getsquishflags(int format)
{
	int flags;

	switch (gltexcomprquality) {
		case 2: flags = squish::kColourIterativeClusterFit;	// slower
			break;
		case 1: flags = squish::kColourClusterFit;		// slow, but also quite good looking
			break;
		default: flags = squish::kColourRangeFit;		// fast, but worse quality
			break;
	}

	flags |= squish::kSourceBGRA;
	if (format == PTCOMPRESS_DXT1) {
		flags |= squish::kDxt1;
	} else if (format == PTCOMPRESS_DXT5) {
		flags |= squish::kDxt5;
	}

	return flags;
}

static void compressetc1(uint8_t *bgra, int width, int height, uint8_t *out)
{
	rg_etc1::etc1_pack_params params;
	uint8_t block[4][4][4];
	int x, y, s, t, xyoff, stride;

	static int initonce = 0;
	if (!initonce) {
		rg_etc1::pack_etc1_block_init();
		initonce = 1;
	}

	switch (gltexcomprquality) {
		case 2: params.m_quality = rg_etc1::cHighQuality; break;
		case 1: params.m_quality = rg_etc1::cMediumQuality; break;
		default: params.m_quality = rg_etc1::cLowQuality; break;
	}

	stride = width * 4;
	for (y = 0; y < height; y += 4) {
		for (x = 0; x < width; x += 4) {
			xyoff = y * stride + x * 4;

			// Copy the block of pixels to encode, byte swizzling to RGBA order.
			for (t = 0; t < min(4, height - y); t++) {
				for (s = 0; s < min(4, width - x); s++) {
					block[t][s][0] = bgra[xyoff + t * stride + s * 4 + 2];
					block[t][s][1] = bgra[xyoff + t * stride + s * 4 + 1];
					block[t][s][2] = bgra[xyoff + t * stride + s * 4 + 0];
					block[t][s][3] = 255;
				}
				// Repeat the final pixel to pad to 4.
				for (; s < 4; s++) {
					memcpy(&block[t][s][0], &block[t][s-1][0], 4);
				}
			}
			// Repeat the final row to pad to 4.
			for (; t < 4; t++) {
				memcpy(&block[t][0][0], &block[t-1][0][0], 4 * 4);
			}

			rg_etc1::pack_etc1_block(out, (const uint32_t *)block, params);

			out += 8;
		}
	}
}

extern "C" int ptcompress_getstorage(int width, int height, int format)
{
	switch (format) {
		case PTCOMPRESS_DXT1:
		case PTCOMPRESS_DXT5:
			return squish::GetStorageRequirements(width, height, getsquishflags(format));
		case PTCOMPRESS_ETC1:
			return 8 * ((width + 3) / 4) * ((height + 3) / 4);
	}
	return 0;
}

extern "C" int ptcompress_compress(void * bgra, int width, int height, unsigned char * output, int format)
{
	switch (format) {
		case PTCOMPRESS_DXT1:
		case PTCOMPRESS_DXT5:
			squish::CompressImage( (squish::u8 const *) bgra, width, height, output, getsquishflags(format));
			return 0;
		case PTCOMPRESS_ETC1:
			compressetc1((uint8_t *)bgra, width, height, output);
			return 0;
	}
	return -1;
}

#endif	//USE_OPENGL
