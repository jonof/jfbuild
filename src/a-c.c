// A.ASM replacement using C
// Mainly by Ken Silverman, with things melded with my port by
// Jonathon Fowler (jf@jonof.id.au)
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.

#include "compat.h"
#include "a.h"

#ifdef ENGINE_USING_A_C

int krecip(int num);	// from engine.c

#define BITSOFPRECISION 3
#define BITSOFPRECISIONPOW 8

extern int asm1, asm2, asm4, fpuasm, globalx3, globaly3;
extern intptr_t asm3;
extern void *reciptable;

static int bpl, transmode = 0;
static int glogx, glogy, gbxinc, gbyinc, gpinc;
static unsigned char *gbuf, *gpal, *ghlinepal, *gtrans;

	//Global variable functions
void setvlinebpl(int dabpl) { bpl = dabpl; }
void fixtransluscence(void *datransoff) { gtrans = (unsigned char *)datransoff; }
void settransnormal(void) { transmode = 0; }
void settransreverse(void) { transmode = 1; }


	//Ceiling/floor horizontal line functions
void sethlinesizes(int logx, int logy, void *bufplc)
	{ glogx = logx; glogy = logy; gbuf = (unsigned char *)bufplc; }
void setpalookupaddress(void *paladdr) { ghlinepal = (unsigned char *)paladdr; }
void setuphlineasm4(int bxinc, int byinc) { gbxinc = bxinc; gbyinc = byinc; }
void hlineasm4(int cnt, int skiploadincs, int paloffs, unsigned int by, unsigned int bx, void *p)
{
	unsigned char *palptr, *pp;

	palptr = &ghlinepal[paloffs];
	pp = (unsigned char *)p;
	if (!skiploadincs) { gbxinc = asm1; gbyinc = asm2; }
	for(;cnt>=0;cnt--)
	{
		*pp = palptr[gbuf[((bx>>(32-glogx))<<glogy)+(by>>(32-glogy))]];
		bx -= gbxinc;
		by -= gbyinc;
		pp--;
	}
}


	//Sloped ceiling/floor vertical line functions
void setupslopevlin(int logylogx, void *bufplc, int pinc)
{
	glogx = (logylogx&255); glogy = (logylogx>>8);
	gbuf = (unsigned char *)bufplc; gpinc = pinc;
}
void slopevlin(void *p, int i, void *slopaloffs, int cnt, int bx, int by)
{
	intptr_t *slopalptr;
	int bz, bzinc;
	unsigned int u, v;
	unsigned char *pp;

	bz = asm3; bzinc = (asm1>>3);
	slopalptr = (intptr_t *)slopaloffs;
	pp = (unsigned char *)p;
	for(;cnt>0;cnt--)
	{
		i = krecip(bz>>6); bz += bzinc;
		u = bx+globalx3*i;
		v = by+globaly3*i;
		*pp = *(unsigned char *)(slopalptr[0]+gbuf[((u>>(32-glogx))<<glogy)+(v>>(32-glogy))]);
		slopalptr--;
		pp += gpinc;
	}
}


	//Wall,face sprite/wall sprite vertical line functions
void setupvlineasm(int neglogy) { glogy = neglogy; }
void vlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p)
{
	unsigned char *pp;

	gbuf = (unsigned char *)bufplc;
	gpal = (unsigned char *)paloffs;
	pp = (unsigned char *)p;
	for(;cnt>=0;cnt--)
	{
		*pp = gpal[gbuf[vplc>>glogy]];
		pp += bpl;
		vplc += vinc;
	}
}

void setupmvlineasm(int neglogy) { glogy = neglogy; }
void mvlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p)
{
	unsigned char ch, *pp;

	gbuf = (unsigned char *)bufplc;
	gpal = (unsigned char *)paloffs;
	pp = (unsigned char *)p;
	for(;cnt>=0;cnt--)
	{
		ch = gbuf[vplc>>glogy]; if (ch != 255) *pp = gpal[ch];
		pp += bpl;
		vplc += vinc;
	}
}

void setuptvlineasm(int neglogy) { glogy = neglogy; }
void tvlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p)
{
	unsigned char ch, *pp;

	gbuf = (unsigned char *)bufplc;
	gpal = (unsigned char *)paloffs;
	pp = (unsigned char *)p;
	if (transmode)
	{
		for(;cnt>=0;cnt--)
		{
			ch = gbuf[vplc>>glogy];
			if (ch != 255) *pp = gtrans[(*pp)+(gpal[ch]<<8)];
			pp += bpl;
			vplc += vinc;
		}
	}
	else
	{
		for(;cnt>=0;cnt--)
		{
			ch = gbuf[vplc>>glogy];
			if (ch != 255) *pp = gtrans[((*pp)<<8)+gpal[ch]];
			pp += bpl;
			vplc += vinc;
		}
	}
}

	//Floor sprite horizontal line functions
