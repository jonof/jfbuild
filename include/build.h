// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)


#ifndef __build_h__
#define __build_h__

#ifndef USE_POLYMOST
#  define USE_POLYMOST 0
#endif
#ifndef USE_OPENGL
#  define USE_OPENGL 0
#endif
#define USE_GL2 2
#define USE_GL3 3
#define USE_GLES2 12

#include "compat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAXSECTORSV8 4096
#define MAXWALLSV8 16384
#define MAXSPRITESV8 16384

#define MAXSECTORSV7 1024
#define MAXWALLSV7 8192
#define MAXSPRITESV7 4096

#define MAXSECTORSV6 1024
#define MAXWALLSV6   8192
#define MAXSPRITESV6 4096
#define MAXTILESV6   4096

#define MAXSECTORSV5 1024
#define MAXWALLSV5   4096
#define MAXSPRITESV5 4096
#define MAXTILESV5   4096

#define MAXSECTORS MAXSECTORSV8
#define MAXWALLS MAXWALLSV8
#define MAXSPRITES MAXSPRITESV8

#define MAXTILES 9216
#define MAXVOXELS 4096
#define MAXSTATUS 1024
#define MAXPLAYERS 16
#define MAXXDIM 3840
#define MAXYDIM 2160
#define MAXPALOOKUPS 256
#define MAXPSKYTILES 256
#define MAXSPRITESONSCREEN 2048
#define MAXUNIQHUDID 256 //Extra slots so HUD models can store animation state without messing game sprites

#define CLIPMASK0 (((1L)<<16)+1L)
#define CLIPMASK1 (((256L)<<16)+64L)

	//Make all variables in BUILD.H defined in the ENGINE,
	//and externed in GAME
#ifdef ENGINE
#  define EXTERN
#else
#  define EXTERN extern
#endif

#ifdef __GNUC__
#  if __GNUC__ == 4 && __GNUC_MINOR__ >= 7
#    define BPACK __attribute__ ((packed, gcc_struct))
#  else
#    define BPACK __attribute__ ((packed))
#  endif
#else
#define BPACK
#endif

#ifdef _MSC_VER
#pragma pack(1)
#endif

#ifdef __WATCOMC__
#pragma pack(push,1);
#endif


//ceilingstat/floorstat:
//   bit 0: 1 = parallaxing, 0 = not                                 "P"
//   bit 1: 1 = groudraw, 0 = not
//   bit 2: 1 = swap x&y, 0 = not                                    "F"
//   bit 3: 1 = double smooshiness                                   "E"
//   bit 4: 1 = x-flip                                               "F"
//   bit 5: 1 = y-flip                                               "F"
//   bit 6: 1 = Align texture to first wall of sector                "R"
//   bits 7-8:                                                       "T"
//          00 = normal floors
//          01 = masked floors
//          10 = transluscent masked floors
//          11 = reverse transluscent masked floors
//   bits 9-15: reserved

	//40 bytes
typedef struct
{
	short wallptr, wallnum;
	int ceilingz, floorz;
	short ceilingstat, floorstat;
	short ceilingpicnum, ceilingheinum;
	signed char ceilingshade;
	unsigned char ceilingpal, ceilingxpanning, ceilingypanning;
	short floorpicnum, floorheinum;
	signed char floorshade;
	unsigned char floorpal, floorxpanning, floorypanning;
	unsigned char visibility, filler;
	short lotag, hitag, extra;
} sectortype;

//cstat:
//   bit 0: 1 = Blocking wall (use with clipmove, getzrange)         "B"
//   bit 1: 1 = bottoms of invisible walls swapped, 0 = not          "2"
//   bit 2: 1 = align picture on bottom (for doors), 0 = top         "O"
//   bit 3: 1 = x-flipped, 0 = normal                                "F"
//   bit 4: 1 = masking wall, 0 = not                                "M"
//   bit 5: 1 = 1-way wall, 0 = not                                  "1"
//   bit 6: 1 = Blocking wall (use with hitscan / cliptype 1)        "H"
//   bit 7: 1 = Transluscence, 0 = not                               "T"
//   bit 8: 1 = y-flipped, 0 = normal                                "F"
//   bit 9: 1 = Transluscence reversing, 0 = normal                  "T"
//   bits 10-15: reserved

	//32 bytes
