//-------------------------------------  MD2 LIBRARY BEGINS ----------------------------------------
	//This MD2 code is based on the source code from David Henry (tfc_duke(at)hotmail.com)
	//   http://tfc.duke.free.fr/us/tutorials/models/md2.htm
	//He probably wouldn't recognize it if he looked at it though :)
typedef struct { float x, y, z; } point3d;

typedef struct
{  long id, vers, skinxsiz, skinysiz, framebytes; //id:"IPD2", vers:8
	long numskins, numverts, numuv, numtris, numglcmds, numframes;
	long ofsskins, ofsuv, ofstris, ofsframes, ofsglcmds, ofseof; //ofsskins: skin names (64 bytes each)
} md2typ;

typedef struct { unsigned char v[3], lightnormalindex; } verttyp; //compressed vertex coords (x,y,z)
typedef struct
{  point3d mul, add; //scale&translation vector
	char name[16];    //frame name
	verttyp verts[1]; //first vertex of this frame
} frametyp;

typedef struct _md2animation
{
	int startframe, endframe;
	int fpssc, flags;
	struct _md2animation *next;
} md2animation;
#define MD2ANIM_LOOP 0
#define MD2ANIM_ONESHOT 1

typedef struct _md2skinmap
{
	unsigned char palette;	// Build palette number
	int skinnum;	// Skin identifier
	char *fn;	// Skin filename
	unsigned int texid[HICEFFECTMASK+1];	// OpenGL texture numbers for effect variations
	struct _md2skinmap *next;
} md2skinmap;

typedef struct
{
	long numskins, numframes, numverts, numglcmds, framebytes, *glcmds;
	long frame0, frame1, cframe, nframe, fpssc, usesalpha, shadeoff;
	long skinloaded;	// set to 1+numofskin when a skin is loaded and the tex coords are modified,
				// so we don't do it twice
	float oldtime, curtime, interpol, scale, bscale;
	unsigned int *texid;   // texture ids for base skin if no mappings defined
	char *frames;
	char *basepath;   // pointer to string of base path
	char *skinfn;   // pointer to first of numskins 64-char strings
	md2animation *animations;
	md2skinmap *skinmap;
} md2model;

typedef struct
{ // maps build tiles to particular animation frames of a model
	int modelid;
	int skinnum;
	int framenum;   // calculate the number from the name when declaring
} md2tilemap;
static md2tilemap tiletomodel[MAXTILES];

static char md2inited=0;

#define MODELALLOCGROUP 10
static int nummodelsalloced = 0, nextmodelid = 0;
static md2model *models = NULL;

static long maxmodelverts = 0, allocmodelverts = 0;
static point3d *vertlist = NULL;


static void freeallmodels(void)
{
	if (models) {
		int i;
		md2animation *anim, *nanim = NULL;
		md2skinmap *sk, *nsk = NULL;
	
		for (i=0; i<nextmodelid; i++) {
			for(anim=models[i].animations; anim; anim=nanim) {
				nanim = anim->next;
				free(anim);
			}
			for(sk=models[i].skinmap; sk; sk=nsk) {
				nsk = sk->next;
				free(sk->fn);
				free(sk);
			}

			if (models[i].frames) free(models[i].frames);
			if (models[i].glcmds) free(models[i].glcmds);
			if (models[i].basepath) free(models[i].basepath);
			if (models[i].skinfn) free(models[i].skinfn);
			if (models[i].texid) free(models[i].texid);
		}

		free(models);
		models = NULL;
		nummodelsalloced = 0;
		nextmodelid = 0;
	}

	memset(tiletomodel, -1, sizeof(tiletomodel));

	if (vertlist) {
		free(vertlist);
		vertlist = NULL;
		allocmodelverts = maxmodelverts = 0;
	}
}

static void clearskins(void)
{
	int i,j;
	md2skinmap *sk;

	for (i=0;i<nextmodelid;i++) {
		for (j=0;j<models[i].numskins*(HICEFFECTMASK+1);j++) {
			if (models[i].texid[j]) bglDeleteTextures(1,&models[i].texid[j]);
			models[i].texid[j] = 0;
		}

		for (sk = models[i].skinmap; sk; sk = sk->next) {
			for (j=0;j<(HICEFFECTMASK+1);j++) {
				if (sk->texid[j]) bglDeleteTextures(1,&sk->texid[j]);
				sk->texid[j] = 0;
			}
		}
	}
}

