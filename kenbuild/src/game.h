// game.h

#define NUMOPTIONS 8
#define NUMKEYS 20

extern char option[NUMOPTIONS];
extern int keys[NUMKEYS];
extern int xdimgame, ydimgame, bppgame;
extern int forcesetup;

void	operatesector(short dasector);
void	operatesprite(short dasprite);
int	changehealth(short snum, short deltahealth);
void	changenumbombs(short snum, short deltanumbombs);
void	changenummissiles(short snum, short deltanummissiles);
void	changenumgrabbers(short snum, short deltanumgrabbers);
void	drawstatusflytime(short snum);
void	drawstatusbar(short snum);
void	prepareboard(char *daboardfilename);
void	checktouchsprite(short snum, short sectnum);
void	checkgrabbertouchsprite(short snum, short sectnum);
void	shootgun(short snum, int x, int y, int z, short daang, int dahoriz, short dasectnum, unsigned char guntype);
void	analyzesprites(int dax, int day);
void	tagcode(void);
void	statuslistcode(void);
void	activatehitag(short dahitag);
void	bombexplode(int i);
void	processinput(short snum);
void	view(short snum, int *vx, int *vy, int *vz, short *vsectnum, short ang, int horiz);
void	drawscreen(short snum, int dasmoothratio);
void	movethings(void);
void	fakedomovethings(void);
void	fakedomovethingscorrect(void);
void	domovethings(void);
void	getinput(void);
void	initplayersprite(short snum);
void	playback(void);
void	setup3dscreen(void);
void	findrandomspot(int *x, int *y, short *sectnum);
void	warp(int *x, int *y, int *z, short *daang, short *dasector);
void	warpsprite(short spritenum);
void	initlava(void);
void	movelava(unsigned char *dapic);
void	doanimations(void);
int	getanimationgoal(int *animptr);
int	setanimation(int *animptr, int thegoal, int thevel, int theacc);
void	checkmasterslaveswitch(void);
int	testneighborsectors(short sect1, short sect2);
int	loadgame(void);
int	savegame(void);
void	faketimerhandler(void);
void	getpackets(void);
void	drawoverheadmap(int cposx, int cposy, int czoom, short cang);
int	movesprite(short spritenum, int dx, int dy, int dz, int ceildist, int flordist, int clipmask);
void	waitforeverybody(void);
void	searchmap(short startsector);
void	setinterpolation(int *posptr);
void	stopinterpolation(int *posptr);
void	updateinterpolations(void);
void	dointerpolations(void);
void	restoreinterpolations(void);
void	printext(int x, int y, char *buffer, short tilenum, unsigned char invisiblecol);
void	drawtilebackground (int thex, int they, short tilenum, signed char shade, int cx1, int cy1, int cx2, int cy2, unsigned char dapalnum);



struct startwin_settings {
    int fullscreen;
    int xdim3d, ydim3d, bpp3d;
    int forcesetup;

    int numplayers;
    char *joinhost;
    int netoverride;
};