typedef struct
{
	int x, y;
	short point2, nextwall, nextsector, cstat;
	short picnum, overpicnum;
	signed char shade;
	unsigned char pal, xrepeat, yrepeat, xpanning, ypanning;
	short lotag, hitag, extra;
} walltype;

//cstat:
//   bit 0: 1 = Blocking sprite (use with clipmove, getzrange)       "B"
//   bit 1: 1 = transluscence, 0 = normal                            "T"
//   bit 2: 1 = x-flipped, 0 = normal                                "F"
//   bit 3: 1 = y-flipped, 0 = normal                                "F"
//   bits 5-4: 00 = FACE sprite (default)                            "R"
//             01 = WALL sprite (like masked walls)
//             10 = FLOOR sprite (parallel to ceilings&floors)
//   bit 6: 1 = 1-sided sprite, 0 = normal                           "1"
//   bit 7: 1 = Real centered centering, 0 = foot center             "C"
//   bit 8: 1 = Blocking sprite (use with hitscan / cliptype 1)      "H"
//   bit 9: 1 = Transluscence reversing, 0 = normal                  "T"
//   bits 10-14: reserved
//   bit 15: 1 = Invisible sprite, 0 = not invisible

	//44 bytes
typedef struct
{
	int x, y, z;
	short cstat, picnum;
	signed char shade;
	unsigned char pal, clipdist, filler;
	unsigned char xrepeat, yrepeat;
	signed char xoffset, yoffset;
	short sectnum, statnum;
	short ang, owner, xvel, yvel, zvel;
	short lotag, hitag, extra;
} spritetype;

	// 12 bytes
typedef struct {
	unsigned int mdanimtims;
	short mdanimcur;
	short angoff;
	unsigned char flags;
	char filler[3];
} spriteexttype;
#define SPREXT_NOTMD 1
#define SPREXT_NOMDANIM 2

EXTERN sectortype sector[MAXSECTORS];
EXTERN walltype wall[MAXWALLS];
EXTERN spritetype sprite[MAXSPRITES];
EXTERN spriteexttype spriteext[MAXSPRITES+MAXUNIQHUDID];
EXTERN int guniqhudid;

EXTERN int spritesortcnt;
EXTERN spritetype tsprite[MAXSPRITESONSCREEN];

//numpages==127 means no persistence. Permanent rotatesprites will be retained until flushed.
//The initial frame contents will be invalid after each swap.
EXTERN int xdim, ydim, ylookup[MAXYDIM+1], numpages;
EXTERN int yxaspect, xyaspect, pixelaspect, widescreen, tallscreen, viewingrange;

#define MAXVALIDMODES 256
EXTERN int validmodecnt;
struct validmode_t {
	int xdim,ydim;
	unsigned char bpp;
	unsigned char fs;
	unsigned char display; // 0 for windowed modes or the primary display.
	unsigned char filler;
	int extra;			// *layer-defined use
};
EXTERN struct validmode_t validmode[MAXVALIDMODES];

EXTERN short numsectors, numwalls;
EXTERN /*volatile*/ int totalclock;
EXTERN int numframes, randomseed;
EXTERN short sintable[2048];
EXTERN unsigned char palette[768];
EXTERN short numpalookups;
EXTERN unsigned char *palookup[MAXPALOOKUPS];
EXTERN unsigned char parallaxtype, showinvisibility;
EXTERN int parallaxyoffs, parallaxyscale;
EXTERN int visibility, parallaxvisibility;

EXTERN int windowx1, windowy1, windowx2, windowy2;
EXTERN short startumost[MAXXDIM], startdmost[MAXXDIM];

EXTERN short pskyoff[MAXPSKYTILES], pskybits;

