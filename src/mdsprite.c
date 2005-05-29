//------------------------------------- MD2/MD3 LIBRARY BEGINS -------------------------------------

typedef struct _mdanim_t
{
	int startframe, endframe;
	int fpssc, flags;
	struct _mdanim_t *next;
} mdanim_t;
#define MDANIM_LOOP 0
#define MDANIM_ONESHOT 1

typedef struct _mdskinmap_t
{
	unsigned char palette, filler[3]; // Build palette number
	int skinnum;   // Skin identifier
	char *fn;   // Skin filename
	unsigned int texid[HICEFFECTMASK+1];   // OpenGL texture numbers for effect variations
	struct _mdskinmap_t *next;
} mdskinmap_t;


	//This MD2 code is based on the source code from David Henry (tfc_duke(at)hotmail.com)
	//   Was at http://tfc.duke.free.fr/us/tutorials/models/md2.htm
	//   Available from http://web.archive.org/web/20040215150157/http://tfc.duke.free.fr/us/tutorials/models/md2.htm
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
	long mdnum; //MD2=2, MD3=3. NOTE: must be first in structure!
	long numframes, cframe, nframe, fpssc, usesalpha, shadeoff;
	float oldtime, curtime, interpol, scale, bscale, zadd;
	mdanim_t *animations;
	mdskinmap_t *skinmap;
	long numskins, skinloaded;   // set to 1+numofskin when a skin is loaded and the tex coords are modified,
	unsigned int *texid;   // texture ids for base skin if no mappings defined

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
	long mdnum; //MD2=2, MD3=3. NOTE: must be first in structure!
	long numframes, cframe, nframe, fpssc, usesalpha, shadeoff;
	float oldtime, curtime, interpol, scale, bscale, zadd;
	mdanim_t *animations;
	mdskinmap_t *skinmap;
	long numskins, skinloaded;   // set to 1+numofskin when a skin is loaded and the tex coords are modified,
	unsigned int *texid;   // texture ids for base skin if no mappings defined

		//MD3 specific
	md3head_t head;
	float zmin, zmax;
} md3model;


typedef struct
{ // maps build tiles to particular animation frames of a model
	int modelid;
	int skinnum;
	int framenum;   // calculate the number from the name when declaring
} tile2model_t;
static tile2model_t tile2model[MAXTILES];

	//Move this to appropriate place!
typedef struct { float xadd, yadd, zadd; short angadd, flags; } hudtyp;
hudtyp hudmem[2][MAXTILES]; //~320KB ... ok for now ... could replace with dynamic alloc

static char mdinited=0;

#define MODELALLOCGROUP 256
static long nummodelsalloced = 0, nextmodelid = 0;
static void **models = NULL;

static long maxmodelverts = 0, allocmodelverts = 0;
static point3d *vertlist = NULL; //temp array to store interpolated vertices for drawing

void *mdload (const char *);
void mddraw (spritetype *);
void mdfree (void *);

static void freeallmodels ()
{
	int i;

	if (models)
	{
		for(i=0;i<nextmodelid;i++) mdfree(models[i]);
		free(models); models = NULL;
		nummodelsalloced = 0;
		nextmodelid = 0;
	}

	memset(tile2model,-1,sizeof(tile2model));

	if (vertlist)
	{
		free(vertlist);
		vertlist = NULL;
		allocmodelverts = maxmodelverts = 0;
	}
}

static void clearskins ()
{
	md2model *m;
	mdskinmap_t *sk;
	int i, j;

	for(i=0;i<nextmodelid;i++)
	{
		m = (md2model *)models[i];
		for(j=0;j<m->numskins*(HICEFFECTMASK+1);j++)
		{
			if (m->texid[j]) bglDeleteTextures(1,&m->texid[j]);
			m->texid[j] = 0;
		}

		for(sk=m->skinmap;sk;sk=sk->next)
			for(j=0;j<(HICEFFECTMASK+1);j++)
			{
				if (sk->texid[j]) bglDeleteTextures(1,&sk->texid[j]);
				sk->texid[j] = 0;
			}
	}
}

static void mdinit ()
{
	memset(hudmem,0,sizeof(hudmem));
	freeallmodels();
	mdinited = 1;
}

int md_loadmodel (const char *fn)
{
	void *vm, **ml;

	if (!mdinited) mdinit();

	if (nextmodelid >= nummodelsalloced)
	{
		ml = (void **)realloc(models,(nummodelsalloced+MODELALLOCGROUP)*4); if (!ml) return(-1);
		models = ml; nummodelsalloced += MODELALLOCGROUP;
	}

	vm = mdload(fn); if (!vm) return(-1);
	models[nextmodelid++] = vm;
	return(nextmodelid-1);
}

int md_setmisc (int modelid, float scale, int shadeoff, float zadd)
{
	md2model *m;

	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;
	m = (md2model *)models[modelid];
	m->bscale = scale;
	m->shadeoff = shadeoff;
	m->zadd = zadd;

	return 0;
}

int md_tilehasmodel (int tilenume)
{
	return (tile2model[tilenume].modelid != -1);
}

static long framename2index (void *vm, const char *nam)
{
	int i;

	switch(*(long *)vm)
	{
		case 2:
			{
			md2model *m = (md2model *)vm;
			md2frame_t *fr;
			for(i=0;i<m->numframes;i++)
			{
				fr = (md2frame_t *)&m->frames[i*m->framebytes];
				if (!Bstrcmp(fr->name, nam)) break;
			}
			}
			break;
		case 3:
			{
			md3model *m = (md3model *)vm;
			for(i=0;i<m->numframes;i++)
				if (!Bstrcmp(m->head.frames[i].nam,nam)) break;
			}
			break;
	}
	return(i);
}

