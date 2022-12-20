/**************************************************************************************************
KPLIB.C: Ken's Picture LIBrary written by Ken Silverman
Copyright (c) 1998-2006 Ken Silverman
Ken Silverman's official web site: http://advsys.net/ken

Features of KPLIB.C:
	* Routines for decoding JPG, PNG, GIF, PCX, TGA, BMP, DDS, CEL.
		See kpgetdim(), kprender(), and optional helper function: kpzload().
	* Routines for ZIP decompression. All ZIP functions start with the letters "kz".
	* Multi-platform support: Dos/Windows/Linux/Mac/etc..
	* Compact code, all in a single source file. Yeah, bad design on my part... but makes life
		  easier for everyone else - you simply add a single C file to your project, throw a few
		  externs in there, add the function calls, and you're done!

Brief history:
1998?: Wrote KPEG, a JPEG viewer for DOS
2000: Wrote KPNG, a PNG viewer for DOS
2001: Combined KPEG & KPNG, ported to Visual C, and made it into a library called KPLIB.C
2002: Added support for TGA, GIF, CEL, ZIP
2003: Added support for BMP
05/18/2004: Added support for 8&24 bit PCX
12/09/2005: Added support for progressive JPEG
01/05/2006: Added support for DDS

I offer this code to the community for free use - all I ask is that my name be included in the
credits.

-Ken S.
**************************************************************************************************/

#if !defined(_WIN32) && !defined(__DOS__)
// For dirent.h DT_ macros.
#define _DEFAULT_SOURCE
#endif

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(__BIG_ENDIAN__)
# define BIGENDIAN 1
#endif

#ifdef BIGENDIAN
static unsigned int LSWAPIB (unsigned int a) { return(((a>>8)&0xff00)+((a&0xff00)<<8)+(a<<24)+(a>>24)); }
static unsigned short SSWAPIB (unsigned short a) { return((a>>8)+(a<<8)); }
#define LSWAPIL(a) (a)
#define SSWAPIL(a) (a)
#else
#define LSWAPIB(a) (a)
#define SSWAPIB(a) (a)
static unsigned int LSWAPIL (unsigned int a) { return(((a>>8)&0xff00)+((a&0xff00)<<8)+(a<<24)+(a>>24)); }
static unsigned short SSWAPIL (unsigned short a) { return((a>>8)+(a<<8)); }
#endif

#if !defined(_WIN32) && !defined(__DOS__)
#include <unistd.h>
#include <dirent.h>
typedef long long __int64;
static inline int _lrotl (int i, int sh)
	{ return((i>>(-sh))|(i<<sh)); }
static inline int _filelength (int h)
{
	struct stat st;
	if (fstat(h,&st) < 0) return(-1);
	return((int)st.st_size);
}
#define _fileno fileno
#else
#include <io.h>
#endif

#if defined(__DOS__)
#include <dos.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif
#if !defined(max)
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#if !defined(min)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

static int bytesperline, xres, yres, globxoffs, globyoffs;
static intptr_t frameplace;

static const int pow2mask[32] =
{
	0x00000000,0x00000001,0x00000003,0x00000007,
	0x0000000f,0x0000001f,0x0000003f,0x0000007f,
	0x000000ff,0x000001ff,0x000003ff,0x000007ff,
	0x00000fff,0x00001fff,0x00003fff,0x00007fff,
	0x0000ffff,0x0001ffff,0x0003ffff,0x0007ffff,
	0x000fffff,0x001fffff,0x003fffff,0x007fffff,
	0x00ffffff,0x01ffffff,0x03ffffff,0x07ffffff,
	0x0fffffff,0x1fffffff,0x3fffffff,0x7fffffff,
};
static const int pow2long[32] =
{
	0x00000001,0x00000002,0x00000004,0x00000008,
	0x00000010,0x00000020,0x00000040,0x00000080,
	0x00000100,0x00000200,0x00000400,0x00000800,
	0x00001000,0x00002000,0x00004000,0x00008000,
	0x00010000,0x00020000,0x00040000,0x00080000,
	0x00100000,0x00200000,0x00400000,0x00800000,
	0x01000000,0x02000000,0x04000000,0x08000000,
	0x10000000,0x20000000,0x40000000,0x80000000,
};

	//Hack for peekbits,getbits,suckbits (to prevent lots of duplicate code)
	//   0: PNG: do 12-byte chunk_header removal hack
	// !=0: ZIP: use 64K buffer (olinbuf)
static int zipfilmode;
typedef struct
{
	FILE *fil;   //0:no file open, !=0:open file (either stand-alone or zip)
	int comptyp; //0:raw data (can be ZIP or stand-alone), 8:PKZIP LZ77 *flate
	unsigned seek0;   //0:stand-alone file, !=0: start of zip compressed stream data
	int compleng;//Global variable for compression FIFO
	int comptell;//Global variable for compression FIFO
	int leng;    //Uncompressed file size (bytes)
	int pos;     //Current uncompressed relative file position (0<=pos<=leng)
	int endpos;  //Temp global variable for kzread
	int jmpplc;  //Store place where decompression paused
	int i;       //For stand-alone/ZIP comptyp#0, this is like "uncomptell"
					  //For ZIP comptyp#8&btype==0 "<64K store", this saves i state
	int bfinal;  //LZ77 decompression state (for later calls)
} kzfilestate;
static kzfilestate kzfs;

//Initialized tables (can't be in union)
//jpg:                png:
//   crmul      16384    abstab10    4096
//   cbmul      16384    hxbit        472
//   dct         4608    pow2mask     128*
//   colclip     4096
//   colclipup8  4096
//   colclipup16 4096
//   unzig        256
//   pow2mask     128*
//   dcflagor      64

int palcol[256], paleng, bakcol, numhufblocks, zlibcompflags;
signed char coltype, filtype, bitdepth;

//============================ KPNGILIB begins ===============================

#define PROCESSALPHAHERE 0  //Set to 1 for KPNG, 0 in all other cases

//07/31/2000: KPNG.C first ported to C from READPNG.BAS
//10/11/2000: KPNG.C split into 2 files: KPNG.C and PNGINLIB.C
//11/24/2000: Finished adding support for coltypes 4&6
//03/31/2001: Added support for Adam7-type interlaced images
//Currently, there is no support for:
//   * 16-bit color depth
//   * Some useless ancillary chunks, like: gAMA(gamma) & pHYs(aspect ratio)

	//.PNG specific variables:
static int bakr = 0x80, bakg = 0x80, bakb = 0x80; //this used to be public...
static int gslidew = 0, gslider = 0, xm, xmn[4], xr0, xr1, xplc, yplc;
static intptr_t nfplace;
static int clen[320], cclen[19], bitpos, filt, xsiz, ysiz;
static int xsizbpl, ixsiz, ixoff, iyoff, ixstp, iystp, intlac, nbpl, trnsrgb;
static int ccind[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
static int hxbit[59][2], ibuf0[288], nbuf0[32], ibuf1[32], nbuf1[32];
static const unsigned char *filptr;
static unsigned char slidebuf[32768], opixbuf0[4], opixbuf1[4];
static unsigned char pnginited = 0, olinbuf[65536]; //WARNING:max xres is: 65536/bpp-1
static int gotcmov = -2, abstab10[1024];

	//Variables to speed up dynamic Huffman decoding:
#define LOGQHUFSIZ0 9
#define LOGQHUFSIZ1 6
static int qhufval0[1<<LOGQHUFSIZ0], qhufval1[1<<LOGQHUFSIZ1];
static unsigned char qhufbit0[1<<LOGQHUFSIZ0], qhufbit1[1<<LOGQHUFSIZ1];

#if defined(__WATCOMC__) && USE_ASM

static int bswap (int);
#pragma aux bswap =\
	".586"\
	"bswap eax"\
	parm [eax]\
	modify nomemory exact [eax]\
	value [eax]

static int bitrev (int, int);
#pragma aux bitrev =\
	"xor eax, eax"\
	"beg: shr ebx, 1"\
	"adc eax, eax"\
	"dec ecx"\
	"jnz short beg"\
	parm [ebx][ecx]\
	modify nomemory exact [eax ebx ecx]\
	value [eax]

static int testflag (int);
#pragma aux testflag =\
	"pushfd"\
	"pop eax"\
	"mov ebx, eax"\
	"xor eax, ecx"\
	"push eax"\
	"popfd"\
	"pushfd"\
	"pop eax"\
	"xor eax, ebx"\
	"mov eax, 1"\
	"jne menostinx"\
	"xor eax, eax"\
	"menostinx:"\
	parm nomemory [ecx]\
	modify exact [eax ebx]\
	value [eax]

static void cpuid (int, int *);
#pragma aux cpuid =\
	".586"\
	"cpuid"\
	"mov dword ptr [esi], eax"\
	"mov dword ptr [esi+4], ebx"\
	"mov dword ptr [esi+8], ecx"\
	"mov dword ptr [esi+12], edx"\
	parm [eax][esi]\
	modify exact [eax ebx ecx edx]\
	value

#elif defined(_MSC_VER) && defined(_M_IX86) && USE_ASM

static _inline unsigned int bswap (unsigned int a)
{
	_asm
	{
		mov eax, a
		bswap eax
	}
}

static _inline int bitrev (int b, int c)
{
	_asm
	{
		mov edx, b
		mov ecx, c
		xor eax, eax
 beg: shr edx, 1
		adc eax, eax
		sub ecx, 1
		jnz short beg
	}
}

static _inline int testflag (int c)
{
	_asm
	{
		mov ecx, c
		pushfd
		pop eax
		mov edx, eax
		xor eax, ecx
		push eax
		popfd
		pushfd
		pop eax
		xor eax, edx
		mov eax, 1
		jne menostinx
		xor eax, eax
		menostinx:
	}
}

static _inline void cpuid (int a, int *s)
{
	_asm
	{
		push ebx
		push esi
		mov eax, a
		cpuid
		mov esi, s
		mov dword ptr [esi+0], eax
		mov dword ptr [esi+4], ebx
		mov dword ptr [esi+8], ecx
		mov dword ptr [esi+12], edx
		pop esi
		pop ebx
	}
}

#elif defined(__GNUC__) && defined(__i386__) && USE_ASM

static inline unsigned int bswap (unsigned int a)
{
	__asm__ __volatile__ ("bswap %0" : "+r" (a) : : "cc" );
	return a;
}

static inline int bitrev (int b, int c)
{
	int a;
	__asm__ __volatile__ (
		"xorl %%eax, %%eax\n"
		"0:\n"
		"shrl $1, %%edx\n"
		"adcl %%eax, %%eax\n"
		"subl $1, %%ecx\n"
		"jnz 0b"
		: "+a" (a), "+d" (b), "+c" (c)
		: : "cc");
	return a;
}

static inline int testflag (int c)
{
	int a;
	__asm__ __volatile__ (
		"pushf\n"
		"popl %%eax\n"
		"movl %%eax, %%edx\n"
		"xorl %%ecx, %%eax\n"
		"pushl %%eax\n"
		"popf\n"
		"pushf\n"
		"popl %%eax\n"
		"xorl %%edx, %%eax\n"
		"movl $1, %%eax\n"
		"jne 0f\n"
		"xorl %%eax, %%eax\n"
		"0:"
		: "=a" (a)
		: "c" (c)
		: "edx","cc" );
	return a;
}

static inline void cpuid (int a, int *s)
{
	__asm__ __volatile__ (
		"pushl %%ebx\n"
		"cpuid\n"
		"movl %%eax, (%%esi)\n"
		"movl %%ebx, 4(%%esi)\n"
		"movl %%ecx, 8(%%esi)\n"
		"movl %%edx, 12(%%esi)\n"
		"popl %%ebx\n"
		: "+a" (a)
		: "S" (s)
		: "ecx","edx","memory","cc");
}

#else

#if defined(BIGENDIAN)
static inline unsigned int bswap (unsigned int a)
{
	return(((a&0xff0000)>>8) + ((a&0xff00)<<8) + (a<<24) + (a>>24));
}
#endif

static inline int bitrev (int b, int c)
{
	int i, j;
	for(i=1,j=0,c=(1<<c);i<c;i+=i) { j += j; if (b&i) j++; }
	return(j);
}

static inline int testflag (int c) { (void)c; return(0); }

static inline void cpuid (int a, int *s) { (void)a; (void)s; }

#endif

	//Bit numbers of return value:
	//0:FPU, 4:RDTSC, 15:CMOV, 22:MMX+, 23:MMX, 25:SSE, 26:SSE2, 30:3DNow!+, 31:3DNow!
static int getcputype ()
{
	int i, cpb[4], cpid[4];
	if (!testflag(0x200000)) return(0);
	cpuid(0,cpid); if (!cpid[0]) return(0);
	cpuid(1,cpb); i = (cpb[3]&~((1<<22)|(1<<30)|(1<<31)));
	cpuid(0x80000000,cpb);
	if (((unsigned int)cpb[0]) > 0x80000000)
	{
		cpuid(0x80000001,cpb);
		i |= (cpb[3]&(1<<31));
		if (!((cpid[1]^0x68747541)|(cpid[3]^0x69746e65)|(cpid[2]^0x444d4163))) //AuthenticAMD
			i |= (cpb[3]&((1<<22)|(1<<30)));
	}
	if (i&(1<<25)) i |= (1<<22); //SSE implies MMX+ support
	return(i);
}

static unsigned char fakebuf[8], *nfilptr;
static int nbitpos;
static void suckbitsnextblock ()
{
	int n;

	if (!zipfilmode)
	{
		if (!nfilptr)
		{     //|===|===|crc|lng|typ|===|===|
				//        \  fakebuf: /
				//          |===|===|
				//----x     O---x     O--------
			nbitpos = LSWAPIL(*(int *)&filptr[8]);
			nfilptr = (unsigned char *)&filptr[nbitpos+12];
			*(int *)&fakebuf[0] = *(int *)&filptr[0]; //Copy last dword of IDAT chunk
			if (*(unsigned int *)&filptr[12] == LSWAPIB(0x54414449)) //Copy 1st dword of next IDAT chunk
				*(int *)&fakebuf[4] = *(int *)&filptr[16];
			filptr = &fakebuf[4]; bitpos -= 32;
		}
		else
		{
			filptr = nfilptr; nfilptr = 0;
			bitpos -= ((nbitpos-4)<<3);
		}
		//if (n_from_suckbits < 4) will it crash?
	}
	else
	{
			//NOTE: should only read bytes inside compsize, not 64K!!! :/
		*(int *)&olinbuf[0] = *(int *)&olinbuf[sizeof(olinbuf)-4];
		n = min((kzfs.compleng-kzfs.comptell),(int)sizeof(olinbuf)-4);
		n = (int)fread(&olinbuf[4],1,n,kzfs.fil);
		kzfs.comptell += n;
		bitpos -= ((sizeof(olinbuf)-4)<<3);
	}
}

static inline int peekbits (int n) { return((LSWAPIB(*(int *)&filptr[bitpos>>3])>>(bitpos&7))&pow2mask[n]); }
static inline void suckbits (int n) { bitpos += n; if (bitpos >= 0) suckbitsnextblock(); }
static inline int getbits (int n) { int i = peekbits(n); suckbits(n); return(i); }

static int hufgetsym (int *hitab, int *hbmax)
{
	int v, n;

	v = n = 0;
	do { v = (v<<1)+getbits(1)+hbmax[n]-hbmax[n+1]; n++; } while (v >= 0);
	return(hitab[hbmax[n]+v]);
}

	//This did not result in a speed-up on P4-3.6Ghz (02/22/2005)
//static int hufgetsym_skipb (int *hitab, int *hbmax, int n, int addit)
//{
//   int v;
//
//   v = bitrev(getbits(n),n)+addit;
//   do { v = (v<<1)+getbits(1)+hbmax[n]-hbmax[n+1]; n++; } while (v >= 0);
//   return(hitab[hbmax[n]+v]);
//}

static void qhufgencode (int *hitab, int *hbmax, int *qhval, unsigned char *qhbit, int numbits)
{
	int i, j, k, n, r;

		//r is the bit reverse of i. Ex: if: i = 1011100111, r = 1110011101
	i = r = 0;
	for(n=1;n<=numbits;n++)
		for(k=hbmax[n-1];k<hbmax[n];k++)
			for(j=i+pow2mask[numbits-n];i<=j;i++)
			{
				r = bitrev(i,numbits);
				qhval[r] = hitab[k];
				qhbit[r] = (unsigned char)n;
			}
	for(j=pow2mask[numbits];i<=j;i++)
	{
		r = bitrev(i,numbits);

		//k = 0;
		//for(n=0;n<numbits;n++)
		//   k = (k<<1) + ((r>>n)&1) + hbmax[n]-hbmax[n+1];
		//
		//n = numbits;
		//k = hbmax[n]-r;
		//
		//j = peekbits(LOGQHUFSIZ); i = qhufval[j]; j = qhufbit[j];
		//
		//i = j = 0;
		//do
		//{
		//   i = (i<<1)+getbits(1)+nbuf0[j]-nbuf0[j+1]; j++;
		//} while (i >= 0);
		//i = ibuf0[nbuf0[j]+i];
		//qhval[r] = k;

		qhbit[r] = 0; //n-32;
	}

	//   //hufgetsym_skipb related code:
	//for(k=n=0;n<numbits;n++) k = (k<<1)+hbmax[n]-hbmax[n+1];
	//return(k);
}

	//inbuf[inum] : Bit length of each symbol
	//inum        : Number of indices
	//hitab[inum] : Indices from size-ordered list to original symbol
	//hbmax[0-31] : Highest index (+1) of n-bit symbol
static void hufgencode (int *inbuf, int inum, int *hitab, int *hbmax)
{
	int i, tbuf[31];

	for(i=30;i;i--) tbuf[i] = 0;
	for(i=inum-1;i>=0;i--) tbuf[inbuf[i]]++;
	tbuf[0] = hbmax[0] = 0; //Hack to remove symbols of length 0?
	for(i=0;i<31;i++) hbmax[i+1] = hbmax[i]+tbuf[i];
	for(i=0;i<inum;i++) if (inbuf[i]) hitab[hbmax[inbuf[i]]++] = i;
}

static int initpass () //Interlaced images have 7 "passes", non-interlaced have 1
{
	int i, j, k;

	do
	{
		i = (intlac<<2);
		ixoff = ((0x04020100>>i)&15);
		iyoff = ((0x00402010>>i)&15);
		if (((ixoff >= xsiz) || (iyoff >= ysiz)) && (intlac >= 2)) { i = -1; intlac--; }
	} while (i < 0);
	j = ((0x33221100>>i)&15); ixstp = (1<<j);
	k = ((0x33322110>>i)&15); iystp = (1<<k);

		//xsiz=12      0123456789ab
		//j=3,ixoff=0  0       1       ((12+(1<<3)-1 - 0)>>3) = 2
		//j=3,ixoff=4      2           ((12+(1<<3)-1 - 4)>>3) = 1
		//j=2,ixoff=2    3   4   5     ((12+(1<<2)-1 - 2)>>2) = 3
		//j=1,ixoff=1   6 7 8 9 a b    ((12+(1<<1)-1 - 1)>>1) = 6
	ixsiz = ((xsiz+ixstp-1-ixoff)>>j); //It's confusing! See the above example.
	nbpl = (bytesperline<<k);

		//Initialize this to make filters fast:
	xsizbpl = ((0x04021301>>(coltype<<2))&15)*ixsiz;
	switch (bitdepth)
	{
		case 1: xsizbpl = ((xsizbpl+7)>>3); break;
		case 2: xsizbpl = ((xsizbpl+3)>>2); break;
		case 4: xsizbpl = ((xsizbpl+1)>>1); break;
	}

	memset(olinbuf,0,(xsizbpl+1)*sizeof(olinbuf[0]));
	*(int *)&opixbuf0[0] = *(int *)&opixbuf1[0] = 0;
	xplc = xsizbpl; yplc = globyoffs+iyoff; xm = 0; filt = -1;

	i = globxoffs+ixoff; i = (((-(i>=0))|(ixstp-1))&i);
	k = (((-(yplc>=0))|(iystp-1))&yplc);
	nfplace = k*bytesperline + (i<<2) + frameplace;

		//Precalculate x-clipping to screen borders (speeds up putbuf)
		//Equation: (0 <= xr <= ixsiz) && (0 <= xr*ixstp+globxoffs+ixoff <= xres)
	xr0 = max((-globxoffs-ixoff+(1<<j)-1)>>j,0);
	xr1 = min((xres-globxoffs-ixoff+(1<<j)-1)>>j,ixsiz);
	xr0 = ixsiz-xr0;
	xr1 = ixsiz-xr1;

		  if (coltype == 4) { xr0 = xr0*2;   xr1 = xr1*2;   }
	else if (coltype == 2) { xr0 = xr0*3-2; xr1 = xr1*3-2; }
	else if (coltype == 6) { xr0 = xr0*4-2; xr1 = xr1*4-2; }
	else
	{
		switch(bitdepth)
		{
			case 1: xr0 += ((-ixsiz)&7)+7;
					  xr1 += ((-ixsiz)&7)+7; break;
			case 2: xr0 = ((xr0+((-ixsiz)&3)+3)<<1);
					  xr1 = ((xr1+((-ixsiz)&3)+3)<<1); break;
			case 4: xr0 = ((xr0+((-ixsiz)&1)+1)<<2);
					  xr1 = ((xr1+((-ixsiz)&1)+1)<<2); break;
		}
	}
	ixstp <<= 2;
	return(0);
}

static int Paeth (int a, int b, int c)
{
	int pa, pb, pc;

	pa = b-c; pb = a-c; pc = abs(pa+pb); pa = abs(pa); pb = abs(pb);
	if ((pa <= pb) && (pa <= pc)) return(a);
	if (pb <= pc) return(b); else return(c);
}

#if defined(__WATCOMC__) && USE_ASM

	//NOTE: cmov now has correctly ordered registers (thx to bug fix in 11.0c!)
static int Paeth686 (int, int, int);
#pragma aux Paeth686 =\
	".686"\
	"mov edx, ecx"\
	"sub edx, eax"\
	"sub edx, ebx"\
	"lea edx, abstab10[edx*4+2048]"\
	"mov esi, [ebx*4+edx]"\
	"mov edi, [ecx*4+edx]"\
	"cmp edi, esi"\
	"cmovge edi, esi"\
	"cmovge ecx, ebx"\
	"cmp edi, [eax*4+edx]"\
	"cmovge ecx, eax"\
	parm nomemory [eax][ebx][ecx]\
	modify exact [ecx edx esi edi]\
	value [ecx]

	//Note: "cmove eax,?" may be faster than "jne ?:and eax,?" but who cares
static void rgbhlineasm (int, int, intptr_t, int);
#pragma aux rgbhlineasm =\
	"sub ecx, edx"\
	"jle short endit"\
	"add edx, offset olinbuf"\
	"cmp dword ptr trnsrgb, 0"\
	"jz short begit2"\
	"begit: mov eax, dword ptr [ecx+edx]"\
	"or eax, 0xff000000"\
	"cmp eax, dword ptr trnsrgb"\
	"jne short skipit"\
	"and eax, 0xffffff"\
	"skipit: sub ecx, 3"\
	"mov [edi], eax"\
	"lea edi, [edi+ebx]"\
	"jnz short begit"\
	"jmp short endit"\
	"begit2: mov eax, dword ptr [ecx+edx]"\
	"or eax, 0xff000000"\
	"sub ecx, 3"\
	"mov [edi], eax"\
	"lea edi, [edi+ebx]"\
	"jnz short begit2"\
	"endit:"\
	parm [ecx][edx][edi][ebx]\
	modify exact [eax ecx edi]\
	value

static void pal8hlineasm (int, int, intptr_t, int);
#pragma aux pal8hlineasm =\
	"sub ecx, edx"\
	"jle short endit"\
	"add edx, offset olinbuf"\
	"begit: movzx eax, byte ptr [ecx+edx]"\
	"mov eax, dword ptr palcol[eax*4]"\
	"dec ecx"\
	"mov [edi], eax"\
	"lea edi, [edi+ebx]"\
	"jnz short begit"\
	"endit:"\
	parm [ecx][edx][edi][ebx]\
	modify exact [eax ecx edi]\
	value

#elif defined(_MSC_VER) && defined(_M_IX86) && USE_ASM

static _inline int Paeth686 (int a, int b, int c)
{
	_asm
	{
		mov eax, a
		mov ebx, b
		mov ecx, c
		mov edx, ecx
		sub edx, eax
		sub edx, ebx
		lea edx, abstab10[edx*4+2048]
		mov esi, [ebx*4+edx]
		mov edi, [ecx*4+edx]
		cmp edi, esi
		cmovge edi, esi
		cmovge ecx, ebx
		cmp edi, [eax*4+edx]
		cmovl eax, ecx
	}
}

static _inline void rgbhlineasm (int c, int d, intptr_t t, int b)
{
	_asm
	{
		mov ecx, c
		mov edx, d
		mov edi, t
		mov ebx, b
		sub ecx, edx
		jle short endit
		add edx, offset olinbuf
		cmp dword ptr trnsrgb, 0
		jz short begit2
		begit: mov eax, dword ptr [ecx+edx]
		or eax, 0xff000000
		cmp eax, dword ptr trnsrgb
		jne short skipit
		and eax, 0xffffff
		skipit: sub ecx, 3
		mov [edi], eax
		lea edi, [edi+ebx]
		jnz short begit
		jmp short endit
		begit2: mov eax, dword ptr [ecx+edx]
		or eax, 0xff000000
		sub ecx, 3
		mov [edi], eax
		lea edi, [edi+ebx]
		jnz short begit2
		endit:
	}
}

static _inline void pal8hlineasm (int c, int d, intptr_t t, int b)
{
	_asm
	{
		mov ecx, c
		mov edx, d
		mov edi, t
		mov ebx, b
		sub ecx, edx
		jle short endit
		add edx, offset olinbuf
		begit: movzx eax, byte ptr [ecx+edx]
		mov eax, dword ptr palcol[eax*4]
		sub ecx, 1
		mov [edi], eax
		lea edi, [edi+ebx]
		jnz short begit
		endit:
	}
}

#elif defined(__GNUC__) && defined(__i386__) && USE_ASM

static inline int Paeth686 (int a, int b, int c)
{
	__asm__ __volatile__ (
		"movl %%ecx, %%edx\n"
		"subl %%eax, %%edx\n"
		"subl %%ebx, %%edx\n"
		"leal (%[abstab10]+2048)(,%%edx,4), %%edx\n"
		"movl (%%edx,%%ebx,4), %%esi\n"
		"movl (%%edx,%%ecx,4), %%edi\n"
		"cmpl %%esi, %%edi\n"
		"cmovgel %%esi, %%edi\n"
		"cmovgel %%ebx, %%ecx\n"
		"cmpl (%%edx,%%eax,4), %%edi\n"
		"cmovgel %%eax, %%ecx"
		: "+c" (c)
		: "a" (a), "b" (b), [abstab10] "m" (abstab10[0])
		: "edx", "esi","edi","memory","cc"
		);
	return c;
}

	//Note: "cmove eax,?" may be faster than "jne ?:and eax,?" but who cares
static inline void rgbhlineasm (int c, int d, intptr_t t, int b)
{
	__asm__ __volatile__ (
		"subl %%edx, %%ecx\n"
		"jle 3f\n"
		"addl $%[olinbuf], %%edx\n"
		"cmpl $0, %[trnsrgb](,1)\n"
		"jz 2f\n"
		"0: movl (%%ecx,%%edx,1), %%eax\n"
		"orl $0xff000000, %%eax\n"
		"cmpl %[trnsrgb](,1), %%eax\n"
		"jne 1f\n"
		"andl $0xffffff, %%eax\n"
		"1: subl $3, %%ecx\n"
		"movl %%eax, (%%edi)\n"
		"leal (%%edi,%%ebx,1), %%edi\n"
		"jnz 0b\n"
		"jmp 3f\n"
		"2: movl (%%ecx,%%edx,1), %%eax\n"
		"orl $0xff000000, %%eax\n"
		"subl $3, %%ecx\n"
		"movl %%eax, (%%edi)\n"
		"leal (%%edi,%%ebx,1), %%edi\n"
		"jnz 2b\n"
		"3:"
		: "+c" (c), "+d" (d), "+D" (t)
		: "b" (b), [trnsrgb] "m" (trnsrgb), [olinbuf] "m" (olinbuf[0])
		: "eax","memory","cc"
		);
}

static inline void pal8hlineasm (int c, int d, intptr_t t, int b)
{
	__asm__ __volatile__ (
		"subl %%edx, %%ecx\n"
		"jle 1f\n"
		"addl $%[olinbuf], %%edx\n"
		"0: movzbl (%%ecx,%%edx,1), %%eax\n"
		"movl %[palcol](,%%eax,4), %%eax\n"
		"subl $1, %%ecx\n"
		"movl %%eax, (%%edi)\n"
		"leal (%%edi,%%ebx,1), %%edi\n"
		"jnz 0b\n"
		"1:"
		: "+c" (c), "+d" (d), "+D" (t)
		: "b" (b), [palcol] "m" (palcol[0]), [olinbuf] "m" (olinbuf[0])
		: "eax","memory","cc"
		);
}

#else

static inline int Paeth686 (int a, int b, int c)
{
	return(Paeth(a,b,c));
}

static inline void rgbhlineasm (int x, int xr1, intptr_t p, int ixstp)
{
	int i;
	if (!trnsrgb)
	{
		for(;x>xr1;p+=ixstp,x-=3) *(int *)p = (*(int *)&olinbuf[x])|LSWAPIB(0xff000000);
		return;
	}
	for(;x>xr1;p+=ixstp,x-=3)
	{
		i = (*(int *)&olinbuf[x])|LSWAPIB(0xff000000);
		if (i == trnsrgb) i &= LSWAPIB(0xffffff);
		*(int *)p = i;
	}
}

static inline void pal8hlineasm (int x, int xr1, intptr_t p, int ixstp)
{
	for(;x>xr1;p+=ixstp,x--) *(int *)p = palcol[olinbuf[x]];
}

#endif

	//Autodetect filter
	//    /f0: 0000000...
	//    /f1: 1111111...
	//    /f2: 2222222...
	//    /f3: 1333333...
	//    /f3: 3333333...
	//    /f4: 4444444...
	//    /f5: 0142321...
static int filter1st, filterest;
static void putbuf (const unsigned char *buf, int leng)
{
	int i, x;
	intptr_t p;

	if (filt < 0)
	{
		if (leng <= 0) return;
		filt = buf[0];
		if (filter1st < 0) filter1st = filt; else filterest |= (1<<filt);
		if (filt == gotcmov) filt = 5;
		i = 1;
	} else i = 0;

	while (i < leng)
	{
		x = i+xplc; if (x > leng) x = leng;
		switch (filt)
		{
			case 0:
				while (i < x) { olinbuf[xplc] = buf[i]; xplc--; i++; }
				break;
			case 1:
				while (i < x)
				{
					olinbuf[xplc] = (opixbuf1[xm] += buf[i]);
					xm = xmn[xm]; xplc--; i++;
				}
				break;
			case 2:
				while (i < x) { olinbuf[xplc] += buf[i]; xplc--; i++; }
				break;
			case 3:
				while (i < x)
				{
					opixbuf1[xm] = olinbuf[xplc] = ((opixbuf1[xm]+olinbuf[xplc])>>1)+buf[i];
					xm = xmn[xm]; xplc--; i++;
				}
				break;
			case 4:
				while (i < x)
				{
					opixbuf1[xm] = (unsigned char)(Paeth(opixbuf1[xm],olinbuf[xplc],opixbuf0[xm])+buf[i]);
					opixbuf0[xm] = olinbuf[xplc];
					olinbuf[xplc] = opixbuf1[xm];
					xm = xmn[xm]; xplc--; i++;
				}
				break;
			case 5: //Special hack for Paeth686 (Doesn't have to be case 5)
				while (i < x)
				{
					opixbuf1[xm] = (unsigned char)(Paeth686(opixbuf1[xm],olinbuf[xplc],opixbuf0[xm])+buf[i]);
					opixbuf0[xm] = olinbuf[xplc];
					olinbuf[xplc] = opixbuf1[xm];
					xm = xmn[xm]; xplc--; i++;
				}
				break;
		}

		if (xplc > 0) return;

			//Draw line!
		if ((unsigned int)yplc < (unsigned int)yres)
		{
			x = xr0; p = nfplace;
			switch (coltype)
			{
				case 2:
					rgbhlineasm(x,xr1,p,ixstp);
					break;
				case 4:
					for(;x>xr1;p+=ixstp,x-=2)
					{
#if (PROCESSALPHAHERE == 1)
							//Enable this code to process alpha right here!
						if (olinbuf[x-1] == 255) { *(int *)p = palcol[olinbuf[x]]; continue; }
						if (!olinbuf[x-1]) { *(int *)p = bakcol; continue; }
							//I do >>8, but theoretically should be: /255
						*(unsigned char *)(p) = *(unsigned char *)(p+1) = *(unsigned char *)(p+2) = *(unsigned char *)(p+3) =
							(((((int)olinbuf[x])-bakr)*(int)olinbuf[x-1])>>8) + bakr;
#else
						*(int *)p = (palcol[olinbuf[x]]&LSWAPIB(0xffffff))|LSWAPIL((int)olinbuf[x-1]);
#endif
					}
					break;
				case 6:
					for(;x>xr1;p+=ixstp,x-=4)
					{
#if (PROCESSALPHAHERE == 1)
							//Enable this code to process alpha right here!
						if (olinbuf[x-1] == 255) { *(int *)p = *(int *)&olinbuf[x]; continue; }
						if (!olinbuf[x-1]) { *(int *)p = bakcol; continue; }
							//I do >>8, but theoretically should be: /255
						*(unsigned char *)(p  ) = (((((int)olinbuf[x  ])-bakr)*(int)olinbuf[x-1])>>8) + bakr;
						*(unsigned char *)(p+1) = (((((int)olinbuf[x+1])-bakg)*(int)olinbuf[x-1])>>8) + bakg;
						*(unsigned char *)(p+2) = (((((int)olinbuf[x+2])-bakb)*(int)olinbuf[x-1])>>8) + bakb;
#else
						*(unsigned char *)(p  ) = olinbuf[x  ]; //R
						*(unsigned char *)(p+1) = olinbuf[x+1]; //G
						*(unsigned char *)(p+2) = olinbuf[x+2]; //B
						*(unsigned char *)(p+3) = olinbuf[x-1]; //A
#endif
					}
					break;
				default:
					switch(bitdepth)
					{
						case 1: for(;x>xr1;p+=ixstp,x-- ) *(int *)p = palcol[olinbuf[x>>3]>>(x&7)]; break;
						case 2: for(;x>xr1;p+=ixstp,x-=2) *(int *)p = palcol[olinbuf[x>>3]>>(x&6)]; break;
						case 4: for(;x>xr1;p+=ixstp,x-=4) *(int *)p = palcol[olinbuf[x>>3]>>(x&4)]; break;
						case 8: pal8hlineasm(x,xr1,p,ixstp); break; //for(;x>xr1;p+=ixstp,x--) *(int *)p = palcol[olinbuf[x]]; break;
					}
					break;
			}
			nfplace += nbpl;
		}

		*(int *)&opixbuf0[0] = *(int *)&opixbuf1[0] = 0;
		xplc = xsizbpl; yplc += iystp;
		if ((intlac) && (yplc >= globyoffs+ysiz)) { intlac--; initpass(); }
		if (i < leng)
		{
			filt = buf[i++];
			if (filter1st < 0) filter1st = filt; else filterest |= (1<<filt);
			if (filt == gotcmov) filt = 5;
		} else filt = -1;
	}
}

static void initpngtables()
{
	int i, j, k;

		//hxbit[0-58][0-1] is a combination of 4 different tables:
		//   1st parameter: [0-29] are distances, [30-58] are lengths
		//   2nd parameter: [0]: extra bits, [1]: base number

	j = 1; k = 0;
	for(i=0;i<30;i++)
	{
		hxbit[i][1] = j; j += (1<<k);
		hxbit[i][0] = k; k += ((i&1) && (i >= 2));
	}
	j = 3; k = 0;
	for(i=257;i<285;i++)
	{
		hxbit[i+30-257][1] = j; j += (1<<k);
		hxbit[i+30-257][0] = k; k += ((!(i&3)) && (i >= 264));
	}
	hxbit[285+30-257][1] = 258; hxbit[285+30-257][0] = 0;

	k = getcputype();
	if (k&(1<<15))
	{
		gotcmov = 4;
		for(i=0;i<512;i++) abstab10[512+i] = abstab10[512-i] = i;
	}
}

static int kpngrend (const char *kfilebuf, int kfilength,
	intptr_t daframeplace, int dabytesperline, int daxres, int dayres,
	int daglobxoffs, int daglobyoffs)
{
	int i, j, k, bfinal, btype, hlit, hdist;
	unsigned int leng;
	int slidew, slider;
	//int qhuf0v, qhuf1v;

	if (!pnginited) { pnginited = 1; initpngtables(); }

	if ((*(unsigned int *)&kfilebuf[0] != LSWAPIB(0x474e5089)) || (*(int *)&kfilebuf[4] != LSWAPIB(0x0a1a0a0d)))
		return(-1); //"Invalid PNG file signature"
	filptr = (unsigned char *)&kfilebuf[8];

	trnsrgb = 0; filter1st = -1; filterest = 0;

	while (1)
	{
		leng = LSWAPIL(*(int *)&filptr[0]); i = *(int *)&filptr[4];
		filptr = &filptr[8];
		if (4+leng+((intptr_t)filptr-(intptr_t)kfilebuf) >= kfilength) return(-1); //Chunk length is OOB

		if ((unsigned)i == LSWAPIB(0x52444849)) //IHDR (must be first)
		{
			xsiz = LSWAPIL(*(int *)&filptr[0]); if (xsiz <= 0) return(-1);
			ysiz = LSWAPIL(*(int *)&filptr[4]); if (ysiz <= 0) return(-1);
			bitdepth = filptr[8]; if (!((1<<bitdepth)&0x116)) return(-1); //"Bit depth not supported"
			coltype = filptr[9]; if (!((1<<coltype)&0x5d)) return(-1); //"Color type not supported"
			if (filptr[10]) return(-1); //"Only *flate is supported"
			if (filptr[11]) return(-1); //"Filter not supported"
			if (filptr[12] >= 2) return(-1); //"Unsupported interlace type"
			intlac = filptr[12]*7; //0=no interlace/1=Adam7 interlace

				//Save code by making grayscale look like a palette color scheme
			if ((!coltype) || (coltype == 4))
			{
				j = 0xff000000; k = (255 / ((1<<bitdepth)-1))*0x10101;
				paleng = (1<<bitdepth);
				for(i=0;i<paleng;i++,j+=k) palcol[i] = LSWAPIB(j);
			}
		}
		else if ((unsigned)i == LSWAPIB(0x45544c50)) //PLTE (must be before IDAT)
		{
			paleng = leng/3;
			for(i=paleng-1;i>=0;i--) palcol[i] = LSWAPIB((LSWAPIL(*(int *)&filptr[i*3])>>8)|0xff000000);
		}
		else if ((unsigned)i == LSWAPIB(0x44474b62)) //bKGD (must be after PLTE and before IDAT)
		{
			switch(coltype)
			{
				case 0: case 4:
					bakcol = (((int)filptr[0]<<8)+(int)filptr[1])*255/((1<<bitdepth)-1);
					bakcol = bakcol*0x10101+0xff000000; break;
				case 2: case 6:
					if (bitdepth == 8)
						{ bakcol = (((int)filptr[1])<<16)+(((int)filptr[3])<<8)+((int)filptr[5])+0xff000000; }
					else
					{
						for(i=0,bakcol=0xff000000;i<3;i++)
							bakcol += ((((((int)filptr[i<<1])<<8)+((int)filptr[(i<<1)+1]))/257)<<(16-(i<<3)));
					}
					break;
				case 3:
					bakcol = palcol[filptr[0]]; break;
			}
			bakr = ((bakcol>>16)&255);
			bakg = ((bakcol>>8)&255);
			bakb = (bakcol&255);
			bakcol = LSWAPIB(bakcol);
		}
		else if ((unsigned)i == LSWAPIB(0x534e5274)) //tRNS (must be after PLTE and before IDAT)
		{
			switch(coltype)
			{
				case 0:
					if (bitdepth <= 8)
						palcol[(int)filptr[1]] &= LSWAPIB(0xffffff);
					//else {} // /c0 /d16 not yet supported
					break;
				case 2:
					if (bitdepth == 8)
						{ trnsrgb = LSWAPIB((((int)filptr[1])<<16)+(((int)filptr[3])<<8)+((int)filptr[5])+0xff000000); }
					//else {} //WARNING: PNG docs say: MUST compare all 48 bits :(
					break;
				case 3:
					for(i=min((int)leng,paleng)-1;i>=0;i--)
						palcol[i] &= LSWAPIB((((int)filptr[i])<<24)|0xffffff);
					break;
				default:;
			}
		}
		else if ((unsigned)i == LSWAPIB(0x54414449)) { break; }  //IDAT

		filptr = &filptr[leng+4]; //crc = LSWAPIL(*(int *)&filptr[-4]);
	}

		//Initialize this for the getbits() function
	zipfilmode = 0;
	filptr = &filptr[leng-4]; bitpos = -((leng-4)<<3); nfilptr = 0;
	//if (leng < 4) will it crash?

	frameplace = daframeplace;
	bytesperline = dabytesperline;
	xres = daxres;
	yres = dayres;
	globxoffs = daglobxoffs;
	globyoffs = daglobyoffs;

	switch (coltype)
	{
		case 4: xmn[0] = 1; xmn[1] = 0; break;
		case 2: xmn[0] = 1; xmn[1] = 2; xmn[2] = 0; break;
		case 6: xmn[0] = 1; xmn[1] = 2; xmn[2] = 3; xmn[3] = 0; break;
		default: xmn[0] = 0; break;
	}
	switch (bitdepth)
	{
		case 1: for(i=2;i<256;i++) palcol[i] = palcol[i&1]; break;
		case 2: for(i=4;i<256;i++) palcol[i] = palcol[i&3]; break;
		case 4: for(i=16;i<256;i++) palcol[i] = palcol[i&15]; break;
	}

		//coltype: bitdepth:  format:
		//  0     1,2,4,8,16  I
		//  2           8,16  RGB
		//  3     1,2,4,8     P
		//  4           8,16  IA
		//  6           8,16  RGBA
	xsizbpl = ((0x04021301>>(coltype<<2))&15)*xsiz;
	switch (bitdepth)
	{
		case 1: xsizbpl = ((xsizbpl+7)>>3); break;
		case 2: xsizbpl = ((xsizbpl+3)>>2); break;
		case 4: xsizbpl = ((xsizbpl+1)>>1); break;
	}
		//Tests to see if xsiz > allocated space in olinbuf
		//Note: xsizbpl gets re-written inside initpass()
	if ((xsizbpl+1)*sizeof(olinbuf[0]) > sizeof(olinbuf)) return(-1);

	initpass();

	slidew = 0; slider = 16384;
	zlibcompflags = getbits(16); //Actually 2 fields: 8:compmethflags, 8:addflagscheck
	do
	{
		numhufblocks++;
		bfinal = getbits(1); btype = getbits(2);
		if (btype == 0)
		{
			  //Raw (uncompressed)
			suckbits((-bitpos)&7);  //Synchronize to start of next byte
			i = getbits(16); if ((getbits(16)^i) != 0xffff) return(-1);
			for(;i;i--)
			{
				if (slidew >= slider)
				{
					putbuf(&slidebuf[(slider-16384)&32767],16384); slider += 16384;
					if ((yplc >= yres) && (intlac < 2)) goto kpngrend_goodret;
				}
				slidebuf[(slidew++)&32767] = (unsigned char)getbits(8);
			}
			continue;
		}
		if (btype == 3) continue;

		if (btype == 1) //Fixed Huffman
		{
			hlit = 288; hdist = 32; i = 0;
			for(;i<144;i++) clen[i] = 8; //Fixed bit sizes (literals)
			for(;i<256;i++) clen[i] = 9; //Fixed bit sizes (literals)
			for(;i<280;i++) clen[i] = 7; //Fixed bit sizes (EOI,lengths)
			for(;i<288;i++) clen[i] = 8; //Fixed bit sizes (lengths)
			for(;i<320;i++) clen[i] = 5; //Fixed bit sizes (distances)
		}
		else  //Dynamic Huffman
		{
			hlit = getbits(5)+257; hdist = getbits(5)+1; j = getbits(4)+4;
			for(i=0;i<j;i++) cclen[ccind[i]] = getbits(3);
			for(;i<19;i++) cclen[ccind[i]] = 0;
			hufgencode(cclen,19,ibuf0,nbuf0);

			j = 0; k = hlit+hdist;
			while (j < k)
			{
				i = hufgetsym(ibuf0,nbuf0);
				if (i < 16) { clen[j++] = i; continue; }
				if (i == 16)
					{ for(i=getbits(2)+3;i;i--) { clen[j] = clen[j-1]; j++; } }
				else
				{
					if (i == 17) i = getbits(3)+3; else i = getbits(7)+11;
					for(;i;i--) clen[j++] = 0;
				}
			}
		}

		hufgencode(clen,hlit,ibuf0,nbuf0);
		//qhuf0v = //hufgetsym_skipb related code
		qhufgencode(ibuf0,nbuf0,qhufval0,qhufbit0,LOGQHUFSIZ0);

		hufgencode(&clen[hlit],hdist,ibuf1,nbuf1);
		//qhuf1v = //hufgetsym_skipb related code
		qhufgencode(ibuf1,nbuf1,qhufval1,qhufbit1,LOGQHUFSIZ1);

		while (1)
		{
			if (slidew >= slider)
			{
				putbuf(&slidebuf[(slider-16384)&32767],16384); slider += 16384;
				if ((yplc >= yres) && (intlac < 2)) goto kpngrend_goodret;
			}

			k = peekbits(LOGQHUFSIZ0);
			if (qhufbit0[k]) { i = qhufval0[k]; suckbits((int)qhufbit0[k]); } else i = hufgetsym(ibuf0,nbuf0);
			//else i = hufgetsym_skipb(ibuf0,nbuf0,LOGQHUFSIZ0,qhuf0v); //hufgetsym_skipb related code

			if (i < 256) { slidebuf[(slidew++)&32767] = (unsigned char)i; continue; }
			if (i == 256) break;
			i = getbits(hxbit[i+30-257][0]) + hxbit[i+30-257][1];

			k = peekbits(LOGQHUFSIZ1);
			if (qhufbit1[k]) { j = qhufval1[k]; suckbits((int)qhufbit1[k]); } else j = hufgetsym(ibuf1,nbuf1);
			//else j = hufgetsym_skipb(ibuf1,nbuf1,LOGQHUFSIZ1,qhuf1v); //hufgetsym_skipb related code

			j = getbits(hxbit[j][0]) + hxbit[j][1];
			i += slidew; do { slidebuf[slidew&32767] = slidebuf[(slidew-j)&32767]; slidew++; } while (slidew < i);
		}
	} while (!bfinal);

	slider -= 16384;
	if (!((slider^slidew)&32768))
		putbuf(&slidebuf[slider&32767],slidew-slider);
	else
	{
		putbuf(&slidebuf[slider&32767],(-slider)&32767);
		putbuf(slidebuf,slidew&32767);
	}

kpngrend_goodret:;
	if (!(filterest&~(1<<filter1st))) filtype = filter1st;
	else if ((filter1st == 1) && (!(filterest&~(1<<3)))) filtype = 3;
	else filtype = 5;
	if (coltype == 4) paleng = 0; //For /c4, palcol/paleng used as LUT for "*0x10101": alpha is invalid!
	return(0);
}

//============================= KPNGILIB ends ================================
//============================ KPEGILIB begins ===============================

	//11/01/2000: This code was originally from KPEG.C
	//   All non 32-bit color drawing was removed
	//   "Motion" JPG code was removed
	//   A lot of parameters were added to kpeg() for library usage
static int kpeginited = 0;
static int clipxdim, clipydim;

static int hufmaxatbit[8][20], hufvalatbit[8][20], hufcnt[8];
static unsigned char hufnumatbit[8][20], huftable[8][256];
static int hufquickval[8][1024], hufquickbits[8][1024], hufquickcnt[8];
static int quantab[4][64], dct[12][64], lastdc[4], unzig[64], zigit[64]; //dct:10=MAX (says spec);+2 for hacks
static unsigned char gnumcomponents, dcflagor[64];
static int gcompid[4], gcomphsamp[4], gcompvsamp[4], gcompquantab[4], gcomphsampshift[4], gcompvsampshift[4];
static int lnumcomponents, lcompid[4], lcompdc[4], lcompac[4], lcomphsamp[4], lcompvsamp[4], lcompquantab[4];
static int lcomphvsamp0, lcomphsampshift0, lcompvsampshift0;
static int colclip[1024], colclipup8[1024], colclipup16[1024];
//static unsigned char pow2char[8] = {1,2,4,8,16,32,64,128};

#if defined(__WATCOMC__) && USE_ASM

static int mulshr24 (int, int);
#pragma aux mulshr24 =\
	"imul edx"\
	"shrd eax, edx, 24"\
	parm nomemory [eax][edx]\
	modify exact [eax edx]

static int mulshr32 (int, int);
#pragma aux mulshr32 =\
	"imul edx"\
	parm nomemory [eax][edx]\
	modify exact [eax edx]\
	value [edx]

#elif defined(_MSC_VER) && defined(_M_IX86) && USE_ASM

static _inline int mulshr24 (int a, int d)
{
	_asm
	{
		mov eax, a
		imul d
		shrd eax, edx, 24
	}
}

static _inline int mulshr32 (int a, int d)
{
	_asm
	{
		mov eax, a
		imul d
		mov eax, edx
	}
}

#elif defined(__GNUC__) && defined(__i386__) && USE_ASM

#define mulshr24(a,d) \
	({ int __a=(a), __d=(d); \
		__asm__ __volatile__ ("imull %%edx; shrdl $24, %%edx, %%eax" \
		: "+a" (__a), "+d" (__d) : : "cc"); \
	 __a; })

#define mulshr32(a,d) \
	({ int __a=(a), __d=(d); \
		__asm__ __volatile__ ("imull %%edx" \
		: "+a" (__a), "+d" (__d) : : "cc"); \
	 __d; })

#else

static inline int mulshr24 (int a, int b)
{
	return((int)((((__int64)a)*((__int64)b))>>24));
}

static inline int mulshr32 (int a, int b)
{
	return((int)((((__int64)a)*((__int64)b))>>32));
}

#endif

static int cosqr16[8] =    //cosqr16[i] = ((cos(PI*i/16)*sqrt(2))<<24);
  {23726566,23270667,21920489,19727919,16777216,13181774,9079764,4628823};
static int crmul[4096], cbmul[4096];

static void initkpeg ()
{
	int i, x, y;

	x = 0;  //Back & forth diagonal pattern (aligning bytes for best compression)
	for(i=0;i<16;i+=2)
	{
		for(y=8-1;y>=0;y--)
			if ((unsigned)(i-y) < (unsigned)8) unzig[x++] = (y<<3)+i-y;
		for(y=0;y<8;y++)
			if ((unsigned)(i+1-y) < (unsigned)8) unzig[x++] = (y<<3)+i+1-y;
	}
	for(i=64-1;i>=0;i--) zigit[unzig[i]] = i;
	for(i=64-1;i>=0;i--) dcflagor[i] = (unsigned char)(1<<(unzig[i]>>3));

	for(i=0;i<128;i++) colclip[i] = i+128;
	for(i=128;i<512;i++) colclip[i] = 255;
	for(i=512;i<896;i++) colclip[i] = 0;
	for(i=896;i<1024;i++) colclip[i] = i-896;
	for(i=0;i<1024;i++)
	{
		colclipup8[i] = (colclip[i]<<8);
		colclipup16[i] = (colclip[i]<<16)+0xff000000; //Hack: set alphas to 255
	}
#if defined(BIGENDIAN)
	for(i=0;i<1024;i++)
	{
		colclip[i] = bswap(colclip[i]);
		colclipup8[i] = bswap(colclipup8[i]);
		colclipup16[i] = bswap(colclipup16[i]);
	}
#endif

	for(i=0;i<2048;i++)
	{
		crmul[(i<<1)+0] = (i-1024)*1470104; //1.402*1048576
		crmul[(i<<1)+1] = (i-1024)*-748830; //-0.71414*1048576
		cbmul[(i<<1)+0] = (i-1024)*-360857; //-0.34414*1048576
		cbmul[(i<<1)+1] = (i-1024)*1858077; //1.772*1048576
	}

	memset((void *)&dct[10][0],0,64*2*sizeof(dct[0][0]));
}

static void huffgetval (int index, int curbits, int num, int *daval, int *dabits)
{
	int b, v, pow2, *hmax;

	hmax = &hufmaxatbit[index][0];
	pow2 = pow2long[curbits-1];
	if (num&pow2) v = 1; else v = 0;
	for(b=1;b<=16;b++)
	{
		if (v < hmax[b])
		{
			*dabits = b;
			*daval = huftable[index][hufvalatbit[index][b]+v];
			return;
		}
		pow2 >>= 1; v <<= 1;
		if (num&pow2) v++;
	}
	*dabits = 16; *daval = 0;
}

static void invdct8x8 (int *dc, unsigned char dcflag)
{
	#define SQRT2 23726566   //(sqrt(2))<<24
	#define C182 31000253    //(cos(PI/8)*2)<<24
	#define C18S22 43840978  //(cos(PI/8)*sqrt(2)*2)<<24
	#define C38S22 18159528  //(cos(PI*3/8)*sqrt(2)*2)<<24
	int *edc, t0, t1, t2, t3, t4, t5, t6, t7;

	edc = dc+64;
	do
	{
		if (dcflag&1) //pow2char[z])
		{
			t3 = dc[2] + dc[6];
			t2 = (mulshr32(dc[2]-dc[6],SQRT2<<6)<<2) - t3;
			t4 = dc[0] + dc[4]; t5 = dc[0] - dc[4];
			t0 = t4+t3; t3 = t4-t3; t1 = t5+t2; t2 = t5-t2;
			t4 = (mulshr32(dc[5]-dc[3]+dc[1]-dc[7],C182<<6)<<2);
			t7 = dc[1] + dc[7] + dc[5] + dc[3];
			t6 = (mulshr32(dc[3]-dc[5],C18S22<<5)<<3) + t4 - t7;
			t5 = (mulshr32(dc[1]+dc[7]-dc[5]-dc[3],SQRT2<<6)<<2) - t6;
			t4 = (mulshr32(dc[1]-dc[7],C38S22<<6)<<2) - t4 + t5;
			dc[0] = t0+t7; dc[7] = t0-t7; dc[1] = t1+t6; dc[6] = t1-t6;
			dc[2] = t2+t5; dc[5] = t2-t5; dc[4] = t3+t4; dc[3] = t3-t4;
		}
		dc += 8; dcflag >>= 1;
	} while (dc < edc);
	dc -= 32; edc -= 24;
	do
	{
		t3 = dc[2*8-32] + dc[6*8-32];
		t2 = (mulshr32(dc[2*8-32]-dc[6*8-32],SQRT2<<6)<<2) - t3;
		t4 = dc[0*8-32] + dc[4*8-32]; t5 = dc[0*8-32] - dc[4*8-32];
		t0 = t4+t3; t3 = t4-t3; t1 = t5+t2; t2 = t5-t2;
		t4 = (mulshr32(dc[5*8-32]-dc[3*8-32]+dc[1*8-32]-dc[7*8-32],C182<<6)<<2);
		t7 = dc[1*8-32] + dc[7*8-32] + dc[5*8-32] + dc[3*8-32];
		t6 = (mulshr32(dc[3*8-32]-dc[5*8-32],C18S22<<5)<<3) + t4 - t7;
		t5 = (mulshr32(dc[1*8-32]+dc[7*8-32]-dc[5*8-32]-dc[3*8-32],SQRT2<<6)<<2) - t6;
		t4 = (mulshr32(dc[1*8-32]-dc[7*8-32],C38S22<<6)<<2) - t4 + t5;
		dc[0*8-32] = t0+t7; dc[7*8-32] = t0-t7; dc[1*8-32] = t1+t6; dc[6*8-32] = t1-t6;
		dc[2*8-32] = t2+t5; dc[5*8-32] = t2-t5; dc[4*8-32] = t3+t4; dc[3*8-32] = t3-t4;
		dc++;
	} while (dc < edc);
}

static void yrbrend (int x, int y)
{
	int i, j, ox, oy, xx, yy, xxx, yyy, xxxend, yyyend, yv, cr=0, cb=0, *odc, *dc, *dc2;
	intptr_t p, pp;

	odc = dct[0]; dc2 = dct[10];
	for(yy=0;yy<(lcompvsamp[0]<<3);yy+=8)
	{
		oy = y+yy+globyoffs; if ((unsigned)oy >= (unsigned)clipydim) { odc += (lcomphsamp[0]<<6); continue; }
		pp = oy*bytesperline + ((x+globxoffs)<<2) + frameplace;
		for(xx=0;xx<(lcomphsamp[0]<<3);xx+=8,odc+=64)
		{
			ox = x+xx+globxoffs; if ((unsigned)ox >= (unsigned)clipxdim) continue;
			p = pp+(xx<<2);
			dc = odc;
			if (lnumcomponents > 1) dc2 = &dct[lcomphvsamp0][((yy>>lcompvsampshift0)<<3)+(xx>>lcomphsampshift0)];
			xxxend = min(clipxdim-ox,8);
			yyyend = min(clipydim-oy,8);
			if ((lcomphsamp[0] == 1) && (xxxend == 8))
			{
				for(yyy=0;yyy<yyyend;yyy++)
				{
					for(xxx=0;xxx<8;xxx++)
					{
						yv = dc[xxx];
						cr = (dc2[xxx+64]>>13)&~1;
						cb = (dc2[xxx   ]>>13)&~1;
						((int *)p)[xxx] = colclipup16[(unsigned)(yv+crmul[cr+2048]               )>>22]+
												  colclipup8[(unsigned)(yv+crmul[cr+2049]+cbmul[cb+2048])>>22]+
													  colclip[(unsigned)(yv+cbmul[cb+2049]               )>>22];
					}
					p += bytesperline;
					dc += 8;
					if (!((yyy+1)&(lcompvsamp[0]-1))) dc2 += 8;
				}
			}
			else if ((lcomphsamp[0] == 2) && (xxxend == 8))
			{
				for(yyy=0;yyy<yyyend;yyy++)
				{
					for(xxx=0;xxx<8;xxx+=2)
					{
						yv = dc[xxx];
						cr = (dc2[(xxx>>1)+64]>>13)&~1;
						cb = (dc2[(xxx>>1)   ]>>13)&~1;
						i = crmul[cr+2049]+cbmul[cb+2048];
						cr = crmul[cr+2048];
						cb = cbmul[cb+2049];
						((int *)p)[xxx] = colclipup16[(unsigned)(yv+cr)>>22]+
												  colclipup8[(unsigned)(yv+ i)>>22]+
													  colclip[(unsigned)(yv+cb)>>22];
						yv = dc[xxx+1];
						((int *)p)[xxx+1] = colclipup16[(unsigned)(yv+cr)>>22]+
													 colclipup8[(unsigned)(yv+ i)>>22]+
														 colclip[(unsigned)(yv+cb)>>22];
					}
					p += bytesperline;
					dc += 8;
					if (!((yyy+1)&(lcompvsamp[0]-1))) dc2 += 8;
				}
			}
			else
			{
				for(yyy=0;yyy<yyyend;yyy++)
				{
					i = 0; j = 1;
					for(xxx=0;xxx<xxxend;xxx++)
					{
						yv = dc[xxx];
						j--;
						if (!j)
						{
							j = lcomphsamp[0];
							cr = (dc2[i+64]>>13)&~1;
							cb = (dc2[i   ]>>13)&~1;
							i++;
						}
						((int *)p)[xxx] = colclipup16[(unsigned)(yv+crmul[cr+2048]               )>>22]+
												  colclipup8[(unsigned)(yv+crmul[cr+2049]+cbmul[cb+2048])>>22]+
													  colclip[(unsigned)(yv+cbmul[cb+2049]               )>>22];
					}
					p += bytesperline;
					dc += 8;
					if (!((yyy+1)&(lcompvsamp[0]-1))) dc2 += 8;
				}
			}
		}
	}
}

static int kpegrend (const char *kfilebuf, int kfilength,
	intptr_t daframeplace, int dabytesperline, int daxres, int dayres,
	int daglobxoffs, int daglobyoffs)
{
	int i, j, v, leng=0, xdim=0, ydim=0, index, prec, restartcnt, restartinterval;
	int x, y, z, xx, yy, zz, *dc=NULL, num, curbits, c, daval, dabits, *hqval, *hqbits, hqcnt, *quanptr;
	int passcnt = 0, ghsampmax=0, gvsampmax=0, glhsampmax=0, glvsampmax=0, glhstep, glvstep;
	int eobrun, Ss, Se, Ah, Al, Alut[2], dctx[12], dcty[12], ldctx[12], /*ldcty[12],*/ lshx[4], lshy[4];
	short *dctbuf = NULL, *dctptr[12], *ldctptr[12], *dcs=NULL;
	unsigned char ch, marker, dcflag;
	const unsigned char *kfileptr;

	if (!kpeginited) { kpeginited = 1; initkpeg(); }

	kfileptr = (unsigned char *)kfilebuf;

	if (*(unsigned short *)kfileptr == SSWAPIB(0xd8ff)) kfileptr += 2;
	else return(-1); //"%s is not a JPEG file\n",filename

	restartinterval = 0;
	for(i=0;i<4;i++) lastdc[i] = 0;
	for(i=0;i<8;i++) hufcnt[i] = 0;

	coltype = 0; bitdepth = 8; //For PNGOUT
	do
	{
		ch = *kfileptr++; if (ch != 255) continue;
		do { marker = *kfileptr++; } while (marker == 255);
		if (marker != 0xd9) //Don't read past end of buffer
		{
			leng = ((int)kfileptr[0]<<8)+(int)kfileptr[1]-2;
			kfileptr += 2;
		}
		//printf("fileoffs=%08x, marker=%02x,leng=%d",((int)kfileptr)-((int)kfilebuf)-2,marker,leng);
		switch(marker)
		{
			case 0xc0: case 0xc1: case 0xc2:
					//processit!
				kfileptr++; //numbits = *kfileptr++;

				ydim = SSWAPIL(*(unsigned short *)&kfileptr[0]);
				xdim = SSWAPIL(*(unsigned short *)&kfileptr[2]);
				//printf("%s: %ld / %ld = %ld\n",filename,xdim*ydim*3,kfilength,(xdim*ydim*3)/kfilength);

				frameplace = daframeplace;
				bytesperline = dabytesperline;
				xres = daxres;
				yres = dayres;
				globxoffs = daglobxoffs;
				globyoffs = daglobyoffs;

				gnumcomponents = kfileptr[4];
				kfileptr += 5;
				ghsampmax = gvsampmax = glhsampmax = glvsampmax = 0;
				for(z=0;z<gnumcomponents;z++)
				{
					gcompid[z] = kfileptr[0];
					gcomphsamp[z] = (kfileptr[1]>>4);
					gcompvsamp[z] = (kfileptr[1]&15);
					gcompquantab[z] = kfileptr[2];
					for(i=0;i<8;i++) if (gcomphsamp[z] == pow2long[i]) { gcomphsampshift[z] = i; break; }
					for(i=0;i<8;i++) if (gcompvsamp[z] == pow2long[i]) { gcompvsampshift[z] = i; break; }
					if (gcomphsamp[z] > ghsampmax) { ghsampmax = gcomphsamp[z]; glhsampmax = gcomphsampshift[z]; }
					if (gcompvsamp[z] > gvsampmax) { gvsampmax = gcompvsamp[z]; glvsampmax = gcompvsampshift[z]; }
					kfileptr += 3;
				}

				break;
			case 0xc4:  //Huffman table
				do
				{
					ch = *kfileptr++; leng--;
					if (ch >= 16) { index = ch-12; }
								else { index = ch; }
					memcpy((void *)&hufnumatbit[index][1],(void *)kfileptr,16); kfileptr += 16;
					leng -= 16;

					v = 0; hufcnt[index] = 0;
					hufquickcnt[index] = 0;
					for(i=1;i<=16;i++)
					{
						hufmaxatbit[index][i] = v+hufnumatbit[index][i];
						hufvalatbit[index][i] = hufcnt[index]-v;
						memcpy((void *)&huftable[index][hufcnt[index]],(void *)kfileptr,(int)hufnumatbit[index][i]);
						if (i <= 10)
							for(c=0;c<hufnumatbit[index][i];c++)
								for(j=(1<<(10-i));j>0;j--)
								{
									hufquickval[index][hufquickcnt[index]] = huftable[index][hufcnt[index]+c];
									hufquickbits[index][hufquickcnt[index]] = i;
									hufquickcnt[index]++;
								}
						kfileptr += hufnumatbit[index][i];
						leng -= hufnumatbit[index][i];
						hufcnt[index] += hufnumatbit[index][i];
						v = ((v+hufnumatbit[index][i])<<1);
					}

				} while (leng > 0);
				break;
			case 0xdb:
				do
				{
					ch = *kfileptr++; leng--;
					index = (ch&15);
					prec = (ch>>4);
					for(z=0;z<64;z++)
					{
						v = (int)(*kfileptr++);
						if (prec) v = (v<<8)+((int)(*kfileptr++));
						v <<= 19;
						if (unzig[z]&7 ) v = mulshr24(v,cosqr16[unzig[z]&7 ]);
						if (unzig[z]>>3) v = mulshr24(v,cosqr16[unzig[z]>>3]);
						if (index) v >>= 6;
						quantab[index][unzig[z]] = v;
					}
					leng -= 64;
					if (prec) leng -= 64;
				} while (leng > 0);
				break;
			case 0xdd:
				restartinterval = SSWAPIL(*(unsigned short *)&kfileptr[0]);
				kfileptr += leng;
				break;
			case 0xda:
				if ((xdim <= 0) || (ydim <= 0)) { if (dctbuf) free(dctbuf); return(-1); }

				lnumcomponents = (int)(*kfileptr++); if (!lnumcomponents) { if (dctbuf) free(dctbuf); return(-1); }
				if (lnumcomponents > 1) coltype = 2;
				for(z=0;z<lnumcomponents;z++)
				{
					lcompid[z] = kfileptr[0];
					lcompdc[z] = (kfileptr[1]>>4);
					lcompac[z] = (kfileptr[1]&15);
					kfileptr += 2;
				}

				Ss = kfileptr[0];
				Se = kfileptr[1];
				Ah = (kfileptr[2]>>4);
				Al = (kfileptr[2]&15);
				kfileptr += 3;
				//printf("passcnt=%d, Ss=%d, Se=%d, Ah=%d, Al=%d\n",passcnt,Ss,Se,Ah,Al);

				if ((!passcnt) && ((Ss) || (Se != 63) || (Ah) || (Al)))
				{
					for(z=zz=0;z<gnumcomponents;z++)
					{
						dctx[z] = ((xdim+(ghsampmax<<3)-1)>>(glhsampmax+3)) << gcomphsampshift[z];
						dcty[z] = ((ydim+(gvsampmax<<3)-1)>>(glvsampmax+3)) << gcompvsampshift[z];
						zz += dctx[z]*dcty[z];
					}
					z = zz*64*sizeof(short);
					dctbuf = (short *)malloc(z); if (!dctbuf) return(-1);
					memset(dctbuf,0,z);
					for(z=zz=0;z<gnumcomponents;z++) { dctptr[z] = &dctbuf[zz*64]; zz += dctx[z]*dcty[z]; }
				}

				glhstep = glvstep = 0x7fffffff;
				for(z=0;z<lnumcomponents;z++)
					for(zz=0;zz<gnumcomponents;zz++)
						if (lcompid[z] == gcompid[zz])
						{
							ldctptr[z] = dctptr[zz];
							ldctx[z] = dctx[zz];
							//ldcty[z] = dcty[zz];
							lcomphsamp[z] = gcomphsamp[zz];
							lcompvsamp[z] = gcompvsamp[zz];
							lcompquantab[z] = gcompquantab[zz];
							if (!z)
							{
								lcomphsampshift0 = gcomphsampshift[zz];
								lcompvsampshift0 = gcompvsampshift[zz];
							}
							lshx[z] = glhsampmax-gcomphsampshift[zz]+3;
							lshy[z] = glvsampmax-gcompvsampshift[zz]+3;
							if (gcomphsampshift[zz] < glhstep) glhstep = gcomphsampshift[zz];
							if (gcompvsampshift[zz] < glvstep) glvstep = gcompvsampshift[zz];
						}
				glhstep = (ghsampmax>>glhstep); lcomphsamp[0] = min(lcomphsamp[0],glhstep); glhstep <<= 3;
				glvstep = (gvsampmax>>glvstep); lcompvsamp[0] = min(lcompvsamp[0],glvstep); glvstep <<= 3;
				lcomphvsamp0 = lcomphsamp[0]*lcompvsamp[0];

				clipxdim = min(xdim+globxoffs,xres);
				clipydim = min(ydim+globyoffs,yres);

				if ((max(globxoffs,0) >= xres) || (min(globxoffs+xdim,xres) <= 0) ||
					 (max(globyoffs,0) >= yres) || (min(globyoffs+ydim,yres) <= 0))
					{ if (dctbuf) free(dctbuf); return(0); }

				Alut[0] = (1<<Al); Alut[1] = -Alut[0];

				restartcnt = restartinterval; eobrun = 0; marker = 0xd0;
				num = 0; curbits = 0;
				for(y=0;y<ydim;y+=glvstep)
					for(x=0;x<xdim;x+=glhstep)
					{
						if (kfileptr-4-(unsigned char *)kfilebuf >= kfilength) goto kpegrend_break2; //rest of file is missing!

						if (!dctbuf) dc = dct[0];
						for(c=0;c<lnumcomponents;c++)
						{
							hqval = &hufquickval[lcompac[c]+4][0];
							hqbits = &hufquickbits[lcompac[c]+4][0];
							hqcnt = hufquickcnt[lcompac[c]+4];
							if (!dctbuf) quanptr = &quantab[lcompquantab[c]][0];
							for(yy=0;yy<(lcompvsamp[c]<<3);yy+=8)
								for(xx=0;xx<(lcomphsamp[c]<<3);xx+=8)
								{  //NOTE: Might help to split this code into firstime vs. refinement (!Ah vs. Ah!=0)

									if (dctbuf) dcs = &ldctptr[c][(((y+yy)>>lshy[c])*ldctx[c] + ((x+xx)>>lshx[c]))<<6];

										//Get DC
									if (!Ss)
									{
										while (curbits < 24) //Getbits
										{
											ch = *kfileptr++; if (ch == 255) kfileptr++;
											num = (num<<8)+((int)ch); curbits += 8;
										}

										if (!Ah)
										{
											i = ((num>>(curbits-10))&1023);
											if (i < hufquickcnt[lcompdc[c]])
												  { daval = hufquickval[lcompdc[c]][i]; curbits -= hufquickbits[lcompdc[c]][i]; }
											else { huffgetval(lcompdc[c],curbits,num,&daval,&dabits); curbits -= dabits; }

											if (daval)
											{
												while (curbits < 24) //Getbits
												{
													ch = *kfileptr++; if (ch == 255) kfileptr++;
													num = (num<<8)+((int)ch); curbits += 8;
												}

												curbits -= daval; v = ((unsigned)num >> curbits) & pow2mask[daval];
												if (v <= pow2mask[daval-1]) v -= pow2mask[daval];
												lastdc[c] += v;
											}
											if (!dctbuf) dc[0] = lastdc[c]; else dcs[0] = (short)(lastdc[c]<<Al);
										}
										else if (num&(pow2long[--curbits])) dcs[0] |= ((short)Alut[0]);
									}

										//Get AC
									if (!dctbuf) memset((void *)&dc[1],0,63*4);
									z = max(Ss,1); dcflag = 1;
									if (eobrun <= 0)
									{
										for(;z<=Se;z++)
										{
											while (curbits < 24) //Getbits
											{
												ch = *kfileptr++; if (ch == 255) kfileptr++;
												num = (num<<8)+((int)ch); curbits += 8;
											}
											i = ((num>>(curbits-10))&1023);
											if (i < hqcnt)
												  { daval = hqval[i]; curbits -= hqbits[i]; }
											else { huffgetval(lcompac[c]+4,curbits,num,&daval,&dabits); curbits -= dabits; }

											zz = (daval>>4); daval &= 15;
											if (daval)
											{
												if (Ah)
												{
													//NOTE: Getbits not needed here - buffer should have enough bits
													if (num&(pow2long[--curbits])) daval = Alut[0]; else daval = Alut[1];
												}
											}
											else if (zz < 15)
											{
												eobrun = pow2long[zz];
												if (zz)
												{
													while (curbits < 24) //Getbits
													{
														ch = *kfileptr++; if (ch == 255) kfileptr++;
														num = (num<<8)+((int)ch); curbits += 8;
													}
													curbits -= zz; eobrun += ((unsigned)num >> curbits) & pow2mask[zz];
												}
												if (!Ah) eobrun--;
												break;
											}
											if (Ah)
											{
												do
												{
													if (dcs[z])
													{
														while (curbits < 24) //Getbits
														{
															ch = *kfileptr++; if (ch == 255) kfileptr++;
															num = (num<<8)+((int)ch); curbits += 8;
														}
														if (num&(pow2long[--curbits])) dcs[z] += ((short)Alut[dcs[z] < 0]);
												  } else if (--zz < 0) break;
												  z++;
												} while (z <= Se);
												if (daval) dcs[z] = daval;
											}
											else
											{
												z += zz; if (z > Se) break;

												while (curbits < 24) //Getbits
												{
													ch = *kfileptr++; if (ch == 255) kfileptr++;
													num = (num<<8)+((int)ch); curbits += 8;
												}
												curbits -= daval; v = ((unsigned)num >> curbits) & pow2mask[daval];
												if (v <= pow2mask[daval-1]) v -= pow2mask[daval];
												dcflag |= dcflagor[z];
												if (!dctbuf) dc[unzig[z]] = v; else dcs[z] = (short)(v<<Al);
											}
										}
									} else if (!Ah) eobrun--;
									if ((Ah) && (eobrun > 0))
									{
										eobrun--;
										for(;z<=Se;z++)
										{
											if (!dcs[z]) continue;
											while (curbits < 24) //Getbits
											{
												ch = *kfileptr++; if (ch == 255) kfileptr++;
												num = (num<<8)+((int)ch); curbits += 8;
											}
											if (num&(pow2long[--curbits])) dcs[z] += ((short)Alut[dcs[z] < 0]);
										}
									}

									if (!dctbuf)
									{
										for(z=64-1;z>=0;z--) dc[z] *= quanptr[z];
										invdct8x8(dc,dcflag); dc += 64;
									}
								}
							}

						if (!dctbuf) yrbrend(x,y);

						restartcnt--;
						if (!restartcnt)
						{
							kfileptr += 1-(curbits>>3); curbits = 0;
							if ((kfileptr[-2] != 255) || (kfileptr[-1] != marker)) kfileptr--;
							marker++; if (marker >= 0xd8) marker = 0xd0;
							restartcnt = restartinterval;
							for(i=0;i<4;i++) lastdc[i] = 0;
							eobrun = 0;
						}
					}
kpegrend_break2:;
				if (!dctbuf) return(0);
				passcnt++; kfileptr -= ((curbits>>3)+1); break;
			case 0xd9: break;
			default: kfileptr += leng; break;
		}
	} while (kfileptr-(unsigned char *)kfilebuf < kfilength);

	if (!dctbuf) return(0);

	lnumcomponents = gnumcomponents;
	for(i=0;i<gnumcomponents;i++)
	{
		lcomphsamp[i] = gcomphsamp[i]; gcomphsamp[i] <<= 3;
		lcompvsamp[i] = gcompvsamp[i]; gcompvsamp[i] <<= 3;
		lshx[i] = glhsampmax-gcomphsampshift[i]+3;
		lshy[i] = glvsampmax-gcompvsampshift[i]+3;
	}
	lcomphsampshift0 = gcomphsampshift[0];
	lcompvsampshift0 = gcompvsampshift[0];
	lcomphvsamp0 = (lcomphsamp[0]<<lcompvsampshift0);
	for(y=0;y<ydim;y+=gcompvsamp[0])
		for(x=0;x<xdim;x+=gcomphsamp[0])
		{
			dc = dct[0];
			for(c=0;c<gnumcomponents;c++)
				for(yy=0;yy<gcompvsamp[c];yy+=8)
					for(xx=0;xx<gcomphsamp[c];xx+=8,dc+=64)
					{
						dcs = &dctptr[c][(((y+yy)>>lshy[c])*dctx[c] + ((x+xx)>>lshx[c]))<<6];
						quanptr = &quantab[gcompquantab[c]][0];
						for(z=0;z<64;z++) dc[z] = ((int)dcs[zigit[z]])*quanptr[z];
						invdct8x8(dc,-1);
					}
			yrbrend(x,y);
		}

	free(dctbuf); return(0);
}

//==============================  KPEGILIB ends ==============================
//================================ GIF begins ================================

static unsigned char suffix[4100], filbuffer[768], tempstack[4096];
static int prefix[4100];

static int kgifrend (const char *kfilebuf, int kfilelength,
	intptr_t daframeplace, int dabytesperline, int daxres, int dayres,
	int daglobxoffs, int daglobyoffs)
{
	int i, x, y, xsiz, ysiz, yinc, xend, xspan, yspan, currstr, numbitgoal;
	int lzcols, dat, blocklen, bitcnt, xoff, yoff, transcol, backcol, *lptr;
	intptr_t p=0;
	unsigned char numbits, startnumbits, chunkind, ilacefirst;
	const unsigned char *ptr, *cptr=NULL;

	(void)kfilelength;

	coltype = 3; bitdepth = 8; //For PNGOUT

	if ((kfilebuf[0] != 'G') || (kfilebuf[1] != 'I') ||
		 (kfilebuf[2] != 'F') || (kfilebuf[12])) return(-1);
	paleng = (1<<((kfilebuf[10]&7)+1));
	ptr = (unsigned char *)&kfilebuf[13];
	if (kfilebuf[10]&128) { cptr = ptr; ptr += paleng*3; }
	transcol = -1;
	while ((chunkind = *ptr++) == '!')
	{      //! 0xf9 leng flags ?? ?? transcol
		if (ptr[0] == 0xf9) { if (ptr[2]&1) transcol = (int)(((unsigned char)ptr[5])); }
		ptr++;
		do { i = *ptr++; ptr += i; } while (i);
	}
	if (chunkind != ',') return(-1);

	xoff = SSWAPIB(*(unsigned short *)&ptr[0]);
	yoff = SSWAPIB(*(unsigned short *)&ptr[2]);
	xspan = SSWAPIB(*(unsigned short *)&ptr[4]);
	yspan = SSWAPIB(*(unsigned short *)&ptr[6]); ptr += 9;
	if (ptr[-1]&64) { yinc = 8; ilacefirst = 1; }
				  else { yinc = 1; ilacefirst = 0; }
	if (ptr[-1]&128)
	{
		paleng = (1<<((ptr[-1]&7)+1));
		cptr = ptr; ptr += paleng*3;
	}

	for(i=0;i<paleng;i++)
		palcol[i] = LSWAPIB((((int)cptr[i*3])<<16) + (((int)cptr[i*3+1])<<8) + ((int)cptr[i*3+2]) + 0xff000000);
	for(;i<256;i++) palcol[i] = LSWAPIB(0xff000000);
	if (transcol >= 0) palcol[transcol] &= LSWAPIB(~0xff000000);

		//Handle GIF files with different logical&image sizes or non-0 offsets (added 05/15/2004)
	xsiz = SSWAPIB(*(unsigned short *)&kfilebuf[6]);
	ysiz = SSWAPIB(*(unsigned short *)&kfilebuf[8]);
	if ((xoff != 0) || (yoff != 0) || (xsiz != xspan) || (ysiz != yspan))
	{
		int xx[4], yy[4];
		if (kfilebuf[10]&128) backcol = palcol[(unsigned char)kfilebuf[11]]; else backcol = 0;

			//Fill border to backcol
		xx[0] = max(daglobxoffs           ,     0); yy[0] = max(daglobyoffs           ,     0);
		xx[1] = min(daglobxoffs+xoff      ,daxres); yy[1] = min(daglobyoffs+yoff      ,dayres);
		xx[2] = max(daglobxoffs+xoff+xspan,     0); yy[2] = min(daglobyoffs+yoff+yspan,dayres);
		xx[3] = min(daglobxoffs+xsiz      ,daxres); yy[3] = min(daglobyoffs+ysiz      ,dayres);

		lptr = (int *)(yy[0]*dabytesperline+daframeplace);
		for(y=yy[0];y<yy[1];y++,lptr=(int *)(((intptr_t)lptr)+dabytesperline))
			for(x=xx[0];x<xx[3];x++) lptr[x] = backcol;
		for(;y<yy[2];y++,lptr=(int *)(((intptr_t)lptr)+dabytesperline))
		{  for(x=xx[0];x<xx[1];x++) lptr[x] = backcol;
			for(x=xx[2];x<xx[3];x++) lptr[x] = backcol;
		}
		for(;y<yy[3];y++,lptr=(int *)(((intptr_t)lptr)+dabytesperline))
			for(x=xx[0];x<xx[3];x++) lptr[x] = backcol;

		daglobxoffs += xoff; //Offset bitmap image by extra amount
		daglobyoffs += yoff;
	}

	xspan += daglobxoffs;
	yspan += daglobyoffs;  //UGLY HACK
	y = daglobyoffs;
	if ((unsigned int)y < (unsigned int)dayres)
		{ p = y*dabytesperline+daframeplace; x = daglobxoffs; xend = xspan; }
	else
		{ x = daglobxoffs+0x80000000; xend = xspan+0x80000000; }

	lzcols = (1<<(*ptr)); startnumbits = (unsigned char)((*ptr)+1); ptr++;
	for(i=lzcols-1;i>=0;i--) { suffix[i] = (unsigned char)(prefix[i] = i); }
	currstr = lzcols+2; numbits = startnumbits; numbitgoal = (lzcols<<1);
	blocklen = *ptr++;
	memcpy(filbuffer,ptr,blocklen); ptr += blocklen;
	bitcnt = 0;
	while (1)
	{
		dat = (LSWAPIB(*(int *)&filbuffer[bitcnt>>3])>>(bitcnt&7)) & (numbitgoal-1);
		bitcnt += numbits;
		if ((bitcnt>>3) > blocklen-3)
		{
			*(short *)filbuffer = *(short *)&filbuffer[bitcnt>>3];
			i = blocklen-(bitcnt>>3);
			blocklen = (int)*ptr++;
			memcpy(&filbuffer[i],ptr,blocklen); ptr += blocklen;
			bitcnt &= 7; blocklen += i;
		}
		if (dat == lzcols)
		{
			currstr = lzcols+2; numbits = startnumbits; numbitgoal = (lzcols<<1);
			continue;
		}
		if ((currstr == numbitgoal) && (numbits < 12))
			{ numbits++; numbitgoal <<= 1; }

		prefix[currstr] = dat;
		for(i=0;dat>=lzcols;dat=prefix[dat]) tempstack[i++] = suffix[dat];
		tempstack[i] = (unsigned char)prefix[dat];
		suffix[currstr-1] = suffix[currstr] = (unsigned char)dat;

		for(;i>=0;i--)
		{
			if ((unsigned int)x < (unsigned int)daxres)
				*(int *)(p+(x<<2)) = palcol[(int)tempstack[i]];
			x++;
			if (x == xend)
			{
				y += yinc;
				if (y >= yspan)
					switch(yinc)
					{
						case 8: if (!ilacefirst) { y = daglobyoffs+2; yinc = 4; break; }
								  ilacefirst = 0; y = daglobyoffs+4; yinc = 8; break;
						case 4: y = daglobyoffs+1; yinc = 2; break;
						case 2: case 1: return(0);
					}
				if ((unsigned int)y < (unsigned int)dayres)
					{ p = y*dabytesperline+daframeplace; x = daglobxoffs; xend = xspan; }
				else
					{ x = daglobxoffs+0x80000000; xend = xspan+0x80000000; }
			}
		}
		currstr++;
	}
}

//===============================  GIF ends ==================================
//==============================  CEL begins =================================

	//   //old .CEL format:
	//short id = 0x9119, xdim, ydim, xoff, yoff, id = 0x0008;
	//int imagebytes, filler[4];
	//char pal6bit[256][3], image[ydim][xdim];
static int kcelrend (const char *buf, int fleng,
	intptr_t daframeplace, int dabytesperline, int daxres, int dayres,
	int daglobxoffs, int daglobyoffs)
{
	int i, x, y, x0, x1, y0, y1, xsiz, ysiz;
	const unsigned char *cptr;

	(void)fleng; (void)daglobxoffs;

	if (((unsigned char)buf[0] != 0x19) || ((unsigned char)buf[1] != 0x91) ||
		 ((unsigned char)buf[10] != 8) || ((unsigned char)buf[11] != 0)) return(-1);

	coltype = 3; bitdepth = 8; paleng = 256; //For PNGOUT

	xsiz = (int)SSWAPIB(*(unsigned short *)&buf[2]); if (xsiz <= 0) return(-1);
	ysiz = (int)SSWAPIB(*(unsigned short *)&buf[4]); if (ysiz <= 0) return(-1);

	cptr = (unsigned char *)&buf[32];
	for(i=0;i<256;i++)
	{
		palcol[i] = (((int)cptr[0])<<18) +
						(((int)cptr[1])<<10) +
						(((int)cptr[2])<< 2) + LSWAPIB(0xff000000);
		cptr += 3;
	}

	x0 = daglobyoffs; x1 = xsiz+daglobyoffs;
	y0 = daglobyoffs; y1 = ysiz+daglobyoffs;
	for(y=y0;y<y1;y++)
		for(x=x0;x<x1;x++)
		{
			if (((unsigned int)x < (unsigned int)daxres) && ((unsigned int)y < (unsigned int)dayres))
				*(int *)(y*dabytesperline+x*4+daframeplace) = palcol[cptr[0]];
			cptr++;
		}
	return(0);
}

//===============================  CEL ends ==================================
//=============================  TARGA begins ================================

static int ktgarend (const char *header, int fleng,
	intptr_t daframeplace, int dabytesperline, int daxres, int dayres,
	int daglobxoffs, int daglobyoffs)
{
	int i=0, x, y, pi, xi, yi, x0, x1, y0, y1, xsiz, ysiz, rlestat, colbyte, pixbyte;
	intptr_t p;
	const unsigned char *fptr, *cptr=0, *nptr;

		//Ugly and unreliable identification for .TGA!
	if ((fleng < 20) || (header[1]&0xfe)) return(-1);
	if ((header[2] >= 12) || (!((1<<header[2])&0xe0e))) return(-1);
	if ((header[16]&7) || (header[16] == 0) || (header[16] > 32)) return(-1);
	if (header[17]&0xc0) return(-1);

	fptr = (unsigned char *)&header[header[0]+18];
	xsiz = (int)SSWAPIB(*(unsigned short *)&header[12]); if (xsiz <= 0) return(-1);
	ysiz = (int)SSWAPIB(*(unsigned short *)&header[14]); if (ysiz <= 0) return(-1);
	colbyte = ((((int)header[16])+7)>>3);

	if (header[1] == 1)
	{
		pixbyte = ((((int)header[7])+7)>>3);
		cptr = &fptr[-SSWAPIB(*(unsigned short *)&header[3])*pixbyte];
		fptr += SSWAPIB(*(unsigned short *)&header[5])*pixbyte;
	} else pixbyte = colbyte;

	switch(pixbyte) //For PNGOUT
	{
		case 1: coltype = 0; bitdepth = 8; palcol[0] = LSWAPIB(0xff000000);
				  for(i=1;i<256;i++) palcol[i] = palcol[i-1]+LSWAPIB(0x10101);
				  break;
		case 2: case 3: coltype = 2; break;
		case 4: coltype = 6; break;
	}

	if (!(header[17]&16)) { x0 = 0;      x1 = xsiz; xi = 1; }
						  else { x0 = xsiz-1; x1 = -1;   xi =-1; }
	if (header[17]&32) { y0 = 0;      y1 = ysiz; yi = 1; pi = dabytesperline; }
					  else { y0 = ysiz-1; y1 = -1;   yi =-1; pi =-dabytesperline; }
	x0 += daglobxoffs; y0 += daglobyoffs;
	x1 += daglobxoffs; y1 += daglobyoffs;
	if (header[2] < 8) rlestat = -2; else rlestat = -1;

	p = y0*dabytesperline+daframeplace;
	for(y=y0;y!=y1;y+=yi,p+=pi)
		for(x=x0;x!=x1;x+=xi)
		{
			if (rlestat < 128)
			{
				if ((rlestat&127) == 127) { rlestat = (int)fptr[0]; fptr++; }
				if (header[1] == 1)
				{
					if (colbyte == 1) i = fptr[0];
									 else i = (int)SSWAPIB(*(unsigned short *)&fptr[0]);
					nptr = &cptr[i*pixbyte];
				} else nptr = fptr;

				switch(pixbyte)
				{
					case 1: i = palcol[(int)nptr[0]]; break;
					case 2: i = (int)SSWAPIB(*(unsigned short *)&nptr[0]);
						i = LSWAPIB(((i&0x7c00)<<9) + ((i&0x03e0)<<6) + ((i&0x001f)<<3) + 0xff000000);
						break;
					case 3: i = (*(int *)&nptr[0]) | LSWAPIB(0xff000000); break;
					case 4: i = (*(int *)&nptr[0]); break;
				}
				fptr += colbyte;
			}
			if (rlestat >= 0) rlestat--;

			if (((unsigned int)x < (unsigned int)daxres) && ((unsigned int)y < (unsigned int)dayres))
				*(int *)(x*4+p) = i;
		}
	return(0);
}

//==============================  TARGA ends =================================
//==============================  BMP begins =================================
	//TODO: handle BI_RLE8 and BI_RLE4 (compression types 1&2 respectively)
	//                        ┌───────────────┐
	//                        │  0(2): "BM"   │
	// ┌─────────────────────┐│ 10(4): rastoff│ ┌──────────────────┐
	// │headsiz=12 (OS/2 1.x)││ 14(4): headsiz│ │ All new formats: │
	//┌┴─────────────────────┴┴─────────────┬─┴─┴──────────────────┴───────────────────────┐
	//│ 18(2): xsiz                         │ 18(4): xsiz                                  │
	//│ 20(2): ysiz                         │ 22(4): ysiz                                  │
	//│ 22(2): planes (always 1)            │ 26(2): planes (always 1)                     │
	//│ 24(2): cdim (1,4,8,24)              │ 28(2): cdim (1,4,8,16,24,32)                 │
	//│ if (cdim < 16)                      │ 30(4): compression (0,1,2,3!?,4)             │
	//│    26(rastoff-14-headsiz): pal(bgr) │ 34(4): (bitmap data size+3)&3                │
	//│                                     │ 46(4): N colors (0=2^cdim)                   │
	//│                                     │ if (cdim < 16)                               │
	//│                                     │    14+headsiz(rastoff-14-headsiz): pal(bgr0) │
	//└─────────────────────┬───────────────┴─────────┬────────────────────────────────────┘
	//                      │ rastoff(?): bitmap data │
	//                      └─────────────────────────┘
static int kbmprend (const char *buf, int fleng,
	intptr_t daframeplace, int dabytesperline, int daxres, int dayres,
	int daglobxoffs, int daglobyoffs)
{
	int i, j, x, y, x0, x1, y0, y1, rastoff, headsiz, xsiz, ysiz, cdim, comp, cptrinc, *lptr;
	const unsigned char *cptr, *ubuf = (const unsigned char *)buf;

	(void)fleng;

	headsiz = *(int *)&buf[14];
	if (headsiz == LSWAPIB(12)) //OS/2 1.x (old format)
	{
		if (*(short *)(&buf[22]) != SSWAPIB(1)) return(-1);
		xsiz = (int)SSWAPIB(*(unsigned short *)&buf[18]);
		ysiz = (int)SSWAPIB(*(unsigned short *)&buf[20]);
		cdim = (int)SSWAPIB(*(unsigned short *)&buf[24]);
		comp = 0;
	}
	else //All newer formats...
	{
		if (*(short *)(&buf[26]) != SSWAPIB(1)) return(-1);
		xsiz = LSWAPIB(*(int *)&buf[18]);
		ysiz = LSWAPIB(*(int *)&buf[22]);
		cdim = (int)SSWAPIB(*(unsigned short *)&buf[28]);
		comp = LSWAPIB(*(int *)&buf[30]);
	}
	if ((xsiz <= 0) || (!ysiz)) return(-1);
		//cdim must be: (1,4,8,16,24,32)
	if (((unsigned int)(cdim-1) >= (unsigned int)32) || (!((1<<cdim)&0x1010113))) return(-1);
	if ((comp != 0) && (comp != 3)) return(-1);

	rastoff = LSWAPIB(*(int *)&buf[10]);

	if (cdim < 16)
	{
		if (cdim == 2) { palcol[0] = 0xffffffff; palcol[1] = LSWAPIB(0xff000000); }
		if (headsiz == LSWAPIB(12)) j = 3; else j = 4;
		for(i=0,cptr=&ubuf[headsiz+14];cptr<&ubuf[rastoff];i++,cptr+=j)
			palcol[i] = ((*(int *)&cptr[0])|LSWAPIB(0xff000000));
		coltype = 3; bitdepth = cdim; paleng = i; //For PNGOUT
	}
	else if (!(cdim&15))
	{
		coltype = 2;
		switch(cdim)
		{
			case 16: palcol[0] = 10; palcol[1] = 5; palcol[2] = 0; palcol[3] = 5; palcol[4] = 5; palcol[5] = 5; break;
			case 32: palcol[0] = 16; palcol[1] = 8; palcol[2] = 0; palcol[3] = 8; palcol[4] = 8; palcol[5] = 8; break;
		}
		if (comp == 3) //BI_BITFIELD (RGB masks)
		{
			for(i=0;i<3;i++)
			{
				j = *(int *)&buf[headsiz+(i<<2)+14];
				for(palcol[i]=0;palcol[i]<32;palcol[i]++)
				{
					if (j&1) break;
					j = (((unsigned int)j)>>1);
				}
				for(palcol[i+3]=0;palcol[i+3]<32;palcol[i+3]++)
				{
					if (!(j&1)) break;
					j = (((unsigned int)j)>>1);
				}
			}
		}
		palcol[0] = 24-(palcol[0]+palcol[3]);
		palcol[1] = 16-(palcol[1]+palcol[4]);
		palcol[2] =  8-(palcol[2]+palcol[5]);
		palcol[3] = (-(1<<(24-palcol[3])))&0x00ff0000;
		palcol[4] = (-(1<<(16-palcol[4])))&0x0000ff00;
		palcol[5] = (-(1<<( 8-palcol[5])))&0x000000ff;
	}

	cptrinc = (((xsiz*cdim+31)>>3)&~3); cptr = &ubuf[rastoff];
	if (ysiz < 0) { ysiz = -ysiz; } else { cptr = &cptr[(ysiz-1)*cptrinc]; cptrinc = -cptrinc; }

	x0 = daglobxoffs; x1 = xsiz+daglobxoffs;
	y0 = daglobyoffs; y1 = ysiz+daglobyoffs;
	if ((x0 >= daxres) || (x1 <= 0) || (y0 >= dayres) || (y1 <= 0)) return(0);
	if (x0 < 0) x0 = 0;
	if (x1 > daxres) x1 = daxres;
	for(y=y0;y<y1;y++,cptr=&cptr[cptrinc])
	{
		if ((unsigned int)y >= (unsigned int)dayres) continue;
		lptr = (int *)(y*dabytesperline-(daglobyoffs<<2)+daframeplace);
		switch(cdim)
		{
			case  1: for(x=x0;x<x1;x++) lptr[x] = palcol[(int)((cptr[x>>3]>>((x&7)^7))&1)]; break;
			case  4: for(x=x0;x<x1;x++) lptr[x] = palcol[(int)((cptr[x>>1]>>(((x&1)^1)<<2))&15)]; break;
			case  8: for(x=x0;x<x1;x++) lptr[x] = palcol[(int)(cptr[x])]; break;
			case 16: for(x=x0;x<x1;x++)
						{
							i = ((int)(*(short *)&cptr[x<<1]));
							lptr[x] = (_lrotl(i,palcol[0])&palcol[3]) +
										 (_lrotl(i,palcol[1])&palcol[4]) +
										 (_lrotl(i,palcol[2])&palcol[5]) + LSWAPIB(0xff000000);
						} break;
			case 24: for(x=x0;x<x1;x++) lptr[x] = ((*(int *)&cptr[x*3])|LSWAPIB(0xff000000)); break;
			case 32: for(x=x0;x<x1;x++)
						{
							i = (*(int *)&cptr[x<<2]);
							lptr[x] = (_lrotl(i,palcol[0])&palcol[3]) +
										 (_lrotl(i,palcol[1])&palcol[4]) +
										 (_lrotl(i,palcol[2])&palcol[5]) + LSWAPIB(0xff000000);
						} break;
		}

	}
	return(0);
}
//===============================  BMP ends ==================================
//==============================  PCX begins =================================
	//Note: currently only supports 8 and 24 bit PCX
static int kpcxrend (const char *buf, int fleng,
	intptr_t daframeplace, int dabytesperline, int daxres, int dayres,
	int daglobxoffs, int daglobyoffs)
{
	int i, j, x, y, nplanes, x0, x1, y0, y1, xsiz, ysiz;
	intptr_t p;
	unsigned char c, *cptr;

	if (*(int *)buf != LSWAPIB(0x0801050a)) return(-1);
	xsiz = SSWAPIB(*(short *)&buf[ 8])-SSWAPIB(*(short *)&buf[4])+1; if (xsiz <= 0) return(-1);
	ysiz = SSWAPIB(*(short *)&buf[10])-SSWAPIB(*(short *)&buf[6])+1; if (ysiz <= 0) return(-1);

		//buf[3]: bpp/plane:{1,2,4,8}
		//bpl = *(short *)&buf[66];  //#bytes per scanline. Must be EVEN. May have unused data.
		//nplanes*bpl bytes per scanline; always be decoding break at the end of scan line

	nplanes = buf[65];
	if (nplanes == 1)
	{
		//if (buf[fleng-769] != 12) return(-1); //Some PCX are buggy!
		cptr = (unsigned char *)&buf[fleng-768];
		for(i=0;i<256;i++)
		{
			palcol[i] = (((int)cptr[0])<<16) +
							(((int)cptr[1])<< 8) +
							(((int)cptr[2])    ) + LSWAPIB(0xff000000);
			cptr += 3;
		}
		coltype = 3; bitdepth = 8; paleng = 256; //For PNGOUT
	}
	else if (nplanes == 3)
	{
		coltype = 2;

			//Make sure background is opaque (since 24-bit PCX renderer doesn't do it)
		x0 = max(daglobxoffs,0); x1 = min(xsiz+daglobxoffs,daxres);
		y0 = max(daglobyoffs,0); y1 = min(ysiz+daglobyoffs,dayres);
		p = y0*dabytesperline + daframeplace+3;
		for(y=y0;y<y1;y++,p+=dabytesperline)
			for(x=x0;x<x1;x++) *(unsigned char *)((x<<2)+p) = 255;
	}

	x = x0 = daglobxoffs; x1 = xsiz+daglobxoffs;
	y = y0 = daglobyoffs; y1 = ysiz+daglobyoffs;
	cptr = (unsigned char *)&buf[128];
	p = y*dabytesperline+daframeplace;

	j = nplanes-1; daxres <<= 2; x0 <<= 2; x1 <<= 2; x <<= 2; x += j;
	if (nplanes == 1) //8-bit PCX
	{
		do
		{
			c = *cptr++; if (c < 192) i = 1; else { i = (c&63); c = *cptr++; }
			j = palcol[(int)c];
			for(;i;i--)
			{
				if ((unsigned int)y < (unsigned int)dayres)
					if ((unsigned int)x < (unsigned int)daxres) *(int *)(x+p) = j;
				x += 4; if (x >= x1) { x = x0; y++; p += dabytesperline; }
			}
		} while (y < y1);
	}
	else if (nplanes == 3) //24-bit PCX
	{
		do
		{
			c = *cptr++; if (c < 192) i = 1; else { i = (c&63); c = *cptr++; }
			for(;i;i--)
			{
				if ((unsigned int)y < (unsigned int)dayres)
					if ((unsigned int)x < (unsigned int)daxres) *(unsigned char *)(x+p) = c;
				x += 4; if (x >= x1) { j--; if (j < 0) { j = 3-1; y++; p += dabytesperline; } x = x0+j; }
			}
		} while (y < y1);
	}

	return(0);
}

//===============================  PCX ends ==================================
//==============================  DDS begins =================================

	//Note:currently supports: DXT1,DXT2,DXT3,DXT4,DXT5,A8R8G8B8
static int kddsrend (const char *buf, int leng,
	intptr_t frameptr, int bpl, int xdim, int ydim, int xoff, int yoff)
{
	int x, y, z=0, xx, yy, xsiz, ysiz, dxt, al[2], ai, j, k, v, c0, c1, stride;
	intptr_t p;
	unsigned int lut[256], r[4], g[4], b[4], a[8], rr, gg, bb;
	unsigned char *uptr, *wptr;

	(void)leng;

	xsiz = LSWAPIB(*(int *)&buf[16]);
	ysiz = LSWAPIB(*(int *)&buf[12]);
	if ((*(int *)&buf[80])&LSWAPIB(64)) //Uncompressed supports only A8R8G8B8 for now
	{
		if ((*(unsigned int *)&buf[88]) != LSWAPIB(32)) return(-1);
		if ((*(unsigned int *)&buf[92]) != LSWAPIB(0x00ff0000)) return(-1);
		if ((*(unsigned int *)&buf[96]) != LSWAPIB(0x0000ff00)) return(-1);
		if ((*(unsigned int *)&buf[100]) != LSWAPIB(0x000000ff)) return(-1);
		if ((*(unsigned int *)&buf[104]) != LSWAPIB(0xff000000)) return(-1);
		buf += 128;

		p = yoff*bpl + (xoff<<2) + frameptr; xx = (xsiz<<2);
		if (xoff < 0) { p -= (xoff<<2); buf -= (xoff<<2); xsiz += xoff; }
		xsiz = (min(xsiz,xdim-xoff)<<2); ysiz = min(ysiz,ydim);
		for(y=0;y<ysiz;y++,p+=bpl,buf+=xx)
		{
			if ((unsigned int)(y+yoff) >= (unsigned int)ydim) continue;
			memcpy((void *)p,(void *)buf,xsiz);
		}
		return(0);
	}
	if (!((*(int *)&buf[80])&LSWAPIB(4))) return(-1); //FOURCC invalid
	dxt = buf[87]-'0';
	if ((buf[84] != 'D') || (buf[85] != 'X') || (buf[86] != 'T') || (dxt < 1) || (dxt > 5)) return(-1);
	buf += 128;

	if (!(dxt&1))
	{
		for(z=256-1;z>0;z--) lut[z] = (255<<16)/z;
		lut[0] = (1<<16);
	}
	if (dxt == 1) stride = (xsiz<<1); else stride = (xsiz<<2);

	for(y=0;y<ysiz;y+=4,buf+=stride)
		for(x=0;x<xsiz;x+=4)
		{
			if (dxt == 1) uptr = (unsigned char *)(((intptr_t)buf)+(x<<1));
						else uptr = (unsigned char *)(((intptr_t)buf)+(x<<2)+8);
			c0 = SSWAPIB(*(unsigned short *)&uptr[0]);
			r[0] = ((c0>>8)&0xf8); g[0] = ((c0>>3)&0xfc); b[0] = ((c0<<3)&0xfc); a[0] = 255;
			c1 = SSWAPIB(*(unsigned short *)&uptr[2]);
			r[1] = ((c1>>8)&0xf8); g[1] = ((c1>>3)&0xfc); b[1] = ((c1<<3)&0xfc); a[1] = 255;
			if ((c0 > c1) || (dxt != 1))
			{
				r[2] = (((r[0]*2 + r[1] + 1)*(65536/3))>>16);
				g[2] = (((g[0]*2 + g[1] + 1)*(65536/3))>>16);
				b[2] = (((b[0]*2 + b[1] + 1)*(65536/3))>>16); a[2] = 255;
				r[3] = (((r[0] + r[1]*2 + 1)*(65536/3))>>16);
				g[3] = (((g[0] + g[1]*2 + 1)*(65536/3))>>16);
				b[3] = (((b[0] + b[1]*2 + 1)*(65536/3))>>16); a[3] = 255;
			}
			else
			{
				r[2] = (r[0] + r[1])>>1;
				g[2] = (g[0] + g[1])>>1;
				b[2] = (b[0] + b[1])>>1; a[2] = 255;
				r[3] = g[3] = b[3] = a[3] = 0; //Transparent
			}
			v = LSWAPIB(*(int *)&uptr[4]);
			if (dxt >= 4)
			{
				a[0] = uptr[-8]; a[1] = uptr[-7]; k = a[1]-a[0];
				if (k < 0)
				{
					z = a[0]*6 + a[1] + 3;
					for(j=2;j<8;j++) { a[j] = ((z*(65536/7))>>16); z += k; }
				}
				else
				{
					z = a[0]*4 + a[1] + 2;
					for(j=2;j<6;j++) { a[j] = ((z*(65536/5))>>16); z += k; }
					a[6] = 0; a[7] = 255;
				}
				al[0] = LSWAPIB(*(int *)&uptr[-6]);
				al[1] = LSWAPIB(*(int *)&uptr[-3]);
			}
			wptr = (unsigned char *)((y+yoff)*bpl + ((x+xoff)<<2) + frameptr);
			ai = 0;
			for(yy=0;yy<4;yy++,wptr+=bpl)
			{
				if ((unsigned int)(y+yy+yoff) >= (unsigned int)ydim) { ai += 4; continue; }
				for(xx=0;xx<4;xx++,ai++)
				{
					if ((unsigned int)(x+xx+xoff) >= (unsigned int)xdim) continue;

					j = ((v>>(ai<<1))&3);
					switch(dxt)
					{
						case 1: z = a[j]; break;
						case 2: case 3: z = (( uptr[(ai>>1)-8] >> ((xx&1)<<2) )&15)*17; break;
						case 4: case 5: z = a[( al[yy>>1] >> ((ai&7)*3) )&7]; break;
					}
					rr = r[j]; gg = g[j]; bb = b[j];
					if (!(dxt&1))
					{
						bb = min((bb*lut[z])>>16,255);
						gg = min((gg*lut[z])>>16,255);
						rr = min((rr*lut[z])>>16,255);
					}
					wptr[(xx<<2)+0] = bb;
					wptr[(xx<<2)+1] = gg;
					wptr[(xx<<2)+2] = rr;
					wptr[(xx<<2)+3] = z;
				}
			}
		}
	return(0);
}

//===============================  DDS ends ==================================
//=================== External picture interface begins ======================

int kpgetdim (const char *buf, int leng, int *xsiz, int *ysiz)
{
	int *lptr;
	const unsigned char *cptr;
	unsigned char *ubuf = (unsigned char *)buf;

	(*xsiz) = (*ysiz) = 0; if (leng < 16) return(-1);
	if ((ubuf[0] == 0x89) && (ubuf[1] == 0x50)) //.PNG
	{
		lptr = (int *)buf;
		if ((lptr[0] != LSWAPIB(0x474e5089)) || (lptr[1] != LSWAPIB(0x0a1a0a0d))) return(-1);
		lptr = &lptr[2];
		while (((uintptr_t)lptr-(uintptr_t)buf) < (uintptr_t)(leng-16))
		{
			if (lptr[1] == LSWAPIB(0x52444849)) //IHDR
				{ (*xsiz) = LSWAPIL(lptr[2]); (*ysiz) = LSWAPIL(lptr[3]); break; }
			lptr = (int *)((intptr_t)lptr + LSWAPIL(lptr[0]) + 12);
		}
	}
	else if ((ubuf[0] == 0xff) && (ubuf[1] == 0xd8)) //.JPG
	{
		cptr = (unsigned char *)&buf[2];
		while (((uintptr_t)cptr-(uintptr_t)buf) < (uintptr_t)(leng-8))
		{
			if (cptr[0] != 255) { cptr = &cptr[1]; continue; }
			if ((unsigned int)(cptr[1]-0xc0) < 3)
			{
				(*ysiz) = SSWAPIL(*(unsigned short *)&cptr[5]);
				(*xsiz) = SSWAPIL(*(unsigned short *)&cptr[7]);
				break;
			}
			cptr = &cptr[SSWAPIL(*(unsigned short *)&cptr[2])+2];
		}
	}
	else if ((ubuf[0] == 'G') && (ubuf[1] == 'I') && (ubuf[2] == 'F') && (ubuf[12] == 0)) //.GIF
	{
		(*xsiz) = (int)SSWAPIB(*(unsigned short *)&buf[6]);
		(*ysiz) = (int)SSWAPIB(*(unsigned short *)&buf[8]);
	}
	else if ((ubuf[0] == 0x19) && (ubuf[1] == 0x91) && (ubuf[10] == 8) && (ubuf[11] == 0)) //old .CEL/.PIC
	{
		(*xsiz) = (int)SSWAPIB(*(unsigned short *)&buf[2]);
		(*ysiz) = (int)SSWAPIB(*(unsigned short *)&buf[4]);
	}
	else if ((ubuf[0] == 'B') && (ubuf[1] == 'M')) //.BMP
	{
		if (*(int *)(&buf[14]) == LSWAPIB(12)) //OS/2 1.x (old format)
		{
			if (*(short *)(&buf[22]) != SSWAPIB(1)) return(-1);
			(*xsiz) = (int)SSWAPIB(*(unsigned short *)&buf[18]);
			(*ysiz) = (int)SSWAPIB(*(unsigned short *)&buf[20]);
		}
		else //All newer formats...
		{
			if (*(short *)(&buf[26]) != SSWAPIB(1)) return(-1);
			(*xsiz) = LSWAPIB(*(int *)&buf[18]);
			(*ysiz) = LSWAPIB(*(int *)&buf[22]);
		}
	}
	else if (*(int *)ubuf == LSWAPIB(0x0801050a)) //.PCX
	{
		(*xsiz) = SSWAPIB(*(short *)&buf[ 8])-SSWAPIB(*(short *)&buf[4])+1;
		(*ysiz) = SSWAPIB(*(short *)&buf[10])-SSWAPIB(*(short *)&buf[6])+1;
	}
	else if ((*(int *)ubuf == LSWAPIB(0x20534444)) && (*(int *)&ubuf[4] == LSWAPIB(124))) //.DDS
	{
		(*xsiz) = LSWAPIB(*(int *)&buf[16]);
		(*ysiz) = LSWAPIB(*(int *)&buf[12]);
	}
	else
	{     //Unreliable .TGA identification - this MUST be final case!
		if ((leng >= 20) && (!(ubuf[1]&0xfe)))
			if ((ubuf[2] < 12) && ((1<<ubuf[2])&0xe0e))
				if ((!(ubuf[16]&7)) && (ubuf[16] != 0) && (ubuf[16] <= 32))
					if (!(buf[17]&0xc0))
					{
						(*xsiz) = (int)SSWAPIB(*(unsigned short *)&buf[12]);
						(*ysiz) = (int)SSWAPIB(*(unsigned short *)&buf[14]);
					}
	}
	return(0);
}

int kprender (const char *buf, int leng, intptr_t frameptr, int bpl,
					int xdim, int ydim, int xoff, int yoff)
{
	unsigned char *ubuf = (unsigned char *)buf;

	paleng = 0; bakcol = 0; numhufblocks = zlibcompflags = 0; filtype = -1;

	if ((ubuf[0] == 0x89) && (ubuf[1] == 0x50)) //.PNG
		return(kpngrend(buf,leng,frameptr,bpl,xdim,ydim,xoff,yoff));

	if ((ubuf[0] == 0xff) && (ubuf[1] == 0xd8)) //.JPG
		return(kpegrend(buf,leng,frameptr,bpl,xdim,ydim,xoff,yoff));

	if ((ubuf[0] == 'G') && (ubuf[1] == 'I') && (ubuf[2] == 'F')) //.GIF
		return(kgifrend(buf,leng,frameptr,bpl,xdim,ydim,xoff,yoff));

	if ((ubuf[0] == 0x19) && (ubuf[1] == 0x91) && (ubuf[10] == 8) && (ubuf[11] == 0)) //old .CEL/.PIC
		return(kcelrend(buf,leng,frameptr,bpl,xdim,ydim,xoff,yoff));

	if ((ubuf[0] == 'B') && (ubuf[1] == 'M')) //.BMP
		return(kbmprend(buf,leng,frameptr,bpl,xdim,ydim,xoff,yoff));

	if (*(int *)ubuf == LSWAPIB(0x0801050a)) //.PCX
		return(kpcxrend(buf,leng,frameptr,bpl,xdim,ydim,xoff,yoff));

	if ((*(int *)ubuf == LSWAPIB(0x20534444)) && (*(int *)&ubuf[4] == LSWAPIB(124))) //.DDS
		return(kddsrend(buf,leng,frameptr,bpl,xdim,ydim,xoff,yoff));

		//Unreliable .TGA identification - this MUST be final case!
	if ((leng >= 20) && (!(ubuf[1]&0xfe)))
		if ((ubuf[2] < 12) && ((1<<ubuf[2])&0xe0e))
			if ((!(ubuf[16]&7)) && (ubuf[16] != 0) && (ubuf[16] <= 32))
				if (!(ubuf[17]&0xc0))
					return(ktgarend(buf,leng,frameptr,bpl,xdim,ydim,xoff,yoff));

	return(-1);
}

//==================== External picture interface ends =======================

	//Brute-force case-insensitive, slash-insensitive, * and ? wildcard matcher
	//Given: string i and string j. string j can have wildcards
	//Returns: 1:matches, 0:doesn't match
static int wildmatch (const char *i, const char *j)
{
	const char *k;
	char c0, c1;

	if (!*j) return(1);
	do
	{
		if (*j == '*')
		{
			for(k=i,j++;*k;k++) if (wildmatch(k,j)) return(1);
			continue;
		}
		if (!*i) return(0);
		if (*j == '?') { i++; j++; continue; }
		c0 = *i; if ((c0 >= 'a') && (c0 <= 'z')) c0 -= 32;
		c1 = *j; if ((c1 >= 'a') && (c1 <= 'z')) c1 -= 32;
		if (c0 == '/') c0 = '\\';
		if (c1 == '/') c1 = '\\';
		if (c0 != c1) return(0);
		i++; j++;
	} while (*j);
	return(!*i);
}

	//Same as: stricmp(st0,st1) except: '/' == '\'
static int filnamcmp (const char *st0, const char *st1)
{
	int i;
	char ch0, ch1;

	for(i=0;st0[i];i++)
	{
		ch0 = st0[i]; if ((ch0 >= 'a') && (ch0 <= 'z')) ch0 -= 32;
		ch1 = st1[i]; if ((ch1 >= 'a') && (ch1 <= 'z')) ch1 -= 32;
		if (ch0 == '/') ch0 = '\\';
		if (ch1 == '/') ch1 = '\\';
		if (ch0 != ch1) return(-1);
	}
	if (!st1[i]) return(0);
	return(-1);
}

//===================== ZIP decompression code begins ========================

	//format: (used by kzaddstack/kzopen to cache file name&start info)
	//[char zipnam[?]\0]
	//[next hashindex/-1][next index/-1][zipnam index][zipseek][char filnam[?]\0]
	//[next hashindex/-1][next index/-1][zipnam index][zipseek][char filnam[?]\0]
	//...
	//[char zipnam[?]\0]
	//[next hashindex/-1][next index/-1][zipnam index][zipseek][char filnam[?]\0]
	//[next hashindex/-1][next index/-1][zipnam index][zipseek][char filnam[?]\0]
	//...
#define KZHASHINITSIZE 8192
static char *kzhashbuf = 0;
static int kzhashead[256], kzhashpos, kzlastfnam, kzhashsiz;

static int kzcheckhashsiz (int siz)
{
	int i;

	if (!kzhashbuf) //Initialize hash table on first call
	{
		memset(kzhashead,-1,sizeof(kzhashead));
		kzhashbuf = (char *)malloc(KZHASHINITSIZE); if (!kzhashbuf) return(0);
		kzhashpos = 0; kzlastfnam = -1; kzhashsiz = KZHASHINITSIZE;
	}
	if (kzhashpos+siz > kzhashsiz) //Make sure string fits in kzhashbuf
	{
		i = kzhashsiz; do { i <<= 1; } while (kzhashpos+siz > i);
		kzhashbuf = (char *)realloc(kzhashbuf,i); if (!kzhashbuf) return(0);
		kzhashsiz = i;
	}
	return(1);
}

static int kzcalchash (const char *st)
{
	int i, hashind;
	char ch;

	for(i=0,hashind=0;st[i];i++)
	{
		ch = st[i];
		if ((ch >= 'a') && (ch <= 'z')) ch -= 32;
		if (ch == '/') ch = '\\';
		hashind = (ch - hashind*3);
	}
	return(hashind%(sizeof(kzhashead)/sizeof(kzhashead[0])));
}

static int kzcheckhash (const char *filnam, char **zipnam, unsigned int *zipseek)
{
	int i;

	if (!kzhashbuf) return(0);
	if (filnam[0] == '|') filnam++;
	for(i=kzhashead[kzcalchash(filnam)];i>=0;i=(*(int *)&kzhashbuf[i]))
		if (!filnamcmp(filnam,&kzhashbuf[i+16]))
		{
			(*zipnam) = &kzhashbuf[*(int *)&kzhashbuf[i+8]];
			(*zipseek) = *(unsigned int *)&kzhashbuf[i+12];
			return(1);
		}
	return(0);
}

void kzuninit (void)
{
	if (kzhashbuf) { free(kzhashbuf); kzhashbuf = 0; }
	kzhashpos = kzhashsiz = 0;
}

	//Load ZIP directory into memory (hash) to allow fast access later
int kzaddstack (const char *zipnam)
{
	FILE *fil;
	int i, j, hashind, zipnamoffs, numfiles;
	char tempbuf[260+46];

	fil = fopen(zipnam,"rb"); if (!fil) return(-1);

		//Write ZIP filename to hash
	i = (int)strlen(zipnam)+1; if (!kzcheckhashsiz(i)) { fclose(fil); return(-1); }
	strcpy(&kzhashbuf[kzhashpos],zipnam);
	zipnamoffs = kzhashpos; kzhashpos += i;

	fseek(fil,-22,SEEK_END);
	if (fread(tempbuf,22,1,fil) != 1) { fclose(fil); return(-1); }
	if (*(unsigned int *)&tempbuf[0] == LSWAPIB(0x06054b50)) //Fast way of finding dir info
	{
		numfiles = SSWAPIB(*(unsigned short *)&tempbuf[10]);
		fseek(fil,LSWAPIB(*(unsigned int *)&tempbuf[16]),SEEK_SET);
	}
	else //Slow way of finding dir info (used when ZIP has junk at end)
	{
		fseek(fil,0,SEEK_SET); numfiles = 0;
		while (1)
		{
			if (!fread(&j,4,1,fil)) { numfiles = -1; break; }
			if ((unsigned)j == LSWAPIB(0x02014b50)) break; //Found central file header :)
			if ((unsigned)j != LSWAPIB(0x04034b50)) { numfiles = -1; break; }
			if (fread(tempbuf,26,1,fil) != 1) { fclose(fil); return(-1); }
			fseek(fil,LSWAPIB(*(unsigned int *)&tempbuf[14]) + SSWAPIB(*(unsigned short *)&tempbuf[24]) + SSWAPIB(*(unsigned short *)&tempbuf[22]),SEEK_CUR);
			numfiles++;
		}
		if (numfiles < 0) { fclose(fil); return(-1); }
		fseek(fil,-4,SEEK_CUR);
	}
	for(i=0;i<numfiles;i++)
	{
		if (fread(tempbuf,46,1,fil) != 1) { fclose(fil); return(-1); }
		if (*(int *)&tempbuf[0] != LSWAPIB(0x02014b50)) { fclose(fil); return(0); }

		j = SSWAPIB(*(unsigned short *)&tempbuf[28]); //filename length
		if (fread(&tempbuf[46],j,1,fil) != 1) { fclose(fil); return(-1); }
		tempbuf[j+46] = 0;

			//Write information into hash
		j = (int)strlen(&tempbuf[46])+17; if (!kzcheckhashsiz(j)) { fclose(fil); return(-1); }
		hashind = kzcalchash(&tempbuf[46]);
		*(int *)&kzhashbuf[kzhashpos] = kzhashead[hashind];
		*(int *)&kzhashbuf[kzhashpos+4] = kzlastfnam;
		*(int *)&kzhashbuf[kzhashpos+8] = zipnamoffs;
		*(unsigned int *)&kzhashbuf[kzhashpos+12] = LSWAPIB(*(unsigned int *)&tempbuf[42]); //zipseek
		strcpy(&kzhashbuf[kzhashpos+16],&tempbuf[46]);
		kzhashead[hashind] = kzhashpos; kzlastfnam = kzhashpos; kzhashpos += j;

		j  = SSWAPIB(*(unsigned short *)&tempbuf[30]); //extra field length
		j += SSWAPIB(*(unsigned short *)&tempbuf[32]); //file comment length
		fseek(fil,j,SEEK_CUR);
	}
	fclose(fil);
	return(0);
}

int kzopen (const char *filnam)
{
	FILE *fil;
	unsigned int zipseek;
	char tempbuf[46+260], *zipnam;

	//kzfs.fil = 0;
	if (filnam[0] != '|')
	{
		kzfs.fil = fopen(filnam,"rb");
		if (kzfs.fil)
		{
			kzfs.comptyp = 0;
			kzfs.seek0 = 0;
			kzfs.leng = _filelength(_fileno(kzfs.fil));
			kzfs.pos = 0;
			kzfs.i = 0;
			return(1);
		}
	}
	if (kzcheckhash(filnam,&zipnam,&zipseek))
	{
		fil = fopen(zipnam,"rb"); if (!fil) return(0);
		fseek(fil,zipseek,SEEK_SET);
		if (fread(tempbuf,30,1,fil) != 1) { fclose(fil); return(0); }
		if (*(int *)&tempbuf[0] != LSWAPIB(0x04034b50)) { fclose(fil); return(0); }
		fseek(fil,SSWAPIB(*(unsigned short *)&tempbuf[26])+SSWAPIB(*(unsigned short *)&tempbuf[28]),SEEK_CUR);

		kzfs.fil = fil;
		kzfs.comptyp = SSWAPIB(*(short *)&tempbuf[8]);
		kzfs.seek0 = (unsigned int)ftell(fil);
		kzfs.leng = LSWAPIB(*(int *)&tempbuf[22]);
		kzfs.pos = 0;
		if (kzfs.leng < 0) { fclose(kzfs.fil); kzfs.fil = 0; return(0); }   // File is ≥ 2GiB.
		switch(kzfs.comptyp) //Compression method
		{
			case 0: kzfs.i = 0; return(1);
			case 8:
				if (!pnginited) { pnginited = 1; initpngtables(); }
				kzfs.comptell = 0;
				kzfs.compleng = LSWAPIB(*(int *)&tempbuf[18]);

					//WARNING: No file in ZIP can be > 2GB-32K bytes
				gslidew = 0x7fffffff; //Force reload at beginning

				return(1);
			default: fclose(kzfs.fil); kzfs.fil = 0; return(0);
		}
	}
	return(0);
}

// --------------------------------------------------------------------------

static int srchstat = -1, wildstpathleng;

#if defined(__DOS__)
static char wildst[260] = "";
static struct find_t findata;
#elif defined(_WIN32)
static char wildst[MAX_PATH] = "";
static HANDLE hfind = INVALID_HANDLE_VALUE;
static WIN32_FIND_DATA findata;
#else
static char wildst[260] = "";
static DIR *hfind = NULL;
static struct dirent *findata = NULL;
#endif

void kzfindfilestart (const char *st)
{
#if defined(__DOS__)
#elif defined(_WIN32)
	if (hfind != INVALID_HANDLE_VALUE)
		{ FindClose(hfind); hfind = INVALID_HANDLE_VALUE; }
#else
	if (hfind) { closedir(hfind); hfind = NULL; }
#endif
	strcpy(wildst,st);
	srchstat = -3;
}

int kzfindfile (char *filnam)
{
	int i;

	filnam[0] = 0;
	if (srchstat == -3)
	{
		if (!wildst[0]) { srchstat = -1; return(0); }
		do
		{
			srchstat = -2;

				//Extract directory from wildcard string for pre-pending
			wildstpathleng = 0;
			for(i=0;wildst[i];i++)
				if ((wildst[i] == '/') || (wildst[i] == '\\'))
					wildstpathleng = i+1;

			memcpy(filnam,wildst,wildstpathleng);

#if defined(__DOS__)
			if (_dos_findfirst(wildst,_A_SUBDIR,&findata))
				{ if (!kzhashbuf) return(0); srchstat = kzlastfnam; continue; }
			i = wildstpathleng;
			if (findata.attrib&16)
				if ((findata.name[0] == '.') && (!findata.name[1])) continue;
			strcpy(&filnam[i],findata.name);
			if (findata.attrib&16) strcat(&filnam[i],"\\");
#elif defined(_WIN32)
			hfind = FindFirstFile(wildst,&findata);
			if (hfind == INVALID_HANDLE_VALUE)
				{ if (!kzhashbuf) return(0); srchstat = kzlastfnam; continue; }
			if (findata.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) continue;
			i = wildstpathleng;
			if (findata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
				if ((findata.cFileName[0] == '.') && (!findata.cFileName[1])) continue;
			strcpy(&filnam[i],findata.cFileName);
			if (findata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) strcat(&filnam[i],"\\");
#else
			if (!hfind)
			{
				char *s = ".";
				if (wildstpathleng > 0) {
					filnam[wildstpathleng] = 0;
					s = filnam;
				}
				hfind = opendir(s);
				if (!hfind) { if (!kzhashbuf) return 0; srchstat = kzlastfnam; continue; }
			}
			break;   // process srchstat == -2
#endif
			return(1);
		} while (0);
	}
	if (srchstat == -2)
		while (1)
		{
			memcpy(filnam,wildst,wildstpathleng);
#if defined(__DOS__)
			if (_dos_findnext(&findata))
				{ if (!kzhashbuf) return(0); srchstat = kzlastfnam; break; }
			i = wildstpathleng;
			if (findata.attrib&16)
				if ((findata.name[0] == '.') && (!findata.name[1])) continue;
			strcpy(&filnam[i],findata.name);
			if (findata.attrib&16) strcat(&filnam[i],"\\");
#elif defined(_WIN32)
			if (!FindNextFile(hfind,&findata))
				{ FindClose(hfind); hfind = INVALID_HANDLE_VALUE; if (!kzhashbuf) return(0); srchstat = kzlastfnam; break; }
			if (findata.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) continue;
			i = wildstpathleng;
			if (findata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
				if ((findata.cFileName[0] == '.') && (!findata.cFileName[1])) continue;
			strcpy(&filnam[i],findata.cFileName);
			if (findata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) strcat(&filnam[i],"\\");
#else
			if ((findata = readdir(hfind)) == NULL)
				{ closedir(hfind); hfind = NULL; if (!kzhashbuf) return 0; srchstat = kzlastfnam; break; }
			i = wildstpathleng;
			if (findata->d_type == DT_DIR)
				{ if (findata->d_name[0] == '.' && !findata->d_name[1]) continue; } //skip .
			else if ((findata->d_type == DT_REG) || (findata->d_type == DT_LNK))
				{ if (findata->d_name[0] == '.') continue; } //skip hidden (dot) files
			else continue; //skip devices and fifos and such
			if (!wildmatch(findata->d_name,&wildst[wildstpathleng])) continue;
			strcpy(&filnam[i],findata->d_name);
			if (findata->d_type == DT_DIR) strcat(&filnam[i],"/");
#endif
			return(1);
		}
	while (srchstat >= 0)
	{
		if (wildmatch(&kzhashbuf[srchstat+16],wildst))
		{
			//strcpy(filnam,&kzhashbuf[srchstat+16]);
			filnam[0] = '|'; strcpy(&filnam[1],&kzhashbuf[srchstat+16]);
			srchstat = *(int *)&kzhashbuf[srchstat+4];
			return(1);
		}
		srchstat = *(int *)&kzhashbuf[srchstat+4];
	}
	return(0);
}

//File searching code (supports inside ZIP files!) How to use this code:
//   char filnam[MAX_PATH];
//   kzfindfilestart("vxl/*.vxl");
//   while (kzfindfile(filnam)) puts(filnam);
//NOTES:
// * Directory names end with '\' or '/' (depending on system)
// * Files inside zip begin with '|'

// --------------------------------------------------------------------------

static char *gzbufptr;
static void putbuf4zip (const unsigned char *buf, int uncomp0, int uncomp1)
{
	int i0, i1;
		//              uncomp0 ... uncomp1
		//  &gzbufptr[kzfs.pos] ... &gzbufptr[kzfs.endpos];
	i0 = max(uncomp0,kzfs.pos);
	i1 = min(uncomp1,kzfs.endpos);
	if (i0 < i1) memcpy(&gzbufptr[i0],&buf[i0-uncomp0],i1-i0);
}

	//returns number of bytes copied
int kzread (void *buffer, int leng)
{
	int i, j, k, bfinal, btype, hlit, hdist;

	if ((!kzfs.fil) || (leng <= 0)) return(0);

	if (kzfs.comptyp == 0)
	{
		if (kzfs.pos != kzfs.i) //Seek only when position changes
			fseek(kzfs.fil,kzfs.seek0+kzfs.pos,SEEK_SET);
		i = min(kzfs.leng-kzfs.pos,leng);
		i = (int)fread(buffer,1,i,kzfs.fil);
		kzfs.i += i; //kzfs.i is a local copy of ftell(kzfs.fil);
	}
	else if (kzfs.comptyp == 8)
	{
		zipfilmode = 1;

			//Initialize for putbuf4zip
		gzbufptr = (char *)buffer; gzbufptr = &gzbufptr[-kzfs.pos];
		kzfs.endpos = min(kzfs.pos+leng,kzfs.leng);
		if (kzfs.endpos == kzfs.pos) return(0); //Guard against reading 0 length

		if (kzfs.pos < gslidew-32768) // Must go back to start :(
		{
			if (kzfs.comptell) fseek(kzfs.fil,kzfs.seek0,SEEK_SET);

			gslidew = 0; gslider = 16384;
			kzfs.jmpplc = 0;

				//Initialize for suckbits/peekbits/getbits
			kzfs.comptell = min(kzfs.compleng,(int)sizeof(olinbuf));
			kzfs.comptell = (int)fread(&olinbuf[0],1,kzfs.comptell,kzfs.fil);
				//Make it re-load when there are < 32 bits left in FIFO
			bitpos = -(((int)sizeof(olinbuf)-4)<<3);
				//Identity: filptr + (bitpos>>3) = &olinbuf[0]
			filptr = &olinbuf[-(bitpos>>3)];
		}
		else
		{
			i = max(gslidew-32768,0); j = gslider-16384;

				//HACK: Don't unzip anything until you have to...
				//   (keeps file pointer as low as possible)
			if (kzfs.endpos <= gslidew) j = kzfs.endpos;

				//write uncompoffs on slidebuf from: i to j
			if (!((i^j)&32768))
				putbuf4zip(&slidebuf[i&32767],i,j);
			else
			{
				putbuf4zip(&slidebuf[i&32767],i,j&~32767);
				putbuf4zip(slidebuf,j&~32767,j);
			}

				//HACK: Don't unzip anything until you have to...
				//   (keeps file pointer as low as possible)
			if (kzfs.endpos <= gslidew) goto retkzread;
		}

		switch (kzfs.jmpplc)
		{
			case 0: goto kzreadplc0;
			case 1: goto kzreadplc1;
			case 2: goto kzreadplc2;
			case 3: goto kzreadplc3;
		}
kzreadplc0:;
		do
		{
			bfinal = getbits(1); btype = getbits(2);

#if 0
				//Display Huffman block offsets&lengths of input file - for debugging only!
			{
			static int ouncomppos = 0, ocomppos = 0;
			if (kzfs.comptell == sizeof(olinbuf)) i = 0;
			else if (kzfs.comptell < kzfs.compleng) i = kzfs.comptell-(sizeof(olinbuf)-4);
			else i = kzfs.comptell-(kzfs.comptell%(sizeof(olinbuf)-4));
			i += ((char *)&filptr[bitpos>>3])-((char *)(&olinbuf[0]));
			i = (i<<3)+(bitpos&7)-3;
			if (gslidew) printf(" ULng:0x%08x CLng:0x%08x.%x",gslidew-ouncomppos,(i-ocomppos)>>3,((i-ocomppos)&7)<<1);
			printf("\ntype:%d, Uoff:0x%08x Coff:0x%08x.%x",btype,gslidew,i>>3,(i&7)<<1);
			if (bfinal)
			{
				printf(" ULng:0x%08x CLng:0x%08x.%x",kzfs.leng-gslidew,((kzfs.compleng<<3)-i)>>3,(((kzfs.compleng<<3)-i)&7)<<1);
				printf("\n        Uoff:0x%08x Coff:0x%08x.0",kzfs.leng,kzfs.compleng);
				ouncomppos = ocomppos = 0;
			}
			else { ouncomppos = gslidew; ocomppos = i; }
			}
#endif

			if (btype == 0)
			{
				  //Raw (uncompressed)
				suckbits((-bitpos)&7);  //Synchronize to start of next byte
				i = getbits(16); if ((getbits(16)^i) != 0xffff) return(-1);
				for(;i;i--)
				{
					if (gslidew >= gslider)
					{
						putbuf4zip(&slidebuf[(gslider-16384)&32767],gslider-16384,gslider); gslider += 16384;
						if (gslider-16384 >= kzfs.endpos)
						{
							kzfs.jmpplc = 1; kzfs.i = i; kzfs.bfinal = bfinal;
							goto retkzread;
kzreadplc1:;         i = kzfs.i; bfinal = kzfs.bfinal;
						}
					}
					slidebuf[(gslidew++)&32767] = (char)getbits(8);
				}
				continue;
			}
			if (btype == 3) continue;

			if (btype == 1) //Fixed Huffman
			{
				hlit = 288; hdist = 32; i = 0;
				for(;i<144;i++) clen[i] = 8; //Fixed bit sizes (literals)
				for(;i<256;i++) clen[i] = 9; //Fixed bit sizes (literals)
				for(;i<280;i++) clen[i] = 7; //Fixed bit sizes (EOI,lengths)
				for(;i<288;i++) clen[i] = 8; //Fixed bit sizes (lengths)
				for(;i<320;i++) clen[i] = 5; //Fixed bit sizes (distances)
			}
			else  //Dynamic Huffman
			{
				hlit = getbits(5)+257; hdist = getbits(5)+1; j = getbits(4)+4;
				for(i=0;i<j;i++) cclen[ccind[i]] = getbits(3);
				for(;i<19;i++) cclen[ccind[i]] = 0;
				hufgencode(cclen,19,ibuf0,nbuf0);

				j = 0; k = hlit+hdist;
				while (j < k)
				{
					i = hufgetsym(ibuf0,nbuf0);
					if (i < 16) { clen[j++] = i; continue; }
					if (i == 16)
						{ for(i=getbits(2)+3;i;i--) { clen[j] = clen[j-1]; j++; } }
					else
					{
						if (i == 17) i = getbits(3)+3; else i = getbits(7)+11;
						for(;i;i--) clen[j++] = 0;
					}
				}
			}

			hufgencode(clen,hlit,ibuf0,nbuf0);
			qhufgencode(ibuf0,nbuf0,qhufval0,qhufbit0,LOGQHUFSIZ0);

			hufgencode(&clen[hlit],hdist,ibuf1,nbuf1);
			qhufgencode(ibuf1,nbuf1,qhufval1,qhufbit1,LOGQHUFSIZ1);

			while (1)
			{
				if (gslidew >= gslider)
				{
					putbuf4zip(&slidebuf[(gslider-16384)&32767],gslider-16384,gslider); gslider += 16384;
					if (gslider-16384 >= kzfs.endpos)
					{
						kzfs.jmpplc = 2; kzfs.bfinal = bfinal; goto retkzread;
kzreadplc2:;      bfinal = kzfs.bfinal;
					}
				}

				k = peekbits(LOGQHUFSIZ0);
				if (qhufbit0[k]) { i = qhufval0[k]; suckbits((int)qhufbit0[k]); }
				else i = hufgetsym(ibuf0,nbuf0);

				if (i < 256) { slidebuf[(gslidew++)&32767] = (char)i; continue; }
				if (i == 256) break;
				i = getbits(hxbit[i+30-257][0]) + hxbit[i+30-257][1];

				k = peekbits(LOGQHUFSIZ1);
				if (qhufbit1[k]) { j = qhufval1[k]; suckbits((int)qhufbit1[k]); }
				else j = hufgetsym(ibuf1,nbuf1);

				j = getbits(hxbit[j][0]) + hxbit[j][1];
				for(;i;i--,gslidew++) slidebuf[gslidew&32767] = slidebuf[(gslidew-j)&32767];
			}
		} while (!bfinal);

		gslider -= 16384;
		if (!((gslider^gslidew)&32768))
			putbuf4zip(&slidebuf[gslider&32767],gslider,gslidew);
		else
		{
			putbuf4zip(&slidebuf[gslider&32767],gslider,gslidew&~32767);
			putbuf4zip(slidebuf,gslidew&~32767,gslidew);
		}
kzreadplc3:; kzfs.jmpplc = 3;
	}

retkzread:;
	i = kzfs.pos;
	kzfs.pos += leng; if (kzfs.pos > kzfs.leng) kzfs.pos = kzfs.leng;
	return(kzfs.pos-i);
}

int kzfilelength (void)
{
	if (!kzfs.fil) return(-1);
	return(kzfs.leng);
}

	//WARNING: kzseek(<-32768,SEEK_CUR); or:
	//         kzseek(0,SEEK_END);       can make next kzread very slow!!!
int kzseek (int offset, int whence)
{
	if (!kzfs.fil) return(-1);
	switch (whence)
	{
		case SEEK_CUR: kzfs.pos += offset; break;
		case SEEK_END: kzfs.pos = kzfs.leng+offset; break;
		case SEEK_SET: default: kzfs.pos = offset;
	}
	if (kzfs.pos < 0) kzfs.pos = 0;
	if (kzfs.pos > kzfs.leng) kzfs.pos = kzfs.leng;
	return(kzfs.pos);
}

int kztell (void)
{
	if (!kzfs.fil) return(-1);
	return(kzfs.pos);
}

int kzgetc (void)
{
	char ch;
	if (!kzread(&ch,1)) return(-1);
	return((int)ch);
}

int kzeof (void)
{
	if (!kzfs.fil) return(-1);
	return(kzfs.pos >= kzfs.leng);
}

void kzclose (void)
{
	if (kzfs.fil) { fclose(kzfs.fil); kzfs.fil = 0; }
}

//====================== ZIP decompression code ends =========================
//===================== HANDY PICTURE function begins ========================

void kpzload (const char *filnam, intptr_t *pic, int *bpl, int *xsiz, int *ysiz)
{
	char *buf;
	int leng;

	(*pic) = 0;
	if (!kzopen(filnam)) return;
	leng = kzfilelength();
	buf = (char *)malloc(leng); if (!buf) return;
	kzread(buf,leng);
	kzclose();

	kpgetdim(buf,leng,xsiz,ysiz);
	(*bpl) = ((*xsiz)<<2);
	(*pic) = (intptr_t)malloc((*ysiz)*(*bpl)); if (!(*pic)) { free(buf); return; }
	if (kprender(buf,leng,*pic,*bpl,*xsiz,*ysiz,0,0) < 0) { free(buf); free((void *)*pic); (*pic) = 0; return; }
	free(buf);
}
//====================== HANDY PICTURE function ends =========================