EXTERN short headspritesect[MAXSECTORS+1], headspritestat[MAXSTATUS+1];
EXTERN short prevspritesect[MAXSPRITES], prevspritestat[MAXSPRITES];
EXTERN short nextspritesect[MAXSPRITES], nextspritestat[MAXSPRITES];

EXTERN short tilesizx[MAXTILES], tilesizy[MAXTILES];
EXTERN unsigned char walock[MAXTILES];
EXTERN int numtiles, picanm[MAXTILES];
EXTERN intptr_t waloff[MAXTILES];

	//These variables are for auto-mapping with the draw2dscreen function.
	//When you load a new board, these bits are all set to 0 - since
	//you haven't mapped out anything yet.  Note that these arrays are
	//bit-mapped.
	//If you want draw2dscreen() to show sprite #54 then you say:
	//   spritenum = 54;
	//   show2dsprite[spritenum>>3] |= (1<<(spritenum&7));
	//And if you want draw2dscreen() to not show sprite #54 then you say:
	//   spritenum = 54;
	//   show2dsprite[spritenum>>3] &= ~(1<<(spritenum&7));
	//Automapping defaults to 0 (do nothing).  If you set automapping to 1,
	//   then in 3D mode, the walls and sprites that you see will show up the
	//   next time you flip to 2D mode.

EXTERN unsigned char show2dsector[(MAXSECTORS+7)>>3];
EXTERN unsigned char show2dwall[(MAXWALLS+7)>>3];
EXTERN unsigned char show2dsprite[(MAXSPRITES+7)>>3];
EXTERN unsigned char automapping;

EXTERN unsigned char gotpic[(MAXTILES+7)>>3];
EXTERN unsigned char gotsector[(MAXSECTORS+7)>>3];

EXTERN int captureformat;
extern int usegammabrightness;	//0=palette, 1=shader, 2=system

extern unsigned char vgapalette[5*256];
extern unsigned int drawlinepat;

extern void faketimerhandler(void);

typedef struct {
	unsigned char r,g,b,f;
} palette_t;
extern palette_t curpalette[256], curpalettefaded[256], palfadergb;
extern unsigned char palfadedelta;

extern int dommxoverlay, novoxmips;

extern int tiletovox[MAXTILES];
extern int usevoxels, voxscale[MAXVOXELS];
#if USE_POLYMOST && USE_OPENGL
extern int usemodels, usehightile;
#endif

extern char *engineerrstr;
extern const char *build_version, *build_date, *build_time;

