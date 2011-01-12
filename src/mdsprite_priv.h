#ifndef MDSPRITE_PRIV_H
#define MDSPRITE_PRIV_H

typedef struct _mdskinmap_t
{
	unsigned char palette, filler[3]; // Build palette number
	int skinnum, surfnum;   // Skin identifier, surface number
	char *fn;   // Skin filename
	PTMHead *tex[HICEFFECTMASK+1];
	struct _mdskinmap_t *next;
} mdskinmap_t;

typedef struct
{
	long mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	long shadeoff;
	float scale, bscale, zadd;
	PTMHead *tex;	// skins
} mdmodel;

typedef struct _mdanim_t
{
	int startframe, endframe;
	int fpssc, flags;
	struct _mdanim_t *next;
} mdanim_t;
#define MDANIM_LOOP 0
#define MDANIM_ONESHOT 1


	//This MD2 code is based on the source code from David Henry (tfc_duke(at)hotmail.com)
	//   Was at http://tfc.duke.free.fr/us/tutorials/models/md2.htm
	//   Available from http://web.archive.org/web/20030816010242/http://tfc.duke.free.fr/us/tutorials/models/md2.htm
	//   Now at http://tfc.duke.free.fr/coding/md2.html (in French)
	//He probably wouldn't recognize it if he looked at it though :)
typedef struct { float x, y, z; } point3d;

typedef struct
{
	long id, vers, skinxsiz, skinysiz, framebytes; //id:"IPD2", vers:8
	long numskins, numverts, numuv, numtris, numglcmds, numframes;
	long ofsskins, ofsuv, ofstris, ofsframes, ofsglcmds, ofseof; //ofsskins: skin names (64 bytes each)
} md2head_t;

typedef struct { unsigned char v[3], ni; } md2vert_t; //compressed vertex coords (x,y,z)
typedef struct
{
	point3d mul, add; //scale&translation vector
	char name[16];    //frame name
	md2vert_t verts[1]; //first vertex of this frame
} md2frame_t;

typedef struct
{
		//WARNING: This top block is a union between md2model&md3model: Make sure it matches!
	long mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	long shadeoff;
	float scale, bscale, zadd;
	PTMHead **tex;   // textures for base skin if no mappings defined
	
	long numframes, cframe, nframe, fpssc, usesalpha;
	float oldtime, curtime, interpol;
	mdanim_t *animations;
	mdskinmap_t *skinmap;
	long numskins, skinloaded;   // set to 1+numofskin when a skin is loaded and the tex coords are modified,
	
		//MD2 specific stuff:
	long numverts, numglcmds, framebytes, *glcmds;
	char *frames;
	char *basepath;   // pointer to string of base path
	char *skinfn;   // pointer to first of numskins 64-char strings
} md2model;


typedef struct { char nam[64]; long i; } md3shader_t; //ascz path of shader, shader index
typedef struct { long i[3]; } md3tri_t; //indices of tri
typedef struct { float u, v; } md3uv_t;
typedef struct { signed short x, y, z; unsigned char nlat, nlng; } md3xyzn_t; //xyz are [10:6] ints

typedef struct
{
	point3d min, max, cen; //bounding box&origin
	float r; //radius of bounding sphere
	char nam[16]; //ascz frame name
} md3frame_t;

typedef struct
{
	char nam[64]; //ascz tag name
	point3d p, x, y, z; //tag object pos&orient
} md3tag_t;

typedef struct
{
	long id; //IDP3(0x33806873)
	char nam[64]; //ascz surface name
	long flags; //?
	long numframes, numshaders, numverts, numtris; //numframes same as md3head,max shade=~256,vert=~4096,tri=~8192
	md3tri_t *tris;       //file format: rel offs from md3surf
	md3shader_t *shaders; //file format: rel offs from md3surf
	md3uv_t *uv;          //file format: rel offs from md3surf
	md3xyzn_t *xyzn;      //file format: rel offs from md3surf
	long ofsend;
} md3surf_t;

typedef struct
{
	long id, vers; //id=IDP3(0x33806873), vers=15
	char nam[64]; //ascz path in PK3
	long flags; //?
	long numframes, numtags, numsurfs, numskins; //max=~1024,~16,~32,numskins=artifact of MD2; use shader field instead
	md3frame_t *frames; //file format: abs offs
	md3tag_t *tags;     //file format: abs offs
	md3surf_t *surfs;   //file format: abs offs
	long eof;           //file format: abs offs
} md3head_t;

typedef struct
{
		//WARNING: This top block is a union between md2model&md3model: Make sure it matches!
	long mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	long shadeoff;
	float scale, bscale, zadd;
	PTMHead **tex;   // textures for base skin if no mappings defined
	
	long numframes, cframe, nframe, fpssc, usesalpha;
	float oldtime, curtime, interpol;
	mdanim_t *animations;
	mdskinmap_t *skinmap;
	long numskins, skinloaded;   // set to 1+numofskin when a skin is loaded and the tex coords are modified,
	
		//MD3 specific
	md3head_t head;
} md3model;

typedef struct
{ // maps build tiles to particular animation frames of a model
	int modelid;
	int skinnum;
	int framenum;   // calculate the number from the name when declaring
} tile2model_t;
extern tile2model_t tile2model[MAXTILES];

typedef struct { float xadd, yadd, zadd; short angadd, flags; } hudtyp;
extern hudtyp hudmem[2][MAXTILES];

#define VOXUSECHAR 0
#if (VOXUSECHAR != 0)
typedef struct { unsigned char x, y, z, u, v; } vert_t;
#else
typedef struct { unsigned short x, y, z, u, v; } vert_t;
#endif
typedef struct { vert_t v[4]; } voxrect_t;
typedef struct
{
		//WARNING: This top block is a union of md2model,md3model,voxmodel: Make sure it matches!
	long mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	long shadeoff;
	float scale, bscale, zadd;
	GLuint *texid;	// skins for palettes
	
		//VOX specific stuff:
	voxrect_t *quad; long qcnt, qfacind[7];
	long *mytex, mytexx, mytexy;
	long xsiz, ysiz, zsiz;
	float xpiv, ypiv, zpiv;
	long is8bit;
} voxmodel;

extern voxmodel *voxmodels[MAXVOXELS];
extern mdmodel **models;

extern char mdinited;
extern long mdtims, omdtims;
extern long nextmodelid;

void freeallmodels ();
void clearskins ();
void voxfree (voxmodel *m);
voxmodel *voxload (const char *filnam);
int voxdraw (voxmodel *m, spritetype *tspr);

void mdinit ();
PTMHead * mdloadskin (md2model *m, int number, int pal, int surf);
int mddraw (spritetype *);

#endif
