// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.

#ifndef __editor_h__
#define __editor_h__

#ifdef __cplusplus
extern "C" {
#endif

#define NUMBUILDKEYS 20

extern int qsetmode;
extern short searchsector, searchwall, searchstat;
extern int zmode, kensplayerheight;
extern short defaultspritecstat;
extern int posx, posy, posz, horiz;
extern short ang, cursectnum;
extern short ceilingheinum, floorheinum;
extern int zlock;
extern short editstatus, searchit;
extern int searchx, searchy;                          //search input

extern short temppicnum, tempcstat, templotag, temphitag, tempextra;
extern unsigned char tempshade, temppal, tempxrepeat, tempyrepeat;
extern unsigned char somethingintab;
extern char names[MAXTILES][25];

extern int buildkeys[NUMBUILDKEYS];

extern int ydim16, halfxdim16, midydim16, xdimgame, ydimgame, bppgame, xdim2d, ydim2d, forcesetup;

struct startwin_settings {
    int fullscreen;
    int xdim2d, ydim2d;
    int xdim3d, ydim3d, bpp3d;
    int forcesetup;
};

extern int ExtInit(void);
extern void ExtUnInit(void);
extern void ExtPreCheckKeys(void);
extern void ExtAnalyzeSprites(void);
extern void ExtCheckKeys(void);
extern void ExtPreLoadMap(void);
extern void ExtLoadMap(const char *mapname);
extern void ExtPreSaveMap(void);
extern void ExtSaveMap(const char *mapname);
extern const char *ExtGetSectorCaption(short sectnum);
extern const char *ExtGetWallCaption(short wallnum);
extern const char *ExtGetSpriteCaption(short spritenum);
extern void ExtShowSectorData(short sectnum);
extern void ExtShowWallData(short wallnum);
extern void ExtShowSpriteData(short spritenum);
extern void ExtEditSectorData(short sectnum);
extern void ExtEditWallData(short wallnum);
extern void ExtEditSpriteData(short spritenum);


int loadsetup(const char *fn);	// from config.c
int writesetup(const char *fn);	// from config.c

void editinput(void);
void clearmidstatbar16(void);

short getnumber256(char *namestart, short num, int maxnumber, char sign);
short getnumber16(char *namestart, short num, int maxnumber, char sign);
void printmessage256(char *name);
void printmessage16(char *name);

void getpoint(int searchxe, int searchye, int *x, int *y);
int getpointhighlight(int xplc, int yplc);

#ifdef __cplusplus
}
#endif

#endif