static void md2init(void)
{
	freeallmodels();
	md2inited = 1;
}

static long md2load (md2model *m, const char *filename);
int md2_loadmodel(const char *fn, float scale, int shadeoff)
{
	md2model *m;

	if (!md2inited) md2init();

	if (!models) {
		models = (md2model*)malloc(sizeof(md2model) * MODELALLOCGROUP);
		if (!models) return -1;

		nummodelsalloced = MODELALLOCGROUP;
	} else {
		if (nummodelsalloced <= nextmodelid) {
			m = (md2model*)realloc(models, sizeof(md2model) * (nummodelsalloced + MODELALLOCGROUP));
			if (!m) return -1;

			nummodelsalloced += MODELALLOCGROUP;
			models = m;
		}
	}

	m = &models[nextmodelid];
	memset(m, 0, sizeof(md2model));
	m->bscale = scale;
	m->shadeoff = shadeoff;

	if (md2load(m, fn)) {
		nextmodelid++;
		return nextmodelid-1;
	}

	return -1;
}

int md2_tilehasmodel(int tilenume)
{
	return (tiletomodel[tilenume].modelid != -1);
}

int md2_defineframe(int modelid, const char *framename, int tilenume, int skinnum)
{
	md2model *m;
	frametyp *fr;
	int i;

	if (!md2inited) md2init();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;
	if ((unsigned long)tilenume >= (unsigned long)MAXTILES) return -2;

	m = &models[modelid];
	for (i=0; i<m->numframes; i++) {
		fr = (frametyp*)&m->frames[i*m->framebytes];
		if (!Bstrcmp(fr->name, framename)) break;
	}
	if (i == m->numframes) return -3;   // frame name invalid

	tiletomodel[tilenume].modelid = modelid;
	tiletomodel[tilenume].framenum = i;
	tiletomodel[tilenume].skinnum = skinnum;

	return 0;
}

int md2_defineanimation(int modelid, const char *framestart, const char *frameend, int fpssc, int flags)
{
	md2model *m;
	md2animation ma, *map;
	frametyp *fr;
	int i;

	if (!md2inited) md2init();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;

	memset(&ma, 0, sizeof(ma));
	m = &models[modelid];

	// find the index of the start frame
	for (i=0; i<m->numframes; i++) {
		fr = (frametyp*)&m->frames[i*m->framebytes];
		if (!Bstrcmp(fr->name, framestart)) break;
	}
	if (i == m->numframes) return -2;
	ma.startframe = i;

	// find the index of the finish frame which must trail the start frame
	for (; i<m->numframes; i++) {
		fr = (frametyp*)&m->frames[i*m->framebytes];
		if (!Bstrcmp(fr->name, frameend)) break;
	}
	if (i == m->numframes) return -3;
	ma.endframe = i;

	ma.fpssc = fpssc;
	ma.flags = flags;

	map = (md2animation*)calloc(1,sizeof(md2animation));
	if (!map) return -4;
	memcpy(map, &ma, sizeof(ma));

	map->next = m->animations;
	m->animations = map;

	return 0;
}

int md2_defineskin(int modelid, const char *skinfn, int palnum, int skinnum)
{
	md2skinmap *sk, *skl;

	if (!md2inited) md2init();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;
	if (!skinfn) return -2;
	if ((unsigned)palnum >= (unsigned)MAXPALOOKUPS) return -3;

	skl = NULL;
	for (sk = models[modelid].skinmap; sk; skl = sk, sk = sk->next)
		if (sk->palette == (unsigned char)palnum && skinnum == sk->skinnum) break;
	if (!sk) {
		sk = (md2skinmap *)calloc(1,sizeof(md2skinmap));
		if (!sk) return -4;

		if (!skl) models[modelid].skinmap = sk;
		else skl->next = sk;
	}

	sk->palette = (unsigned char)palnum;
	sk->skinnum = skinnum;
	sk->fn = (char *)malloc(strlen(skinfn)+1);
	if (!sk->fn) return -4;
	strcpy(sk->fn, skinfn);

	return 0;
}

static int daskinloader(const char *fn, long *fptr, long *bpl, long *sizx, long *sizy, long *osizx, long *osizy, char *hasalpha, char effect)
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


