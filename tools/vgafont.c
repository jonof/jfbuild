// VGA ROM Font Grabber
// Copyright (c) 1997,2022 Jonathon Fowler
// This is a 16-bit DOS program, compact/large memory model.

// With Borland C++ 4.5: bcc -1 -mc vgafont.c
// With OpenWatcom 1.9: wcl -1 -mc vgafont.c

// Ref http://www.delorie.com/djgpp/doc/rbinter/id/63/1.html


#include <dos.h>
#include <string.h>
#include <stdio.h>

struct spec {
	const char *descr;
	const char *filename;
	short bx[2];
	short bytes[2];
	int checkdupeidx;
} specs[] = {
	{
		"8-by-8",
		"8x8.dat",
		{ 0x0300, 0x0400 },
		{ 8*128, 8*128 },
		-1
	},
	{
		"8-by-14",
		"8x14.dat",
		{ 0x0200, 0 },
		{ 14*256, 0 },

		// Could return 8x16 font data if the video card BIOS followed
		// VBE3.0 advice to remove it for creating space. Such is true
		// of the Matrox Millennium. This index indicates a check be
		// done to identify duplication.
		2
	},
	{
		"8-by-16",
		"8x16.dat",
		{ 0x0600, 0 },
		{ 16*256, 0 },
		-1
	},
};


#ifdef __WATCOMC__
typedef union REGPACK REGS;
#define REG(p,r) p.w.r
#else
typedef struct REGPACK REGS;
#define REG(p,r) p.r_##r
#endif

void dumpfont(const struct spec *spec)
{
	REGS r;
	FILE *fp;
	int bytes;
	char *data;

	printf("Getting %s... ", spec->descr);

	fp = fopen(spec->filename, "w+b");
	if (fp == NULL) {
		puts("Error");
		return;
	}

	REG(r, ax) = 0x1130;
	REG(r, bx) = spec->bx[0];
	intr(0x10, &r);

	data = MK_FP(REG(r, es), REG(r, bp));
	fwrite(data, 1, spec->bytes[0], fp);

	if (spec->checkdupeidx >= 0) {
		REG(r, ax) = 0x1130;
		REG(r, bx) = specs[spec->checkdupeidx].bx[0];
		intr(0x10, &r);

		if (memcmp(MK_FP(REG(r, es), REG(r, bp)), data, spec->bytes[0]) == 0) {
			printf("Likely a dupe of %s... ", specs[spec->checkdupeidx].descr);
		}
	}

	if (spec->bx[1]) {
		REG(r, ax) = 0x1130;
		REG(r, bx) = spec->bx[1];
		intr(0x10, &r);

		data = MK_FP(REG(r, es), REG(r, bp));
		fwrite(data, 1, spec->bytes[1], fp);
	}

	fclose(fp);

	puts(spec->filename);
}

void main(void)
{
	int font;
	int numfonts = sizeof(specs) / sizeof(specs[0]);

	puts("VGA ROM Font Grabber\n"
	     "Copyright (c) 1997,2022 Jonathon Fowler\n");

	for (font = 0; font < numfonts; font++) {
		dumpfont(&specs[font]);
	}
}
