// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)


#define ENGINE

#include "build.h"
#include "pragmas.h"
#include "cache1d.h"
#include "a.h"
#include "osd.h"
#include "crc32.h"

#include "baselayer.h"

#include "engine_priv.h"
#if USE_POLYMOST
# include "polymost_priv.h"
# if USE_OPENGL
#  include "hightile_priv.h"
#  include "polymosttex_priv.h"
#  include "polymosttexcache.h"
#  include "mdsprite_priv.h"
# endif
# ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
# endif
#endif

#include <math.h>
#include <assert.h>

void *kmalloc(bsize_t size) { return(Bmalloc(size)); }
void kfree(void *buffer) { Bfree(buffer); }

void loadvoxel(int voxindex) { (void)voxindex; }
int tiletovox[MAXTILES];
int usevoxels = 1;
#define kloadvoxel loadvoxel

int novoxmips = 0;

	//These variables need to be copied into BUILD
#define MAXXSIZ 256
#define MAXYSIZ 256
#define MAXZSIZ 255
#define MAXVOXMIPS 5
intptr_t voxoff[MAXVOXELS][MAXVOXMIPS];
unsigned char voxlock[MAXVOXELS][MAXVOXMIPS];
int voxscale[MAXVOXELS];

static int ggxinc[MAXXSIZ+1], ggyinc[MAXXSIZ+1];
static int lowrecip[1024], nytooclose, nytoofar;
static unsigned int distrecip[65536];

static int *lookups = NULL;
int dommxoverlay = 1, beforedrawrooms = 1;

static int oxdimen = -1, oviewingrange = -1, oxyaspect = -1;

int curbrightness = 0, gammabrightness = 0;
float curgamma = 1.0;

	//Textured Map variables
static unsigned char globalpolytype;
static short *dotp1[MAXYDIM], *dotp2[MAXYDIM];

static unsigned char tempbuf[MAXWALLS];

int ebpbak, espbak;
#define SLOPALOOKUPSIZ (MAXXDIM<<1)
intptr_t slopalookup[SLOPALOOKUPSIZ];
#if USE_POLYMOST && USE_OPENGL
palette_t palookupfog[MAXPALOOKUPS];
#endif

int artversion, mapversion=7L;	// JBF 20040211: default mapversion to 7
void *pic = NULL;
unsigned char picsiz[MAXTILES], tilefilenum[MAXTILES];
int lastageclock;
int tilefileoffs[MAXTILES];

int artsize = 0;
size_t cachesize = 0;
int editorgridextent = 131072;

static short radarang[1280], radarang2[MAXXDIM];
static unsigned short sqrtable[4096], shlookup[4096+256];
unsigned char pow2char[8] = {1,2,4,8,16,32,64,128};
int pow2long[32] =
{
	1L,2L,4L,8L,
	16L,32L,64L,128L,
	256L,512L,1024L,2048L,
	4096L,8192L,16384L,32768L,
	65536L,131072L,262144L,524288L,
	1048576L,2097152L,4194304L,8388608L,
	16777216L,33554432L,67108864L,134217728L,
	268435456L,536870912L,1073741824L,2147483647L
};
int reciptable[2048], fpuasm;

unsigned char britable[16][256];
extern unsigned char textfont[2048], smalltextfont[2048];

static char kensmessage[128];
char *engineerrstr = NULL;
static BFILE *logfile=NULL;		// log filehandle


//unsigned int ratelimitlast[32], ratelimitn = 0, ratelimit = 60;

#if defined(__WATCOMC__) && USE_ASM

//
// Watcom Inline Assembly Routines
//

#pragma aux nsqrtasm =\
	"test eax, 0xff000000",\
	"mov ebx, eax",\
	"jnz short over24",\
	"shr ebx, 12",\
	"mov cx, word ptr shlookup[ebx*2]",\
	"jmp short under24",\
	"over24: shr ebx, 24",\
	"mov cx, word ptr shlookup[ebx*2+8192]",\
	"under24: shr eax, cl",\
	"mov cl, ch",\
	"mov ax, word ptr sqrtable[eax*2]",\
	"shr eax, cl",\
	parm nomemory [eax]\
	modify exact [eax ebx ecx]
unsigned int nsqrtasm(unsigned int);

#pragma aux msqrtasm =\
	"mov eax, 0x40000000",\
	"mov ebx, 0x20000000",\
	"begit: cmp ecx, eax",\
	"jl skip",\
	"sub ecx, eax",\
	"lea eax, [eax+ebx*4]",\
	"skip: sub eax, ebx",\
	"shr eax, 1",\
	"shr ebx, 2",\
	"jnz begit",\
	"cmp ecx, eax",\
	"sbb eax, -1",\
	"shr eax, 1",\
	parm nomemory [ecx]\
	modify exact [eax ebx ecx]
int msqrtasm(unsigned int);

	//0x007ff000 is (11<<13), 0x3f800000 is (127<<23)
#pragma aux krecipasm =\
	"mov fpuasm, eax",\
	"fild dword ptr fpuasm",\
	"add eax, eax",\
	"fstp dword ptr fpuasm",\
	"sbb ebx, ebx",\
	"mov eax, fpuasm",\
	"mov ecx, eax",\
	"and eax, 0x007ff000",\
	"shr eax, 10",\
	"sub ecx, 0x3f800000",\
	"shr ecx, 23",\
	"mov eax, dword ptr reciptable[eax]",\
	"sar eax, cl",\
	"xor eax, ebx",\
	parm [eax]\
	modify exact [eax ebx ecx]
int krecipasm(int);

#pragma aux getclipmask =\
	"sar eax, 31",\
	"add ebx, ebx",\
	"adc eax, eax",\
	"add ecx, ecx",\
	"adc eax, eax",\
	"add edx, edx",\
	"adc eax, eax",\
	"mov ebx, eax",\
	"shl ebx, 4",\
	"or al, 0xf0",\
	"xor eax, ebx",\
	parm [eax][ebx][ecx][edx]\
	modify exact [eax ebx ecx edx]
int getclipmask(int,int,int,int);

#elif defined(_MSC_VER) && defined(_M_IX86) && USE_ASM	// __WATCOMC__

//
// Microsoft C Inline Assembly Routines
//

static inline int nsqrtasm(int a)
{
	_asm {
		push ebx
		mov eax, a
		test eax, 0xff000000
		mov ebx, eax
		jnz short over24
		shr ebx, 12
		mov cx, word ptr shlookup[ebx*2]
		jmp short under24
	over24:
		shr ebx, 24
		mov cx, word ptr shlookup[ebx*2+8192]
	under24:
		shr eax, cl
		mov cl, ch
		mov ax, word ptr sqrtable[eax*2]
		shr eax, cl
		pop ebx
	}
}

static inline int msqrtasm(int c)
{
	_asm {
		push ebx
		mov ecx, c
		mov eax, 0x40000000
		mov ebx, 0x20000000
	begit:
		cmp ecx, eax
		jl skip
		sub ecx, eax
		lea eax, [eax+ebx*4]
	skip:
		sub eax, ebx
		shr eax, 1
		shr ebx, 2
		jnz begit
		cmp ecx, eax
		sbb eax, -1
		shr eax, 1
		pop ebx
	}
}

	//0x007ff000 is (11<<13), 0x3f800000 is (127<<23)
static inline int krecipasm(int a)
{
	_asm {
		push ebx
		mov eax, a
		mov fpuasm, eax
		fild dword ptr fpuasm
		add eax, eax
		fstp dword ptr fpuasm
		sbb ebx, ebx
		mov eax, fpuasm
		mov ecx, eax
		and eax, 0x007ff000
		shr eax, 10
		sub ecx, 0x3f800000
		shr ecx, 23
		mov eax, dword ptr reciptable[eax]
		sar eax, cl
		xor eax, ebx
		pop ebx
	}
}

static inline int getclipmask(int a, int b, int c, int d)
{
	_asm {
		push ebx
		mov eax, a
		mov ebx, b
		mov ecx, c
		mov edx, d
		sar eax, 31
		add ebx, ebx
		adc eax, eax
		add ecx, ecx
		adc eax, eax
		add edx, edx
		adc eax, eax
		mov ebx, eax
		shl ebx, 4
		or al, 0xf0
		xor eax, ebx
		pop ebx
	}
}

#elif defined(__GNUC__) && defined(__i386__) && USE_ASM	// _MSC_VER

//
// GCC "Inline" Assembly Routines
//

#define nsqrtasm(a) \
	({ int __r, __a=(a); \
	   __asm__ __volatile__ ( \
		"testl $0xff000000, %%eax\n\t" \
		"movl %%eax, %%ebx\n\t" \
		"jnz 0f\n\t" \
		"shrl $12, %%ebx\n\t" \
		"movw %[shlookup](,%%ebx,2), %%cx\n\t" \
		"jmp 1f\n\t" \
		"0:\n\t" \
		"shrl $24, %%ebx\n\t" \
		"movw (%[shlookup]+8192)(,%%ebx,2), %%cx\n\t" \
		"1:\n\t" \
		"shrl %%cl, %%eax\n\t" \
		"movb %%ch, %%cl\n\t" \
		"movw %[sqrtable](,%%eax,2), %%ax\n\t" \
		"shrl %%cl, %%eax" \
		: "=a" (__r) \
		: "a" (__a), [shlookup] "m" (shlookup[0]), [sqrtable] "m" (sqrtable[0]) \
		: "ebx", "ecx", "cc"); \
	 __r; })

	// edx is blown by this code somehow?!
#define msqrtasm(c) \
	({ int __r, __c=(c); \
	   __asm__ __volatile__ ( \
		"movl $0x40000000, %%eax\n\t" \
		"movl $0x20000000, %%ebx\n\t" \
		"0:\n\t" \
		"cmpl %%eax, %%ecx\n\t" \
		"jl 1f\n\t" \
		"subl %%eax, %%ecx\n\t" \
		"leal (%%eax,%%ebx,4), %%eax\n\t" \
		"1:\n\t" \
		"subl %%ebx, %%eax\n\t" \
		"shrl $1, %%eax\n\t" \
		"shrl $2, %%ebx\n\t" \
		"jnz 0b\n\t" \
		"cmpl %%eax, %%ecx\n\t" \
		"sbbl $-1, %%eax\n\t" \
		"shrl $1, %%eax" \
		: "=a" (__r) : "c" (__c) : "edx","ebx", "cc"); \
	 __r; })

#define krecipasm(a) \
	({ int __a=(a); \
	   __asm__ __volatile__ ( \
			"movl %%eax, (%[fpuasm]); fildl (%[fpuasm]); " \
			"addl %%eax, %%eax; fstps (%[fpuasm]); sbbl %%ebx, %%ebx; " \
			"movl (%[fpuasm]), %%eax; movl %%eax, %%ecx; " \
			"andl $0x007ff000, %%eax; shrl $10, %%eax; subl $0x3f800000, %%ecx; " \
			"shrl $23, %%ecx; movl %[reciptable](%%eax), %%eax; " \
			"sarl %%cl, %%eax; xorl %%ebx, %%eax" \
		: "=a" (__a) \
		: "a" (__a), [fpuasm] "m" (fpuasm), [reciptable] "m" (reciptable[0]) \
		: "ebx", "ecx", "memory", "cc"); \
	 __a; })

#define getclipmask(a,b,c,d) \
	({ int __a=(a), __b=(b), __c=(c), __d=(d); \
	   __asm__ __volatile__ ("sarl $31, %%eax; addl %%ebx, %%ebx; adcl %%eax, %%eax; " \
				"addl %%ecx, %%ecx; adcl %%eax, %%eax; addl %%edx, %%edx; " \
				"adcl %%eax, %%eax; movl %%eax, %%ebx; shl $4, %%ebx; " \
				"orb $0xf0, %%al; xorl %%ebx, %%eax" \
		: "=a" (__a), "=b" (__b), "=c" (__c), "=d" (__d) \
		: "a" (__a), "b" (__b), "c" (__c), "d" (__d) : "cc"); \
	 __a; })

#else	// __GNUC__ && __i386__

static inline unsigned int nsqrtasm(unsigned int a)
{	// JBF 20030901: This was a damn lot simpler to reverse engineer than
	// msqrtasm was. Really, it was just like simplifying an algebra equation.
	unsigned short c;

	if (a & 0xff000000) {			// test eax, 0xff000000  /  jnz short over24
		c = shlookup[(a >> 24) + 4096];	// mov ebx, eax
						// over24: shr ebx, 24
						// mov cx, word ptr shlookup[ebx*2+8192]
	} else {
		c = shlookup[a >> 12];		// mov ebx, eax
						// shr ebx, 12
						// mov cx, word ptr shlookup[ebx*2]
						// jmp short under24
	}
	a >>= c&0xff;				// under24: shr eax, cl
	a = (a&0xffff0000)|(sqrtable[a]);	// mov ax, word ptr sqrtable[eax*2]
	a >>= ((c&0xff00) >> 8);		// mov cl, ch
						// shr eax, cl
	return a;
}

static inline int msqrtasm(unsigned int c)
{
	unsigned int a,b;

	a = 0x40000000l;		// mov eax, 0x40000000
	b = 0x20000000l;		// mov ebx, 0x20000000
	do {				// begit:
		if (c >= a) {		// cmp ecx, eax	 /  jl skip
			c -= a;		// sub ecx, eax
			a += b*4;	// lea eax, [eax+ebx*4]
		}			// skip:
		a -= b;			// sub eax, ebx
		a >>= 1;		// shr eax, 1
		b >>= 2;		// shr ebx, 2
	} while (b);			// jnz begit
	if (c >= a)			// cmp ecx, eax
		a++;			// sbb eax, -1
	a >>= 1;			// shr eax, 1
	return a;
}

static inline int krecipasm(int i)
{ // Ken did this
	float f = (float)i; i = *(int *)&f;
	return((reciptable[(i>>12)&2047]>>(((i-0x3f800000)>>23)&31))^(i>>31));
}


static inline int getclipmask(int a, int b, int c, int d)
{ // Ken did this
	d = ((a<0)*8) + ((b<0)*4) + ((c<0)*2) + (d<0);
	return(((d<<4)^0xf0)|d);
}

#endif


int xb1[MAXWALLSB];
static int yb1[MAXWALLSB], xb2[MAXWALLSB], yb2[MAXWALLSB];
int rx1[MAXWALLSB], ry1[MAXWALLSB];
static int rx2[MAXWALLSB], ry2[MAXWALLSB];
short p2[MAXWALLSB];
short thesector[MAXWALLSB], thewall[MAXWALLSB];

short bunchfirst[MAXWALLSB], bunchlast[MAXWALLSB];

static short smost[MAXYSAVES], smostcnt;
static short smoststart[MAXWALLSB];
static unsigned char smostwalltype[MAXWALLSB];
static int smostwall[MAXWALLSB], smostwallcnt = -1L;

short maskwall[MAXWALLSB], maskwallcnt;
static int spritesx[MAXSPRITESONSCREEN];
static int spritesy[MAXSPRITESONSCREEN+1];
static int spritesz[MAXSPRITESONSCREEN];
spritetype *tspriteptr[MAXSPRITESONSCREEN];

short umost[MAXXDIM], dmost[MAXXDIM];
static short bakumost[MAXXDIM], bakdmost[MAXXDIM];
short uplc[MAXXDIM], dplc[MAXXDIM];
static short uwall[MAXXDIM], dwall[MAXXDIM];
static int swplc[MAXXDIM], lplc[MAXXDIM];
static int swall[MAXXDIM], lwall[MAXXDIM+4];
int xdimen = -1, xdimenrecip, halfxdimen, xdimenscale, xdimscale;
int wx1, wy1, wx2, wy2, ydimen, ydimenscale;
intptr_t frameoffset;

static int nrx1[8], nry1[8], nrx2[8], nry2[8];	// JBF 20031206: Thanks Ken

static int rxi[8], ryi[8], rzi[8], rxi2[8], ryi2[8], rzi2[8];
static int xsi[8], ysi[8], *horizlookup=0, *horizlookup2=0, horizycent;

int globalposx, globalposy, globalposz, globalhoriz;
short globalang, globalcursectnum;
int globalpal, cosglobalang, singlobalang;
int cosviewingrangeglobalang, sinviewingrangeglobalang;
unsigned char *globalpalwritten;
int globaluclip, globaldclip, globvis;
int globalvisibility, globalhisibility, globalpisibility, globalcisibility;
unsigned char globparaceilclip, globparaflorclip;

int viewingrangerecip;

int asm1, asm2, asm4;
intptr_t asm3;
int vplce[4], vince[4];
intptr_t palookupoffse[4], bufplce[4];
unsigned char globalxshift, globalyshift;
int globalxpanning, globalypanning, globalshade;
short globalpicnum, globalshiftval;
int globalzd, globalyscale, globalorientation;
intptr_t globalbufplc;
int globalx1, globaly1, globalx2, globaly2, globalx3, globaly3, globalzx;
int globalx, globaly, globalz;

short sectorborder[256], sectorbordercnt;
int pageoffset, ydim16, qsetmode = 0;
int startposx, startposy, startposz;
short startang, startsectnum;
short pointhighlight, linehighlight, highlightcnt;
int lastx[MAXYDIM];
unsigned char *transluc = NULL;

int halfxdim16, midydim16;

#define FASTPALGRIDSIZ 8
static int rdist[129], gdist[129], bdist[129];
static unsigned char colhere[((FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2))>>3];
static unsigned char colhead[(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)];
static int colnext[256];
static unsigned char coldist[8] = {0,1,2,3,4,3,2,1};
static int colscan[27];

static short clipnum, hitwalls[4];
int hitscangoalx = (1<<29)-1, hitscangoaly = (1<<29)-1;
#if USE_POLYMOST
int hitallsprites = 0;
#endif

typedef struct { int x1, y1, x2, y2; } linetype;
static linetype clipit[MAXCLIPNUM];
static short clipsectorlist[MAXCLIPNUM], clipsectnum;
static short clipobjectval[MAXCLIPNUM];

typedef struct
{
	int sx, sy, z;
	short a, picnum;
	signed char dashade;
	unsigned char dapalnum, dastat, pagesleft;
	int cx1, cy1, cx2, cy2;
	int uniqid;	//JF extension
} permfifotype;
static permfifotype permfifo[MAXPERMS];
static int permhead = 0, permtail = 0;

short numscans, numhits, numbunches;

static short capturecount = 0;
static char capturename[20] = "capt0000.xxx", captureatnextpage = 0;
static int screencapture_pcx(char mode);
static int screencapture_tga(char mode);
static int screencapture_png(char mode);

unsigned char vgapal16[4*256] =
{
	00,00,00,00, 42,00,00,00, 00,42,00,00, 42,42,00,00, 00,00,42,00,
	42,00,42,00, 00,21,42,00, 42,42,42,00, 21,21,21,00, 63,21,21,00,
	21,63,21,00, 63,63,21,00, 21,21,63,00, 63,21,63,00, 21,63,63,00,
	63,63,63,00
};

short editstatus = 0;
short searchit;
int searchx = -1, searchy;                          //search input
short searchsector, searchwall, searchstat;     //search output

static char artfilename[20];
static int numtilefiles, artfil = -1, artfilnum, artfilplc;

char inpreparemirror = 0;
static int mirrorsx1, mirrorsy1, mirrorsx2, mirrorsy2;

static int setviewcnt = 0;	// interface layers use this now
static intptr_t bakframeplace[4];
static int bakxsiz[4], bakysiz[4];
static int bakwindowx1[4], bakwindowy1[4];
static int bakwindowx2[4], bakwindowy2[4];
#if USE_POLYMOST
static int bakrendmode,baktile;
#endif

int totalclocklock;

palette_t curpalette[256];			// the current palette, unadjusted for brightness or tint
palette_t curpalettefaded[256];		// the current palette, adjusted for brightness and tint (ie. what gets sent to the card)
palette_t palfadergb = { 0,0,0,0 };
unsigned char palfadedelta = 0;



//
// Internal Engine Functions
//
//int cacheresets = 0,cacheinvalidates = 0;

//
// getpalookup (internal)
//
static inline int getpalookup(int davis, int dashade)
{
	return(min(max(dashade+(davis>>8),0),numpalookups-1));
}


//
// scansector (internal)
//
static void scansector(short sectnum)
{
	walltype *wal, *wal2;
	spritetype *spr;
	int xs, ys, x1, y1, x2, y2, xp1, yp1, xp2=0, yp2=0, templong;
	short z, zz, startwall, endwall, numscansbefore, scanfirst, bunchfrst;
	short nextsectnum;

	if (sectnum < 0) return;

	if (automapping) show2dsector[sectnum>>3] |= pow2char[sectnum&7];

	sectorborder[0] = sectnum, sectorbordercnt = 1;
	do
	{
		sectnum = sectorborder[--sectorbordercnt];

		for(z=headspritesect[sectnum];z>=0;z=nextspritesect[z])
		{
			spr = &sprite[z];
			if ((((spr->cstat&0x8000) == 0) || (showinvisibility)) &&
				  (spr->xrepeat > 0) && (spr->yrepeat > 0) &&
				  (spritesortcnt < MAXSPRITESONSCREEN))
			{
				xs = spr->x-globalposx; ys = spr->y-globalposy;
				if ((spr->cstat&48) || (xs*cosglobalang+ys*singlobalang > 0))
				{
					copybufbyte(spr,&tsprite[spritesortcnt],sizeof(spritetype));
					tsprite[spritesortcnt++].owner = z;
				}
			}
		}

		gotsector[sectnum>>3] |= pow2char[sectnum&7];

		bunchfrst = numbunches;
		numscansbefore = numscans;

		startwall = sector[sectnum].wallptr;
		endwall = startwall + sector[sectnum].wallnum;
		scanfirst = numscans;
		for(z=startwall,wal=&wall[z];z<endwall;z++,wal++)
		{
			nextsectnum = wal->nextsector;

			wal2 = &wall[wal->point2];
			x1 = wal->x-globalposx; y1 = wal->y-globalposy;
			x2 = wal2->x-globalposx; y2 = wal2->y-globalposy;

			if ((nextsectnum >= 0) && ((wal->cstat&32) == 0))
				if ((gotsector[nextsectnum>>3]&pow2char[nextsectnum&7]) == 0)
				{
					templong = x1*y2-x2*y1;
					if (((unsigned)templong+262144) < 524288)
						if (mulscale5(templong,templong) <= (x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))
							sectorborder[sectorbordercnt++] = nextsectnum;
				}

			if ((z == startwall) || (wall[z-1].point2 != z))
			{
				xp1 = dmulscale6(y1,cosglobalang,-x1,singlobalang);
				yp1 = dmulscale6(x1,cosviewingrangeglobalang,y1,sinviewingrangeglobalang);
			}
			else
			{
				xp1 = xp2;
				yp1 = yp2;
			}
			xp2 = dmulscale6(y2,cosglobalang,-x2,singlobalang);
			yp2 = dmulscale6(x2,cosviewingrangeglobalang,y2,sinviewingrangeglobalang);
			if ((yp1 < 256) && (yp2 < 256)) goto skipitaddwall;

				//If wall's NOT facing you
			if (dmulscale32(xp1,yp2,-xp2,yp1) >= 0) goto skipitaddwall;

			if (xp1 >= -yp1)
			{
				if ((xp1 > yp1) || (yp1 == 0)) goto skipitaddwall;
				xb1[numscans] = halfxdimen + scale(xp1,halfxdimen,yp1);
				if (xp1 >= 0) xb1[numscans]++;   //Fix for SIGNED divide
				if (xb1[numscans] >= xdimen) xb1[numscans] = xdimen-1;
				yb1[numscans] = yp1;
			}
			else
			{
				if (xp2 < -yp2) goto skipitaddwall;
				xb1[numscans] = 0;
				templong = yp1-yp2+xp1-xp2;
				if (templong == 0) goto skipitaddwall;
				yb1[numscans] = yp1 + scale(yp2-yp1,xp1+yp1,templong);
			}
			if (yb1[numscans] < 256) goto skipitaddwall;

			if (xp2 <= yp2)
			{
				if ((xp2 < -yp2) || (yp2 == 0)) goto skipitaddwall;
				xb2[numscans] = halfxdimen + scale(xp2,halfxdimen,yp2) - 1;
				if (xp2 >= 0) xb2[numscans]++;   //Fix for SIGNED divide
				if (xb2[numscans] >= xdimen) xb2[numscans] = xdimen-1;
				yb2[numscans] = yp2;
			}
			else
			{
				if (xp1 > yp1) goto skipitaddwall;
				xb2[numscans] = xdimen-1;
				templong = xp2-xp1+yp1-yp2;
				if (templong == 0) goto skipitaddwall;
				yb2[numscans] = yp1 + scale(yp2-yp1,yp1-xp1,templong);
			}
			if ((yb2[numscans] < 256) || (xb1[numscans] > xb2[numscans])) goto skipitaddwall;

				//Made it all the way!
			thesector[numscans] = sectnum; thewall[numscans] = z;
			rx1[numscans] = xp1; ry1[numscans] = yp1;
			rx2[numscans] = xp2; ry2[numscans] = yp2;
			p2[numscans] = numscans+1;
			numscans++;
skipitaddwall:

			if ((wall[z].point2 < z) && (scanfirst < numscans))
				p2[numscans-1] = scanfirst, scanfirst = numscans;
		}

		for(z=numscansbefore;z<numscans;z++)
			if ((wall[thewall[z]].point2 != thewall[p2[z]]) || (xb2[z] >= xb1[p2[z]]))
				bunchfirst[numbunches++] = p2[z], p2[z] = -1;

		for(z=bunchfrst;z<numbunches;z++)
		{
			for(zz=bunchfirst[z];p2[zz]>=0;zz=p2[zz]);
			bunchlast[z] = zz;
		}
	} while (sectorbordercnt > 0);
}


//
// maskwallscan (internal)
//
static void maskwallscan(int x1, int x2, short *uwal, short *dwal, int *swal, int *lwal)
{
	int x, startx, xnice, ynice;
	intptr_t i, fpalookup, p;
	int y1ve[4], y2ve[4], u4, d4, dax, z, tsizx, tsizy;
	char bad;

	tsizx = tilesizx[globalpicnum];
	tsizy = tilesizy[globalpicnum];
	setgotpic(globalpicnum);
	if ((tsizx <= 0) || (tsizy <= 0)) return;
	if ((uwal[x1] > ydimen) && (uwal[x2] > ydimen)) return;
	if ((dwal[x1] < 0) && (dwal[x2] < 0)) return;

	if (waloff[globalpicnum] == 0) loadtile(globalpicnum);

	startx = x1;

	xnice = (pow2long[picsiz[globalpicnum]&15] == tsizx);
	if (xnice) tsizx = (tsizx-1);
	ynice = (pow2long[picsiz[globalpicnum]>>4] == tsizy);
	if (ynice) tsizy = (picsiz[globalpicnum]>>4);

	fpalookup = (intptr_t)palookup[globalpal];

	setupmvlineasm(globalshiftval);

#ifndef ENGINE_USING_A_C

	x = startx;
	while ((startumost[x+windowx1] > startdmost[x+windowx1]) && (x <= x2)) x++;

	p = x+frameoffset;

	for(;(x<=x2)&&(p&3);x++,p++)
	{
		y1ve[0] = max(uwal[x],startumost[x+windowx1]-windowy1);
		y2ve[0] = min(dwal[x],startdmost[x+windowx1]-windowy1);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscale16(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(p+ylookup[y1ve[0]]));
	}
	for(;x<=x2-3;x+=4,p+=4)
	{
		bad = 0;
		for(z=3,dax=x+3;z>=0;z--,dax--)
		{
			y1ve[z] = max(uwal[dax],startumost[dax+windowx1]-windowy1);
			y2ve[z] = min(dwal[dax],startdmost[dax+windowx1]-windowy1)-1;
			if (y2ve[z] < y1ve[z]) { bad += pow2char[z]; continue; }

			i = lwal[dax] + globalxpanning;
			if (i >= tsizx) { if (xnice == 0) i %= tsizx; else i &= tsizx; }
			if (ynice == 0) i *= tsizy; else i <<= tsizy;
			bufplce[z] = waloff[globalpicnum]+i;

			vince[z] = swal[dax]*globalyscale;
			vplce[z] = globalzd + vince[z]*(y1ve[z]-globalhoriz+1);
		}
		if (bad == 15) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscale16(swal[x],globvis),globalshade)<<8);
		palookupoffse[3] = fpalookup+(getpalookup((int)mulscale16(swal[x+3],globvis),globalshade)<<8);

		if ((palookupoffse[0] == palookupoffse[3]) && ((bad&0x9) == 0))
		{
			palookupoffse[1] = palookupoffse[0];
			palookupoffse[2] = palookupoffse[0];
		}
		else
		{
			palookupoffse[1] = fpalookup+(getpalookup((int)mulscale16(swal[x+1],globvis),globalshade)<<8);
			palookupoffse[2] = fpalookup+(getpalookup((int)mulscale16(swal[x+2],globvis),globalshade)<<8);
		}

		u4 = max(max(y1ve[0],y1ve[1]),max(y1ve[2],y1ve[3]));
		d4 = min(min(y2ve[0],y2ve[1]),min(y2ve[2],y2ve[3]));

		if ((bad > 0) || (u4 >= d4))
		{
			if (!(bad&1)) mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0],vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
			if (!(bad&2)) mvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y1ve[1],vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
			if (!(bad&4)) mvlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-y1ve[2],vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
			if (!(bad&8)) mvlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-y1ve[3],vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));
			continue;
		}

		if (u4 > y1ve[0]) vplce[0] = mvlineasm1(vince[0],(void *)palookupoffse[0],u4-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
		if (u4 > y1ve[1]) vplce[1] = mvlineasm1(vince[1],(void *)palookupoffse[1],u4-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
		if (u4 > y1ve[2]) vplce[2] = mvlineasm1(vince[2],(void *)palookupoffse[2],u4-y1ve[2]-1,vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
		if (u4 > y1ve[3]) vplce[3] = mvlineasm1(vince[3],(void *)palookupoffse[3],u4-y1ve[3]-1,vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));

		if (d4 >= u4) mvlineasm4(d4-u4+1,(void *)(ylookup[u4]+p));

		i = p+ylookup[d4+1];
		if (y2ve[0] > d4) mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-d4-1,vplce[0],(void *)bufplce[0],(void *)(i+0));
		if (y2ve[1] > d4) mvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-d4-1,vplce[1],(void *)bufplce[1],(void *)(i+1));
		if (y2ve[2] > d4) mvlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-d4-1,vplce[2],(void *)bufplce[2],(void *)(i+2));
		if (y2ve[3] > d4) mvlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-d4-1,vplce[3],(void *)bufplce[3],(void *)(i+3));
	}
	for(;x<=x2;x++,p++)
	{
		y1ve[0] = max(uwal[x],startumost[x+windowx1]-windowy1);
		y2ve[0] = min(dwal[x],startdmost[x+windowx1]-windowy1);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscale16(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(p+ylookup[y1ve[0]]));
	}

#else	// ENGINE_USING_A_C
	(void)i; (void)u4; (void)d4; (void)dax; (void)z; (void)bad;

	p = startx+frameoffset;
	for(x=startx;x<=x2;x++,p++)
	{
		y1ve[0] = max(uwal[x],startumost[x+windowx1]-windowy1);
		y2ve[0] = min(dwal[x],startdmost[x+windowx1]-windowy1);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscale16(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(p+ylookup[y1ve[0]]));
	}

#endif

	faketimerhandler();
}


//
// wallfront (internal)
//
int wallfront(int l1, int l2)
{
	walltype *wal;
	int x11, y11, x21, y21, x12, y12, x22, y22, dx, dy, t1, t2;

	wal = &wall[thewall[l1]]; x11 = wal->x; y11 = wal->y;
	wal = &wall[wal->point2]; x21 = wal->x; y21 = wal->y;
	wal = &wall[thewall[l2]]; x12 = wal->x; y12 = wal->y;
	wal = &wall[wal->point2]; x22 = wal->x; y22 = wal->y;

	dx = x21-x11; dy = y21-y11;
	t1 = dmulscale2(x12-x11,dy,-dx,y12-y11); //p1(l2) vs. l1
	t2 = dmulscale2(x22-x11,dy,-dx,y22-y11); //p2(l2) vs. l1
	if (t1 == 0) { t1 = t2; if (t1 == 0) return(-1); }
	if (t2 == 0) t2 = t1;
	if ((t1^t2) >= 0)
	{
		t2 = dmulscale2(globalposx-x11,dy,-dx,globalposy-y11); //pos vs. l1
		return((t2^t1) >= 0);
	}

	dx = x22-x12; dy = y22-y12;
	t1 = dmulscale2(x11-x12,dy,-dx,y11-y12); //p1(l1) vs. l2
	t2 = dmulscale2(x21-x12,dy,-dx,y21-y12); //p2(l1) vs. l2
	if (t1 == 0) { t1 = t2; if (t1 == 0) return(-1); }
	if (t2 == 0) t2 = t1;
	if ((t1^t2) >= 0)
	{
		t2 = dmulscale2(globalposx-x12,dy,-dx,globalposy-y12); //pos vs. l2
		return((t2^t1) < 0);
	}
	return(-2);
}


//
// spritewallfront (internal)
//
static int spritewallfront(spritetype *s, int w)
{
	walltype *wal;
	int x1, y1;

	wal = &wall[w]; x1 = wal->x; y1 = wal->y;
	wal = &wall[wal->point2];
	return (dmulscale32(wal->x-x1,s->y-y1,-(s->x-x1),wal->y-y1) >= 0);
}


//
// bunchfront (internal)
//
static int bunchfront(int b1, int b2)
{
	int x1b1, x2b1, x1b2, x2b2, b1f, b2f, i;

	b1f = bunchfirst[b1]; x1b1 = xb1[b1f]; x2b2 = xb2[bunchlast[b2]]+1;
	if (x1b1 >= x2b2) return(-1);
	b2f = bunchfirst[b2]; x1b2 = xb1[b2f]; x2b1 = xb2[bunchlast[b1]]+1;
	if (x1b2 >= x2b1) return(-1);

	if (x1b1 >= x1b2)
	{
		for(i=b2f;xb2[i]<x1b1;i=p2[i]);
		return(wallfront(b1f,i));
	}
	for(i=b1f;xb2[i]<x1b2;i=p2[i]);
	return(wallfront(i,b2f));
}


//
// hline (internal)
//
static void hline(int xr, int yp)
{
	int xl, r, s;

	xl = lastx[yp]; if (xl > xr) return;
	r = horizlookup2[yp-globalhoriz+horizycent];
	asm1 = globalx1*r;
	asm2 = globaly2*r;
	s = ((int)getpalookup((int)mulscale16(r,globvis),globalshade)<<8);

	hlineasm4(xr-xl,0L,s,globalx2*r+globalypanning,globaly1*r+globalxpanning,
		(void *)(ylookup[yp]+xr+frameoffset));
}


//
// slowhline (internal)
//
static void slowhline(int xr, int yp)
{
	int xl, r;

	xl = lastx[yp]; if (xl > xr) return;
	r = horizlookup2[yp-globalhoriz+horizycent];
	asm1 = globalx1*r;
	asm2 = globaly2*r;

	asm3 = (intptr_t)globalpalwritten + ((int)getpalookup((int)mulscale16(r,globvis),globalshade)<<8);
	if (!(globalorientation&256))
	{
		mhline((void *)globalbufplc,globaly1*r+globalxpanning-asm1*(xr-xl),(xr-xl)<<16,0L,
			globalx2*r+globalypanning-asm2*(xr-xl),(void *)(ylookup[yp]+xl+frameoffset));
		return;
	}
	thline((void *)globalbufplc,globaly1*r+globalxpanning-asm1*(xr-xl),(xr-xl)<<16,0L,
		globalx2*r+globalypanning-asm2*(xr-xl),(void *)(ylookup[yp]+xl+frameoffset));
}


//
// prepwall (internal)
//
static void prepwall(int z, walltype *wal)
{
	int i, l=0, ol=0, splc, sinc, x, topinc, top, botinc, bot, walxrepeat;

	walxrepeat = (wal->xrepeat<<3);

		//lwall calculation
	i = xb1[z]-halfxdimen;
	topinc = -(ry1[z]>>2);
	botinc = ((ry2[z]-ry1[z])>>8);
	top = mulscale5(rx1[z],xdimen)+mulscale2(topinc,i);
	bot = mulscale11(rx1[z]-rx2[z],xdimen)+mulscale2(botinc,i);

	splc = mulscale19(ry1[z],xdimscale);
	sinc = mulscale16(ry2[z]-ry1[z],xdimscale);

	x = xb1[z];
	if (bot != 0)
	{
		l = divscale12(top,bot);
		swall[x] = mulscale21(l,sinc)+splc;
		l *= walxrepeat;
		lwall[x] = (l>>18);
	}
	while (x+4 <= xb2[z])
	{
		top += topinc; bot += botinc;
		if (bot != 0)
		{
			ol = l; l = divscale12(top,bot);
			swall[x+4] = mulscale21(l,sinc)+splc;
			l *= walxrepeat;
			lwall[x+4] = (l>>18);
		}
		i = ((ol+l)>>1);
		lwall[x+2] = (i>>18);
		lwall[x+1] = ((ol+i)>>19);
		lwall[x+3] = ((l+i)>>19);
		swall[x+2] = ((swall[x]+swall[x+4])>>1);
		swall[x+1] = ((swall[x]+swall[x+2])>>1);
		swall[x+3] = ((swall[x+4]+swall[x+2])>>1);
		x += 4;
	}
	if (x+2 <= xb2[z])
	{
		top += (topinc>>1); bot += (botinc>>1);
		if (bot != 0)
		{
			ol = l; l = divscale12(top,bot);
			swall[x+2] = mulscale21(l,sinc)+splc;
			l *= walxrepeat;
			lwall[x+2] = (l>>18);
		}
		lwall[x+1] = ((l+ol)>>19);
		swall[x+1] = ((swall[x]+swall[x+2])>>1);
		x += 2;
	}
	if (x+1 <= xb2[z])
	{
		bot += (botinc>>2);
		if (bot != 0)
		{
			l = divscale12(top+(topinc>>2),bot);
			swall[x+1] = mulscale21(l,sinc)+splc;
			lwall[x+1] = mulscale18(l,walxrepeat);
		}
	}

	if (lwall[xb1[z]] < 0) lwall[xb1[z]] = 0;
	if ((lwall[xb2[z]] >= walxrepeat) && (walxrepeat)) lwall[xb2[z]] = walxrepeat-1;
	if (wal->cstat&8)
	{
		walxrepeat--;
		for(x=xb1[z];x<=xb2[z];x++) lwall[x] = walxrepeat-lwall[x];
	}
}


//
// animateoffs (internal)
//
int animateoffs(short tilenum, short fakevar)
{
	int i, k, offs;

	(void)fakevar;

	offs = 0;
	i = (totalclocklock>>((picanm[tilenum]>>24)&15));
	if ((picanm[tilenum]&63) > 0)
	{
		switch(picanm[tilenum]&192)
		{
			case 64:
				k = (i%((picanm[tilenum]&63)<<1));
				if (k < (picanm[tilenum]&63))
					offs = k;
				else
					offs = (((picanm[tilenum]&63)<<1)-k);
				break;
			case 128:
				offs = (i%((picanm[tilenum]&63)+1));
					break;
			case 192:
				offs = -(i%((picanm[tilenum]&63)+1));
		}
	}
	return(offs);
}


//
// owallmost (internal)
//
static int owallmost(short *mostbuf, int w, int z)
{
	int bad, inty, xcross, y, yinc;
	int s1, s2, s3, s4, ix1, ix2, iy1, iy2, t;
	int i;

	z <<= 7;
	s1 = mulscale20(globaluclip,yb1[w]); s2 = mulscale20(globaluclip,yb2[w]);
	s3 = mulscale20(globaldclip,yb1[w]); s4 = mulscale20(globaldclip,yb2[w]);
	bad = (z<s1)+((z<s2)<<1)+((z>s3)<<2)+((z>s4)<<3);

	ix1 = xb1[w]; iy1 = yb1[w];
	ix2 = xb2[w]; iy2 = yb2[w];

	if ((bad&3) == 3)
	{
		//clearbufbyte(&mostbuf[ix1],(ix2-ix1+1)*sizeof(mostbuf[0]),0L);
		for (i=ix1; i<=ix2; i++) mostbuf[i] = 0;
		return(bad);
	}

	if ((bad&12) == 12)
	{
		//clearbufbyte(&mostbuf[ix1],(ix2-ix1+1)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
		for (i=ix1; i<=ix2; i++) mostbuf[i] = ydimen;
		return(bad);
	}

	if (bad&3)
	{
		t = divscale30(z-s1,s2-s1);
		inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		xcross = xb1[w] + scale(mulscale30(yb2[w],t),xb2[w]-xb1[w],inty);

		if ((bad&3) == 2)
		{
			if (xb1[w] <= xcross) { iy2 = inty; ix2 = xcross; }
			//clearbufbyte(&mostbuf[xcross+1],(xb2[w]-xcross)*sizeof(mostbuf[0]),0L);
			for (i=xcross+1; i<=xb2[w]; i++) mostbuf[i] = 0;
		}
		else
		{
			if (xcross <= xb2[w]) { iy1 = inty; ix1 = xcross; }
			//clearbufbyte(&mostbuf[xb1[w]],(xcross-xb1[w]+1)*sizeof(mostbuf[0]),0L);
			for (i=xb1[w]; i<=xcross; i++) mostbuf[i] = 0;
		}
	}

	if (bad&12)
	{
		t = divscale30(z-s3,s4-s3);
		inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		xcross = xb1[w] + scale(mulscale30(yb2[w],t),xb2[w]-xb1[w],inty);

		if ((bad&12) == 8)
		{
			if (xb1[w] <= xcross) { iy2 = inty; ix2 = xcross; }
			//clearbufbyte(&mostbuf[xcross+1],(xb2[w]-xcross)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
			for (i=xcross+1; i<=xb2[w]; i++) mostbuf[i] = ydimen;
		}
		else
		{
			if (xcross <= xb2[w]) { iy1 = inty; ix1 = xcross; }
			//clearbufbyte(&mostbuf[xb1[w]],(xcross-xb1[w]+1)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
			for (i=xb1[w]; i<=xcross; i++) mostbuf[i] = ydimen;
		}
	}

	y = (scale(z,xdimenscale,iy1)<<4);
	yinc = ((scale(z,xdimenscale,iy2)<<4)-y) / (ix2-ix1+1);
	qinterpolatedown16short(&mostbuf[ix1],ix2-ix1+1,y+(globalhoriz<<16),yinc);

	if (mostbuf[ix1] < 0) mostbuf[ix1] = 0;
	if (mostbuf[ix1] > ydimen) mostbuf[ix1] = ydimen;
	if (mostbuf[ix2] < 0) mostbuf[ix2] = 0;
	if (mostbuf[ix2] > ydimen) mostbuf[ix2] = ydimen;

	return(bad);
}


//
// wallmost (internal)
//
int wallmost(short *mostbuf, int w, int sectnum, unsigned char dastat)
{
	int bad, i, j, t, y, z, inty, intz, xcross, yinc, fw;
	int x1, y1, z1, x2, y2, z2, xv, yv, dx, dy, dasqr, oz1, oz2;
	int s1, s2, s3, s4, ix1, ix2, iy1, iy2;

	if (dastat == 0)
	{
		z = sector[sectnum].ceilingz-globalposz;
		if ((sector[sectnum].ceilingstat&2) == 0) return(owallmost(mostbuf,w,z));
	}
	else
	{
		z = sector[sectnum].floorz-globalposz;
		if ((sector[sectnum].floorstat&2) == 0) return(owallmost(mostbuf,w,z));
	}

	i = thewall[w];
	if (i == sector[sectnum].wallptr) return(owallmost(mostbuf,w,z));

	x1 = wall[i].x; x2 = wall[wall[i].point2].x-x1;
	y1 = wall[i].y; y2 = wall[wall[i].point2].y-y1;

	fw = sector[sectnum].wallptr; i = wall[fw].point2;
	dx = wall[i].x-wall[fw].x; dy = wall[i].y-wall[fw].y;
	dasqr = krecipasm(nsqrtasm(dx*dx+dy*dy));

	if (xb1[w] == 0)
		{ xv = cosglobalang+sinviewingrangeglobalang; yv = singlobalang-cosviewingrangeglobalang; }
	else
		{ xv = x1-globalposx; yv = y1-globalposy; }
	i = xv*(y1-globalposy)-yv*(x1-globalposx); j = yv*x2-xv*y2;
	if (klabs(j) > klabs(i>>3)) i = divscale28(i,j);
	if (dastat == 0)
	{
		t = mulscale15(sector[sectnum].ceilingheinum,dasqr);
		z1 = sector[sectnum].ceilingz;
	}
	else
	{
		t = mulscale15(sector[sectnum].floorheinum,dasqr);
		z1 = sector[sectnum].floorz;
	}
	z1 = dmulscale24(dx*t,mulscale20(y2,i)+((y1-wall[fw].y)<<8),
						 -dy*t,mulscale20(x2,i)+((x1-wall[fw].x)<<8))+((z1-globalposz)<<7);


	if (xb2[w] == xdimen-1)
		{ xv = cosglobalang-sinviewingrangeglobalang; yv = singlobalang+cosviewingrangeglobalang; }
	else
		{ xv = (x2+x1)-globalposx; yv = (y2+y1)-globalposy; }
	i = xv*(y1-globalposy)-yv*(x1-globalposx); j = yv*x2-xv*y2;
	if (klabs(j) > klabs(i>>3)) i = divscale28(i,j);
	if (dastat == 0)
	{
		t = mulscale15(sector[sectnum].ceilingheinum,dasqr);
		z2 = sector[sectnum].ceilingz;
	}
	else
	{
		t = mulscale15(sector[sectnum].floorheinum,dasqr);
		z2 = sector[sectnum].floorz;
	}
	z2 = dmulscale24(dx*t,mulscale20(y2,i)+((y1-wall[fw].y)<<8),
						 -dy*t,mulscale20(x2,i)+((x1-wall[fw].x)<<8))+((z2-globalposz)<<7);


	s1 = mulscale20(globaluclip,yb1[w]); s2 = mulscale20(globaluclip,yb2[w]);
	s3 = mulscale20(globaldclip,yb1[w]); s4 = mulscale20(globaldclip,yb2[w]);
	bad = (z1<s1)+((z2<s2)<<1)+((z1>s3)<<2)+((z2>s4)<<3);

	ix1 = xb1[w]; ix2 = xb2[w];
	iy1 = yb1[w]; iy2 = yb2[w];
	oz1 = z1; oz2 = z2;

	if ((bad&3) == 3)
	{
		//clearbufbyte(&mostbuf[ix1],(ix2-ix1+1)*sizeof(mostbuf[0]),0L);
		for (i=ix1; i<=ix2; i++) mostbuf[i] = 0;
		return(bad);
	}

	if ((bad&12) == 12)
	{
		//clearbufbyte(&mostbuf[ix1],(ix2-ix1+1)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
		for (i=ix1; i<=ix2; i++) mostbuf[i] = ydimen;
		return(bad);
	}

	if (bad&3)
	{
			//inty = intz / (globaluclip>>16)
		t = divscale30(oz1-s1,s2-s1+oz1-oz2);
		inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		intz = oz1 + mulscale30(oz2-oz1,t);
		xcross = xb1[w] + scale(mulscale30(yb2[w],t),xb2[w]-xb1[w],inty);

		//t = divscale30((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
		//inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		//intz = z1 + mulscale30(z2-z1,t);

		if ((bad&3) == 2)
		{
			if (xb1[w] <= xcross) { z2 = intz; iy2 = inty; ix2 = xcross; }
			//clearbufbyte(&mostbuf[xcross+1],(xb2[w]-xcross)*sizeof(mostbuf[0]),0L);
			for (i=xcross+1; i<=xb2[w]; i++) mostbuf[i] = 0;
		}
		else
		{
			if (xcross <= xb2[w]) { z1 = intz; iy1 = inty; ix1 = xcross; }
			//clearbufbyte(&mostbuf[xb1[w]],(xcross-xb1[w]+1)*sizeof(mostbuf[0]),0L);
			for (i=xb1[w]; i<=xcross; i++) mostbuf[i] = 0;
		}
	}

	if (bad&12)
	{
			//inty = intz / (globaldclip>>16)
		t = divscale30(oz1-s3,s4-s3+oz1-oz2);
		inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		intz = oz1 + mulscale30(oz2-oz1,t);
		xcross = xb1[w] + scale(mulscale30(yb2[w],t),xb2[w]-xb1[w],inty);

		//t = divscale30((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
		//inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		//intz = z1 + mulscale30(z2-z1,t);

		if ((bad&12) == 8)
		{
			if (xb1[w] <= xcross) { z2 = intz; iy2 = inty; ix2 = xcross; }
			//clearbufbyte(&mostbuf[xcross+1],(xb2[w]-xcross)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
			for (i=xcross+1; i<=xb2[w]; i++) mostbuf[i] = ydimen;
		}
		else
		{
			if (xcross <= xb2[w]) { z1 = intz; iy1 = inty; ix1 = xcross; }
			//clearbufbyte(&mostbuf[xb1[w]],(xcross-xb1[w]+1)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
			for (i=xb1[w]; i<=xcross; i++) mostbuf[i] = ydimen;
		}
	}

	y = (scale(z1,xdimenscale,iy1)<<4);
	yinc = ((scale(z2,xdimenscale,iy2)<<4)-y) / (ix2-ix1+1);
	qinterpolatedown16short(&mostbuf[ix1],ix2-ix1+1,y+(globalhoriz<<16),yinc);

	if (mostbuf[ix1] < 0) mostbuf[ix1] = 0;
	if (mostbuf[ix1] > ydimen) mostbuf[ix1] = ydimen;
	if (mostbuf[ix2] < 0) mostbuf[ix2] = 0;
	if (mostbuf[ix2] > ydimen) mostbuf[ix2] = ydimen;

	return(bad);
}


//
// ceilscan (internal)
//
static void ceilscan(int x1, int x2, int sectnum)
{
	int i, j, ox, oy, x, y1, y2, twall, bwall;
	sectortype *sec;

	sec = &sector[sectnum];
	if (palookup[sec->ceilingpal] != globalpalwritten)
	{
		globalpalwritten = palookup[sec->ceilingpal];
		if (!globalpalwritten) globalpalwritten = palookup[globalpal];	// JBF: fixes null-pointer crash
		setpalookupaddress(globalpalwritten);
	}

	globalzd = sec->ceilingz-globalposz;
	if (globalzd > 0) return;
	globalpicnum = sec->ceilingpicnum;
	if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
	setgotpic(globalpicnum);
	if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0)) return;
	if (picanm[globalpicnum]&192) globalpicnum += animateoffs((short)globalpicnum,(short)sectnum);

	if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
	globalbufplc = waloff[globalpicnum];

	globalshade = (int)sec->ceilingshade;
	globvis = globalcisibility;
	if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
	globalorientation = (int)sec->ceilingstat;


	if ((globalorientation&64) == 0)
	{
		globalx1 = singlobalang; globalx2 = singlobalang;
		globaly1 = cosglobalang; globaly2 = cosglobalang;
		globalxpanning = (globalposx<<20);
		globalypanning = -(globalposy<<20);
	}
	else
	{
		j = sec->wallptr;
		ox = wall[wall[j].point2].x - wall[j].x;
		oy = wall[wall[j].point2].y - wall[j].y;
		i = nsqrtasm(ox*ox+oy*oy); if (i == 0) i = 1024; else i = 1048576/i;
		globalx1 = mulscale10(dmulscale10(ox,singlobalang,-oy,cosglobalang),i);
		globaly1 = mulscale10(dmulscale10(ox,cosglobalang,oy,singlobalang),i);
		globalx2 = -globalx1;
		globaly2 = -globaly1;

		ox = ((wall[j].x-globalposx)<<6); oy = ((wall[j].y-globalposy)<<6);
		i = dmulscale14(oy,cosglobalang,-ox,singlobalang);
		j = dmulscale14(ox,cosglobalang,oy,singlobalang);
		ox = i; oy = j;
		globalxpanning = globalx1*ox - globaly1*oy;
		globalypanning = globaly2*ox + globalx2*oy;
	}
	globalx2 = mulscale16(globalx2,viewingrangerecip);
	globaly1 = mulscale16(globaly1,viewingrangerecip);
	globalxshift = (8-(picsiz[globalpicnum]&15));
	globalyshift = (8-(picsiz[globalpicnum]>>4));
	if (globalorientation&8) { globalxshift++; globalyshift++; }

	if ((globalorientation&0x4) > 0)
	{
		i = globalxpanning; globalxpanning = globalypanning; globalypanning = i;
		i = globalx2; globalx2 = -globaly1; globaly1 = -i;
		i = globalx1; globalx1 = globaly2; globaly2 = i;
	}
	if ((globalorientation&0x10) > 0) globalx1 = -globalx1, globaly1 = -globaly1, globalxpanning = -globalxpanning;
	if ((globalorientation&0x20) > 0) globalx2 = -globalx2, globaly2 = -globaly2, globalypanning = -globalypanning;
	globalx1 <<= globalxshift; globaly1 <<= globalxshift;
	globalx2 <<= globalyshift;  globaly2 <<= globalyshift;
	globalxpanning <<= globalxshift; globalypanning <<= globalyshift;
	globalxpanning += (((int)sec->ceilingxpanning)<<24);
	globalypanning += (((int)sec->ceilingypanning)<<24);
	globaly1 = (-globalx1-globaly1)*halfxdimen;
	globalx2 = (globalx2-globaly2)*halfxdimen;

	sethlinesizes(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4,(void *)globalbufplc);

	globalx2 += globaly2*(x1-1);
	globaly1 += globalx1*(x1-1);
	globalx1 = mulscale16(globalx1,globalzd);
	globalx2 = mulscale16(globalx2,globalzd);
	globaly1 = mulscale16(globaly1,globalzd);
	globaly2 = mulscale16(globaly2,globalzd);
	globvis = klabs(mulscale10(globvis,globalzd));

	if (!(globalorientation&0x180))
	{
		y1 = umost[x1]; y2 = y1;
		for(x=x1;x<=x2;x++)
		{
			twall = umost[x]-1; bwall = min(uplc[x],dmost[x]);
			if (twall < bwall-1)
			{
				if (twall >= y2)
				{
					while (y1 < y2-1) hline(x-1,++y1);
					y1 = twall;
				}
				else
				{
					while (y1 < twall) hline(x-1,++y1);
					while (y1 > twall) lastx[y1--] = x;
				}
				while (y2 > bwall) hline(x-1,--y2);
				while (y2 < bwall) lastx[y2++] = x;
			}
			else
			{
				while (y1 < y2-1) hline(x-1,++y1);
				if (x == x2) { globalx2 += globaly2; globaly1 += globalx1; break; }
				y1 = umost[x+1]; y2 = y1;
			}
			globalx2 += globaly2; globaly1 += globalx1;
		}
		while (y1 < y2-1) hline(x2,++y1);
		faketimerhandler();
		return;
	}

	switch(globalorientation&0x180)
	{
		case 128:
			msethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
		case 256:
			settransnormal();
			tsethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
		case 384:
			settransreverse();
			tsethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
	}

	y1 = umost[x1]; y2 = y1;
	for(x=x1;x<=x2;x++)
	{
		twall = umost[x]-1; bwall = min(uplc[x],dmost[x]);
		if (twall < bwall-1)
		{
			if (twall >= y2)
			{
				while (y1 < y2-1) slowhline(x-1,++y1);
				y1 = twall;
			}
			else
			{
				while (y1 < twall) slowhline(x-1,++y1);
				while (y1 > twall) lastx[y1--] = x;
			}
			while (y2 > bwall) slowhline(x-1,--y2);
			while (y2 < bwall) lastx[y2++] = x;
		}
		else
		{
			while (y1 < y2-1) slowhline(x-1,++y1);
			if (x == x2) { globalx2 += globaly2; globaly1 += globalx1; break; }
			y1 = umost[x+1]; y2 = y1;
		}
		globalx2 += globaly2; globaly1 += globalx1;
	}
	while (y1 < y2-1) slowhline(x2,++y1);
	faketimerhandler();
}


//
// florscan (internal)
//
static void florscan(int x1, int x2, int sectnum)
{
	int i, j, ox, oy, x, y1, y2, twall, bwall;
	sectortype *sec;

	sec = &sector[sectnum];
	if (palookup[sec->floorpal] != globalpalwritten)
	{
		globalpalwritten = palookup[sec->floorpal];
		if (!globalpalwritten) globalpalwritten = palookup[globalpal];	// JBF: fixes null-pointer crash
		setpalookupaddress(globalpalwritten);
	}

	globalzd = globalposz-sec->floorz;
	if (globalzd > 0) return;
	globalpicnum = sec->floorpicnum;
	if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
	setgotpic(globalpicnum);
	if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0)) return;
	if (picanm[globalpicnum]&192) globalpicnum += animateoffs((short)globalpicnum,(short)sectnum);

	if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
	globalbufplc = waloff[globalpicnum];

	globalshade = (int)sec->floorshade;
	globvis = globalcisibility;
	if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
	globalorientation = (int)sec->floorstat;


	if ((globalorientation&64) == 0)
	{
		globalx1 = singlobalang; globalx2 = singlobalang;
		globaly1 = cosglobalang; globaly2 = cosglobalang;
		globalxpanning = (globalposx<<20);
		globalypanning = -(globalposy<<20);
	}
	else
	{
		j = sec->wallptr;
		ox = wall[wall[j].point2].x - wall[j].x;
		oy = wall[wall[j].point2].y - wall[j].y;
		i = nsqrtasm(ox*ox+oy*oy); if (i == 0) i = 1024; else i = 1048576/i;
		globalx1 = mulscale10(dmulscale10(ox,singlobalang,-oy,cosglobalang),i);
		globaly1 = mulscale10(dmulscale10(ox,cosglobalang,oy,singlobalang),i);
		globalx2 = -globalx1;
		globaly2 = -globaly1;

		ox = ((wall[j].x-globalposx)<<6); oy = ((wall[j].y-globalposy)<<6);
		i = dmulscale14(oy,cosglobalang,-ox,singlobalang);
		j = dmulscale14(ox,cosglobalang,oy,singlobalang);
		ox = i; oy = j;
		globalxpanning = globalx1*ox - globaly1*oy;
		globalypanning = globaly2*ox + globalx2*oy;
	}
	globalx2 = mulscale16(globalx2,viewingrangerecip);
	globaly1 = mulscale16(globaly1,viewingrangerecip);
	globalxshift = (8-(picsiz[globalpicnum]&15));
	globalyshift = (8-(picsiz[globalpicnum]>>4));
	if (globalorientation&8) { globalxshift++; globalyshift++; }

	if ((globalorientation&0x4) > 0)
	{
		i = globalxpanning; globalxpanning = globalypanning; globalypanning = i;
		i = globalx2; globalx2 = -globaly1; globaly1 = -i;
		i = globalx1; globalx1 = globaly2; globaly2 = i;
	}
	if ((globalorientation&0x10) > 0) globalx1 = -globalx1, globaly1 = -globaly1, globalxpanning = -globalxpanning;
	if ((globalorientation&0x20) > 0) globalx2 = -globalx2, globaly2 = -globaly2, globalypanning = -globalypanning;
	globalx1 <<= globalxshift; globaly1 <<= globalxshift;
	globalx2 <<= globalyshift;  globaly2 <<= globalyshift;
	globalxpanning <<= globalxshift; globalypanning <<= globalyshift;
	globalxpanning += (((int)sec->floorxpanning)<<24);
	globalypanning += (((int)sec->floorypanning)<<24);
	globaly1 = (-globalx1-globaly1)*halfxdimen;
	globalx2 = (globalx2-globaly2)*halfxdimen;

	sethlinesizes(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4,(void *)globalbufplc);

	globalx2 += globaly2*(x1-1);
	globaly1 += globalx1*(x1-1);
	globalx1 = mulscale16(globalx1,globalzd);
	globalx2 = mulscale16(globalx2,globalzd);
	globaly1 = mulscale16(globaly1,globalzd);
	globaly2 = mulscale16(globaly2,globalzd);
	globvis = klabs(mulscale10(globvis,globalzd));

	if (!(globalorientation&0x180))
	{
		y1 = max(dplc[x1],umost[x1]); y2 = y1;
		for(x=x1;x<=x2;x++)
		{
			twall = max(dplc[x],umost[x])-1; bwall = dmost[x];
			if (twall < bwall-1)
			{
				if (twall >= y2)
				{
					while (y1 < y2-1) hline(x-1,++y1);
					y1 = twall;
				}
				else
				{
					while (y1 < twall) hline(x-1,++y1);
					while (y1 > twall) lastx[y1--] = x;
				}
				while (y2 > bwall) hline(x-1,--y2);
				while (y2 < bwall) lastx[y2++] = x;
			}
			else
			{
				while (y1 < y2-1) hline(x-1,++y1);
				if (x == x2) { globalx2 += globaly2; globaly1 += globalx1; break; }
				y1 = max(dplc[x+1],umost[x+1]); y2 = y1;
			}
			globalx2 += globaly2; globaly1 += globalx1;
		}
		while (y1 < y2-1) hline(x2,++y1);
		faketimerhandler();
		return;
	}

	switch(globalorientation&0x180)
	{
		case 128:
			msethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
		case 256:
			settransnormal();
			tsethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
		case 384:
			settransreverse();
			tsethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
	}

	y1 = max(dplc[x1],umost[x1]); y2 = y1;
	for(x=x1;x<=x2;x++)
	{
		twall = max(dplc[x],umost[x])-1; bwall = dmost[x];
		if (twall < bwall-1)
		{
			if (twall >= y2)
			{
				while (y1 < y2-1) slowhline(x-1,++y1);
				y1 = twall;
			}
			else
			{
				while (y1 < twall) slowhline(x-1,++y1);
				while (y1 > twall) lastx[y1--] = x;
			}
			while (y2 > bwall) slowhline(x-1,--y2);
			while (y2 < bwall) lastx[y2++] = x;
		}
		else
		{
			while (y1 < y2-1) slowhline(x-1,++y1);
			if (x == x2) { globalx2 += globaly2; globaly1 += globalx1; break; }
			y1 = max(dplc[x+1],umost[x+1]); y2 = y1;
		}
		globalx2 += globaly2; globaly1 += globalx1;
	}
	while (y1 < y2-1) slowhline(x2,++y1);
	faketimerhandler();
}


//
// wallscan (internal)
//
static void wallscan(int x1, int x2, short *uwal, short *dwal, int *swal, int *lwal)
{
	int x, xnice, ynice;
	intptr_t i, fpalookup;
	int y1ve[4], y2ve[4], u4, d4, z, tsizx, tsizy;
	char bad;

	if (x2 >= xdim) x2 = xdim-1;

	tsizx = tilesizx[globalpicnum];
	tsizy = tilesizy[globalpicnum];
	setgotpic(globalpicnum);
	if ((tsizx <= 0) || (tsizy <= 0)) return;
	if ((uwal[x1] > ydimen) && (uwal[x2] > ydimen)) return;
	if ((dwal[x1] < 0) && (dwal[x2] < 0)) return;

	if (waloff[globalpicnum] == 0) loadtile(globalpicnum);

	xnice = (pow2long[picsiz[globalpicnum]&15] == tsizx);
	if (xnice) tsizx--;
	ynice = (pow2long[picsiz[globalpicnum]>>4] == tsizy);
	if (ynice) tsizy = (picsiz[globalpicnum]>>4);

	fpalookup = (intptr_t)palookup[globalpal];

	setupvlineasm(globalshiftval);

#ifndef ENGINE_USING_A_C

	x = x1;
	while ((umost[x] > dmost[x]) && (x <= x2)) x++;

	for(;(x<=x2)&&((x+frameoffset)&3);x++)
	{
		y1ve[0] = max(uwal[x],umost[x]);
		y2ve[0] = min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscale16(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		vlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(x+frameoffset+ylookup[y1ve[0]]));
	}
	for(;x<=x2-3;x+=4)
	{
		bad = 0;
		for(z=3;z>=0;z--)
		{
			y1ve[z] = max(uwal[x+z],umost[x+z]);
			y2ve[z] = min(dwal[x+z],dmost[x+z])-1;
			if (y2ve[z] < y1ve[z]) { bad += pow2char[z]; continue; }

			i = lwal[x+z] + globalxpanning;
			if (i >= tsizx) { if (xnice == 0) i %= tsizx; else i &= tsizx; }
			if (ynice == 0) i *= tsizy; else i <<= tsizy;
			bufplce[z] = waloff[globalpicnum]+i;

			vince[z] = swal[x+z]*globalyscale;
			vplce[z] = globalzd + vince[z]*(y1ve[z]-globalhoriz+1);
		}
		if (bad == 15) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscale16(swal[x],globvis),globalshade)<<8);
		palookupoffse[3] = fpalookup+(getpalookup((int)mulscale16(swal[x+3],globvis),globalshade)<<8);

		if ((palookupoffse[0] == palookupoffse[3]) && ((bad&0x9) == 0))
		{
			palookupoffse[1] = palookupoffse[0];
			palookupoffse[2] = palookupoffse[0];
		}
		else
		{
			palookupoffse[1] = fpalookup+(getpalookup((int)mulscale16(swal[x+1],globvis),globalshade)<<8);
			palookupoffse[2] = fpalookup+(getpalookup((int)mulscale16(swal[x+2],globvis),globalshade)<<8);
		}

		u4 = max(max(y1ve[0],y1ve[1]),max(y1ve[2],y1ve[3]));
		d4 = min(min(y2ve[0],y2ve[1]),min(y2ve[2],y2ve[3]));

		if ((bad != 0) || (u4 >= d4))
		{
			if (!(bad&1)) prevlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0],vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+x+frameoffset+0));
			if (!(bad&2)) prevlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y1ve[1],vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+x+frameoffset+1));
			if (!(bad&4)) prevlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-y1ve[2],vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+x+frameoffset+2));
			if (!(bad&8)) prevlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-y1ve[3],vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+x+frameoffset+3));
			continue;
		}

		if (u4 > y1ve[0]) vplce[0] = prevlineasm1(vince[0],(void *)palookupoffse[0],u4-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+x+frameoffset+0));
		if (u4 > y1ve[1]) vplce[1] = prevlineasm1(vince[1],(void *)palookupoffse[1],u4-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+x+frameoffset+1));
		if (u4 > y1ve[2]) vplce[2] = prevlineasm1(vince[2],(void *)palookupoffse[2],u4-y1ve[2]-1,vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+x+frameoffset+2));
		if (u4 > y1ve[3]) vplce[3] = prevlineasm1(vince[3],(void *)palookupoffse[3],u4-y1ve[3]-1,vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+x+frameoffset+3));

		if (d4 >= u4) vlineasm4(d4-u4+1,(void *)(ylookup[u4]+x+frameoffset));

		i = x+frameoffset+ylookup[d4+1];
		if (y2ve[0] > d4) prevlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-d4-1,vplce[0],(void *)bufplce[0],(void *)(i+0));
		if (y2ve[1] > d4) prevlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-d4-1,vplce[1],(void *)bufplce[1],(void *)(i+1));
		if (y2ve[2] > d4) prevlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-d4-1,vplce[2],(void *)bufplce[2],(void *)(i+2));
		if (y2ve[3] > d4) prevlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-d4-1,vplce[3],(void *)bufplce[3],(void *)(i+3));
	}
	for(;x<=x2;x++)
	{
		y1ve[0] = max(uwal[x],umost[x]);
		y2ve[0] = min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscale16(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		vlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(x+frameoffset+ylookup[y1ve[0]]));
	}

#else	// ENGINE_USING_A_C
	(void)i; (void)u4; (void)d4; (void)z; (void)bad;

	for(x=x1;x<=x2;x++)
	{
		y1ve[0] = max(uwal[x],umost[x]);
		y2ve[0] = min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscale16(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		vlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(x+frameoffset+ylookup[y1ve[0]]));
	}

#endif

	faketimerhandler();
}


//
// transmaskvline (internal)
//
static void transmaskvline(int x)
{
	int vplc, vinc, i;
	intptr_t p, palookupoffs, bufplc;
	short y1v, y2v;

	if ((x < 0) || (x >= xdimen)) return;

	y1v = max(uwall[x],startumost[x+windowx1]-windowy1);
	y2v = min(dwall[x],startdmost[x+windowx1]-windowy1);
	y2v--;
	if (y2v < y1v) return;

	palookupoffs = (intptr_t)palookup[globalpal] + (getpalookup((int)mulscale16(swall[x],globvis),globalshade)<<8);

	vinc = swall[x]*globalyscale;
	vplc = globalzd + vinc*(y1v-globalhoriz+1);

	i = lwall[x]+globalxpanning;
	if (i >= tilesizx[globalpicnum]) i %= tilesizx[globalpicnum];
	bufplc = waloff[globalpicnum]+i*tilesizy[globalpicnum];

	p = ylookup[y1v]+x+frameoffset;

	tvlineasm1(vinc,(void *)palookupoffs,y2v-y1v,vplc,(void *)bufplc,(void *)p);
}


//
// transmaskvline2 (internal)
//
#ifndef ENGINE_USING_A_C
static void transmaskvline2(int x)
{
	intptr_t i;
	int y1, y2, x2;
	short y1ve[2], y2ve[2];

	if ((x < 0) || (x >= xdimen)) return;
	if (x == xdimen-1) { transmaskvline(x); return; }

	x2 = x+1;

	y1ve[0] = max(uwall[x],startumost[x+windowx1]-windowy1);
	y2ve[0] = min(dwall[x],startdmost[x+windowx1]-windowy1)-1;
	if (y2ve[0] < y1ve[0]) { transmaskvline(x2); return; }
	y1ve[1] = max(uwall[x2],startumost[x2+windowx1]-windowy1);
	y2ve[1] = min(dwall[x2],startdmost[x2+windowx1]-windowy1)-1;
	if (y2ve[1] < y1ve[1]) { transmaskvline(x); return; }

	palookupoffse[0] = (intptr_t)palookup[globalpal] + (getpalookup((int)mulscale16(swall[x],globvis),globalshade)<<8);
	palookupoffse[1] = (intptr_t)palookup[globalpal] + (getpalookup((int)mulscale16(swall[x2],globvis),globalshade)<<8);

	setuptvlineasm2(globalshiftval,(void *)palookupoffse[0],(void *)palookupoffse[1]);

	vince[0] = swall[x]*globalyscale;
	vince[1] = swall[x2]*globalyscale;
	vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);
	vplce[1] = globalzd + vince[1]*(y1ve[1]-globalhoriz+1);

	i = lwall[x] + globalxpanning;
	if (i >= tilesizx[globalpicnum]) i %= tilesizx[globalpicnum];
	bufplce[0] = waloff[globalpicnum]+i*tilesizy[globalpicnum];

	i = lwall[x2] + globalxpanning;
	if (i >= tilesizx[globalpicnum]) i %= tilesizx[globalpicnum];
	bufplce[1] = waloff[globalpicnum]+i*tilesizy[globalpicnum];

	y1 = max(y1ve[0],y1ve[1]);
	y2 = min(y2ve[0],y2ve[1]);

	i = x+frameoffset;

	if (y1ve[0] != y1ve[1])
	{
		if (y1ve[0] < y1)
			vplce[0] = tvlineasm1(vince[0],(void *)palookupoffse[0],y1-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+i));
		else
			vplce[1] = tvlineasm1(vince[1],(void *)palookupoffse[1],y1-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+i+1));
	}

	if (y2 > y1)
	{
		asm1 = vince[1];
		asm2 = ylookup[y2]+i+1;
		tvlineasm2(vplce[1],vince[0],(void *)bufplce[0],(void *)bufplce[1],vplce[0],(void *)(ylookup[y1]+i));
	}
	else
	{
		asm1 = vplce[0];
		asm2 = vplce[1];
	}

	if (y2ve[0] > y2ve[1])
		tvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y2-1,asm1,(void *)bufplce[0],(void *)(ylookup[y2+1]+i));
	else if (y2ve[0] < y2ve[1])
		tvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y2-1,asm2,(void *)bufplce[1],(void *)(ylookup[y2+1]+i+1));

	faketimerhandler();
}
#endif

//
// transmaskwallscan (internal)
//
static void transmaskwallscan(int x1, int x2)
{
	int x;

	setgotpic(globalpicnum);
	if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0)) return;

	if (waloff[globalpicnum] == 0) loadtile(globalpicnum);

	setuptvlineasm(globalshiftval);

	x = x1;
	while ((startumost[x+windowx1] > startdmost[x+windowx1]) && (x <= x2)) x++;
#ifndef ENGINE_USING_A_C
	if ((x <= x2) && (x&1)) transmaskvline(x), x++;
	while (x < x2) transmaskvline2(x), x += 2;
#endif
	while (x <= x2) transmaskvline(x), x++;
	faketimerhandler();
}


//
// ceilspritehline (internal)
//
static void ceilspritehline(int x2, int y)
{
	int x1, v, bx, by;

	//x = x1 + (x2-x1)t + (y1-y2)u  ~  x = 160v
	//y = y1 + (y2-y1)t + (x2-x1)u  ~  y = (scrx-160)v
	//z = z1 = z2                   ~  z = posz + (scry-horiz)v

	x1 = lastx[y]; if (x2 < x1) return;

	v = mulscale20(globalzd,horizlookup[y-globalhoriz+horizycent]);
	bx = mulscale14(globalx2*x1+globalx1,v) + globalxpanning;
	by = mulscale14(globaly2*x1+globaly1,v) + globalypanning;
	asm1 = mulscale14(globalx2,v);
	asm2 = mulscale14(globaly2,v);

	asm3 = (intptr_t)palookup[globalpal] + (getpalookup((int)mulscale28(klabs(v),globvis),globalshade)<<8);

	if ((globalorientation&2) == 0)
		mhline((void *)globalbufplc,bx,(x2-x1)<<16,0L,by,(void *)(ylookup[y]+x1+frameoffset));
	else
	{
		thline((void *)globalbufplc,bx,(x2-x1)<<16,0L,by,(void *)(ylookup[y]+x1+frameoffset));
	}
}


//
// ceilspritescan (internal)
//
static void ceilspritescan(int x1, int x2)
{
	int x, y1, y2, twall, bwall;

	y1 = uwall[x1]; y2 = y1;
	for(x=x1;x<=x2;x++)
	{
		twall = uwall[x]-1; bwall = dwall[x];
		if (twall < bwall-1)
		{
			if (twall >= y2)
			{
				while (y1 < y2-1) ceilspritehline(x-1,++y1);
				y1 = twall;
			}
			else
			{
				while (y1 < twall) ceilspritehline(x-1,++y1);
				while (y1 > twall) lastx[y1--] = x;
			}
			while (y2 > bwall) ceilspritehline(x-1,--y2);
			while (y2 < bwall) lastx[y2++] = x;
		}
		else
		{
			while (y1 < y2-1) ceilspritehline(x-1,++y1);
			if (x == x2) break;
			y1 = uwall[x+1]; y2 = y1;
		}
	}
	while (y1 < y2-1) ceilspritehline(x2,++y1);
	faketimerhandler();
}


//
// grouscan (internal)
//
#define BITSOFPRECISION 3  //Don't forget to change this in A.ASM also!
static void grouscan(int dax1, int dax2, int sectnum, unsigned char dastat)
{
	int i, j, l, x, y, dx, dy, wx, wy, y1, y2, daz;
	int daslope, dasqr;
	int shoffs, shinc, m1, m2;
	intptr_t *mptr1, *mptr2, *nptr1, *nptr2;
	walltype *wal;
	sectortype *sec;

	sec = &sector[sectnum];

	if (dastat == 0)
	{
		if (globalposz <= getceilzofslope(sectnum,globalposx,globalposy))
			return;  //Back-face culling
		globalorientation = sec->ceilingstat;
		globalpicnum = sec->ceilingpicnum;
		globalshade = sec->ceilingshade;
		globalpal = sec->ceilingpal;
		daslope = sec->ceilingheinum;
		daz = sec->ceilingz;
	}
	else
	{
		if (globalposz >= getflorzofslope(sectnum,globalposx,globalposy))
			return;  //Back-face culling
		globalorientation = sec->floorstat;
		globalpicnum = sec->floorpicnum;
		globalshade = sec->floorshade;
		globalpal = sec->floorpal;
		daslope = sec->floorheinum;
		daz = sec->floorz;
	}

	if (palookup[globalpal] == 0) globalpal = 0;
	if ((picanm[globalpicnum]&192) != 0) globalpicnum += animateoffs(globalpicnum,sectnum);
	setgotpic(globalpicnum);
	if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0)) return;
	if (waloff[globalpicnum] == 0) loadtile(globalpicnum);

	wal = &wall[sec->wallptr];
	wx = wall[wal->point2].x - wal->x;
	wy = wall[wal->point2].y - wal->y;
	dasqr = krecipasm(nsqrtasm(wx*wx+wy*wy));
	i = mulscale21(daslope,dasqr);
	wx *= i; wy *= i;

	globalx = -mulscale19(singlobalang,xdimenrecip);
	globaly = mulscale19(cosglobalang,xdimenrecip);
	globalx1 = (globalposx<<8);
	globaly1 = -(globalposy<<8);
	i = (dax1-halfxdimen)*xdimenrecip;
	globalx2 = mulscale16(cosglobalang<<4,viewingrangerecip) - mulscale27(singlobalang,i);
	globaly2 = mulscale16(singlobalang<<4,viewingrangerecip) + mulscale27(cosglobalang,i);
	globalzd = (xdimscale<<9);
	globalzx = -dmulscale17(wx,globaly2,-wy,globalx2) + mulscale10(1-globalhoriz,globalzd);
	globalz = -dmulscale25(wx,globaly,-wy,globalx);

	if (globalorientation&64)  //Relative alignment
	{
		dx = mulscale14(wall[wal->point2].x-wal->x,dasqr);
		dy = mulscale14(wall[wal->point2].y-wal->y,dasqr);

		i = nsqrtasm(daslope*daslope+16777216);

		x = globalx; y = globaly;
		globalx = dmulscale16(x,dx,y,dy);
		globaly = mulscale12(dmulscale16(-y,dx,x,dy),i);

		x = ((wal->x-globalposx)<<8); y = ((wal->y-globalposy)<<8);
		globalx1 = dmulscale16(-x,dx,-y,dy);
		globaly1 = mulscale12(dmulscale16(-y,dx,x,dy),i);

		x = globalx2; y = globaly2;
		globalx2 = dmulscale16(x,dx,y,dy);
		globaly2 = mulscale12(dmulscale16(-y,dx,x,dy),i);
	}
	if (globalorientation&0x4)
	{
		i = globalx; globalx = -globaly; globaly = -i;
		i = globalx1; globalx1 = globaly1; globaly1 = i;
		i = globalx2; globalx2 = -globaly2; globaly2 = -i;
	}
	if (globalorientation&0x10) { globalx1 = -globalx1, globalx2 = -globalx2, globalx = -globalx; }
	if (globalorientation&0x20) { globaly1 = -globaly1, globaly2 = -globaly2, globaly = -globaly; }

	daz = dmulscale9(wx,globalposy-wal->y,-wy,globalposx-wal->x) + ((daz-globalposz)<<8);
	globalx2 = mulscale20(globalx2,daz); globalx = mulscale28(globalx,daz);
	globaly2 = mulscale20(globaly2,-daz); globaly = mulscale28(globaly,-daz);

	i = 8-(picsiz[globalpicnum]&15); j = 8-(picsiz[globalpicnum]>>4);
	if (globalorientation&8) { i++; j++; }
	globalx1 <<= (i+12); globalx2 <<= i; globalx <<= i;
	globaly1 <<= (j+12); globaly2 <<= j; globaly <<= j;

	if (dastat == 0)
	{
		globalx1 += (((int)sec->ceilingxpanning)<<24);
		globaly1 += (((int)sec->ceilingypanning)<<24);
	}
	else
	{
		globalx1 += (((int)sec->floorxpanning)<<24);
		globaly1 += (((int)sec->floorypanning)<<24);
	}

	asm1 = -(globalzd>>(16-BITSOFPRECISION));

	globvis = globalvisibility;
	if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
	globvis = mulscale13(globvis,daz);
	globvis = mulscale16(globvis,xdimscale);

	setupslopevlin(((int)(picsiz[globalpicnum]&15))+(((int)(picsiz[globalpicnum]>>4))<<8),(void *)waloff[globalpicnum],-ylookup[1]);

	l = (globalzd>>16);

	assert(SLOPALOOKUPSIZ - 4 - ydimen > 0);

	shinc = mulscale16(globalz,xdimenscale);
	if (shinc > 0) shoffs = (4<<15); else shoffs = ((SLOPALOOKUPSIZ-4-ydimen)<<15);
	if (dastat == 0) y1 = umost[dax1]; else y1 = max(umost[dax1],dplc[dax1]);
	m1 = mulscale16(y1,globalzd) + (globalzx>>6);
		//Avoid visibility overflow by crossing horizon
	if (globalzd > 0) m1 += (globalzd>>16); else m1 -= (globalzd>>16);
	m2 = m1+l;
	mptr1 = &slopalookup[y1+(shoffs>>15)]; mptr2 = mptr1+1;

	assert(y1+(shoffs>>15) >= 0);
	assert(y1+(shoffs>>15) <= SLOPALOOKUPSIZ-2);

	for(x=dax1;x<=dax2;x++)
	{
		if (dastat == 0) { y1 = umost[x]; y2 = min(dmost[x],uplc[x])-1; }
		else { y1 = max(umost[x],dplc[x]); y2 = dmost[x]-1; }
		if (y1 <= y2)
		{
			assert(y1+(shoffs>>15) >= 0);
			assert(y1+(shoffs>>15) <= SLOPALOOKUPSIZ-1);
			assert(y2+(shoffs>>15) >= 0);
			assert(y2+(shoffs>>15) <= SLOPALOOKUPSIZ-1);

			nptr1 = &slopalookup[y1+(shoffs>>15)];
			nptr2 = &slopalookup[y2+(shoffs>>15)];
			while (nptr1 <= mptr1)
			{
				*mptr1-- = (intptr_t)palookup[globalpal] + (getpalookup((int)mulscale24(krecipasm(m1),globvis),globalshade)<<8);
				m1 -= l;
			}
			while (nptr2 >= mptr2)
			{
				*mptr2++ = (intptr_t)palookup[globalpal] + (getpalookup((int)mulscale24(krecipasm(m2),globvis),globalshade)<<8);
				m2 += l;
			}

			globalx3 = (globalx2>>10);
			globaly3 = (globaly2>>10);
			asm3 = mulscale16(y2,globalzd) + (globalzx>>6);
			slopevlin((void *)(ylookup[y2]+x+frameoffset),krecipasm((int)asm3>>3),nptr2,y2-y1+1,globalx1,globaly1);

			if ((x&15) == 0) faketimerhandler();
		}
		globalx2 += globalx;
		globaly2 += globaly;
		globalzx += globalz;
		shoffs += shinc;
	}
}


//
// parascan (internal)
//
static void parascan(int dax1, int dax2, int sectnum, unsigned char dastat, int bunch)
{
	sectortype *sec;
	int j, k, l, m, n, x, z, wallnum, nextsectnum, globalhorizbak;
	short *topptr, *botptr;

	(void)dax1; (void)dax2;

	sectnum = thesector[bunchfirst[bunch]]; sec = &sector[sectnum];

	globalhorizbak = globalhoriz;
	if (parallaxyscale != 65536)
		globalhoriz = mulscale16(globalhoriz-(ydimen>>1),parallaxyscale) + (ydimen>>1);
	globvis = globalpisibility;
	//globalorientation = 0L;
	if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));

	if (dastat == 0)
	{
		globalpal = sec->ceilingpal;
		globalpicnum = sec->ceilingpicnum;
		globalshade = (int)sec->ceilingshade;
		globalxpanning = (int)sec->ceilingxpanning;
		globalypanning = (int)sec->ceilingypanning;
		topptr = umost;
		botptr = uplc;
	}
	else
	{
		globalpal = sec->floorpal;
		globalpicnum = sec->floorpicnum;
		globalshade = (int)sec->floorshade;
		globalxpanning = (int)sec->floorxpanning;
		globalypanning = (int)sec->floorypanning;
		topptr = dplc;
		botptr = dmost;
	}

	if (palookup[globalpal] == 0) globalpal = 0;
	if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
	if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)sectnum);
	globalshiftval = (picsiz[globalpicnum]>>4);
	if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
	globalshiftval = 32-globalshiftval;
	globalzd = (((tilesizy[globalpicnum]>>1)+parallaxyoffs)<<globalshiftval)+(globalypanning<<24);
	globalyscale = (8<<(globalshiftval-19));
	//if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;

	k = 11 - (picsiz[globalpicnum]&15) - pskybits;
	x = -1;

	for(z=bunchfirst[bunch];z>=0;z=p2[z])
	{
		wallnum = thewall[z]; nextsectnum = wall[wallnum].nextsector;

		if (dastat == 0) j = sector[nextsectnum].ceilingstat;
						else j = sector[nextsectnum].floorstat;

		if ((nextsectnum < 0) || (wall[wallnum].cstat&32) || ((j&1) == 0))
		{
			if (x == -1) x = xb1[z];

			if (parallaxtype == 0)
			{
				n = mulscale16(xdimenrecip,viewingrange);
				for(j=xb1[z];j<=xb2[z];j++)
					lplc[j] = (((mulscale23(j-halfxdimen,n)+globalang)&2047)>>k);
			}
			else
			{
				for(j=xb1[z];j<=xb2[z];j++)
					lplc[j] = ((((int)radarang2[j]+globalang)&2047)>>k);
			}
			if (parallaxtype == 2)
			{
				n = mulscale16(xdimscale,viewingrange);
				for(j=xb1[z];j<=xb2[z];j++)
					swplc[j] = mulscale14(sintable[((int)radarang2[j]+512)&2047],n);
			}
			else
				clearbuf(&swplc[xb1[z]],xb2[z]-xb1[z]+1,mulscale16(xdimscale,viewingrange));
		}
		else if (x >= 0)
		{
			l = globalpicnum; m = (picsiz[globalpicnum]&15);
			globalpicnum = l+pskyoff[lplc[x]>>m];

			if (((lplc[x]^lplc[xb1[z]-1])>>m) == 0)
				wallscan(x,xb1[z]-1,topptr,botptr,swplc,lplc);
			else
			{
				j = x;
				while (x < xb1[z])
				{
					n = l+pskyoff[lplc[x]>>m];
					if (n != globalpicnum)
					{
						wallscan(j,x-1,topptr,botptr,swplc,lplc);
						j = x;
						globalpicnum = n;
					}
					x++;
				}
				if (j < x)
					wallscan(j,x-1,topptr,botptr,swplc,lplc);
			}

			globalpicnum = l;
			x = -1;
		}
	}

	if (x >= 0)
	{
		l = globalpicnum; m = (picsiz[globalpicnum]&15);
		globalpicnum = l+pskyoff[lplc[x]>>m];

		if (((lplc[x]^lplc[xb2[bunchlast[bunch]]])>>m) == 0)
			wallscan(x,xb2[bunchlast[bunch]],topptr,botptr,swplc,lplc);
		else
		{
			j = x;
			while (x <= xb2[bunchlast[bunch]])
			{
				n = l+pskyoff[lplc[x]>>m];
				if (n != globalpicnum)
				{
					wallscan(j,x-1,topptr,botptr,swplc,lplc);
					j = x;
					globalpicnum = n;
				}
				x++;
			}
			if (j <= x)
				wallscan(j,x,topptr,botptr,swplc,lplc);
		}
		globalpicnum = l;
	}
	globalhoriz = globalhorizbak;
}


//
// drawalls (internal)
//
static void drawalls(int bunch)
{
	sectortype *sec, *nextsec;
	walltype *wal;
	int i, x, x1, x2, cz[5], fz[5];
	int z, wallnum, sectnum, nextsectnum;
	int startsmostwallcnt, startsmostcnt, gotswall;
	unsigned char andwstat1, andwstat2;

	z = bunchfirst[bunch];
	sectnum = thesector[z]; sec = &sector[sectnum];

	andwstat1 = 0xff; andwstat2 = 0xff;
	for(;z>=0;z=p2[z])  //uplc/dplc calculation
	{
		andwstat1 &= wallmost(uplc,z,sectnum,(char)0);
		andwstat2 &= wallmost(dplc,z,sectnum,(char)1);
	}

	if ((andwstat1&3) != 3)     //draw ceilings
	{
		if ((sec->ceilingstat&3) == 2)
			grouscan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum,0);
		else if ((sec->ceilingstat&1) == 0)
			ceilscan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum);
		else
			parascan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum,0,bunch);
	}
	if ((andwstat2&12) != 12)   //draw floors
	{
		if ((sec->floorstat&3) == 2)
			grouscan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum,1);
		else if ((sec->floorstat&1) == 0)
			florscan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum);
		else
			parascan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum,1,bunch);
	}

		//DRAW WALLS SECTION!
	for(z=bunchfirst[bunch];z>=0;z=p2[z])
	{
		x1 = xb1[z]; x2 = xb2[z];
		if (umost[x2] >= dmost[x2])
		{
			for(x=x1;x<x2;x++)
				if (umost[x] < dmost[x]) break;
			if (x >= x2)
			{
				smostwall[smostwallcnt] = z;
				smostwalltype[smostwallcnt] = 0;
				smostwallcnt++;
				continue;
			}
		}

		wallnum = thewall[z]; wal = &wall[wallnum];
		nextsectnum = wal->nextsector; nextsec = &sector[nextsectnum];

		gotswall = 0;

		startsmostwallcnt = smostwallcnt;
		startsmostcnt = smostcnt;

		if ((searchit == 2) && (searchx >= x1) && (searchx <= x2))
		{
			if (searchy <= uplc[searchx]) //ceiling
			{
				searchsector = sectnum; searchwall = wallnum;
				searchstat = 1; searchit = 1;
			}
			else if (searchy >= dplc[searchx]) //floor
			{
				searchsector = sectnum; searchwall = wallnum;
				searchstat = 2; searchit = 1;
			}
		}

		if (nextsectnum >= 0)
		{
			getzsofslope((short)sectnum,wal->x,wal->y,&cz[0],&fz[0]);
			getzsofslope((short)sectnum,wall[wal->point2].x,wall[wal->point2].y,&cz[1],&fz[1]);
			getzsofslope((short)nextsectnum,wal->x,wal->y,&cz[2],&fz[2]);
			getzsofslope((short)nextsectnum,wall[wal->point2].x,wall[wal->point2].y,&cz[3],&fz[3]);
			getzsofslope((short)nextsectnum,globalposx,globalposy,&cz[4],&fz[4]);

			if ((wal->cstat&48) == 16) maskwall[maskwallcnt++] = z;

			if (((sec->ceilingstat&1) == 0) || ((nextsec->ceilingstat&1) == 0))
			{
				if ((cz[2] <= cz[0]) && (cz[3] <= cz[1]))
				{
					if (globparaceilclip)
						for(x=x1;x<=x2;x++)
							if (uplc[x] > umost[x])
								if (umost[x] <= dmost[x])
								{
									umost[x] = uplc[x];
									if (umost[x] > dmost[x]) numhits--;
								}
				}
				else
				{
					wallmost(dwall,z,nextsectnum,(char)0);
					if ((cz[2] > fz[0]) || (cz[3] > fz[1]))
						for(i=x1;i<=x2;i++) if (dwall[i] > dplc[i]) dwall[i] = dplc[i];

					if ((searchit == 2) && (searchx >= x1) && (searchx <= x2))
						if (searchy <= dwall[searchx]) //wall
						{
							searchsector = sectnum; searchwall = wallnum;
							searchstat = 0; searchit = 1;
						}

					globalorientation = (int)wal->cstat;
					globalpicnum = wal->picnum;
					if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
					globalxpanning = (int)wal->xpanning;
					globalypanning = (int)wal->ypanning;
					globalshiftval = (picsiz[globalpicnum]>>4);
					if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
					globalshiftval = 32-globalshiftval;
					if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)wallnum+16384);
					globalshade = (int)wal->shade;
					globvis = globalvisibility;
					if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
					globalpal = (int)wal->pal;
					if (palookup[globalpal] == 0) globalpal = 0;	// JBF: fixes crash
					globalyscale = (wal->yrepeat<<(globalshiftval-19));
					if ((globalorientation&4) == 0)
						globalzd = (((globalposz-nextsec->ceilingz)*globalyscale)<<8);
					else
						globalzd = (((globalposz-sec->ceilingz)*globalyscale)<<8);
					globalzd += (globalypanning<<24);
					if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;

					if (gotswall == 0) { gotswall = 1; prepwall(z,wal); }
					wallscan(x1,x2,uplc,dwall,swall,lwall);

					if ((cz[2] >= cz[0]) && (cz[3] >= cz[1]))
					{
						for(x=x1;x<=x2;x++)
							if (dwall[x] > umost[x])
								if (umost[x] <= dmost[x])
								{
									umost[x] = dwall[x];
									if (umost[x] > dmost[x]) numhits--;
								}
					}
					else
					{
						for(x=x1;x<=x2;x++)
							if (umost[x] <= dmost[x])
							{
								i = max(uplc[x],dwall[x]);
								if (i > umost[x])
								{
									umost[x] = i;
									if (umost[x] > dmost[x]) numhits--;
								}
							}
					}
				}
				if ((cz[2] < cz[0]) || (cz[3] < cz[1]) || (globalposz < cz[4]))
				{
					i = x2-x1+1;
					if (smostcnt+i < MAXYSAVES)
					{
						smoststart[smostwallcnt] = smostcnt;
						smostwall[smostwallcnt] = z;
						smostwalltype[smostwallcnt] = 1;   //1 for umost
						smostwallcnt++;
						copybufbyte(&umost[x1],&smost[smostcnt],i*sizeof(smost[0]));
						smostcnt += i;
					}
				}
			}
			if (((sec->floorstat&1) == 0) || ((nextsec->floorstat&1) == 0))
			{
				if ((fz[2] >= fz[0]) && (fz[3] >= fz[1]))
				{
					if (globparaflorclip)
						for(x=x1;x<=x2;x++)
							if (dplc[x] < dmost[x])
								if (umost[x] <= dmost[x])
								{
									dmost[x] = dplc[x];
									if (umost[x] > dmost[x]) numhits--;
								}
				}
				else
				{
					wallmost(uwall,z,nextsectnum,(char)1);
					if ((fz[2] < cz[0]) || (fz[3] < cz[1]))
						for(i=x1;i<=x2;i++) if (uwall[i] < uplc[i]) uwall[i] = uplc[i];

					if ((searchit == 2) && (searchx >= x1) && (searchx <= x2))
						if (searchy >= uwall[searchx]) //wall
						{
							searchsector = sectnum; searchwall = wallnum;
							if ((wal->cstat&2) > 0) searchwall = wal->nextwall;
							searchstat = 0; searchit = 1;
						}

					if ((wal->cstat&2) > 0)
					{
						wallnum = wal->nextwall; wal = &wall[wallnum];
						globalorientation = (int)wal->cstat;
						globalpicnum = wal->picnum;
						if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
						globalxpanning = (int)wal->xpanning;
						globalypanning = (int)wal->ypanning;
						if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)wallnum+16384);
						globalshade = (int)wal->shade;
						globalpal = (int)wal->pal;
						wallnum = thewall[z]; wal = &wall[wallnum];
					}
					else
					{
						globalorientation = (int)wal->cstat;
						globalpicnum = wal->picnum;
						if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
						globalxpanning = (int)wal->xpanning;
						globalypanning = (int)wal->ypanning;
						if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)wallnum+16384);
						globalshade = (int)wal->shade;
						globalpal = (int)wal->pal;
					}
					if (palookup[globalpal] == 0) globalpal = 0;	// JBF: fixes crash
					globvis = globalvisibility;
					if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
					globalshiftval = (picsiz[globalpicnum]>>4);
					if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
					globalshiftval = 32-globalshiftval;
					globalyscale = (wal->yrepeat<<(globalshiftval-19));
					if ((globalorientation&4) == 0)
						globalzd = (((globalposz-nextsec->floorz)*globalyscale)<<8);
					else
						globalzd = (((globalposz-sec->ceilingz)*globalyscale)<<8);
					globalzd += (globalypanning<<24);
					if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;

					if (gotswall == 0) { gotswall = 1; prepwall(z,wal); }
					wallscan(x1,x2,uwall,dplc,swall,lwall);

					if ((fz[2] <= fz[0]) && (fz[3] <= fz[1]))
					{
						for(x=x1;x<=x2;x++)
							if (uwall[x] < dmost[x])
								if (umost[x] <= dmost[x])
								{
									dmost[x] = uwall[x];
									if (umost[x] > dmost[x]) numhits--;
								}
					}
					else
					{
						for(x=x1;x<=x2;x++)
							if (umost[x] <= dmost[x])
							{
								i = min(dplc[x],uwall[x]);
								if (i < dmost[x])
								{
									dmost[x] = i;
									if (umost[x] > dmost[x]) numhits--;
								}
							}
					}
				}
				if ((fz[2] > fz[0]) || (fz[3] > fz[1]) || (globalposz > fz[4]))
				{
					i = x2-x1+1;
					if (smostcnt+i < MAXYSAVES)
					{
						smoststart[smostwallcnt] = smostcnt;
						smostwall[smostwallcnt] = z;
						smostwalltype[smostwallcnt] = 2;   //2 for dmost
						smostwallcnt++;
						copybufbyte(&dmost[x1],&smost[smostcnt],i*sizeof(smost[0]));
						smostcnt += i;
					}
				}
			}
			if (numhits < 0) return;
			if ((!(wal->cstat&32)) && ((gotsector[nextsectnum>>3]&pow2char[nextsectnum&7]) == 0))
			{
				if (umost[x2] < dmost[x2])
					scansector(nextsectnum);
				else
				{
					for(x=x1;x<x2;x++)
						if (umost[x] < dmost[x])
							{ scansector(nextsectnum); break; }

						//If can't see sector beyond, then cancel smost array and just
						//store wall!
					if (x == x2)
					{
						smostwallcnt = startsmostwallcnt;
						smostcnt = startsmostcnt;
						smostwall[smostwallcnt] = z;
						smostwalltype[smostwallcnt] = 0;
						smostwallcnt++;
					}
				}
			}
		}
		if ((nextsectnum < 0) || (wal->cstat&32))   //White/1-way wall
		{
			globalorientation = (int)wal->cstat;
			if (nextsectnum < 0) globalpicnum = wal->picnum;
								  else globalpicnum = wal->overpicnum;
			if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
			globalxpanning = (int)wal->xpanning;
			globalypanning = (int)wal->ypanning;
			if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)wallnum+16384);
			globalshade = (int)wal->shade;
			globvis = globalvisibility;
			if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
			globalpal = (int)wal->pal;
			if (palookup[globalpal] == 0) globalpal = 0;	// JBF: fixes crash
			globalshiftval = (picsiz[globalpicnum]>>4);
			if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
			globalshiftval = 32-globalshiftval;
			globalyscale = (wal->yrepeat<<(globalshiftval-19));
			if (nextsectnum >= 0)
			{
				if ((globalorientation&4) == 0) globalzd = globalposz-nextsec->ceilingz;
													else globalzd = globalposz-sec->ceilingz;
			}
			else
			{
				if ((globalorientation&4) == 0) globalzd = globalposz-sec->ceilingz;
													else globalzd = globalposz-sec->floorz;
			}
			globalzd = ((globalzd*globalyscale)<<8) + (globalypanning<<24);
			if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;

			if (gotswall == 0) { gotswall = 1; prepwall(z,wal); }
			wallscan(x1,x2,uplc,dplc,swall,lwall);

			for(x=x1;x<=x2;x++)
				if (umost[x] <= dmost[x])
					{ umost[x] = 1; dmost[x] = 0; numhits--; }
			smostwall[smostwallcnt] = z;
			smostwalltype[smostwallcnt] = 0;
			smostwallcnt++;

			if ((searchit == 2) && (searchx >= x1) && (searchx <= x2))
			{
				searchit = 1; searchsector = sectnum; searchwall = wallnum;
				if (nextsectnum < 0) searchstat = 0; else searchstat = 4;
			}
		}
	}
}


//
// drawvox
//
static void drawvox(int dasprx, int daspry, int dasprz, int dasprang,
		  int daxscale, int dayscale, unsigned char daindex,
		  signed char dashade, unsigned char dapal, int *daumost, int *dadmost)
{
	int i, j, k, x, y, syoff, ggxstart, ggystart, nxoff;
	int cosang, sinang, sprcosang, sprsinang, backx, backy, gxinc, gyinc;
	int daxsiz, daysiz, dazsiz, daxpivot, daypivot, dazpivot;
	int daxscalerecip, dayscalerecip, cnt, gxstart, gystart, odayscale;
	int l1, l2, xyvoxoffs, *longptr;
	intptr_t slabxoffs;
	int lx, rx, nx, ny, x1=0, y1=0, z1, x2=0, y2=0, z2, yplc, yinc=0;
	int yoff, xs=0, ys=0, xe, ye, xi=0, yi=0, cbackx, cbacky, dagxinc, dagyinc;
	short *shortptr;
	unsigned char *voxptr, *voxend, *davoxptr, oand, oand16, oand32;

	(void)dazsiz;

	cosang = sintable[(globalang+512)&2047];
	sinang = sintable[globalang&2047];
	sprcosang = sintable[(dasprang+512)&2047];
	sprsinang = sintable[dasprang&2047];

	i = klabs(dmulscale6(dasprx-globalposx,cosang,daspry-globalposy,sinang));
	j = (int)(getpalookup((int)mulscale21(globvis,i),(int)dashade)<<8);
	setupdrawslab(ylookup[1], palookup[dapal]+j);
	j = 1310720;
	j *= min(daxscale,dayscale); j >>= 6;  //New hacks (for sized-down voxels)
	for(k=0;k<MAXVOXMIPS;k++)
	{
		if (i < j) { i = k; break; }
		j <<= 1;
	}
	if (k >= MAXVOXMIPS) i = MAXVOXMIPS-1;

	if (novoxmips) i = 0;
	davoxptr = (unsigned char *)voxoff[daindex][i];
	if (!davoxptr && i > 0) { davoxptr = (unsigned char *)voxoff[daindex][0]; i = 0; }
	if (!davoxptr) return;

	if (voxscale[daindex] == 65536)
		{ daxscale <<= (i+8); dayscale <<= (i+8); }
	else
	{
		daxscale = mulscale8(daxscale<<i,voxscale[daindex]);
		dayscale = mulscale8(dayscale<<i,voxscale[daindex]);
	}

	odayscale = dayscale;
	daxscale = mulscale16(daxscale,xyaspect);
	daxscale = scale(daxscale,xdimenscale,xdimen<<8);
	dayscale = scale(dayscale,mulscale16(xdimenscale,viewingrangerecip),xdimen<<8);

	daxscalerecip = (1<<30)/daxscale;
	dayscalerecip = (1<<30)/dayscale;

	longptr = (int *)davoxptr;
	daxsiz = B_LITTLE32(longptr[0]); daysiz = B_LITTLE32(longptr[1]); dazsiz = B_LITTLE32(longptr[2]);
	daxpivot = B_LITTLE32(longptr[3]); daypivot = B_LITTLE32(longptr[4]); dazpivot = B_LITTLE32(longptr[5]);
	davoxptr += (6<<2);

	x = mulscale16(globalposx-dasprx,daxscalerecip);
	y = mulscale16(globalposy-daspry,daxscalerecip);
	backx = ((dmulscale10(x,sprcosang,y,sprsinang)+daxpivot)>>8);
	backy = ((dmulscale10(y,sprcosang,x,-sprsinang)+daypivot)>>8);
	cbackx = min(max(backx,0),daxsiz-1);
	cbacky = min(max(backy,0),daysiz-1);

	sprcosang = mulscale14(daxscale,sprcosang);
	sprsinang = mulscale14(daxscale,sprsinang);

	x = (dasprx-globalposx) - dmulscale18(daxpivot,sprcosang,daypivot,-sprsinang);
	y = (daspry-globalposy) - dmulscale18(daypivot,sprcosang,daxpivot,sprsinang);

	cosang = mulscale16(cosang,dayscalerecip);
	sinang = mulscale16(sinang,dayscalerecip);

	gxstart = y*cosang - x*sinang;
	gystart = x*cosang + y*sinang;
	gxinc = dmulscale10(sprsinang,cosang,sprcosang,-sinang);
	gyinc = dmulscale10(sprcosang,cosang,sprsinang,sinang);

	x = 0; y = 0; j = max(daxsiz,daysiz);
	for(i=0;i<=j;i++)
	{
		ggxinc[i] = x; x += gxinc;
		ggyinc[i] = y; y += gyinc;
	}

	if ((klabs(globalposz-dasprz)>>10) >= klabs(odayscale)) return;
	syoff = divscale21(globalposz-dasprz,odayscale) + (dazpivot<<7);
	yoff = ((klabs(gxinc)+klabs(gyinc))>>1);
	longptr = (int *)davoxptr;
	xyvoxoffs = ((daxsiz+1)<<2);

	begindrawing();	//{{{

	for(cnt=0;cnt<8;cnt++)
	{
		switch(cnt)
		{
			case 0: xs = 0;        ys = 0;        xi = 1;  yi = 1;  break;
			case 1: xs = daxsiz-1; ys = 0;        xi = -1; yi = 1;  break;
			case 2: xs = 0;        ys = daysiz-1; xi = 1;  yi = -1; break;
			case 3: xs = daxsiz-1; ys = daysiz-1; xi = -1; yi = -1; break;
			case 4: xs = 0;        ys = cbacky;   xi = 1;  yi = 2;  break;
			case 5: xs = daxsiz-1; ys = cbacky;   xi = -1; yi = 2;  break;
			case 6: xs = cbackx;   ys = 0;        xi = 2;  yi = 1;  break;
			case 7: xs = cbackx;   ys = daysiz-1; xi = 2;  yi = -1; break;
		}
		xe = cbackx; ye = cbacky;
		if (cnt < 4)
		{
			if ((xi < 0) && (xe >= xs)) continue;
			if ((xi > 0) && (xe <= xs)) continue;
			if ((yi < 0) && (ye >= ys)) continue;
			if ((yi > 0) && (ye <= ys)) continue;
		}
		else
		{
			if ((xi < 0) && (xe > xs)) continue;
			if ((xi > 0) && (xe < xs)) continue;
			if ((yi < 0) && (ye > ys)) continue;
			if ((yi > 0) && (ye < ys)) continue;
			xe += xi; ye += yi;
		}

		i = ksgn(ys-backy)+ksgn(xs-backx)*3+4;
		switch(i)
		{
			case 6: case 7: x1 = 0; y1 = 0; break;
			case 8: case 5: x1 = gxinc; y1 = gyinc; break;
			case 0: case 3: x1 = gyinc; y1 = -gxinc; break;
			case 2: case 1: x1 = gxinc+gyinc; y1 = gyinc-gxinc; break;
		}
		switch(i)
		{
			case 2: case 5: x2 = 0; y2 = 0; break;
			case 0: case 1: x2 = gxinc; y2 = gyinc; break;
			case 8: case 7: x2 = gyinc; y2 = -gxinc; break;
			case 6: case 3: x2 = gxinc+gyinc; y2 = gyinc-gxinc; break;
		}
		oand = pow2char[(xs<backx)+0]+pow2char[(ys<backy)+2];
		oand16 = oand+16;
		oand32 = oand+32;

		if (yi > 0) { dagxinc = gxinc; dagyinc = mulscale16(gyinc,viewingrangerecip); }
		else { dagxinc = -gxinc; dagyinc = -mulscale16(gyinc,viewingrangerecip); }

			//Fix for non 90 degree viewing ranges
		nxoff = mulscale16(x2-x1,viewingrangerecip);
		x1 = mulscale16(x1,viewingrangerecip);

		ggxstart = gxstart+ggyinc[ys];
		ggystart = gystart-ggxinc[ys];

		for(x=xs;x!=xe;x+=xi)
		{
			slabxoffs = (intptr_t)&davoxptr[B_LITTLE32(longptr[x])];
			shortptr = (short *)&davoxptr[((x*(daysiz+1))<<1)+xyvoxoffs];

			nx = mulscale16(ggxstart+ggxinc[x],viewingrangerecip)+x1;
			ny = ggystart+ggyinc[x];
			for(y=ys;y!=ye;y+=yi,nx+=dagyinc,ny-=dagxinc)
			{
				if ((ny <= nytooclose) || (ny >= nytoofar)) continue;
				voxptr = (unsigned char *)(B_LITTLE16(shortptr[y])+slabxoffs);
				voxend = (unsigned char *)(B_LITTLE16(shortptr[y+1])+slabxoffs);
				if (voxptr == voxend) continue;

				lx = mulscale32(nx>>3,distrecip[(ny+y1)>>14])+halfxdimen;
				if (lx < 0) lx = 0;
				rx = mulscale32((nx+nxoff)>>3,distrecip[(ny+y2)>>14])+halfxdimen;
				if (rx > xdimen) rx = xdimen;
				if (rx <= lx) continue;
				rx -= lx;

				l1 = distrecip[(ny-yoff)>>14];
				l2 = distrecip[(ny+yoff)>>14];
				for(;voxptr<voxend;voxptr+=voxptr[1]+3)
				{
					j = (voxptr[0]<<15)-syoff;
					if (j < 0)
					{
						k = j+(voxptr[1]<<15);
						if (k < 0)
						{
							if ((voxptr[2]&oand32) == 0) continue;
							z2 = mulscale32(l2,k) + globalhoriz;     //Below slab
						}
						else
						{
							if ((voxptr[2]&oand) == 0) continue;    //Middle of slab
							z2 = mulscale32(l1,k) + globalhoriz;
						}
						z1 = mulscale32(l1,j) + globalhoriz;
					}
					else
					{
						if ((voxptr[2]&oand16) == 0) continue;
						z1 = mulscale32(l2,j) + globalhoriz;        //Above slab
						z2 = mulscale32(l1,j+(voxptr[1]<<15)) + globalhoriz;
					}

					if (voxptr[1] == 1)
					{
						yplc = 0; yinc = 0;
						if (z1 < daumost[lx]) z1 = daumost[lx];
					}
					else
					{
						if (z2-z1 >= 1024) yinc = divscale16(voxptr[1],z2-z1);
						else if (z2 > z1) yinc = (lowrecip[z2-z1]*voxptr[1]>>8);
						if (z1 < daumost[lx]) { yplc = yinc*(daumost[lx]-z1); z1 = daumost[lx]; } else yplc = 0;
					}
					if (z2 > dadmost[lx]) z2 = dadmost[lx];
					z2 -= z1; if (z2 <= 0) continue;

					drawslab(rx,yplc,z2,yinc,&voxptr[3],(void *)(ylookup[z1]+lx+frameoffset));
				}
			}
		}
	}

	enddrawing();	//}}}
}


//
// drawsprite (internal)
//
static void drawsprite(int snum)
{
	spritetype *tspr;
	sectortype *sec;
	int startum, startdm, sectnum, xb, yp, cstat;
	int siz, xsiz, ysiz, xoff, yoff, xspan, yspan;
	int x1, y1, x2, y2, lx, rx, dalx2, darx2, i, j, k, x, linum, linuminc;
	int yinc, z, z1, z2, xp1, yp1, xp2, yp2;
	int xv, yv, top, topinc, bot, botinc, hplc, hinc;
	int cosang, sinang, dax, day, lpoint, lmax, rpoint, rmax, dax1, dax2, y;
	int npoints, npoints2, zz, t, zsgn, zzsgn, *longptr;
	int tilenum, vtilenum = 0, spritenum;
	unsigned char swapped, daclip;

	//============================================================================= //POLYMOST BEGINS
#if USE_POLYMOST
	if (rendmode) { polymost_drawsprite(snum); return; }
#endif
	//============================================================================= //POLYMOST ENDS

	tspr = tspriteptr[snum];

	xb = spritesx[snum];
	yp = spritesy[snum];
	tilenum = tspr->picnum;
	spritenum = tspr->owner;
	cstat = tspr->cstat;

	if ((cstat&48)==48) vtilenum = tilenum;	// if the game wants voxels, it gets voxels
	else if ((cstat&48)!=48 && (usevoxels) && (tiletovox[tilenum] != -1)
#if USE_POLYMOST && USE_OPENGL
		 && (!(spriteext[tspr->owner].flags&SPREXT_NOTMD))
#endif
	   ) {
		vtilenum = tiletovox[tilenum];
		cstat |= 48;
	}

	if ((cstat&48) != 48)
	{
		if (picanm[tilenum]&192) tilenum += animateoffs(tilenum,spritenum+32768);
		if ((tilesizx[tilenum] <= 0) || (tilesizy[tilenum] <= 0) || (spritenum < 0))
			return;
	}
	if ((tspr->xrepeat <= 0) || (tspr->yrepeat <= 0)) return;

	sectnum = tspr->sectnum; sec = &sector[sectnum];
	globalpal = tspr->pal;
	if (palookup[globalpal] == 0) globalpal = 0;	// JBF: fixes null-pointer crash
	globalshade = tspr->shade;
	if (cstat&2)
	{
		if (cstat&512) settransreverse(); else settransnormal();
	}

	xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)tspr->xoffset);
	yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)tspr->yoffset);

	if ((cstat&48) == 0)
	{
		if (yp <= (4<<8)) return;

		siz = divscale19(xdimenscale,yp);

		xv = mulscale16(((int)tspr->xrepeat)<<16,xyaspect);

		xspan = tilesizx[tilenum];
		yspan = tilesizy[tilenum];
		xsiz = mulscale30(siz,xv*xspan);
		ysiz = mulscale14(siz,tspr->yrepeat*yspan);

		if (((tilesizx[tilenum]>>11) >= xsiz) || (yspan >= (ysiz>>1)))
			return;  //Watch out for divscale overflow

		x1 = xb-(xsiz>>1);
		if (xspan&1) x1 += mulscale31(siz,xv);  //Odd xspans
		i = mulscale30(siz,xv*xoff);
		if ((cstat&4) == 0) x1 -= i; else x1 += i;

		y1 = mulscale16(tspr->z-globalposz,siz);
		y1 -= mulscale14(siz,tspr->yrepeat*yoff);
		y1 += (globalhoriz<<8)-ysiz;
		if (cstat&128)
		{
			y1 += (ysiz>>1);
			if (yspan&1) y1 += mulscale15(siz,tspr->yrepeat);  //Odd yspans
		}

		x2 = x1+xsiz-1;
		y2 = y1+ysiz-1;
		if ((y1|255) >= (y2|255)) return;

		lx = (x1>>8)+1; if (lx < 0) lx = 0;
		rx = (x2>>8); if (rx >= xdimen) rx = xdimen-1;
		if (lx > rx) return;

		yinc = divscale32(yspan,ysiz);

		if ((sec->ceilingstat&3) == 0)
			startum = globalhoriz+mulscale24(siz,sec->ceilingz-globalposz)-1;
		else
			startum = 0;
		if ((sec->floorstat&3) == 0)
			startdm = globalhoriz+mulscale24(siz,sec->floorz-globalposz)+1;
		else
			startdm = 0x7fffffff;
		if ((y1>>8) > startum) startum = (y1>>8);
		if ((y2>>8) < startdm) startdm = (y2>>8);

		if (startum < -32768) startum = -32768;
		if (startdm > 32767) startdm = 32767;
		if (startum >= startdm) return;

		if ((cstat&4) == 0)
		{
			linuminc = divscale24(xspan,xsiz);
			linum = mulscale8((lx<<8)-x1,linuminc);
		}
		else
		{
			linuminc = -divscale24(xspan,xsiz);
			linum = mulscale8((lx<<8)-x2,linuminc);
		}
		if ((cstat&8) > 0)
		{
			yinc = -yinc;
			i = y1; y1 = y2; y2 = i;
		}

		for(x=lx;x<=rx;x++)
		{
			uwall[x] = max(startumost[x+windowx1]-windowy1,(short)startum);
			dwall[x] = min(startdmost[x+windowx1]-windowy1,(short)startdm);
		}
		daclip = 0;
		for(i=smostwallcnt-1;i>=0;i--)
		{
			if (smostwalltype[i]&daclip) continue;
			j = smostwall[i];
			if ((xb1[j] > rx) || (xb2[j] < lx)) continue;
			if ((yp <= yb1[j]) && (yp <= yb2[j])) continue;
			if (spritewallfront(tspr,(int)thewall[j]) && ((yp <= yb1[j]) || (yp <= yb2[j]))) continue;

			dalx2 = max(xb1[j],lx); darx2 = min(xb2[j],rx);

			switch(smostwalltype[i])
			{
				case 0:
					if (dalx2 <= darx2)
					{
						if ((dalx2 == lx) && (darx2 == rx)) return;
						//clearbufbyte(&dwall[dalx2],(darx2-dalx2+1)*sizeof(dwall[0]),0L);
						for (k=dalx2; k<=darx2; k++) dwall[k] = 0;
					}
					break;
				case 1:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] > uwall[x]) uwall[x] = smost[k+x];
					if ((dalx2 == lx) && (darx2 == rx)) daclip |= 1;
					break;
				case 2:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] < dwall[x]) dwall[x] = smost[k+x];
					if ((dalx2 == lx) && (darx2 == rx)) daclip |= 2;
					break;
			}
		}

		if (uwall[rx] >= dwall[rx])
		{
			for(x=lx;x<rx;x++)
				if (uwall[x] < dwall[x]) break;
			if (x == rx) return;
		}

			//sprite
		if ((searchit >= 1) && (searchx >= lx) && (searchx <= rx))
			if ((searchy >= uwall[searchx]) && (searchy < dwall[searchx]))
			{
				searchsector = sectnum; searchwall = spritenum;
				searchstat = 3; searchit = 1;
			}

		z2 = tspr->z - ((yoff*tspr->yrepeat)<<2);
		if (cstat&128)
		{
			z2 += ((yspan*tspr->yrepeat)<<1);
			if (yspan&1) z2 += (tspr->yrepeat<<1);        //Odd yspans
		}
		z1 = z2 - ((yspan*tspr->yrepeat)<<2);

		globalorientation = 0;
		globalpicnum = tilenum;
		if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
		globalxpanning = 0L;
		globalypanning = 0L;
		globvis = globalvisibility;
		if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
		globalshiftval = (picsiz[globalpicnum]>>4);
		if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
		globalshiftval = 32-globalshiftval;
		globalyscale = divscale(512,tspr->yrepeat,globalshiftval-19);
		globalzd = (((globalposz-z1)*globalyscale)<<8);
		if ((cstat&8) > 0)
		{
			globalyscale = -globalyscale;
			globalzd = (((globalposz-z2)*globalyscale)<<8);
		}

		qinterpolatedown16(&lwall[lx],rx-lx+1,linum,linuminc);
		clearbuf(&swall[lx],rx-lx+1,mulscale19(yp,xdimscale));

		if ((cstat&2) == 0)
			maskwallscan(lx,rx,uwall,dwall,swall,lwall);
		else
			transmaskwallscan(lx,rx);
	}
	else if ((cstat&48) == 16)
	{
		if ((cstat&4) > 0) xoff = -xoff;
		if ((cstat&8) > 0) yoff = -yoff;

		xspan = tilesizx[tilenum]; yspan = tilesizy[tilenum];
		xv = tspr->xrepeat*sintable[(tspr->ang+2560+1536)&2047];
		yv = tspr->xrepeat*sintable[(tspr->ang+2048+1536)&2047];
		i = (xspan>>1)+xoff;
		x1 = tspr->x-globalposx-mulscale16(xv,i); x2 = x1+mulscale16(xv,xspan);
		y1 = tspr->y-globalposy-mulscale16(yv,i); y2 = y1+mulscale16(yv,xspan);

		yp1 = dmulscale6(x1,cosviewingrangeglobalang,y1,sinviewingrangeglobalang);
		yp2 = dmulscale6(x2,cosviewingrangeglobalang,y2,sinviewingrangeglobalang);
		if ((yp1 <= 0) && (yp2 <= 0)) return;
		xp1 = dmulscale6(y1,cosglobalang,-x1,singlobalang);
		xp2 = dmulscale6(y2,cosglobalang,-x2,singlobalang);

		x1 += globalposx; y1 += globalposy;
		x2 += globalposx; y2 += globalposy;

		swapped = 0;
		if (dmulscale32(xp1,yp2,-xp2,yp1) >= 0)  //If wall's NOT facing you
		{
			if ((cstat&64) != 0) return;
			i = xp1, xp1 = xp2, xp2 = i;
			i = yp1, yp1 = yp2, yp2 = i;
			i = x1, x1 = x2, x2 = i;
			i = y1, y1 = y2, y2 = i;
			swapped = 1;
		}

		if (xp1 >= -yp1)
		{
			if (xp1 > yp1) return;

			if (yp1 == 0) return;
			xb1[MAXWALLSB-1] = halfxdimen + scale(xp1,halfxdimen,yp1);
			if (xp1 >= 0) xb1[MAXWALLSB-1]++;   //Fix for SIGNED divide
			if (xb1[MAXWALLSB-1] >= xdimen) xb1[MAXWALLSB-1] = xdimen-1;
			yb1[MAXWALLSB-1] = yp1;
		}
		else
		{
			if (xp2 < -yp2) return;
			xb1[MAXWALLSB-1] = 0;
			i = yp1-yp2+xp1-xp2;
			if (i == 0) return;
			yb1[MAXWALLSB-1] = yp1 + scale(yp2-yp1,xp1+yp1,i);
		}
		if (xp2 <= yp2)
		{
			if (xp2 < -yp2) return;

			if (yp2 == 0) return;
			xb2[MAXWALLSB-1] = halfxdimen + scale(xp2,halfxdimen,yp2) - 1;
			if (xp2 >= 0) xb2[MAXWALLSB-1]++;   //Fix for SIGNED divide
			if (xb2[MAXWALLSB-1] >= xdimen) xb2[MAXWALLSB-1] = xdimen-1;
			yb2[MAXWALLSB-1] = yp2;
		}
		else
		{
			if (xp1 > yp1) return;

			xb2[MAXWALLSB-1] = xdimen-1;
			i = xp2-xp1+yp1-yp2;
			if (i == 0) return;
			yb2[MAXWALLSB-1] = yp1 + scale(yp2-yp1,yp1-xp1,i);
		}

		if ((yb1[MAXWALLSB-1] < 256) || (yb2[MAXWALLSB-1] < 256) || (xb1[MAXWALLSB-1] > xb2[MAXWALLSB-1]))
			return;

		topinc = -mulscale10(yp1,xspan);
		top = (((mulscale10(xp1,xdimen) - mulscale9(xb1[MAXWALLSB-1]-halfxdimen,yp1))*xspan)>>3);
		botinc = ((yp2-yp1)>>8);
		bot = mulscale11(xp1-xp2,xdimen) + mulscale2(xb1[MAXWALLSB-1]-halfxdimen,botinc);

		j = xb2[MAXWALLSB-1]+3;
		z = mulscale20(top,krecipasm(bot));
		lwall[xb1[MAXWALLSB-1]] = (z>>8);
		for(x=xb1[MAXWALLSB-1]+4;x<=j;x+=4)
		{
			top += topinc; bot += botinc;
			zz = z; z = mulscale20(top,krecipasm(bot));
			lwall[x] = (z>>8);
			i = ((z+zz)>>1);
			lwall[x-2] = (i>>8);
			lwall[x-3] = ((i+zz)>>9);
			lwall[x-1] = ((i+z)>>9);
		}

		if (lwall[xb1[MAXWALLSB-1]] < 0) lwall[xb1[MAXWALLSB-1]] = 0;
		if (lwall[xb2[MAXWALLSB-1]] >= xspan) lwall[xb2[MAXWALLSB-1]] = xspan-1;

		if ((swapped^((cstat&4)>0)) > 0)
		{
			j = xspan-1;
			for(x=xb1[MAXWALLSB-1];x<=xb2[MAXWALLSB-1];x++)
				lwall[x] = j-lwall[x];
		}

		rx1[MAXWALLSB-1] = xp1; ry1[MAXWALLSB-1] = yp1;
		rx2[MAXWALLSB-1] = xp2; ry2[MAXWALLSB-1] = yp2;

		hplc = divscale19(xdimenscale,yb1[MAXWALLSB-1]);
		hinc = divscale19(xdimenscale,yb2[MAXWALLSB-1]);
		hinc = (hinc-hplc)/(xb2[MAXWALLSB-1]-xb1[MAXWALLSB-1]+1);

		z2 = tspr->z - ((yoff*tspr->yrepeat)<<2);
		if (cstat&128)
		{
			z2 += ((yspan*tspr->yrepeat)<<1);
			if (yspan&1) z2 += (tspr->yrepeat<<1);        //Odd yspans
		}
		z1 = z2 - ((yspan*tspr->yrepeat)<<2);

		globalorientation = 0;
		globalpicnum = tilenum;
		if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
		globalxpanning = 0L;
		globalypanning = 0L;
		globvis = globalvisibility;
		if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
		globalshiftval = (picsiz[globalpicnum]>>4);
		if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
		globalshiftval = 32-globalshiftval;
		globalyscale = divscale(512,tspr->yrepeat,globalshiftval-19);
		globalzd = (((globalposz-z1)*globalyscale)<<8);
		if ((cstat&8) > 0)
		{
			globalyscale = -globalyscale;
			globalzd = (((globalposz-z2)*globalyscale)<<8);
		}

		if (((sec->ceilingstat&1) == 0) && (z1 < sec->ceilingz))
			z1 = sec->ceilingz;
		if (((sec->floorstat&1) == 0) && (z2 > sec->floorz))
			z2 = sec->floorz;

		owallmost(uwall,(int)(MAXWALLSB-1),z1-globalposz);
		owallmost(dwall,(int)(MAXWALLSB-1),z2-globalposz);
		for(i=xb1[MAXWALLSB-1];i<=xb2[MAXWALLSB-1];i++)
			{ swall[i] = (krecipasm(hplc)<<2); hplc += hinc; }

		for(i=smostwallcnt-1;i>=0;i--)
		{
			j = smostwall[i];

			if ((xb1[j] > xb2[MAXWALLSB-1]) || (xb2[j] < xb1[MAXWALLSB-1])) continue;

			dalx2 = xb1[j]; darx2 = xb2[j];
			if (max(yb1[MAXWALLSB-1],yb2[MAXWALLSB-1]) > min(yb1[j],yb2[j]))
			{
				if (min(yb1[MAXWALLSB-1],yb2[MAXWALLSB-1]) > max(yb1[j],yb2[j]))
				{
					x = 0x80000000;
				}
				else
				{
					x = thewall[j]; xp1 = wall[x].x; yp1 = wall[x].y;
					x = wall[x].point2; xp2 = wall[x].x; yp2 = wall[x].y;

					z1 = (xp2-xp1)*(y1-yp1) - (yp2-yp1)*(x1-xp1);
					z2 = (xp2-xp1)*(y2-yp1) - (yp2-yp1)*(x2-xp1);
					if ((z1^z2) >= 0)
						x = (z1+z2);
					else
					{
						z1 = (x2-x1)*(yp1-y1) - (y2-y1)*(xp1-x1);
						z2 = (x2-x1)*(yp2-y1) - (y2-y1)*(xp2-x1);

						if ((z1^z2) >= 0)
							x = -(z1+z2);
						else
						{
							if ((xp2-xp1)*(tspr->y-yp1) == (tspr->x-xp1)*(yp2-yp1))
							{
								if (wall[thewall[j]].nextsector == tspr->sectnum)
									x = 0x80000000;
								else
									x = 0x7fffffff;
							}
							else
							{     //INTERSECTION!
								x = (xp1-globalposx) + scale(xp2-xp1,z1,z1-z2);
								y = (yp1-globalposy) + scale(yp2-yp1,z1,z1-z2);

								yp1 = dmulscale14(x,cosglobalang,y,singlobalang);
								if (yp1 > 0)
								{
									xp1 = dmulscale14(y,cosglobalang,-x,singlobalang);

									x = halfxdimen + scale(xp1,halfxdimen,yp1);
									if (xp1 >= 0) x++;   //Fix for SIGNED divide

									if (z1 < 0)
										{ if (dalx2 < x) dalx2 = x; }
									else
										{ if (darx2 > x) darx2 = x; }
									x = 0x80000001;
								}
								else
									x = 0x7fffffff;
							}
						}
					}
				}
				if (x < 0)
				{
					if (dalx2 < xb1[MAXWALLSB-1]) dalx2 = xb1[MAXWALLSB-1];
					if (darx2 > xb2[MAXWALLSB-1]) darx2 = xb2[MAXWALLSB-1];
					switch(smostwalltype[i])
					{
						case 0:
							if (dalx2 <= darx2)
							{
								if ((dalx2 == xb1[MAXWALLSB-1]) && (darx2 == xb2[MAXWALLSB-1])) return;
								//clearbufbyte(&dwall[dalx2],(darx2-dalx2+1)*sizeof(dwall[0]),0L);
								for (k=dalx2; k<=darx2; k++) dwall[k] = 0;
							}
							break;
						case 1:
							k = smoststart[i] - xb1[j];
							for(x=dalx2;x<=darx2;x++)
								if (smost[k+x] > uwall[x]) uwall[x] = smost[k+x];
							break;
						case 2:
							k = smoststart[i] - xb1[j];
							for(x=dalx2;x<=darx2;x++)
								if (smost[k+x] < dwall[x]) dwall[x] = smost[k+x];
							break;
					}
				}
			}
		}

			//sprite
		if ((searchit >= 1) && (searchx >= xb1[MAXWALLSB-1]) && (searchx <= xb2[MAXWALLSB-1]))
			if ((searchy >= uwall[searchx]) && (searchy <= dwall[searchx]))
			{
				searchsector = sectnum; searchwall = spritenum;
				searchstat = 3; searchit = 1;
			}

		if ((cstat&2) == 0) {
			maskwallscan(xb1[MAXWALLSB-1],xb2[MAXWALLSB-1],uwall,dwall,swall,lwall);
		} else {
			transmaskwallscan(xb1[MAXWALLSB-1],xb2[MAXWALLSB-1]);
		}
	}
	else if ((cstat&48) == 32)
	{
		if ((cstat&64) != 0)
			if ((globalposz > tspr->z) == ((cstat&8)==0))
				return;

		if ((cstat&4) > 0) xoff = -xoff;
		if ((cstat&8) > 0) yoff = -yoff;
		xspan = tilesizx[tilenum];
		yspan = tilesizy[tilenum];

			//Rotate center point
		dax = tspr->x-globalposx;
		day = tspr->y-globalposy;
		rzi[0] = dmulscale10(cosglobalang,dax,singlobalang,day);
		rxi[0] = dmulscale10(cosglobalang,day,-singlobalang,dax);

			//Get top-left corner
		i = ((tspr->ang+2048-globalang)&2047);
		cosang = sintable[(i+512)&2047]; sinang = sintable[i];
		dax = ((xspan>>1)+xoff)*tspr->xrepeat;
		day = ((yspan>>1)+yoff)*tspr->yrepeat;
		rzi[0] += dmulscale12(sinang,dax,cosang,day);
		rxi[0] += dmulscale12(sinang,day,-cosang,dax);

			//Get other 3 corners
		dax = xspan*tspr->xrepeat;
		day = yspan*tspr->yrepeat;
		rzi[1] = rzi[0]-mulscale12(sinang,dax);
		rxi[1] = rxi[0]+mulscale12(cosang,dax);
		dax = -mulscale12(cosang,day);
		day = -mulscale12(sinang,day);
		rzi[2] = rzi[1]+dax; rxi[2] = rxi[1]+day;
		rzi[3] = rzi[0]+dax; rxi[3] = rxi[0]+day;

			//Put all points on same z
		ryi[0] = scale((tspr->z-globalposz),yxaspect,320<<8);
		if (ryi[0] == 0) return;
		ryi[1] = ryi[2] = ryi[3] = ryi[0];

		if ((cstat&4) == 0)
			{ z = 0; z1 = 1; z2 = 3; }
		else
			{ z = 1; z1 = 0; z2 = 2; }

		dax = rzi[z1]-rzi[z]; day = rxi[z1]-rxi[z];
		bot = dmulscale8(dax,dax,day,day);
		if (((klabs(dax)>>13) >= bot) || ((klabs(day)>>13) >= bot)) return;
		globalx1 = divscale18(dax,bot);
		globalx2 = divscale18(day,bot);

		dax = rzi[z2]-rzi[z]; day = rxi[z2]-rxi[z];
		bot = dmulscale8(dax,dax,day,day);
		if (((klabs(dax)>>13) >= bot) || ((klabs(day)>>13) >= bot)) return;
		globaly1 = divscale18(dax,bot);
		globaly2 = divscale18(day,bot);

			//Calculate globals for hline texture mapping function
		globalxpanning = (rxi[z]<<12);
		globalypanning = (rzi[z]<<12);
		globalzd = (ryi[z]<<12);

		rzi[0] = mulscale16(rzi[0],viewingrange);
		rzi[1] = mulscale16(rzi[1],viewingrange);
		rzi[2] = mulscale16(rzi[2],viewingrange);
		rzi[3] = mulscale16(rzi[3],viewingrange);

		if (ryi[0] < 0)   //If ceilsprite is above you, reverse order of points
		{
			i = rxi[1]; rxi[1] = rxi[3]; rxi[3] = i;
			i = rzi[1]; rzi[1] = rzi[3]; rzi[3] = i;
		}


			//Clip polygon in 3-space
		npoints = 4;

			//Clip edge 1
		npoints2 = 0;
		zzsgn = rxi[0]+rzi[0];
		for(z=0;z<npoints;z++)
		{
			zz = z+1; if (zz == npoints) zz = 0;
			zsgn = zzsgn; zzsgn = rxi[zz]+rzi[zz];
			if (zsgn >= 0)
			{
				rxi2[npoints2] = rxi[z]; ryi2[npoints2] = ryi[z]; rzi2[npoints2] = rzi[z];
				npoints2++;
			}
			if ((zsgn^zzsgn) < 0)
			{
				t = divscale30(zsgn,zsgn-zzsgn);
				rxi2[npoints2] = rxi[z] + mulscale30(t,rxi[zz]-rxi[z]);
				ryi2[npoints2] = ryi[z] + mulscale30(t,ryi[zz]-ryi[z]);
				rzi2[npoints2] = rzi[z] + mulscale30(t,rzi[zz]-rzi[z]);
				npoints2++;
			}
		}
		if (npoints2 <= 2) return;

			//Clip edge 2
		npoints = 0;
		zzsgn = rxi2[0]-rzi2[0];
		for(z=0;z<npoints2;z++)
		{
			zz = z+1; if (zz == npoints2) zz = 0;
			zsgn = zzsgn; zzsgn = rxi2[zz]-rzi2[zz];
			if (zsgn <= 0)
			{
				rxi[npoints] = rxi2[z]; ryi[npoints] = ryi2[z]; rzi[npoints] = rzi2[z];
				npoints++;
			}
			if ((zsgn^zzsgn) < 0)
			{
				t = divscale30(zsgn,zsgn-zzsgn);
				rxi[npoints] = rxi2[z] + mulscale30(t,rxi2[zz]-rxi2[z]);
				ryi[npoints] = ryi2[z] + mulscale30(t,ryi2[zz]-ryi2[z]);
				rzi[npoints] = rzi2[z] + mulscale30(t,rzi2[zz]-rzi2[z]);
				npoints++;
			}
		}
		if (npoints <= 2) return;

			//Clip edge 3
		npoints2 = 0;
		zzsgn = ryi[0]*halfxdimen + (rzi[0]*(globalhoriz-0));
		for(z=0;z<npoints;z++)
		{
			zz = z+1; if (zz == npoints) zz = 0;
			zsgn = zzsgn; zzsgn = ryi[zz]*halfxdimen + (rzi[zz]*(globalhoriz-0));
			if (zsgn >= 0)
			{
				rxi2[npoints2] = rxi[z];
				ryi2[npoints2] = ryi[z];
				rzi2[npoints2] = rzi[z];
				npoints2++;
			}
			if ((zsgn^zzsgn) < 0)
			{
				t = divscale30(zsgn,zsgn-zzsgn);
				rxi2[npoints2] = rxi[z] + mulscale30(t,rxi[zz]-rxi[z]);
				ryi2[npoints2] = ryi[z] + mulscale30(t,ryi[zz]-ryi[z]);
				rzi2[npoints2] = rzi[z] + mulscale30(t,rzi[zz]-rzi[z]);
				npoints2++;
			}
		}
		if (npoints2 <= 2) return;

			//Clip edge 4
		npoints = 0;
		zzsgn = ryi2[0]*halfxdimen + (rzi2[0]*(globalhoriz-ydimen));
		for(z=0;z<npoints2;z++)
		{
			zz = z+1; if (zz == npoints2) zz = 0;
			zsgn = zzsgn; zzsgn = ryi2[zz]*halfxdimen + (rzi2[zz]*(globalhoriz-ydimen));
			if (zsgn <= 0)
			{
				rxi[npoints] = rxi2[z];
				ryi[npoints] = ryi2[z];
				rzi[npoints] = rzi2[z];
				npoints++;
			}
			if ((zsgn^zzsgn) < 0)
			{
				t = divscale30(zsgn,zsgn-zzsgn);
				rxi[npoints] = rxi2[z] + mulscale30(t,rxi2[zz]-rxi2[z]);
				ryi[npoints] = ryi2[z] + mulscale30(t,ryi2[zz]-ryi2[z]);
				rzi[npoints] = rzi2[z] + mulscale30(t,rzi2[zz]-rzi2[z]);
				npoints++;
			}
		}
		if (npoints <= 2) return;

			//Project onto screen
		lpoint = -1; lmax = 0x7fffffff;
		rpoint = -1; rmax = 0x80000000;
		for(z=0;z<npoints;z++)
		{
			xsi[z] = scale(rxi[z],xdimen<<15,rzi[z]) + (xdimen<<15);
			ysi[z] = scale(ryi[z],xdimen<<15,rzi[z]) + (globalhoriz<<16);
			if (xsi[z] < 0) xsi[z] = 0;
			if (xsi[z] > (xdimen<<16)) xsi[z] = (xdimen<<16);
			if (ysi[z] < ((int)0<<16)) ysi[z] = ((int)0<<16);
			if (ysi[z] > ((int)ydimen<<16)) ysi[z] = ((int)ydimen<<16);
			if (xsi[z] < lmax) lmax = xsi[z], lpoint = z;
			if (xsi[z] > rmax) rmax = xsi[z], rpoint = z;
		}

			//Get uwall arrays
		for(z=lpoint;z!=rpoint;z=zz)
		{
			zz = z+1; if (zz == npoints) zz = 0;

			dax1 = ((xsi[z]+65535)>>16);
			dax2 = ((xsi[zz]+65535)>>16);
			if (dax2 > dax1)
			{
				yinc = divscale16(ysi[zz]-ysi[z],xsi[zz]-xsi[z]);
				y = ysi[z] + mulscale16((dax1<<16)-xsi[z],yinc);
				qinterpolatedown16short(&uwall[dax1],dax2-dax1,y,yinc);
			}
		}

			//Get dwall arrays
		for(;z!=lpoint;z=zz)
		{
			zz = z+1; if (zz == npoints) zz = 0;

			dax1 = ((xsi[zz]+65535)>>16);
			dax2 = ((xsi[z]+65535)>>16);
			if (dax2 > dax1)
			{
				yinc = divscale16(ysi[zz]-ysi[z],xsi[zz]-xsi[z]);
				y = ysi[zz] + mulscale16((dax1<<16)-xsi[zz],yinc);
				qinterpolatedown16short(&dwall[dax1],dax2-dax1,y,yinc);
			}
		}


		lx = ((lmax+65535)>>16);
		rx = ((rmax+65535)>>16);
		for(x=lx;x<=rx;x++)
		{
			uwall[x] = max(uwall[x],startumost[x+windowx1]-windowy1);
			dwall[x] = min(dwall[x],startdmost[x+windowx1]-windowy1);
		}

			//Additional uwall/dwall clipping goes here
		for(i=smostwallcnt-1;i>=0;i--)
		{
			j = smostwall[i];
			if ((xb1[j] > rx) || (xb2[j] < lx)) continue;
			if ((yp <= yb1[j]) && (yp <= yb2[j])) continue;

				//if (spritewallfront(tspr,thewall[j]) == 0)
			x = thewall[j]; xp1 = wall[x].x; yp1 = wall[x].y;
			x = wall[x].point2; xp2 = wall[x].x; yp2 = wall[x].y;
			x = (xp2-xp1)*(tspr->y-yp1)-(tspr->x-xp1)*(yp2-yp1);
			if ((yp > yb1[j]) && (yp > yb2[j])) x = -1;
			if ((x >= 0) && ((x != 0) || (wall[thewall[j]].nextsector != tspr->sectnum))) continue;

			dalx2 = max(xb1[j],lx); darx2 = min(xb2[j],rx);

			switch(smostwalltype[i])
			{
				case 0:
					if (dalx2 <= darx2)
					{
						if ((dalx2 == lx) && (darx2 == rx)) return;
						//clearbufbyte(&dwall[dalx2],(darx2-dalx2+1)*sizeof(dwall[0]),0L);
						for (x=dalx2; x<=darx2; x++) dwall[x] = 0;
					}
					break;
				case 1:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] > uwall[x]) uwall[x] = smost[k+x];
					break;
				case 2:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] < dwall[x]) dwall[x] = smost[k+x];
					break;
			}
		}

			//sprite
		if ((searchit >= 1) && (searchx >= lx) && (searchx <= rx))
			if ((searchy >= uwall[searchx]) && (searchy <= dwall[searchx]))
			{
				searchsector = sectnum; searchwall = spritenum;
				searchstat = 3; searchit = 1;
			}

		globalorientation = cstat;
		globalpicnum = tilenum;
		if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
		//if (picanm[globalpicnum]&192) globalpicnum += animateoffs((short)globalpicnum,spritenum+32768);

		if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
		setgotpic(globalpicnum);
		globalbufplc = waloff[globalpicnum];

		globvis = mulscale16(globalhisibility,viewingrange);
		if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));

		x = picsiz[globalpicnum]; y = ((x>>4)&15); x &= 15;
		if (pow2long[x] != xspan)
		{
			x++;
			globalx1 = mulscale(globalx1,xspan,x);
			globalx2 = mulscale(globalx2,xspan,x);
		}

		dax = globalxpanning; day = globalypanning;
		globalxpanning = -dmulscale6(globalx1,day,globalx2,dax);
		globalypanning = -dmulscale6(globaly1,day,globaly2,dax);

		globalx2 = mulscale16(globalx2,viewingrange);
		globaly2 = mulscale16(globaly2,viewingrange);
		globalzd = mulscale16(globalzd,viewingrangerecip);

		globalx1 = (globalx1-globalx2)*halfxdimen;
		globaly1 = (globaly1-globaly2)*halfxdimen;

		if ((cstat&2) == 0)
			msethlineshift(x,y);
		else
			tsethlineshift(x,y);

			//Draw it!
		ceilspritescan(lx,rx-1);
	}
	else if ((cstat&48) == 48)
	{
		int nxrepeat, nyrepeat;

		lx = 0; rx = xdim-1;
		for(x=lx;x<=rx;x++)
		{
			lwall[x] = (int)startumost[x+windowx1]-windowy1;
			swall[x] = (int)startdmost[x+windowx1]-windowy1;
		}
		for(i=smostwallcnt-1;i>=0;i--)
		{
			j = smostwall[i];
			if ((xb1[j] > rx) || (xb2[j] < lx)) continue;
			if ((yp <= yb1[j]) && (yp <= yb2[j])) continue;
			if (spritewallfront(tspr,(int)thewall[j]) && ((yp <= yb1[j]) || (yp <= yb2[j]))) continue;

			dalx2 = max(xb1[j],lx); darx2 = min(xb2[j],rx);

			switch(smostwalltype[i])
			{
				case 0:
					if (dalx2 <= darx2)
					{
						if ((dalx2 == lx) && (darx2 == rx)) return;
							//clearbufbyte(&swall[dalx2],(darx2-dalx2+1)*sizeof(swall[0]),0L);
						for (x=dalx2; x<=darx2; x++) swall[x] = 0;
					}
					break;
				case 1:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] > lwall[x]) lwall[x] = smost[k+x];
					break;
				case 2:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] < swall[x]) swall[x] = smost[k+x];
					break;
			}
		}

		if (lwall[rx] >= swall[rx])
		{
			for(x=lx;x<rx;x++)
				if (lwall[x] < swall[x]) break;
			if (x == rx) return;
		}

		for(i=0;i<MAXVOXMIPS;i++)
			if (!voxoff[vtilenum][i])
			{
				kloadvoxel(vtilenum);
				break;
			}

		longptr = (int *)voxoff[vtilenum][0];

		if (voxscale[vtilenum] == 65536)
		{
			nxrepeat = (((int)tspr->xrepeat)<<16);
			nyrepeat = (((int)tspr->yrepeat)<<16);
		}
		else
		{
			nxrepeat = ((int)tspr->xrepeat)*voxscale[vtilenum];
			nyrepeat = ((int)tspr->yrepeat)*voxscale[vtilenum];
		}

		if (!(cstat&128)) tspr->z -= mulscale22(B_LITTLE32(longptr[5]),nyrepeat);
		yoff = (int)((signed char)((picanm[sprite[tspr->owner].picnum]>>16)&255))+((int)tspr->yoffset);
		tspr->z -= mulscale14(yoff,nyrepeat);

		globvis = globalvisibility;
		if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));

		if ((searchit >= 1) && (yp > (4<<8)) && (searchy >= lwall[searchx]) && (searchy < swall[searchx]))
		{
			siz = divscale19(xdimenscale,yp);

			xv = mulscale16(nxrepeat,xyaspect);

			xspan = ((B_LITTLE32(longptr[0])+B_LITTLE32(longptr[1]))>>1);
			yspan = B_LITTLE32(longptr[2]);
			xsiz = mulscale30(siz,xv*xspan);
			ysiz = mulscale30(siz,nyrepeat*yspan);

				//Watch out for divscale overflow
			if (((xspan>>11) < xsiz) && (yspan < (ysiz>>1)))
			{
				x1 = xb-(xsiz>>1);
				if (xspan&1) x1 += mulscale31(siz,xv);  //Odd xspans
				i = mulscale30(siz,xv*xoff);
				if ((cstat&4) == 0) x1 -= i; else x1 += i;

				y1 = mulscale16(tspr->z-globalposz,siz);
				//y1 -= mulscale30(siz,nyrepeat*yoff);
				y1 += (globalhoriz<<8)-ysiz;
				//if (cstat&128)  //Already fixed up above
				y1 += (ysiz>>1);

				x2 = x1+xsiz-1;
				y2 = y1+ysiz-1;
				if (((y1|255) < (y2|255)) && (searchx >= (x1>>8)+1) && (searchx <= (x2>>8)))
				{
					if ((sec->ceilingstat&3) == 0)
						startum = globalhoriz+mulscale24(siz,sec->ceilingz-globalposz)-1;
					else
						startum = 0;
					if ((sec->floorstat&3) == 0)
						startdm = globalhoriz+mulscale24(siz,sec->floorz-globalposz)+1;
					else
						startdm = 0x7fffffff;

						//sprite
					if ((searchy >= max(startum,(y1>>8))) && (searchy < min(startdm,(y2>>8))))
					{
						searchsector = sectnum; searchwall = spritenum;
						searchstat = 3; searchit = 1;
					}
				}
			}
		}

		i = (int)tspr->ang+1536;
#if USE_POLYMOST && USE_OPENGL
		i += spriteext[tspr->owner].angoff;
#endif
		drawvox(tspr->x,tspr->y,tspr->z,i,(int)tspr->xrepeat,(int)tspr->yrepeat,vtilenum,tspr->shade,tspr->pal,lwall,swall);
	}

	if (automapping == 1) show2dsprite[spritenum>>3] |= pow2char[spritenum&7];
}


//
// drawmaskwall (internal)
//
static void drawmaskwall(short damaskwallcnt)
{
	int i, j, k, x, z, sectnum, z1, z2, lx, rx;
	sectortype *sec, *nsec;
	walltype *wal;

	//============================================================================= //POLYMOST BEGINS
#if USE_POLYMOST
	if (rendmode) { polymost_drawmaskwall(damaskwallcnt); return; }
#endif
	//============================================================================= //POLYMOST ENDS

	z = maskwall[damaskwallcnt];
	wal = &wall[thewall[z]];
	sectnum = thesector[z]; sec = &sector[sectnum];
	nsec = &sector[wal->nextsector];
	z1 = max(nsec->ceilingz,sec->ceilingz);
	z2 = min(nsec->floorz,sec->floorz);

	wallmost(uwall,z,sectnum,(char)0);
	wallmost(uplc,z,(int)wal->nextsector,(char)0);
	for(x=xb1[z];x<=xb2[z];x++) if (uplc[x] > uwall[x]) uwall[x] = uplc[x];
	wallmost(dwall,z,sectnum,(char)1);
	wallmost(dplc,z,(int)wal->nextsector,(char)1);
	for(x=xb1[z];x<=xb2[z];x++) if (dplc[x] < dwall[x]) dwall[x] = dplc[x];
	prepwall(z,wal);

	globalorientation = (int)wal->cstat;
	globalpicnum = wal->overpicnum;
	if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
	globalxpanning = (int)wal->xpanning;
	globalypanning = (int)wal->ypanning;
	if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)thewall[z]+16384);
	globalshade = (int)wal->shade;
	globvis = globalvisibility;
	if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
	globalpal = (int)wal->pal;
	if (palookup[globalpal] == 0) globalpal = 0;
	globalshiftval = (picsiz[globalpicnum]>>4);
	if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
	globalshiftval = 32-globalshiftval;
	globalyscale = (wal->yrepeat<<(globalshiftval-19));
	if ((globalorientation&4) == 0)
		globalzd = (((globalposz-z1)*globalyscale)<<8);
	else
		globalzd = (((globalposz-z2)*globalyscale)<<8);
	globalzd += (globalypanning<<24);
	if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;

	for(i=smostwallcnt-1;i>=0;i--)
	{
		j = smostwall[i];
		if ((xb1[j] > xb2[z]) || (xb2[j] < xb1[z])) continue;
		if (wallfront(j,z)) continue;

		lx = max(xb1[j],xb1[z]); rx = min(xb2[j],xb2[z]);

		switch(smostwalltype[i])
		{
			case 0:
				if (lx <= rx)
				{
					if ((lx == xb1[z]) && (rx == xb2[z])) return;
					//clearbufbyte(&dwall[lx],(rx-lx+1)*sizeof(dwall[0]),0L);
					for (x=lx; x<=rx; x++) dwall[x] = 0;
				}
				break;
			case 1:
				k = smoststart[i] - xb1[j];
				for(x=lx;x<=rx;x++)
					if (smost[k+x] > uwall[x]) uwall[x] = smost[k+x];
				break;
			case 2:
				k = smoststart[i] - xb1[j];
				for(x=lx;x<=rx;x++)
					if (smost[k+x] < dwall[x]) dwall[x] = smost[k+x];
				break;
		}
	}

		//maskwall
	if ((searchit >= 1) && (searchx >= xb1[z]) && (searchx <= xb2[z]))
		if ((searchy >= uwall[searchx]) && (searchy <= dwall[searchx]))
		{
			searchsector = sectnum; searchwall = thewall[z];
			searchstat = 4; searchit = 1;
		}

	if ((globalorientation&128) == 0)
		maskwallscan(xb1[z],xb2[z],uwall,dwall,swall,lwall);
	else
	{
		if (globalorientation&128)
		{
			if (globalorientation&512) settransreverse(); else settransnormal();
		}
		transmaskwallscan(xb1[z],xb2[z]);
	}
}


//
// fillpolygon (internal)
//
static void fillpolygon(int npoints)
{
	int z, zz, x1, y1, x2, y2, miny, maxy, y, xinc, cnt;
	int ox, oy, bx, by, day1, day2;
	intptr_t p;
	short *ptr, *ptr2;

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == 3) { polymost_fillpolygon(npoints); return; }
#endif

	miny = 0x7fffffff; maxy = 0x80000000;
	for(z=npoints-1;z>=0;z--)
		{ y = ry1[z]; miny = min(miny,y); maxy = max(maxy,y); }
	miny = (miny>>12); maxy = (maxy>>12);
	if (miny < 0) miny = 0;
	if (maxy >= ydim) maxy = ydim-1;
	ptr = smost;    //They're pointers! - watch how you optimize this thing
	for(y=miny;y<=maxy;y++)
	{
		dotp1[y] = ptr; dotp2[y] = ptr+(MAXNODESPERLINE>>1);
		ptr += MAXNODESPERLINE;
	}

	for(z=npoints-1;z>=0;z--)
	{
		zz = xb1[z];
		y1 = ry1[z]; day1 = (y1>>12);
		y2 = ry1[zz]; day2 = (y2>>12);
		if (day1 != day2)
		{
			x1 = rx1[z]; x2 = rx1[zz];
			xinc = divscale12(x2-x1,y2-y1);
			if (day2 > day1)
			{
				x1 += mulscale12((day1<<12)+4095-y1,xinc);
				for(y=day1;y<day2;y++) { *dotp2[y]++ = (x1>>12); x1 += xinc; }
			}
			else
			{
				x2 += mulscale12((day2<<12)+4095-y2,xinc);
				for(y=day2;y<day1;y++) { *dotp1[y]++ = (x2>>12); x2 += xinc; }
			}
		}
	}

	globalx1 = mulscale16(globalx1,xyaspect);
	globaly2 = mulscale16(globaly2,xyaspect);

	oy = miny+1-(ydim>>1);
	globalposx += oy*globalx1;
	globalposy += oy*globaly2;

	setuphlineasm4(asm1,asm2);

	ptr = smost;
	for(y=miny;y<=maxy;y++)
	{
		cnt = (int)(dotp1[y]-ptr); ptr2 = ptr+(MAXNODESPERLINE>>1);
		for(z=cnt-1;z>=0;z--)
		{
			day1 = 0; day2 = 0;
			for(zz=z;zz>0;zz--)
			{
				if (ptr[zz] < ptr[day1]) day1 = zz;
				if (ptr2[zz] < ptr2[day2]) day2 = zz;
			}
			x1 = ptr[day1]; ptr[day1] = ptr[z];
			x2 = ptr2[day2]-1; ptr2[day2] = ptr2[z];
			if (x1 > x2) continue;

			if (globalpolytype < 1)
			{
					//maphline
				ox = x2+1-(xdim>>1);
				bx = ox*asm1 + globalposx;
				by = ox*asm2 - globalposy;

				p = ylookup[y]+x2+frameplace;
				hlineasm4(x2-x1,-1L,globalshade<<8,by,bx,(void *)p);
			}
			else
			{
					//maphline
				ox = x1+1-(xdim>>1);
				bx = ox*asm1 + globalposx;
				by = ox*asm2 - globalposy;

				p = ylookup[y]+x1+frameplace;
				if (globalpolytype == 1)
					mhline((void *)globalbufplc,bx,(x2-x1)<<16,0L,by,(void *)p);
				else
				{
					thline((void *)globalbufplc,bx,(x2-x1)<<16,0L,by,(void *)p);
				}
			}
		}
		globalposx += globalx1;
		globalposy += globaly2;
		ptr += MAXNODESPERLINE;
	}
	faketimerhandler();
}


//
// clippoly (internal)
//
static int clippoly(int npoints, int clipstat)
{
	int z, zz, s1, s2, t, npoints2, start2, z1, z2, z3, z4, splitcnt;
	int cx1, cy1, cx2, cy2;

	cx1 = windowx1;
	cy1 = windowy1;
	cx2 = windowx2+1;
	cy2 = windowy2+1;
	cx1 <<= 12; cy1 <<= 12; cx2 <<= 12; cy2 <<= 12;

	if (clipstat&0xa)   //Need to clip top or left
	{
		npoints2 = 0; start2 = 0; z = 0; splitcnt = 0;
		do
		{
			s2 = cx1-rx1[z];
			do
			{
				zz = xb1[z]; xb1[z] = -1;
				s1 = s2; s2 = cx1-rx1[zz];
				if (s1 < 0)
				{
					rx2[npoints2] = rx1[z]; ry2[npoints2] = ry1[z];
					xb2[npoints2] = npoints2+1; npoints2++;
				}
				if ((s1^s2) < 0)
				{
					rx2[npoints2] = rx1[z]+scale(rx1[zz]-rx1[z],s1,s1-s2);
					ry2[npoints2] = ry1[z]+scale(ry1[zz]-ry1[z],s1,s1-s2);
					if (s1 < 0) p2[splitcnt++] = npoints2;
					xb2[npoints2] = npoints2+1;
					npoints2++;
				}
				z = zz;
			} while (xb1[z] >= 0);

			if (npoints2 >= start2+3)
				xb2[npoints2-1] = start2, start2 = npoints2;
			else
				npoints2 = start2;

			z = 1;
			while ((z < npoints) && (xb1[z] < 0)) z++;
		} while (z < npoints);
		if (npoints2 <= 2) return(0);

		for(z=1;z<splitcnt;z++)
			for(zz=0;zz<z;zz++)
			{
				z1 = p2[z]; z2 = xb2[z1]; z3 = p2[zz]; z4 = xb2[z3];
				s1  = klabs(rx2[z1]-rx2[z2])+klabs(ry2[z1]-ry2[z2]);
				s1 += klabs(rx2[z3]-rx2[z4])+klabs(ry2[z3]-ry2[z4]);
				s2  = klabs(rx2[z1]-rx2[z4])+klabs(ry2[z1]-ry2[z4]);
				s2 += klabs(rx2[z3]-rx2[z2])+klabs(ry2[z3]-ry2[z2]);
				if (s2 < s1)
					{ t = xb2[p2[z]]; xb2[p2[z]] = xb2[p2[zz]]; xb2[p2[zz]] = t; }
			}


		npoints = 0; start2 = 0; z = 0; splitcnt = 0;
		do
		{
			s2 = cy1-ry2[z];
			do
			{
				zz = xb2[z]; xb2[z] = -1;
				s1 = s2; s2 = cy1-ry2[zz];
				if (s1 < 0)
				{
					rx1[npoints] = rx2[z]; ry1[npoints] = ry2[z];
					xb1[npoints] = npoints+1; npoints++;
				}
				if ((s1^s2) < 0)
				{
					rx1[npoints] = rx2[z]+scale(rx2[zz]-rx2[z],s1,s1-s2);
					ry1[npoints] = ry2[z]+scale(ry2[zz]-ry2[z],s1,s1-s2);
					if (s1 < 0) p2[splitcnt++] = npoints;
					xb1[npoints] = npoints+1;
					npoints++;
				}
				z = zz;
			} while (xb2[z] >= 0);

			if (npoints >= start2+3)
				xb1[npoints-1] = start2, start2 = npoints;
			else
				npoints = start2;

			z = 1;
			while ((z < npoints2) && (xb2[z] < 0)) z++;
		} while (z < npoints2);
		if (npoints <= 2) return(0);

		for(z=1;z<splitcnt;z++)
			for(zz=0;zz<z;zz++)
			{
				z1 = p2[z]; z2 = xb1[z1]; z3 = p2[zz]; z4 = xb1[z3];
				s1  = klabs(rx1[z1]-rx1[z2])+klabs(ry1[z1]-ry1[z2]);
				s1 += klabs(rx1[z3]-rx1[z4])+klabs(ry1[z3]-ry1[z4]);
				s2  = klabs(rx1[z1]-rx1[z4])+klabs(ry1[z1]-ry1[z4]);
				s2 += klabs(rx1[z3]-rx1[z2])+klabs(ry1[z3]-ry1[z2]);
				if (s2 < s1)
					{ t = xb1[p2[z]]; xb1[p2[z]] = xb1[p2[zz]]; xb1[p2[zz]] = t; }
			}
	}
	if (clipstat&0x5)   //Need to clip bottom or right
	{
		npoints2 = 0; start2 = 0; z = 0; splitcnt = 0;
		do
		{
			s2 = rx1[z]-cx2;
			do
			{
				zz = xb1[z]; xb1[z] = -1;
				s1 = s2; s2 = rx1[zz]-cx2;
				if (s1 < 0)
				{
					rx2[npoints2] = rx1[z]; ry2[npoints2] = ry1[z];
					xb2[npoints2] = npoints2+1; npoints2++;
				}
				if ((s1^s2) < 0)
				{
					rx2[npoints2] = rx1[z]+scale(rx1[zz]-rx1[z],s1,s1-s2);
					ry2[npoints2] = ry1[z]+scale(ry1[zz]-ry1[z],s1,s1-s2);
					if (s1 < 0) p2[splitcnt++] = npoints2;
					xb2[npoints2] = npoints2+1;
					npoints2++;
				}
				z = zz;
			} while (xb1[z] >= 0);

			if (npoints2 >= start2+3)
				xb2[npoints2-1] = start2, start2 = npoints2;
			else
				npoints2 = start2;

			z = 1;
			while ((z < npoints) && (xb1[z] < 0)) z++;
		} while (z < npoints);
		if (npoints2 <= 2) return(0);

		for(z=1;z<splitcnt;z++)
			for(zz=0;zz<z;zz++)
			{
				z1 = p2[z]; z2 = xb2[z1]; z3 = p2[zz]; z4 = xb2[z3];
				s1  = klabs(rx2[z1]-rx2[z2])+klabs(ry2[z1]-ry2[z2]);
				s1 += klabs(rx2[z3]-rx2[z4])+klabs(ry2[z3]-ry2[z4]);
				s2  = klabs(rx2[z1]-rx2[z4])+klabs(ry2[z1]-ry2[z4]);
				s2 += klabs(rx2[z3]-rx2[z2])+klabs(ry2[z3]-ry2[z2]);
				if (s2 < s1)
					{ t = xb2[p2[z]]; xb2[p2[z]] = xb2[p2[zz]]; xb2[p2[zz]] = t; }
			}


		npoints = 0; start2 = 0; z = 0; splitcnt = 0;
		do
		{
			s2 = ry2[z]-cy2;
			do
			{
				zz = xb2[z]; xb2[z] = -1;
				s1 = s2; s2 = ry2[zz]-cy2;
				if (s1 < 0)
				{
					rx1[npoints] = rx2[z]; ry1[npoints] = ry2[z];
					xb1[npoints] = npoints+1; npoints++;
				}
				if ((s1^s2) < 0)
				{
					rx1[npoints] = rx2[z]+scale(rx2[zz]-rx2[z],s1,s1-s2);
					ry1[npoints] = ry2[z]+scale(ry2[zz]-ry2[z],s1,s1-s2);
					if (s1 < 0) p2[splitcnt++] = npoints;
					xb1[npoints] = npoints+1;
					npoints++;
				}
				z = zz;
			} while (xb2[z] >= 0);

			if (npoints >= start2+3)
				xb1[npoints-1] = start2, start2 = npoints;
			else
				npoints = start2;

			z = 1;
			while ((z < npoints2) && (xb2[z] < 0)) z++;
		} while (z < npoints2);
		if (npoints <= 2) return(0);

		for(z=1;z<splitcnt;z++)
			for(zz=0;zz<z;zz++)
			{
				z1 = p2[z]; z2 = xb1[z1]; z3 = p2[zz]; z4 = xb1[z3];
				s1  = klabs(rx1[z1]-rx1[z2])+klabs(ry1[z1]-ry1[z2]);
				s1 += klabs(rx1[z3]-rx1[z4])+klabs(ry1[z3]-ry1[z4]);
				s2  = klabs(rx1[z1]-rx1[z4])+klabs(ry1[z1]-ry1[z4]);
				s2 += klabs(rx1[z3]-rx1[z2])+klabs(ry1[z3]-ry1[z2]);
				if (s2 < s1)
					{ t = xb1[p2[z]]; xb1[p2[z]] = xb1[p2[zz]]; xb1[p2[zz]] = t; }
			}
	}
	return(npoints);
}


//
// clippoly4 (internal)
//
	//Assume npoints=4 with polygon on &nrx1,&nry1
	//JBF 20031206: Thanks to Ken's hunting, s/(rx1|ry1|rx2|ry2)/n\1/ in this function
static int clippoly4(int cx1, int cy1, int cx2, int cy2)
{
	int n, nn, z, zz, x, x1, x2, y, y1, y2, t;

	nn = 0; z = 0;
	do
	{
		zz = ((z+1)&3);
		x1 = nrx1[z]; x2 = nrx1[zz]-x1;

		if ((cx1 <= x1) && (x1 <= cx2))
			nrx2[nn] = x1, nry2[nn] = nry1[z], nn++;

		if (x2 <= 0) x = cx2; else x = cx1;
		t = x-x1;
		if (((t-x2)^t) < 0)
			nrx2[nn] = x, nry2[nn] = nry1[z]+scale(t,nry1[zz]-nry1[z],x2), nn++;

		if (x2 <= 0) x = cx1; else x = cx2;
		t = x-x1;
		if (((t-x2)^t) < 0)
			nrx2[nn] = x, nry2[nn] = nry1[z]+scale(t,nry1[zz]-nry1[z],x2), nn++;

		z = zz;
	} while (z != 0);
	if (nn < 3) return(0);

	n = 0; z = 0;
	do
	{
		zz = z+1; if (zz == nn) zz = 0;
		y1 = nry2[z]; y2 = nry2[zz]-y1;

		if ((cy1 <= y1) && (y1 <= cy2))
			nry1[n] = y1, nrx1[n] = nrx2[z], n++;

		if (y2 <= 0) y = cy2; else y = cy1;
		t = y-y1;
		if (((t-y2)^t) < 0)
			nry1[n] = y, nrx1[n] = nrx2[z]+scale(t,nrx2[zz]-nrx2[z],y2), n++;

		if (y2 <= 0) y = cy1; else y = cy2;
		t = y-y1;
		if (((t-y2)^t) < 0)
			nry1[n] = y, nrx1[n] = nrx2[z]+scale(t,nrx2[zz]-nrx2[z],y2), n++;

		z = zz;
	} while (z != 0);
	return(n);
}


//
// dorotatesprite (internal)
//
	//JBF 20031206: Thanks to Ken's hunting, s/(rx1|ry1|rx2|ry2)/n\1/ in this function
static void dorotatesprite(int sx, int sy, int z, short a, short picnum, signed char dashade,
	unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2, int uniqid)
{
	int cosang, sinang, v, nextv, dax1, dax2, oy, bx, by;
	int x, y, x1, y1, x2, y2, gx1, gy1, iv;
	intptr_t i, p, bufplc, palookupoffs;
	int xsiz, ysiz, xoff, yoff, npoints, yplc, yinc, lx, rx;
	int xv, yv, xv2, yv2;

	//============================================================================= //POLYMOST BEGINS
#if USE_POLYMOST
	if (rendmode) { polymost_dorotatesprite(sx,sy,z,a,picnum,dashade,dapalnum,dastat,cx1,cy1,cx2,cy2,uniqid); return; }
#else
	(void)uniqid;
#endif
	//============================================================================= //POLYMOST ENDS

	if (cx1 < 0) cx1 = 0;
	if (cy1 < 0) cy1 = 0;
	if (cx2 > xres-1) cx2 = xres-1;
	if (cy2 > yres-1) cy2 = yres-1;

	xsiz = tilesizx[picnum]; ysiz = tilesizy[picnum];
	if (dastat&16) { xoff = 0; yoff = 0; }
	else
	{
		xoff = (int)((signed char)((picanm[picnum]>>8)&255))+(xsiz>>1);
		yoff = (int)((signed char)((picanm[picnum]>>16)&255))+(ysiz>>1);
	}

	if (dastat&4) yoff = ysiz-yoff;

	cosang = sintable[(a+512)&2047]; sinang = sintable[a&2047];

	if ((dastat&2) != 0)  //Auto window size scaling
	{
		if ((dastat&8) == 0)
		{
			if (widescreen) {
				x = ydimenscale;   //= scale(xdimen,yxaspect,320);
				sx = ((cx1+cx2+2)<<15)+scale(sx-(320<<15),ydimen<<16,200*pixelaspect);
			} else {
				x = xdimenscale;   //= scale(xdimen,yxaspect,320);
				sx = ((cx1+cx2+2)<<15)+scale(sx-(320<<15),xdimen,320);
			}
			sy = ((cy1+cy2+2)<<15)+mulscale16(sy-(200<<15),x);
		}
		else
		{
			  //If not clipping to startmosts, & auto-scaling on, as a
			  //hard-coded bonus, scale to full screen instead
			if (widescreen) {
				x = scale(ydim<<16,yxaspect,200*pixelaspect);
				sx = (xdim<<15)+32768+scale(sx-(320<<15),ydim<<16,200*pixelaspect);
			} else {
				x = scale(xdim,yxaspect,320);
				sx = (xdim<<15)+32768+scale(sx-(320<<15),xdim,320);
			}
			sy = (ydim<<15)+32768+mulscale16(sy-(200<<15),x);
		}
		z = mulscale16(z,x);
	}

	xv = mulscale14(cosang,z);
	yv = mulscale14(sinang,z);
	if (((dastat&2) != 0) || ((dastat&8) == 0)) //Don't aspect unscaled perms
	{
		xv2 = mulscale16(xv,xyaspect);
		yv2 = mulscale16(yv,xyaspect);
	}
	else
	{
		xv2 = xv;
		yv2 = yv;
	}

	nry1[0] = sy - (yv*xoff + xv*yoff);
	nry1[1] = nry1[0] + yv*xsiz;
	nry1[3] = nry1[0] + xv*ysiz;
	nry1[2] = nry1[1]+nry1[3]-nry1[0];
	i = (cy1<<16); if ((nry1[0]<i) && (nry1[1]<i) && (nry1[2]<i) && (nry1[3]<i)) return;
	i = (cy2<<16); if ((nry1[0]>i) && (nry1[1]>i) && (nry1[2]>i) && (nry1[3]>i)) return;

	nrx1[0] = sx - (xv2*xoff - yv2*yoff);
	nrx1[1] = nrx1[0] + xv2*xsiz;
	nrx1[3] = nrx1[0] - yv2*ysiz;
	nrx1[2] = nrx1[1]+nrx1[3]-nrx1[0];
	i = (cx1<<16); if ((nrx1[0]<i) && (nrx1[1]<i) && (nrx1[2]<i) && (nrx1[3]<i)) return;
	i = (cx2<<16); if ((nrx1[0]>i) && (nrx1[1]>i) && (nrx1[2]>i) && (nrx1[3]>i)) return;

	gx1 = nrx1[0]; gy1 = nry1[0];   //back up these before clipping

	if ((npoints = clippoly4(cx1<<16,cy1<<16,(cx2+1)<<16,(cy2+1)<<16)) < 3) return;

	lx = nrx1[0]; rx = nrx1[0];

	nextv = 0;
	for(v=npoints-1;v>=0;v--)
	{
		x1 = nrx1[v]; x2 = nrx1[nextv];
		dax1 = (x1>>16); if (x1 < lx) lx = x1;
		dax2 = (x2>>16); if (x1 > rx) rx = x1;
		if (dax1 != dax2)
		{
			y1 = nry1[v]; y2 = nry1[nextv];
			yinc = divscale16(y2-y1,x2-x1);
			if (dax2 > dax1)
			{
				yplc = y1 + mulscale16((dax1<<16)+65535-x1,yinc);
				qinterpolatedown16short(&uplc[dax1],dax2-dax1,yplc,yinc);
			}
			else
			{
				yplc = y2 + mulscale16((dax2<<16)+65535-x2,yinc);
				qinterpolatedown16short(&dplc[dax2],dax1-dax2,yplc,yinc);
			}
		}
		nextv = v;
	}

	if (waloff[picnum] == 0) loadtile(picnum);
	setgotpic(picnum);
	bufplc = waloff[picnum];

	palookupoffs = (intptr_t)palookup[dapalnum] + (getpalookup(0L,(int)dashade)<<8);

	iv = divscale32(1L,z);
	xv = mulscale14(sinang,iv);
	yv = mulscale14(cosang,iv);
	if (((dastat&2) != 0) || ((dastat&8) == 0)) //Don't aspect unscaled perms
	{
		yv2 = mulscale16(-xv,yxaspect);
		xv2 = mulscale16(yv,yxaspect);
	}
	else
	{
		yv2 = -xv;
		xv2 = yv;
	}

	x1 = (lx>>16); x2 = (rx>>16);

	oy = 0;
	x = (x1<<16)-1-gx1; y = (oy<<16)+65535-gy1;
	bx = dmulscale16(x,xv2,y,xv);
	by = dmulscale16(x,yv2,y,yv);
	if (dastat&4) { yv = -yv; yv2 = -yv2; by = (ysiz<<16)-1-by; }

#ifndef ENGINE_USING_A_C
	int ny1, ny2, xx, xend, qlinemode=0, y1ve[4], y2ve[4], u4, d4;
	char bad;

	if ((dastat&1) == 0)
	{
		if (((a&1023) == 0) && (ysiz <= 256))  //vlineasm4 has 256 high limit!
		{
			if (dastat&64) setupvlineasm(24L); else setupmvlineasm(24L);
			by <<= 8; yv <<= 8; yv2 <<= 8;

			palookupoffse[0] = palookupoffse[1] = palookupoffse[2] = palookupoffse[3] = palookupoffs;
			vince[0] = vince[1] = vince[2] = vince[3] = yv;

			for(x=x1;x<x2;x+=4)
			{
				bad = 15; xend = min(x2-x,4);
				for(xx=0;xx<xend;xx++)
				{
					bx += xv2;

					y1 = uplc[x+xx]; y2 = dplc[x+xx];
					if ((dastat&8) == 0)
					{
						if (startumost[x+xx] > y1) y1 = startumost[x+xx];
						if (startdmost[x+xx] < y2) y2 = startdmost[x+xx];
					}
					if (y2 <= y1) continue;

					by += yv*(y1-oy); oy = y1;

					bufplce[xx] = (bx>>16)*ysiz+bufplc;
					vplce[xx] = by;
					y1ve[xx] = y1;
					y2ve[xx] = y2-1;
					bad &= ~pow2char[xx];
				}

				p = x+frameplace;

				u4 = max(max(y1ve[0],y1ve[1]),max(y1ve[2],y1ve[3]));
				d4 = min(min(y2ve[0],y2ve[1]),min(y2ve[2],y2ve[3]));

				if (dastat&64)
				{
					if ((bad != 0) || (u4 >= d4))
					{
						if (!(bad&1)) prevlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0],vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
						if (!(bad&2)) prevlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y1ve[1],vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
						if (!(bad&4)) prevlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-y1ve[2],vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
						if (!(bad&8)) prevlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-y1ve[3],vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));
						continue;
					}

					if (u4 > y1ve[0]) vplce[0] = prevlineasm1(vince[0],(void *)palookupoffse[0],u4-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
					if (u4 > y1ve[1]) vplce[1] = prevlineasm1(vince[1],(void *)palookupoffse[1],u4-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
					if (u4 > y1ve[2]) vplce[2] = prevlineasm1(vince[2],(void *)palookupoffse[2],u4-y1ve[2]-1,vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
					if (u4 > y1ve[3]) vplce[3] = prevlineasm1(vince[3],(void *)palookupoffse[3],u4-y1ve[3]-1,vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));

					if (d4 >= u4) vlineasm4(d4-u4+1,(void *)(ylookup[u4]+p));

					i = p+ylookup[d4+1];
					if (y2ve[0] > d4) prevlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-d4-1,vplce[0],(void *)bufplce[0],(void *)(i+0));
					if (y2ve[1] > d4) prevlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-d4-1,vplce[1],(void *)bufplce[1],(void *)(i+1));
					if (y2ve[2] > d4) prevlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-d4-1,vplce[2],(void *)bufplce[2],(void *)(i+2));
					if (y2ve[3] > d4) prevlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-d4-1,vplce[3],(void *)bufplce[3],(void *)(i+3));
				}
				else
				{
					if ((bad != 0) || (u4 >= d4))
					{
						if (!(bad&1)) mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0],vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
						if (!(bad&2)) mvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y1ve[1],vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
						if (!(bad&4)) mvlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-y1ve[2],vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
						if (!(bad&8)) mvlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-y1ve[3],vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));
						continue;
					}

					if (u4 > y1ve[0]) vplce[0] = mvlineasm1(vince[0],(void *)palookupoffse[0],u4-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
					if (u4 > y1ve[1]) vplce[1] = mvlineasm1(vince[1],(void *)palookupoffse[1],u4-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
					if (u4 > y1ve[2]) vplce[2] = mvlineasm1(vince[2],(void *)palookupoffse[2],u4-y1ve[2]-1,vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
					if (u4 > y1ve[3]) vplce[3] = mvlineasm1(vince[3],(void *)palookupoffse[3],u4-y1ve[3]-1,vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));

					if (d4 >= u4) mvlineasm4(d4-u4+1,(void *)(ylookup[u4]+p));

					i = p+ylookup[d4+1];
					if (y2ve[0] > d4) mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-d4-1,vplce[0],(void *)bufplce[0],(void *)(i+0));
					if (y2ve[1] > d4) mvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-d4-1,vplce[1],(void *)bufplce[1],(void *)(i+1));
					if (y2ve[2] > d4) mvlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-d4-1,vplce[2],(void *)bufplce[2],(void *)(i+2));
					if (y2ve[3] > d4) mvlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-d4-1,vplce[3],(void *)bufplce[3],(void *)(i+3));
				}

				faketimerhandler();
			}
		}
		else
		{
			if (dastat&64)
			{
				if ((xv2&0x0000ffff) == 0)
				{
					qlinemode = 1;
					setupqrhlineasm4(0L,yv2<<16,(xv2>>16)*ysiz+(yv2>>16),(void *)palookupoffs,0L,0L);
				}
				else
				{
					qlinemode = 0;
					setuprhlineasm4(xv2<<16,yv2<<16,(xv2>>16)*ysiz+(yv2>>16),(void *)palookupoffs,ysiz,0L);
				}
			}
			else
				setuprmhlineasm4(xv2<<16,yv2<<16,(xv2>>16)*ysiz+(yv2>>16),(void *)palookupoffs,ysiz,0L);

			y1 = uplc[x1];
			if (((dastat&8) == 0) && (startumost[x1] > y1)) y1 = startumost[x1];
			y2 = y1;
			for(x=x1;x<x2;x++)
			{
				ny1 = uplc[x]-1; ny2 = dplc[x];
				if ((dastat&8) == 0)
				{
					if (startumost[x]-1 > ny1) ny1 = startumost[x]-1;
					if (startdmost[x] < ny2) ny2 = startdmost[x];
				}

				if (ny1 < ny2-1)
				{
					if (ny1 >= y2)
					{
						while (y1 < y2-1)
						{
							y1++; if ((y1&31) == 0) faketimerhandler();

								//x,y1
							bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1;
							if (dastat&64) {
								if (qlinemode) qrhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L    ,by<<16,(void *)(ylookup[y1]+x+frameplace));
								else rhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
							} else rmhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
						}
						y1 = ny1;
					}
					else
					{
						while (y1 < ny1)
						{
							y1++; if ((y1&31) == 0) faketimerhandler();

								//x,y1
							bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1;
							if (dastat&64) {
								if (qlinemode) qrhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L    ,by<<16,(void *)(ylookup[y1]+x+frameplace));
								else rhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
							} else rmhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
						}
						while (y1 > ny1) lastx[y1--] = x;
					}
					while (y2 > ny2)
					{
						y2--; if ((y2&31) == 0) faketimerhandler();

							//x,y2
						bx += xv*(y2-oy); by += yv*(y2-oy); oy = y2;
						if (dastat&64) {
							if (qlinemode) qrhlineasm4(x-lastx[y2],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L    ,by<<16,(void *)(ylookup[y2]+x+frameplace));
							else rhlineasm4(x-lastx[y2],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y2]+x+frameplace));
						} else rmhlineasm4(x-lastx[y2],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y2]+x+frameplace));
					}
					while (y2 < ny2) lastx[y2++] = x;
				}
				else
				{
					while (y1 < y2-1)
					{
						y1++; if ((y1&31) == 0) faketimerhandler();

							//x,y1
						bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1;
						if (dastat&64) {
							if (qlinemode) qrhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L    ,by<<16,(void *)(ylookup[y1]+x+frameplace));
							else rhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
						} else rmhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
					}
					if (x == x2-1) { bx += xv2; by += yv2; break; }
					y1 = uplc[x+1];
					if (((dastat&8) == 0) && (startumost[x+1] > y1)) y1 = startumost[x+1];
					y2 = y1;
				}
				bx += xv2; by += yv2;
			}
			while (y1 < y2-1)
			{
				y1++; if ((y1&31) == 0) faketimerhandler();

					//x2,y1
				bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1;
				if (dastat&64) {
					if (qlinemode) qrhlineasm4(x2-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L,by<<16,(void *)(ylookup[y1]+x2+frameplace));
					else rhlineasm4(x2-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x2+frameplace));
				} else rmhlineasm4(x2-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x2+frameplace));
			}
		}
	}
	else
	{
		if ((dastat&1) == 0)
		{
			if (dastat&64)
				setupspritevline((void *)palookupoffs,(xv>>16)*ysiz,xv<<16,ysiz,yv,0L);
			else
				msetupspritevline((void *)palookupoffs,(xv>>16)*ysiz,xv<<16,ysiz,yv,0L);
		}
		else
		{
			tsetupspritevline((void *)palookupoffs,(xv>>16)*ysiz,xv<<16,ysiz,yv,0L);
			if (dastat&32) settransreverse(); else settransnormal();
		}
		for(x=x1;x<x2;x++)
		{
			bx += xv2; by += yv2;

			y1 = uplc[x]; y2 = dplc[x];
			if ((dastat&8) == 0)
			{
				if (startumost[x] > y1) y1 = startumost[x];
				if (startdmost[x] < y2) y2 = startdmost[x];
			}
			if (y2 <= y1) continue;

			switch(y1-oy)
			{
				case -1: bx -= xv; by -= yv; oy = y1; break;
				case 0: break;
				case 1: bx += xv; by += yv; oy = y1; break;
				default: bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1; break;
			}

			p = ylookup[y1]+x+frameplace;

			if ((dastat&1) == 0)
			{
				if (dastat&64)
					spritevline(0L,by<<16,y2-y1+1,bx<<16,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
				else
					mspritevline(0L,by<<16,y2-y1+1,bx<<16,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
			}
			else
			{
				tspritevline(0L,by<<16,y2-y1+1,bx<<16,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
			}
			faketimerhandler();
		}
	}

#else	// ENGINE_USING_A_C

	if ((dastat&1) == 0)
	{
		if (dastat&64)
			setupspritevline((void *)palookupoffs,xv,yv,ysiz);
		else
			msetupspritevline((void *)palookupoffs,xv,yv,ysiz);
	}
	else
	{
		tsetupspritevline((void *)palookupoffs,xv,yv,ysiz);
		if (dastat&32) settransreverse(); else settransnormal();
	}
	for(x=x1;x<x2;x++)
	{
		bx += xv2; by += yv2;

		y1 = uplc[x]; y2 = dplc[x];
		if ((dastat&8) == 0)
		{
			if (startumost[x] > y1) y1 = startumost[x];
			if (startdmost[x] < y2) y2 = startdmost[x];
		}
		if (y2 <= y1) continue;

		switch(y1-oy)
		{
			case -1: bx -= xv; by -= yv; oy = y1; break;
			case 0: break;
			case 1: bx += xv; by += yv; oy = y1; break;
			default: bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1; break;
		}

		p = ylookup[y1]+x+frameplace;

		if ((dastat&1) == 0)
		{
			if (dastat&64)
				spritevline(bx&65535,by&65535,y2-y1+1,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
			else
				mspritevline(bx&65535,by&65535,y2-y1+1,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
		}
		else
		{
			tspritevline(bx&65535,by&65535,y2-y1+1,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
			//transarea += (y2-y1);
		}
		faketimerhandler();
	}

#endif
}


//
// initksqrt (internal)
//
static void initksqrt(void)
{
	int i, j, k;

	j = 1; k = 0;
	for(i=0;i<4096;i++)
	{
		if (i >= j) { j <<= 2; k++; }
		sqrtable[i] = (unsigned short)(msqrtasm((i<<18)+131072)<<1);
		shlookup[i] = (k<<1)+((10-k)<<8);
		if (i < 256) shlookup[i+4096] = ((k+6)<<1)+((10-(k+6))<<8);
	}
}


//
// dosetaspect
//
static void dosetaspect(void)
{
	int i, j, k, x, xinc;

	if (xyaspect != oxyaspect)
	{
		oxyaspect = xyaspect;
		j = xyaspect*320;
		horizlookup2[horizycent-1] = divscale26(131072,j);
		for(i=ydim*4-1;i>=0;i--)
			if (i != (horizycent-1))
			{
				horizlookup[i] = divscale28(1,i-(horizycent-1));
				horizlookup2[i] = divscale14(klabs(horizlookup[i]),j);
			}
	}
	if ((xdimen != oxdimen) || (viewingrange != oviewingrange))
	{
		oxdimen = xdimen;
		oviewingrange = viewingrange;
		xinc = mulscale32(viewingrange*320,xdimenrecip);
		x = (640<<16)-mulscale1(xinc,xdimen);
		for(i=0;i<xdimen;i++)
		{
			j = (x&65535); k = (x>>16); x += xinc;
			if (j != 0) j = mulscale16((int)radarang[k+1]-(int)radarang[k],j);
			radarang2[i] = (short)(((int)radarang[k]+j)>>6);
		}
		for(i=1;i<65536;i++) distrecip[i] = divscale20(xdimen,i);
		nytooclose = xdimen*2100;
		nytoofar = 65536*16384-1048576;
	}
}


//
// loadtables (internal)
//
static void calcbritable(void)
{
	int i,j;
	double a,b;
	for(i=0;i<16;i++) {
		a = (double)8 / ((double)i+8);
		b = (double)255 / pow((double)255,a);
		for(j=0;j<256;j++)	// JBF 20040207: full 8bit precision
			britable[i][j] = (unsigned char)(pow((double)j,a)*b);
	}
}

static int loadtables(void)
{
	int i;

	initksqrt();
    for(i=0;i<2048;i++) {
        sintable[i] = (short)(16384*sin((double)i*3.14159265358979/1024));
        reciptable[i] = divscale30(2048L,i+2048);
    }
    for(i=0;i<640;i++) {
        radarang[i] = (short)(atan(((double)i-639.5)/160)*64*1024/3.14159265358979);
        radarang[1279-i] = -radarang[i];
    }
	calcbritable();

    if (crc32once((unsigned char *)sintable, sizeof(sintable)) != 0xee1e7aba) {
        engineerrstr = "Calculation of sintable yielded unexpected results.";
        return 1;
    }
    if (crc32once((unsigned char *)radarang, sizeof(radarang)/2) != 0xee893d92) {
        engineerrstr = "Calculation of radarang yielded unexpected results.";
        return 1;
    }

	return 0;
}


//
// initfastcolorlookup (internal)
//
static void initfastcolorlookup(int rscale, int gscale, int bscale)
{
	int i, j, x, y, z;
	unsigned char *pal1;

	j = 0;
	for(i=64;i>=0;i--)
	{
		//j = (i-64)*(i-64);
		rdist[i] = rdist[128-i] = j*rscale;
		gdist[i] = gdist[128-i] = j*gscale;
		bdist[i] = bdist[128-i] = j*bscale;
		j += 129-(i<<1);
	}

	//clearbufbyte(colhere,sizeof(colhere),0L);
	//clearbufbyte(colhead,sizeof(colhead),0L);
	Bmemset(colhere,0,sizeof(colhere));
	Bmemset(colhead,0,sizeof(colhead));

	pal1 = &palette[768-3];
	for(i=255;i>=0;i--,pal1-=3)
	{
		j = (pal1[0]>>3)*FASTPALGRIDSIZ*FASTPALGRIDSIZ+(pal1[1]>>3)*FASTPALGRIDSIZ+(pal1[2]>>3)+FASTPALGRIDSIZ*FASTPALGRIDSIZ+FASTPALGRIDSIZ+1;
		if (colhere[j>>3]&pow2char[j&7]) colnext[i] = colhead[j]; else colnext[i] = -1;
		colhead[j] = i;
		colhere[j>>3] |= pow2char[j&7];
	}

	i = 0;
	for(x=-FASTPALGRIDSIZ*FASTPALGRIDSIZ;x<=FASTPALGRIDSIZ*FASTPALGRIDSIZ;x+=FASTPALGRIDSIZ*FASTPALGRIDSIZ)
		for(y=-FASTPALGRIDSIZ;y<=FASTPALGRIDSIZ;y+=FASTPALGRIDSIZ)
			for(z=-1;z<=1;z++)
				colscan[i++] = x+y+z;
	i = colscan[13]; colscan[13] = colscan[26]; colscan[26] = i;
}


//
// loadpalette (internal)
//
static int loadpalette(void)
{
	int fil = -1;
	off_t flen;

	if ((fil = kopen4load("palette.dat",0)) < 0) goto badpalette;
	flen = kfilelength(fil);

	if (kread(fil,palette,768) != 768) {
		buildputs("loadpalette: truncated palette\n");
		goto badpalette;
	}

	if ((flen - 65536 - 2 - 768) % 256 == 0) {
		// Map format 7+ palette.
		if (kread(fil,&numpalookups,2) != 2) {
			buildputs("loadpalette: read error\n");
			goto badpalette;
		}
		numpalookups = B_LITTLE16(numpalookups);
	} else if ((flen - 32640 - 768) % 256 == 0) {
		// Old format palette.
		numpalookups = (flen - 32640 - 768) >> 8;
		buildprintf("loadpalette: old format palette (%d shades)\n",
			numpalookups);
		goto badpalette;
	} else {
		buildprintf("loadpalette: damaged palette\n");
		goto badpalette;
	}

	if ((palookup[0] = kmalloc(numpalookups<<8)) == NULL) {
		engineerrstr = "Failed to allocate palette memory";
		kclose(fil);
		return 1;
	}
	if ((transluc = kmalloc(65536L)) == NULL) {
		engineerrstr = "Failed to allocate translucency memory";
		kclose(fil);
		return 1;
	}

	globalpalwritten = palookup[0]; globalpal = 0;
	setpalookupaddress(globalpalwritten);

	fixtransluscence(transluc);

	kread(fil,palookup[globalpal],numpalookups<<8);
	kread(fil,transluc,65536);
	kclose(fil);

	initfastcolorlookup(30L,59L,11L);

	return 0;

badpalette:
	engineerrstr = "Failed to load PALETTE.DAT!";
	if (fil >= 0) kclose(fil);
	return 1;
}


//
// getclosestcol
//
int getclosestcol(int r, int g, int b)
{
	int i, j, k, dist, mindist, retcol;
	unsigned char *pal1;

	j = (r>>3)*FASTPALGRIDSIZ*FASTPALGRIDSIZ+(g>>3)*FASTPALGRIDSIZ+(b>>3)+FASTPALGRIDSIZ*FASTPALGRIDSIZ+FASTPALGRIDSIZ+1;
	mindist = min(rdist[coldist[r&7]+64+8],gdist[coldist[g&7]+64+8]);
	mindist = min(mindist,bdist[coldist[b&7]+64+8]);
	mindist++;

	r = 64-r; g = 64-g; b = 64-b;

	retcol = -1;
	for(k=26;k>=0;k--)
	{
		i = colscan[k]+j; if ((colhere[i>>3]&pow2char[i&7]) == 0) continue;
		i = colhead[i];
		do
		{
			pal1 = &palette[i*3];
			dist = gdist[pal1[1]+g];
			if (dist < mindist)
			{
				dist += rdist[pal1[0]+r];
				if (dist < mindist)
				{
					dist += bdist[pal1[2]+b];
					if (dist < mindist) { mindist = dist; retcol = i; }
				}
			}
			i = colnext[i];
		} while (i >= 0);
	}
	if (retcol >= 0) return(retcol);

	mindist = 0x7fffffff;
	pal1 = &palette[768-3];
	for(i=255;i>=0;i--,pal1-=3)
	{
		dist = gdist[pal1[1]+g]; if (dist >= mindist) continue;
		dist += rdist[pal1[0]+r]; if (dist >= mindist) continue;
		dist += bdist[pal1[2]+b]; if (dist >= mindist) continue;
		mindist = dist; retcol = i;
	}
	return(retcol);
}


//
// insertspritesect (internal)
//
static int insertspritesect(short sectnum)
{
	short blanktouse;

	if ((sectnum >= MAXSECTORS) || (headspritesect[MAXSECTORS] == -1))
		return(-1);  //list full

	blanktouse = headspritesect[MAXSECTORS];

	headspritesect[MAXSECTORS] = nextspritesect[blanktouse];
	if (headspritesect[MAXSECTORS] >= 0)
		prevspritesect[headspritesect[MAXSECTORS]] = -1;

	prevspritesect[blanktouse] = -1;
	nextspritesect[blanktouse] = headspritesect[sectnum];
	if (headspritesect[sectnum] >= 0)
		prevspritesect[headspritesect[sectnum]] = blanktouse;
	headspritesect[sectnum] = blanktouse;

	sprite[blanktouse].sectnum = sectnum;

	return(blanktouse);
}


//
// insertspritestat (internal)
//
static int insertspritestat(short statnum)
{
	short blanktouse;

	if ((statnum >= MAXSTATUS) || (headspritestat[MAXSTATUS] == -1))
		return(-1);  //list full

	blanktouse = headspritestat[MAXSTATUS];

	headspritestat[MAXSTATUS] = nextspritestat[blanktouse];
	if (headspritestat[MAXSTATUS] >= 0)
		prevspritestat[headspritestat[MAXSTATUS]] = -1;

	prevspritestat[blanktouse] = -1;
	nextspritestat[blanktouse] = headspritestat[statnum];
	if (headspritestat[statnum] >= 0)
		prevspritestat[headspritestat[statnum]] = blanktouse;
	headspritestat[statnum] = blanktouse;

	sprite[blanktouse].statnum = statnum;

	return(blanktouse);
}


//
// deletespritesect (internal)
//
static int deletespritesect(short deleteme)
{
	if (sprite[deleteme].sectnum == MAXSECTORS)
		return(-1);

	if (headspritesect[sprite[deleteme].sectnum] == deleteme)
		headspritesect[sprite[deleteme].sectnum] = nextspritesect[deleteme];

	if (prevspritesect[deleteme] >= 0) nextspritesect[prevspritesect[deleteme]] = nextspritesect[deleteme];
	if (nextspritesect[deleteme] >= 0) prevspritesect[nextspritesect[deleteme]] = prevspritesect[deleteme];

	if (headspritesect[MAXSECTORS] >= 0) prevspritesect[headspritesect[MAXSECTORS]] = deleteme;
	prevspritesect[deleteme] = -1;
	nextspritesect[deleteme] = headspritesect[MAXSECTORS];
	headspritesect[MAXSECTORS] = deleteme;

	sprite[deleteme].sectnum = MAXSECTORS;
	return(0);
}


//
// deletespritestat (internal)
//
static int deletespritestat(short deleteme)
{
	if (sprite[deleteme].statnum == MAXSTATUS)
		return(-1);

	if (headspritestat[sprite[deleteme].statnum] == deleteme)
		headspritestat[sprite[deleteme].statnum] = nextspritestat[deleteme];

	if (prevspritestat[deleteme] >= 0) nextspritestat[prevspritestat[deleteme]] = nextspritestat[deleteme];
	if (nextspritestat[deleteme] >= 0) prevspritestat[nextspritestat[deleteme]] = prevspritestat[deleteme];

	if (headspritestat[MAXSTATUS] >= 0) prevspritestat[headspritestat[MAXSTATUS]] = deleteme;
	prevspritestat[deleteme] = -1;
	nextspritestat[deleteme] = headspritestat[MAXSTATUS];
	headspritestat[MAXSTATUS] = deleteme;

	sprite[deleteme].statnum = MAXSTATUS;
	return(0);
}


//
// lintersect (internal)
//
static int lintersect(int x1, int y1, int z1, int x2, int y2, int z2, int x3,
		  int y3, int x4, int y4, int *intx, int *inty, int *intz)
{     //p1 to p2 is a line segment
	int x21, y21, x34, y34, x31, y31, bot, topt, topu, t;

	x21 = x2-x1; x34 = x3-x4;
	y21 = y2-y1; y34 = y3-y4;
	bot = x21*y34 - y21*x34;
	if (bot >= 0)
	{
		if (bot == 0) return(0);
		x31 = x3-x1; y31 = y3-y1;
		topt = x31*y34 - y31*x34; if ((topt < 0) || (topt >= bot)) return(0);
		topu = x21*y31 - y21*x31; if ((topu < 0) || (topu >= bot)) return(0);
	}
	else
	{
		x31 = x3-x1; y31 = y3-y1;
		topt = x31*y34 - y31*x34; if ((topt > 0) || (topt <= bot)) return(0);
		topu = x21*y31 - y21*x31; if ((topu > 0) || (topu <= bot)) return(0);
	}
	t = divscale24(topt,bot);
	*intx = x1 + mulscale24(x21,t);
	*inty = y1 + mulscale24(y21,t);
	*intz = z1 + mulscale24(z2-z1,t);
	return(1);
}


//
// rintersect (internal)
//
static int rintersect(int x1, int y1, int z1, int vx, int vy, int vz, int x3,
		  int y3, int x4, int y4, int *intx, int *inty, int *intz)
{     //p1 towards p2 is a ray
	int x34, y34, x31, y31, bot, topt, topu, t;

	x34 = x3-x4; y34 = y3-y4;
	bot = vx*y34 - vy*x34;
	if (bot >= 0)
	{
		if (bot == 0) return(0);
		x31 = x3-x1; y31 = y3-y1;
		topt = x31*y34 - y31*x34; if (topt < 0) return(0);
		topu = vx*y31 - vy*x31; if ((topu < 0) || (topu >= bot)) return(0);
	}
	else
	{
		x31 = x3-x1; y31 = y3-y1;
		topt = x31*y34 - y31*x34; if (topt > 0) return(0);
		topu = vx*y31 - vy*x31; if ((topu > 0) || (topu <= bot)) return(0);
	}
	t = divscale16(topt,bot);
	*intx = x1 + mulscale16(vx,t);
	*inty = y1 + mulscale16(vy,t);
	*intz = z1 + mulscale16(vz,t);
	return(1);
}


//
// keepaway (internal)
//
static void keepaway (int *x, int *y, int w)
{
	int dx, dy, ox, oy, x1, y1;
	char first;

	x1 = clipit[w].x1; dx = clipit[w].x2-x1;
	y1 = clipit[w].y1; dy = clipit[w].y2-y1;
	ox = ksgn(-dy); oy = ksgn(dx);
	first = (klabs(dx) <= klabs(dy));
	while (1)
	{
		if (dx*(*y-y1) > (*x-x1)*dy) return;
		if (first == 0) *x += ox; else *y += oy;
		first ^= 1;
	}
}


//
// raytrace (internal)
//
static int raytrace(int x3, int y3, int *x4, int *y4)
{
	int x1, y1, x2, y2, bot, topu, nintx, ninty, cnt, z, hitwall;
	int x21, y21, x43, y43;

	hitwall = -1;
	for(z=clipnum-1;z>=0;z--)
	{
		x1 = clipit[z].x1; x2 = clipit[z].x2; x21 = x2-x1;
		y1 = clipit[z].y1; y2 = clipit[z].y2; y21 = y2-y1;

		topu = x21*(y3-y1) - (x3-x1)*y21; if (topu <= 0) continue;
		if (x21*(*y4-y1) > (*x4-x1)*y21) continue;
		x43 = *x4-x3; y43 = *y4-y3;
		if (x43*(y1-y3) > (x1-x3)*y43) continue;
		if (x43*(y2-y3) <= (x2-x3)*y43) continue;
		bot = x43*y21 - x21*y43; if (bot == 0) continue;

		cnt = 256;
		do
		{
			cnt--; if (cnt < 0) { *x4 = x3; *y4 = y3; return(z); }
			nintx = x3 + scale(x43,topu,bot);
			ninty = y3 + scale(y43,topu,bot);
			topu--;
		} while (x21*(ninty-y1) <= (nintx-x1)*y21);

		if (klabs(x3-nintx)+klabs(y3-ninty) < klabs(x3-*x4)+klabs(y3-*y4))
			{ *x4 = nintx; *y4 = ninty; hitwall = z; }
	}
	return(hitwall);
}



//
// Exported Engine Functions
//

#if !defined _WIN32 && defined DEBUGGINGAIDS
#include <signal.h>
static void sighandler(int sig, siginfo_t *info, void *ctx)
{
	const char *s;

	(void)ctx;

	switch (sig) {
		case SIGFPE:
			switch (info->si_code) {
				case FPE_INTDIV: s = "FPE_INTDIV (integer divide by zero)"; break;
				case FPE_INTOVF: s = "FPE_INTOVF (integer overflow)"; break;
				case FPE_FLTDIV: s = "FPE_FLTDIV (floating-point divide by zero)"; break;
				case FPE_FLTOVF: s = "FPE_FLTOVF (floating-point overflow)"; break;
				case FPE_FLTUND: s = "FPE_FLTUND (floating-point underflow)"; break;
				case FPE_FLTRES: s = "FPE_FLTRES (floating-point inexact result)"; break;
				case FPE_FLTINV: s = "FPE_FLTINV (floating-point invalid operation)"; break;
				case FPE_FLTSUB: s = "FPE_FLTSUB (floating-point subscript out of range)"; break;
				default: s = "?! (unknown)"; break;
			}
			fprintf(stderr, "Caught SIGFPE at address %p, code %s. Aborting.\n", info->si_addr, s);
			break;
		default: break;
	}
	abort();
}
#endif

//
// preinitengine
//
static int preinitcalled = 0;
int preinitengine(void)
{
	char *e;
	char compiler[30] = "an unidentified compiler";

#if defined(_MSC_VER)
	sprintf(compiler, "MS Visual C++ %d.%02d", _MSC_VER/100, _MSC_VER%100);
#elif defined(__clang__)
	sprintf(compiler, "Clang %d.%d", __clang_major__, __clang_minor__);
#elif defined(__GNUC__)
	sprintf(compiler, "GCC %d.%d", __GNUC__, __GNUC_MINOR__);
#endif

	buildprintf("\nBUILD engine by Ken Silverman (http://www.advsys.net/ken)\n"
	       "Additional improvements by Jonathon Fowler (http://www.jonof.id.au)\n"
	       "and other contributors. See BUILDLIC.TXT for terms.\n\n"
	       "Version %s.\nBuilt %s %s using %s.\n%d-bit word size.\n\n",
	       build_version, build_date, build_time, compiler, (int)(sizeof(intptr_t)<<3));

	// Detect anomalous structure packing.
	assert(sizeof(sectortype) == 40);
	assert((intptr_t)&sector[1] - (intptr_t)&sector[0] == sizeof(sectortype));
	assert(sizeof(walltype) == 32);
	assert((intptr_t)&wall[1] - (intptr_t)&wall[0] == sizeof(walltype));
	assert(sizeof(spritetype) == 44);
	assert((intptr_t)&sprite[1] - (intptr_t)&sprite[0] == sizeof(spritetype));
	assert(sizeof(spriteexttype) == 12);
	assert((intptr_t)&spriteext[1] - (intptr_t)&spriteext[0] == sizeof(spriteexttype));

	if (initsystem()) exit(1);

	makeasmwriteable();

	if ((e = Bgetenv("BUILD_NOP6")) != NULL)
		if (!Bstrcasecmp(e, "TRUE")) {
			buildprintf("Disabling P6 optimizations.\n");
			dommxoverlay = 0;
		}
	if (dommxoverlay) mmxoverlay();

	validmodecnt = 0;
	getvalidmodes();

	initcrc32table();

	preinitcalled = 1;
	return 0;
}


//
// initengine
//
int initengine(void)
{
	int i, j;

#if !defined _WIN32 && defined DEBUGGINGAIDS
	struct sigaction sigact, oldact;
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_sigaction = sighandler;
	sigact.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sigact, &oldact);
#endif
	if (!preinitcalled) {
		i = preinitengine();
		if (i) return i;
	}

	xyaspect = -1;

	pskyoff[0] = 0; pskybits = 0;

	parallaxtype = 2; parallaxyoffs = 0L; parallaxyscale = 65536;
	showinvisibility = 0;

	for(i=1;i<1024;i++) lowrecip[i] = ((1<<24)-1)/i;
	for(i=0;i<MAXVOXELS;i++)
		for(j=0;j<MAXVOXMIPS;j++)
		{
			voxoff[i][j] = 0L;
			voxlock[i][j] = 200;
		}
	for(i=0;i<MAXTILES;i++)
		tiletovox[i] = -1;
	clearbuf(&voxscale[0],sizeof(voxscale)>>2,65536L);

#if USE_POLYMOST
	polymost_initosdfuncs();
#endif

	searchit = 0; searchstat = -1;

	for(i=0;i<MAXPALOOKUPS;i++) palookup[i] = NULL;

	clearbuf(&waloff[0],(int)MAXTILES,0L);

	clearbuf(&show2dsector[0],(int)((MAXSECTORS+3)>>5),0L);
	clearbuf(&show2dsprite[0],(int)((MAXSPRITES+3)>>5),0L);
	clearbuf(&show2dwall[0],(int)((MAXWALLS+3)>>5),0L);
	automapping = 0;

	pointhighlight = -1;
	linehighlight = -1;
	highlightcnt = 0;

	totalclock = 0;
	visibility = 512;
	parallaxvisibility = 512;

	captureformat = 2;  // PNG

	if (loadtables()) return 1;
	if (loadpalette()) return 1;

#if USE_POLYMOST && USE_OPENGL
	if (!hicfirstinit) hicinit();
	if (!mdinited) mdinit();
	PTCacheLoadIndex();
#endif

	return 0;
}


//
// uninitengine
//
void uninitengine(void)
{
	int i;

	//buildprintf("cacheresets = %d, cacheinvalidates = %d\n", cacheresets, cacheinvalidates);

#if USE_POLYMOST && USE_OPENGL
	polymost_glreset();
	PTClear();
	PTCacheUnloadIndex();
	hicinit();
	freeallmodels();
#endif

	uninitsystem();

	if (logfile) Bfclose(logfile);
	logfile = NULL;

	if (artfil != -1) kclose(artfil);

	if (transluc != NULL) { kfree(transluc); transluc = NULL; }
	if (pic != NULL) { kfree(pic); pic = NULL; }
	if (lookups != NULL) { kfree(lookups); lookups = NULL; }
	for(i=0;i<MAXPALOOKUPS;i++)
		if (palookup[i] != NULL) { kfree(palookup[i]); palookup[i] = NULL; }
}


//
// initspritelists
//
void initspritelists(void)
{
	int i;

	for (i=0;i<MAXSECTORS;i++)     //Init doubly-linked sprite sector lists
		headspritesect[i] = -1;
	headspritesect[MAXSECTORS] = 0;
	for(i=0;i<MAXSPRITES;i++)
	{
		prevspritesect[i] = i-1;
		nextspritesect[i] = i+1;
		sprite[i].sectnum = MAXSECTORS;
	}
	prevspritesect[0] = -1;
	nextspritesect[MAXSPRITES-1] = -1;


	for(i=0;i<MAXSTATUS;i++)      //Init doubly-linked sprite status lists
		headspritestat[i] = -1;
	headspritestat[MAXSTATUS] = 0;
	for(i=0;i<MAXSPRITES;i++)
	{
		prevspritestat[i] = i-1;
		nextspritestat[i] = i+1;
		sprite[i].statnum = MAXSTATUS;
	}
	prevspritestat[0] = -1;
	nextspritestat[MAXSPRITES-1] = -1;
}


//
// drawrooms
//
void drawrooms(int daposx, int daposy, int daposz,
		 short daang, int dahoriz, short dacursectnum)
{
	int i, j, z, cz, fz, closest;
	short *shortptr1, *shortptr2;

#if defined(DEBUGGINGAIDS)
	if (numscans > MAXWALLSB) debugprintf("damage report: numscans %d exceeded %d\n", numscans, MAXWALLSB);
	if (numbunches > MAXWALLSB) debugprintf("damage report: numbunches %d exceeded %d\n", numbunches, MAXWALLSB);
	if (maskwallcnt > MAXWALLSB) debugprintf("damage report: maskwallcnt %d exceeded %d\n", maskwallcnt, MAXWALLSB);
	if (smostwallcnt > MAXWALLSB) debugprintf("damage report: smostwallcnt %d exceeded %d\n", smostwallcnt, MAXWALLSB);
#endif

	beforedrawrooms = 0;

	globalposx = daposx; globalposy = daposy; globalposz = daposz;
	globalang = (daang&2047);

	globalhoriz = mulscale16(dahoriz-100,xdimenscale)+(ydimen>>1);
	globaluclip = (0-globalhoriz)*xdimscale;
	globaldclip = (ydimen-globalhoriz)*xdimscale;

	i = mulscale16(xdimenscale,viewingrangerecip);
	globalpisibility = mulscale16(parallaxvisibility,i);
	globalvisibility = mulscale16(visibility,i);
	globalhisibility = mulscale16(globalvisibility,xyaspect);
	globalcisibility = mulscale8(globalhisibility,320);

	globalcursectnum = dacursectnum;
	totalclocklock = totalclock;

	cosglobalang = sintable[(globalang+512)&2047];
	singlobalang = sintable[globalang&2047];
	cosviewingrangeglobalang = mulscale16(cosglobalang,viewingrange);
	sinviewingrangeglobalang = mulscale16(singlobalang,viewingrange);

	if ((xyaspect != oxyaspect) || (xdimen != oxdimen) || (viewingrange != oviewingrange))
		dosetaspect();

	//clearbufbyte(&gotsector[0],(int)((numsectors+7)>>3),0L);
	Bmemset(&gotsector[0],0,(int)((numsectors+7)>>3));

	shortptr1 = (short *)&startumost[windowx1];
	shortptr2 = (short *)&startdmost[windowx1];
	i = xdimen-1;
	do
	{
		umost[i] = shortptr1[i]-windowy1;
		dmost[i] = shortptr2[i]-windowy1;
		i--;
	} while (i != 0);
	umost[0] = shortptr1[0]-windowy1;
	dmost[0] = shortptr2[0]-windowy1;

	//============================================================================= //POLYMOST BEGINS
#if USE_POLYMOST
	polymost_drawrooms(); if (rendmode) { return; }
#endif
	//============================================================================= //POLYMOST ENDS

	begindrawing();	//{{{

	frameoffset = frameplace + windowy1*bytesperline + windowx1;

	numhits = xdimen; numscans = 0; numbunches = 0;
	maskwallcnt = 0; smostwallcnt = 0; smostcnt = 0; spritesortcnt = 0;

	if (globalcursectnum >= MAXSECTORS)
		globalcursectnum -= MAXSECTORS;
	else
	{
		i = globalcursectnum;
		updatesector(globalposx,globalposy,&globalcursectnum);
		if (globalcursectnum < 0) globalcursectnum = i;
	}

	globparaceilclip = 1;
	globparaflorclip = 1;
	getzsofslope(globalcursectnum,globalposx,globalposy,&cz,&fz);
	if (globalposz < cz) globparaceilclip = 0;
	if (globalposz > fz) globparaflorclip = 0;

	scansector(globalcursectnum);

	if (inpreparemirror)
	{
		inpreparemirror = 0;
		mirrorsx1 = xdimen-1; mirrorsx2 = 0;
		for(i=numscans-1;i>=0;i--)
		{
			if (wall[thewall[i]].nextsector < 0) continue;
			if (xb1[i] < mirrorsx1) mirrorsx1 = xb1[i];
			if (xb2[i] > mirrorsx2) mirrorsx2 = xb2[i];
		}

		for(i=0;i<mirrorsx1;i++)
			if (umost[i] <= dmost[i])
				{ umost[i] = 1; dmost[i] = 0; numhits--; }
		for(i=mirrorsx2+1;i<xdimen;i++)
			if (umost[i] <= dmost[i])
				{ umost[i] = 1; dmost[i] = 0; numhits--; }

		drawalls(0L);
		numbunches--;
		bunchfirst[0] = bunchfirst[numbunches];
		bunchlast[0] = bunchlast[numbunches];

		mirrorsy1 = min(umost[mirrorsx1],umost[mirrorsx2]);
		mirrorsy2 = max(dmost[mirrorsx1],dmost[mirrorsx2]);
	}

	while ((numbunches > 0) && (numhits > 0))
	{
		clearbuf(&tempbuf[0],(int)((numbunches+3)>>2),0L);
		tempbuf[0] = 1;

		closest = 0;              //Almost works, but not quite :(
		for(i=1;i<numbunches;i++)
		{
			if ((j = bunchfront(i,closest)) < 0) continue;
			tempbuf[i] = 1;
			if (j == 0) tempbuf[closest] = 1, closest = i;
		}
		for(i=0;i<numbunches;i++) //Double-check
		{
			if (tempbuf[i]) continue;
			if ((j = bunchfront(i,closest)) < 0) continue;
			tempbuf[i] = 1;
			if (j == 0) tempbuf[closest] = 1, closest = i, i = 0;
		}

		drawalls(closest);

		if (automapping)
		{
			for(z=bunchfirst[closest];z>=0;z=p2[z])
				show2dwall[thewall[z]>>3] |= pow2char[thewall[z]&7];
		}

		numbunches--;
		bunchfirst[closest] = bunchfirst[numbunches];
		bunchlast[closest] = bunchlast[numbunches];
	}

	enddrawing();	//}}}
}


//
// drawmasks
//
void drawmasks(void)
{
	int i, j, k, l, gap, xs, ys, xp, yp, yoff, yspan;

	for(i=spritesortcnt-1;i>=0;i--) tspriteptr[i] = &tsprite[i];
	for(i=spritesortcnt-1;i>=0;i--)
	{
		xs = tspriteptr[i]->x-globalposx; ys = tspriteptr[i]->y-globalposy;
		yp = dmulscale6(xs,cosviewingrangeglobalang,ys,sinviewingrangeglobalang);
		if (yp > (4<<8))
		{
			xp = dmulscale6(ys,cosglobalang,-xs,singlobalang);
			if (mulscale24(abs(xp+yp),xdimen) >= yp) goto killsprite;
			spritesx[i] = scale(xp+yp,xdimen<<7,yp);
		}
		else if ((tspriteptr[i]->cstat&48) == 0)
		{
killsprite:
			spritesortcnt--;  //Delete face sprite if on wrong side!
			if (i != spritesortcnt)
			{
				tspriteptr[i] = tspriteptr[spritesortcnt];
				spritesx[i] = spritesx[spritesortcnt];
				spritesy[i] = spritesy[spritesortcnt];
			}
			continue;
		}
		spritesy[i] = yp;
	}

	gap = 1; while (gap < spritesortcnt) gap = (gap<<1)+1;
	for(gap>>=1;gap>0;gap>>=1)      //Sort sprite list
		for(i=0;i<spritesortcnt-gap;i++)
			for(l=i;l>=0;l-=gap)
			{
				if (spritesy[l] <= spritesy[l+gap]) break;
				swaplong(&tspriteptr[l],&tspriteptr[l+gap]);
				swaplong(&spritesx[l],&spritesx[l+gap]);
				swaplong(&spritesy[l],&spritesy[l+gap]);
			}

	if (spritesortcnt > 0)
		spritesy[spritesortcnt] = (spritesy[spritesortcnt-1]^1);

	ys = spritesy[0]; i = 0;
	for(j=1;j<=spritesortcnt;j++)
	{
		if (spritesy[j] == ys) continue;
		ys = spritesy[j];
		if (j > i+1)
		{
			for(k=i;k<j;k++)
			{
				spritesz[k] = tspriteptr[k]->z;
				if ((tspriteptr[k]->cstat&48) != 32)
				{
					yoff = (int)((signed char)((picanm[tspriteptr[k]->picnum]>>16)&255))+((int)tspriteptr[k]->yoffset);
					spritesz[k] -= ((yoff*tspriteptr[k]->yrepeat)<<2);
					yspan = (tilesizy[tspriteptr[k]->picnum]*tspriteptr[k]->yrepeat<<2);
					if (!(tspriteptr[k]->cstat&128)) spritesz[k] -= (yspan>>1);
					if (klabs(spritesz[k]-globalposz) < (yspan>>1)) spritesz[k] = globalposz;
				}
			}
			for(k=i+1;k<j;k++)
				for(l=i;l<k;l++)
					if (klabs(spritesz[k]-globalposz) < klabs(spritesz[l]-globalposz))
					{
						swaplong(&tspriteptr[k],&tspriteptr[l]);
						swaplong(&spritesx[k],&spritesx[l]);
						swaplong(&spritesy[k],&spritesy[l]);
						swaplong(&spritesz[k],&spritesz[l]);
					}
			for(k=i+1;k<j;k++)
				for(l=i;l<k;l++)
					if (tspriteptr[k]->statnum < tspriteptr[l]->statnum)
					{
						swaplong(&tspriteptr[k],&tspriteptr[l]);
						swaplong(&spritesx[k],&spritesx[l]);
						swaplong(&spritesy[k],&spritesy[l]);
					}
		}
		i = j;
	}

	begindrawing();	//{{{

	/*for(i=spritesortcnt-1;i>=0;i--)
	{
		xs = tspriteptr[i].x-globalposx;
		ys = tspriteptr[i].y-globalposy;
		zs = tspriteptr[i].z-globalposz;

		xp = ys*cosglobalang-xs*singlobalang;
		yp = (zs<<1);
		zp = xs*cosglobalang+ys*singlobalang;

		xs = scale(xp,halfxdimen<<12,zp)+((halfxdimen+windowx1)<<12);
		ys = scale(yp,xdimenscale<<12,zp)+((globalhoriz+windowy1)<<12);

		drawline256(xs-65536,ys-65536,xs+65536,ys+65536,31);
		drawline256(xs+65536,ys-65536,xs-65536,ys+65536,31);
	}*/


#if USE_POLYMOST
		//Hack to make it draw all opaque quads first. This should reduce the chances of
		//bad sorting causing transparent quads knocking out opaque quads behind it.
		//
		//Need to store alpha flag with all textures before this works right!
	if (rendmode > 0)
	{
		for(i=spritesortcnt-1;i>=0;i--)
			if ((!(tspriteptr[i]->cstat&2))
#if USE_OPENGL
			    && (!polymost_texmayhavealpha(tspriteptr[i]->picnum,tspriteptr[i]->pal))
#endif
			   )
				{ drawsprite(i); tspriteptr[i] = 0; } //draw only if it is fully opaque
		for(i=j=0;i<spritesortcnt;i++)
		{
			if (!tspriteptr[i]) continue;
			tspriteptr[j] = tspriteptr[i];
			spritesx[j] = spritesx[i];
			spritesy[j] = spritesy[i]; j++;
		}
		spritesortcnt = j;


		for(i=maskwallcnt-1;i>=0;i--)
		{
			k = thewall[maskwall[i]];
			if ((!(wall[k].cstat&128))
#if USE_OPENGL
			    && (!polymost_texmayhavealpha(wall[k].overpicnum,wall[k].pal))
#endif
			   )
				{ drawmaskwall(i); maskwall[i] = -1; } //draw only if it is fully opaque
		}
		for(i=j=0;i<maskwallcnt;i++)
		{
			if (maskwall[i] < 0) continue;
			maskwall[j++] = maskwall[i];
		}
		maskwallcnt = j;
	}
#endif

	while ((spritesortcnt > 0) && (maskwallcnt > 0))  //While BOTH > 0
	{
		j = maskwall[maskwallcnt-1];
		if (spritewallfront(tspriteptr[spritesortcnt-1],(int)thewall[j]) == 0)
			drawsprite(--spritesortcnt);
		else
		{
				//Check to see if any sprites behind the masked wall...
			k = -1;
			gap = 0;
			for(i=spritesortcnt-2;i>=0;i--)
			{
#if USE_POLYMOST
				if (rendmode > 0)
					l = dxb1[j] <= (double)spritesx[i]/256.0 && (double)spritesx[i]/256.0 <= dxb2[j];
				else
#endif
					l = xb1[j] <= (spritesx[i]>>8) && (spritesx[i]>>8) <= xb2[j];
				if (l && spritewallfront(tspriteptr[i],(int)thewall[j]) == 0)
				{
					drawsprite(i);
					tspriteptr[i]->owner = -1;
					k = i;
					gap++;
				}
			}
			if (k >= 0)       //remove holes in sprite list
			{
				for(i=k;i<spritesortcnt;i++)
					if (tspriteptr[i]->owner >= 0)
					{
						if (i > k)
						{
							tspriteptr[k] = tspriteptr[i];
							spritesx[k] = spritesx[i];
							spritesy[k] = spritesy[i];
						}
						k++;
					}
				spritesortcnt -= gap;
			}

				//finally safe to draw the masked wall
			drawmaskwall(--maskwallcnt);
		}
	}
	while (spritesortcnt > 0) drawsprite(--spritesortcnt);
	while (maskwallcnt > 0) drawmaskwall(--maskwallcnt);

	enddrawing();	//}}}
}


//
// drawmapview
//
void drawmapview(int dax, int day, int zoome, short ang)
{
	walltype *wal;
	sectortype *sec;
	spritetype *spr;
	int tilenum, xoff, yoff, i, j, k, l, cosang, sinang, xspan, yspan;
	int xrepeat, yrepeat, x, y, x1, y1, x2, y2, x3, y3, x4, y4, bakx1, baky1;
	int s, w, ox, oy, startwall, cx1, cy1, cx2, cy2;
	int bakgxvect, bakgyvect, sortnum, gap, npoints;
	int xvect, yvect, xvect2, yvect2, daslope;

	beforedrawrooms = 0;

	clearbuf(&gotsector[0],(int)((numsectors+31)>>5),0L);

	cx1 = (windowx1<<12); cy1 = (windowy1<<12);
	cx2 = ((windowx2+1)<<12)-1; cy2 = ((windowy2+1)<<12)-1;
	zoome <<= 8;
	bakgxvect = divscale28(sintable[(1536-ang)&2047],zoome);
	bakgyvect = divscale28(sintable[(2048-ang)&2047],zoome);
	xvect = mulscale8(sintable[(2048-ang)&2047],zoome);
	yvect = mulscale8(sintable[(1536-ang)&2047],zoome);
	xvect2 = mulscale16(xvect,yxaspect);
	yvect2 = mulscale16(yvect,yxaspect);

	sortnum = 0;

	begindrawing();	//{{{

	for(s=0,sec=&sector[s];s<numsectors;s++,sec++)
		if (show2dsector[s>>3]&pow2char[s&7])
		{
			npoints = 0; i = 0;
			startwall = sec->wallptr;
#if 0
			for(w=sec->wallnum,wal=&wall[startwall];w>0;w--,wal++)
			{
				ox = wal->x - dax; oy = wal->y - day;
				x = dmulscale16(ox,xvect,-oy,yvect) + (xdim<<11);
				y = dmulscale16(oy,xvect2,ox,yvect2) + (ydim<<11);
				i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
				rx1[npoints] = x;
				ry1[npoints] = y;
				xb1[npoints] = wal->point2 - startwall;
				npoints++;
			}
#else
			j = startwall; l = 0;
			for(w=sec->wallnum,wal=&wall[startwall];w>0;w--,wal++,j++)
			{
				k = lastwall(j);
				if ((k > j) && (npoints > 0)) { xb1[npoints-1] = l; l = npoints; } //overwrite point2
					//wall[k].x wal->x wall[wal->point2].x
					//wall[k].y wal->y wall[wal->point2].y
				if (!dmulscale1(wal->x-wall[k].x,wall[wal->point2].y-wal->y,-(wal->y-wall[k].y),wall[wal->point2].x-wal->x)) continue;
				ox = wal->x - dax; oy = wal->y - day;
				x = dmulscale16(ox,xvect,-oy,yvect) + (xdim<<11);
				y = dmulscale16(oy,xvect2,ox,yvect2) + (ydim<<11);
				i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
				rx1[npoints] = x;
				ry1[npoints] = y;
				xb1[npoints] = npoints+1;
				npoints++;
			}
			if (npoints > 0) xb1[npoints-1] = l; //overwrite point2
#endif
			if ((i&0xf0) != 0xf0) continue;
			bakx1 = rx1[0]; baky1 = mulscale16(ry1[0]-(ydim<<11),xyaspect)+(ydim<<11);
			if (i&0x0f)
			{
				npoints = clippoly(npoints,i);
				if (npoints < 3) continue;
			}

				//Collect floor sprites to draw
			for(i=headspritesect[s];i>=0;i=nextspritesect[i])
				if ((sprite[i].cstat&48) == 32)
				{
					if ((sprite[i].cstat&(64+8)) == (64+8)) continue;
					tsprite[sortnum++].owner = i;
				}

			gotsector[s>>3] |= pow2char[s&7];

			globalorientation = (int)sec->floorstat;
			if ((globalorientation&1) != 0) continue;

			globalpal = sec->floorpal;
			if (palookup[sec->floorpal] != globalpalwritten)
			{
				globalpalwritten = palookup[sec->floorpal];
				if (!globalpalwritten) globalpalwritten = palookup[0];	// JBF: fixes null-pointer crash
				setpalookupaddress(globalpalwritten);
			}
			globalpicnum = sec->floorpicnum;
			if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
			setgotpic(globalpicnum);
			if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0)) continue;
			if ((picanm[globalpicnum]&192) != 0) globalpicnum += animateoffs((short)globalpicnum,s);
			if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
			globalbufplc = waloff[globalpicnum];
			globalshade = max(min(sec->floorshade,numpalookups-1),0);
			globvis = globalhisibility;
			if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
			globalpolytype = 0;
			if ((globalorientation&64) == 0)
			{
				globalposx = dax; globalx1 = bakgxvect; globaly1 = bakgyvect;
				globalposy = day; globalx2 = bakgxvect; globaly2 = bakgyvect;
			}
			else
			{
				ox = wall[wall[startwall].point2].x - wall[startwall].x;
				oy = wall[wall[startwall].point2].y - wall[startwall].y;
				i = nsqrtasm(ox*ox+oy*oy); if (i == 0) continue;
				i = 1048576/i;
				globalx1 = mulscale10(dmulscale10(ox,bakgxvect,oy,bakgyvect),i);
				globaly1 = mulscale10(dmulscale10(ox,bakgyvect,-oy,bakgxvect),i);
				ox = (bakx1>>4)-(xdim<<7); oy = (baky1>>4)-(ydim<<7);
				globalposx = dmulscale28(-oy,globalx1,-ox,globaly1);
				globalposy = dmulscale28(-ox,globalx1,oy,globaly1);
				globalx2 = -globalx1;
				globaly2 = -globaly1;

				daslope = sector[s].floorheinum;
				i = nsqrtasm(daslope*daslope+16777216);
				globalposy = mulscale12(globalposy,i);
				globalx2 = mulscale12(globalx2,i);
				globaly2 = mulscale12(globaly2,i);
			}
			globalxshift = (8-(picsiz[globalpicnum]&15));
			globalyshift = (8-(picsiz[globalpicnum]>>4));
			if (globalorientation&8) {globalxshift++; globalyshift++; }

			sethlinesizes(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4,(void *)globalbufplc);

			if ((globalorientation&0x4) > 0)
			{
				i = globalposx; globalposx = -globalposy; globalposy = -i;
				i = globalx2; globalx2 = globaly1; globaly1 = i;
				i = globalx1; globalx1 = -globaly2; globaly2 = -i;
			}
			if ((globalorientation&0x10) > 0) globalx1 = -globalx1, globaly1 = -globaly1, globalposx = -globalposx;
			if ((globalorientation&0x20) > 0) globalx2 = -globalx2, globaly2 = -globaly2, globalposy = -globalposy;
			asm1 = (globaly1<<globalxshift);
			asm2 = (globalx2<<globalyshift);
			globalx1 <<= globalxshift;
			globaly2 <<= globalyshift;
			globalposx = (globalposx<<(20+globalxshift))+(((int)sec->floorxpanning)<<24);
			globalposy = (globalposy<<(20+globalyshift))-(((int)sec->floorypanning)<<24);

			fillpolygon(npoints);
		}

		//Sort sprite list
	gap = 1; while (gap < sortnum) gap = (gap<<1)+1;
	for(gap>>=1;gap>0;gap>>=1)
		for(i=0;i<sortnum-gap;i++)
			for(j=i;j>=0;j-=gap)
			{
				if (sprite[tsprite[j].owner].z <= sprite[tsprite[j+gap].owner].z) break;
				swapshort(&tsprite[j].owner,&tsprite[j+gap].owner);
			}

	for(s=sortnum-1;s>=0;s--)
	{
		spr = &sprite[tsprite[s].owner];
		if ((spr->cstat&48) == 32)
		{
			npoints = 0;

			tilenum = spr->picnum;
			xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
			yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
			if ((spr->cstat&4) > 0) xoff = -xoff;
			if ((spr->cstat&8) > 0) yoff = -yoff;

			k = spr->ang;
			cosang = sintable[(k+512)&2047]; sinang = sintable[k];
			xspan = tilesizx[tilenum]; xrepeat = spr->xrepeat;
			yspan = tilesizy[tilenum]; yrepeat = spr->yrepeat;

			ox = ((xspan>>1)+xoff)*xrepeat; oy = ((yspan>>1)+yoff)*yrepeat;
			x1 = spr->x + mulscale16(sinang,ox) + mulscale16(cosang,oy);
			y1 = spr->y + mulscale16(sinang,oy) - mulscale16(cosang,ox);
			l = xspan*xrepeat;
			x2 = x1 - mulscale16(sinang,l);
			y2 = y1 + mulscale16(cosang,l);
			l = yspan*yrepeat;
			k = -mulscale16(cosang,l); x3 = x2+k; x4 = x1+k;
			k = -mulscale16(sinang,l); y3 = y2+k; y4 = y1+k;

			xb1[0] = 1; xb1[1] = 2; xb1[2] = 3; xb1[3] = 0;
			npoints = 4;

			i = 0;

			ox = x1 - dax; oy = y1 - day;
			x = dmulscale16(ox,xvect,-oy,yvect) + (xdim<<11);
			y = dmulscale16(oy,xvect2,ox,yvect2) + (ydim<<11);
			i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
			rx1[0] = x; ry1[0] = y;

			ox = x2 - dax; oy = y2 - day;
			x = dmulscale16(ox,xvect,-oy,yvect) + (xdim<<11);
			y = dmulscale16(oy,xvect2,ox,yvect2) + (ydim<<11);
			i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
			rx1[1] = x; ry1[1] = y;

			ox = x3 - dax; oy = y3 - day;
			x = dmulscale16(ox,xvect,-oy,yvect) + (xdim<<11);
			y = dmulscale16(oy,xvect2,ox,yvect2) + (ydim<<11);
			i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
			rx1[2] = x; ry1[2] = y;

			x = rx1[0]+rx1[2]-rx1[1];
			y = ry1[0]+ry1[2]-ry1[1];
			i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
			rx1[3] = x; ry1[3] = y;

			if ((i&0xf0) != 0xf0) continue;
			bakx1 = rx1[0]; baky1 = mulscale16(ry1[0]-(ydim<<11),xyaspect)+(ydim<<11);
			if (i&0x0f)
			{
				npoints = clippoly(npoints,i);
				if (npoints < 3) continue;
			}

			globalpicnum = spr->picnum;
			if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
			setgotpic(globalpicnum);
			if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0)) continue;
			if ((picanm[globalpicnum]&192) != 0) globalpicnum += animateoffs((short)globalpicnum,s);
			if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
			globalbufplc = waloff[globalpicnum];
			if ((sector[spr->sectnum].ceilingstat&1) > 0)
				globalshade = ((int)sector[spr->sectnum].ceilingshade);
			else
				globalshade = ((int)sector[spr->sectnum].floorshade);
			globalshade = max(min(globalshade+spr->shade+6,numpalookups-1),0);
			asm3 = (intptr_t)palookup[spr->pal]+(globalshade<<8);
			globvis = globalhisibility;
			if (sec->visibility != 0) globvis = mulscale4(globvis,(int)((unsigned char)(sec->visibility+16)));
			globalpolytype = ((spr->cstat&2)>>1)+1;

				//relative alignment stuff
			ox = x2-x1; oy = y2-y1;
			i = ox*ox+oy*oy; if (i == 0) continue; i = (65536*16384)/i;
			globalx1 = mulscale10(dmulscale10(ox,bakgxvect,oy,bakgyvect),i);
			globaly1 = mulscale10(dmulscale10(ox,bakgyvect,-oy,bakgxvect),i);
			ox = y1-y4; oy = x4-x1;
			i = ox*ox+oy*oy; if (i == 0) continue; i = (65536*16384)/i;
			globalx2 = mulscale10(dmulscale10(ox,bakgxvect,oy,bakgyvect),i);
			globaly2 = mulscale10(dmulscale10(ox,bakgyvect,-oy,bakgxvect),i);

			ox = picsiz[globalpicnum]; oy = ((ox>>4)&15); ox &= 15;
			if (pow2long[ox] != xspan)
			{
				ox++;
				globalx1 = mulscale(globalx1,xspan,ox);
				globaly1 = mulscale(globaly1,xspan,ox);
			}

			bakx1 = (bakx1>>4)-(xdim<<7); baky1 = (baky1>>4)-(ydim<<7);
			globalposx = dmulscale28(-baky1,globalx1,-bakx1,globaly1);
			globalposy = dmulscale28(bakx1,globalx2,-baky1,globaly2);

			if ((spr->cstat&2) == 0)
				msethlineshift(ox,oy);
			else {
				if (spr->cstat&512) settransreverse(); else settransnormal();
				tsethlineshift(ox,oy);
			}

			if ((spr->cstat&0x4) > 0) globalx1 = -globalx1, globaly1 = -globaly1, globalposx = -globalposx;
			asm1 = (globaly1<<2); globalx1 <<= 2; globalposx <<= (20+2);
			asm2 = (globalx2<<2); globaly2 <<= 2; globalposy <<= (20+2);

			globalorientation = ((spr->cstat&2)<<7) | ((spr->cstat&512)>>2);	// so polymost can get the translucency. ignored in software mode.
			fillpolygon(npoints);
		}
	}

	enddrawing();	//}}}
}


//
// loadboard
//
int loadboard(char *filename, char fromwhere, int *daposx, int *daposy, int *daposz,
			 short *daang, short *dacursectnum)
{
	short fil, i, numsprites;
	short maxsectors, maxwalls, maxsprites;

	i = strlen(filename)-1;
	if ((unsigned char)filename[i] == 255) { filename[i] = 0; fromwhere = 1; }	// JBF 20040119: "compatibility"
	if ((fil = kopen4load(filename,fromwhere)) == -1)
		{ mapversion = 7L; return(-1); }

	kread(fil,&mapversion,4); mapversion = B_LITTLE32(mapversion);
	if (mapversion == 7)
	{
		maxsectors = MAXSECTORSV7;
		maxwalls = MAXWALLSV7;
		maxsprites = MAXSPRITESV7;
	}
	else if (mapversion == 8)
	{
		maxsectors = MAXSECTORSV8;
		maxwalls = MAXWALLSV8;
		maxsprites = MAXSPRITESV8;
	}
	else
	{
		kclose(fil);
		return(-2);
	}

	/*
	// Enable this for doing map checksum tests
	clearbufbyte(&wall,   sizeof(wall),   0);
	clearbufbyte(&sector, sizeof(sector), 0);
	clearbufbyte(&sprite, sizeof(sprite), 0);
	*/

	initspritelists();

	clearbuf(&show2dsector[0],(int)((MAXSECTORS+3)>>5),0L);
	clearbuf(&show2dsprite[0],(int)((MAXSPRITES+3)>>5),0L);
	clearbuf(&show2dwall[0],(int)((MAXWALLS+3)>>5),0L);

	kread(fil,daposx,4); *daposx = B_LITTLE32(*daposx);
	kread(fil,daposy,4); *daposy = B_LITTLE32(*daposy);
	kread(fil,daposz,4); *daposz = B_LITTLE32(*daposz);
	kread(fil,daang,2);  *daang  = B_LITTLE16(*daang);
	kread(fil,dacursectnum,2); *dacursectnum = B_LITTLE16(*dacursectnum);

	kread(fil,&numsectors,2); numsectors = B_LITTLE16(numsectors);
	if (numsectors > maxsectors) { kclose(fil); return(-2); }
	kread(fil,&sector[0],sizeof(sectortype)*numsectors);
	for (i=numsectors-1; i>=0; i--) {
		sector[i].wallptr       = B_LITTLE16(sector[i].wallptr);
		sector[i].wallnum       = B_LITTLE16(sector[i].wallnum);
		sector[i].ceilingz      = B_LITTLE32(sector[i].ceilingz);
		sector[i].floorz        = B_LITTLE32(sector[i].floorz);
		sector[i].ceilingstat   = B_LITTLE16(sector[i].ceilingstat);
		sector[i].floorstat     = B_LITTLE16(sector[i].floorstat);
		sector[i].ceilingpicnum = B_LITTLE16(sector[i].ceilingpicnum);
		sector[i].ceilingheinum = B_LITTLE16(sector[i].ceilingheinum);
		sector[i].floorpicnum   = B_LITTLE16(sector[i].floorpicnum);
		sector[i].floorheinum   = B_LITTLE16(sector[i].floorheinum);
		sector[i].lotag         = B_LITTLE16(sector[i].lotag);
		sector[i].hitag         = B_LITTLE16(sector[i].hitag);
		sector[i].extra         = B_LITTLE16(sector[i].extra);
	}

	kread(fil,&numwalls,2); numwalls = B_LITTLE16(numwalls);
	if (numwalls > maxwalls) { kclose(fil); return(-2); }
	kread(fil,&wall[0],sizeof(walltype)*numwalls);
	for (i=numwalls-1; i>=0; i--) {
		wall[i].x          = B_LITTLE32(wall[i].x);
		wall[i].y          = B_LITTLE32(wall[i].y);
		wall[i].point2     = B_LITTLE16(wall[i].point2);
		wall[i].nextwall   = B_LITTLE16(wall[i].nextwall);
		wall[i].nextsector = B_LITTLE16(wall[i].nextsector);
		wall[i].cstat      = B_LITTLE16(wall[i].cstat);
		wall[i].picnum     = B_LITTLE16(wall[i].picnum);
		wall[i].overpicnum = B_LITTLE16(wall[i].overpicnum);
		wall[i].lotag      = B_LITTLE16(wall[i].lotag);
		wall[i].hitag      = B_LITTLE16(wall[i].hitag);
		wall[i].extra      = B_LITTLE16(wall[i].extra);
	}

	kread(fil,&numsprites,2); numsprites = B_LITTLE16(numsprites);
	if (numsprites > maxsprites) { kclose(fil); return(-2); }
	kread(fil,&sprite[0],sizeof(spritetype)*numsprites);
	for (i=numsprites-1; i>=0; i--) {
		sprite[i].x       = B_LITTLE32(sprite[i].x);
		sprite[i].y       = B_LITTLE32(sprite[i].y);
		sprite[i].z       = B_LITTLE32(sprite[i].z);
		sprite[i].cstat   = B_LITTLE16(sprite[i].cstat);
		sprite[i].picnum  = B_LITTLE16(sprite[i].picnum);
		sprite[i].sectnum = B_LITTLE16(sprite[i].sectnum);
		sprite[i].statnum = B_LITTLE16(sprite[i].statnum);
		sprite[i].ang     = B_LITTLE16(sprite[i].ang);
		sprite[i].owner   = B_LITTLE16(sprite[i].owner);
		sprite[i].xvel    = B_LITTLE16(sprite[i].xvel);
		sprite[i].yvel    = B_LITTLE16(sprite[i].yvel);
		sprite[i].zvel    = B_LITTLE16(sprite[i].zvel);
		sprite[i].lotag   = B_LITTLE16(sprite[i].lotag);
		sprite[i].hitag   = B_LITTLE16(sprite[i].hitag);
		sprite[i].extra   = B_LITTLE16(sprite[i].extra);
	}

	for(i=0;i<numsprites;i++) {
		if ((sprite[i].cstat & 48) == 48) sprite[i].cstat &= ~48;
		insertsprite(sprite[i].sectnum,sprite[i].statnum);
	}

		//Must be after loading sectors, etc!
	updatesector(*daposx,*daposy,dacursectnum);

	kclose(fil);

#if USE_POLYMOST && USE_OPENGL
	memset(spriteext, 0, sizeof(spriteext));
#endif
	guniqhudid = 0;

	return(0);
}


//
// loadboardv5/6
//
struct sectortypev5
{
	unsigned short wallptr, wallnum;
	short ceilingpicnum, floorpicnum;
	short ceilingheinum, floorheinum;
	int ceilingz, floorz;
	signed char ceilingshade, floorshade;
	unsigned char ceilingxpanning, floorxpanning;
	unsigned char ceilingypanning, floorypanning;
	unsigned char ceilingstat, floorstat;
	unsigned char ceilingpal, floorpal;
	unsigned char visibility;
	short lotag, hitag;
	short extra;
};
struct walltypev5
{
	int x, y;
	short point2;
	short picnum, overpicnum;
	signed char shade;
	short cstat;
	unsigned char xrepeat, yrepeat, xpanning, ypanning;
	short nextsector1, nextwall1;
	short nextsector2, nextwall2;
	short lotag, hitag;
	short extra;
};
struct spritetypev5
{
	int x, y, z;
	unsigned char cstat;
	signed char shade;
	unsigned char xrepeat, yrepeat;
	short picnum, ang, xvel, yvel, zvel, owner;
	short sectnum, statnum;
	short lotag, hitag;
	short extra;
};
struct sectortypev6
{
	unsigned short wallptr, wallnum;
	short ceilingpicnum, floorpicnum;
	short ceilingheinum, floorheinum;
	int ceilingz, floorz;
	signed char ceilingshade, floorshade;
	unsigned char ceilingxpanning, floorxpanning;
	unsigned char ceilingypanning, floorypanning;
	unsigned char ceilingstat, floorstat;
	unsigned char ceilingpal, floorpal;
	unsigned char visibility;
	short lotag, hitag, extra;
};
struct walltypev6
{
	int x, y;
	short point2, nextsector, nextwall;
	short picnum, overpicnum;
	signed char shade;
	unsigned char pal;
	short cstat;
	unsigned char xrepeat, yrepeat, xpanning, ypanning;
	short lotag, hitag, extra;
};
struct spritetypev6
{
	int x, y, z;
	short cstat;
	signed char shade;
	unsigned char pal, clipdist;
	unsigned char xrepeat, yrepeat;
	signed char xoffset, yoffset;
	short picnum, ang, xvel, yvel, zvel, owner;
	short sectnum, statnum;
	short lotag, hitag, extra;
};

static short sectorofwallv5(short theline)
{
	short i, startwall, endwall, sucksect;

	sucksect = -1;
	for(i=0;i<numsectors;i++)
	{
		startwall = sector[i].wallptr;
		endwall = startwall + sector[i].wallnum - 1;
		if ((theline >= startwall) && (theline <= endwall))
		{
			sucksect = i;
			break;
		}
	}
	return(sucksect);
}

static int readv5sect(int fil, struct sectortypev5 *sect)
{
	if (kread(fil, &sect->wallptr, 2) != 2) return -1;
	if (kread(fil, &sect->wallnum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingpicnum, 2) != 2) return -1;
	if (kread(fil, &sect->floorpicnum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingheinum, 2) != 2) return -1;
	if (kread(fil, &sect->floorheinum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingz, 4) != 4) return -1;
	if (kread(fil, &sect->floorz, 4) != 4) return -1;
	if (kread(fil, &sect->ceilingshade, 1) != 1) return -1;
	if (kread(fil, &sect->floorshade, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingxpanning, 1) != 1) return -1;
	if (kread(fil, &sect->floorxpanning, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingypanning, 1) != 1) return -1;
	if (kread(fil, &sect->floorypanning, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingstat, 1) != 1) return -1;
	if (kread(fil, &sect->floorstat, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingpal, 1) != 1) return -1;
	if (kread(fil, &sect->floorpal, 1) != 1) return -1;
	if (kread(fil, &sect->visibility, 1) != 1) return -1;
	if (kread(fil, &sect->lotag, 2) != 2) return -1;
	if (kread(fil, &sect->hitag, 2) != 2) return -1;
	if (kread(fil, &sect->extra, 2) != 2) return -1;

	sect->wallptr = B_LITTLE16(sect->wallptr);
	sect->wallnum = B_LITTLE16(sect->wallnum);
	sect->ceilingpicnum = B_LITTLE16(sect->ceilingpicnum);
	sect->floorpicnum = B_LITTLE16(sect->floorpicnum);
	sect->ceilingheinum = B_LITTLE16(sect->ceilingheinum);
	sect->floorheinum = B_LITTLE16(sect->floorheinum);
	sect->ceilingz = B_LITTLE32(sect->ceilingz);
	sect->floorz = B_LITTLE32(sect->floorz);
	sect->lotag = B_LITTLE16(sect->lotag);
	sect->hitag = B_LITTLE16(sect->hitag);
	sect->extra = B_LITTLE16(sect->extra);

	return 0;
}

static int writev5sect(int fil, struct sectortypev5 *sect)
{
	uint32_t tl;
	uint16_t ts;

	ts = B_LITTLE16(sect->wallptr); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->wallnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->ceilingpicnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->floorpicnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->ceilingheinum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->floorheinum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	tl = B_LITTLE32(sect->ceilingz); if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = B_LITTLE32(sect->floorz); if (Bwrite(fil, &tl, 4) != 4) return -1;
	if (Bwrite(fil, &sect->ceilingshade, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorshade, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingxpanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorxpanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingypanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorypanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingstat, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorstat, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingpal, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorpal, 1) != 1) return -1;
	if (Bwrite(fil, &sect->visibility, 1) != 1) return -1;
	ts = B_LITTLE16(sect->lotag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->hitag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->extra); if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

static void convertv5sectv6(struct sectortypev5 *from, struct sectortypev6 *to)
{
	to->wallptr = from->wallptr;
	to->wallnum = from->wallnum;
	to->ceilingpicnum = from->ceilingpicnum;
	to->floorpicnum = from->floorpicnum;
	to->ceilingheinum = from->ceilingheinum;
	to->floorheinum = from->floorheinum;
	to->ceilingz = from->ceilingz;
	to->floorz = from->floorz;
	to->ceilingshade = from->ceilingshade;
	to->floorshade = from->floorshade;
	to->ceilingxpanning = from->ceilingxpanning;
	to->floorxpanning = from->floorxpanning;
	to->ceilingypanning = from->ceilingypanning;
	to->floorypanning = from->floorypanning;
	to->ceilingstat = from->ceilingstat;
	to->floorstat = from->floorstat;
	to->ceilingpal = from->ceilingpal;
	to->floorpal = from->floorpal;
	to->visibility = from->visibility;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static void convertv6sectv5(struct sectortypev6 *from, struct sectortypev5 *to)
{
	to->wallptr = from->wallptr;
	to->wallnum = from->wallnum;
	to->ceilingpicnum = from->ceilingpicnum;
	to->floorpicnum = from->floorpicnum;
	to->ceilingheinum = from->ceilingheinum;
	to->floorheinum = from->floorheinum;
	to->ceilingz = from->ceilingz;
	to->floorz = from->floorz;
	to->ceilingshade = from->ceilingshade;
	to->floorshade = from->floorshade;
	to->ceilingxpanning = from->ceilingxpanning;
	to->floorxpanning = from->floorxpanning;
	to->ceilingypanning = from->ceilingypanning;
	to->floorypanning = from->floorypanning;
	to->ceilingstat = from->ceilingstat;
	to->floorstat = from->floorstat;
	to->ceilingpal = from->ceilingpal;
	to->floorpal = from->floorpal;
	to->visibility = from->visibility;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static int readv5wall(int fil, struct walltypev5 *wall)
{
	if (kread(fil, &wall->x, 4) != 4) return -1;
	if (kread(fil, &wall->y, 4) != 4) return -1;
	if (kread(fil, &wall->point2, 2) != 2) return -1;
	if (kread(fil, &wall->picnum, 2) != 2) return -1;
	if (kread(fil, &wall->overpicnum, 2) != 2) return -1;
	if (kread(fil, &wall->shade, 1) != 1) return -1;
	if (kread(fil, &wall->cstat, 2) != 2) return -1;
	if (kread(fil, &wall->xrepeat, 1) != 1) return -1;
	if (kread(fil, &wall->yrepeat, 1) != 1) return -1;
	if (kread(fil, &wall->xpanning, 1) != 1) return -1;
	if (kread(fil, &wall->ypanning, 1) != 1) return -1;
	if (kread(fil, &wall->nextsector1, 2) != 2) return -1;
	if (kread(fil, &wall->nextwall1, 2) != 2) return -1;
	if (kread(fil, &wall->nextsector2, 2) != 2) return -1;
	if (kread(fil, &wall->nextwall2, 2) != 2) return -1;
	if (kread(fil, &wall->lotag, 2) != 2) return -1;
	if (kread(fil, &wall->hitag, 2) != 2) return -1;
	if (kread(fil, &wall->extra, 2) != 2) return -1;

	wall->x = B_LITTLE32(wall->x);
	wall->y = B_LITTLE32(wall->y);
	wall->point2 = B_LITTLE16(wall->point2);
	wall->picnum = B_LITTLE16(wall->picnum);
	wall->overpicnum = B_LITTLE16(wall->overpicnum);
	wall->cstat = B_LITTLE16(wall->cstat);
	wall->nextsector1 = B_LITTLE16(wall->nextsector1);
	wall->nextwall1 = B_LITTLE16(wall->nextwall1);
	wall->nextsector2 = B_LITTLE16(wall->nextsector2);
	wall->nextwall2 = B_LITTLE16(wall->nextwall2);
	wall->lotag = B_LITTLE16(wall->lotag);
	wall->hitag = B_LITTLE16(wall->hitag);
	wall->extra = B_LITTLE16(wall->extra);

	return 0;
}

static int writev5wall(int fil, struct walltypev5 *wall)
{
	uint32_t tl;
	uint16_t ts;

	tl = B_LITTLE32(wall->x); if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = B_LITTLE32(wall->y); if (Bwrite(fil, &tl, 4) != 4) return -1;
	ts = B_LITTLE16(wall->point2); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->picnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->overpicnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &wall->shade, 1) != 1) return -1;
	ts = B_LITTLE16(wall->cstat); if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &wall->xrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &wall->yrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &wall->xpanning, 1) != 1) return -1;
	if (Bwrite(fil, &wall->ypanning, 1) != 1) return -1;
	ts = B_LITTLE16(wall->nextsector1); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->nextwall1); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->nextsector2); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->nextwall2); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->lotag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->hitag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->extra); if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

static void convertv5wallv6(struct walltypev5 *from, struct walltypev6 *to, int i)
{
	to->x = from->x;
	to->y = from->y;
	to->point2 = from->point2;
	to->nextsector = from->nextsector1;
	to->nextwall = from->nextwall1;
	to->picnum = from->picnum;
	to->overpicnum = from->overpicnum;
	to->shade = from->shade;
	to->pal = sector[sectorofwallv5((short)i)].floorpal;
	to->cstat = from->cstat;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xpanning = from->xpanning;
	to->ypanning = from->ypanning;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static void convertv6wallv5(struct walltypev6 *from, struct walltypev5 *to)
{
	to->x = from->x;
	to->y = from->y;
	to->point2 = from->point2;
	to->nextsector1 = from->nextsector;
	to->nextwall1 = from->nextwall;
	to->nextsector2 = -1;
	to->nextwall2 = -1;
	to->picnum = from->picnum;
	to->overpicnum = from->overpicnum;
	to->shade = from->shade;
	to->cstat = from->cstat;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xpanning = from->xpanning;
	to->ypanning = from->ypanning;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static int readv5sprite(int fil, struct spritetypev5 *spr)
{
	if (kread(fil, &spr->x, 4) != 4) return -1;
	if (kread(fil, &spr->y, 4) != 4) return -1;
	if (kread(fil, &spr->z, 4) != 4) return -1;
	if (kread(fil, &spr->cstat, 1) != 1) return -1;
	if (kread(fil, &spr->shade, 1) != 1) return -1;
	if (kread(fil, &spr->xrepeat, 1) != 1) return -1;
	if (kread(fil, &spr->yrepeat, 1) != 1) return -1;
	if (kread(fil, &spr->picnum, 2) != 2) return -1;
	if (kread(fil, &spr->ang, 2) != 2) return -1;
	if (kread(fil, &spr->xvel, 2) != 2) return -1;
	if (kread(fil, &spr->yvel, 2) != 2) return -1;
	if (kread(fil, &spr->zvel, 2) != 2) return -1;
	if (kread(fil, &spr->owner, 2) != 2) return -1;
	if (kread(fil, &spr->sectnum, 2) != 2) return -1;
	if (kread(fil, &spr->statnum, 2) != 2) return -1;
	if (kread(fil, &spr->lotag, 2) != 2) return -1;
	if (kread(fil, &spr->hitag, 2) != 2) return -1;
	if (kread(fil, &spr->extra, 2) != 2) return -1;

	spr->x = B_LITTLE32(spr->x);
	spr->y = B_LITTLE32(spr->y);
	spr->z = B_LITTLE32(spr->z);
	spr->picnum = B_LITTLE16(spr->picnum);
	spr->ang = B_LITTLE16(spr->ang);
	spr->xvel = B_LITTLE16(spr->xvel);
	spr->yvel = B_LITTLE16(spr->yvel);
	spr->zvel = B_LITTLE16(spr->zvel);
	spr->owner = B_LITTLE16(spr->owner);
	spr->sectnum = B_LITTLE16(spr->sectnum);
	spr->statnum = B_LITTLE16(spr->statnum);
	spr->lotag = B_LITTLE16(spr->lotag);
	spr->hitag = B_LITTLE16(spr->hitag);
	spr->extra = B_LITTLE16(spr->extra);

	return 0;
}

static int writev5sprite(int fil, struct spritetypev5 *spr)
{
	uint32_t tl;
	uint16_t ts;

	tl = B_LITTLE32(spr->x); if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = B_LITTLE32(spr->y); if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = B_LITTLE32(spr->z); if (Bwrite(fil, &tl, 4) != 4) return -1;
	if (Bwrite(fil, &spr->cstat, 1) != 1) return -1;
	if (Bwrite(fil, &spr->shade, 1) != 1) return -1;
	if (Bwrite(fil, &spr->xrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &spr->yrepeat, 1) != 1) return -1;
	ts = B_LITTLE16(spr->picnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->ang); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->xvel); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->yvel); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->zvel); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->owner); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->sectnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->statnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->lotag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->hitag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->extra); if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

static void convertv5sprv6(struct spritetypev5 *from, struct spritetypev6 *to)
{
	short j;
	to->x = from->x;
	to->y = from->y;
	to->z = from->z;
	to->cstat = from->cstat;
	to->shade = from->shade;

	j = from->sectnum;
	if ((sector[j].ceilingstat&1) > 0)
		to->pal = sector[j].ceilingpal;
	else
		to->pal = sector[j].floorpal;

	to->clipdist = 32;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xoffset = 0;
	to->yoffset = 0;
	to->picnum = from->picnum;
	to->ang = from->ang;
	to->xvel = from->xvel;
	to->yvel = from->yvel;
	to->zvel = from->zvel;
	to->owner = from->owner;
	to->sectnum = from->sectnum;
	to->statnum = from->statnum;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static void convertv6sprv5(struct spritetypev6 *from, struct spritetypev5 *to)
{
	to->x = from->x;
	to->y = from->y;
	to->z = from->z;
	to->cstat = (unsigned char)from->cstat;
	to->shade = from->shade;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->picnum = from->picnum;
	to->ang = from->ang;
	to->xvel = from->xvel;
	to->yvel = from->yvel;
	to->zvel = from->zvel;
	to->owner = from->owner;
	to->sectnum = from->sectnum;
	to->statnum = from->statnum;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static int readv6sect(int fil, struct sectortypev6 *sect)
{
	if (kread(fil, &sect->wallptr, 2) != 2) return -1;
	if (kread(fil, &sect->wallnum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingpicnum, 2) != 2) return -1;
	if (kread(fil, &sect->floorpicnum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingheinum, 2) != 2) return -1;
	if (kread(fil, &sect->floorheinum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingz, 4) != 4) return -1;
	if (kread(fil, &sect->floorz, 4) != 4) return -1;
	if (kread(fil, &sect->ceilingshade, 1) != 1) return -1;
	if (kread(fil, &sect->floorshade, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingxpanning, 1) != 1) return -1;
	if (kread(fil, &sect->floorxpanning, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingypanning, 1) != 1) return -1;
	if (kread(fil, &sect->floorypanning, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingstat, 1) != 1) return -1;
	if (kread(fil, &sect->floorstat, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingpal, 1) != 1) return -1;
	if (kread(fil, &sect->floorpal, 1) != 1) return -1;
	if (kread(fil, &sect->visibility, 1) != 1) return -1;
	if (kread(fil, &sect->lotag, 2) != 2) return -1;
	if (kread(fil, &sect->hitag, 2) != 2) return -1;
	if (kread(fil, &sect->extra, 2) != 2) return -1;

	sect->wallptr = B_LITTLE16(sect->wallptr);
	sect->wallnum = B_LITTLE16(sect->wallnum);
	sect->ceilingpicnum = B_LITTLE16(sect->ceilingpicnum);
	sect->floorpicnum = B_LITTLE16(sect->floorpicnum);
	sect->ceilingheinum = B_LITTLE16(sect->ceilingheinum);
	sect->floorheinum = B_LITTLE16(sect->floorheinum);
	sect->ceilingz = B_LITTLE32(sect->ceilingz);
	sect->floorz = B_LITTLE32(sect->floorz);
	sect->lotag = B_LITTLE16(sect->lotag);
	sect->hitag = B_LITTLE16(sect->hitag);
	sect->extra = B_LITTLE16(sect->extra);

	return 0;
}

static int writev6sect(int fil, struct sectortypev6 *sect)
{
	uint32_t tl;
	uint16_t ts;

	ts = B_LITTLE16(sect->wallptr); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->wallnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->ceilingpicnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->floorpicnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->ceilingheinum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->floorheinum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	tl = B_LITTLE32(sect->ceilingz); if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = B_LITTLE32(sect->floorz); if (Bwrite(fil, &tl, 4) != 4) return -1;
	if (Bwrite(fil, &sect->ceilingshade, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorshade, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingxpanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorxpanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingypanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorypanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingstat, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorstat, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingpal, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorpal, 1) != 1) return -1;
	if (Bwrite(fil, &sect->visibility, 1) != 1) return -1;
	ts = B_LITTLE16(sect->lotag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->hitag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(sect->extra); if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

static void convertv6sectv7(struct sectortypev6 *from, sectortype *to)
{
	to->ceilingz = from->ceilingz;
	to->floorz = from->floorz;
	to->wallptr = from->wallptr;
	to->wallnum = from->wallnum;
	to->ceilingpicnum = from->ceilingpicnum;
	to->ceilingheinum = max(min(((int)from->ceilingheinum)<<5,32767),-32768);
	if ((from->ceilingstat&2) == 0) to->ceilingheinum = 0;
	to->ceilingshade = from->ceilingshade;
	to->ceilingpal = from->ceilingpal;
	to->ceilingxpanning = from->ceilingxpanning;
	to->ceilingypanning = from->ceilingypanning;
	to->floorpicnum = from->floorpicnum;
	to->floorheinum = max(min(((int)from->floorheinum)<<5,32767),-32768);
	if ((from->floorstat&2) == 0) to->floorheinum = 0;
	to->floorshade = from->floorshade;
	to->floorpal = from->floorpal;
	to->floorxpanning = from->floorxpanning;
	to->floorypanning = from->floorypanning;
	to->ceilingstat = from->ceilingstat;
	to->floorstat = from->floorstat;
	to->visibility = from->visibility;
	to->filler = 0;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static void convertv7sectv6(sectortype *from, struct sectortypev6 *to)
{
	to->ceilingz = from->ceilingz;
	to->floorz = from->floorz;
	to->wallptr = from->wallptr;
	to->wallnum = from->wallnum;
	to->ceilingpicnum = from->ceilingpicnum;
	to->ceilingheinum = (((int)from->ceilingheinum)>>5);
	to->ceilingshade = from->ceilingshade;
	to->ceilingpal = from->ceilingpal;
	to->ceilingxpanning = from->ceilingxpanning;
	to->ceilingypanning = from->ceilingypanning;
	to->floorpicnum = from->floorpicnum;
	to->floorheinum = (((int)from->floorheinum)>>5);
	to->floorshade = from->floorshade;
	to->floorpal = from->floorpal;
	to->floorxpanning = from->floorxpanning;
	to->floorypanning = from->floorypanning;
	to->ceilingstat = from->ceilingstat;
	if (to->ceilingheinum == 0) to->ceilingstat &= ~2;
	to->floorstat = from->floorstat;
	if (to->floorheinum == 0) to->floorstat &= ~2;
	to->visibility = from->visibility;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static int readv6wall(int fil, struct walltypev6 *wall)
{
	if (kread(fil, &wall->x, 4) != 4) return -1;
	if (kread(fil, &wall->y, 4) != 4) return -1;
	if (kread(fil, &wall->point2, 2) != 2) return -1;
	if (kread(fil, &wall->nextsector, 2) != 2) return -1;
	if (kread(fil, &wall->nextwall, 2) != 2) return -1;
	if (kread(fil, &wall->picnum, 2) != 2) return -1;
	if (kread(fil, &wall->overpicnum, 2) != 2) return -1;
	if (kread(fil, &wall->shade, 1) != 1) return -1;
	if (kread(fil, &wall->pal, 1) != 1) return -1;
	if (kread(fil, &wall->cstat, 2) != 2) return -1;
	if (kread(fil, &wall->xrepeat, 1) != 1) return -1;
	if (kread(fil, &wall->yrepeat, 1) != 1) return -1;
	if (kread(fil, &wall->xpanning, 1) != 1) return -1;
	if (kread(fil, &wall->ypanning, 1) != 1) return -1;
	if (kread(fil, &wall->lotag, 2) != 2) return -1;
	if (kread(fil, &wall->hitag, 2) != 2) return -1;
	if (kread(fil, &wall->extra, 2) != 2) return -1;

	wall->x = B_LITTLE32(wall->x);
	wall->y = B_LITTLE32(wall->y);
	wall->point2 = B_LITTLE16(wall->point2);
	wall->nextsector = B_LITTLE16(wall->nextsector);
	wall->nextwall = B_LITTLE16(wall->nextwall);
	wall->picnum = B_LITTLE16(wall->picnum);
	wall->overpicnum = B_LITTLE16(wall->overpicnum);
	wall->cstat = B_LITTLE16(wall->cstat);
	wall->lotag = B_LITTLE16(wall->lotag);
	wall->hitag = B_LITTLE16(wall->hitag);
	wall->extra = B_LITTLE16(wall->extra);

	return 0;
}

static int writev6wall(int fil, struct walltypev6 *wall)
{
	uint32_t tl;
	uint16_t ts;

	tl = B_LITTLE32(wall->x); if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = B_LITTLE32(wall->y); if (Bwrite(fil, &tl, 4) != 4) return -1;
	ts = B_LITTLE16(wall->point2); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->nextsector); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->nextwall); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->picnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->overpicnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &wall->shade, 1) != 1) return -1;
	if (Bwrite(fil, &wall->pal, 1) != 1) return -1;
	ts = B_LITTLE16(wall->cstat); if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &wall->xrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &wall->yrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &wall->xpanning, 1) != 1) return -1;
	if (Bwrite(fil, &wall->ypanning, 1) != 1) return -1;
	ts = B_LITTLE16(wall->lotag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->hitag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(wall->extra); if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

static void convertv6wallv7(struct walltypev6 *from, walltype *to)
{
	to->x = from->x;
	to->y = from->y;
	to->point2 = from->point2;
	to->nextwall = from->nextwall;
	to->nextsector = from->nextsector;
	to->cstat = from->cstat;
	to->picnum = from->picnum;
	to->overpicnum = from->overpicnum;
	to->shade = from->shade;
	to->pal = from->pal;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xpanning = from->xpanning;
	to->ypanning = from->ypanning;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static void convertv7wallv6(walltype *from, struct walltypev6 *to)
{
	to->x = from->x;
	to->y = from->y;
	to->point2 = from->point2;
	to->nextwall = from->nextwall;
	to->nextsector = from->nextsector;
	to->cstat = from->cstat;
	to->picnum = from->picnum;
	to->overpicnum = from->overpicnum;
	to->shade = from->shade;
	to->pal = from->pal;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xpanning = from->xpanning;
	to->ypanning = from->ypanning;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static int readv6sprite(int fil, struct spritetypev6 *spr)
{
	if (kread(fil, &spr->x, 4) != 4) return -1;
	if (kread(fil, &spr->y, 4) != 4) return -1;
	if (kread(fil, &spr->z, 4) != 4) return -1;
	if (kread(fil, &spr->cstat, 2) != 2) return -1;
	if (kread(fil, &spr->shade, 1) != 1) return -1;
	if (kread(fil, &spr->pal, 1) != 1) return -1;
	if (kread(fil, &spr->clipdist, 1) != 1) return -1;
	if (kread(fil, &spr->xrepeat, 1) != 1) return -1;
	if (kread(fil, &spr->yrepeat, 1) != 1) return -1;
	if (kread(fil, &spr->xoffset, 1) != 1) return -1;
	if (kread(fil, &spr->yoffset, 1) != 1) return -1;
	if (kread(fil, &spr->picnum, 2) != 2) return -1;
	if (kread(fil, &spr->ang, 2) != 2) return -1;
	if (kread(fil, &spr->xvel, 2) != 2) return -1;
	if (kread(fil, &spr->yvel, 2) != 2) return -1;
	if (kread(fil, &spr->zvel, 2) != 2) return -1;
	if (kread(fil, &spr->owner, 2) != 2) return -1;
	if (kread(fil, &spr->sectnum, 2) != 2) return -1;
	if (kread(fil, &spr->statnum, 2) != 2) return -1;
	if (kread(fil, &spr->lotag, 2) != 2) return -1;
	if (kread(fil, &spr->hitag, 2) != 2) return -1;
	if (kread(fil, &spr->extra, 2) != 2) return -1;

	spr->x = B_LITTLE32(spr->x);
	spr->y = B_LITTLE32(spr->y);
	spr->z = B_LITTLE32(spr->z);
	spr->cstat = B_LITTLE16(spr->cstat);
	spr->picnum = B_LITTLE16(spr->picnum);
	spr->ang = B_LITTLE16(spr->ang);
	spr->xvel = B_LITTLE16(spr->xvel);
	spr->yvel = B_LITTLE16(spr->yvel);
	spr->zvel = B_LITTLE16(spr->zvel);
	spr->owner = B_LITTLE16(spr->owner);
	spr->sectnum = B_LITTLE16(spr->sectnum);
	spr->statnum = B_LITTLE16(spr->statnum);
	spr->lotag = B_LITTLE16(spr->lotag);
	spr->hitag = B_LITTLE16(spr->hitag);
	spr->extra = B_LITTLE16(spr->extra);

	return 0;
}

static int writev6sprite(int fil, struct spritetypev6 *spr)
{
	uint32_t tl;
	uint16_t ts;

	tl = B_LITTLE32(spr->x); if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = B_LITTLE32(spr->y); if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = B_LITTLE32(spr->z); if (Bwrite(fil, &tl, 4) != 4) return -1;
	ts = B_LITTLE16(spr->cstat); if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &spr->shade, 1) != 1) return -1;
	if (Bwrite(fil, &spr->pal, 1) != 1) return -1;
	if (Bwrite(fil, &spr->clipdist, 1) != 1) return -1;
	if (Bwrite(fil, &spr->xrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &spr->yrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &spr->xoffset, 1) != 1) return -1;
	if (Bwrite(fil, &spr->yoffset, 1) != 1) return -1;
	ts = B_LITTLE16(spr->picnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->ang); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->xvel); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->yvel); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->zvel); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->owner); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->sectnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->statnum); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->lotag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->hitag); if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = B_LITTLE16(spr->extra); if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

static void convertv6sprv7(struct spritetypev6 *from, spritetype *to)
{
	to->x = from->x;
	to->y = from->y;
	to->z = from->z;
	to->cstat = from->cstat;
	to->picnum = from->picnum;
	to->shade = from->shade;
	to->pal = from->pal;
	to->clipdist = from->clipdist;
	to->filler = 0;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xoffset = from->xoffset;
	to->yoffset = from->yoffset;
	to->sectnum = from->sectnum;
	to->statnum = from->statnum;
	to->ang = from->ang;
	to->owner = from->owner;
	to->xvel = from->xvel;
	to->yvel = from->yvel;
	to->zvel = from->zvel;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

static void convertv7sprv6(spritetype *from, struct spritetypev6 *to)
{
	to->x = from->x;
	to->y = from->y;
	to->z = from->z;
	to->cstat = from->cstat;
	to->picnum = from->picnum;
	to->shade = from->shade;
	to->pal = from->pal;
	to->clipdist = from->clipdist;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xoffset = from->xoffset;
	to->yoffset = from->yoffset;
	to->sectnum = from->sectnum;
	to->statnum = from->statnum;
	to->ang = from->ang;
	to->owner = from->owner;
	to->xvel = from->xvel;
	to->yvel = from->yvel;
	to->zvel = from->zvel;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

// Powerslave uses v6
// Witchaven 1 and TekWar use v5
int loadoldboard(char *filename, char fromwhere, int *daposx, int *daposy, int *daposz,
			 short *daang, short *dacursectnum)
{
	short fil, i, numsprites;
	struct sectortypev5 v5sect;
	struct walltypev5   v5wall;
	struct spritetypev5 v5spr;
	struct sectortypev6 v6sect;
	struct walltypev6   v6wall;
	struct spritetypev6 v6spr;

	i = strlen(filename)-1;
	if ((unsigned char)filename[i] == 255) { filename[i] = 0; fromwhere = 1; }	// JBF 20040119: "compatibility"
	if ((fil = kopen4load(filename,fromwhere)) == -1)
		{ mapversion = 5L; return(-1); }

	if (kread(fil,&mapversion,4) != 4) goto readerror;
	mapversion = B_LITTLE32(mapversion);
	if (mapversion != 5L && mapversion != 6L) {
		kclose(fil);
		return(-2);
	}

	initspritelists();

	clearbuf(&show2dsector[0],(int)((MAXSECTORS+3)>>5),0L);
	clearbuf(&show2dsprite[0],(int)((MAXSPRITES+3)>>5),0L);
	clearbuf(&show2dwall[0],(int)((MAXWALLS+3)>>5),0L);

	if (kread(fil,daposx,4) != 4) goto readerror;
	if (kread(fil,daposy,4) != 4) goto readerror;
	if (kread(fil,daposz,4) != 4) goto readerror;
	if (kread(fil,daang,2) != 2) goto readerror;
	if (kread(fil,dacursectnum,2) != 2) goto readerror;
	*daposx = B_LITTLE32(*daposx);
	*daposy = B_LITTLE32(*daposy);
	*daposz = B_LITTLE32(*daposz);
	*daang  = B_LITTLE16(*daang);
	*dacursectnum = B_LITTLE16(*dacursectnum);

	if (kread(fil,&numsectors,2) != 2) goto readerror;
	numsectors = B_LITTLE16(numsectors);
	if (numsectors > MAXSECTORS) {
		kclose(fil);
		return(-1);
	}

	for (i=0; i<numsectors; i++) {
		switch (mapversion) {
			case 5:
				if (readv5sect(fil,&v5sect)) goto readerror;
				convertv5sectv6(&v5sect,&v6sect);
				convertv6sectv7(&v6sect,&sector[i]);
				break;
			case 6:
				if (readv6sect(fil,&v6sect)) goto readerror;
				convertv6sectv7(&v6sect,&sector[i]);
				break;
		}
	}

	if (kread(fil,&numwalls,2) != 2) goto readerror;
	numwalls = B_LITTLE16(numwalls);
	if (numwalls > MAXWALLS) {
		kclose(fil);
		return(-1);
	}

	for (i=0; i<numwalls; i++) {
		switch (mapversion) {
			case 5:
				if (readv5wall(fil,&v5wall)) goto readerror;
				convertv5wallv6(&v5wall,&v6wall,i);
				convertv6wallv7(&v6wall,&wall[i]);
				break;
			case 6:
				if (readv6wall(fil,&v6wall)) goto readerror;
				convertv6wallv7(&v6wall,&wall[i]);
				break;
		}
	}

	if (kread(fil,&numsprites,2) != 2) goto readerror;
	numsprites = B_LITTLE16(numsprites);
	if (numsprites > MAXSPRITES) {
		kclose(fil);
		return(-1);
	}

	for (i=0; i<numsprites; i++) {
		switch (mapversion) {
			case 5:
				if (readv5sprite(fil,&v5spr)) goto readerror;
				convertv5sprv6(&v5spr,&v6spr);
				convertv6sprv7(&v6spr,&sprite[i]);
				break;
			case 6:
				if (readv6sprite(fil,&v6spr)) goto readerror;
				convertv6sprv7(&v6spr,&sprite[i]);
				break;
		}
	}

	for(i=0;i<numsprites;i++) {
		if ((sprite[i].cstat & 48) == 48) sprite[i].cstat &= ~48;
		insertsprite(sprite[i].sectnum,sprite[i].statnum);
	}

		//Must be after loading sectors, etc!
	updatesector(*daposx,*daposy,dacursectnum);

	kclose(fil);

#if USE_POLYMOST && USE_OPENGL
	memset(spriteext, 0, sizeof(spriteext));
#endif
	guniqhudid = 0;

	return(0);

readerror:
	kclose(fil);
	return -3;
}


//
// loadmaphack
//
#include "scriptfile.h"
int loadmaphack(char *filename)
{
#if USE_POLYMOST && USE_OPENGL
	static struct { char *text; int tokenid; } legaltokens[] = {
		{ "sprite", 0 },
		{ "angleoff", 1 },
		{ "angoff", 1 },
		{ "notmd2", 2 },
		{ "notmd3", 2 },
		{ "notmd", 2 },
		{ "nomd2anim", 3 },
		{ "nomd3anim", 3 },
		{ "nomdanim", 3 },
		{ NULL, -1 }
	};

	scriptfile *script;
	char *tok, *cmdtokptr;
	int i;
	int whichsprite = -1;

	script = scriptfile_fromfile(filename);
	if (!script) return -1;

	memset(spriteext, 0, sizeof(spriteext));

	while (1) {
		tok = scriptfile_gettoken(script);
		if (!tok) break;
		for (i=0;legaltokens[i].text;i++) if (!Bstrcasecmp(tok,legaltokens[i].text)) break;
		cmdtokptr = script->ltextptr;
		switch (legaltokens[i].tokenid) {
			case 0:		// sprite <xx>
				if (scriptfile_getnumber(script, &whichsprite)) break;

				if ((unsigned)whichsprite >= (unsigned)MAXSPRITES) {
					// sprite number out of range
					buildprintf("Sprite number out of range 0-%d on line %s:%d\n",
							MAXSPRITES-1,script->filename, scriptfile_getlinum(script,cmdtokptr));
					whichsprite = -1;
					break;
				}

				break;
			case 1:		// angoff <xx>
				{
					int ang;
					if (scriptfile_getnumber(script, &ang)) break;

					if (whichsprite < 0) {
						// no sprite directive preceeding
						buildprintf("Ignoring angle offset directive because of absent/invalid sprite number on line %s:%d\n",
							script->filename, scriptfile_getlinum(script,cmdtokptr));
						break;
					}
					spriteext[whichsprite].angoff = (short)ang;
				}
				break;
			case 2:      // notmd
				if (whichsprite < 0) {
					// no sprite directive preceeding
					buildprintf("Ignoring not-MD2/MD3 directive because of absent/invalid sprite number on line %s:%d\n",
							script->filename, scriptfile_getlinum(script,cmdtokptr));
					break;
				}
				spriteext[whichsprite].flags |= SPREXT_NOTMD;
				break;
			case 3:      // nomdanim
				if (whichsprite < 0) {
					// no sprite directive preceeding
					buildprintf("Ignoring no-MD2/MD3-anim directive because of absent/invalid sprite number on line %s:%d\n",
							script->filename, scriptfile_getlinum(script,cmdtokptr));
					break;
				}
				spriteext[whichsprite].flags |= SPREXT_NOMDANIM;
				break;
			default:
				// unrecognised token
				break;
		}
	}

	scriptfile_close(script);
#else
	(void)filename;
#endif //USE_POLYMOST && USE_OPENGL

	return 0;
}


//
// saveboard
//
int saveboard(char *filename, int *daposx, int *daposy, int *daposz,
			 short *daang, short *dacursectnum)
{
	short fil, i, j, numsprites, ts;
	int tl;
	sectortype tsect;
	walltype   twall;
	spritetype tspri;

	if ((fil = Bopen(filename,BO_BINARY|BO_TRUNC|BO_CREAT|BO_WRONLY,BS_IREAD|BS_IWRITE)) == -1)
		return(-1);

	numsprites = 0;
	for(j=0;j<MAXSTATUS;j++)
	{
		i = headspritestat[j];
		while (i != -1)
		{
			numsprites++;
			i = nextspritestat[i];
		}
	}

	if (numsectors > MAXSECTORSV7 || numwalls > MAXWALLSV7 || numsprites > MAXSPRITESV7)
		mapversion = 8;
	else
		mapversion = 7;
	tl = B_LITTLE32(mapversion);    if (Bwrite(fil,&tl,4) != 4) goto writeerror;

	tl = B_LITTLE32(*daposx);       if (Bwrite(fil,&tl,4) != 4) goto writeerror;
	tl = B_LITTLE32(*daposy);       if (Bwrite(fil,&tl,4) != 4) goto writeerror;
	tl = B_LITTLE32(*daposz);       if (Bwrite(fil,&tl,4) != 4) goto writeerror;
	ts = B_LITTLE16(*daang);        if (Bwrite(fil,&ts,2) != 2) goto writeerror;
	ts = B_LITTLE16(*dacursectnum); if (Bwrite(fil,&ts,2) != 2) goto writeerror;

	ts = B_LITTLE16(numsectors);    if (Bwrite(fil,&ts,2) != 2) goto writeerror;
	for (i=0; i<numsectors; i++) {
		tsect = sector[i];
		tsect.wallptr       = B_LITTLE16(tsect.wallptr);
		tsect.wallnum       = B_LITTLE16(tsect.wallnum);
		tsect.ceilingz      = B_LITTLE32(tsect.ceilingz);
		tsect.floorz        = B_LITTLE32(tsect.floorz);
		tsect.ceilingstat   = B_LITTLE16(tsect.ceilingstat);
		tsect.floorstat     = B_LITTLE16(tsect.floorstat);
		tsect.ceilingpicnum = B_LITTLE16(tsect.ceilingpicnum);
		tsect.ceilingheinum = B_LITTLE16(tsect.ceilingheinum);
		tsect.floorpicnum   = B_LITTLE16(tsect.floorpicnum);
		tsect.floorheinum   = B_LITTLE16(tsect.floorheinum);
		tsect.lotag         = B_LITTLE16(tsect.lotag);
		tsect.hitag         = B_LITTLE16(tsect.hitag);
		tsect.extra         = B_LITTLE16(tsect.extra);
		if (Bwrite(fil,&tsect,sizeof(sectortype)) != sizeof(sectortype)) goto writeerror;
	}

	ts = B_LITTLE16(numwalls);      if (Bwrite(fil,&ts,2) != 2) goto writeerror;
	for (i=0; i<numwalls; i++) {
		twall = wall[i];
		twall.x          = B_LITTLE32(twall.x);
		twall.y          = B_LITTLE32(twall.y);
		twall.point2     = B_LITTLE16(twall.point2);
		twall.nextwall   = B_LITTLE16(twall.nextwall);
		twall.nextsector = B_LITTLE16(twall.nextsector);
		twall.cstat      = B_LITTLE16(twall.cstat);
		twall.picnum     = B_LITTLE16(twall.picnum);
		twall.overpicnum = B_LITTLE16(twall.overpicnum);
		twall.lotag      = B_LITTLE16(twall.lotag);
		twall.hitag      = B_LITTLE16(twall.hitag);
		twall.extra      = B_LITTLE16(twall.extra);
		if (Bwrite(fil,&twall,sizeof(walltype)) != sizeof(walltype)) goto writeerror;
	}

	ts = B_LITTLE16(numsprites);    if (Bwrite(fil,&ts,2) != 2) goto writeerror;

	for(j=0;j<MAXSTATUS;j++)
	{
		i = headspritestat[j];
		while (i != -1)
		{
			tspri = sprite[i];
			tspri.x       = B_LITTLE32(tspri.x);
			tspri.y       = B_LITTLE32(tspri.y);
			tspri.z       = B_LITTLE32(tspri.z);
			tspri.cstat   = B_LITTLE16(tspri.cstat);
			tspri.picnum  = B_LITTLE16(tspri.picnum);
			tspri.sectnum = B_LITTLE16(tspri.sectnum);
			tspri.statnum = B_LITTLE16(tspri.statnum);
			tspri.ang     = B_LITTLE16(tspri.ang);
			tspri.owner   = B_LITTLE16(tspri.owner);
			tspri.xvel    = B_LITTLE16(tspri.xvel);
			tspri.yvel    = B_LITTLE16(tspri.yvel);
			tspri.zvel    = B_LITTLE16(tspri.zvel);
			tspri.lotag   = B_LITTLE16(tspri.lotag);
			tspri.hitag   = B_LITTLE16(tspri.hitag);
			tspri.extra   = B_LITTLE16(tspri.extra);
			if (Bwrite(fil,&tspri,sizeof(spritetype)) != sizeof(spritetype)) goto writeerror;
			i = nextspritestat[i];
		}
	}

	Bclose(fil);
	return(0);

writeerror:
	Bclose(fil);
	return(-1);
}


int saveoldboard(char *filename, int *daposx, int *daposy, int *daposz,
			 short *daang, short *dacursectnum)
{
	short fil, i, j, numsprites, ts;
	int tl;
	struct sectortypev5 v5sect;
	struct walltypev5   v5wall;
	struct spritetypev5 v5spr;
	struct sectortypev6 v6sect;
	struct walltypev6   v6wall;
	struct spritetypev6 v6spr;

	if (mapversion != 5 && mapversion != 6) {
		buildputs("saveoldboard: map version not 5 or 6\n");
		return -2;
	}

	numsprites = 0;
	for(j=0;j<MAXSTATUS;j++)
	{
		i = headspritestat[j];
		while (i != -1)
		{
			numsprites++;
			i = nextspritestat[i];
		}
	}

	switch (mapversion) {
		case 5:
			if (numsectors > MAXSECTORSV5 || numwalls > MAXWALLSV5 || numsprites > MAXSPRITESV5) {
				buildprintf("saveoldboard: too many sectors/walls/sprites for map version 5 (%d/%d, %d/%d, %d/%d)\n",
					numsectors, MAXSECTORSV5, numwalls, MAXWALLSV5, numsprites, MAXSPRITESV5);
				return -2;
			}
			break;
		case 6:
			if (numsectors > MAXSECTORSV6 || numwalls > MAXWALLSV6 || numsprites > MAXSPRITESV6) {
				buildprintf("saveoldboard: too many sectors/walls/sprites for map version 6 (%d/%d, %d/%d, %d/%d)\n",
					numsectors, MAXSECTORSV6, numwalls, MAXWALLSV6, numsprites, MAXSPRITESV6);
				return -2;
			}
			break;
	}

	if ((fil = Bopen(filename,BO_BINARY|BO_TRUNC|BO_CREAT|BO_WRONLY,BS_IREAD|BS_IWRITE)) == -1)
		return(-1);

	tl = B_LITTLE32(mapversion);
	if (Bwrite(fil,&tl,4) != 4) goto writeerror;

	tl = B_LITTLE32(*daposx);
	if (Bwrite(fil,&tl,4) != 4) goto writeerror;
	tl = B_LITTLE32(*daposy);
	if (Bwrite(fil,&tl,4) != 4) goto writeerror;
	tl = B_LITTLE32(*daposz);
	if (Bwrite(fil,&tl,4) != 4) goto writeerror;
	ts = B_LITTLE16(*daang);
	if (Bwrite(fil,&ts,2) != 2) goto writeerror;
	ts = B_LITTLE16(*dacursectnum);
	if (Bwrite(fil,&ts,2) != 2) goto writeerror;

	ts = B_LITTLE16(numsectors);
	if (Bwrite(fil,&ts,2) != 2) goto writeerror;
	for (i=0; i<numsectors; i++) {
		switch (mapversion) {
			case 6:
				convertv7sectv6(&sector[i], &v6sect);
				if (writev6sect(fil, &v6sect)) goto writeerror;
				break;
			case 5:
				convertv7sectv6(&sector[i], &v6sect);
				convertv6sectv5(&v6sect, &v5sect);
				if (writev5sect(fil, &v5sect)) goto writeerror;
				break;
		}
	}

	ts = B_LITTLE16(numwalls);
	if (Bwrite(fil,&ts,2) != 2) goto writeerror;
	for (i=0; i<numwalls; i++) {
		switch (mapversion) {
			case 6:
				convertv7wallv6(&wall[i], &v6wall);
				if (writev6wall(fil, &v6wall)) goto writeerror;
				break;
			case 5:
				convertv7wallv6(&wall[i], &v6wall);
				convertv6wallv5(&v6wall, &v5wall);
				if (writev5wall(fil, &v5wall)) goto writeerror;
				break;
		}
	}

	ts = B_LITTLE16(numsprites);
	if (Bwrite(fil,&ts,2) != 2) goto writeerror;
	for(j=0;j<MAXSTATUS;j++)
	{
		i = headspritestat[j];
		while (i != -1)
		{
			switch (mapversion) {
				case 6:
					convertv7sprv6(&sprite[i], &v6spr);
					if (writev6sprite(fil, &v6spr)) goto writeerror;
					break;
				case 5:
					convertv7sprv6(&sprite[i], &v6spr);
					convertv6sprv5(&v6spr, &v5spr);
					if (writev5sprite(fil, &v5spr)) goto writeerror;
					break;
			}
			i = nextspritestat[i];
		}
	}

	Bclose(fil);
	return(0);

writeerror:
	Bclose(fil);
	return -1;
}


//
// setgamemode
//
// JBF: davidoption now functions as a windowed-mode flag (0 == windowed, 1 == fullscreen)
extern char videomodereset;
int setgamemode(char davidoption, int daxdim, int daydim, int dabpp)
{
	int i, j;

	if ((qsetmode == 200) && (videomodereset == 0) &&
	    (davidoption == fullscreen) && (xdim == daxdim) && (ydim == daydim) && (bpp == dabpp))
		return(0);

	strcpy(kensmessage,"!!!! BUILD engine&tools programmed by Ken Silverman of E.G. RI.  (c) Copyright 1995 Ken Silverman.  Summary:  BUILD = Ken. !!!!");

#if USE_POLYMOST && USE_OPENGL
	int oldbpp = bpp;
#endif
	//bytesperline is set in this function
	if (setvideomode(daxdim,daydim,dabpp,davidoption) < 0) return(-1);

	// it's possible the previous call protected our code sections again
	makeasmwriteable();

#if USE_POLYMOST && USE_OPENGL
	if (dabpp > 8) rendmode = 3;	// GL renderer
	else if (dabpp == 8 && oldbpp != 8) rendmode = 0;	// going from GL to software activates classic
#endif

	xdim = xres; ydim = yres;

	// determine the corrective factor for pixel-squareness. Build
	// is built around the non-square pixels of Mode 13h, so to get
	// things back square on VGA screens, things need to be "compressed"
	// vertically a little.
	widescreen = 0;
	tallscreen = 0;
	if ((xdim == 320 && ydim == 200) || (xdim == 640 && ydim == 400)) {
		pixelaspect = 65536;
	} else {
		int ratio = divscale16(ydim*320, xdim*240);
		pixelaspect = divscale16(240*320L,320*200L);

		if (ratio < 65536) {
			widescreen = 1;
		} else if (ratio > 65536) {
			tallscreen = 1;

			// let tall screens (eg. 1280x1024) stretch the 2D elements
			// vertically a little until something better is thought of
			pixelaspect = divscale16(ydim*320L,xdim*200L);
		}
	}

	j = ydim*4*sizeof(int);  //Leave room for horizlookup&horizlookup2

	if (lookups != NULL) { kfree((void *)lookups); lookups = NULL; }
	if ((lookups = kmalloc(j<<1)) == NULL) {
		engineerrstr = "Failed to allocate lookups memory";
		return -1;
	}

	horizlookup = (int *)(lookups);
	horizlookup2 = (int *)((intptr_t)lookups+j);
	horizycent = ((ydim*4)>>1);

	//Force drawrooms to call dosetaspect & recalculate stuff
	oxyaspect = oxdimen = oviewingrange = -1;

	setvlinebpl(bytesperline);
	j = 0;
	for(i=0;i<=ydim;i++) ylookup[i] = j, j += bytesperline;

	setview(0L,0L,xdim-1,ydim-1);

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == 3) {
		polymost_glreset();
		polymost_glinit();
	}
#endif

	setbrightness(curbrightness,&palette[0],0);
	clearallviews(0L);

	if (searchx < 0) { searchx = halfxdimen; searchy = (ydimen>>1); }

	qsetmode = 200;

	//memset(ratelimitlast,0,sizeof(ratelimitlast));
	//ratelimitn = 0;

	return(0);
}


//
// nextpage
//
void nextpage(void)
{
	int i;
	permfifotype *per;

	//char snotbuf[32];
	//j = 0; k = 0;
	//for(i=0;i<4096;i++)
	//   if (waloff[i] != 0)
	//   {
	//      sprintf(snotbuf,"%ld-%ld",i,tilesizx[i]*tilesizy[i]);
	//      printext256((j>>5)*40+32,(j&31)*6,walock[i]>>3,-1,snotbuf,1);
	//      k += tilesizx[i]*tilesizy[i];
	//      j++;
	//   }
	//sprintf(snotbuf,"Total: %ld",k);
	//printext256((j>>5)*40+32,(j&31)*6,31,-1,snotbuf,1);

	switch(qsetmode)
	{
		case 200:
			begindrawing();	//{{{
			for(i=permtail;i!=permhead;i=((i+1)&(MAXPERMS-1)))
			{
				per = &permfifo[i];
				if ((per->pagesleft > 0) && (per->pagesleft <= numpages))
					dorotatesprite(per->sx,per->sy,per->z,per->a,per->picnum,
							per->dashade,per->dapalnum,per->dastat,
							per->cx1,per->cy1,per->cx2,per->cy2,per->uniqid);
			}
			enddrawing();	//}}}

			OSD_Draw();
#if USE_POLYMOST
			polymost_nextpage();
#endif

			if (captureatnextpage) {
				screencapture(NULL, captureatnextpage);
				captureatnextpage = 0;
			}

			showframe();
#if USE_POLYMOST && USE_OPENGL
			polymost_aftershowframe();
#endif

			/*
			if (ratelimit > 0) {
				int delaytime;
				unsigned int thisticks, thist;

				ratelimitlast[ ratelimitn++ & 31 ] = thist = getusecticks();
				delaytime = 0;
				if (ratelimitn >= 32) {
					for (i=1;i<32;i++) delaytime += ratelimitlast[i] - ratelimitlast[i-1];
					delaytime = (1000000/ratelimit) - (delaytime/31);
				}
#ifdef _WIN32
				while (delaytime > 0) {
					Sleep(1);
					thisticks = getusecticks();
					delaytime -= (thisticks - thist);
					thist = thisticks;
				}
#endif
			}
			*/

			begindrawing();	//{{{
			for(i=permtail;i!=permhead;i=((i+1)&(MAXPERMS-1)))
			{
				per = &permfifo[i];
				if (per->pagesleft >= 130)
					dorotatesprite(per->sx,per->sy,per->z,per->a,per->picnum,
										per->dashade,per->dapalnum,per->dastat,
										per->cx1,per->cy1,per->cx2,per->cy2,per->uniqid);

				if ((per->pagesleft&127) && (numpages < 127)) per->pagesleft--;
				if (((per->pagesleft&127) == 0) && (i == permtail))
					permtail = ((permtail+1)&(MAXPERMS-1));
			}
			enddrawing();	//}}}
			break;

		case 350:
		case 480:
			break;
	}
	faketimerhandler();

	if ((totalclock >= lastageclock+8) || (totalclock < lastageclock))
		{ lastageclock = totalclock; agecache(); }

#if USE_POLYMOST && USE_OPENGL
	omdtims = mdtims; mdtims = getticks();
	if (((unsigned int)(mdtims-omdtims)) > 10000) omdtims = mdtims;
#endif

	beforedrawrooms = 1;
	numframes++;
}


//
// loadpics
//
int loadpics(char *filename, int askedsize)
{
	int offscount, localtilestart, localtileend, dasiz;
	short fil, i, j, k;

	Bstrcpy(artfilename,filename);

	for(i=0;i<MAXTILES;i++)
	{
		tilesizx[i] = 0;
		tilesizy[i] = 0;
		picanm[i] = 0L;
	}

	artsize = 0L;

	numtilefiles = 0;
	do
	{
		k = numtilefiles;

		artfilename[7] = (k%10)+48;
		artfilename[6] = ((k/10)%10)+48;
		artfilename[5] = ((k/100)%10)+48;
		if ((fil = kopen4load(artfilename,0)) != -1)
		{
			kread(fil,&artversion,4); artversion = B_LITTLE32(artversion);
			if (artversion != 1) {
				buildprintf("loadpics(): Invalid art file version in %s\n", artfilename);
				return(-1);
			}
			kread(fil,&numtiles,4);       numtiles       = B_LITTLE32(numtiles);
			kread(fil,&localtilestart,4); localtilestart = B_LITTLE32(localtilestart);
			kread(fil,&localtileend,4);   localtileend   = B_LITTLE32(localtileend);
			kread(fil,&tilesizx[localtilestart],(localtileend-localtilestart+1)<<1);
			kread(fil,&tilesizy[localtilestart],(localtileend-localtilestart+1)<<1);
			kread(fil,&picanm[localtilestart],(localtileend-localtilestart+1)<<2);
			for (i=localtilestart; i<=localtileend; i++) {
				tilesizx[i] = B_LITTLE16(tilesizx[i]);
				tilesizy[i] = B_LITTLE16(tilesizy[i]);
				picanm[i]   = B_LITTLE32(picanm[i]);
			}

			offscount = 4+4+4+4+((localtileend-localtilestart+1)<<3);
			for(i=localtilestart;i<=localtileend;i++)
			{
				tilefilenum[i] = k;
				tilefileoffs[i] = offscount;
				dasiz = (int)(tilesizx[i]*tilesizy[i]);
				offscount += dasiz;
				artsize += ((dasiz+15)&0xfffffff0);
			}
			kclose(fil);

			numtilefiles++;
		}
	}
	while (k != numtilefiles);

	clearbuf(&gotpic[0],(int)((MAXTILES+31)>>5),0L);

	//try dpmi_DETERMINEMAXREALALLOC!

	//cachesize = min((int)((Bgetsysmemsize()/100)*60),max(artsize,askedsize));
	if (Bgetsysmemsize() <= (unsigned int)askedsize)
		cachesize = (Bgetsysmemsize()/100)*60;
	else
		cachesize = askedsize;
	while ((pic = kmalloc(cachesize)) == NULL)
	{
		cachesize -= 65536L;
		if (cachesize < 65536) return(-1);
	}
	initcache(pic, cachesize);

	for(i=0;i<MAXTILES;i++)
	{
		j = 15;
		while ((j > 1) && (pow2long[j] > tilesizx[i])) j--;
		picsiz[i] = ((unsigned char)j);
		j = 15;
		while ((j > 1) && (pow2long[j] > tilesizy[i])) j--;
		picsiz[i] += ((unsigned char)(j<<4));
	}

	artfil = -1;
	artfilnum = -1;
	artfilplc = 0L;

	return(0);
}


//
// loadtile
//
char cachedebug = 0;
void loadtile(short tilenume)
{
	char *ptr;
	int i, dasiz;

	if ((unsigned)tilenume >= (unsigned)MAXTILES) return;
	dasiz = tilesizx[tilenume]*tilesizy[tilenume];
	if (dasiz <= 0) return;

	i = tilefilenum[tilenume];
	if (i != artfilnum)
	{
		if (artfil != -1) kclose(artfil);
		artfilnum = i;
		artfilplc = 0L;

		artfilename[7] = (i%10)+48;
		artfilename[6] = ((i/10)%10)+48;
		artfilename[5] = ((i/100)%10)+48;
		artfil = kopen4load(artfilename,0);
		faketimerhandler();
	}

	if (cachedebug) buildprintf("Tile:%d\n",tilenume);

	if (waloff[tilenume] == 0)
	{
		walock[tilenume] = 199;
		allocache((void **)&waloff[tilenume],dasiz,&walock[tilenume]);
	}

	if (artfilplc != tilefileoffs[tilenume])
	{
		klseek(artfil,tilefileoffs[tilenume]-artfilplc,BSEEK_CUR);
		faketimerhandler();
	}
	ptr = (char *)waloff[tilenume];
	kread(artfil,ptr,dasiz);
	faketimerhandler();
	artfilplc = tilefileoffs[tilenume]+dasiz;
}


//
// allocatepermanenttile
//
intptr_t allocatepermanenttile(short tilenume, int xsiz, int ysiz)
{
	int j, dasiz;

	if ((xsiz <= 0) || (ysiz <= 0) || ((unsigned)tilenume >= (unsigned)MAXTILES))
		return(0);

	dasiz = xsiz*ysiz;

	walock[tilenume] = 255;
	allocache((void **)&waloff[tilenume],dasiz,&walock[tilenume]);

	tilesizx[tilenume] = xsiz;
	tilesizy[tilenume] = ysiz;
	picanm[tilenume] = 0;

	j = 15; while ((j > 1) && (pow2long[j] > xsiz)) j--;
	picsiz[tilenume] = ((unsigned char)j);
	j = 15; while ((j > 1) && (pow2long[j] > ysiz)) j--;
	picsiz[tilenume] += ((unsigned char)(j<<4));

	return(waloff[tilenume]);
}


//
// copytilepiece
//
void copytilepiece(int tilenume1, int sx1, int sy1, int xsiz, int ysiz,
		  int tilenume2, int sx2, int sy2)
{
	unsigned char *ptr1, *ptr2, dat;
	int xsiz1, ysiz1, xsiz2, ysiz2, i, j, x1, y1, x2, y2;

	xsiz1 = tilesizx[tilenume1]; ysiz1 = tilesizy[tilenume1];
	xsiz2 = tilesizx[tilenume2]; ysiz2 = tilesizy[tilenume2];
	if ((xsiz1 > 0) && (ysiz1 > 0) && (xsiz2 > 0) && (ysiz2 > 0))
	{
		if (waloff[tilenume1] == 0) loadtile(tilenume1);
		if (waloff[tilenume2] == 0) loadtile(tilenume2);

		x1 = sx1;
		for(i=0;i<xsiz;i++)
		{
			y1 = sy1;
			for(j=0;j<ysiz;j++)
			{
				x2 = sx2+i;
				y2 = sy2+j;
				if ((x2 >= 0) && (y2 >= 0) && (x2 < xsiz2) && (y2 < ysiz2))
				{
					ptr1 = (unsigned char *)(waloff[tilenume1] + x1*ysiz1 + y1);
					ptr2 = (unsigned char *)(waloff[tilenume2] + x2*ysiz2 + y2);
					dat = *ptr1;
					if (dat != 255)
						*ptr2 = *ptr1;
				}

				y1++; if (y1 >= ysiz1) y1 = 0;
			}
			x1++; if (x1 >= xsiz1) x1 = 0;
		}
	}
}


//
// qloadkvx
//
int qloadkvx(int voxindex, char *filename)
{
	int i, fil, dasiz, lengcnt, lengtot;
	unsigned char *ptr;

	if ((fil = kopen4load(filename,0)) == -1) return -1;

	lengcnt = 0;
	lengtot = kfilelength(fil);

	for(i=0;i<MAXVOXMIPS;i++)
	{
		kread(fil,&dasiz,4); dasiz = B_LITTLE32(dasiz);
			//Must store filenames to use cacheing system :(
		voxlock[voxindex][i] = 200;
		allocache((void **)&voxoff[voxindex][i],dasiz,&voxlock[voxindex][i]);
		ptr = (unsigned char *)voxoff[voxindex][i];
		kread(fil,ptr,dasiz);

		lengcnt += dasiz+4;
		if (lengcnt >= lengtot-768) break;
	}
	kclose(fil);

#if USE_POLYMOST && USE_OPENGL
	if (voxmodels[voxindex]) {
		voxfree(voxmodels[voxindex]);
		voxmodels[voxindex] = NULL;
	}
	voxmodels[voxindex] = voxload(filename);
#endif
	return 0;
}


//
// clipinsidebox
//
int clipinsidebox(int x, int y, short wallnum, int walldist)
{
	walltype *wal;
	int x1, y1, x2, y2, r;

	r = (walldist<<1);
	wal = &wall[wallnum];     x1 = wal->x+walldist-x; y1 = wal->y+walldist-y;
	wal = &wall[wal->point2]; x2 = wal->x+walldist-x; y2 = wal->y+walldist-y;

	if ((x1 < 0) && (x2 < 0)) return(0);
	if ((y1 < 0) && (y2 < 0)) return(0);
	if ((x1 >= r) && (x2 >= r)) return(0);
	if ((y1 >= r) && (y2 >= r)) return(0);

	x2 -= x1; y2 -= y1;
	if (x2*(walldist-y1) >= y2*(walldist-x1))  //Front
	{
		if (x2 > 0) x2 *= (0-y1); else x2 *= (r-y1);
		if (y2 > 0) y2 *= (r-x1); else y2 *= (0-x1);
		return(x2 < y2);
	}
	if (x2 > 0) x2 *= (r-y1); else x2 *= (0-y1);
	if (y2 > 0) y2 *= (0-x1); else y2 *= (r-x1);
	return((x2 >= y2)<<1);
}


//
// clipinsideboxline
//
int clipinsideboxline(int x, int y, int x1, int y1, int x2, int y2, int walldist)
{
	int r;

	r = (walldist<<1);

	x1 += walldist-x; x2 += walldist-x;
	if ((x1 < 0) && (x2 < 0)) return(0);
	if ((x1 >= r) && (x2 >= r)) return(0);

	y1 += walldist-y; y2 += walldist-y;
	if ((y1 < 0) && (y2 < 0)) return(0);
	if ((y1 >= r) && (y2 >= r)) return(0);

	x2 -= x1; y2 -= y1;
	if (x2*(walldist-y1) >= y2*(walldist-x1))  //Front
	{
		if (x2 > 0) x2 *= (0-y1); else x2 *= (r-y1);
		if (y2 > 0) y2 *= (r-x1); else y2 *= (0-x1);
		return(x2 < y2);
	}
	if (x2 > 0) x2 *= (r-y1); else x2 *= (0-y1);
	if (y2 > 0) y2 *= (0-x1); else y2 *= (r-x1);
	return((x2 >= y2)<<1);
}


//
// inside
//
int inside(int x, int y, short sectnum)
{
	walltype *wal;
	int i, x1, y1, x2, y2;
	unsigned int cnt;

	if ((sectnum < 0) || (sectnum >= numsectors)) return(-1);

	cnt = 0;
	wal = &wall[sector[sectnum].wallptr];
	i = sector[sectnum].wallnum;
	do
	{
		y1 = wal->y-y; y2 = wall[wal->point2].y-y;
		if ((y1^y2) < 0)
		{
			x1 = wal->x-x; x2 = wall[wal->point2].x-x;
			if ((x1^x2) >= 0) cnt ^= x1; else cnt ^= (x1*y2-x2*y1)^y2;
		}
		wal++; i--;
	} while (i);
	return(cnt>>31);
}


//
// getangle
//
int getangle(int xvect, int yvect)
{
	if ((xvect|yvect) == 0) return(0);
	if (xvect == 0) return(512+((yvect<0)<<10));
	if (yvect == 0) return(((xvect<0)<<10));
	if (xvect == yvect) return(256+((xvect<0)<<10));
	if (xvect == -yvect) return(768+((xvect>0)<<10));
	if (klabs(xvect) > klabs(yvect))
		return(((radarang[640+scale(160,yvect,xvect)]>>6)+((xvect<0)<<10))&2047);
	return(((radarang[640-scale(160,xvect,yvect)]>>6)+512+((yvect<0)<<10))&2047);
}


//
// ksqrt
//
int ksqrt(int num)
{
	return(nsqrtasm(num));
}


//
// krecip
//
int krecip(int num)
{
	return(krecipasm(num));
}


//
// setsprite
//
int setsprite(short spritenum, int newx, int newy, int newz)
{
	short tempsectnum;

	sprite[spritenum].x = newx;
	sprite[spritenum].y = newy;
	sprite[spritenum].z = newz;

	tempsectnum = sprite[spritenum].sectnum;
	updatesector(newx,newy,&tempsectnum);
	if (tempsectnum < 0)
		return(-1);
	if (tempsectnum != sprite[spritenum].sectnum)
		changespritesect(spritenum,tempsectnum);

	return(0);
}

//
// setspritez
//
int setspritez(short spritenum, int newx, int newy, int newz)
{
	short tempsectnum;

	sprite[spritenum].x = newx;
	sprite[spritenum].y = newy;
	sprite[spritenum].z = newz;

	tempsectnum = sprite[spritenum].sectnum;
	updatesectorz(newx,newy,newz,&tempsectnum);
	if (tempsectnum < 0)
		return(-1);
	if (tempsectnum != sprite[spritenum].sectnum)
		changespritesect(spritenum,tempsectnum);

	return(0);
}


//
// insertsprite
//
int insertsprite(short sectnum, short statnum)
{
	insertspritestat(statnum);
	return(insertspritesect(sectnum));
}


//
// deletesprite
//
int deletesprite(short spritenum)
{
	deletespritestat(spritenum);
	return(deletespritesect(spritenum));
}


//
// changespritesect
//
int changespritesect(short spritenum, short newsectnum)
{
	if ((newsectnum < 0) || (newsectnum > MAXSECTORS)) return(-1);
	if (sprite[spritenum].sectnum == newsectnum) return(0);
	if (sprite[spritenum].sectnum == MAXSECTORS) return(-1);
	if (deletespritesect(spritenum) < 0) return(-1);
	insertspritesect(newsectnum);
	return(0);
}


//
// changespritestat
//
int changespritestat(short spritenum, short newstatnum)
{
	if ((newstatnum < 0) || (newstatnum > MAXSTATUS)) return(-1);
	if (sprite[spritenum].statnum == newstatnum) return(0);
	if (sprite[spritenum].statnum == MAXSTATUS) return(-1);
	if (deletespritestat(spritenum) < 0) return(-1);
	insertspritestat(newstatnum);
	return(0);
}


//
// nextsectorneighborz
//
int nextsectorneighborz(short sectnum, int thez, short topbottom, short direction)
{
	walltype *wal;
	int i, testz, nextz;
	short sectortouse;

	if (direction == 1) nextz = 0x7fffffff; else nextz = 0x80000000;

	sectortouse = -1;

	wal = &wall[sector[sectnum].wallptr];
	i = sector[sectnum].wallnum;
	do
	{
		if (wal->nextsector >= 0)
		{
			if (topbottom == 1)
			{
				testz = sector[wal->nextsector].floorz;
				if (direction == 1)
				{
					if ((testz > thez) && (testz < nextz))
					{
						nextz = testz;
						sectortouse = wal->nextsector;
					}
				}
				else
				{
					if ((testz < thez) && (testz > nextz))
					{
						nextz = testz;
						sectortouse = wal->nextsector;
					}
				}
			}
			else
			{
				testz = sector[wal->nextsector].ceilingz;
				if (direction == 1)
				{
					if ((testz > thez) && (testz < nextz))
					{
						nextz = testz;
						sectortouse = wal->nextsector;
					}
				}
				else
				{
					if ((testz < thez) && (testz > nextz))
					{
						nextz = testz;
						sectortouse = wal->nextsector;
					}
				}
			}
		}
		wal++;
		i--;
	} while (i != 0);

	return(sectortouse);
}


//
// cansee
//
int cansee(int x1, int y1, int z1, short sect1, int x2, int y2, int z2, short sect2)
{
	sectortype *sec;
	walltype *wal, *wal2;
	int i, cnt, nexts, x, y, z, cz, fz, dasectnum, dacnt, danum;
	int x21, y21, z21, x31, y31, x34, y34, bot, t;

	if ((x1 == x2) && (y1 == y2)) return(sect1 == sect2);

	x21 = x2-x1; y21 = y2-y1; z21 = z2-z1;

	clipsectorlist[0] = sect1; danum = 1;
	for(dacnt=0;dacnt<danum;dacnt++)
	{
		dasectnum = clipsectorlist[dacnt]; sec = &sector[dasectnum];
		for(cnt=sec->wallnum,wal=&wall[sec->wallptr];cnt>0;cnt--,wal++)
		{
			wal2 = &wall[wal->point2];
			x31 = wal->x-x1; x34 = wal->x-wal2->x;
			y31 = wal->y-y1; y34 = wal->y-wal2->y;

			bot = y21*x34-x21*y34; if (bot <= 0) continue;
			t = y21*x31-x21*y31; if ((unsigned)t >= (unsigned)bot) continue;
			t = y31*x34-x31*y34; if ((unsigned)t >= (unsigned)bot) continue;

			nexts = wal->nextsector;
			if ((nexts < 0) || (wal->cstat&32)) return(0);

			t = divscale24(t,bot);
			x = x1 + mulscale24(x21,t);
			y = y1 + mulscale24(y21,t);
			z = z1 + mulscale24(z21,t);

			getzsofslope((short)dasectnum,x,y,&cz,&fz);
			if ((z <= cz) || (z >= fz)) return(0);
			getzsofslope((short)nexts,x,y,&cz,&fz);
			if ((z <= cz) || (z >= fz)) return(0);

			for(i=danum-1;i>=0;i--) if (clipsectorlist[i] == nexts) break;
			if (i < 0) clipsectorlist[danum++] = nexts;
		}
	}
	for(i=danum-1;i>=0;i--) if (clipsectorlist[i] == sect2) return(1);
	return(0);
}


//
// hitscan
//
int hitscan(int xs, int ys, int zs, short sectnum, int vx, int vy, int vz,
	short *hitsect, short *hitwall, short *hitsprite,
	int *hitx, int *hity, int *hitz, unsigned int cliptype)
{
	sectortype *sec;
	walltype *wal, *wal2;
	spritetype *spr;
	int z, zz, x1, y1=0, z1=0, x2, y2, x3, y3, x4, y4, intx, inty, intz;
	int topt, topu, bot, dist, offx, offy, cstat;
	int i, j, k, l, tilenum, xoff, yoff, dax, day, daz, daz2;
	int ang, cosang, sinang, xspan, yspan, xrepeat, yrepeat;
	int dawalclipmask, dasprclipmask;
	short tempshortcnt, tempshortnum, dasector, startwall, endwall;
	short nextsector;
	unsigned char clipyou;

	*hitsect = -1; *hitwall = -1; *hitsprite = -1;
	if (sectnum < 0) return(-1);

	*hitx = hitscangoalx; *hity = hitscangoaly;

	dawalclipmask = (cliptype&65535);
	dasprclipmask = (cliptype>>16);

	clipsectorlist[0] = sectnum;
	tempshortcnt = 0; tempshortnum = 1;
	do
	{
		dasector = clipsectorlist[tempshortcnt]; sec = &sector[dasector];

		x1 = 0x7fffffff;
		if (sec->ceilingstat&2)
		{
			wal = &wall[sec->wallptr]; wal2 = &wall[wal->point2];
			dax = wal2->x-wal->x; day = wal2->y-wal->y;
			i = nsqrtasm(dax*dax+day*day); if (i == 0) continue;
			i = divscale15(sec->ceilingheinum,i);
			dax *= i; day *= i;

			j = (vz<<8)-dmulscale15(dax,vy,-day,vx);
			if (j != 0)
			{
				i = ((sec->ceilingz-zs)<<8)+dmulscale15(dax,ys-wal->y,-day,xs-wal->x);
				if (((i^j) >= 0) && ((klabs(i)>>1) < klabs(j)))
				{
					i = divscale30(i,j);
					x1 = xs + mulscale30(vx,i);
					y1 = ys + mulscale30(vy,i);
					z1 = zs + mulscale30(vz,i);
				}
			}
		}
		else if ((vz < 0) && (zs >= sec->ceilingz))
		{
			z1 = sec->ceilingz; i = z1-zs;
			if ((klabs(i)>>1) < -vz)
			{
				i = divscale30(i,vz);
				x1 = xs + mulscale30(vx,i);
				y1 = ys + mulscale30(vy,i);
			}
		}
		if ((x1 != 0x7fffffff) && (klabs(x1-xs)+klabs(y1-ys) < klabs((*hitx)-xs)+klabs((*hity)-ys)))
			if (inside(x1,y1,dasector) != 0)
			{
				*hitsect = dasector; *hitwall = -1; *hitsprite = -1;
				*hitx = x1; *hity = y1; *hitz = z1;
			}

		x1 = 0x7fffffff;
		if (sec->floorstat&2)
		{
			wal = &wall[sec->wallptr]; wal2 = &wall[wal->point2];
			dax = wal2->x-wal->x; day = wal2->y-wal->y;
			i = nsqrtasm(dax*dax+day*day); if (i == 0) continue;
			i = divscale15(sec->floorheinum,i);
			dax *= i; day *= i;

			j = (vz<<8)-dmulscale15(dax,vy,-day,vx);
			if (j != 0)
			{
				i = ((sec->floorz-zs)<<8)+dmulscale15(dax,ys-wal->y,-day,xs-wal->x);
				if (((i^j) >= 0) && ((klabs(i)>>1) < klabs(j)))
				{
					i = divscale30(i,j);
					x1 = xs + mulscale30(vx,i);
					y1 = ys + mulscale30(vy,i);
					z1 = zs + mulscale30(vz,i);
				}
			}
		}
		else if ((vz > 0) && (zs <= sec->floorz))
		{
			z1 = sec->floorz; i = z1-zs;
			if ((klabs(i)>>1) < vz)
			{
				i = divscale30(i,vz);
				x1 = xs + mulscale30(vx,i);
				y1 = ys + mulscale30(vy,i);
			}
		}
		if ((x1 != 0x7fffffff) && (klabs(x1-xs)+klabs(y1-ys) < klabs((*hitx)-xs)+klabs((*hity)-ys)))
			if (inside(x1,y1,dasector) != 0)
			{
				*hitsect = dasector; *hitwall = -1; *hitsprite = -1;
				*hitx = x1; *hity = y1; *hitz = z1;
			}

		startwall = sec->wallptr; endwall = startwall + sec->wallnum;
		for(z=startwall,wal=&wall[startwall];z<endwall;z++,wal++)
		{
			wal2 = &wall[wal->point2];
			x1 = wal->x; y1 = wal->y; x2 = wal2->x; y2 = wal2->y;

			if ((x1-xs)*(y2-ys) < (x2-xs)*(y1-ys)) continue;
			if (rintersect(xs,ys,zs,vx,vy,vz,x1,y1,x2,y2,&intx,&inty,&intz) == 0) continue;

			if (klabs(intx-xs)+klabs(inty-ys) >= klabs((*hitx)-xs)+klabs((*hity)-ys)) continue;

			nextsector = wal->nextsector;
			if ((nextsector < 0) || (wal->cstat&dawalclipmask))
			{
				*hitsect = dasector; *hitwall = z; *hitsprite = -1;
				*hitx = intx; *hity = inty; *hitz = intz;
				continue;
			}
			getzsofslope(nextsector,intx,inty,&daz,&daz2);
			if ((intz <= daz) || (intz >= daz2))
			{
				*hitsect = dasector; *hitwall = z; *hitsprite = -1;
				*hitx = intx; *hity = inty; *hitz = intz;
				continue;
			}

			for(zz=tempshortnum-1;zz>=0;zz--)
				if (clipsectorlist[zz] == nextsector) break;
			if (zz < 0) clipsectorlist[tempshortnum++] = nextsector;
		}

		for(z=headspritesect[dasector];z>=0;z=nextspritesect[z])
		{
			spr = &sprite[z];
			cstat = spr->cstat;
#if USE_POLYMOST
			if (!hitallsprites)
#endif
			if ((cstat&dasprclipmask) == 0) continue;

			x1 = spr->x; y1 = spr->y; z1 = spr->z;
			switch(cstat&48)
			{
				case 0:
					topt = vx*(x1-xs) + vy*(y1-ys); if (topt <= 0) continue;
					bot = vx*vx + vy*vy; if (bot == 0) continue;

					intz = zs+scale(vz,topt,bot);

					i = (tilesizy[spr->picnum]*spr->yrepeat<<2);
					if (cstat&128) z1 += (i>>1);
					if (picanm[spr->picnum]&0x00ff0000) z1 -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
					if ((intz > z1) || (intz < z1-i)) continue;
					topu = vx*(y1-ys) - vy*(x1-xs);

					offx = scale(vx,topu,bot);
					offy = scale(vy,topu,bot);
					dist = offx*offx + offy*offy;
					i = tilesizx[spr->picnum]*spr->xrepeat; i *= i;
					if (dist > (i>>7)) continue;
					intx = xs + scale(vx,topt,bot);
					inty = ys + scale(vy,topt,bot);

					if (klabs(intx-xs)+klabs(inty-ys) > klabs((*hitx)-xs)+klabs((*hity)-ys)) continue;

					*hitsect = dasector; *hitwall = -1; *hitsprite = z;
					*hitx = intx; *hity = inty; *hitz = intz;
					break;
				case 16:
						//These lines get the 2 points of the rotated sprite
						//Given: (x1, y1) starts out as the center point
					tilenum = spr->picnum;
					xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
					if ((cstat&4) > 0) xoff = -xoff;
					k = spr->ang; l = spr->xrepeat;
					dax = sintable[k&2047]*l; day = sintable[(k+1536)&2047]*l;
					l = tilesizx[tilenum]; k = (l>>1)+xoff;
					x1 -= mulscale16(dax,k); x2 = x1+mulscale16(dax,l);
					y1 -= mulscale16(day,k); y2 = y1+mulscale16(day,l);

					if ((cstat&64) != 0)   //back side of 1-way sprite
						if ((x1-xs)*(y2-ys) < (x2-xs)*(y1-ys)) continue;

					if (rintersect(xs,ys,zs,vx,vy,vz,x1,y1,x2,y2,&intx,&inty,&intz) == 0) continue;

					if (klabs(intx-xs)+klabs(inty-ys) > klabs((*hitx)-xs)+klabs((*hity)-ys)) continue;

					k = ((tilesizy[spr->picnum]*spr->yrepeat)<<2);
					if (cstat&128) daz = spr->z+(k>>1); else daz = spr->z;
					if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
					if ((intz < daz) && (intz > daz-k))
					{
						*hitsect = dasector; *hitwall = -1; *hitsprite = z;
						*hitx = intx; *hity = inty; *hitz = intz;
					}
					break;
				case 32:
					if (vz == 0) continue;
					intz = z1;
					if (((intz-zs)^vz) < 0) continue;
					if ((cstat&64) != 0)
						if ((zs > intz) == ((cstat&8)==0)) continue;

					intx = xs+scale(intz-zs,vx,vz);
					inty = ys+scale(intz-zs,vy,vz);

					if (klabs(intx-xs)+klabs(inty-ys) > klabs((*hitx)-xs)+klabs((*hity)-ys)) continue;

					tilenum = spr->picnum;
					xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
					yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
					if ((cstat&4) > 0) xoff = -xoff;
					if ((cstat&8) > 0) yoff = -yoff;

					ang = spr->ang;
					cosang = sintable[(ang+512)&2047]; sinang = sintable[ang];
					xspan = tilesizx[tilenum]; xrepeat = spr->xrepeat;
					yspan = tilesizy[tilenum]; yrepeat = spr->yrepeat;

					dax = ((xspan>>1)+xoff)*xrepeat; day = ((yspan>>1)+yoff)*yrepeat;
					x1 += dmulscale16(sinang,dax,cosang,day)-intx;
					y1 += dmulscale16(sinang,day,-cosang,dax)-inty;
					l = xspan*xrepeat;
					x2 = x1 - mulscale16(sinang,l);
					y2 = y1 + mulscale16(cosang,l);
					l = yspan*yrepeat;
					k = -mulscale16(cosang,l); x3 = x2+k; x4 = x1+k;
					k = -mulscale16(sinang,l); y3 = y2+k; y4 = y1+k;

					clipyou = 0;
					if ((y1^y2) < 0)
					{
						if ((x1^x2) < 0) clipyou ^= (x1*y2<x2*y1)^(y1<y2);
						else if (x1 >= 0) clipyou ^= 1;
					}
					if ((y2^y3) < 0)
					{
						if ((x2^x3) < 0) clipyou ^= (x2*y3<x3*y2)^(y2<y3);
						else if (x2 >= 0) clipyou ^= 1;
					}
					if ((y3^y4) < 0)
					{
						if ((x3^x4) < 0) clipyou ^= (x3*y4<x4*y3)^(y3<y4);
						else if (x3 >= 0) clipyou ^= 1;
					}
					if ((y4^y1) < 0)
					{
						if ((x4^x1) < 0) clipyou ^= (x4*y1<x1*y4)^(y4<y1);
						else if (x4 >= 0) clipyou ^= 1;
					}

					if (clipyou != 0)
					{
						*hitsect = dasector; *hitwall = -1; *hitsprite = z;
						*hitx = intx; *hity = inty; *hitz = intz;
					}
					break;
			}
		}
		tempshortcnt++;
	} while (tempshortcnt < tempshortnum);
	return(0);
}


//
// neartag
//
int neartag(int xs, int ys, int zs, short sectnum, short ange, short *neartagsector, short *neartagwall,
	short *neartagsprite, int *neartaghitdist, int neartagrange, unsigned char tagsearch)
{
	walltype *wal, *wal2;
	spritetype *spr;
	int i, z, zz, xe, ye, ze, x1, y1, z1, x2, y2, intx, inty, intz;
	int topt, topu, bot, dist, offx, offy, vx, vy, vz;
	short tempshortcnt, tempshortnum, dasector, startwall, endwall;
	short nextsector, good;

	*neartagsector = -1; *neartagwall = -1; *neartagsprite = -1;
	*neartaghitdist = 0;

	if (sectnum < 0) return(0);
	if ((tagsearch < 1) || (tagsearch > 3)) return(0);

	vx = mulscale14(sintable[(ange+2560)&2047],neartagrange); xe = xs+vx;
	vy = mulscale14(sintable[(ange+2048)&2047],neartagrange); ye = ys+vy;
	vz = 0; ze = 0;

	clipsectorlist[0] = sectnum;
	tempshortcnt = 0; tempshortnum = 1;

	do
	{
		dasector = clipsectorlist[tempshortcnt];

		startwall = sector[dasector].wallptr;
		endwall = startwall + sector[dasector].wallnum - 1;
		for(z=startwall,wal=&wall[startwall];z<=endwall;z++,wal++)
		{
			wal2 = &wall[wal->point2];
			x1 = wal->x; y1 = wal->y; x2 = wal2->x; y2 = wal2->y;

			nextsector = wal->nextsector;

			good = 0;
			if (nextsector >= 0)
			{
				if ((tagsearch&1) && sector[nextsector].lotag) good |= 1;
				if ((tagsearch&2) && sector[nextsector].hitag) good |= 1;
			}
			if ((tagsearch&1) && wal->lotag) good |= 2;
			if ((tagsearch&2) && wal->hitag) good |= 2;

			if ((good == 0) && (nextsector < 0)) continue;
			if ((x1-xs)*(y2-ys) < (x2-xs)*(y1-ys)) continue;

			if (lintersect(xs,ys,zs,xe,ye,ze,x1,y1,x2,y2,&intx,&inty,&intz) == 1)
			{
				if (good != 0)
				{
					if (good&1) *neartagsector = nextsector;
					if (good&2) *neartagwall = z;
					*neartaghitdist = dmulscale14(intx-xs,sintable[(ange+2560)&2047],inty-ys,sintable[(ange+2048)&2047]);
					xe = intx; ye = inty; ze = intz;
				}
				if (nextsector >= 0)
				{
					for(zz=tempshortnum-1;zz>=0;zz--)
						if (clipsectorlist[zz] == nextsector) break;
					if (zz < 0) clipsectorlist[tempshortnum++] = nextsector;
				}
			}
		}

		for(z=headspritesect[dasector];z>=0;z=nextspritesect[z])
		{
			spr = &sprite[z];

			good = 0;
			if ((tagsearch&1) && spr->lotag) good |= 1;
			if ((tagsearch&2) && spr->hitag) good |= 1;
			if (good != 0)
			{
				x1 = spr->x; y1 = spr->y; z1 = spr->z;

				topt = vx*(x1-xs) + vy*(y1-ys);
				if (topt > 0)
				{
					bot = vx*vx + vy*vy;
					if (bot != 0)
					{
						intz = zs+scale(vz,topt,bot);
						i = tilesizy[spr->picnum]*spr->yrepeat;
						if (spr->cstat&128) z1 += (i<<1);
						if (picanm[spr->picnum]&0x00ff0000) z1 -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
						if ((intz <= z1) && (intz >= z1-(i<<2)))
						{
							topu = vx*(y1-ys) - vy*(x1-xs);

							offx = scale(vx,topu,bot);
							offy = scale(vy,topu,bot);
							dist = offx*offx + offy*offy;
							i = (tilesizx[spr->picnum]*spr->xrepeat); i *= i;
							if (dist <= (i>>7))
							{
								intx = xs + scale(vx,topt,bot);
								inty = ys + scale(vy,topt,bot);
								if (klabs(intx-xs)+klabs(inty-ys) < klabs(xe-xs)+klabs(ye-ys))
								{
									*neartagsprite = z;
									*neartaghitdist = dmulscale14(intx-xs,sintable[(ange+2560)&2047],inty-ys,sintable[(ange+2048)&2047]);
									xe = intx;
									ye = inty;
									ze = intz;
								}
							}
						}
					}
				}
			}
		}

		tempshortcnt++;
	} while (tempshortcnt < tempshortnum);
	return(0);
}


//
// dragpoint
//
void dragpoint(short pointhighlight, int dax, int day)
{
	short cnt, tempshort;

	wall[pointhighlight].x = dax;
	wall[pointhighlight].y = day;

	cnt = MAXWALLS;
	tempshort = pointhighlight;    //search points CCW
	do
	{
		if (wall[tempshort].nextwall >= 0)
		{
			tempshort = wall[wall[tempshort].nextwall].point2;
			wall[tempshort].x = dax;
			wall[tempshort].y = day;
		}
		else
		{
			tempshort = pointhighlight;    //search points CW if not searched all the way around
			do
			{
				if (wall[lastwall(tempshort)].nextwall >= 0)
				{
					tempshort = wall[lastwall(tempshort)].nextwall;
					wall[tempshort].x = dax;
					wall[tempshort].y = day;
				}
				else
				{
					break;
				}
				cnt--;
			}
			while ((tempshort != pointhighlight) && (cnt > 0));
			break;
		}
		cnt--;
	}
	while ((tempshort != pointhighlight) && (cnt > 0));
}


//
// lastwall
//
int lastwall(short point)
{
	int i, j, cnt;

	if ((point > 0) && (wall[point-1].point2 == point)) return(point-1);
	i = point;
	cnt = MAXWALLS;
	do
	{
		j = wall[i].point2;
		if (j == point) return(i);
		i = j;
		cnt--;
	} while (cnt > 0);
	return(point);
}



#define addclipline(dax1, day1, dax2, day2, daoval)      \
{                                                        \
	if (clipnum < MAXCLIPNUM) { \
	clipit[clipnum].x1 = dax1; clipit[clipnum].y1 = day1; \
	clipit[clipnum].x2 = dax2; clipit[clipnum].y2 = day2; \
	clipobjectval[clipnum] = daoval;                      \
	clipnum++;                                            \
	}                           \
}                                                        \

int clipmoveboxtracenum = 3;

//
// clipmove
//
int clipmove (int *x, int *y, int *z, short *sectnum,
		 int xvect, int yvect,
		 int walldist, int ceildist, int flordist, unsigned int cliptype)
{
	walltype *wal, *wal2;
	spritetype *spr;
	sectortype *sec, *sec2;
	int i, j, templong1, templong2;
	int oxvect, oyvect, goalx, goaly, intx, inty, lx, ly, retval;
	int k, l, clipsectcnt, startwall, endwall, cstat, dasect;
	int x1, y1, x2, y2, cx, cy, rad, xmin, ymin, xmax, ymax, daz, daz2;
	int bsz, dax, day, xoff, yoff, xspan, yspan, cosang, sinang, tilenum;
	int xrepeat, yrepeat, gx, gy, dx, dy, dasprclipmask, dawalclipmask;
	int hitwall, cnt, clipyou;

	if (((xvect|yvect) == 0) || (*sectnum < 0)) return(0);
	retval = 0;

	oxvect = xvect;
	oyvect = yvect;

	goalx = (*x) + (xvect>>14);
	goaly = (*y) + (yvect>>14);


	clipnum = 0;

	cx = (((*x)+goalx)>>1);
	cy = (((*y)+goaly)>>1);
		//Extra walldist for sprites on sector lines
	gx = goalx-(*x); gy = goaly-(*y);
	rad = nsqrtasm(gx*gx + gy*gy) + MAXCLIPDIST+walldist + 8;
	xmin = cx-rad; ymin = cy-rad;
	xmax = cx+rad; ymax = cy+rad;

	dawalclipmask = (cliptype&65535);        //CLIPMASK0 = 0x00010001
	dasprclipmask = (cliptype>>16);          //CLIPMASK1 = 0x01000040

	clipsectorlist[0] = (*sectnum);
	clipsectcnt = 0; clipsectnum = 1;
	do
	{
		dasect = clipsectorlist[clipsectcnt++];
		sec = &sector[dasect];
		startwall = sec->wallptr; endwall = startwall + sec->wallnum;
		for(j=startwall,wal=&wall[startwall];j<endwall;j++,wal++)
		{
			wal2 = &wall[wal->point2];
			if ((wal->x < xmin) && (wal2->x < xmin)) continue;
			if ((wal->x > xmax) && (wal2->x > xmax)) continue;
			if ((wal->y < ymin) && (wal2->y < ymin)) continue;
			if ((wal->y > ymax) && (wal2->y > ymax)) continue;

			x1 = wal->x; y1 = wal->y; x2 = wal2->x; y2 = wal2->y;

			dx = x2-x1; dy = y2-y1;
			if (dx*((*y)-y1) < ((*x)-x1)*dy) continue;  //If wall's not facing you

			if (dx > 0) dax = dx*(ymin-y1); else dax = dx*(ymax-y1);
			if (dy > 0) day = dy*(xmax-x1); else day = dy*(xmin-x1);
			if (dax >= day) continue;

			clipyou = 0;
			if ((wal->nextsector < 0) || (wal->cstat&dawalclipmask)) clipyou = 1;
			else if (editstatus == 0)
			{
				if (rintersect(*x,*y,0,gx,gy,0,x1,y1,x2,y2,&dax,&day,&daz) == 0)
					dax = *x, day = *y;
				daz = getflorzofslope((short)dasect,dax,day);
				daz2 = getflorzofslope(wal->nextsector,dax,day);

				sec2 = &sector[wal->nextsector];
				if (daz2 < daz-(1<<8))
					if ((sec2->floorstat&1) == 0)
						if ((*z) >= daz2-(flordist-1)) clipyou = 1;
				if (clipyou == 0)
				{
					daz = getceilzofslope((short)dasect,dax,day);
					daz2 = getceilzofslope(wal->nextsector,dax,day);
					if (daz2 > daz+(1<<8))
						if ((sec2->ceilingstat&1) == 0)
							if ((*z) <= daz2+(ceildist-1)) clipyou = 1;
				}
			}

			if (clipyou)
			{
					//Add 2 boxes at endpoints
				bsz = walldist; if (gx < 0) bsz = -bsz;
				addclipline(x1-bsz,y1-bsz,x1-bsz,y1+bsz,(short)j+32768);
				addclipline(x2-bsz,y2-bsz,x2-bsz,y2+bsz,(short)j+32768);
				bsz = walldist; if (gy < 0) bsz = -bsz;
				addclipline(x1+bsz,y1-bsz,x1-bsz,y1-bsz,(short)j+32768);
				addclipline(x2+bsz,y2-bsz,x2-bsz,y2-bsz,(short)j+32768);

				dax = walldist; if (dy > 0) dax = -dax;
				day = walldist; if (dx < 0) day = -day;
				addclipline(x1+dax,y1+day,x2+dax,y2+day,(short)j+32768);
			}
			else
			{
				for(i=clipsectnum-1;i>=0;i--)
					if (wal->nextsector == clipsectorlist[i]) break;
				if (i < 0) clipsectorlist[clipsectnum++] = wal->nextsector;
			}
		}

		for(j=headspritesect[dasect];j>=0;j=nextspritesect[j])
		{
			spr = &sprite[j];
			cstat = spr->cstat;
			if ((cstat&dasprclipmask) == 0) continue;
			x1 = spr->x; y1 = spr->y;
			switch(cstat&48)
			{
				case 0:
					if ((x1 >= xmin) && (x1 <= xmax) && (y1 >= ymin) && (y1 <= ymax))
					{
						k = ((tilesizy[spr->picnum]*spr->yrepeat)<<2);
						if (cstat&128) daz = spr->z+(k>>1); else daz = spr->z;
						if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
						if (((*z) < daz+ceildist) && ((*z) > daz-k-flordist))
						{
							bsz = (spr->clipdist<<2)+walldist; if (gx < 0) bsz = -bsz;
							addclipline(x1-bsz,y1-bsz,x1-bsz,y1+bsz,(short)j+49152);
							bsz = (spr->clipdist<<2)+walldist; if (gy < 0) bsz = -bsz;
							addclipline(x1+bsz,y1-bsz,x1-bsz,y1-bsz,(short)j+49152);
						}
					}
					break;
				case 16:
					k = ((tilesizy[spr->picnum]*spr->yrepeat)<<2);
					if (cstat&128) daz = spr->z+(k>>1); else daz = spr->z;
					if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
					daz2 = daz-k;
					daz += ceildist; daz2 -= flordist;
					if (((*z) < daz) && ((*z) > daz2))
					{
							//These lines get the 2 points of the rotated sprite
							//Given: (x1, y1) starts out as the center point
						tilenum = spr->picnum;
						xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						if ((cstat&4) > 0) xoff = -xoff;
						k = spr->ang; l = spr->xrepeat;
						dax = sintable[k&2047]*l; day = sintable[(k+1536)&2047]*l;
						l = tilesizx[tilenum]; k = (l>>1)+xoff;
						x1 -= mulscale16(dax,k); x2 = x1+mulscale16(dax,l);
						y1 -= mulscale16(day,k); y2 = y1+mulscale16(day,l);
						if (clipinsideboxline(cx,cy,x1,y1,x2,y2,rad) != 0)
						{
							dax = mulscale14(sintable[(spr->ang+256+512)&2047],walldist);
							day = mulscale14(sintable[(spr->ang+256)&2047],walldist);

							if ((x1-(*x))*(y2-(*y)) >= (x2-(*x))*(y1-(*y)))   //Front
							{
								addclipline(x1+dax,y1+day,x2+day,y2-dax,(short)j+49152);
							}
							else
							{
								if ((cstat&64) != 0) continue;
								addclipline(x2-dax,y2-day,x1-day,y1+dax,(short)j+49152);
							}

								//Side blocker
							if ((x2-x1)*((*x)-x1) + (y2-y1)*((*y)-y1) < 0)
								{ addclipline(x1-day,y1+dax,x1+dax,y1+day,(short)j+49152); }
							else if ((x1-x2)*((*x)-x2) + (y1-y2)*((*y)-y2) < 0)
								{ addclipline(x2+day,y2-dax,x2-dax,y2-day,(short)j+49152); }
						}
					}
					break;
				case 32:
					daz = spr->z+ceildist;
					daz2 = spr->z-flordist;
					if (((*z) < daz) && ((*z) > daz2))
					{
						if ((cstat&64) != 0)
							if (((*z) > spr->z) == ((cstat&8)==0)) continue;

						tilenum = spr->picnum;
						xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
						if ((cstat&4) > 0) xoff = -xoff;
						if ((cstat&8) > 0) yoff = -yoff;

						k = spr->ang;
						cosang = sintable[(k+512)&2047]; sinang = sintable[k];
						xspan = tilesizx[tilenum]; xrepeat = spr->xrepeat;
						yspan = tilesizy[tilenum]; yrepeat = spr->yrepeat;

						dax = ((xspan>>1)+xoff)*xrepeat; day = ((yspan>>1)+yoff)*yrepeat;
						rxi[0] = x1 + dmulscale16(sinang,dax,cosang,day);
						ryi[0] = y1 + dmulscale16(sinang,day,-cosang,dax);
						l = xspan*xrepeat;
						rxi[1] = rxi[0] - mulscale16(sinang,l);
						ryi[1] = ryi[0] + mulscale16(cosang,l);
						l = yspan*yrepeat;
						k = -mulscale16(cosang,l); rxi[2] = rxi[1]+k; rxi[3] = rxi[0]+k;
						k = -mulscale16(sinang,l); ryi[2] = ryi[1]+k; ryi[3] = ryi[0]+k;

						dax = mulscale14(sintable[(spr->ang-256+512)&2047],walldist);
						day = mulscale14(sintable[(spr->ang-256)&2047],walldist);

						if ((rxi[0]-(*x))*(ryi[1]-(*y)) < (rxi[1]-(*x))*(ryi[0]-(*y)))
						{
							if (clipinsideboxline(cx,cy,rxi[1],ryi[1],rxi[0],ryi[0],rad) != 0)
								addclipline(rxi[1]-day,ryi[1]+dax,rxi[0]+dax,ryi[0]+day,(short)j+49152);
						}
						else if ((rxi[2]-(*x))*(ryi[3]-(*y)) < (rxi[3]-(*x))*(ryi[2]-(*y)))
						{
							if (clipinsideboxline(cx,cy,rxi[3],ryi[3],rxi[2],ryi[2],rad) != 0)
								addclipline(rxi[3]+day,ryi[3]-dax,rxi[2]-dax,ryi[2]-day,(short)j+49152);
						}

						if ((rxi[1]-(*x))*(ryi[2]-(*y)) < (rxi[2]-(*x))*(ryi[1]-(*y)))
						{
							if (clipinsideboxline(cx,cy,rxi[2],ryi[2],rxi[1],ryi[1],rad) != 0)
								addclipline(rxi[2]-dax,ryi[2]-day,rxi[1]-day,ryi[1]+dax,(short)j+49152);
						}
						else if ((rxi[3]-(*x))*(ryi[0]-(*y)) < (rxi[0]-(*x))*(ryi[3]-(*y)))
						{
							if (clipinsideboxline(cx,cy,rxi[0],ryi[0],rxi[3],ryi[3],rad) != 0)
								addclipline(rxi[0]+dax,ryi[0]+day,rxi[3]+day,ryi[3]-dax,(short)j+49152);
						}
					}
					break;
			}
		}
	} while (clipsectcnt < clipsectnum);


	hitwall = 0;
	cnt = clipmoveboxtracenum;
	do
	{
		intx = goalx; inty = goaly;
		if ((hitwall = raytrace(*x, *y, &intx, &inty)) >= 0)
		{
			lx = clipit[hitwall].x2-clipit[hitwall].x1;
			ly = clipit[hitwall].y2-clipit[hitwall].y1;
			templong2 = lx*lx + ly*ly;
			if (templong2 > 0)
			{
				templong1 = (goalx-intx)*lx + (goaly-inty)*ly;

				if ((klabs(templong1)>>11) < templong2)
					i = divscale20(templong1,templong2);
				else
					i = 0;
				goalx = mulscale20(lx,i)+intx;
				goaly = mulscale20(ly,i)+inty;
			}

			templong1 = dmulscale6(lx,oxvect,ly,oyvect);
			for(i=cnt+1;i<=clipmoveboxtracenum;i++)
			{
				j = hitwalls[i];
				templong2 = dmulscale6(clipit[j].x2-clipit[j].x1,oxvect,clipit[j].y2-clipit[j].y1,oyvect);
				if ((templong1^templong2) < 0)
				{
					updatesector(*x,*y,sectnum);
					return(retval);
				}
			}

			keepaway(&goalx, &goaly, hitwall);
			xvect = ((goalx-intx)<<14);
			yvect = ((goaly-inty)<<14);

			if (cnt == clipmoveboxtracenum) retval = clipobjectval[hitwall];
			hitwalls[cnt] = hitwall;
		}
		cnt--;

		*x = intx;
		*y = inty;
	} while (((xvect|yvect) != 0) && (hitwall >= 0) && (cnt > 0));

	for(j=0;j<clipsectnum;j++)
		if (inside(*x,*y,clipsectorlist[j]) == 1)
		{
			*sectnum = clipsectorlist[j];
			return(retval);
		}

	*sectnum = -1; templong1 = 0x7fffffff;
	for(j=numsectors-1;j>=0;j--)
		if (inside(*x,*y,j) == 1)
		{
			if (sector[j].ceilingstat&2)
				templong2 = (getceilzofslope((short)j,*x,*y)-(*z));
			else
				templong2 = (sector[j].ceilingz-(*z));

			if (templong2 > 0)
			{
				if (templong2 < templong1)
					{ *sectnum = j; templong1 = templong2; }
			}
			else
			{
				if (sector[j].floorstat&2)
					templong2 = ((*z)-getflorzofslope((short)j,*x,*y));
				else
					templong2 = ((*z)-sector[j].floorz);

				if (templong2 <= 0)
				{
					*sectnum = j;
					return(retval);
				}
				if (templong2 < templong1)
					{ *sectnum = j; templong1 = templong2; }
			}
		}

	return(retval);
}


//
// pushmove
//
int pushmove (int *x, int *y, int *z, short *sectnum,
		 int walldist, int ceildist, int flordist, unsigned int cliptype)
{
	sectortype *sec, *sec2;
	walltype *wal;
	int i, j, k, t, dx, dy, dax, day, daz, daz2, bad, dir;
	int dawalclipmask;
	short startwall, endwall, clipsectcnt;
	char bad2;

	if ((*sectnum) < 0) return(-1);

	dawalclipmask = (cliptype&65535);
	//int dasprclipmask = (cliptype>>16);

	k = 32;
	dir = 1;
	do
	{
		bad = 0;

		clipsectorlist[0] = *sectnum;
		clipsectcnt = 0; clipsectnum = 1;
		do
		{
			/*Push FACE sprites
			for(i=headspritesect[clipsectorlist[clipsectcnt]];i>=0;i=nextspritesect[i])
			{
				spritetype *spr = &sprite[i];
				if (((spr->cstat&48) != 0) && ((spr->cstat&48) != 48)) continue;
				if ((spr->cstat&dasprclipmask) == 0) continue;

				dax = (*x)-spr->x; day = (*y)-spr->y;
				t = (spr->clipdist<<2)+walldist;
				if ((klabs(dax) < t) && (klabs(day) < t))
				{
					t = ((tilesizy[spr->picnum]*spr->yrepeat)<<2);
					if (spr->cstat&128) daz = spr->z+(t>>1); else daz = spr->z;
					if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
					if (((*z) < daz+ceildist) && ((*z) > daz-t-flordist))
					{
						t = (spr->clipdist<<2)+walldist;

						j = getangle(dax,day);
						dx = (sintable[(j+512)&2047]>>11);
						dy = (sintable[(j)&2047]>>11);
						bad2 = 16;
						do
						{
							*x = (*x) + dx; *y = (*y) + dy;
							bad2--; if (bad2 == 0) break;
						} while ((klabs((*x)-spr->x) < t) && (klabs((*y)-spr->y) < t));
						bad = -1;
						k--; if (k <= 0) return(bad);
						updatesector(*x,*y,sectnum);
					}
				}
			}*/

			sec = &sector[clipsectorlist[clipsectcnt]];
			if (dir > 0)
				startwall = sec->wallptr, endwall = startwall + sec->wallnum;
			else
				endwall = sec->wallptr, startwall = endwall + sec->wallnum;

			for(i=startwall,wal=&wall[startwall];i!=endwall;i+=dir,wal+=dir)
				if (clipinsidebox(*x,*y,i,walldist-4) == 1)
				{
					j = 0;
					if (wal->nextsector < 0) j = 1;
					if (wal->cstat&dawalclipmask) j = 1;
					if (j == 0)
					{
						sec2 = &sector[wal->nextsector];


							//Find closest point on wall (dax, day) to (*x, *y)
						dax = wall[wal->point2].x-wal->x;
						day = wall[wal->point2].y-wal->y;
						daz = dax*((*x)-wal->x) + day*((*y)-wal->y);
						if (daz <= 0)
							t = 0;
						else
						{
							daz2 = dax*dax+day*day;
							if (daz >= daz2) t = (1<<30); else t = divscale30(daz,daz2);
						}
						dax = wal->x + mulscale30(dax,t);
						day = wal->y + mulscale30(day,t);


						daz = getflorzofslope(clipsectorlist[clipsectcnt],dax,day);
						daz2 = getflorzofslope(wal->nextsector,dax,day);
						if ((daz2 < daz-(1<<8)) && ((sec2->floorstat&1) == 0))
							if (*z >= daz2-(flordist-1)) j = 1;

						daz = getceilzofslope(clipsectorlist[clipsectcnt],dax,day);
						daz2 = getceilzofslope(wal->nextsector,dax,day);
						if ((daz2 > daz+(1<<8)) && ((sec2->ceilingstat&1) == 0))
							if (*z <= daz2+(ceildist-1)) j = 1;
					}
					if (j != 0)
					{
						j = getangle(wall[wal->point2].x-wal->x,wall[wal->point2].y-wal->y);
						dx = (sintable[(j+1024)&2047]>>11);
						dy = (sintable[(j+512)&2047]>>11);
						bad2 = 16;
						do
						{
							*x = (*x) + dx; *y = (*y) + dy;
							bad2--; if (bad2 == 0) break;
						} while (clipinsidebox(*x,*y,i,walldist-4) != 0);
						bad = -1;
						k--; if (k <= 0) return(bad);
						updatesector(*x,*y,sectnum);
					}
					else
					{
						for(j=clipsectnum-1;j>=0;j--)
							if (wal->nextsector == clipsectorlist[j]) break;
						if (j < 0) clipsectorlist[clipsectnum++] = wal->nextsector;
					}
				}

			clipsectcnt++;
		} while (clipsectcnt < clipsectnum);
		dir = -dir;
	} while (bad != 0);

	return(bad);
}


//
// updatesector[z]
//
void updatesector(int x, int y, short *sectnum)
{
	walltype *wal;
	int i, j;

	if (inside(x,y,*sectnum) == 1) return;

	if ((*sectnum >= 0) && (*sectnum < numsectors))
	{
		wal = &wall[sector[*sectnum].wallptr];
		j = sector[*sectnum].wallnum;
		do
		{
			i = wal->nextsector;
			if (i >= 0)
				if (inside(x,y,(short)i) == 1)
				{
					*sectnum = i;
					return;
				}
			wal++;
			j--;
		} while (j != 0);
	}

	for(i=numsectors-1;i>=0;i--)
		if (inside(x,y,(short)i) == 1)
		{
			*sectnum = i;
			return;
		}

	*sectnum = -1;
}

void updatesectorz(int x, int y, int z, short *sectnum)
{
	walltype *wal;
	int i, j, cz, fz;

	getzsofslope(*sectnum, x, y, &cz, &fz);
	if ((z >= cz) && (z <= fz))
		if (inside(x,y,*sectnum) != 0) return;

	if ((*sectnum >= 0) && (*sectnum < numsectors))
	{
		wal = &wall[sector[*sectnum].wallptr];
		j = sector[*sectnum].wallnum;
		do
		{
			i = wal->nextsector;
			if (i >= 0)
			{
				getzsofslope(i, x, y, &cz, &fz);
				if ((z >= cz) && (z <= fz))
					if (inside(x,y,(short)i) == 1)
						{ *sectnum = i; return; }
			}
			wal++; j--;
		} while (j != 0);
	}

	for (i=numsectors-1;i>=0;i--)
	{
		getzsofslope(i, x, y, &cz, &fz);
		if ((z >= cz) && (z <= fz))
			if (inside(x,y,(short)i) == 1)
				{ *sectnum = i; return; }
	}

	*sectnum = -1;
}


//
// rotatepoint
//
void rotatepoint(int xpivot, int ypivot, int x, int y, short daang, int *x2, int *y2)
{
	int dacos, dasin;

	dacos = sintable[(daang+2560)&2047];
	dasin = sintable[(daang+2048)&2047];
	x -= xpivot;
	y -= ypivot;
	*x2 = dmulscale14(x,dacos,-y,dasin) + xpivot;
	*y2 = dmulscale14(y,dacos,x,dasin) + ypivot;
}


//
// getmousevalues
//
void getmousevalues(int *mousx, int *mousy, int *bstatus)
{
	readmousexy(mousx,mousy);
	readmousebstatus(bstatus);
}


//
// krand
//
int krand(void)
{
	randomseed = (randomseed*27584621)+1;
	return(((unsigned int)randomseed)>>16);
}


//
// getzrange
//
void getzrange(int x, int y, int z, short sectnum,
		 int *ceilz, int *ceilhit, int *florz, int *florhit,
		 int walldist, unsigned int cliptype)
{
	sectortype *sec;
	walltype *wal, *wal2;
	spritetype *spr;
	int clipsectcnt, startwall, endwall, tilenum, xoff, yoff, dax, day;
	int xmin, ymin, xmax, ymax, i, j, k, l, daz, daz2, dx, dy;
	int x1, y1, x2, y2, x3, y3, x4, y4, ang, cosang, sinang;
	int xspan, yspan, xrepeat, yrepeat, dasprclipmask, dawalclipmask;
	short cstat;
	unsigned char clipyou;

	if (sectnum < 0)
	{
		*ceilz = 0x80000000; *ceilhit = -1;
		*florz = 0x7fffffff; *florhit = -1;
		return;
	}

		//Extra walldist for sprites on sector lines
	i = walldist+MAXCLIPDIST+1;
	xmin = x-i; ymin = y-i;
	xmax = x+i; ymax = y+i;

	getzsofslope(sectnum,x,y,ceilz,florz);
	*ceilhit = sectnum+16384; *florhit = sectnum+16384;

	dawalclipmask = (cliptype&65535);
	dasprclipmask = (cliptype>>16);

	clipsectorlist[0] = sectnum;
	clipsectcnt = 0; clipsectnum = 1;

	do  //Collect sectors inside your square first
	{
		sec = &sector[clipsectorlist[clipsectcnt]];
		startwall = sec->wallptr; endwall = startwall + sec->wallnum;
		for(j=startwall,wal=&wall[startwall];j<endwall;j++,wal++)
		{
			k = wal->nextsector;
			if (k >= 0)
			{
				wal2 = &wall[wal->point2];
				x1 = wal->x; x2 = wal2->x;
				if ((x1 < xmin) && (x2 < xmin)) continue;
				if ((x1 > xmax) && (x2 > xmax)) continue;
				y1 = wal->y; y2 = wal2->y;
				if ((y1 < ymin) && (y2 < ymin)) continue;
				if ((y1 > ymax) && (y2 > ymax)) continue;

				dx = x2-x1; dy = y2-y1;
				if (dx*(y-y1) < (x-x1)*dy) continue; //back
				if (dx > 0) dax = dx*(ymin-y1); else dax = dx*(ymax-y1);
				if (dy > 0) day = dy*(xmax-x1); else day = dy*(xmin-x1);
				if (dax >= day) continue;

				if (wal->cstat&dawalclipmask) continue;
				sec = &sector[k];
				if (editstatus == 0)
				{
					if (((sec->ceilingstat&1) == 0) && (z <= sec->ceilingz+(3<<8))) continue;
					if (((sec->floorstat&1) == 0) && (z >= sec->floorz-(3<<8))) continue;
				}

				for(i=clipsectnum-1;i>=0;i--) if (clipsectorlist[i] == k) break;
				if (i < 0) clipsectorlist[clipsectnum++] = k;

				if ((x1 < xmin+MAXCLIPDIST) && (x2 < xmin+MAXCLIPDIST)) continue;
				if ((x1 > xmax-MAXCLIPDIST) && (x2 > xmax-MAXCLIPDIST)) continue;
				if ((y1 < ymin+MAXCLIPDIST) && (y2 < ymin+MAXCLIPDIST)) continue;
				if ((y1 > ymax-MAXCLIPDIST) && (y2 > ymax-MAXCLIPDIST)) continue;
				if (dx > 0) dax += dx*MAXCLIPDIST; else dax -= dx*MAXCLIPDIST;
				if (dy > 0) day -= dy*MAXCLIPDIST; else day += dy*MAXCLIPDIST;
				if (dax >= day) continue;

					//It actually got here, through all the continue's!!!
				getzsofslope((short)k,x,y,&daz,&daz2);
				if (daz > *ceilz) { *ceilz = daz; *ceilhit = k+16384; }
				if (daz2 < *florz) { *florz = daz2; *florhit = k+16384; }
			}
		}
		clipsectcnt++;
	} while (clipsectcnt < clipsectnum);

	for(i=0;i<clipsectnum;i++)
	{
		for(j=headspritesect[clipsectorlist[i]];j>=0;j=nextspritesect[j])
		{
			spr = &sprite[j];
			cstat = spr->cstat;
			if (cstat&dasprclipmask)
			{
				x1 = spr->x; y1 = spr->y;

				clipyou = 0;
				switch(cstat&48)
				{
					case 0:
						k = walldist+(spr->clipdist<<2)+1;
						if ((klabs(x1-x) <= k) && (klabs(y1-y) <= k))
						{
							daz = spr->z;
							k = ((tilesizy[spr->picnum]*spr->yrepeat)<<1);
							if (cstat&128) daz += k;
							if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
							daz2 = daz - (k<<1);
							clipyou = 1;
						}
						break;
					case 16:
						tilenum = spr->picnum;
						xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						if ((cstat&4) > 0) xoff = -xoff;
						k = spr->ang; l = spr->xrepeat;
						dax = sintable[k&2047]*l; day = sintable[(k+1536)&2047]*l;
						l = tilesizx[tilenum]; k = (l>>1)+xoff;
						x1 -= mulscale16(dax,k); x2 = x1+mulscale16(dax,l);
						y1 -= mulscale16(day,k); y2 = y1+mulscale16(day,l);
						if (clipinsideboxline(x,y,x1,y1,x2,y2,walldist+1) != 0)
						{
							daz = spr->z; k = ((tilesizy[spr->picnum]*spr->yrepeat)<<1);
							if (cstat&128) daz += k;
							if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
							daz2 = daz-(k<<1);
							clipyou = 1;
						}
						break;
					case 32:
						daz = spr->z; daz2 = daz;

						if ((cstat&64) != 0)
							if ((z > daz) == ((cstat&8)==0)) continue;

						tilenum = spr->picnum;
						xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
						if ((cstat&4) > 0) xoff = -xoff;
						if ((cstat&8) > 0) yoff = -yoff;

						ang = spr->ang;
						cosang = sintable[(ang+512)&2047]; sinang = sintable[ang];
						xspan = tilesizx[tilenum]; xrepeat = spr->xrepeat;
						yspan = tilesizy[tilenum]; yrepeat = spr->yrepeat;

						dax = ((xspan>>1)+xoff)*xrepeat; day = ((yspan>>1)+yoff)*yrepeat;
						x1 += dmulscale16(sinang,dax,cosang,day)-x;
						y1 += dmulscale16(sinang,day,-cosang,dax)-y;
						l = xspan*xrepeat;
						x2 = x1 - mulscale16(sinang,l);
						y2 = y1 + mulscale16(cosang,l);
						l = yspan*yrepeat;
						k = -mulscale16(cosang,l); x3 = x2+k; x4 = x1+k;
						k = -mulscale16(sinang,l); y3 = y2+k; y4 = y1+k;

						dax = mulscale14(sintable[(spr->ang-256+512)&2047],walldist+4);
						day = mulscale14(sintable[(spr->ang-256)&2047],walldist+4);
						x1 += dax; x2 -= day; x3 -= dax; x4 += day;
						y1 += day; y2 += dax; y3 -= day; y4 -= dax;

						if ((y1^y2) < 0)
						{
							if ((x1^x2) < 0) clipyou ^= (x1*y2<x2*y1)^(y1<y2);
							else if (x1 >= 0) clipyou ^= 1;
						}
						if ((y2^y3) < 0)
						{
							if ((x2^x3) < 0) clipyou ^= (x2*y3<x3*y2)^(y2<y3);
							else if (x2 >= 0) clipyou ^= 1;
						}
						if ((y3^y4) < 0)
						{
							if ((x3^x4) < 0) clipyou ^= (x3*y4<x4*y3)^(y3<y4);
							else if (x3 >= 0) clipyou ^= 1;
						}
						if ((y4^y1) < 0)
						{
							if ((x4^x1) < 0) clipyou ^= (x4*y1<x1*y4)^(y4<y1);
							else if (x4 >= 0) clipyou ^= 1;
						}
						break;
				}

				if (clipyou != 0)
				{
					if ((z > daz) && (daz > *ceilz)) { *ceilz = daz; *ceilhit = j+49152; }
					if ((z < daz2) && (daz2 < *florz)) { *florz = daz2; *florhit = j+49152; }
				}
			}
		}
	}
}


//
// setview
//
void setview(int x1, int y1, int x2, int y2)
{
	int i;
	float xfov;

	xfov = ((float)xdim / (float)ydim) / (4.f / 3.f);

	windowx1 = x1; wx1 = (x1<<12);
	windowy1 = y1; wy1 = (y1<<12);
	windowx2 = x2; wx2 = ((x2+1)<<12);
	windowy2 = y2; wy2 = ((y2+1)<<12);

	xdimen = (x2-x1)+1; halfxdimen = (xdimen>>1);
	xdimenrecip = divscale32(1L,xdimen);
	ydimen = (y2-y1)+1;

	setaspect((int)(65536.f * xfov), pixelaspect);

	for(i=0;i<windowx1;i++) { startumost[i] = 1, startdmost[i] = 0; }
	for(i=windowx1;i<=windowx2;i++)
		{ startumost[i] = windowy1, startdmost[i] = windowy2+1; }
	for(i=windowx2+1;i<xdim;i++) { startumost[i] = 1, startdmost[i] = 0; }

#if USE_POLYMOST && USE_OPENGL
	polymost_setview();
#endif
}


//
// setaspect
//
void setaspect(int daxrange, int daaspect)
{
    int ys = mulscale16(200, pixelaspect);

    viewingrange = daxrange;
    viewingrangerecip = divscale32(1L,daxrange);

    yxaspect = daaspect;
    xyaspect = divscale32(1,yxaspect);
    xdimenscale = scale(xdimen,yxaspect,320);
    xdimscale = scale(320,xyaspect,xdimen);

    ydimenscale = scale(ydimen, yxaspect, ys);
}


//
// flushperms
//
void flushperms(void)
{
	permhead = permtail = 0;
}


//
// rotatesprite
//
void rotatesprite(int sx, int sy, int z, short a, short picnum, signed char dashade,
	unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2)
{
	int i, gap = -1;
	permfifotype *per, *per2;

	if ((cx1 > cx2) || (cy1 > cy2)) return;
	if (z <= 16) return;
	if (picanm[picnum]&192) picnum += animateoffs(picnum,(short)0xc000);
	if ((tilesizx[picnum] <= 0) || (tilesizy[picnum] <= 0)) return;

	if (((dastat&128) == 0) || (numpages < 2) || (beforedrawrooms != 0)) {
		begindrawing();	//{{{
		dorotatesprite(sx,sy,z,a,picnum,dashade,dapalnum,dastat,cx1,cy1,cx2,cy2,guniqhudid);
		enddrawing();	//}}}
	}

	if ((dastat&64) && (cx1 <= 0) && (cy1 <= 0) && (cx2 >= xdim-1) && (cy2 >= ydim-1) &&
		 (sx == (160<<16)) && (sy == (100<<16)) && (z == 65536L) && (a == 0) && ((dastat&1) == 0))
		permhead = permtail = 0;

	if ((dastat&128) == 0) return;
	if (numpages >= 2)
	{
		if (((permhead+1)&(MAXPERMS-1)) == permtail)
		{
			for(i=permtail;i!=permhead;i=((i+1)&(MAXPERMS-1)))
			{
				if ((permfifo[i].pagesleft&127) == 0)
					{ if (gap < 0) gap = i; }
				else if (gap >= 0)
				{
					copybufbyte(&permfifo[i], &permfifo[gap], sizeof(permfifotype));
					permfifo[i].pagesleft = 0;
					for (;gap!=i;gap=((gap+1)&(MAXPERMS-1)))
						if ((permfifo[gap].pagesleft&127) == 0)
							break;
					if (gap==i) gap = -1;
				}
			}
			if (gap >= 0) permhead = gap;
			else permtail = ((permtail+1)&(MAXPERMS-1));
		}
		per = &permfifo[permhead];
		per->sx = sx; per->sy = sy; per->z = z; per->a = a;
		per->picnum = picnum;
		per->dashade = dashade; per->dapalnum = dapalnum;
		per->dastat = dastat;
		per->pagesleft = numpages+((beforedrawrooms&1)<<7);
		per->cx1 = cx1; per->cy1 = cy1; per->cx2 = cx2; per->cy2 = cy2;
		per->uniqid = guniqhudid;	//JF extension

			//Would be better to optimize out true bounding boxes
		if (dastat&64)  //If non-masking write, checking for overlapping cases
		{
			for(i=permtail;i!=permhead;i=((i+1)&(MAXPERMS-1)))
			{
				per2 = &permfifo[i];
				if ((per2->pagesleft&127) == 0) continue;
				if (per2->sx != per->sx) continue;
				if (per2->sy != per->sy) continue;
				if (per2->z != per->z) continue;
				if (per2->a != per->a) continue;
				if (tilesizx[per2->picnum] > tilesizx[per->picnum]) continue;
				if (tilesizy[per2->picnum] > tilesizy[per->picnum]) continue;
				if (per2->cx1 < per->cx1) continue;
				if (per2->cy1 < per->cy1) continue;
				if (per2->cx2 > per->cx2) continue;
				if (per2->cy2 > per->cy2) continue;
				per2->pagesleft = 0;
			}
			if (per->a == 0)
				for(i=permtail;i!=permhead;i=((i+1)&(MAXPERMS-1)))
				{
					per2 = &permfifo[i];
					if ((per2->pagesleft&127) == 0) continue;
					if (per2->z != per->z) continue;
					if (per2->a != 0) continue;
					if (per2->cx1 < per->cx1) continue;
					if (per2->cy1 < per->cy1) continue;
					if (per2->cx2 > per->cx2) continue;
					if (per2->cy2 > per->cy2) continue;
					if ((per2->sx>>16) < (per->sx>>16)) continue;
					if ((per2->sy>>16) < (per->sy>>16)) continue;
					if (per2->sx+(tilesizx[per2->picnum]*per2->z) > per->sx+(tilesizx[per->picnum]*per->z)) continue;
					if (per2->sy+(tilesizy[per2->picnum]*per2->z) > per->sy+(tilesizy[per->picnum]*per->z)) continue;
					per2->pagesleft = 0;
				}
		}

		permhead = ((permhead+1)&(MAXPERMS-1));
	}
}


//
// makepalookup
//
int makepalookup(int palnum, unsigned char *remapbuf, signed char r, signed char g, signed char b, unsigned char dastat)
{
	int i, j, palscale;
	unsigned char *ptr, *ptr2;

	if (palookup[palnum] == NULL)
	{
			//Allocate palookup buffer
		if ((palookup[palnum] = kmalloc(numpalookups<<8)) == NULL) {
			engineerrstr = "Failed to allocate palette lookup memory";
			return 1;
		}
	}

	if (dastat == 0) return 0;
	if ((r|g|b|63) != 63) return 0;

	if ((r|g|b) == 0)
	{
		for(i=0;i<256;i++)
		{
			ptr = (unsigned char *)((intptr_t)palookup[0]+remapbuf[i]);
			ptr2 = (unsigned char *)((intptr_t)palookup[palnum]+i);
			for(j=0;j<numpalookups;j++)
				{ *ptr2 = *ptr; ptr += 256; ptr2 += 256; }
		}
#if USE_POLYMOST && USE_OPENGL
		palookupfog[palnum].r = 0;
		palookupfog[palnum].g = 0;
		palookupfog[palnum].b = 0;
#endif
	}
	else
	{
		ptr2 = palookup[palnum];
		for(i=0;i<numpalookups;i++)
		{
			palscale = divscale16(i,numpalookups);
			for(j=0;j<256;j++)
			{
				ptr = &palette[remapbuf[j]*3];
				*ptr2++ = getclosestcol((int)ptr[0]+mulscale16(r-ptr[0],palscale),
							(int)ptr[1]+mulscale16(g-ptr[1],palscale),
							(int)ptr[2]+mulscale16(b-ptr[2],palscale));
			}
		}
#if USE_POLYMOST && USE_OPENGL
		palookupfog[palnum].r = r;
		palookupfog[palnum].g = g;
		palookupfog[palnum].b = b;
#endif
	}

	return 0;
}


void setvgapalette(void)
{
	int i;

	for (i=0;i<256;i++) {
		curpalettefaded[i].b = curpalette[i].b = vgapal16[4*i] << 2;
		curpalettefaded[i].g = curpalette[i].g = vgapal16[4*i+1] << 2;
		curpalettefaded[i].r = curpalette[i].r = vgapal16[4*i+2] << 2;
	}
	setpalette(0,256,vgapal16);
}

//
// setbrightness
//
void setbrightness(int dabrightness, unsigned char *dapal, char noapply)
{
	int i, k, j;

	if (!(noapply&4))
		curbrightness = min(max((int)dabrightness,0),15);

	curgamma = 1.0 + ((float)curbrightness / 10.0);
	if (setgamma(curgamma)) j = curbrightness; else j = 0;

	for(k=i=0;i<256;i++)
	{
		// save palette without any brightness adjustment
		curpalette[i].r = dapal[i*3+0] << 2;
		curpalette[i].g = dapal[i*3+1] << 2;
		curpalette[i].b = dapal[i*3+2] << 2;
		curpalette[i].f = 0;

		// brightness adjust the palette
		curpalettefaded[i].b = (tempbuf[k++] = britable[j][ curpalette[i].b ]);
		curpalettefaded[i].g = (tempbuf[k++] = britable[j][ curpalette[i].g ]);
		curpalettefaded[i].r = (tempbuf[k++] = britable[j][ curpalette[i].r ]);
		curpalettefaded[i].f = tempbuf[k++] = 0;
	}

	if ((noapply&1) == 0) setpalette(0,256,(unsigned char*)tempbuf);

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == 3) {
		static unsigned int lastpalettesum = 0;
		unsigned int newpalettesum = crc32once((unsigned char *)curpalettefaded, sizeof(curpalettefaded));

		// only reset the textures if the preserve flag (bit 1 of noapply) is clear and
		// either (a) the new palette is different to the last, or (b) the brightness
		// changed and we couldn't set it using hardware gamma
		if (!(noapply&2) && (newpalettesum != lastpalettesum))
			polymost_texinvalidateall();

		lastpalettesum = newpalettesum;
	}
#endif

	palfadergb.r = palfadergb.g = palfadergb.b = 0;
	palfadedelta = 0;
}


//
// setpalettefade
//
void setpalettefade(unsigned char r, unsigned char g, unsigned char b, unsigned char offset)
{
	int i,k;
	palette_t p;

	palfadergb.r = min(63,r) << 2;
	palfadergb.g = min(63,g) << 2;
	palfadergb.b = min(63,b) << 2;
	palfadedelta = min(63,offset) << 2;

	k = 0;
	for (i=0;i<256;i++) {
		if (gammabrightness) p = curpalette[i];
		else {
			p.b = britable[curbrightness][ curpalette[i].b ];
			p.g	= britable[curbrightness][ curpalette[i].g ];
			p.r = britable[curbrightness][ curpalette[i].r ];
		}

		tempbuf[k++] =
			(curpalettefaded[i].b =
				p.b + ( ( ( (int)palfadergb.b - (int)p.b ) * (int)offset ) >> 6 ) ) >> 2;
		tempbuf[k++] =
			(curpalettefaded[i].g =
				p.g + ( ( ( (int)palfadergb.g - (int)p.g ) * (int)offset ) >> 6 ) ) >> 2;
		tempbuf[k++] =
			(curpalettefaded[i].r =
				p.r + ( ( ( (int)palfadergb.r - (int)p.r ) * (int)offset ) >> 6 ) ) >> 2;
		tempbuf[k++] = curpalettefaded[i].f = 0;
	}

	setpalette(0,256,(unsigned char*)tempbuf);
}


//
// clearview
//
void clearview(int dacol)
{
	int y, dx;
	intptr_t p;

	if (qsetmode != 200) return;

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == 3) {
		palette_t p;
		if (gammabrightness) p = curpalette[dacol];
		else {
			p.r = britable[curbrightness][ curpalette[dacol].r ];
			p.g = britable[curbrightness][ curpalette[dacol].g ];
			p.b = britable[curbrightness][ curpalette[dacol].b ];
		}
		glfunc.glClearColor(((float)p.r)/255.0,
					  ((float)p.g)/255.0,
					  ((float)p.b)/255.0,
					  0);
		glfunc.glScissor(windowx1,yres-(windowy2+1),windowx2-windowx1+1,windowy2-windowy1+1);
		glfunc.glEnable(GL_SCISSOR_TEST);
		glfunc.glClear(GL_COLOR_BUFFER_BIT);
		glfunc.glDisable(GL_SCISSOR_TEST);
		return;
	}
#endif

	begindrawing();	//{{{
	dx = windowx2-windowx1+1;
	//dacol += (dacol<<8); dacol += (dacol<<16);
	p = frameplace+ylookup[windowy1]+windowx1;
	for(y=windowy1;y<=windowy2;y++) {
		//clearbufbyte((void*)p,dx,dacol);
		Bmemset((void*)p,dacol,dx);
		p += ylookup[1];
	}
	enddrawing();	//}}}

	faketimerhandler();
}


//
// clearallviews
//
void clearallviews(int dacol)
{
	if (qsetmode != 200) return;
	//dacol += (dacol<<8); dacol += (dacol<<16);

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == 3) {
		palette_t p;
		if (gammabrightness) p = curpalette[dacol];
		else {
			p.r = britable[curbrightness][ curpalette[dacol].r ];
			p.g = britable[curbrightness][ curpalette[dacol].g ];
			p.b = britable[curbrightness][ curpalette[dacol].b ];
		}
		glfunc.glViewport(0,0,xdim,ydim); glox1 = -1;
		glfunc.glClearColor(((float)p.r)/255.0,
					  ((float)p.g)/255.0,
					  ((float)p.b)/255.0,
					  0);
		glfunc.glClear(GL_COLOR_BUFFER_BIT);
		return;
	}
#endif

	begindrawing();	//{{{
	//clearbufbyte((void*)frameplace,imageSize,0L);
	Bmemset((void*)frameplace,dacol,imageSize);
	enddrawing();	//}}}
	//nextpage();

	faketimerhandler();
}


//
// plotpixel
//
void plotpixel(int x, int y, unsigned char col)
{
#if USE_POLYMOST && USE_OPENGL
	if (!polymost_plotpixel(x,y,col)) return;
#endif

	begindrawing();	//{{{
	drawpixel((void*)(ylookup[y]+x+frameplace),(int)col);
	enddrawing();	//}}}
}


//
// getpixel
//
unsigned char getpixel(int x, int y)
{
	unsigned char r;

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == 3 && qsetmode == 200) return 0;
#endif

	begindrawing();	//{{{
	r = readpixel((void*)(ylookup[y]+x+frameplace));
	enddrawing();	//}}}
	return(r);
}


	//MUST USE RESTOREFORDRAWROOMS AFTER DRAWING

//
// setviewtotile
//
void setviewtotile(short tilenume, int xsiz, int ysiz)
{
	int i, j;

		//DRAWROOMS TO TILE BACKUP&SET CODE
	tilesizx[tilenume] = xsiz; tilesizy[tilenume] = ysiz;
	bakxsiz[setviewcnt] = xsiz; bakysiz[setviewcnt] = ysiz;
	bakframeplace[setviewcnt] = frameplace; frameplace = waloff[tilenume];
	bakwindowx1[setviewcnt] = windowx1; bakwindowy1[setviewcnt] = windowy1;
	bakwindowx2[setviewcnt] = windowx2; bakwindowy2[setviewcnt] = windowy2;
#if USE_POLYMOST
	if (setviewcnt == 0) {
		bakrendmode = rendmode;
		baktile = tilenume;
	}
	rendmode = 0;//2;
#endif
	copybufbyte(&startumost[windowx1],&bakumost[windowx1],(windowx2-windowx1+1)*sizeof(bakumost[0]));
	copybufbyte(&startdmost[windowx1],&bakdmost[windowx1],(windowx2-windowx1+1)*sizeof(bakdmost[0]));
	setviewcnt++;

	offscreenrendering = 1;
	setview(0,0,ysiz-1,xsiz-1);
	setaspect(65536,65536);
	j = 0; for(i=0;i<=xsiz;i++) { ylookup[i] = j, j += ysiz; }
	setvlinebpl(ysiz);
}


//
// setviewback
//
extern char modechange;
void setviewback(void)
{
	int i, j, k;

	if (setviewcnt <= 0) return;
	setviewcnt--;

	offscreenrendering = (setviewcnt>0);
#if USE_POLYMOST
	if (setviewcnt == 0) {
		rendmode = bakrendmode;
#if USE_OPENGL
		invalidatetile(baktile,-1,-1);
#endif
	}
#endif

	setview(bakwindowx1[setviewcnt],bakwindowy1[setviewcnt],
			  bakwindowx2[setviewcnt],bakwindowy2[setviewcnt]);
	copybufbyte(&bakumost[windowx1],&startumost[windowx1],(windowx2-windowx1+1)*sizeof(startumost[0]));
	copybufbyte(&bakdmost[windowx1],&startdmost[windowx1],(windowx2-windowx1+1)*sizeof(startdmost[0]));
	frameplace = bakframeplace[setviewcnt];
	if (setviewcnt == 0)
		k = bakxsiz[0];
	else
		k = max(bakxsiz[setviewcnt-1],bakxsiz[setviewcnt]);
	j = 0; for(i=0;i<=k;i++) ylookup[i] = j, j += bytesperline;
	setvlinebpl(bytesperline);
	modechange=1;
}


//
// squarerotatetile
//
void squarerotatetile(short tilenume)
{
	int i, j, k, xsiz, ysiz;
	unsigned char *ptr1, *ptr2;

	xsiz = tilesizx[tilenume]; ysiz = tilesizy[tilenume];

		//supports square tiles only for rotation part
	if (xsiz == ysiz)
	{
		k = (xsiz<<1);
		for(i=xsiz-1;i>=0;i--)
		{
			ptr1 = (unsigned char *)(waloff[tilenume]+i*(xsiz+1)); ptr2 = ptr1;
			if ((i&1) != 0) { ptr1--; ptr2 -= xsiz; swapchar(ptr1,ptr2); }
			for(j=(i>>1)-1;j>=0;j--)
				{ ptr1 -= 2; ptr2 -= k; swapchar2(ptr1,ptr2,xsiz); }
		}
	}
}


//
// preparemirror
//
void preparemirror(int dax, int day, int daz, short daang, int dahoriz, short dawall, short dasector, int *tposx, int *tposy, short *tang)
{
	int i, j, x, y, dx, dy;

	(void)daz; (void)dahoriz; (void)dasector;

	x = wall[dawall].x; dx = wall[wall[dawall].point2].x-x;
	y = wall[dawall].y; dy = wall[wall[dawall].point2].y-y;
	j = dx*dx + dy*dy; if (j == 0) return;
	i = (((dax-x)*dx + (day-y)*dy)<<1);
	*tposx = (x<<1) + scale(dx,i,j) - dax;
	*tposy = (y<<1) + scale(dy,i,j) - day;
	*tang = (((getangle(dx,dy)<<1)-daang)&2047);

	inpreparemirror = 1;
}


//
// completemirror
//
void completemirror(void)
{
	int i, dy;
	intptr_t p;

#if USE_POLYMOST
	if (rendmode) return;
#endif

		//Can't reverse with uninitialized data
	if (inpreparemirror) { inpreparemirror = 0; return; }
	if (mirrorsx1 > 0) mirrorsx1--;
	if (mirrorsx2 < windowx2-windowx1-1) mirrorsx2++;
	if (mirrorsx2 < mirrorsx1) return;

	begindrawing();
	p = frameplace+ylookup[windowy1+mirrorsy1]+windowx1+mirrorsx1;
	i = windowx2-windowx1-mirrorsx2-mirrorsx1; mirrorsx2 -= mirrorsx1;
	for(dy=mirrorsy2-mirrorsy1-1;dy>=0;dy--)
	{
		copybufbyte((void*)(p+1),tempbuf,mirrorsx2+1);
		tempbuf[mirrorsx2] = tempbuf[mirrorsx2-1];
		copybufreverse(&tempbuf[mirrorsx2],(void*)(p+i),mirrorsx2+1);
		p += ylookup[1];
		faketimerhandler();
	}
	enddrawing();
}


//
// sectorofwall
//
int sectorofwall(short theline)
{
	int i, gap;

	if ((theline < 0) || (theline >= numwalls)) return(-1);
	i = wall[theline].nextwall; if (i >= 0) return(wall[i].nextsector);

	gap = (numsectors>>1); i = gap;
	while (gap > 1)
	{
		gap >>= 1;
		if (sector[i].wallptr < theline) i += gap; else i -= gap;
	}
	while (sector[i].wallptr > theline) i--;
	while (sector[i].wallptr+sector[i].wallnum <= theline) i++;
	return(i);
}


//
// getceilzofslope
//
int getceilzofslope(short sectnum, int dax, int day)
{
	int dx, dy, i, j;
	walltype *wal;

	if (!(sector[sectnum].ceilingstat&2)) return(sector[sectnum].ceilingz);
	wal = &wall[sector[sectnum].wallptr];
	dx = wall[wal->point2].x-wal->x; dy = wall[wal->point2].y-wal->y;
	i = (nsqrtasm(dx*dx+dy*dy)<<5); if (i == 0) return(sector[sectnum].ceilingz);
	j = dmulscale3(dx,day-wal->y,-dy,dax-wal->x);
	return(sector[sectnum].ceilingz+scale(sector[sectnum].ceilingheinum,j,i));
}


//
// getflorzofslope
//
int getflorzofslope(short sectnum, int dax, int day)
{
	int dx, dy, i, j;
	walltype *wal;

	if (!(sector[sectnum].floorstat&2)) return(sector[sectnum].floorz);
	wal = &wall[sector[sectnum].wallptr];
	dx = wall[wal->point2].x-wal->x; dy = wall[wal->point2].y-wal->y;
	i = (nsqrtasm(dx*dx+dy*dy)<<5); if (i == 0) return(sector[sectnum].floorz);
	j = dmulscale3(dx,day-wal->y,-dy,dax-wal->x);
	return(sector[sectnum].floorz+scale(sector[sectnum].floorheinum,j,i));
}


//
// getzsofslope
//
void getzsofslope(short sectnum, int dax, int day, int *ceilz, int *florz)
{
	int dx, dy, i, j;
	walltype *wal, *wal2;
	sectortype *sec;

	sec = &sector[sectnum];
	*ceilz = sec->ceilingz; *florz = sec->floorz;
	if ((sec->ceilingstat|sec->floorstat)&2)
	{
		wal = &wall[sec->wallptr]; wal2 = &wall[wal->point2];
		dx = wal2->x-wal->x; dy = wal2->y-wal->y;
		i = (nsqrtasm(dx*dx+dy*dy)<<5); if (i == 0) return;
		j = dmulscale3(dx,day-wal->y,-dy,dax-wal->x);
		if (sec->ceilingstat&2) *ceilz = (*ceilz)+scale(sec->ceilingheinum,j,i);
		if (sec->floorstat&2) *florz = (*florz)+scale(sec->floorheinum,j,i);
	}
}


//
// alignceilslope
//
void alignceilslope(short dasect, int x, int y, int z)
{
	int i, dax, day;
	walltype *wal;

	wal = &wall[sector[dasect].wallptr];
	dax = wall[wal->point2].x-wal->x;
	day = wall[wal->point2].y-wal->y;

	i = (y-wal->y)*dax - (x-wal->x)*day; if (i == 0) return;
	sector[dasect].ceilingheinum = scale((z-sector[dasect].ceilingz)<<8,
	  nsqrtasm(dax*dax+day*day),i);

	if (sector[dasect].ceilingheinum == 0) sector[dasect].ceilingstat &= ~2;
	else sector[dasect].ceilingstat |= 2;
}


//
// alignflorslope
//
void alignflorslope(short dasect, int x, int y, int z)
{
	int i, dax, day;
	walltype *wal;

	wal = &wall[sector[dasect].wallptr];
	dax = wall[wal->point2].x-wal->x;
	day = wall[wal->point2].y-wal->y;

	i = (y-wal->y)*dax - (x-wal->x)*day; if (i == 0) return;
	sector[dasect].floorheinum = scale((z-sector[dasect].floorz)<<8,
	  nsqrtasm(dax*dax+day*day),i);

	if (sector[dasect].floorheinum == 0) sector[dasect].floorstat &= ~2;
	else sector[dasect].floorstat |= 2;
}


//
// loopnumofsector
//
int loopnumofsector(short sectnum, short wallnum)
{
	int i, numloops, startwall, endwall;

	numloops = 0;
	startwall = sector[sectnum].wallptr;
	endwall = startwall + sector[sectnum].wallnum;
	for(i=startwall;i<endwall;i++)
	{
		if (i == wallnum) return(numloops);
		if (wall[i].point2 < i) numloops++;
	}
	return(-1);
}


//
// setfirstwall
//
void setfirstwall(short sectnum, short newfirstwall)
{
	int i, j, k, numwallsofloop;
	int startwall, endwall, danumwalls, dagoalloop;

	startwall = sector[sectnum].wallptr;
	danumwalls = sector[sectnum].wallnum;
	endwall = startwall+danumwalls;
	if ((newfirstwall < startwall) || (newfirstwall >= startwall+danumwalls)) return;
	for(i=0;i<danumwalls;i++)
		Bmemcpy(&wall[i+numwalls],&wall[i+startwall],sizeof(walltype));

	numwallsofloop = 0;
	i = newfirstwall;
	do
	{
		numwallsofloop++;
		i = wall[i].point2;
	} while (i != newfirstwall);

		//Put correct loop at beginning
	dagoalloop = loopnumofsector(sectnum,newfirstwall);
	if (dagoalloop > 0)
	{
		j = 0;
		while (loopnumofsector(sectnum,j+startwall) != dagoalloop) j++;
		for(i=0;i<danumwalls;i++)
		{
			k = i+j; if (k >= danumwalls) k -= danumwalls;
			Bmemcpy(&wall[startwall+i],&wall[numwalls+k],sizeof(walltype));

			wall[startwall+i].point2 += danumwalls-startwall-j;
			if (wall[startwall+i].point2 >= danumwalls)
				wall[startwall+i].point2 -= danumwalls;
			wall[startwall+i].point2 += startwall;
		}
		newfirstwall += danumwalls-j;
		if (newfirstwall >= startwall+danumwalls) newfirstwall -= danumwalls;
	}

	for(i=0;i<numwallsofloop;i++)
		Bmemcpy(&wall[i+numwalls],&wall[i+startwall],sizeof(walltype));
	for(i=0;i<numwallsofloop;i++)
	{
		k = i+newfirstwall-startwall;
		if (k >= numwallsofloop) k -= numwallsofloop;
		Bmemcpy(&wall[startwall+i],&wall[numwalls+k],sizeof(walltype));

		wall[startwall+i].point2 += numwallsofloop-newfirstwall;
		if (wall[startwall+i].point2 >= numwallsofloop)
			wall[startwall+i].point2 -= numwallsofloop;
		wall[startwall+i].point2 += startwall;
	}

	for(i=startwall;i<endwall;i++)
		if (wall[i].nextwall >= 0) wall[wall[i].nextwall].nextwall = i;
}


//
// drawline256
//
void drawline256(int x1, int y1, int x2, int y2, unsigned char col)
{
	int dx, dy, i, j, inc, daend, plc;
	intptr_t p;

	col = palookup[0][col];

	dx = x2-x1; dy = y2-y1;
	if (dx >= 0)
	{
		if ((x1 >= wx2) || (x2 < wx1)) return;
		if (x1 < wx1) y1 += scale(wx1-x1,dy,dx), x1 = wx1;
		if (x2 > wx2) y2 += scale(wx2-x2,dy,dx), x2 = wx2;
	}
	else
	{
		if ((x2 >= wx2) || (x1 < wx1)) return;
		if (x2 < wx1) y2 += scale(wx1-x2,dy,dx), x2 = wx1;
		if (x1 > wx2) y1 += scale(wx2-x1,dy,dx), x1 = wx2;
	}
	if (dy >= 0)
	{
		if ((y1 >= wy2) || (y2 < wy1)) return;
		if (y1 < wy1) x1 += scale(wy1-y1,dx,dy), y1 = wy1;
		if (y2 > wy2) x2 += scale(wy2-y2,dx,dy), y2 = wy2;
	}
	else
	{
		if ((y2 >= wy2) || (y1 < wy1)) return;
		if (y2 < wy1) x2 += scale(wy1-y2,dx,dy), y2 = wy1;
		if (y1 > wy2) x1 += scale(wy2-y1,dx,dy), y1 = wy2;
	}

#if USE_POLYMOST && USE_OPENGL
	if (!polymost_drawline256(x1,y1,x2,y2,col)) return;
#endif

	if (klabs(dx) >= klabs(dy))
	{
		if (dx == 0) return;
		if (dx < 0)
		{
			i = x1; x1 = x2; x2 = i;
			i = y1; y1 = y2; y2 = i;
			x1+=4096; x2+=4096;
		}

		inc = divscale12(dy,dx);
		plc = y1+mulscale12((2047-x1)&4095,inc);
		i = ((x1+2048)>>12); daend = ((x2+2048)>>12);

		begindrawing();	//{{{
		for(;i<daend;i++)
		{
			j = (plc>>12);
			if ((j >= startumost[i]) && (j < startdmost[i]))
				drawpixel((void*)(frameplace+ylookup[j]+i),col);
			plc += inc;
		}
		enddrawing();	//}}}
	}
	else
	{
		if (dy < 0)
		{
			i = x1; x1 = x2; x2 = i;
			i = y1; y1 = y2; y2 = i;
			y1+=4096; y2+=4096;
		}

		inc = divscale12(dx,dy);
		plc = x1+mulscale12((2047-y1)&4095,inc);
		i = ((y1+2048)>>12); daend = ((y2+2048)>>12);

		begindrawing();	//{{{
		p = ylookup[i]+frameplace;
		for(;i<daend;i++)
		{
			j = (plc>>12);
			if ((i >= startumost[j]) && (i < startdmost[j]))
				drawpixel((void*)(j+p),col);
			plc += inc; p += ylookup[1];
		}
		enddrawing();	//}}}
	}
}


//
// drawline16
//
// JBF: Had to add extra tests to make sure x-coordinates weren't winding up -'ve
//   after clipping or crashes would ensue
unsigned int drawlinepat = 0xffffffff;

void drawline16(int x1, int y1, int x2, int y2, unsigned char col)
{
	int i, dx, dy, pinc, d;
	intptr_t p;
	unsigned int patc=0;

	dx = x2-x1; dy = y2-y1;
	if (dx >= 0)
	{
		if ((x1 >= xres) || (x2 < 0)) return;
		if (x1 < 0) { if (dy) y1 += scale(0-x1,dy,dx); x1 = 0; }
		if (x2 >= xres) { if (dy) y2 += scale(xres-1-x2,dy,dx); x2 = xres-1; }
	}
	else
	{
		if ((x2 >= xres) || (x1 < 0)) return;
		if (x2 < 0) { if (dy) y2 += scale(0-x2,dy,dx); x2 = 0; }
		if (x1 >= xres) { if (dy) y1 += scale(xres-1-x1,dy,dx); x1 = xres-1; }
	}
	if (dy >= 0)
	{
		if ((y1 >= ydim16) || (y2 < 0)) return;
		if (y1 < 0) { if (dx) x1 += scale(0-y1,dx,dy); y1 = 0; if (x1 < 0) x1 = 0; }
		if (y2 >= ydim16) { if (dx) x2 += scale(ydim16-1-y2,dx,dy); y2 = ydim16-1; if (x2 < 0) x2 = 0; }
	}
	else
	{
		if ((y2 >= ydim16) || (y1 < 0)) return;
		if (y2 < 0) { if (dx) x2 += scale(0-y2,dx,dy); y2 = 0; if (x2 < 0) x2 = 0; }
		if (y1 >= ydim16) { if (dx) x1 += scale(ydim16-1-y1,dx,dy); y1 = ydim16-1; if (x1 < 0) x1 = 0; }
	}

	dx = klabs(x2-x1)+1; dy = klabs(y2-y1)+1;
	if (dx >= dy)
	{
		if (x2 < x1)
		{
			i = x1; x1 = x2; x2 = i;
			i = y1; y1 = y2; y2 = i;
		}
		d = 0;
		if (y2 > y1) pinc = bytesperline; else pinc = -bytesperline;

		begindrawing();	//{{{
		p = (y1*bytesperline)+x1+frameplace;
		if (dy == 0 && drawlinepat == 0xffffffff) {
			i = ((int)col<<24)|((int)col<<16)|((int)col<<8)|col;
			clearbufbyte((void *)p, dx, i);
		} else
		for(i=dx;i>0;i--)
		{
			if (drawlinepat & pow2long[(patc++)&31])
				drawpixel((void *)p, col);
			d += dy;
			if (d >= dx) { d -= dx; p += pinc; }
			p++;
		}
		enddrawing();	//}}}
		return;
	}

	if (y2 < y1)
	{
		i = x1; x1 = x2; x2 = i;
		i = y1; y1 = y2; y2 = i;
	}
	d = 0;
	if (x2 > x1) pinc = 1; else pinc = -1;

	begindrawing();	//{{{
	p = (y1*bytesperline)+x1+frameplace;
	for(i=dy;i>0;i--)
	{
		if (drawlinepat & pow2long[(patc++)&31])
			drawpixel((void *)p, col);
		d += dx;
		if (d >= dy) { d -= dy; p += pinc; }
		p += bytesperline;
	}
	enddrawing();	//}}}
}

void drawcircle16(int x1, int y1, int r, unsigned char col)
{
#if 1
	int xp, yp, xpbpl, ypbpl, d, de, dse, patc=0;
	intptr_t p;

	if (r < 0) r = -r;
	if (x1+r < 0 || x1-r >= xres) return;
	if (y1+r < 0 || y1-r >= ydim16) return;

	/*
	 *      d
	 *    6 | 7
	 *   \  |  /
	 *  5  \|/  8
	 * c----+----a
	 *  4  /|\  1
	 *   /  |  \
	 *    3 | 2
	 *      b
	 */

	xp = 0;
	yp = r;
	d = 1 - r;
	de = 2;
	dse = 5 - (r << 1);

	begindrawing();
	p = (y1*bytesperline)+x1+frameplace;

	if (drawlinepat & pow2long[(patc++)&31]) {
		if ((unsigned int)y1 < (unsigned int)ydim16 && (unsigned int)(x1+r) < (unsigned int)xres  )
			drawpixel((void *)(p+r), col);			// a
		if ((unsigned int)x1 < (unsigned int)xres   && (unsigned int)(y1+r) < (unsigned int)ydim16)
			drawpixel((void *)(p+(r*bytesperline)), col);	// b
		if ((unsigned int)y1 < (unsigned int)ydim16 && (unsigned int)(x1-r) < (unsigned int)xres  )
			drawpixel((void *)(p-r), col);			// c
		if ((unsigned int)x1 < (unsigned int)xres   && (unsigned int)(y1-r) < (unsigned int)ydim16)
			drawpixel((void *)(p-(r*bytesperline)), col);	// d
	}

	while (yp > xp) {
		if (d < 0) {
			d += de;
			de += 2;
			dse += 2;
			xp++;
		} else {
			d += dse;
			de += 2;
			dse += 4;
			xp++;
			yp--;
		}

		ypbpl = yp*bytesperline;
		xpbpl = xp*bytesperline;
		if (drawlinepat & pow2long[(patc++)&31]) {
			if ((unsigned int)(x1+yp) < (unsigned int)xres && (unsigned int)(y1+xp) < (unsigned int)ydim16)
				drawpixel((void *)(p+yp+xpbpl), col);	// 1
			if ((unsigned int)(x1+xp) < (unsigned int)xres && (unsigned int)(y1+yp) < (unsigned int)ydim16)
				drawpixel((void *)(p+xp+ypbpl), col);	// 2
			if ((unsigned int)(x1-xp) < (unsigned int)xres && (unsigned int)(y1+yp) < (unsigned int)ydim16)
				drawpixel((void *)(p-xp+ypbpl), col);	// 3
			if ((unsigned int)(x1-yp) < (unsigned int)xres && (unsigned int)(y1+xp) < (unsigned int)ydim16)
				drawpixel((void *)(p-yp+xpbpl), col);	// 4
			if ((unsigned int)(x1-yp) < (unsigned int)xres && (unsigned int)(y1-xp) < (unsigned int)ydim16)
				drawpixel((void *)(p-yp-xpbpl), col);	// 5
			if ((unsigned int)(x1-xp) < (unsigned int)xres && (unsigned int)(y1-yp) < (unsigned int)ydim16)
				drawpixel((void *)(p-xp-ypbpl), col);	// 6
			if ((unsigned int)(x1+xp) < (unsigned int)xres && (unsigned int)(y1-yp) < (unsigned int)ydim16)
				drawpixel((void *)(p+xp-ypbpl), col);	// 7
			if ((unsigned int)(x1+yp) < (unsigned int)xres && (unsigned int)(y1-xp) < (unsigned int)ydim16)
				drawpixel((void *)(p+yp-xpbpl), col);	// 8
		}
	}
	enddrawing();
#else
	// JonoF's rough approximation of a circle
	int l,spx,spy,lpx,lpy,px,py;

	spx = lpx = x1+mulscale14(r,sintable[0]);
	spy = lpy = y1+mulscale14(r,sintable[512]);

	for (l=64;l<2048;l+=64) {
		px = x1+mulscale14(r,sintable[l]);
		py = y1+mulscale14(r,sintable[(l+512)&2047]);

		drawline16(lpx,lpy,px,py,col);

		lpx = px;
		lpy = py;
	}

	drawline16(lpx,lpy,spx,spy,col);
#endif
}


//
// qsetmode640350
//
void qsetmode640350(void)
{
	if (qsetmode != 350)
	{
		if (setvideomode(640, 350, 8, fullscreen) < 0) {
			//fprintf(stderr, "Couldn't set 640x350 video mode for some reason.\n");
			return;
		}

		xdim = xres;
		ydim = yres;
		pixelaspect = 65536;

		setvgapalette();

		ydim16 = 350;
		halfxdim16 = 320;
		midydim16 = 146;

		begindrawing();	//{{{
		clearbuf((void *)frameplace, (bytesperline*350L) >> 2, 0);
		enddrawing();	//}}}
	}

	qsetmode = 350;
}


//
// qsetmode640480
//
void qsetmode640480(void)
{
	if (qsetmode != 480)
	{
		if (setvideomode(640, 480, 8, fullscreen) < 0) {
			//fprintf(stderr, "Couldn't set 640x480 video mode for some reason.\n");
			return;
		}

		xdim = xres;
		ydim = yres;
		pixelaspect = 65536;

		setvgapalette();

		ydim16 = 336;
		halfxdim16 = 320;
		midydim16 = 200;

		begindrawing();	//{{{
		clearbuf((void *)(frameplace + (336l*bytesperline)), (bytesperline*144L) >> 2, 0x08080808l);
		clearbuf((void *)frameplace, (bytesperline*336L) >> 2, 0L);
		enddrawing();	//}}}
	}

	qsetmode = 480;
}


//
// qsetmodeany
//
void qsetmodeany(int daxdim, int daydim)
{
	if (daxdim < 640) daxdim = 640;
	if (daydim < 480) daydim = 480;

	if (qsetmode != ((daxdim<<16)|(daydim&0xffff))) {
		if (setvideomode(daxdim, daydim, 8, fullscreen) < 0)
			return;

		xdim = xres;
		ydim = yres;
		pixelaspect = 65536;

		setvgapalette();

		ydim16 = yres - STATUS2DSIZ;
		halfxdim16 = xres >> 1;
		midydim16 = scale(200,yres,480);

		begindrawing();	//{{{
		clearbuf((void *)(frameplace + (ydim16*bytesperline)), (bytesperline*STATUS2DSIZ) >> 2, 0x08080808l);
		clearbuf((void *)frameplace, (ydim16*bytesperline) >> 2, 0L);
		enddrawing();	//}}}
	}

	qsetmode = ((daxdim<<16)|(daydim&0xffff));
}


//
// clear2dscreen
//
void clear2dscreen(void)
{
	int clearsz;

	begindrawing();	//{{{
	if (qsetmode == 350) clearsz = 350;
	else {
		if (ydim16 <= yres-STATUS2DSIZ) clearsz = yres - STATUS2DSIZ;
		else clearsz = yres;
	}
	clearbuf((void *)frameplace, (bytesperline*clearsz) >> 2, 0);
	enddrawing();	//}}}
}


//
// draw2dgrid
//
void draw2dgrid(int posxe, int posye, short ange, int zoome, short gride)
{
	int i, xp1, yp1, xp2=0, yp2, tempy;

	(void)ange;

	if (gride > 0)
	{
		begindrawing();	//{{{

		yp1 = midydim16-mulscale14(posye+editorgridextent,zoome);
		if (yp1 < 0) yp1 = 0;
		yp2 = midydim16-mulscale14(posye-editorgridextent,zoome);
		if (yp2 >= ydim16) yp2 = ydim16-1;

		if ((yp1 < ydim16) && (yp2 >= 0) && (yp2 >= yp1))
		{
			xp1 = halfxdim16-mulscale14(posxe+editorgridextent,zoome);

			for(i=-editorgridextent;i<=editorgridextent;i+=(2048>>gride))
			{
				xp2 = xp1;
				xp1 = halfxdim16-mulscale14(posxe-i,zoome);

				if (xp1 >= xdim) break;
				if (xp1 >= 0)
				{
					if (xp1 != xp2)
					{
						drawline16(xp1,yp1,xp1,yp2,8);
					}
				}
			}
			if ((i >= editorgridextent) && (xp1 < xdim))
				xp2 = xp1;
			if ((xp2 >= 0) && (xp2 < xdim))
			{
				drawline16(xp2,yp1,xp2,yp2,8);
			}
		}

		xp1 = mulscale14(posxe+editorgridextent,zoome);
		xp2 = mulscale14(posxe-editorgridextent,zoome);
		tempy = 0x80000000l;
		for(i=-editorgridextent;i<=editorgridextent;i+=(2048>>gride))
		{
			yp1 = (((posye-i)*zoome)>>14);
			if (yp1 != tempy)
			{
				if ((yp1 > midydim16-ydim16) && (yp1 <= midydim16))
				{
					drawline16(halfxdim16-xp1,midydim16-yp1,halfxdim16-xp2,midydim16-yp1,8);
					tempy = yp1;
				}
			}
		}

		enddrawing();	//}}}
	}
}


//
// draw2dscreen
//
void draw2dscreen(int posxe, int posye, short ange, int zoome, short gride)
{
	walltype *wal;
	int i, j, xp1, yp1, xp2, yp2;
	intptr_t templong;
	unsigned char col;

	if (qsetmode == 200) return;

	begindrawing();	//{{{

	if (editstatus == 0)
	{
		faketimerhandler();
		clear2dscreen();

		faketimerhandler();
		draw2dgrid(posxe,posye,ange,zoome,gride);
	}

	faketimerhandler();
	for(i=numwalls-1,wal=&wall[i];i>=0;i--,wal--)
	{
		if (editstatus == 0)
		{
			if ((show2dwall[i>>3]&pow2char[i&7]) == 0) continue;
			j = wal->nextwall;
			if ((j >= 0) && (i > j))
				if ((show2dwall[j>>3]&pow2char[j&7]) > 0) continue;
		}
		else
		{
			j = wal->nextwall;
			if ((j >= 0) && (i > j)) continue;
		}

		if (j < 0)
		{
			col = 7;
			if (i == linehighlight) if (totalclock & 8) col += (2<<2);
		}
		else
		{
			col = 4;
			if ((wal->cstat&1) != 0) col = 5;
			if ((i == linehighlight) || ((linehighlight >= 0) && (i == wall[linehighlight].nextwall)))
				if (totalclock & 8) col += (2<<2);
		}

		xp1 = mulscale14(wal->x-posxe,zoome);
		yp1 = mulscale14(wal->y-posye,zoome);
		xp2 = mulscale14(wall[wal->point2].x-posxe,zoome);
		yp2 = mulscale14(wall[wal->point2].y-posye,zoome);

		if ((wal->cstat&64) > 0)
		{
			if (klabs(xp2-xp1) >= klabs(yp2-yp1))
			{
				drawline16(halfxdim16+xp1,midydim16+yp1+1,halfxdim16+xp2,midydim16+yp2+1,col);
				drawline16(halfxdim16+xp1,midydim16+yp1-1,halfxdim16+xp2,midydim16+yp2-1,col);
			}
			else
			{
				drawline16(halfxdim16+xp1+1,midydim16+yp1,halfxdim16+xp2+1,midydim16+yp2,col);
				drawline16(halfxdim16+xp1-1,midydim16+yp1,halfxdim16+xp2-1,midydim16+yp2,col);
			}
			col += 8;
		}
		drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16+xp2,midydim16+yp2,col);

		if ((zoome >= 256) && (editstatus == 1))
			if (((halfxdim16+xp1) >= 2) && ((halfxdim16+xp1) <= xdim-3))
				if (((midydim16+yp1) >= 2) && ((midydim16+yp1) <= ydim16-3))
				{
					col = 2;
					if (i == pointhighlight) {
						if (totalclock & 8) col += (2<<2);	// JBF 20040116: two braces is all this needed. man I'm a fool sometimes.
					}
					else if ((highlightcnt > 0) && (editstatus == 1))
					{
						if (show2dwall[i>>3]&pow2char[i&7])
							if (totalclock & 8) col += (2<<2);
					}

					templong = ((midydim16+yp1)*bytesperline)+(halfxdim16+xp1)+frameplace;
					drawpixel((void *)(templong-2-(bytesperline<<1)), col);
					drawpixel((void *)(templong-1-(bytesperline<<1)), col);
					drawpixel((void *)(templong+0-(bytesperline<<1)), col);
					drawpixel((void *)(templong+1-(bytesperline<<1)), col);
					drawpixel((void *)(templong+2-(bytesperline<<1)), col);

					drawpixel((void *)(templong-2+(bytesperline<<1)), col);
					drawpixel((void *)(templong-1+(bytesperline<<1)), col);
					drawpixel((void *)(templong+0+(bytesperline<<1)), col);
					drawpixel((void *)(templong+1+(bytesperline<<1)), col);
					drawpixel((void *)(templong+2+(bytesperline<<1)), col);

					drawpixel((void *)(templong-2-bytesperline), col);
					drawpixel((void *)(templong-2+0), col);
					drawpixel((void *)(templong-2+bytesperline), col);

					drawpixel((void *)(templong+2-bytesperline), col);
					drawpixel((void *)(templong+2+0), col);
					drawpixel((void *)(templong+2+bytesperline), col);
				}
	}
	faketimerhandler();

	if ((zoome >= 256) || (editstatus == 0))
		for(i=0;i<numsectors;i++)
			for(j=headspritesect[i];j>=0;j=nextspritesect[j])
				if ((editstatus == 1) || (show2dsprite[j>>3]&pow2char[j&7]))
				{
					col = 3;
					if ((sprite[j].cstat&1) > 0) col = 5;
					if (editstatus == 1)
					{
						if (j+16384 == pointhighlight) {
							if (totalclock & 8) col += (2<<2);
						}
						else if ((highlightcnt > 0) && (editstatus == 1))
						{
							if (show2dsprite[j>>3]&pow2char[j&7])
								if (totalclock & 8) col += (2<<2);
						}
					}

					xp1 = mulscale14(sprite[j].x-posxe,zoome);
					yp1 = mulscale14(sprite[j].y-posye,zoome);
					if (((halfxdim16+xp1) >= 2) && ((halfxdim16+xp1) <= xdim-3))
						if (((midydim16+yp1) >= 2) && ((midydim16+yp1) <= ydim16-3))
						{
							templong = ((midydim16+yp1)*bytesperline)+(halfxdim16+xp1)+frameplace;
							drawpixel((void *)(templong-1-(bytesperline<<1)), col);
							drawpixel((void *)(templong+0-(bytesperline<<1)), col);
							drawpixel((void *)(templong+1-(bytesperline<<1)), col);

							drawpixel((void *)(templong-1+(bytesperline<<1)), col);
							drawpixel((void *)(templong+0+(bytesperline<<1)), col);
							drawpixel((void *)(templong+1+(bytesperline<<1)), col);

							drawpixel((void *)(templong-2-bytesperline), col);
							drawpixel((void *)(templong-2+0), col);
							drawpixel((void *)(templong-2+bytesperline), col);

							drawpixel((void *)(templong+2-bytesperline), col);
							drawpixel((void *)(templong+2+0), col);
							drawpixel((void *)(templong+2+bytesperline), col);

							drawpixel((void *)(templong+1+bytesperline), col);
							drawpixel((void *)(templong-1+bytesperline), col);
							drawpixel((void *)(templong+1-bytesperline), col);
							drawpixel((void *)(templong-1-bytesperline), col);

							/*
							 * JBF 20050103: A little something intended for TerminX. It draws a box
							 * indicating the extents of a floor-aligned sprite in the 2D view of the editor.
							 *
							if ((sprite[j].cstat&32) > 0) {
								int fx = mulscale6(tilesizx[sprite[j].picnum], sprite[j].xrepeat);
								int fy = mulscale6(tilesizy[sprite[j].picnum], sprite[j].yrepeat);
								int co[4][2], ii;
								int sinang = sintable[(sprite[j].ang+512+1024)&2047];
								int cosang = sintable[(sprite[j].ang+1024)&2047];
								int r,s;

								fx = mulscale10(fx,zoome) >> 1;
								fy = mulscale10(fy,zoome) >> 1;

								co[0][0] = -fx;
								co[0][1] = -fy;
								co[1][0] =  fx;
								co[1][1] = -fy;
								co[2][0] =  fx;
								co[2][1] =  fy;
								co[3][0] = -fx;
								co[3][1] =  fy;

								for (ii=0;ii<4;ii++) {
									r = mulscale14(cosang,co[ii][0]) - mulscale14(sinang,co[ii][1]);
									s = mulscale14(sinang,co[ii][0]) + mulscale14(cosang,co[ii][1]);
									co[ii][0] = r;
									co[ii][1] = s;
								}

								drawlinepat = 0xcccccccc;
								for (ii=0;ii<4;ii++) {
									drawline16(halfxdim16 + xp1 + co[ii][0], midydim16 + yp1 - co[ii][1],
										halfxdim16 + xp1 + co[(ii+1)&3][0], midydim16 + yp1 - co[(ii+1)&3][1],
										col);
								}
								drawlinepat = 0xffffffff;
							}
							*/

							xp2 = mulscale11(sintable[(sprite[j].ang+2560)&2047],zoome) / 768;
							yp2 = mulscale11(sintable[(sprite[j].ang+2048)&2047],zoome) / 768;

							if ((sprite[j].cstat&256) > 0)
							{
								if (((sprite[j].ang+256)&512) == 0)
								{
									drawline16(halfxdim16+xp1,midydim16+yp1-1,halfxdim16+xp1+xp2,midydim16+yp1+yp2-1,col);
									drawline16(halfxdim16+xp1,midydim16+yp1+1,halfxdim16+xp1+xp2,midydim16+yp1+yp2+1,col);
								}
								else
								{
									drawline16(halfxdim16+xp1-1,midydim16+yp1,halfxdim16+xp1+xp2-1,midydim16+yp1+yp2,col);
									drawline16(halfxdim16+xp1+1,midydim16+yp1,halfxdim16+xp1+xp2+1,midydim16+yp1+yp2,col);
								}
								col += 8;
							}
							drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16+xp1+xp2,midydim16+yp1+yp2,col);
						}
				}

	faketimerhandler();
	xp1 = mulscale11(sintable[(ange+2560)&2047],zoome) / 768; //Draw white arrow
	yp1 = mulscale11(sintable[(ange+2048)&2047],zoome) / 768;
	drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16-xp1,midydim16-yp1,15);
	drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16+yp1,midydim16-xp1,15);
	drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16-yp1,midydim16+xp1,15);

	enddrawing();	//}}}
}


//
// printext16
//
void printext16(int xpos, int ypos, short col, short backcol, const char *name, char fontsize)
{
	int stx, i, x, y, charxsiz;
	unsigned char *fontptr, *letptr, *ptr;

	stx = xpos;

	if (fontsize) { fontptr = smalltextfont; charxsiz = 4; }
	else { fontptr = textfont; charxsiz = 8; }

	begindrawing();	//{{{
	for(i=0;name[i];i++)
	{
		letptr = &fontptr[((int)(unsigned char)name[i])<<3];
		ptr = (unsigned char *)(bytesperline*(ypos+7)+(stx-fontsize)+frameplace);
		for(y=7;y>=0;y--)
		{
			for(x=charxsiz-1;x>=0;x--)
			{
				if (letptr[y]&pow2char[7-fontsize-x])
					ptr[x] = (unsigned char)col;
				else if (backcol >= 0)
					ptr[x] = (unsigned char)backcol;
			}
			ptr -= bytesperline;
		}
		stx += charxsiz;
	}
	enddrawing();	//}}}
}


//
// printext256
//
void printext256(int xpos, int ypos, short col, short backcol, const char *name, char fontsize)
{
	int stx, i, x, y, charxsiz;
	unsigned char *fontptr, *letptr, *ptr;

	stx = xpos;

	if (fontsize) { fontptr = smalltextfont; charxsiz = 4; }
	else { fontptr = textfont; charxsiz = 8; }

#if USE_POLYMOST && USE_OPENGL
	if (!polymost_printext256(xpos,ypos,col,backcol,name,fontsize)) return;
#endif

	begindrawing();	//{{{
	for(i=0;name[i];i++)
	{
		letptr = &fontptr[name[i]<<3];
		ptr = (unsigned char *)(ylookup[ypos+7]+(stx-fontsize)+frameplace);
		for(y=7;y>=0;y--)
		{
			for(x=charxsiz-1;x>=0;x--)
			{
				if (letptr[y]&pow2char[7-fontsize-x])
					ptr[x] = (unsigned char)col;
				else if (backcol >= 0)
					ptr[x] = (unsigned char)backcol;
			}
			ptr -= ylookup[1];
		}
		stx += charxsiz;
	}
	enddrawing();	//}}}
}


//
// screencapture
//
static BFILE *screencapture_openfile(const char *ext)
{
	char *seq;
	BFILE *fil;

	do {	// JBF 2004022: So we don't overwrite existing screenshots
		if (capturecount > 9999) {
			return NULL;
		}

		seq = strrchr(capturename, '.'); if (!seq) return NULL;
		seq -= 4;
		seq[0] = ((capturecount/1000)%10)+48;
		seq[1] = ((capturecount/100)%10)+48;
		seq[2] = ((capturecount/10)%10)+48;
		seq[3] = (capturecount%10)+48;
		seq[5] = ext[0];
		seq[6] = ext[1];
		seq[7] = ext[2];

		if ((fil = Bfopen(capturename, "rb")) == NULL) break;
		Bfclose(fil);
		capturecount++;
	} while (1);
	fil = Bfopen(capturename, "wb");
	if (fil) capturecount++;
	return fil;
}

static int screencapture_writeframe(BFILE *fil, char mode, void *v,
	void (*writeline)(unsigned char *, int, int, BFILE *, void *))
{
	int y, ystart, yend, yinc, j;
	unsigned char *ptr, *buf;
	char inverseit = 0, bottotop = 0;

	inverseit = (mode & 1);
	bottotop = (mode & 2);

#if USE_POLYMOST && USE_OPENGL
	if (rendmode >= 3 && qsetmode == 200) {
		char bgr = (mode & 4);

		// OpenGL returns bottom-to-top ordered lines.
		if (bottotop) {
			ystart = 0;
			yend = ydim;
			yinc = 1;
		} else {
			ystart = ydim-1;
			yend = -1;
			yinc = -1;
		}
		buf = kmalloc(xdim*ydim*3);
		if (buf) {
			glfunc.glReadPixels(0,0,xdim,ydim,GL_RGB,GL_UNSIGNED_BYTE,buf);
			if (bgr) {
				for (j=(xdim*ydim-1)*3; j>=0; j-=3) {
					swapchar(&buf[j+0], &buf[j+2]);
				}
			}
			for (y = ystart; y != yend; y += yinc) {
				ptr = buf + y*xdim*3;
				writeline(ptr, xdim, 3, fil, v);
			}
			kfree(buf);
		}
		return(0);
	}
#endif

	begindrawing();	//{{{
	ptr = (unsigned char *)frameplace;
	if (bottotop) {
		ystart = ydim-1;
		yend = -1;
		yinc = -1;
	} else {
		ystart = 0;
		yend = ydim;
		yinc = 1;
	}

	if (inverseit && qsetmode != 200) {
		buf = kmalloc(bytesperline);
		if (buf) {
			for (y = ystart; y != yend; y += yinc) {
				copybuf(ptr + y*bytesperline, buf, xdim >> 2);
				for (j=(bytesperline>>2)-1; j>=0; j--) ((int *)buf)[j] ^= 0x0f0f0f0fL;
				writeline(buf, xdim, 1, fil, v);
			}
			kfree(buf);
		}
	} else {
		for (y = ystart; y != yend; y += yinc) {
			writeline(ptr + y*bytesperline, xdim, 1, fil, v);
		}
	}

	enddrawing();	//}}}
	return(0);
}

static void screencapture_writetgaline(unsigned char *buf, int bytes, int elements, BFILE *fp, void *v)
{
	(void)v;
	Bfwrite(buf, bytes, elements, fp);
}

static int screencapture_tga(char mode)
{
	int i;
	unsigned char head[18] = { 0,1,1,0,0,0,1,24,0,0,0,0,0/*wlo*/,0/*whi*/,0/*hlo*/,0/*hhi*/,8,0 };
	BFILE *fil;

	if ((fil = screencapture_openfile("tga")) == NULL) {
		return -1;
	}

#if USE_POLYMOST && USE_OPENGL
	if (rendmode >= 3 && qsetmode == 200) {
		head[1] = 0;	// no colourmap
		head[2] = 2;	// uncompressed truecolour
		head[3] = 0;	// (low) first colourmap index
		head[4] = 0;	// (high) first colourmap index
		head[5] = 0;	// (low) number colourmap entries
		head[6] = 0;	// (high) number colourmap entries
		head[7] = 0;	// colourmap entry size
		head[16] = 24;	// 24 bits per pixel
	}
#endif

	head[12] = xdim & 0xff;
	head[13] = (xdim >> 8) & 0xff;
	head[14] = ydim & 0xff;
	head[15] = (ydim >> 8) & 0xff;

	Bfwrite(head, 18, 1, fil);

	// palette first
#if USE_POLYMOST && USE_OPENGL
	if (rendmode < 3 || (rendmode == 3 && qsetmode != 200)) {
#endif
		for (i=0; i<256; i++) {
			Bfputc(curpalettefaded[i].b, fil);	// b
			Bfputc(curpalettefaded[i].g, fil);	// g
			Bfputc(curpalettefaded[i].r, fil);	// r
		}
#if USE_POLYMOST && USE_OPENGL
	}
#endif

	// Targa renders bottom to top, from left to right.
	// 24bit images use BGR element order.
	screencapture_writeframe(fil, (mode&1) | 2 | 4, NULL, screencapture_writetgaline);

	Bfclose(fil);
	return(0);
}

// PCX is nasty, which is why I've lifted these functions from the PCX spec by ZSoft
static int writepcxbyte(unsigned char colour, unsigned char count, BFILE *fp)
{
	if (!count) return 0;
	if (count == 1 && (colour & 0xc0) != 0xc0) {
		Bfputc(colour, fp);
		return 1;
	} else {
		Bfputc(0xc0 | count, fp);
		Bfputc(colour, fp);
		return 2;
	}
}

static void writepcxline(unsigned char *buf, int bytes, int step, BFILE *fp)
{
	unsigned char ths, last;
	int srcIndex;
	unsigned char runCount;

	runCount = 1;
	last = *buf;

	for (srcIndex=1; srcIndex<bytes; srcIndex++) {
		buf += step;
		ths = *buf;
		if (ths == last) {
			runCount++;
			if (runCount == 63) {
				writepcxbyte(last, runCount, fp);
				runCount = 0;
			}
		} else {
			if (runCount)
				writepcxbyte(last, runCount, fp);

			last = ths;
			runCount = 1;
		}
	}

	if (runCount) writepcxbyte(last, runCount, fp);
	if (bytes&1) writepcxbyte(0, 1, fp);
}

static void screencapture_writepcxline(unsigned char *buf, int bytes, int elements, BFILE *fp, void *v)
{
	(void)v;
	if (elements == 3) {
		writepcxline(buf,   bytes, 3, fp);
		writepcxline(buf+1, bytes, 3, fp);
		writepcxline(buf+2, bytes, 3, fp);
		return;
	}
	writepcxline(buf, bytes, 1, fp);
}

static int screencapture_pcx(char mode)
{
	int i,bpl;
	unsigned char head[128];
	BFILE *fil;

	if ((fil = screencapture_openfile("pcx")) == NULL) {
		return -1;
	}

	memset(head,0,128);
	head[0] = 10;
	head[1] = 5;
	head[2] = 1;
	head[3] = 8;
	head[12] = 72; head[13] = 0;
	head[14] = 72; head[15] = 0;
	head[65] = 1;	// 8-bit
	head[68] = 1;

#if USE_POLYMOST && USE_OPENGL
	if (rendmode >= 3 && qsetmode == 200) {
		head[65] = 3;	// 24-bit
	}
#endif

	head[8] = (xdim-1) & 0xff;
	head[9] = ((xdim-1) >> 8) & 0xff;
	head[10] = (ydim-1) & 0xff;
	head[11] = ((ydim-1) >> 8) & 0xff;

	bpl = xdim + (xdim&1);

	head[66] = bpl & 0xff;
	head[67] = (bpl >> 8) & 0xff;

	Bfwrite(head, 128, 1, fil);

	// PCX renders top to bottom, from left to right.
	// 24-bit images have each scan line written as deinterleaved RGB.
	screencapture_writeframe(fil, (mode&1), NULL, screencapture_writepcxline);

	// palette last
#if USE_POLYMOST && USE_OPENGL
	if (rendmode < 3 || (rendmode == 3 && qsetmode != 200)) {
#endif
		Bfputc(12,fil);
		for (i=0; i<256; i++) {
			Bfputc(curpalettefaded[i].r, fil);	// b
			Bfputc(curpalettefaded[i].g, fil);	// g
			Bfputc(curpalettefaded[i].b, fil);	// r
		}
#if USE_POLYMOST && USE_OPENGL
	}
#endif

	Bfclose(fil);
	return(0);
}

struct pngsums {
	unsigned int crc;
	unsigned short adlers1;
	unsigned short adlers2;
};

static void screencapture_writepngline(unsigned char *buf, int bytes, int elements, BFILE *fp, void *v)
{
	unsigned char header[6];
	unsigned short blklen;
	int i;
	struct pngsums *sums = (struct pngsums *)v;

	blklen = (unsigned short)B_LITTLE16(1 + bytes * elements);	// One extra for the filter type.
	header[0] = 0;	// BFINAL = 0, BTYPE = 00.
	memcpy(&header[1], &blklen, 2);
	blklen = ~blklen;
	memcpy(&header[3], &blklen, 2);

	header[5] = 0;	// No filter.
	sums->adlers2 = (sums->adlers2 + sums->adlers1) % 65521;
	crc32block(&sums->crc, header, sizeof(header));
	Bfwrite(header, sizeof(header), 1, fp);

	for (i=0; i < bytes * elements; i++) {
		sums->adlers1 = (sums->adlers1 + buf[i]) % 65521;
		sums->adlers2 = (sums->adlers2 + sums->adlers1) % 65521;
	}
	crc32block(&sums->crc, buf, bytes * elements);
	Bfwrite(buf, bytes, elements, fp);
}

static int screencapture_png(char mode)
{
	const unsigned char pngsig[] = { 0x89, 'P', 'N', 'G', 0xd, 0xa, 0x1a, 0xa };
	const unsigned char enddeflate[] = { 1, 0, 0, 0xff, 0xff };

#define BEGIN_PNG_CHUNK(type) { \
	acclen = 4; \
	memcpy(&buf[acclen], type, 4); \
	acclen += 4; \
}
#define SET_PNG_CHUNK_LEN(fore) { \
	/* Accumulated and forecast, minus length and type fields. */ \
	int len = B_BIG32(acclen + fore - 8); \
	memcpy(&buf[0], &len, 4); \
}
#define END_PNG_CHUNK(ccrc) { \
	unsigned int crc = B_BIG32(ccrc); \
	memcpy(&buf[acclen], &crc, 4); \
	acclen += 4; \
	Bfwrite(buf, acclen, 1, fil); \
}

	unsigned char buf[1024];
	int length, i, acclen, glmode = 0;
	unsigned short s;
	BFILE *fil;
	struct pngsums sums;

#if USE_POLYMOST && USE_OPENGL
	glmode = (rendmode == 3 && qsetmode == 200);
#endif

	if ((fil = screencapture_openfile("png")) == NULL) {
		return -1;
	}

	Bfwrite(pngsig, sizeof(pngsig), 1, fil);

	// Header.
	BEGIN_PNG_CHUNK("IHDR");
	i = B_BIG32(xdim); memcpy(&buf[acclen], &i, 4); acclen += 4;
	i = B_BIG32(ydim); memcpy(&buf[acclen], &i, 4); acclen += 4;
	buf[acclen++] = 8;	// Bit depth per sample/palette index.
	buf[acclen++] = glmode ? 2 : 3;	// Colour type.
	buf[acclen++] = 0;	// Deflate compression method.
	buf[acclen++] = 0;	// Adaptive filter.
	buf[acclen++] = 0;	// No interlace.
	SET_PNG_CHUNK_LEN(0);
	END_PNG_CHUNK(crc32once(&buf[4], acclen - 4));

	// Palette if needed.
#if USE_POLYMOST && USE_OPENGL
	if (rendmode < 3 || (rendmode == 3 && qsetmode != 200)) {
#endif
		BEGIN_PNG_CHUNK("PLTE");
		for (i=0; i<256; i++, acclen+=3) {
			buf[acclen+0] = curpalettefaded[i].r;
			buf[acclen+1] = curpalettefaded[i].g;
			buf[acclen+2] = curpalettefaded[i].b;
		}
		SET_PNG_CHUNK_LEN(0);
		END_PNG_CHUNK(crc32once(&buf[4], acclen - 4));
#if USE_POLYMOST && USE_OPENGL
	}
#endif

	// Image Data.
	BEGIN_PNG_CHUNK("IDAT");
	crc32init(&sums.crc);

	// Content is a Zlib stream.
	buf[acclen++] = 0x78;	// Deflate, 32k window.
	buf[acclen++] = 1;		// Check bits 0-4: (0x7800 + 1) % 0x1f == 0

	length = (1 + 2 + 2) + 1 + xdim * (glmode ? 3 : 1);	// Length of one scanline deflate block.
	length *= ydim;			// By height.
	length += sizeof(enddeflate);	// Plus length of 'End of Deflate' block.
	length += 4;			// Plus Adler32 checksum.
	SET_PNG_CHUNK_LEN(length);

	crc32block(&sums.crc, &buf[4], acclen - 4);
	Bfwrite(buf, acclen, 1, fil);	// Write header and start of Zlib stream.

	sums.adlers1 = 1;
	sums.adlers2 = 0;
	screencapture_writeframe(fil, (mode&1), &sums, screencapture_writepngline);

	// Finalise the Zlib stream.
	acclen = 0;
	memcpy(&buf[acclen], enddeflate, sizeof(enddeflate)); acclen += sizeof(enddeflate);
	s = B_BIG16(sums.adlers2); memcpy(&buf[acclen], &s, 2); acclen += 2;
	s = B_BIG16(sums.adlers1); memcpy(&buf[acclen], &s, 2); acclen += 2;

	// Finalise the Image Data chunk and write what remains out.
	crc32block(&sums.crc, buf, acclen);
	crc32finish(&sums.crc);
	END_PNG_CHUNK(sums.crc);

	// End chunk.
	BEGIN_PNG_CHUNK("IEND");
	SET_PNG_CHUNK_LEN(0);
	END_PNG_CHUNK(crc32once(&buf[4], acclen - 4));

	Bfclose(fil);
	return(0);
}

int screencapture(char *filename, char mode)
{
	int ret;

	if (filename) {
		strcpy(capturename, filename);
	}
	if (qsetmode == 200 && (mode & 2) && !captureatnextpage) {
		captureatnextpage = mode;
		return 0;
	}
	switch (captureformat) {
		case 0: ret = screencapture_tga(mode&1); break;
		case 1: ret = screencapture_pcx(mode&1); break;
		default: ret = screencapture_png(mode&1); break;
	}
	if (ret == 0) {
		buildprintf("Saved screenshot to %s\n", capturename);
	}
	return ret;
}


#if USE_POLYMOST

//
// setrendermode
//
int setrendermode(int renderer)
{
	if (bpp == 8) {
		if (renderer < 0) renderer = 0;
		else if (renderer > 2) renderer = 2;
	} else {
		renderer = 3;
	}

	rendmode = renderer;

	return 0;
}

//
// getrendermode
//
int getrendermode(void)
{
	return rendmode;
}


//
// setrollangle
//
void setrollangle(int rolla)
{
	if (rolla == 0) gtang = 0.0;
	else gtang = PI * (double)rolla / 1024.0;
}

#endif //USE_POLYMOST

#if USE_POLYMOST && USE_OPENGL

//
// invalidatetile
//  pal: pass -1 to invalidate all palettes for the tile, or >=0 for a particular palette
//  how: pass -1 to invalidate all instances of the tile in texture memory, or a bitfield
//         bit 0: opaque or masked (non-translucent) texture, using repeating
//         bit 1: ignored
//         bit 2: ignored (33% translucence, using repeating)
//         bit 3: ignored (67% translucence, using repeating)
//         bit 4: opaque or masked (non-translucent) texture, using clamping
//         bit 5: ignored
//         bit 6: ignored (33% translucence, using clamping)
//         bit 7: ignored (67% translucence, using clamping)
//       clamping is for sprites, repeating is for walls
//
void invalidatetile(short tilenume, int pal, int how)
{
	int numpal, firstpal, np;
	int hp;

	if (rendmode < 3) return;

	if (pal < 0) {
		numpal = MAXPALOOKUPS;
		firstpal = 0;
	} else {
		numpal = 1;
		firstpal = pal % MAXPALOOKUPS;
	}

	for (hp = 0; hp < 8; hp+=4) {
		if (!(how & pow2long[hp])) continue;

		for (np = firstpal; np < firstpal+numpal; np++) {
			polymost_texinvalidate(tilenume, np, hp);
		}
	}
}


//
// setpolymost2dview
//  Sets OpenGL for 2D drawing
//
void setpolymost2dview(void)
{
	if (rendmode < 3) return;

	if (gloy1 != -1) {
		glfunc.glViewport(0,0,xres,yres);
	}

	gloy1 = -1;

	glfunc.glDisable(GL_DEPTH_TEST);
	glfunc.glDisable(GL_BLEND);
}

#endif //USE_POLYMOST && USE_OPENGL


void buildprintf(const char *fmt, ...)
{
	char tmpstr[1024];
	va_list va, vac;

	va_start(va, fmt);

	va_copy(vac, va);
	vfprintf(stdout, fmt, vac);
	va_end(vac);

	if (logfile) {
		va_copy(vac, va);
		vfprintf(logfile, fmt, vac);
		va_end(vac);
	}

	va_copy(vac, va);
	vsnprintf(tmpstr, sizeof(tmpstr), fmt, vac);
	va_end(vac);

	initputs(tmpstr);
	OSD_Puts(tmpstr);

	va_end(va);
}

void buildputs(const char *str)
{
    fputs(str, stdout);
    if (logfile) fputs(str, logfile);
    initputs(str);  // the startup window
    OSD_Puts(str);  // the onscreen-display
}

void buildsetlogfile(const char *fn)
{
	if (logfile) Bfclose(logfile);
	logfile = NULL;
	if (fn) logfile = Bfopen(fn,"w");
	if (logfile) setvbuf(logfile, (char*)NULL, _IONBF, 0);
}


/*
 * vim:ts=8:
 */

