// Assembly-language function wrappers for a.asm functions
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)


#ifndef __a_h__
#define __a_h__

#if defined(__WATCOMC__) && defined(_M_I386) && USE_ASM

extern long mmxoverlay();
#pragma aux mmxoverlay modify [eax ebx ecx edx];
extern long sethlinesizes(long,long,void *);
#pragma aux sethlinesizes parm [eax][ebx][ecx];
extern long setpalookupaddress(void *);
#pragma aux setpalookupaddress parm [eax];
extern long setuphlineasm4(long,long);
#pragma aux setuphlineasm4 parm [eax][ebx];
extern long hlineasm4(long,long,long,long,long,void *);
#pragma aux hlineasm4 parm [eax][ebx][ecx][edx][esi][edi];
extern long setuprhlineasm4(long,long,long,void *,long,long);
#pragma aux setuprhlineasm4 parm [eax][ebx][ecx][edx][esi][edi];
extern long rhlineasm4(long,void *,long,long,long,void *);
#pragma aux rhlineasm4 parm [eax][ebx][ecx][edx][esi][edi];
extern long setuprmhlineasm4(long,long,long,void *,long,long);
#pragma aux setuprmhlineasm4 parm [eax][ebx][ecx][edx][esi][edi];
extern long rmhlineasm4(long,void *,long,long,long,void *);
#pragma aux rmhlineasm4 parm [eax][ebx][ecx][edx][esi][edi];
extern long setupqrhlineasm4(long,long,long,void *,long,long);
#pragma aux setupqrhlineasm4 parm [eax][ebx][ecx][edx][esi][edi];
extern long qrhlineasm4(long,void *,long,long,long,void *);
#pragma aux qrhlineasm4 parm [eax][ebx][ecx][edx][esi][edi];
extern long setvlinebpl(long);
#pragma aux setvlinebpl parm [eax];
extern long fixtransluscence(void *);
#pragma aux fixtransluscence parm [eax];
extern long prevlineasm1(long,void *,long,long,void *,void *);
#pragma aux prevlineasm1 parm [eax][ebx][ecx][edx][esi][edi];
extern long vlineasm1(long,void *,long,long,void *,void *);
#pragma aux vlineasm1 parm [eax][ebx][ecx][edx][esi][edi];
extern long setuptvlineasm(long);
#pragma aux setuptvlineasm parm [eax];
extern long tvlineasm1(long,void *,long,long,void *,void *);
#pragma aux tvlineasm1 parm [eax][ebx][ecx][edx][esi][edi];
extern long setuptvlineasm2(long,void *,void *);
#pragma aux setuptvlineasm2 parm [eax][ebx][ecx];
extern long tvlineasm2(long,long,void *,void *,long,void *);
#pragma aux tvlineasm2 parm [eax][ebx][ecx][edx][esi][edi];
extern long mvlineasm1(long,void *,long,long,void *,void *);
#pragma aux mvlineasm1 parm [eax][ebx][ecx][edx][esi][edi];
extern long setupvlineasm(long);
#pragma aux setupvlineasm parm [eax];
extern long vlineasm4(long,void *);
#pragma aux vlineasm4 parm [ecx][edi] modify [eax ebx ecx edx esi edi];
extern long setupmvlineasm(long);
#pragma aux setupmvlineasm parm [eax];
extern long mvlineasm4(long,void *);
#pragma aux mvlineasm4 parm [ecx][edi] modify [eax ebx ecx edx esi edi];
extern void setupspritevline(void *,long,long,long,long,long);
#pragma aux setupspritevline parm [eax][ebx][ecx][edx][esi][edi];
extern void spritevline(long,long,long,long,void *,void *);
#pragma aux spritevline parm [eax][ebx][ecx][edx][esi][edi];
extern void msetupspritevline(void *,long,long,long,long,long);
#pragma aux msetupspritevline parm [eax][ebx][ecx][edx][esi][edi];
extern void mspritevline(long,long,long,long,void *,void *);
#pragma aux mspritevline parm [eax][ebx][ecx][edx][esi][edi];
extern void tsetupspritevline(void *,long,long,long,long,long);
#pragma aux tsetupspritevline parm [eax][ebx][ecx][edx][esi][edi];
extern void tspritevline(long,long,long,long,void *,void *);
#pragma aux tspritevline parm [eax][ebx][ecx][edx][esi][edi];
extern long mhline(void *,long,long,long,long,void *);
#pragma aux mhline parm [eax][ebx][ecx][edx][esi][edi];
extern long mhlineskipmodify(void *,long,long,long,long,void *);
#pragma aux mhlineskipmodify parm [eax][ebx][ecx][edx][esi][edi];
extern long msethlineshift(long,long);
#pragma aux msethlineshift parm [eax][ebx];
extern long thline(void *,long,long,long,long,void *);
#pragma aux thline parm [eax][ebx][ecx][edx][esi][edi];
extern long thlineskipmodify(void *,long,long,long,long,void *);
#pragma aux thlineskipmodify parm [eax][ebx][ecx][edx][esi][edi];
extern long tsethlineshift(long,long);
#pragma aux tsethlineshift parm [eax][ebx];
extern long setupslopevlin(long,void *,long);
#pragma aux setupslopevlin parm [eax][ebx][ecx] modify [edx];
extern long slopevlin(void *,long,void *,long,long,long);
#pragma aux slopevlin parm [eax][ebx][ecx][edx][esi][edi];
extern long settransnormal();
#pragma aux settransnormal parm;
extern long settransreverse();
#pragma aux settransreverse parm;
extern long setupdrawslab(long,void *);
#pragma aux setupdrawslab parm [eax][ebx];
extern long drawslab(long,long,long,long,void *,void *);
#pragma aux drawslab parm [eax][ebx][ecx][edx][esi][edi];

