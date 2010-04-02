#ifndef ENGINE_PRIV_H
#define ENGINE_PRIV_H

#define MAXCLIPNUM 1024
#define MAXPERMS 512
#define MAXTILEFILES 256
#define MAXYSAVES ((MAXXDIM*MAXSPRITES)>>7)
#define MAXNODESPERLINE 42   //Warning: This depends on MAXYSAVES & MAXYDIM!
#define MAXWALLSB 2048
#define MAXCLIPDIST 1024

extern char pow2char[8];
extern long pow2long[32];

extern short thesector[MAXWALLSB], thewall[MAXWALLSB];
extern short bunchfirst[MAXWALLSB], bunchlast[MAXWALLSB];
extern short maskwall[MAXWALLSB], maskwallcnt;
extern spritetype *tspriteptr[MAXSPRITESONSCREEN];
extern long xdimen, xdimenrecip, halfxdimen, xdimenscale, xdimscale, ydimen;
extern long frameoffset;
extern long globalposx, globalposy, globalposz, globalhoriz;
extern short globalang, globalcursectnum;
extern long globalpal, cosglobalang, singlobalang;
extern long cosviewingrangeglobalang, sinviewingrangeglobalang;
extern long globalvisibility;
extern long xyaspect;
extern long pixelaspect;
extern long asm1, asm2, asm3, asm4;
extern long globalshade;
extern short globalpicnum;
extern long globalx1, globaly2;
extern long globalorientation;

extern short searchit;
extern long searchx, searchy;
extern short searchsector, searchwall, searchstat;

extern char inpreparemirror;

extern long curbrightness, gammabrightness;
extern unsigned char britable[16][256];
extern char picsiz[MAXTILES];
extern long lastx[MAXYDIM];
extern char *transluc;
extern short sectorborder[256], sectorbordercnt;
extern long qsetmode;
extern long hitallsprites;

extern long xb1[MAXWALLSB];
extern long rx1[MAXWALLSB], ry1[MAXWALLSB];
extern short p2[MAXWALLSB];
extern short numscans, numhits, numbunches;

#ifdef USE_OPENGL
extern palette_t palookupfog[MAXPALOOKUPS];
#endif

long wallmost(short *mostbuf, long w, long sectnum, char dastat);
long wallfront(long l1, long l2);
long animateoffs(short tilenum, short fakevar);


#if defined(__WATCOMC__) && !defined(NOASM)

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
void setgotpic(long);

#elif defined(_MSC_VER) && !defined(NOASM)	// __WATCOMC__

static inline void setgotpic(long a)
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

#elif defined(__GNUC__) && defined(__i386__) && !defined(NOASM)	// _MSC_VER

#define setgotpic(a) \
({ long __a=(a); \
	__asm__ __volatile__ ( \
			       "movl %%eax, %%ebx\n\t" \
			       "cmpb $200, "ASMSYM("walock")"(%%eax)\n\t" \
			       "jae 0f\n\t" \
			       "movb $199, "ASMSYM("walock")"(%%eax)\n\t" \
			       "0:\n\t" \
			       "shrl $3, %%eax\n\t" \
			       "andl $7, %%ebx\n\t" \
			       "movb "ASMSYM("gotpic")"(%%eax), %%dl\n\t" \
			       "movb "ASMSYM("pow2char")"(%%ebx), %%bl\n\t" \
			       "orb %%bl, %%dl\n\t" \
			       "movb %%dl, "ASMSYM("gotpic")"(%%eax)" \
			       : "=a" (__a) : "a" (__a) \
			       : "ebx", "edx", "memory", "cc"); \
				       __a; })

#else	// __GNUC__ && __i386__

static inline void setgotpic(long tilenume)
{
	if (walock[tilenume] < 200) walock[tilenume] = 199;
	gotpic[tilenume>>3] |= pow2char[tilenume&7];
}

#endif

#endif	/* ENGINE_PRIV_H */