int md_defineframe (int modelid, const char *framename, int tilenume, int skinnum)
{
	void *vm;
	md2model *m;
	int i;

	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return(-1);
	if ((unsigned long)tilenume >= (unsigned long)MAXTILES) return(-2);
	if (!framename) return(-3);

	m = (md2model *)models[modelid];
	i = framename2index(m,framename);
	if (i == m->numframes) return(-3);   // frame name invalid

	tile2model[tilenume].modelid = modelid;
	tile2model[tilenume].framenum = i;
	tile2model[tilenume].skinnum = skinnum;

	return 0;
}

int md_defineanimation (int modelid, const char *framestart, const char *frameend, int fpssc, int flags)
{
	md2model *m;
	mdanim_t ma, *map;
	int i;

	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return(-1);

	memset(&ma, 0, sizeof(ma));
	m = (md2model *)models[modelid];

		//find index of start frame
	i = framename2index(m,framestart);
	if (i == m->numframes) return -2;
	ma.startframe = i;

		//find index of finish frame which must trail start frame
	i = framename2index(m,frameend);
	if (i == m->numframes) return -3;
	ma.endframe = i;

	ma.fpssc = fpssc;
	ma.flags = flags;

	map = (mdanim_t*)calloc(1,sizeof(mdanim_t));
	if (!map) return(-4);
	memcpy(map, &ma, sizeof(ma));

	map->next = m->animations;
	m->animations = map;

	return(0);
}

int md_defineskin (int modelid, const char *skinfn, int palnum, int skinnum)
{
	mdskinmap_t *sk, *skl;
	md2model *m;

	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;
	if (!skinfn) return -2;
	if ((unsigned)palnum >= (unsigned)MAXPALOOKUPS) return -3;

	m = (md2model *)models[modelid];
	skl = NULL;
	for (sk = m->skinmap; sk; skl = sk, sk = sk->next)
		if (sk->palette == (unsigned char)palnum && skinnum == sk->skinnum) break;
	if (!sk) {
		sk = (mdskinmap_t *)calloc(1,sizeof(mdskinmap_t));
		if (!sk) return -4;

		if (!skl) m->skinmap = sk;
		else skl->next = sk;
	} else if (sk->fn) free(sk->fn);

	sk->palette = (unsigned char)palnum;
	sk->skinnum = skinnum;
	sk->fn = (char *)malloc(strlen(skinfn)+1);
	if (!sk->fn) return(-4);
	strcpy(sk->fn, skinfn);

	return 0;
}

int md_definehud (int modelid, int tilex, double xadd, double yadd, double zadd, double angadd, int flags)
{
	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;
	if ((unsigned long)tilex >= (unsigned long)MAXTILES) return -2;

	hudmem[(flags>>2)&1][tilex].xadd = xadd;
	hudmem[(flags>>2)&1][tilex].yadd = yadd;
	hudmem[(flags>>2)&1][tilex].zadd = zadd;
	hudmem[(flags>>2)&1][tilex].angadd = ((short)angadd)|2048;
	hudmem[(flags>>2)&1][tilex].flags = (short)flags;

	return 0;
}

static int daskinloader (const char *fn, long *fptr, long *bpl, long *sizx, long *sizy, long *osizx, long *osizy, char *hasalpha, char effect)
{
	long filh, picfillen, j,y,x;
	char *picfil,*cptr,al=255;
	coltype *pic;
	long xsiz, ysiz, tsizx, tsizy;

	if ((filh = kopen4load((char*)fn, 0)) < 0) {
		OSD_Printf("MD2 Skin: %s not found.\n",fn);
		return -1;
	}
	picfillen = kfilelength(filh);
	picfil = (char *)malloc(picfillen); if (!picfil) { kclose(filh); return -1; }
	kread(filh, picfil, picfillen);
	kclose(filh);

	// tsizx/y = replacement texture's natural size
	// xsiz/y = 2^x size of replacement

	kpgetdim(picfil,picfillen,&tsizx,&tsizy);
	if (tsizx == 0 || tsizy == 0) { free(picfil); return -1; }

	for(xsiz=1;xsiz<tsizx;xsiz+=xsiz);
	for(ysiz=1;ysiz<tsizy;ysiz+=ysiz);
	*osizx = tsizx; *osizy = tsizy;
	pic = (coltype *)malloc(xsiz*ysiz*sizeof(coltype));
	if (!pic) { free(picfil); return -1; }
	memset(pic,0,xsiz*ysiz*sizeof(coltype));

	if (kprender(picfil,picfillen,(long)pic,xsiz*sizeof(coltype),xsiz,ysiz,0,0))
		{ free(picfil); free(pic); return -1; }
	free(picfil);

	cptr = &britable[curbrightness][0];
	for(y=0,j=0;y<tsizy;y++,j+=xsiz)
	{
		coltype *rpptr = &pic[j], tcol;

		for(x=0;x<tsizx;x++)
		{
			tcol.b = cptr[rpptr[x].b];
			tcol.g = cptr[rpptr[x].g];
			tcol.r = cptr[rpptr[x].r];

			if (effect & 1) {
				// greyscale
				tcol.b = max(tcol.b, max(tcol.g, tcol.r));
				tcol.g = tcol.r = tcol.b;
			}
			if (effect & 2) {
				// invert
				tcol.b = 255-tcol.b;
				tcol.g = 255-tcol.g;
				tcol.r = 255-tcol.r;
			}

			rpptr[x].b = tcol.b;
			rpptr[x].g = tcol.g;
			rpptr[x].r = tcol.r;
			al &= rpptr[x].a;
		}
	}
	if (!glinfo.bgra) {
		for(j=xsiz*ysiz-1;j>=0;j--) {
			swapchar(&pic[j].r, &pic[j].b);
		}
	}

	*sizx = xsiz;
	*sizy = ysiz;
	*bpl = xsiz;
	*fptr = (long)pic;
	*hasalpha = (al != 255);
	return 0;
}

	//Note: even though it says md2model, it works for both md2model&md3model