/*************************************************************************
POSITION VARIABLES:

		POSX is your x - position ranging from 0 to 65535
		POSY is your y - position ranging from 0 to 65535
			(the length of a side of the grid in EDITBORD would be 1024)
		POSZ is your z - position (height) ranging from 0 to 65535, 0 highest.
		ANG is your angle ranging from 0 to 2047.  Instead of 360 degrees, or
			 2 * PI radians, I use 2048 different angles, so 90 degrees would
			 be 512 in my system.

SPRITE VARIABLES:

	EXTERN short headspritesect[MAXSECTORS+1], headspritestat[MAXSTATUS+1];
	EXTERN short prevspritesect[MAXSPRITES], prevspritestat[MAXSPRITES];
	EXTERN short nextspritesect[MAXSPRITES], nextspritestat[MAXSPRITES];

	Example: if the linked lists look like the following:
		 ⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
		 ≥      Sector lists:               Status lists:               ≥
		 √ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒ¥
		 ≥  Sector0:  4, 5, 8             Status0:  2, 0, 8             ≥
		 ≥  Sector1:  16, 2, 0, 7         Status1:  4, 5, 16, 7, 3, 9   ≥
		 ≥  Sector2:  3, 9                                              ≥
		 ¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
	Notice that each number listed above is shown exactly once on both the
		left and right side.  This is because any sprite that exists must
		be in some sector, and must have some kind of status that you define.


Coding example #1:
	To go through all the sprites in sector 1, the code can look like this:

		sectnum = 1;
		i = headspritesect[sectnum];
		while (i != -1)
		{
			nexti = nextspritesect[i];

			//your code goes here
			//ex: printf("Sprite %d is in sector %d\n",i,sectnum);

			i = nexti;
		}

Coding example #2:
	To go through all sprites with status = 1, the code can look like this:

		statnum = 1;        //status 1
		i = headspritestat[statnum];
		while (i != -1)
		{
			nexti = nextspritestat[i];

			//your code goes here
			//ex: printf("Sprite %d has a status of 1 (active)\n",i,statnum);

			i = nexti;
		}

			 insertsprite(short sectnum, short statnum);
			 deletesprite(short spritenum);
			 changespritesect(short spritenum, short newsectnum);
			 changespritestat(short spritenum, short newstatnum);

TILE VARIABLES:
		NUMTILES - the number of tiles found TILES.DAT.
		TILESIZX[MAXTILES] - simply the x-dimension of the tile number.
		TILESIZY[MAXTILES] - simply the y-dimension of the tile number.
		WALOFF[MAXTILES] - the actual 32-bit offset pointing to the top-left
								 corner of the tile.
		PICANM[MAXTILES] - flags for animating the tile.

TIMING VARIABLES:
		TOTALCLOCK - When the engine is initialized, TOTALCLOCK is set to zero.
			From then on, it is incremented 120 times a second by 1.  That
			means that the number of seconds elapsed is totalclock / 120.
		NUMFRAMES - The number of times the draw3dscreen function was called
			since the engine was initialized.  This helps to determine frame
			rate.  (Frame rate = numframes * 120 / totalclock.)

OTHER VARIABLES:

		STARTUMOST[320] is an array of the highest y-coordinates on each column
				that my engine is allowed to write to.  You need to set it only
				once.
		STARTDMOST[320] is an array of the lowest y-coordinates on each column
				that my engine is allowed to write to.  You need to set it only
				once.
		SINTABLE[2048] is a sin table with 2048 angles rather than the
			normal 360 angles for higher precision.  Also since SINTABLE is in
			all integers, the range is multiplied by 16383, so instead of the
			normal -1<sin(x)<1, the range of sintable is -16383<sintable[]<16383
			If you use this sintable, you can possibly speed up your code as
			well as save space in memory.  If you plan to use sintable, 2
			identities you may want to keep in mind are:
				sintable[ang&2047]       = sin(ang * (3.141592/1024)) * 16383
				sintable[(ang+512)&2047] = cos(ang * (3.141592/1024)) * 16383
		NUMSECTORS - the total number of existing sectors.  Modified every time
			you call the loadboard function.
***************************************************************************/

int    preinitengine(void);	// a partial setup of the engine used for launch windows
int    initengine(void);
void   uninitengine(void);
void   initspritelists(void);
int   loadboard(char *filename, char fromwhere, int *daposx, int *daposy, int *daposz, short *daang, short *dacursectnum);
int   loadmaphack(char *filename);
int   saveboard(char *filename, int *daposx, int *daposy, int *daposz, short *daang, short *dacursectnum);
int   saveoldboard(char *filename, int *daposx, int *daposy, int *daposz, short *daang, short *dacursectnum);
int   loadpics(char *filename, int askedsize);
void   loadtile(short tilenume);
int   qloadkvx(int voxindex, char *filename);
intptr_t allocatepermanenttile(short tilenume, int xsiz, int ysiz);
void   copytilepiece(int tilenume1, int sx1, int sy1, int xsiz, int ysiz, int tilenume2, int sx2, int sy2);
int    makepalookup(int palnum, unsigned char *remapbuf, signed char r, signed char g, signed char b, unsigned char dastat);
void   setvgapalette(void);
	//noapply&1 - update palette but don't apply
	//noapply&2 - avoid invalidating all gl textures
	//noapply&4 - preserve current brightness
void   setbrightness(int dabrightness, unsigned char *dapal, char noapply);
void   setpalettefade(unsigned char r, unsigned char g, unsigned char b, unsigned char offset);
void   squarerotatetile(short tilenume);