void msethlineshift(int logx, int logy) { glogx = logx; glogy = logy; }
void mhline(void *bufplc, unsigned int bx, int cntup16, int UNUSED(junk), unsigned int by, void *p)
{
	unsigned char ch, *pp;

	gbuf = (unsigned char *)bufplc;
	gpal = (unsigned char *)asm3;
	pp = (unsigned char *)p;
	for(cntup16>>=16;cntup16>0;cntup16--)
	{
		ch = gbuf[((bx>>(32-glogx))<<glogy)+(by>>(32-glogy))];
		if (ch != 255) *pp = gpal[ch];
		bx += asm1;
		by += asm2;
		pp++;
	}
}

void tsethlineshift(int logx, int logy) { glogx = logx; glogy = logy; }
void thline(void *bufplc, unsigned int bx, int cntup16, int UNUSED(junk), unsigned int by, void *p)
{
	unsigned char ch, *pp;

	gbuf = (unsigned char *)bufplc;
	gpal = (unsigned char *)asm3;
	pp = (unsigned char *)p;
	if (transmode)
	{
		for(cntup16>>=16;cntup16>0;cntup16--)
		{
			ch = gbuf[((bx>>(32-glogx))<<glogy)+(by>>(32-glogy))];
			if (ch != 255) *pp = gtrans[(*pp)+(gpal[ch]<<8)];
			bx += asm1;
			by += asm2;
			pp++;
		}
	}
	else
	{
		for(cntup16>>=16;cntup16>0;cntup16--)
		{
			ch = gbuf[((bx>>(32-glogx))<<glogy)+(by>>(32-glogy))];
			if (ch != 255) *pp = gtrans[((*pp)<<8)+gpal[ch]];
			bx += asm1;
			by += asm2;
			pp++;
		}
	}
}


	//Rotatesprite vertical line functions
void setupspritevline(void *paloffs, int bxinc, int byinc, int ysiz)
{
	gpal = (unsigned char *)paloffs;
	gbxinc = bxinc;
	gbyinc = byinc;
	glogy = ysiz;
}
void spritevline(int bx, int by, int cnt, void *bufplc, void *p)
{
	unsigned char *pp;

	gbuf = (unsigned char *)bufplc;
	pp = (unsigned char *)p;
	for(;cnt>1;cnt--)
	{
		*pp = gpal[gbuf[(bx>>16)*glogy+(by>>16)]];
		bx += gbxinc;
		by += gbyinc;
		pp += bpl;
	}
}

	//Rotatesprite vertical line functions
void msetupspritevline(void *paloffs, int bxinc, int byinc, int ysiz)
{
	gpal = (unsigned char *)paloffs;
	gbxinc = bxinc;
	gbyinc = byinc;
	glogy = ysiz;
}
void mspritevline(int bx, int by, int cnt, void *bufplc, void *p)
{
	unsigned char ch, *pp;

	gbuf = (unsigned char *)bufplc;
	pp = (unsigned char *)p;
	for(;cnt>1;cnt--)
	{
		ch = gbuf[(bx>>16)*glogy+(by>>16)];
		if (ch != 255) *pp = gpal[ch];
		bx += gbxinc;
		by += gbyinc;
		pp += bpl;
	}
}

void tsetupspritevline(void *paloffs, int bxinc, int byinc, int ysiz)
{
	gpal = (unsigned char *)paloffs;
	gbxinc = bxinc;
	gbyinc = byinc;
	glogy = ysiz;
}
void tspritevline(int bx, int by, int cnt, void *bufplc, void *p)
{
	unsigned char ch, *pp;

	gbuf = (unsigned char *)bufplc;
	pp = (unsigned char *)p;
	if (transmode)
	{
		for(;cnt>1;cnt--)
		{
			ch = gbuf[(bx>>16)*glogy+(by>>16)];
			if (ch != 255) *pp = gtrans[(*pp)+(gpal[ch]<<8)];
			bx += gbxinc;
			by += gbyinc;
			pp += bpl;
		}
	}
	else
	{
		for(;cnt>1;cnt--)
		{
			ch = gbuf[(bx>>16)*glogy+(by>>16)];
			if (ch != 255) *pp = gtrans[((*pp)<<8)+gpal[ch]];
			bx += gbxinc;
			by += gbyinc;
			pp += bpl;
		}
	}
}

void setupdrawslab (int dabpl, void *pal)
	{ bpl = dabpl; gpal = (unsigned char *)pal; }

void drawslab (int dx, int v, int dy, int vi, void *vptr, void *p)
{
	int x;
	unsigned char *pp, *vpptr;
	
	pp = (unsigned char *)p;
	vpptr = (unsigned char *)vptr;
	while (dy > 0)
	{
		for(x=0;x<dx;x++) *(pp+x) = gpal[(int)(*(vpptr+(v>>16)))];
		pp += bpl; v += vi; dy--;
	}
}

void stretchhline (void * UNUSED(p0), int u, int cnt, int uinc, void *rptr, void *p)
{
	unsigned char *pp, *rpptr, *np;

	rpptr = (unsigned char *)rptr;
	pp = (unsigned char *)p;
	np = (unsigned char *)((intptr_t)p-(cnt<<2));
	do
	{
		pp--;
		*pp = *(rpptr+(u>>16)); u -= uinc;
	} while (pp > np);
}


void mmxoverlay() { }

#endif
/*
 * vim:ts=4:
 */