static long mdloadskin (md2model *m, int number, int pal)
{
	long i, fptr, bpl, xsiz, ysiz, osizx, osizy, texfmt = GL_RGBA, intexfmt = GL_RGBA;
	char *skinfile, hasalpha, fn[BMAX_PATH+65];
	unsigned int *texidx;
	mdskinmap_t *sk, *skzero = NULL;

	if ((unsigned)pal >= (unsigned)MAXPALOOKUPS) return 0;
	i = -1;
	for (sk = m->skinmap; sk; sk = sk->next)
	{
		if ((int)sk->palette == pal && sk->skinnum == number)
		{
			skinfile = sk->fn;
			texidx = &sk->texid[ hictinting[pal].f ];
			strcpy(fn,skinfile);
			//OSD_Printf("Using def skin (%d,%d) %s\n",pal,number,skinfile);
			break;
		}
			//If no match, give highest priority to number, then pal.. (Parkar's request, 02/27/2005)
		else if (((int)sk->palette ==   0) && (sk->skinnum == number) && (i < 2)) { i = 2; skzero = sk; }
		else if (((int)sk->palette == pal) && (sk->skinnum ==      0) && (i < 1)) { i = 1; skzero = sk; }
		else if (((int)sk->palette ==   0) && (sk->skinnum ==      0) && (i < 0)) { i = 0; skzero = sk; }
	}
	if (!sk)
	{
		if (skzero)
		{
			skinfile = skzero->fn;
			texidx = &skzero->texid[ hictinting[pal].f ];
			strcpy(fn,skinfile);
			//OSD_Printf("Using def skin 0,0 as fallback, pal=%d\n", pal);
		}
		else
		{
			if ((unsigned)number >= (unsigned)m->numskins) number = 0;
			skinfile = m->skinfn + number*64;
			texidx = &m->texid[ number * (HICEFFECTMASK+1) + hictinting[pal].f ];
			strcpy(fn,m->basepath); strcat(fn,skinfile);
			//OSD_Printf("Using MD2/MD3 skin (%d) %s, pal=%d\n",number,skinfile,pal);
		}
	}
	if (!skinfile[0]) return 0;

	if (*texidx) return *texidx;
	*texidx = 0;

	if (daskinloader(fn,&fptr,&bpl,&xsiz,&ysiz,&osizx,&osizy,&hasalpha,hictinting[pal].f))
	{
		initprintf("Failed loading skin file \"%s\"\n", fn);
		skinfile[0] = 0;
		return(0);
	}
	m->usesalpha = hasalpha;
	if (!m->skinloaded)
	{
		if (xsiz != osizx || ysiz != osizy)
		{
			long i, *lptr;
			float fx, fy;
			fx = ((float)osizx)/((float)xsiz);
			fy = ((float)osizy)/((float)ysiz);
			for(lptr=m->glcmds;(i=*lptr++);)
				for(i=labs(i);i>0;i--,lptr+=3)
				{
					((float *)lptr)[0] *= fx;
					((float *)lptr)[1] *= fy;
				}
		}
		m->skinloaded = 1+number;
	}

	bglGenTextures(1,texidx);
	bglBindTexture(GL_TEXTURE_2D,*texidx);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,glfiltermodes[gltexfiltermode].mag);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glfiltermodes[gltexfiltermode].min);
	if (glinfo.maxanisotropy > 1.0)
		bglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,glanisotropy);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

	//gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGBA,xsiz,ysiz,GL_BGRA_EXT,GL_UNSIGNED_BYTE,(char *)fptr);
	//bglTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,xsiz,ysiz,0,GL_BGRA_EXT,GL_UNSIGNED_BYTE,(char *)fptr);
	if (glinfo.texcompr && glusetexcompr) intexfmt = GL_COMPRESSED_RGBA_ARB;
	if (glinfo.bgra) texfmt = GL_BGRA;
	uploadtexture(1, xsiz, ysiz, intexfmt, texfmt, (coltype*)fptr, xsiz, ysiz, 0);

	free((void*)fptr);
	return(*texidx);
}

	//Note: even though it says md2model, it works for both md2model&md3model
static void updateanimation (md2model *m, spritetype *tspr)
{
	mdanim_t *anim;
	long i, j;

	m->cframe = m->nframe = tile2model[tspr->picnum].framenum;

	for (anim = m->animations;
		  anim && anim->startframe != m->cframe;
		  anim = anim->next) ;
	if (!anim) { m->interpol = 0; return; }

	if (((long)spriteext[tspr->owner].mdanimcur) != anim->startframe ||
			(spriteext[tspr->owner].flags & SPREXT_NOMDANIM))
	{
		spriteext[tspr->owner].mdanimcur = (short)anim->startframe;
		spriteext[tspr->owner].mdanimtims = mdtims;
		m->cframe = m->nframe = anim->startframe;
		m->interpol = 0;
		return;
	}

	i = (mdtims-spriteext[tspr->owner].mdanimtims)*anim->fpssc;
	j = ((anim->endframe+1-anim->startframe)<<16);

		//Just in case you play the game for a VERY long time...
	if (i < 0) { i = 0; spriteext[tspr->owner].mdanimtims = mdtims; }
		//compare with j*2 instead of j to ensure i stays > j-65536 for MDANIM_ONESHOT
	if ((i >= j+j) && (anim->fpssc)) //Keep mdanimtims close to mdtims to avoid the use of MOD
		spriteext[tspr->owner].mdanimtims += j/anim->fpssc;

	if (anim->flags&MDANIM_ONESHOT)
		{ if (i > j-65536) i = j-65536; }
	else { if (i >= j) { i -= j; if (i >= j) i %= j; } }

	m->cframe = (i>>16)+anim->startframe;
	m->nframe = m->cframe+1; if (m->nframe > anim->endframe) m->nframe = anim->startframe;
	m->interpol = ((float)(i&65535))/65536.f;
}