#define SETGAMEMODE_FULLSCREEN(display,fulls) ((((int)(display)&255)<<8)|((int)(fulls)&255))
int   setgamemode(int dafullscreen, int daxdim, int daydim, int dabpp);
void   nextpage(void);
void   setview(int x1, int y1, int x2, int y2);
void   setaspect(int daxrange, int daaspect);
void   flushperms(void);

void   plotpixel(int x, int y, unsigned char col);
unsigned char   getpixel(int x, int y);
void   setviewtotile(short tilenume, int xsiz, int ysiz);
void   setviewback(void);
void   preparemirror(int dax, int day, int daz, short daang, int dahoriz, short dawall, short dasector, int *tposx, int *tposy, short *tang);
void   completemirror(void);

void   drawrooms(int daposx, int daposy, int daposz, short daang, int dahoriz, short dacursectnum);
void   drawmasks(void);
void   clearview(int dacol);
void   clearallviews(int dacol);
void   drawmapview(int dax, int day, int zoome, short ang);
void   rotatesprite(int sx, int sy, int z, short a, short picnum, signed char dashade, unsigned char dapalnum, unsigned short dastat, int cx1, int cy1, int cx2, int cy2);
void   drawline256(int x1, int y1, int x2, int y2, unsigned char col);
void   printext256(int xpos, int ypos, short col, short backcol, const char *name, char fontsize);

int   clipmove(int *x, int *y, int *z, short *sectnum, int xvect, int yvect, int walldist, int ceildist, int flordist, unsigned int cliptype);
int   clipinsidebox(int x, int y, short wallnum, int walldist);
int   clipinsideboxline(int x, int y, int x1, int y1, int x2, int y2, int walldist);
int   pushmove(int *x, int *y, int *z, short *sectnum, int walldist, int ceildist, int flordist, unsigned int cliptype);
void   getzrange(int x, int y, int z, short sectnum, int *ceilz, int *ceilhit, int *florz, int *florhit, int walldist, unsigned int cliptype);
int    hitscan(int xs, int ys, int zs, short sectnum, int vx, int vy, int vz, short *hitsect, short *hitwall, short *hitsprite, int *hitx, int *hity, int *hitz, unsigned int cliptype);
int   neartag(int xs, int ys, int zs, short sectnum, short ange, short *neartagsector, short *neartagwall, short *neartagsprite, int *neartaghitdist, int neartagrange, unsigned char tagsearch);
int   cansee(int x1, int y1, int z1, short sect1, int x2, int y2, int z2, short sect2);
void   updatesector(int x, int y, short *sectnum);
void   updatesectorz(int x, int y, int z, short *sectnum);
int   inside(int x, int y, short sectnum);
void   dragpoint(short pointhighlight, int dax, int day);
void   setfirstwall(short sectnum, short newfirstwall);

void   getmousevalues(int *mousx, int *mousy, int *bstatus);
int    krand(void);
int   ksqrt(int num);
int   getangle(int xvect, int yvect);
void   rotatepoint(int xpivot, int ypivot, int x, int y, short daang, int *x2, int *y2);
int   lastwall(short point);
int   nextsectorneighborz(short sectnum, int thez, short topbottom, short direction);
int   getceilzofslope(short sectnum, int dax, int day);
int   getflorzofslope(short sectnum, int dax, int day);
void   getzsofslope(short sectnum, int dax, int day, int *ceilz, int *florz);
void   alignceilslope(short dasect, int x, int y, int z);
void   alignflorslope(short dasect, int x, int y, int z);
int   sectorofwall(short theline);
int   loopnumofsector(short sectnum, short wallnum);

int   insertsprite(short sectnum, short statnum);
int   deletesprite(short spritenum);
int   changespritesect(short spritenum, short newsectnum);
int   changespritestat(short spritenum, short newstatnum);
int   setsprite(short spritenum, int newx, int newy, int newz);
int   setspritez(short spritenum, int newx, int newy, int newz);

