#include "kplib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct icon {
	unsigned long width,height;
	unsigned long ncolours;
	unsigned long *colourmap;
	unsigned char *pixels;
	unsigned char *mask;
};

int writeicon(FILE *fp, struct icon *ico)
{
	int i;
	
	fprintf(fp,
		"#include \"sdlayer.h\"\n"
		"\n"
		"static struct { unsigned char r,g,b,f; } sdlappicon_colourmap[] = {\n"
	);
	for (i=0;i<ico->ncolours;i++) {
		if ((i%4) == 0) fprintf(fp,"\t");
		else fprintf(fp," ");
		fprintf(fp, "{ %3d,%3d,%3d,%3d }",
				(ico->colourmap[i]>>16)&255, (ico->colourmap[i]>>8)&255,
				(ico->colourmap[i]&255), (ico->colourmap[i]>>24)&255
		);
		if (i<ico->ncolours-1) fprintf(fp, ",");
		if ((i%4) == 3) fprintf(fp,"\n");
	}
	if ((i%4) > 0) fprintf(fp, "\n");
	fprintf(fp,"};\n\nstatic unsigned char sdlappicon_pixels[] = {\n");
	for (i=0;i<ico->width*ico->height;i++) {
		if ((i%16) == 0) fprintf(fp,"\t");
		else fprintf(fp," ");
		fprintf(fp, "%3d", ico->pixels[i]);
		if (i<ico->width*ico->height-1) fprintf(fp, ",");
		if ((i%16) == 15) fprintf(fp,"\n");
	}
	if ((i%16) > 0) fprintf(fp, "\n");
	fprintf(fp,"};\n\n"
		"struct sdlappicon sdlappicon = {\n"
		"	%d,%d,	// width,height\n"
		"	%d,	// ncolours\n"
		"	(void*)sdlappicon_colourmap,\n"
		"	sdlappicon_pixels,\n"
		"	(void*)0\n"
		"};",
		ico->width, ico->height, ico->ncolours
	);

	return 0;
}

int findcolour(long colour, struct icon *ico)
{
	int i;

	for (i=0; i<ico->ncolours; i++) {
		if (ico->colourmap[i] == colour) return i;
	}

	return -1;
}

int main(int argc, char **argv)
{
	struct icon icon;
	long colourmap[256];
	long *pic = NULL, bpl, xsiz, ysiz;
	long i,j,k,c;

	if (argc<2) {
		fprintf(stderr, "generateicon <picture file>\n");
		return 1;
	}

	memset(&icon, 0, sizeof(icon));
	icon.colourmap = colourmap;

	kpzload(argv[1], (long*)&pic, &bpl, &icon.width, &icon.height);
	if (!pic) {
		fprintf(stderr, "Failure loading %s\n", argv[1]);
		return 1;
	}

	icon.pixels = (unsigned char *)calloc(icon.width, icon.height);
	
	for (i=0; i<icon.height; i++) {
		for (j=0; j<icon.width; j++) {
			k = *((long*)((char*)pic+(i*bpl+j*4)));
			if ((c=findcolour( k, &icon )) < 0) {
				if (icon.ncolours == 256) {
					free(pic);
					free(icon.pixels);
					fprintf(stderr, "image has more than 256 colours\n");
					return 1;
				}
				icon.colourmap[ (c = icon.ncolours++) ] = k;
			}
			icon.pixels[icon.width*i+j] = (unsigned char)c;
		}
	}

	writeicon(stdout, &icon);
	
	free(pic);
	free(icon.pixels);

	return 0;
}