#elif ((defined(__GNUC__) && defined(__i386__)) || (defined(_MSC_VER) && defined(_M_IX86))) && USE_ASM	// __WATCOMC__

#if !defined(_WIN32)
#define __cdecl
#endif

void __cdecl mmxoverlay(void);
void __cdecl sethlinesizes(int,int,void *);
void __cdecl setpalookupaddress(void *);
void __cdecl setuphlineasm4(int,int);
void __cdecl hlineasm4(int,int,int,int,int,void *);
void __cdecl setuprhlineasm4(int,int,int,void *,int,int);
void __cdecl rhlineasm4(int,void *,int,int,int,void *);
void __cdecl setuprmhlineasm4(int,int,int,void *,int,int);
void __cdecl rmhlineasm4(int,void *,int,int,int,void *);
void __cdecl setupqrhlineasm4(int,int,int,void *,int,int);
void __cdecl qrhlineasm4(int,void *,int,int,int,void *);
void __cdecl setvlinebpl(int);
void __cdecl fixtransluscence(void *);
int __cdecl prevlineasm1(int,void *,int,int,void *,void *);
void __cdecl vlineasm1(int,void *,int,int,void *,void *);
void __cdecl setuptvlineasm(int);
int __cdecl tvlineasm1(int,void *,int,int,void *,void *);
void __cdecl setuptvlineasm2(int,void *,void *);
void __cdecl tvlineasm2(int,int,void *,void *,int,void *);
int __cdecl mvlineasm1(int,void *,int,int,void *,void *);
void __cdecl setupvlineasm(int);
void __cdecl vlineasm4(int,void *);
void __cdecl setupmvlineasm(int);
void __cdecl mvlineasm4(int,void *);
void __cdecl setupspritevline(void *,int,int,int,int,int);
void __cdecl spritevline(int,int,int,int,void *,void *);
void __cdecl msetupspritevline(void *,int,int,int,int,int);
void __cdecl mspritevline(int,int,int,int,void *,void *);
void __cdecl tsetupspritevline(void *,int,int,int,int,int);
void __cdecl tspritevline(int,int,int,int,void *,void *);
void __cdecl mhline(void *,int,int,int,int,void *);
void __cdecl mhlineskipmodify(void *,int,int,int,int,void *);
void __cdecl msethlineshift(int,int);
void __cdecl thline(void *,int,int,int,int,void *);
void __cdecl thlineskipmodify(void *,int,int,int,int,void *);
void __cdecl tsethlineshift(int,int);
void __cdecl setupslopevlin(int,void *,int);
void __cdecl slopevlin(void *,int,void *,int,int,int);
void __cdecl settransnormal(void);
void __cdecl settransreverse(void);
void __cdecl setupdrawslab(int,void *);
void __cdecl drawslab(int,int,int,int,void *,void *);
void __cdecl stretchhline(void *,int,int,int,void *,void *);

#else				// GCC || MSVC

#define ENGINE_USING_A_C

void setvlinebpl(int dabpl);
void fixtransluscence(void *datransoff);
void settransnormal(void);
void settransreverse(void);

void sethlinesizes(int logx, int logy, void *bufplc);
void setpalookupaddress(void *paladdr);
void setuphlineasm4(int bxinc, int byinc);
void hlineasm4(int cnt, int skiploadincs, int paloffs, unsigned int by, unsigned int bx, void *p);

void setupslopevlin(int logylogx, void *bufplc, int pinc);
void slopevlin(void *p, int i, void *slopaloffs, int cnt, int bx, int by);

void setupvlineasm(int neglogy);
void vlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p);

void setupmvlineasm(int neglogy);
void mvlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p);

void setuptvlineasm(int neglogy);
void tvlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p);

void msethlineshift(int logx, int logy);
void mhline(void *bufplc, unsigned int bx, int cntup16, int junk, unsigned int by, void *p);

void tsethlineshift(int logx, int logy);
void thline(void *bufplc, unsigned int bx, int cntup16, int junk, unsigned int by, void *p);

void setupspritevline(void *paloffs, int bxinc, int byinc, int ysiz);
void spritevline(int bx, int by, int cnt, void *bufplc, void *p);

void msetupspritevline(void *paloffs, int bxinc, int byinc, int ysiz);
void mspritevline(int bx, int by, int cnt, void *bufplc, void *p);

void tsetupspritevline(void *paloffs, int bxinc, int byinc, int ysiz);
void tspritevline(int bx, int by, int cnt, void *bufplc, void *p);

void setupdrawslab (int dabpl, void *pal);
void drawslab (int dx, int v, int dy, int vi, void *vptr, void *p);
void stretchhline (void *p0, int u, int cnt, int uinc, void *rptr, void *p);

void mmxoverlay(void);

#endif	// else

#endif // __a_h__
