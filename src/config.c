// Evil and Nasty Configuration File Reader for KenBuild
// by Jonathon Fowler

#include "build.h"
#include "editor.h"
#include "osd.h"
#include "scriptfile.h"

#ifdef RENDERTYPEWIN
#include "winlayer.h"
#endif
#include "baselayer.h"

extern short brightness;
extern int fullscreen;
extern char option[8];
extern int keys[NUMBUILDKEYS];
extern int msens;

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

enum {
	type_bool = 0,	//int
	type_double = 1,
	type_int = 2,
	type_hex = 3,
	type_fixed16,	//int
};

#if USE_POLYMOST
static int tmprenderer = -1;
#endif
static int tmpfullscreen = -1, tmpdisplay = -1;
static int tmpbrightness = -1;
#ifdef RENDERTYPEWIN
static unsigned tmpmaxrefreshfreq = -1;
#endif

static struct {
	const char *name;
	int type;
	void *store;
	const char *doc;
} configspec[] = {
	{ "forcesetup", type_bool, &forcesetup,
		"; Always show configuration options on startup\n"
		";   0 - No\n"
		";   1 - Yes\n"
	},
	{ "fullscreen", type_bool, &tmpfullscreen,
		"; Video mode selection\n"
		";   0 - Windowed\n"
		";   1 - Fullscreen\n"
	},
	{ "display", type_int, &tmpdisplay,
		"; Video display number\n"
		";   0 - Primary display\n"
	},
	{ "xdim2d", type_int, &xdim2d,
		"; Video resolution\n"
	},
	{ "ydim2d", type_int, &ydim2d, NULL },
	{ "xdim3d", type_int, &xdimgame, NULL },
	{ "ydim3d", type_int, &ydimgame, NULL },
	{ "bpp",    type_int, &bppgame,
		"; 3D-mode colour depth\n"
	},
#if USE_POLYMOST
	{ "renderer", type_int, &tmprenderer,
		"; 3D-mode renderer type\n"
		";   0  - classic\n"
		";   2  - software Polymost\n"
		";   3  - OpenGL Polymost\n"
	},
#endif
	{ "brightness", type_int, &tmpbrightness,
		"; 3D mode brightness setting\n"
		";   0  - lowest\n"
		";   15 - highest\n"
	},
	{ "usegammabrightness", type_bool, &usegammabrightness,
		"; Brightness setting method\n"
		";   0 - palette\n"
#if USE_OPENGL
		";   1 - shader gamma\n"
#endif
		";   2 - system gamma\n"
	},
#if USE_POLYMOST && USE_OPENGL
	{ "glusetexcache", type_bool, &glusetexcache,
		"; OpenGL mode options\n"
	},
#endif
#ifdef RENDERTYPEWIN
	{ "maxrefreshfreq", type_int, &tmpmaxrefreshfreq,
		"; Maximum OpenGL mode refresh rate (Windows only, in Hertz)\n"
	},
#endif
	{ "mousesensitivity", type_fixed16, &msens,
		"; Mouse sensitivity\n"
	},
	{ "keyforward", type_hex, &keys[0],
		"; Key Settings\n"
		";  Here's a map of all the keyboard scan codes: NOTE: values are listed in hex!\n"
		"; +---------------------------------------------------------------------------------------------+\n"
		"; | 01   3B  3C  3D  3E   3F  40  41  42   43  44  57  58          46                           |\n"
		"; |ESC   F1  F2  F3  F4   F5  F6  F7  F8   F9 F10 F11 F12        SCROLL                         |\n"
		"; |                                                                                             |\n"
		"; |29  02  03  04  05  06  07  08  09  0A  0B  0C  0D   0E     D2  C7  C9      45  B5  37  4A   |\n"
		"; | ` '1' '2' '3' '4' '5' '6' '7' '8' '9' '0'  -   =  BACK    INS HOME PGUP  NUMLK KP/ KP* KP-  |\n"
		"; |                                                                                             |\n"
		"; | 0F  10  11  12  13  14  15  16  17  18  19  1A  1B  2B     D3  CF  D1      47  48  49  4E   |\n"
		"; |TAB  Q   W   E   R   T   Y   U   I   O   P   [   ]    \\    DEL END PGDN    KP7 KP8 KP9 KP+   |\n"
		"; |                                                                                             |\n"
		"; | 3A   1E  1F  20  21  22  23  24  25  26  27  28     1C                     4B  4C  4D       |\n"
		"; |CAPS  A   S   D   F   G   H   J   K   L   ;   '   ENTER                    KP4 KP5 KP6    9C |\n"
		"; |                                                                                      KPENTER|\n"
		"; |  2A    2C  2D  2E  2F  30  31  32  33  34  35    36            C8          4F  50  51       |\n"
		"; |LSHIFT  Z   X   C   V   B   N   M   ,   .   /   RSHIFT          UP         KP1 KP2 KP3       |\n"
		"; |                                                                                             |\n"
		"; | 1D     38              39                  B8     9D       CB  D0   CD      52    53        |\n"
		"; |LCTRL  LALT           SPACE                RALT   RCTRL   LEFT DOWN RIGHT    KP0    KP.      |\n"
		"; +---------------------------------------------------------------------------------------------+\n"
	},
	{ "keybackward", type_hex, &keys[1], NULL },
	{ "keyturnleft", type_hex, &keys[2], NULL },
	{ "keyturnright", type_hex, &keys[3], NULL },
	{ "keyrun", type_hex, &keys[4], NULL },
	{ "keystrafe", type_hex, &keys[5], NULL },
	{ "keyfire", type_hex, &keys[6], NULL },
	{ "keyuse", type_hex, &keys[7], NULL },
	{ "keystandhigh", type_hex, &keys[8], NULL },
	{ "keystandlow", type_hex, &keys[9], NULL },
	{ "keylookup", type_hex, &keys[10], NULL },
	{ "keylookdown", type_hex, &keys[11], NULL },
	{ "keystrafeleft", type_hex, &keys[12], NULL },
	{ "keystraferight", type_hex, &keys[13], NULL },
	{ "key2dmode", type_hex, &keys[14], NULL },
	{ "keyviewcycle", type_hex, &keys[15], NULL },
	{ "key2dzoomin", type_hex, &keys[16], NULL },
	{ "key2dzoomout", type_hex, &keys[17], NULL },
	{ "keychat", type_hex, &keys[18], NULL },
	{ "keyconsole", type_hex, &keys[19], NULL },
	{ NULL, 0, NULL, NULL }
};