static void md2free (md2model *m)
{
	mdanim_t *anim, *nanim = NULL;
	mdskinmap_t *sk, *nsk = NULL;

	if (!m) return;

	for(anim=m->animations; anim; anim=nanim)
	{
		nanim = anim->next;
		free(anim);
	}
	for(sk=m->skinmap; sk; sk=nsk)
	{
		nsk = sk->next;
		free(sk->fn);
		free(sk);
	}

	if (m->frames) free(m->frames);
	if (m->glcmds) free(m->glcmds);
	if (m->basepath) free(m->basepath);
	if (m->skinfn) free(m->skinfn);

	if (m->texid) free(m->texid);
}

static void *md2load (int fil, const char *filnam)
{
	md2model *m;
	md2head_t head;
	char *buf, st[BMAX_PATH];
	long i;

	m = (md2model *)calloc(1,sizeof(md2model)); if (!m) return(0);
	m->mdnum = 2; m->scale = .01;

	kread(fil,(char *)&head,sizeof(md2head_t));
	if ((head.id != 0x32504449) || (head.vers != 8)) { free(m); return(0); } //"IDP2"

	m->numskins = head.numskins;
	m->numframes = head.numframes;
	m->numverts = head.numverts;
	m->numglcmds = head.numglcmds;
	m->framebytes = head.framebytes;
	m->frames = (char *)calloc(m->numframes,head.framebytes); if (!m->frames) { free(m); return(0); }
	m->glcmds = (long *)calloc(m->numglcmds,sizeof(long)); if (!m->glcmds) { free(m->frames); free(m); return(0); }
	klseek(fil,head.ofsframes,SEEK_SET);
	if (kread(fil,(char *)m->frames,m->numframes*head.framebytes) != m->numframes*head.framebytes)
		{ free(m->glcmds); free(m->frames); free(m); return(0); }
	klseek(fil,head.ofsglcmds,SEEK_SET);
	if (kread(fil,(char *)m->glcmds,m->numglcmds*sizeof(long)) != (long)(m->numglcmds*sizeof(long)))
		{ free(m->glcmds); free(m->frames); free(m); return(0); }

	strcpy(st,filnam);
	for(i=strlen(st)-1;i>0;i--)
		if ((st[i] == '/') || (st[i] == '\\')) { i++; break; }
	if (i<0) i=0;
	st[i] = 0;
	m->basepath = (char *)malloc(i+1); if (!m->basepath) { free(m->glcmds); free(m->frames); free(m); return(0); }
	strcpy(m->basepath, st);

	m->skinfn = (char *)calloc(head.numskins,64); if (!m->skinfn) { free(m->basepath); free(m->glcmds); free(m->frames); free(m); return(0); }
	klseek(fil,head.ofsskins,SEEK_SET);
	if (kread(fil,m->skinfn,64*head.numskins) != 64*head.numskins)
		{ free(m->glcmds); free(m->frames); free(m); return(0); }

	m->texid = (unsigned int *)calloc(head.numskins, sizeof(unsigned int) * (HICEFFECTMASK+1));
	if (!m->texid) { free(m->skinfn); free(m->basepath); free(m->glcmds); free(m->frames); free(m); return(0); }

	maxmodelverts = max(maxmodelverts, head.numverts);

	return((void *)m);
}

static void md2draw (md2model *m, spritetype *tspr)
{
	point3d fp, m0, m1, a0, a1;
	md2frame_t *f0, *f1;
	unsigned char *c0, *c1;
	long i, *lptr;
	float f, g, k0, k1, k2, k3, k4, k5, k6, k7, mat[16], pc[4];

	updateanimation(m,tspr);

// -------- Unnecessarily clean (lol) code to generate translation/rotation matrix for MD2 ---------

		//create current&next frame's vertex list from whole list
	f0 = (md2frame_t *)&m->frames[m->cframe*m->framebytes];
	f1 = (md2frame_t *)&m->frames[m->nframe*m->framebytes];
	f = m->interpol; g = 1-f;
	m0.x = f0->mul.x*m->scale*g; m1.x = f1->mul.x*m->scale*f;
	m0.y = f0->mul.y*m->scale*g; m1.y = f1->mul.y*m->scale*f;
	m0.z = f0->mul.z*m->scale*g; m1.z = f1->mul.z*m->scale*f;
	a0.x = f0->add.x*m->scale; a0.x = (f1->add.x*m->scale-a0.x)*f+a0.x;
	a0.y = f0->add.y*m->scale; a0.y = (f1->add.y*m->scale-a0.y)*f+a0.y;
	a0.z = f0->add.z*m->scale; a0.z = (f1->add.z*m->scale-a0.z)*f+a0.z + m->zadd*m->scale;
	c0 = &f0->verts[0].v[0]; c1 = &f1->verts[0].v[0];

	if (globalorientation&8) //y-flipping
	{
		m0.z = -m0.z; m1.z = -m1.z; a0.z = -a0.z;
			//Add height of 1st frame (use same frame to prevent animation bounce)
		a0.z += ((md2frame_t *)m->frames)->mul.z*m->scale*255;
	}
	if (globalorientation&4) { m0.y = -m0.y; m1.y = -m1.y; a0.y = -a0.y; } //x-flipping

	f = ((float)tspr->xrepeat)/64*m->bscale;
	m0.x *= f; m1.x *= f; a0.x *= f; f = -f;   // 20040610: backwards models aren't cool
	m0.y *= f; m1.y *= f; a0.y *= f;
	f = ((float)tspr->yrepeat)/64*m->bscale;
	m0.z *= f; m1.z *= f; a0.z *= f;

	k0 = tspr->z;
	if (globalorientation&128) k0 += (float)((tilesizy[tspr->picnum]*tspr->yrepeat)<<1);

	f = (65536.0*512.0)/((float)xdimen*viewingrange);
	g = 32.0/((float)xdimen*gxyaspect);
	m0.y *= f; m1.y *= f; a0.y = (((float)(tspr->x-globalposx))/  1024.0 + a0.y)*f;
	m0.x *=-f; m1.x *=-f; a0.x = (((float)(tspr->y-globalposy))/ -1024.0 + a0.x)*-f;
	m0.z *= g; m1.z *= g; a0.z = (((float)(k0     -globalposz))/-16384.0 + a0.z)*g;

	k0 = ((float)(tspr->x-globalposx))*f/1024.0;
	k1 = ((float)(tspr->y-globalposy))*f/1024.0;
	f = gcosang2*gshang;
	g = gsinang2*gshang;
	k4 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+1024)&2047] / 16384.0;
	k5 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+ 512)&2047] / 16384.0;
	k2 = k0*(1-k4)+k1*k5;
	k3 = k1*(1-k4)-k0*k5;
	k6 = f*gstang - gsinang*gctang; k7 = g*gstang + gcosang*gctang;
	mat[0] = k4*k6 + k5*k7; mat[4] = gchang*gstang; mat[ 8] = k4*k7 - k5*k6; mat[12] = k2*k6 + k3*k7;
	k6 = f*gctang + gsinang*gstang; k7 = g*gctang - gcosang*gstang;
	mat[1] = k4*k6 + k5*k7; mat[5] = gchang*gctang; mat[ 9] = k4*k7 - k5*k6; mat[13] = k2*k6 + k3*k7;
	k6 =           gcosang2*gchang; k7 =           gsinang2*gchang;
	mat[2] = k4*k6 + k5*k7; mat[6] =-gshang;        mat[10] = k4*k7 - k5*k6; mat[14] = k2*k6 + k3*k7;

	mat[12] += a0.y*mat[0] + a0.z*mat[4] + a0.x*mat[ 8];
	mat[13] += a0.y*mat[1] + a0.z*mat[5] + a0.x*mat[ 9];
	mat[14] += a0.y*mat[2] + a0.z*mat[6] + a0.x*mat[10];

		//Mirrors
	if (grhalfxdown10x < 0) { mat[0] = -mat[0]; mat[4] = -mat[4]; mat[8] = -mat[8]; mat[12] = -mat[12]; }