static long md2loadskin (md2model *m, int number, int pal)
{
	long fptr, bpl, xsiz, ysiz, osizx, osizy, texfmt = GL_RGBA, intexfmt = GL_RGBA;
	char *skinfile, hasalpha, fn[MAX_PATH+65];
	unsigned int *texidx;
	md2skinmap *sk, *skzero = NULL;

	if ((unsigned)pal >= (unsigned)MAXPALOOKUPS) return 0;
	for (sk = m->skinmap; sk; sk = sk->next) {
		if ((int)sk->palette == pal && sk->skinnum == number) {
			skinfile = sk->fn;
			texidx = &sk->texid[ hictinting[pal].f ];
			strcpy(fn,skinfile);
			//OSD_Printf("Using def skin (%d,%d) %s\n",pal,number,skinfile);
			break;
		} else if (sk->palette == 0 && sk->skinnum == 0) {
			if (!skzero) skzero = sk;
		} else if ((int)sk->palette == pal && sk->skinnum == 0) {
			skzero = sk;
		}
	}
	if (!sk) {
		if (skzero) {
			skinfile = skzero->fn;
			texidx = &skzero->texid[ hictinting[pal].f ];
			strcpy(fn,skinfile);
			//OSD_Printf("Using def skin 0,0 as fallback, pal=%d\n", pal);
		} else {
			if ((unsigned)number >= (unsigned)m->numskins) number = 0;
			skinfile = m->skinfn + number*64;
			texidx = &m->texid[ number * (HICEFFECTMASK+1) + hictinting[pal].f ];
			strcpy(fn,m->basepath); strcat(fn,skinfile);
			//OSD_Printf("Using md2 skin (%d) %s, pal=%d\n",number,skinfile,pal);
		}
	}
	if (!skinfile[0]) return 0;

	if (*texidx) return *texidx;
	*texidx = 0;

	if (daskinloader(fn,&fptr,&bpl,&xsiz,&ysiz,&osizx,&osizy,&hasalpha,hictinting[pal].f)) {
		initprintf("Failed loading skin file \"%s\"\n", fn);
		skinfile[0] = 0;
		return(0);
	}
	m->usesalpha = hasalpha;
	if (!m->skinloaded) {
		if (xsiz != osizx || ysiz != osizy) {
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

static long md2load (md2model *m, const char *filename)
{
	int fil;
	md2typ head;
	char *buf, st[MAX_PATH];
	long i;

	m->scale = .01;

	fil = kopen4load((char*)filename,0); if (fil<0) return(0);
	kread(fil,(char *)&head,sizeof(md2typ));
	if ((head.id != 0x32504449) || (head.vers != 8)) { kclose(fil); return(0); } //"IDP2"

	m->numskins = head.numskins;
	m->numframes = head.numframes;
	m->numverts = head.numverts;
	m->numglcmds = head.numglcmds;
	m->framebytes = head.framebytes;
	m->frames = (char *)calloc(m->numframes,head.framebytes);
	m->glcmds = (long *)calloc(m->numglcmds,sizeof(long));
	klseek(fil,head.ofsframes,SEEK_SET);
	if (kread(fil,(char *)m->frames,m->numframes*head.framebytes) != m->numframes*head.framebytes)
		{ kclose(fil); return 0; }
	klseek(fil,head.ofsglcmds,SEEK_SET);
	if (kread(fil,(char *)m->glcmds,m->numglcmds*sizeof(long)) != (long)(m->numglcmds*sizeof(long)))
		{ kclose(fil); return 0; }

	strcpy(st,filename);
	for(i=strlen(st)-1;i>0;i--)
		if ((st[i] == '/') || (st[i] == '\\')) { i++; break; }
	if (i<0) i=0;
	st[i] = 0;
	m->basepath = (char *)malloc(i+1);
	strcpy(m->basepath, st);

	m->skinfn = (char *)calloc(head.numskins, 64);
	klseek(fil,head.ofsskins,SEEK_SET);
	if (kread(fil,m->skinfn,64*head.numskins) != 64*head.numskins)
		{ kclose(fil); return 0; }
	kclose(fil);

	m->texid = (unsigned int *)calloc(head.numskins, sizeof(unsigned int) * (HICEFFECTMASK+1));
	
	maxmodelverts = max(maxmodelverts, head.numverts);

	return(1);
}

static void md2draw (float time, spritetype *tspr)
{
	point3d fp, m0, m1, a0, a1;
	frametyp *f0, *f1;
	unsigned char *c0, *c1;
	long i, *lptr;
	float f, g, k0, k1, k2, k3, k4, k5, k6, k7, mat[16], pc[4];
	md2model *m;
	md2animation *anim;

	if (maxmodelverts > allocmodelverts) {
		point3d *xvl;
		xvl = (point3d*)realloc(vertlist, sizeof(point3d)*maxmodelverts);
		if (!xvl) {
			OSD_Printf("ERROR: Not enough memory to allocate %d vertices!\n", maxmodelverts);
			return;
		}
		vertlist = xvl;
		allocmodelverts = maxmodelverts;
	}

	m = &models[ tiletomodel[tspr->picnum].modelid ];

	m->cframe = m->nframe = tiletomodel[tspr->picnum].framenum;

	for (anim = m->animations;
		  anim && anim->startframe != m->cframe;
		  anim = anim->next) ;
	if (anim)
	{
		if (((long)spriteext[tspr->owner].md2animcur) != anim->startframe ||
				(spriteext[tspr->owner].flags & SPREXT_NOMD2ANIM))
		{
			spriteext[tspr->owner].md2animcur = (short)anim->startframe;
			spriteext[tspr->owner].md2animtims = md2tims;
			m->cframe = m->nframe = anim->startframe;
			m->interpol = 0;
		}
		else
		{
			long j;
			i = (md2tims-spriteext[tspr->owner].md2animtims)*anim->fpssc;
			j = ((anim->endframe+1-anim->startframe)<<16);

				//Just in case you play the game for a VERY long time...
			if (i < 0) { i = 0; spriteext[tspr->owner].md2animtims = md2tims; }
				//compare with j*2 instead of j to ensure i stays > j-65536 for MD2ANIM_ONESHOT
			if ((i >= j+j) && (anim->fpssc)) //Keep md2animtims close to md2tims to avoid the use of MOD
				spriteext[tspr->owner].md2animtims += j/anim->fpssc;

			if (anim->flags&MD2ANIM_ONESHOT)
				{ if (i > j-65536) i = j-65536; }
			else { if (i >= j) { i -= j; if (i >= j) i %= j; } }

			m->cframe = (i>>16)+anim->startframe;
			m->nframe = m->cframe+1; if (m->nframe > anim->endframe) m->nframe = anim->startframe;
			m->interpol = ((float)(i&65535))/65536.f;
		}
	}
	else
	{
		m->interpol = 0;
	}

// -------- Unnecessarily clean (lol) code to generate translation/rotation matrix for MD2 ---------

		//create current&next frame's vertex list from whole list
	f0 = (frametyp *)&m->frames[m->cframe*m->framebytes];
	f1 = (frametyp *)&m->frames[m->nframe*m->framebytes];
	f = m->interpol; g = 1-f;
	m0.x = f0->mul.x*m->scale*g; m1.x = f1->mul.x*m->scale*f;
	m0.y = f0->mul.y*m->scale*g; m1.y = f1->mul.y*m->scale*f;
	m0.z = f0->mul.z*m->scale*g; m1.z = f1->mul.z*m->scale*f;
	a0.x = f0->add.x*m->scale; a0.x = (f1->add.x*m->scale-a0.x)*f+a0.x;
	a0.y = f0->add.y*m->scale; a0.y = (f1->add.y*m->scale-a0.y)*f+a0.y;
	a0.z = f0->add.z*m->scale; a0.z = (f1->add.z*m->scale-a0.z)*f+a0.z;
	c0 = &f0->verts[0].v[0]; c1 = &f1->verts[0].v[0];

	if (globalorientation&8) //y-flipping
	{
		m0.z = -m0.z; m1.z = -m1.z; a0.z = -a0.z;
			//Add height of 1st frame (use same frame to prevent animation bounce)
		a0.z += ((frametyp *)m->frames)->mul.z*m->scale*255;
	}
	if (globalorientation&4) { m0.y = -m0.y; m1.y = -m1.y; a0.y = -a0.y; } //x-flipping

	f = ((float)tspr->xrepeat)/64*m->bscale;
	m0.x *= f; m1.x *= f; a0.x *= f; f = -f;	// 20040610: backwards models aren't cool
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

	i = md2loadskin(m,tiletomodel[tspr->picnum].skinnum,globalpal); if (!i) return;

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
		bglEnable(GL_ALPHA_TEST); bglAlphaFunc(GL_GREATER,0.5);
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
#if 1
	bglLoadIdentity();
#endif
}

//--------------------------------------  MD2 LIBRARY ENDS -----------------------------------------
