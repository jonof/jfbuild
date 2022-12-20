// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "compat.h"
#include "pragmas.h"

#define MAXPALOOKUPS 256

static int numpalookups, transratio;
static char palettefilename[13];
static unsigned char origpalookup[MAXPALOOKUPS<<8];
static unsigned char palette[768], palookup[MAXPALOOKUPS<<8], transluc[65536];
static unsigned char closestcol[64][64][64];

#define FASTPALGRIDSIZ 8
static int rdist[129], gdist[129], bdist[129];
static unsigned char colhere[((FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2))>>3];
static unsigned char colhead[(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)];
static int colnext[256];
static char coldist[8] = {0,1,2,3,4,3,2,1};
static int colscan[27];



unsigned char getclosestcol(int r, int g, int b)
{
	int i, j, k, dist, mindist, retcol;
	int *rlookup, *glookup, *blookup;
	unsigned char *ptr;

	if (closestcol[r][g][b] != 255) return(closestcol[r][g][b]);

	j = (r>>3)*FASTPALGRIDSIZ*FASTPALGRIDSIZ+(g>>3)*FASTPALGRIDSIZ+(b>>3)+FASTPALGRIDSIZ*FASTPALGRIDSIZ+FASTPALGRIDSIZ+1;
	mindist = min(rdist[coldist[r&7]+64+8],gdist[coldist[g&7]+64+8]);
	mindist = min(mindist,bdist[coldist[b&7]+64+8]);
	mindist++;

	rlookup = (int *)&rdist[64-r];
	glookup = (int *)&gdist[64-g];
	blookup = (int *)&bdist[64-b];

	retcol = -1;
	for(k=26;k>=0;k--)
	{
		i = colscan[k]+j; if ((colhere[i>>3]&(1<<(i&7))) == 0) continue;
		for(i=colhead[i];i>=0;i=colnext[i])
		{
			ptr = &palette[i*3];
			dist = glookup[ptr[1]]; if (dist >= mindist) continue;
			dist += rlookup[ptr[0]]; if (dist >= mindist) continue;
			dist += blookup[ptr[2]]; if (dist >= mindist) continue;
			mindist = dist; retcol = i;
		}
	}
	if (retcol < 0)
	{
		mindist = 0x7fffffff;
		ptr = &palette[768-3];
		for(i=255;i>=0;i--,ptr-=3)
		{
			dist = glookup[ptr[1]]; if (dist >= mindist) continue;
			dist += rlookup[ptr[0]]; if (dist >= mindist) continue;
			dist += blookup[ptr[2]]; if (dist >= mindist) continue;
			mindist = dist; retcol = i;
		}
	}
	ptr = &closestcol[r][g][b];
	*ptr = retcol;
	if ((r >= 4) && (ptr[(-2)<<12] == retcol)) ptr[(-3)<<12] = retcol, ptr[(-2)<<12] = retcol, ptr[(-1)<<12] = retcol;
	if ((g >= 4) && (ptr[(-2)<<6] == retcol)) ptr[(-3)<<6] = retcol, ptr[(-2)<<6] = retcol, ptr[(-1)<<6] = retcol;
	if ((b >= 4) && (ptr[(-2)] == retcol)) ptr[(-3)] = retcol, ptr[(-2)] = retcol, ptr[(-1)] = retcol;
	if ((r < 64-4) && (ptr[(2)<<12] == retcol)) ptr[(3)<<12] = retcol, ptr[(2)<<12] = retcol, ptr[(1)<<12] = retcol;
	if ((g < 64-4) && (ptr[(2)<<6] == retcol)) ptr[(3)<<6] = retcol, ptr[(2)<<6] = retcol, ptr[(1)<<6] = retcol;
	if ((b < 64-4) && (ptr[(2)] == retcol)) ptr[(3)] = retcol, ptr[(2)] = retcol, ptr[(1)] = retcol;
	if ((r >= 2) && (ptr[(-1)<<12] == retcol)) ptr[(-1)<<12] = retcol;
	if ((g >= 2) && (ptr[(-1)<<6] == retcol)) ptr[(-1)<<6] = retcol;
	if ((b >= 2) && (ptr[(-1)] == retcol)) ptr[(-1)] = retcol;
	if ((r < 64-2) && (ptr[(1)<<12] == retcol)) ptr[(1)<<12] = retcol;
	if ((g < 64-2) && (ptr[(1)<<6] == retcol)) ptr[(1)<<6] = retcol;
	if ((b < 64-2) && (ptr[(1)] == retcol)) ptr[(1)] = retcol;
	return(retcol);
}

unsigned char getpalookup(char dashade, unsigned char dacol)
{
	int r, g, b, t;
	unsigned char *ptr;

	ptr = &palette[dacol*3];
	t = divscale16(numpalookups-dashade,numpalookups);
	r = ((ptr[0]*t+32768)>>16);
	g = ((ptr[1]*t+32768)>>16);
	b = ((ptr[2]*t+32768)>>16);
	return(getclosestcol(r,g,b));
}

unsigned char gettrans(unsigned char dat1, unsigned char dat2, int datransratio)
{
	int r, g, b;
	unsigned char *ptr, *ptr2;

	ptr = &palette[dat1*3];
	ptr2 = &palette[dat2*3];
	r = ptr[0]; r += (((ptr2[0]-r)*datransratio+128)>>8);
	g = ptr[1]; g += (((ptr2[1]-g)*datransratio+128)>>8);
	b = ptr[2]; b += (((ptr2[2]-b)*datransratio+128)>>8);
	return(getclosestcol(r,g,b));
}