// ------ Unnecessarily clean (lol) code to generate translation/rotation matrix for MD2 ends ------

#if 0
	for(i=m->numverts-1;i>=0;i--) //interpolate (for animation) & transform to Build coords
	{
		fp.z = ((float)c0[(i<<2)+0])*m0.x + ((float)c1[(i<<2)+0])*m1.x;
		fp.y = ((float)c0[(i<<2)+2])*m0.z + ((float)c1[(i<<2)+2])*m1.z;
		fp.x = ((float)c0[(i<<2)+1])*m0.y + ((float)c1[(i<<2)+1])*m1.y;
		vertlist[i].x = fp.x*mat[0] + fp.y*mat[4] + fp.z*mat[ 8] + mat[12];
		vertlist[i].y = fp.x*mat[1] + fp.y*mat[5] + fp.z*mat[ 9] + mat[13];
		vertlist[i].z = fp.x*mat[2] + fp.y*mat[6] + fp.z*mat[10] + mat[14];
	}
#else
	for(i=m->numverts-1;i>=0;i--) //interpolate (for animation) & transform to Build coords
	{
		vertlist[i].z = ((float)c0[(i<<2)+0])*m0.x + ((float)c1[(i<<2)+0])*m1.x;
		vertlist[i].y = ((float)c0[(i<<2)+2])*m0.z + ((float)c1[(i<<2)+2])*m1.z;
		vertlist[i].x = ((float)c0[(i<<2)+1])*m0.y + ((float)c1[(i<<2)+1])*m1.y;
	}
	bglMatrixMode(GL_MODELVIEW); //Let OpenGL (and perhaps hardware :) handle the matrix rotation
	mat[3] = mat[7] = mat[11] = 0.f; mat[15] = 1.f; bglLoadMatrixf(mat);
#endif

	i = mdloadskin(m,tile2model[tspr->picnum].skinnum,globalpal); if (!i) return;

	//bit 10 is an ugly hack in game.c\animatesprites telling MD2SPRITE
	//to use Z-buffer hacks to hide overdraw problems with the shadows
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LESS); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.9999);
	}
	bglPushAttrib(GL_POLYGON_BIT);
	if ((grhalfxdown10x >= 0) ^ ((globalorientation&8) != 0) ^ ((globalorientation&4) != 0)) bglFrontFace(GL_CW); else bglFrontFace(GL_CCW);
	bglEnable(GL_CULL_FACE);
	bglCullFace(GL_BACK);

	bglEnable(GL_TEXTURE_2D);
	bglBindTexture(GL_TEXTURE_2D, i);

	pc[0] = pc[1] = pc[2] = ((float)(numpalookups-min(max(globalshade+m->shadeoff,0),numpalookups)))/((float)numpalookups);
	pc[0] *= (float)hictinting[globalpal].r / 255.0;
	pc[1] *= (float)hictinting[globalpal].g / 255.0;
	pc[2] *= (float)hictinting[globalpal].b / 255.0;
	if (tspr->cstat&2) { if (!(tspr->cstat&512)) pc[3] = 0.66; else pc[3] = 0.33; } else pc[3] = 1.0;
	if (m->usesalpha) //Sprites with alpha in texture
	{
		bglEnable(GL_BLEND);// bglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		bglEnable(GL_ALPHA_TEST); bglAlphaFunc(GL_GREATER,0.32);
	}
	else
	{
		if (tspr->cstat&2) bglEnable(GL_BLEND); else bglDisable(GL_BLEND);
	}
	bglColor4f(pc[0],pc[1],pc[2],pc[3]);

	for(lptr=m->glcmds;(i=*lptr++);)
	{
		if (i < 0) { bglBegin(GL_TRIANGLE_FAN); i = -i; }
				else { bglBegin(GL_TRIANGLE_STRIP); }
		for(;i>0;i--,lptr+=3)
		{
			bglTexCoord2f(((float *)lptr)[0],((float *)lptr)[1]);
			bglVertex3fv((float *)&vertlist[lptr[2]]);
		}
		bglEnd();
	}

	if (m->usesalpha) bglDisable(GL_ALPHA_TEST);
	bglDisable(GL_CULL_FACE);
	bglPopAttrib();
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LEQUAL); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.99999);
	}
	bglLoadIdentity();
}

