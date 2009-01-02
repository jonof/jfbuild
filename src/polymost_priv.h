#ifdef POLYMOST

#define PI 3.14159265358979323

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

extern long gltexmaxsize;	// 0 means autodetection on first run
extern long gltexmiplevel;	// discards this many mipmap levels


typedef struct {
	char magic[8];	// 'Polymost'
	long xdim, ydim;	// of image, unpadded
	long flags;		// 1 = !2^x, 2 = has alpha, 4 = lzw compressed
} texcacheheader;
typedef struct {
	long size;
	long format;
	long xdim, ydim;	// of mipmap (possibly padded)
	long border, depth;
} texcachepicture;

#if 0 //DEPRECATED
extern const char *TEXCACHEDIR;
void phex(unsigned char v, char *s);
void uploadtexture(long doalloc, long xsiz, long ysiz, long intexfmt, long texfmt, coltype *pic, long tsizx, long tsizy, long dameth);
#endif

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
