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

extern int rendmode;
extern float gtang;
extern float glox1, gloy1;
extern double gxyaspect, grhalfxdown10x;
extern double gcosang, gsinang, gcosang2, gsinang2;
extern double gchang, gshang, gctang, gstang;

struct glfiltermodes {
	char *name;
	int min,mag;
};
#define numglfiltermodes 6
extern struct glfiltermodes glfiltermodes[numglfiltermodes];

extern int gltexcomprquality;	// 0 = fast, 1 = slow and pretty, 2 = very slow and pretty
extern int gltexmaxsize;	// 0 means autodetection on first run
extern int gltexmiplevel;	// discards this many mipmap levels

int polymost_texmayhavealpha (int dapicnum, int dapalnum);
void polymost_texinvalidate (int dapicnum, int dapalnum, int dameth);
void polymost_texinvalidateall ();
void polymost_glinit();
void polymost_drawrooms ();
void polymost_drawmaskwall (int damaskwallcnt);
void polymost_drawsprite (int snum);
void polymost_dorotatesprite (int sx, int sy, int z, short a, short picnum,
	signed char dashade, unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2, int uniqid);
void polymost_fillpolygon (int npoints);
int polymost_printext256(int xpos, int ypos, short col, short backcol, const char *name, char fontsize);
void polymost_initosdfuncs(void);


#endif