//--------------------------------------- MD3 LIBRARY BEGINS ---------------------------------------

static md3model *md3load (int fil)
{
	char *buf, st[BMAX_PATH+2], bst[BMAX_PATH+2];
	long i, j, surfi, ofsurf, bsc, offs[4], leng[4];
	short szmin, szmax;
	md3model *m;
	md3surf_t *s;

	m = (md3model *)calloc(1,sizeof(md3model)); if (!m) return(0);
	m->mdnum = 3; m->texid = 0; m->scale = .01;

	kread(fil,&m->head,sizeof(md3head_t));
	if ((m->head.id != 0x33504449) && (m->head.vers != 15)) { free(m); return(0); } //"IDP3"

	m->numskins = m->head.numskins; //<- dead code?
	m->numframes = m->head.numframes;

	ofsurf = (long)m->head.surfs;

	klseek(fil,(long)m->head.frames,SEEK_SET); i = m->head.numframes*sizeof(md3frame_t);
	m->head.frames = (md3frame_t *)malloc(i); if (!m->head.frames) { free(m); return(0); }
	kread(fil,m->head.frames,i);

	klseek(fil,(long)m->head.tags,SEEK_SET); i = m->head.numtags*sizeof(md3tag_t);
	m->head.tags = (md3tag_t *)malloc(i); if (!m->head.tags) { free(m->head.frames); free(m); return(0); }
	kread(fil,m->head.tags,i);

	klseek(fil,(long)m->head.surfs,SEEK_SET); i = m->head.numsurfs*sizeof(md3surf_t);
	m->head.surfs = (md3surf_t *)malloc(i); if (!m->head.surfs) { free(m->head.tags); free(m->head.frames); free(m); return(0); }

	szmin = 32767; szmax = -32768;
	for(surfi=0;surfi<m->head.numsurfs;surfi++)
	{
		s = &m->head.surfs[surfi];
		klseek(fil,ofsurf,SEEK_SET); kread(fil,s,sizeof(md3surf_t));

		offs[0] = ofsurf+((long)(s->tris   )); leng[0] = s->numtris*sizeof(md3tri_t);
		offs[1] = ofsurf+((long)(s->shaders)); leng[1] = s->numshaders*sizeof(md3shader_t);
		offs[2] = ofsurf+((long)(s->uv     )); leng[2] = s->numverts*sizeof(md3uv_t);
		offs[3] = ofsurf+((long)(s->xyzn   )); leng[3] = s->numframes*s->numverts*sizeof(md3xyzn_t);

		s->tris = (md3tri_t *)malloc(leng[0]+leng[1]+leng[2]+leng[3]);
		if (!s->tris)
		{
			for(surfi--;surfi>=0;surfi--) free(m->head.surfs[surfi].tris);
			free(m->head.tags); free(m->head.frames); free(m); return(0);
		}
		s->shaders = (md3shader_t *)(((long)s->tris   )+leng[0]);
		s->uv      = (md3uv_t     *)(((long)s->shaders)+leng[1]);
		s->xyzn    = (md3xyzn_t   *)(((long)s->uv     )+leng[2]);

		klseek(fil,offs[0],SEEK_SET); kread(fil,s->tris   ,leng[0]);
		klseek(fil,offs[1],SEEK_SET); kread(fil,s->shaders,leng[1]);
		klseek(fil,offs[2],SEEK_SET); kread(fil,s->uv     ,leng[2]);
		klseek(fil,offs[3],SEEK_SET); kread(fil,s->xyzn   ,leng[3]);

			//Find min/max height coord in first frame of all surfs (for accurate display alignment)
		for(i=0;i<s->numverts*s->numframes;i++)
		{
			if (s->xyzn[i].y < szmin) szmin = s->xyzn[i].y;
			if (s->xyzn[i].y > szmax) szmax = s->xyzn[i].y;
		}

		maxmodelverts = max(maxmodelverts, s->numverts);
		ofsurf += s->ofsend;
	}
	m->zmin = ((float)szmin)/64.0;
	m->zmax = ((float)szmax)/64.0;

#if 0
	strcpy(st,filnam);
	for(i=0,j=0;st[i];i++) if ((st[i] == '/') || (st[i] == '\\')) j = i+1;
	st[j] = '*'; st[j+1] = 0;
	kzfindfilestart(st); bsc = -1;
	while (kzfindfile(st))
	{
		if (st[0] == '\\') continue;

		for(i=0,j=0;st[i];i++) if (st[i] == '.') j = i+1;
		if ((!stricmp(&st[j],"JPG")) || (!stricmp(&st[j],"PNG")) || (!stricmp(&st[j],"GIF")) ||
			 (!stricmp(&st[j],"PCX")) || (!stricmp(&st[j],"TGA")) || (!stricmp(&st[j],"BMP")) ||
			 (!stricmp(&st[j],"CEL")))
		{
			for(i=0;st[i];i++) if (st[i] != filnam[i]) break;
			if (i > bsc) { bsc = i; strcpy(bst,st); }
		}
	}
	if (!mdloadskin(&m->texid,&m->usesalpha,bst)) ;//bad!
#endif

	return((void *)m);
}

