// Evil and Nasty Configuration File Reader for KenBuild
// by Jonathon Fowler

#include "compat.h"
#include "build.h"
#include "osd.h"

#ifdef RENDERTYPEWIN
#include "winlayer.h"
#endif

static int readconfig(BFILE *fp, const char *key, char *value, unsigned len)
{
	char buf[1000], *k, *v, *eq;
	int x=0;

	if (len < 1) return 0;

	Brewind(fp);

	while (1) {
		if (!Bfgets(buf, 1000, fp)) return 0;

		if (buf[0] == ';') continue;

		eq = Bstrchr(buf, '=');
		if (!eq) continue;

		k = buf;
		v = eq+1;

		while (*k == ' ' || *k == '\t') k++;
		*(eq--) = 0;
		while ((*eq == ' ' || *eq == '\t') && eq>=k) *(eq--) = 0;

		if (Bstrcasecmp(k, key)) continue;
		
		while (*v == ' ' || *k == '\t') v++;
		eq = v + Bstrlen(v)-1;

		while ((*eq == ' ' || *eq == '\t' || *eq == '\r' || *eq == '\n') && eq>=v) *(eq--) = 0;

		value[--len] = 0;
		do value[x] = v[x]; while (v[x++] != 0 && len-- > 0);

		return x-1;
	}
}

extern short brightness;
extern long fullscreen, bitsperpixel;
extern char option[8];
extern char keys[19];
extern double msens;

/*
 * SETUP.DAT
 * 0      = video mode (0:chained 1:vesa 2:screen buffered 3/4/5:tseng/paradise/s3 6:red-blue)
 * 1      = sound (0:none)
 * 2      = music (0:none)
 * 3      = input (0:keyboard 1:+mouse)
 * 4      = multiplayer (0:single 1-4:com 5-11:ipx)
 * 5&0xf0 = com speed
 * 5&0x0f = com irq
 * 6&0xf0 = chained y-res
 * 6&0x0f = chained x-res or vesa mode
 * 7&0xf0 = sound samplerate
 * 7&0x01 = sound quality
 * 7&0x02 = 8/16 bit
 * 7&0x04 = mono/stereo
 *
 * bytes 8 to 26 are key settings:
 * 0      = Forward (0xc8)
 * 1      = Backward (0xd0)
 * 2      = Turn left (0xcb)
 * 3      = Turn right (0xcd)
 * 4      = Run (0x2a)
 * 5      = Strafe (0x9d)
 * 6      = Fire (0x1d)
 * 7      = Use (0x39)
 * 8      = Stand high (0x1e)
 * 9      = Stand low (0x2c)
 * 10     = Look up (0xd1)
 * 11     = Look down (0xc9)
 * 12     = Strafe left (0x33)
 * 13     = Strafe right (0x34)
 * 14     = 2D/3D switch (0x9c)
 * 15     = View cycle (0x1c)
 * 16     = 2D Zoom in (0xd)
 * 17     = 2D Zoom out (0xc)
 * 18     = Chat (0xf)
 */

int loadsetup(const char *fn)
{
	BFILE *fp;
#define VL 32
	char val[VL];
	int i;

	if ((fp = Bfopen(fn, "rt")) == NULL) return -1;

	if (readconfig(fp, "fullscreen", val, VL) > 0) {
		if (Batoi(val) != 0) fullscreen = 1;
		else fullscreen = 0;
	}

	if (readconfig(fp, "resolution", val, VL) > 0) {
		option[6] = Batoi(val) & 0x0f;
		option[8] = option[6];
	}

	if (readconfig(fp, "2dresolution", val, VL) > 0) {
		option[8] = Batoi(val) & 0x0f;
	}

	if (readconfig(fp, "samplerate", val, VL) > 0) {
		option[7] = (Batoi(val) & 0x0f) << 4;
	}

	if (readconfig(fp, "music", val, VL) > 0) {
		if (Batoi(val) != 0) option[2] = 1;
		else option[2] = 0;
	}

	if (readconfig(fp, "mouse", val, VL) > 0) {
		if (Batoi(val) != 0) option[3] = 1;
		else option[3] = 0;
	}

	if (readconfig(fp, "bpp", val, VL) > 0) {
		bitsperpixel = Batoi(val);
	}
	
	if (readconfig(fp, "renderer", val, VL) > 0) {
		i = Batoi(val);
		setrendermode(i);
	}

	if (readconfig(fp, "brightness", val, VL) > 0) {
		brightness = min(max(Batoi(val),0),15);
	}

#ifdef RENDERTYPEWIN
	if (readconfig(fp, "glusecds", val, VL) > 0) {
		glusecds = Batoi(val);
	}
#endif

	option[0] = 1;	// vesa all the way...
	option[1] = 1;	// sound all the way...
	option[4] = 0;	// no multiplayer
	option[5] = 0;

	if (readconfig(fp, "keyforward", val, VL) > 0) keys[0] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keybackward", val, VL) > 0) keys[1] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keyturnleft", val, VL) > 0) keys[2] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keyturnright", val, VL) > 0) keys[3] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keyrun", val, VL) > 0) keys[4] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keystrafe", val, VL) > 0) keys[5] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keyfire", val, VL) > 0) keys[6] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keyuse", val, VL) > 0) keys[7] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keystandhigh", val, VL) > 0) keys[8] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keystandlow", val, VL) > 0) keys[9] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keylookup", val, VL) > 0) keys[10] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keylookdown", val, VL) > 0) keys[11] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keystrafeleft", val, VL) > 0) keys[12] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keystraferight", val, VL) > 0) keys[13] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "key2dmode", val, VL) > 0) keys[14] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keyviewcycle", val, VL) > 0) keys[15] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "key2dzoomin", val, VL) > 0) keys[16] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "key2dzoomout", val, VL) > 0) keys[17] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keychat", val, VL) > 0) keys[18] = Bstrtol(val, NULL, 16);
	if (readconfig(fp, "keyconsole", val, VL) > 0) OSD_CaptureKey(Bstrtol(val, NULL, 16));

	if (readconfig(fp, "mousesensitivity", val, VL) > 0) msens = Bstrtod(val, NULL);

	Bfclose(fp);

	return 0;
}

