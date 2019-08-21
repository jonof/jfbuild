#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif

#define PI 3.14159265358979323

extern int rendmode;
extern float gtang;
extern double dxb1[MAXWALLSB], dxb2[MAXWALLSB];

enum {
    METH_SOLID   = 0,
    METH_MASKED  = 1,
    METH_TRANS   = 2,
    METH_INTRANS = 3,

    METH_CLAMPED = 4,
    METH_LAYERS  = 8,       // when given to drawpoly, renders the additional texture layers
    METH_POW2XSPLIT = 16,   // when given to drawpoly, splits polygons for non-2^x-capable GL devices
    METH_ROTATESPRITE = 32, // when given to drawpoly, use the rotatesprite projection matrix
};

#if USE_OPENGL

#include "glbuild.h"

typedef struct { unsigned char r, g, b, a; } coltype;
typedef struct { GLfloat r, g, b, a; } coltypef;

extern float glox1, gloy1;
extern double gxyaspect, grhalfxdown10x;
extern double gcosang, gsinang, gcosang2, gsinang2;
extern double gchang, gshang, gctang, gstang;
extern int gfogpalnum;
extern float gfogdensity;

struct glfiltermodes {
	char *name;
	int min,mag;
};
#define numglfiltermodes 6
extern struct glfiltermodes glfiltermodes[numglfiltermodes];

extern int gltexcomprquality;	// 0 = fast, 1 = slow and pretty, 2 = very slow and pretty
extern int gltexmaxsize;	// 0 means autodetection on first run
extern int gltexmiplevel;	// discards this many mipmap levels

extern const GLfloat gidentitymat[4][4];
extern GLfloat gdrawroomsprojmat[4][4];      // Proj. matrix for drawrooms() calls.
extern GLfloat grotatespriteprojmat[4][4];   // Proj. matrix for rotatesprite() calls.
extern GLfloat gorthoprojmat[4][4];          // Proj. matrix for 2D (aux) calls.

enum {
    POLYMOST_DRAWPOLY_TRIANGLES = 0,
    POLYMOST_DRAWPOLY_TRIANGLEFAN = 1,
};
#define TRIANGLEFAN_POINTS_TO_INDEXES(p) (((p) - 2) * 3)

typedef struct polymostvertexitem {
    struct {    // Vertex
        GLfloat x, y, z;
    } v;
    struct {    // Texture
        GLfloat s, t;
    } t;
    coltypef c; // Colour
} polymostvertexitem;

/* A draw batch specification to request drawing some triangles. */
typedef struct polymostdrawpolyspec {
    GLuint texture0;        // Base texture.
    GLuint texture1;        // Glow texture.
    GLfloat alphacut;       // Alpha discard cutoff.
    coltypef colour;        // Static colour.
    coltypef fogcolour;     // Distance fog colour.
    GLfloat fogdensity;     // Distance fog density.

    GLboolean blend;        // Enable GL_BLEND?
    GLboolean depthtest;    // Enable GL_DEPTH_TEST?

    const GLfloat *modelview;     // 4x4 matrices.
    const GLfloat *projection;
} polymostdrawpolyspec;

/* An opaque handle to a draw batch. */
typedef void * polymostbatchref;

/* A single immediate-mode draw call. */
typedef struct polymostdrawpolycall {
    polymostdrawpolyspec spec;

    GLuint mode;    // POLYMOST_DRAWPOLY_xxx

    GLuint indexbuffer;     // Buffer object identifier.
        // When set to 0:
        //   If mode == POLYMOST_DRAWPOLY_TRIANGLEFAN,
        //     uses the global triangle-fan index buffer.
        //     0 - 1 - 2, 0 - 2 - 3, 0 - 3 - 4, ...
        //   If mode == POLYMOST_DRAWPOLY_TRIANGLES,
        //     uses the global triangle index buffer.
        //     0, 1, 2, 3, 4, ...
    GLuint indexcount;      // Number of index items.

    GLuint vertexbuffer;   // Buffer object identifier. >0 ignores vertexes.
    GLuint vertexcount;    // Number of vertexes in the buffer. Ignored if vertexbuffer >0.
    const polymostvertexitem *vertexes; // Elements. vertexbuffer must be 0 to recognise this.
} polymostdrawpolycall;

// Smallest initial size for the global index buffer.
#define MINVBOINDEXES 16

typedef struct polymostdrawauxcall {
    GLuint texture0;
    coltypef colour;
    coltypef bgcolour;
    int mode;

    GLuint indexcount;      // Number of index items.
    GLushort *indexes;      // Array of indexes, or NULL to use the global index buffer.

    int vertexcount;
    polymostvertexitem *vertexes;
} polymostdrawauxcall;

/* Begin a draw call batch.
    spec        A specification for the polygons to be drawn.
    maxtris     The expected maximum number of triangles to be drawn. If the batch
                will overflow an implicit flush of the batch so far will be done.

    Returns an opaque batch reference to pass into polymost_drawpoly_draw.
*/
polymostbatchref polymost_drawpoly_begin(polymostdrawpolyspec *spec, int maxtris);

/* Force a flush of all batched polygons. */
void polymost_drawpoly_flush(void);

/* Queue some polygons into a batch.
    ref         A batch reference fetched from polymost_drawpoly_begin.
    mode        A POLYMOST_DRAWPOLY_xxx value describing how vertexes will be interpreted.
    vertexes    A collection of vertexes.
    numvertexes A number of vertexes.
*/
void polymost_drawpoly_draw(polymostbatchref ref, int mode, polymostvertexitem *vertexes, int numvertexes);

/* An immediate draw call. */
void polymost_drawpoly_immed(polymostdrawpolycall *draw);

int polymost_texmayhavealpha (int dapicnum, int dapalnum);
void polymost_texinvalidate (int dapicnum, int dapalnum, int dameth);
void polymost_texinvalidateall (void);
void polymost_glinit(void);
int polymost_printext256(int xpos, int ypos, short col, short backcol, const char *name, char fontsize);
int polymost_drawline256(int x1, int y1, int x2, int y2, unsigned char col);
int polymost_plotpixel(int x, int y, unsigned char col);
void polymost_fillpolygon (int npoints);
void polymost_setview(void);

#endif //USE_OPENGL

void polymost_nextpage(void);
void polymost_drawrooms (void);
void polymost_drawmaskwall (int damaskwallcnt);
void polymost_drawsprite (int snum);
void polymost_dorotatesprite (int sx, int sy, int z, short a, short picnum,
    signed char dashade, unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2, int uniqid);
void polymost_initosdfuncs(void);