int   screencapture(char *filename, char mode);	// mode&1 == invert, mode&2 == wait for nextpage

#if USE_POLYMOST
int   setrendermode(int renderer);
int   getrendermode(void);
# define POLYMOST_RENDERMODE_CLASSIC() (getrendermode() == 0)
# define POLYMOST_RENDERMODE_POLYMOST() (getrendermode() > 0)
# define POLYMOST_RENDERMODE_POLYGL() (getrendermode() == 3)

void    setrollangle(int rolla);
#else
# define POLYMOST_RENDERMODE_CLASSIC() (1)
# define POLYMOST_RENDERMODE_POLYMOST() (0)
# define POLYMOST_RENDERMODE_POLYGL() (0)
#endif

#if USE_OPENGL
extern int glswapinterval;
#endif

#if USE_POLYMOST && USE_OPENGL
//  pal: pass -1 to invalidate all palettes for the tile, or >=0 for a particular palette
//  how: pass -1 to invalidate all instances of the tile in texture memory, or a bitfield
//         bit 0: opaque or masked (non-translucent) texture, using repeating
//         bit 1: ignored
//         bit 2: 33% translucence, using repeating
//         bit 3: 67% translucence, using repeating
//         bit 4: opaque or masked (non-translucent) texture, using clamping
//         bit 5: ignored
//         bit 6: 33% translucence, using clamping
//         bit 7: 67% translucence, using clamping
//       clamping is for sprites, repeating is for walls
void invalidatetile(short tilenume, int pal, int how);

void setpolymost2dview(void);   // sets up GL for 2D drawing

int polymost_drawtilescreen(int tilex, int tiley, int wallnum, int dimen);
void polymost_glreset(void);
void polymost_precache_begin(void);
void polymost_precache(int dapicnum, int dapalnum, int datype);
int  polymost_precache_run(int* done, int* total);

extern int glanisotropy;
extern int glusetexcompr;
extern int gltexfiltermode;
extern int glredbluemode;
extern int glusetexcache;
extern int glmultisample, glnvmultisamplehint, glsampleshading;
void gltexapplyprops (void);

extern int polymosttexfullbright;	// set to the first index of the fullbright palette

// effect bitset: 1 = greyscale, 2 = invert
void hicsetpalettetint(int palnum, unsigned char r, unsigned char g, unsigned char b, unsigned char effect);
// flags bitset: 1 = don't compress
int hicsetsubsttex(int picnum, int palnum, char *filen, float alphacut, unsigned char flags);
int hicsetskybox(int picnum, int palnum, char *faces[6]);
int hicclearsubst(int picnum, int palnum);

int md_loadmodel(const char *fn);
int md_setmisc(int modelid, float scale, int shadeoff, float zadd);
int md_tilehasmodel(int tilenume);
int md_defineframe(int modelid, const char *framename, int tilenume, int skinnum);
int md_defineanimation(int modelid, const char *framestart, const char *frameend, int fps, int flags);
int md_defineskin(int modelid, const char *skinfn, int palnum, int skinnum, int surfnum);
int md_definehud (int modelid, int tilex, double xadd, double yadd, double zadd, double angadd, int flags);
int md_undefinetile(int tile);
int md_undefinemodel(int modelid);

#endif //USE_POLYMOST && USE_OPENGL

int loaddefinitionsfile(const char *fn);

extern int mapversion;	// if loadboard() fails with -2 return, try loadoldboard(). if it fails with -2, board is dodgy
int loadoldboard(char *filename, char fromwhere, int *daposx, int *daposy, int *daposz, short *daang, short *dacursectnum);

void buildprintf(const char *fmt, ...) PRINTF_FORMAT(1, 2);
void buildputs(const char *str);
void buildsetlogfile(const char *fn);

#ifdef _MSC_VER
#pragma pack()
#endif

#ifdef __WATCOMC__
#pragma pack(pop)
#endif

#undef BPACK

#ifdef __cplusplus
}
#endif

#endif // __build_h__
