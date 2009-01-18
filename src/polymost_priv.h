#ifdef POLYMOST

#define PI 3.14159265358979323

enum {
	METH_SOLID   = 0,
	METH_MASKED  = 1,
	METH_TRANS   = 2,
	METH_INTRANS = 3,
	
	METH_CLAMPED = 4,
	METH_LAYERS  = 8,	// when given to drawpoly, renders the additional layers
};

typedef struct { unsigned char r, g, b, a; } coltype;

extern long rendmode;
extern float gtang;
extern float glox1, gloy1;
extern double gxyaspect, grhalfxdown10x;
extern double gcosang, gsinang, gcosang2, gsinang2;
extern double gchang, gshang, gctang, gstang;

struct glfiltermodes {
	char *name;
	long min,mag;
};
#define numglfiltermodes 6
extern struct glfiltermodes glfiltermodes[numglfiltermodes];

extern long gltexcomprquality;	// 0 = fast, 1 = slow and pretty, 2 = very slow and pretty
extern long gltexmaxsize;	// 0 means autodetection on first run
extern long gltexmiplevel;	// discards this many mipmap levels

long polymost_texmayhavealpha (long dapicnum, long dapalnum);
void polymost_texinvalidate (long dapicnum, long dapalnum, long dameth);
void polymost_texinvalidateall ();
void polymost_glinit();
void polymost_drawrooms ();
void polymost_drawmaskwall (long damaskwallcnt);
void polymost_drawsprite (long snum);
void polymost_dorotatesprite (long sx, long sy, long z, short a, short picnum,
	signed char dashade, char dapalnum, char dastat, long cx1, long cy1, long cx2, long cy2, long uniqid);
void polymost_fillpolygon (long npoints);
long polymost_printext256(long xpos, long ypos, short col, short backcol, char *name, char fontsize);
void polymost_initosdfuncs(void);


#endif