static void md3draw (md3model *m, spritetype *tspr)
{
	point3d fp, m0, m1, a0, a1;
	md3frame_t *f0, *f1;
	md3xyzn_t *v0, *v1;
	long i, j, k, surfi, *lptr;
	float f, g, k0, k1, k2, k3, k4, k5, k6, k7, mat[16], pc[4];
	md3surf_t *s;

	updateanimation((md2model *)m,tspr);

		//create current&next frame's vertex list from whole list
	f0 = (md3frame_t *)&m->head.frames[m->cframe];
	f1 = (md3frame_t *)&m->head.frames[m->nframe];
	f = m->interpol; g = 1-f;
	m0.x = (1.0/64.0)*m->scale*g; m1.x = (1.0/64.0)*m->scale*f;
	m0.y = (1.0/64.0)*m->scale*g; m1.y = (1.0/64.0)*m->scale*f;
	m0.z = (1.0/64.0)*m->scale*g; m1.z = (1.0/64.0)*m->scale*f;
	a0.x = a0.y = 0; a0.z = m->zadd*m->scale;

	if (globalorientation&8) //y-flipping
	{
		m0.z = -m0.z; m1.z = -m1.z; a0.z = -a0.z;
			//Add height of 1st frame (use same frame to prevent animation bounce)
		f0 = (md3frame_t *)&m->head.frames[0];
		a0.z += (m->zmax-m->zmin)*m->scale;

	}
	if (globalorientation&4) { m0.y = -m0.y; m1.y = -m1.y; a0.y = -a0.y; } //x-flipping

	f = ((float)tspr->xrepeat)/64*m->bscale;
	m0.x *= f; m1.x *= f; a0.x *= f; f = -f;   // 20040610: backwards models aren't cool
	m0.y *= f; m1.y *= f; a0.y *= f;
	f = ((float)tspr->yrepeat)/64*m->bscale;
	m0.z *= f; m1.z *= f; a0.z *= f;

	k0 = tspr->z;
	if (globalorientation&128) k0 += (float)((tilesizy[tspr->picnum]*tspr->yrepeat)<<1);

	f = (65536.0*512.0)/((float)xdimen*viewingrange);
	g = 32.0/((float)xdimen*gxyaspect);
	m0.y *= f; m1.y *= f; a0.y = (((float)(tspr->x-globalposx))/  1024.0 + a0.y)*f;
	m0.x *=-f; m1.x *=-f; a0.x = (((float)(tspr->y-globalposy))/ -1024.0 + a0.x)*-f;
	m0.z *= g; m1.z *= g; a0.z = (((float)(k0     -globalposz))/-16384.0 + a0.z)*g;

	k0 = ((float)(tspr->x-globalposx))*f/1024.0;
	k1 = ((float)(tspr->y-globalposy))*f/1024.0;
	f = gcosang2*gshang;
	g = gsinang2*gshang;
	k4 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+1024)&2047] / 16384.0;
	k5 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+ 512)&2047] / 16384.0;
	k2 = k0*(1-k4)+k1*k5;
	k3 = k1*(1-k4)-k0*k5;
	k6 = f*gstang - gsinang*gctang; k7 = g*gstang + gcosang*gctang;
	mat[0] = k4*k6 + k5*k7; mat[4] = gchang*gstang; mat[ 8] = k4*k7 - k5*k6; mat[12] = k2*k6 + k3*k7;
	k6 = f*gctang + gsinang*gstang; k7 = g*gctang - gcosang*gstang;
	mat[1] = k4*k6 + k5*k7; mat[5] = gchang*gctang; mat[ 9] = k4*k7 - k5*k6; mat[13] = k2*k6 + k3*k7;
	k6 =           gcosang2*gchang; k7 =           gsinang2*gchang;
	mat[2] = k4*k6 + k5*k7; mat[6] =-gshang;        mat[10] = k4*k7 - k5*k6; mat[14] = k2*k6 + k3*k7;

	mat[12] += a0.y*mat[0] + a0.z*mat[4] + a0.x*mat[ 8];
	mat[13] += a0.y*mat[1] + a0.z*mat[5] + a0.x*mat[ 9];
	mat[14] += a0.y*mat[2] + a0.z*mat[6] + a0.x*mat[10];

		//Mirrors
	if (grhalfxdown10x < 0) { mat[0] = -mat[0]; mat[4] = -mat[4]; mat[8] = -mat[8]; mat[12] = -mat[12]; }


//------------
	//bit 10 is an ugly hack in game.c\animatesprites telling MD2SPRITE
	//to use Z-buffer hacks to hide overdraw problems with the shadows
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LESS); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.9999);
	}
	bglPushAttrib(GL_POLYGON_BIT);
	if ((grhalfxdown10x >= 0) ^ ((globalorientation&8) != 0) ^ ((globalorientation&4) != 0)) bglFrontFace(GL_CW); else bglFrontFace(GL_CCW);
	bglEnable(GL_CULL_FACE);
	bglCullFace(GL_BACK);

	bglEnable(GL_TEXTURE_2D);

	pc[0] = pc[1] = pc[2] = ((float)(numpalookups-min(max(globalshade+m->shadeoff,0),numpalookups)))/((float)numpalookups);
	pc[0] *= (float)hictinting[globalpal].r / 255.0;
	pc[1] *= (float)hictinting[globalpal].g / 255.0;
	pc[2] *= (float)hictinting[globalpal].b / 255.0;
	if (tspr->cstat&2) { if (!(tspr->cstat&512)) pc[3] = 0.66; else pc[3] = 0.33; } else pc[3] = 1.0;
	if (m->usesalpha) //Sprites with alpha in texture
	{
		bglEnable(GL_BLEND);// bglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		bglEnable(GL_ALPHA_TEST); bglAlphaFunc(GL_GREATER,0.32);
	}
	else
	{
		if (tspr->cstat&2) bglEnable(GL_BLEND); else bglDisable(GL_BLEND);
	}
	bglColor4f(pc[0],pc[1],pc[2],pc[3]);