int loadsetup(const char *fn)
{
	scriptfile *cfg;
	char *token;
	int item;

	cfg = scriptfile_fromfile(fn);
	if (!cfg) {
		return -1;
	}

	scriptfile_clearsymbols();

	option[0] = 1;	// vesa all the way...
	option[1] = 1;	// sound all the way...
	option[4] = 0;	// no multiplayer
	option[5] = 0;

	while (1) {
		token = scriptfile_gettoken(cfg);
		if (!token) break;	//EOF

		for (item = 0; configspec[item].name; item++) {
			if (!Bstrcasecmp(token, configspec[item].name)) {
				// Seek past any = symbol.
				token = scriptfile_peektoken(cfg);
				if (!Bstrcasecmp("=", token)) {
					scriptfile_gettoken(cfg);
				}

				switch (configspec[item].type) {
					case type_bool: {
						int value = 0;
						if (scriptfile_getnumber(cfg, &value)) break;
						*(int*)configspec[item].store = (value != 0);
						break;
					}
					case type_int: {
						int value = 0;
						if (scriptfile_getnumber(cfg, &value)) break;
						*(int*)configspec[item].store = value;
						break;
					}
					case type_hex: {
						int value = 0;
						if (scriptfile_gethex(cfg, &value)) break;
						*(int*)configspec[item].store = value;
						break;
					}
					case type_fixed16: {
						double value = 0.0;
						if (scriptfile_getdouble(cfg, &value)) break;
						*(int*)configspec[item].store = (int)(value*65536.0);
						break;
					}
					case type_double: {
						double value = 0.0;
						if (scriptfile_getdouble(cfg, &value)) break;
						*(double*)configspec[item].store = value;
						break;
					}
					default: {
						buildputs("loadsetup: unhandled value type\n");
						break;
					}
				}
				break;
			}
		}
		if (!configspec[item].name) {
			buildprintf("loadsetup: error on line %d\n", scriptfile_getlinum(cfg, cfg->ltextptr));
			continue;
		}
	}

#if USE_POLYMOST
	if (tmprenderer >= 0) {
		setrendermode(tmprenderer);
	}
#endif
#ifdef RENDERTYPEWIN
	win_setmaxrefreshfreq(tmpmaxrefreshfreq);
#endif
	if (tmpbrightness >= 0) {
		brightness = min(max(tmpbrightness,0),15);
	}
	fullscreen = 0;
	if (tmpfullscreen >= 0) {
		fullscreen = tmpfullscreen;
	}
	if (tmpdisplay >= 0) {
		fullscreen |= tmpdisplay<<8;
	}
	OSD_CaptureKey(keys[19]);

	scriptfile_close(cfg);
	scriptfile_clearsymbols();

	return 0;
}

int writesetup(const char *fn)
{
	BFILE *fp;
	int item;

	fp = Bfopen(fn,"wt");
	if (!fp) return -1;

	tmpbrightness = brightness;
	tmpfullscreen = !!(fullscreen&255);
	tmpdisplay = fullscreen>>8;
#if USE_POLYMOST
	tmprenderer = getrendermode();
#endif
#ifdef RENDERTYPEWIN
	tmpmaxrefreshfreq = win_getmaxrefreshfreq();
#endif

	for (item = 0; configspec[item].name; item++) {
		if (configspec[item].doc) {
			if (item > 0) fputs("\n", fp);
			fputs(configspec[item].doc, fp);
		}
		fputs(configspec[item].name, fp);
		fputs(" = ", fp);
		switch (configspec[item].type) {
			case type_bool: {
				fprintf(fp, "%d\n", (*(int*)configspec[item].store != 0));
				break;
			}
			case type_int: {
				fprintf(fp, "%d\n", *(int*)configspec[item].store);
				break;
			}
			case type_hex: {
				fprintf(fp, "%X\n", *(int*)configspec[item].store);
				break;
			}
			case type_fixed16: {
				fprintf(fp, "%g\n", (double)(*(int*)configspec[item].store) / 65536.0);
				break;
			}
			case type_double: {
				fprintf(fp, "%g\n", *(double*)configspec[item].store);
				break;
			}
			default: {
				fputs("?\n", fp);
				break;
			}
		}
	}

	Bfclose(fp);

	return 0;
}
