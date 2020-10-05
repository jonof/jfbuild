#ifndef ENGINE_PRIV_H
#define ENGINE_PRIV_H

#define MAXCLIPNUM 1024
#define MAXPERMS 1024
#define MAXTILEFILES 256
#define MAXYSAVES ((MAXXDIM*MAXSPRITES)>>7)
#define MAXNODESPERLINE 42   //Warning: This depends on MAXYSAVES & MAXYDIM!
#define MAXWALLSB 2048
#define MAXCLIPDIST 1024

extern unsigned char pow2char[8];
extern int pow2long[32];

extern short thesector[MAXWALLSB], thewall[MAXWALLSB];
extern short bunchfirst[MAXWALLSB], bunchlast[MAXWALLSB];
extern short maskwall[MAXWALLSB], maskwallcnt;
extern spritetype *tspriteptr[MAXSPRITESONSCREEN];
extern int xdimen, xdimenrecip, halfxdimen, xdimenscale, xdimscale, ydimen, ydimenscale;
extern intptr_t frameoffset;
extern int globalposx, globalposy, globalposz, globalhoriz;
extern short globalang, globalcursectnum;
extern int globalpal, cosglobalang, singlobalang;
extern int cosviewingrangeglobalang, sinviewingrangeglobalang;
extern int globalvisibility;
extern int asm1, asm2, asm4;
extern intptr_t asm3;
extern int globalshade;
extern short globalpicnum;
extern int globalx1, globaly2;
extern int globalorientation;

extern short searchit;
extern int searchx, searchy;
extern short searchsector, searchwall, searchstat;

extern char inpreparemirror;

extern int curbrightness, gammabrightness;
extern float curgamma;
extern unsigned char britable[16][256];
extern unsigned char picsiz[MAXTILES];
extern int lastx[MAXYDIM];
extern unsigned char *transluc;
extern short sectorborder[256], sectorbordercnt;
extern int qsetmode;
extern int hitallsprites;

extern int xb1[MAXWALLSB];
extern int rx1[MAXWALLSB], ry1[MAXWALLSB];
extern short p2[MAXWALLSB];
extern short numscans, numhits, numbunches;

#if USE_OPENGL
extern palette_t palookupfog[MAXPALOOKUPS];
#endif

int wallmost(short *mostbuf, int w, int sectnum, unsigned char dastat);
int wallfront(int l1, int l2);
int animateoffs(short tilenum, short fakevar);


#if defined(__WATCOMC__) && USE_ASM

#pragma aux setgotpic =\
"mov ebx, eax",\
"cmp byte ptr walock[eax], 200",\
"jae skipit",\
"mov byte ptr walock[eax], 199",\
"skipit: shr eax, 3",\
"and ebx, 7",\
"mov dl, byte ptr gotpic[eax]",\
"mov bl, byte ptr pow2char[ebx]",\
"or dl, bl",\
"mov byte ptr gotpic[eax], dl",\
parm [eax]\
modify exact [eax ebx ecx edx]
void setgotpic(int);

#elif defined(_MSC_VER) && defined(_M_IX86) && USE_ASM	// __WATCOMC__

static inline void setgotpic(int a)
{
	_asm {
		push ebx
		mov eax, a
		mov ebx, eax
		cmp byte ptr walock[eax], 200
		jae skipit
		mov byte ptr walock[eax], 199
skipit:
		shr eax, 3
		and ebx, 7
		mov dl, byte ptr gotpic[eax]
		mov bl, byte ptr pow2char[ebx]
		or dl, bl
		mov byte ptr gotpic[eax], dl
		pop ebx
	}
}

#elif defined(__GNUC__) && defined(__i386__) && USE_ASM	// _MSC_VER

#define setgotpic(a) \
({ int __a=(a); \
	__asm__ __volatile__ ( \
			       "movl %%eax, %%ebx\n\t" \
			       "cmpb $200, %[walock](%%eax)\n\t" \
			       "jae 0f\n\t" \
			       "movb $199, %[walock](%%eax)\n\t" \
			       "0:\n\t" \
			       "shrl $3, %%eax\n\t" \
			       "andl $7, %%ebx\n\t" \
			       "movb %[gotpic](%%eax), %%dl\n\t" \
			       "movb %[pow2char](%%ebx), %%bl\n\t" \
			       "orb %%bl, %%dl\n\t" \
			       "movb %%dl, %[gotpic](%%eax)" \
			       : "=a" (__a) \
			       : "a" (__a), [walock] "m" (walock[0]), \
			         [gotpic] "m" (gotpic[0]), \
			         [pow2char] "m" (pow2char[0]) \
			       : "ebx", "edx", "memory", "cc"); \
				       __a; })

#else	// __GNUC__ && __i386__

static inline void setgotpic(int tilenume)
{
	if (walock[tilenume] < 200) walock[tilenume] = 199;
	gotpic[tilenume>>3] |= pow2char[tilenume&7];
}

#endif

#endif	/* ENGINE_PRIV_H */