//------------

	for(surfi=0;surfi<m->head.numsurfs;surfi++)
	{
		s = &m->head.surfs[surfi];
		v0 = &s->xyzn[m->cframe*s->numverts];
		v1 = &s->xyzn[m->nframe*s->numverts];
#if 0
		for(i=s->numverts-1;i>=0;i--) //interpolate (for animation) & transform to Build coords
		{
			fp.z = ((float)v0[i].x)*m0.x + ((float)v1[i].x)*m1.x;
			fp.y = ((float)v0[i].z)*m0.z + ((float)v1[i].z)*m1.z;
			fp.x = ((float)v0[i].y)*m0.y + ((float)v1[i].y)*m1.y;
			vertlist[i].x = fp.x*mat[0] + fp.y*mat[4] + fp.z*mat[ 8] + mat[12];
			vertlist[i].y = fp.x*mat[1] + fp.y*mat[5] + fp.z*mat[ 9] + mat[13];
			vertlist[i].z = fp.x*mat[2] + fp.y*mat[6] + fp.z*mat[10] + mat[14];
		}
#else
		for(i=s->numverts-1;i>=0;i--) //interpolate (for animation) & transform to Build coords
		{
			vertlist[i].z = ((float)v0[i].x)*m0.x + ((float)v1[i].x)*m1.x;
			vertlist[i].y = ((float)v0[i].z)*m0.z + ((float)v1[i].z)*m1.z;
			vertlist[i].x = ((float)v0[i].y)*m0.y + ((float)v1[i].y)*m1.y;
		}
		bglMatrixMode(GL_MODELVIEW); //Let OpenGL (and perhaps hardware :) handle the matrix rotation
		mat[3] = mat[7] = mat[11] = 0.f; mat[15] = 1.f; bglLoadMatrixf(mat);
#endif


#if 0
		//precalc:
	float sinlut256[256+(256>>2)];
	for(i=0;i<sizeof(sinlut256)/sizeof(sinlut256[0]);i++) sinlut256[i] = sin(((float)i)*(PI*2/255.0));

		//normal to xyz:
	md3vert_t *mv = &md3vert[?];
	z = sinlut256[mv->nlng+(256>>2)];
	x = sinlut256[mv->nlat]*z;
	y = sinlut256[mv->nlat+(256>>2)]*z;
	z = sinlut256[mv->nlng];
#endif

		i = mdloadskin((md2model *)m,tile2model[tspr->picnum].skinnum,globalpal); if (!i) continue;
		//i = mdloadskin((md2model *)m,tile2model[tspr->picnum].skinnum,surfi); //hack for testing multiple surfaces per MD3
		bglBindTexture(GL_TEXTURE_2D, i);

		bglBegin(GL_TRIANGLES);
		for(i=s->numtris-1;i>=0;i--)
			for(j=0;j<3;j++)
			{
				k = s->tris[i].i[j];
				bglTexCoord2f(s->uv[k].u,s->uv[k].v);
				bglVertex3fv((float *)&vertlist[k]);
			}
		bglEnd();
	}

//------------
	if (m->usesalpha) bglDisable(GL_ALPHA_TEST);
	bglDisable(GL_CULL_FACE);
	bglPopAttrib();
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LEQUAL); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.99999);
	}
	bglLoadIdentity();
}

static void md3free (md3model *m)
{
	mdanim_t *anim, *nanim = NULL;
	mdskinmap_t *sk, *nsk = NULL;
	md3surf_t *s;
	long surfi;

	if (!m) return;

	for(anim=m->animations; anim; anim=nanim)
	{
		nanim = anim->next;
		free(anim);
	}
	for(sk=m->skinmap; sk; sk=nsk)
	{
		nsk = sk->next;
		free(sk->fn);
		free(sk);
	}

	if (m->head.surfs)
	{
		for(surfi=m->head.numsurfs-1;surfi>=0;surfi--)
		{
			s = &m->head.surfs[surfi];
			if (s->tris) free(s->tris);
		}
		free(m->head.surfs);
	}
	if (m->head.tags) free(m->head.tags);
	if (m->head.frames) free(m->head.frames);

	if (m->texid) free(m->texid);

	free(m);
}

//---------------------------------------- MD3 LIBRARY ENDS ----------------------------------------

void *mdload (const char *filnam)
{
	void *vm;
	int fil;
	long i;

	fil = kopen4load((char*)filnam,0); if (fil<0) return(0);
	kread(fil,&i,4); klseek(fil,0,SEEK_SET);
	switch(i)
	{
		case 0x32504449: vm = md2load(fil,filnam); break; //IDP2
		case 0x33504449: vm = md3load(fil); break; //IDP3
		default: vm = 0; break;
	}
	kclose(fil);
	return(vm);
}

void mddraw (spritetype *tspr)
{
	mdanim_t *anim;
	void *vm;

	if (maxmodelverts > allocmodelverts)
	{
		point3d *vl = (point3d *)realloc(vertlist,sizeof(point3d)*maxmodelverts);
		if (!vl) { OSD_Printf("ERROR: Not enough memory to allocate %d vertices!\n",maxmodelverts); return; }
		vertlist = vl; allocmodelverts = maxmodelverts;
	}

	vm = models[tile2model[tspr->picnum].modelid];
	if (*(long *)vm == 2) { md2draw((md2model *)vm,tspr); return; }
	if (*(long *)vm == 3) { md3draw((md3model *)vm,tspr); return; }
}

void mdfree (void *vm)
{
	if (*(long *)vm == 2) { md2free((md2model *)vm); return; }
	if (*(long *)vm == 3) { md3free((md3model *)vm); return; }
}

//-------------------------------------- MD2/MD3 LIBRARY ENDS --------------------------------------