void initfastcolorlookup(int rscale, int gscale, int bscale)
{
	int i, j, x, y, z;
	unsigned char *ptr;

	j = 0;
	for(i=64;i>=0;i--)
	{
		//j = (i-64)*(i-64);
		rdist[i] = rdist[128-i] = j*rscale;
		gdist[i] = gdist[128-i] = j*gscale;
		bdist[i] = bdist[128-i] = j*bscale;
		j += 129-(i<<1);
	}

	memset(colhere,0,sizeof(colhere));
	memset(colhead,0,sizeof(colhead));

	ptr = &palette[768-3];
	for(i=255;i>=0;i--,ptr-=3)
	{
		j = (ptr[0]>>3)*FASTPALGRIDSIZ*FASTPALGRIDSIZ+(ptr[1]>>3)*FASTPALGRIDSIZ+(ptr[2]>>3)+FASTPALGRIDSIZ*FASTPALGRIDSIZ+FASTPALGRIDSIZ+1;
		if (colhere[j>>3]&(1<<(j&7))) colnext[i] = colhead[j]; else colnext[i] = -1;
		colhead[j] = i;
		colhere[j>>3] |= (1<<(j&7));
	}

	i = 0;
	for(x=-FASTPALGRIDSIZ*FASTPALGRIDSIZ;x<=FASTPALGRIDSIZ*FASTPALGRIDSIZ;x+=FASTPALGRIDSIZ*FASTPALGRIDSIZ)
		for(y=-FASTPALGRIDSIZ;y<=FASTPALGRIDSIZ;y+=FASTPALGRIDSIZ)
			for(z=-1;z<=1;z++)
				colscan[i++] = x+y+z;
	i = colscan[13]; colscan[13] = colscan[26]; colscan[26] = i;
}

int main(int argc, char **argv)
{
	char transonly = 0;
	short orignumpalookups, s;
	int fil, i, j, rscale, gscale, bscale;
	unsigned char col, buf[65536];

	if (argc>1) {
		if (argv[1][0] == '-') {
			if (argv[1][1] == 't') { transonly = 1; puts("Updating translucency table ONLY"); }
			argc--;
			argv++;
		}
	}

	if ((argc != 3) && (argc != 6))
	{
		printf("TRANSPAL [-t] [numshades][trans#(0-inv,256-opa)][r][g][b]     by Kenneth Silverman\n");
		printf("   Ex #1: transpal 32 170 30 59 11      (I use these values in my BUILD demo)\n");
		printf("                          └──┴──┴─── The RGB scales are optional\n");
		printf("   Ex #2: transpal 64 160\n\n");
		printf("Once tables are generated, the optional -t switch determines what to save:\n");
		printf("   Exclude -t to update both the shade table and transluscent table\n");
		printf("   Include -t to update the transluscent table ONLY\n");
		exit(0);
	}

	strcpy(palettefilename,"palette.dat");
	numpalookups = atoi(argv[1]);
	transratio = atoi(argv[2]);

	if (argc == 6)
	{
		rscale = atoi(argv[3]);
		gscale = atoi(argv[4]);
		bscale = atoi(argv[5]);
	}
	else
	{
		rscale = 30;
		gscale = 59;
		bscale = 11;
	}

	if ((numpalookups < 1) || (numpalookups > 256))
		{ printf("Invalid number of shades\n"); exit(0); }
	if ((transratio < 0) || (transratio > 256))
		{ printf("Invalid transluscent ratio\n"); exit(0); }

	if ((fil = Bopen(palettefilename,BO_BINARY|BO_RDONLY,BS_IREAD)) == -1)
	{
		printf("%s not found",palettefilename);
		return(0);
	}
	Bread(fil,palette,768);
	Bread(fil,&orignumpalookups,2); orignumpalookups = B_LITTLE16(orignumpalookups);
	orignumpalookups = min(max(orignumpalookups,1),256);
	Bread(fil,origpalookup,(int)orignumpalookups<<8);
	Bclose(fil);

	memset(buf,0,65536);

	initfastcolorlookup(rscale,gscale,bscale);
	memset(closestcol, 0xff, 262144);

	for(i=0;i<numpalookups;i++)
		for(j=0;j<256;j++)
		{
			col = getpalookup((char)i,(unsigned char)j);
			palookup[(i<<8)+j] = col;

			drawpixel(((((i<<1)+0)*320+(j+8))>>2)+buf,(int)col);
			drawpixel(((((i<<1)+1)*320+(j+8))>>2)+buf,(int)col);
		}

	for(i=0;i<256;i++)
		for(j=0;j<6;j++)
		{
			drawpixel((((j+132+0)*320+(i+8))>>2)+buf,i);

			drawpixel((((i+132+8)*320+(j+0))>>2)+buf,i);
		}

	for(i=0;i<256;i++)
		for(j=0;j<256;j++)
		{
			col = gettrans((unsigned char)i,(unsigned char)j,transratio);
			transluc[(i<<8)+j] = col;

			drawpixel((((j+132+8)*320+(i+8))>>2)+buf,(int)col);
		}

    if ((fil = Bopen(palettefilename,BO_BINARY|BO_TRUNC|BO_CREAT|BO_WRONLY,BS_IREAD|BS_IWRITE)) == -1)
        { printf("Couldn't save file %s",palettefilename); return(0); }
    Bwrite(fil,palette,768);
    if (transonly) {
        s = B_LITTLE16(orignumpalookups); Bwrite(fil,&s,2);
        Bwrite(fil,origpalookup,(int)orignumpalookups<<8);
        printf("Transluscent table updated\n");
    } else {
        s = B_LITTLE16(numpalookups); Bwrite(fil,&s,2);
        Bwrite(fil,palookup,numpalookups<<8);
        printf("Shade table AND transluscent table updated\n");
    }
    Bwrite(fil,transluc,65536);
    Bclose(fil);

	return 0;
}

