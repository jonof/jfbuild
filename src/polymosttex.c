#ifdef USE_OPENGL

#include "compat.h"
#include "baselayer.h"
#include "build.h"
#include "glbuild.h"
#include "kplib.h"
#include "cache1d.h"
#include "pragmas.h"
#include "md4.h"
#include "engine_priv.h"
#include "polymost_priv.h"
#include "hightile_priv.h"
#include "polymosttex_priv.h"
#include "polymosttexcache.h"

/** a texture hash entry */
struct PTHash_typ {
	struct PTHash_typ *next;
	PTHead head;
	struct PTHash_typ *deferto;	// if pt_findpthash can't find an exact match for a set of
				// parameters, it creates a header and defers it to another
				// entry that stands in its place
	int primecnt;	// a count of how many times the texture is touched when priming
};
typedef struct PTHash_typ PTHash;

/** a texture manager hash entry */
struct PTMHash_typ {
	struct PTMHash_typ *next;
	PTMHead head;
	unsigned char id[16];
};
typedef struct PTMHash_typ PTMHash;

/** an iterator for walking the hash */
struct PTIter_typ {
	int i;
	PTHash *pth;
	
	// criteria for doing selective matching
	int match;
	long picnum;
	long palnum;
	unsigned short flagsmask;
	unsigned short flags;
};

/** a convenient structure for passing around texture data that is being baked */
struct PTTexture_typ {
	coltype * pic;
	GLsizei sizx, sizy;	// padded size
	GLsizei tsizx, tsizy;	// true size
	GLenum rawfmt;		// raw format of the data (GL_RGBA, GL_BGRA)
	int hasalpha;
};
typedef struct PTTexture_typ PTTexture;

static int primecnt   = 0;	// expected number of textures to load during priming
static int primedone  = 0;	// running total of how many textures have been primed
static int primepos   = 0;	// the position in pthashhead where we are up to in priming

int polymosttexverbosity = 1;	// 0 = none, 1 = errors, 2 = all
int polymosttexfullbright = 256;	// first index of the fullbright palette entries

#define PTHASHHEADSIZ 8192
static PTHash * pthashhead[PTHASHHEADSIZ];	// will be initialised 0 by .bss segment

#define PTMHASHHEADSIZ 4096
static PTMHash * ptmhashhead[PTMHASHHEADSIZ];	// will be initialised 0 by .bss segment

// from polymosttex-squish.cc
int squish_GetStorageRequirements(int width, int height, int format);
int squish_CompressImage(coltype * rgba, int width, int height, unsigned char * output, int format);

static inline int pt_gethashhead(const long picnum)
{
	return picnum & (PTHASHHEADSIZ-1);
}

static inline int ptm_gethashhead(const unsigned char id[16])
{
	int bytes = (int)id[0] | ((int)id[1] << 8);
	return bytes & (PTMHASHHEADSIZ-1);
}

static void detect_texture_size()
{
	if (gltexmaxsize <= 0) {
		GLint siz = 0;
		bglGetIntegerv(GL_MAX_TEXTURE_SIZE, &siz);
		if (siz == 0) {
			gltexmaxsize = 6;   // 2^6 = 64 == default GL max texture size
		} else {
			gltexmaxsize = 0;
			for (; siz > 1; siz >>= 1) {
				gltexmaxsize++;
			}
		}
	}
}


/**
 * Returns a PTMHead pointer for the given texture id
 * @param id the texture id
 * @return the PTMHead item, or null if it couldn't be created
 */
static PTMHead * ptm_gethead(const unsigned char id[16])
{
	PTMHash * ptmh;
	int i;
	
	i = ptm_gethashhead(id);
	ptmh = ptmhashhead[i];
	
	while (ptmh) {
		if (memcmp(id, ptmh->id, 16) == 0) {
			return &ptmh->head;
		}
	}
	
	ptmh = (PTMHash *) malloc(sizeof(PTMHash));
	if (ptmh) {
		memset(ptmh, 0, sizeof(PTMHash));
		
		memcpy(ptmh->id, id, 16);
		ptmh->next = ptmhashhead[i];
		ptmhashhead[i] = ptmh;
	}
	
	return ptmh;
}


/**
 * Calculates a texture id from the information in a PTHead structure
 * @param pth the PTHead structure
 * @param extra for ART this is the layer number, otherwise 0
 * @param id the 16 bytes into which the id will be written
 */
static void ptm_calculateid(const PTHead * pth, int extra, unsigned char id[16])
{
	struct {
		int type;
		int flags;
		int palnum;
		int picnum;
		int extra;
	} artid;
	struct {
		int type;
		int flags;
		int effects;
		char filename[BMAX_PATH];
		int extra;
	} hightileid;
	
	unsigned char * md4ptr = 0;
	int md4len = 0;
	
	if (pth->flags & PTH_HIGHTILE) {
		if (!pth->repldef) {
			if (polymosttexverbosity >= 1) {
				initprintf("PolymostTex: cannot calculate texture id for pth with no repldef\n");
			}
			memset(id, 0, 16);
			return;
		}
		
		memset(&hightileid, 0, sizeof(hightileid));
		hightileid.type = 1;
		hightileid.flags = pth->flags & (PTH_CLAMPED);
		hightileid.effects = 0;//FIXME!
		strncpy(hightileid.filename, pth->repldef->filename, BMAX_PATH);
		hightileid.extra = extra;
		
		md4ptr = (unsigned char *) &hightileid;
		md4len = sizeof(hightileid);

	} else {
		memset(&artid, 0, sizeof(artid));
		artid.type = 0;
		artid.flags = pth->flags & (PTH_CLAMPED);
		artid.palnum = pth->palnum;
		artid.picnum = pth->picnum;
		artid.extra = extra;
		
		md4ptr = (unsigned char *) &artid;
		md4len = sizeof(artid);
	}
	
	md4once(md4ptr, md4len, id);
}


/**
 * Finds the pthash entry for a tile, possibly creating it if one doesn't exist
 * @param picnum tile number
 * @param palnum palette number
 * @param flags PTH_HIGHTILE = try for hightile, PTH_CLAMPED
 * @param create !0 = create if none found
 * @return the PTHash item, or null if none was found
 */
static PTHash * pt_findhash(long picnum, long palnum, unsigned short flags, int create)
{
	int i = pt_gethashhead(picnum);
	PTHash * pth;
	PTHash * basepth;	// palette 0 in case we find we need it
	
	unsigned short flagmask = flags & (PTH_HIGHTILE | PTH_CLAMPED | PTH_SKYBOX);
	
	// first, try and find an existing match for our parameters
	pth = pthashhead[i];
	while (pth) {
		if (pth->head.picnum == picnum &&
		    pth->head.palnum == palnum &&
		    (pth->head.flags & (PTH_HIGHTILE | PTH_CLAMPED | PTH_SKYBOX)) == flagmask
		   ) {
			while (pth->deferto) {
				pth = pth->deferto;	// find the end of the chain
			}
			return pth;
		}
		
		pth = pth->next;
	}
	
	if (!create) {
		return 0;
	} else {
		// we didn't find one, so we have to create one
		hicreplctyp * replc = 0;
		
		if ((flags & PTH_HIGHTILE)) {
			replc = hicfindsubst(picnum, palnum, (flags & PTH_SKYBOX));
		}

		pth = (PTHash *) malloc(sizeof(PTHash));
		if (!pth) {
			return 0;
		}
		memset(pth, 0, sizeof(PTHash));
		
		pth->next = pthashhead[i];
		pth->head.picnum  = picnum;
		pth->head.palnum  = palnum;
		pth->head.flags   = flagmask;
		pth->head.repldef = replc;
		
		pthashhead[i] = pth;
		
		if (replc && replc->palnum != palnum) {
			// we were given a substitute by hightile, so
			if (hictinting[palnum].f & HICEFFECTMASK) {
				// if effects are defined for the palette we actually want
				// we DO NOT defer immediately to the substitute. instead
				// we apply effects to the replacement and treat it as a
				// distinct texture
				;
			} else {
				// we defer to the substitute
				pth->deferto = pt_findhash(picnum, replc->palnum, flags, create);
				while (pth->deferto) {
					pth = pth->deferto;	// find the end of the chain
				}
			}
		} else if ((flags & PTH_HIGHTILE) && !replc) {
			// there is no replacement, so defer to ART
			if (flags & PTH_SKYBOX) {
				return 0;
			} else {
				pth->deferto = pt_findhash(picnum, palnum, (flags & ~PTH_HIGHTILE), create);
				while (pth->deferto) {
					pth = pth->deferto;	// find the end of the chain
				}
			}
		}
	}
	
	return pth;
}

/**
 * Unloads a texture from memory
 * @param pth pointer to the pthash of the loaded texture
 */
static void pt_unload(PTHash * pth)
{
	int i;
	for (i = PTHPIC_SIZE - 1; i>=0; i--) {
		if (pth->head.pic[i]) {
			bglDeleteTextures(1, &pth->head.pic[i]->glpic);
			pth->head.pic[i]->glpic = 0;
		}
	}
}

static int pt_load_art(PTHead * pth);
static int pt_load_hightile(PTHead * pth);
static int pt_load_cache(PTHead * pth);
static void pt_load_fixtransparency(PTTexture * tex, int clamped);
static void pt_load_applyeffects(PTTexture * tex, int effects, int* hassalpha);
static void pt_load_mipscale(PTTexture * tex);
static void pt_load_uploadtexture(PTMHead * ptm, unsigned short flags, PTTexture * tex, PTCacheTile * tdef);
static void pt_load_applyparameters(PTHead * pth);

/**
 * Loads a texture into memory from disk
 * @param pth pointer to the pthash of the texture to load
 * @return !0 on success
 */
static int pt_load(PTHash * pth)
{
	if (pth->head.pic[PTHPIC_BASE] &&
		pth->head.pic[PTHPIC_BASE]->glpic != 0 &&
		(pth->head.flags & PTH_DIRTY) == 0) {
		return 1;	// loaded
	}
	
	if ((pth->head.flags & PTH_HIGHTILE)) {
		// try and load from a cached replacement
		if (pt_load_cache(&pth->head)) {
			return 1;
		}
		
		// if that failed, try and load from a raw replacement
		if (pt_load_hightile(&pth->head)) {
			return 1;
		}
		
		// if that failed, get the hash for the ART version and
		//   defer this to there
		pth->deferto = pt_findhash(
				pth->head.picnum, pth->head.palnum,
				(pth->head.flags & ~PTH_HIGHTILE),
				1);
		if (!pth->deferto) {
			return 0;
		}
		return pt_load(pth->deferto);
	}
	
	if (pt_load_art(&pth->head)) {
		return 1;
	}
	
	// we're SOL
	return 0;
}


/**
 * Load an ART tile into an OpenGL texture
 * @param pth the header to populate
 * @return !0 on success
 */
static int pt_load_art(PTHead * pth)
{
	PTTexture tex, fbtex;
	coltype * wpptr, * fpptr;
	long x, y, x2, y2;
	long dacol;
	int hasalpha = 0, hasfullbright = 0;
	unsigned char id[16];
	
	tex.tsizx = tilesizx[pth->picnum];
	tex.tsizy = tilesizy[pth->picnum];
	if (!glinfo.texnpot) {
		for (tex.sizx = 1; tex.sizx < tex.tsizx; tex.sizx += tex.sizx) ;
		for (tex.sizy = 1; tex.sizy < tex.tsizy; tex.sizy += tex.sizy) ;
	} else {
		if ((tex.tsizx | tex.tsizy) == 0) {
			tex.sizx = tex.sizy = 1;
		} else {
			tex.sizx = tex.tsizx;
			tex.sizy = tex.tsizy;
		}
	}
	
	tex.rawfmt = GL_RGBA;

	memcpy(&fbtex, &tex, sizeof(PTTexture));

	if (!waloff[pth->picnum]) {
		loadtile(pth->picnum);
	}
	
	tex.pic = (coltype *) malloc(tex.sizx * tex.sizy * sizeof(coltype));
	if (!tex.pic) {
		return 0;
	}
	
	// fullbright is initialised transparent
	fbtex.pic = (coltype *) malloc(tex.sizx * tex.sizy * sizeof(coltype));
	if (!fbtex.pic) {
		free(tex.pic);
		return 0;
	}
	memset(fbtex.pic, 0, tex.sizx * tex.sizy * sizeof(coltype));
	
	if (!waloff[pth->picnum]) {
		// Force invalid textures to draw something - an almost purely transparency texture
		// This allows the Z-buffer to be updated for mirrors (which are invalidated textures)
		tex.pic[0].r = tex.pic[0].g = tex.pic[0].b = 0; tex.pic[0].a = 1;
		tex.tsizx = tex.tsizy = 1;
		hasalpha = 1;
	} else {
		for (y = 0; y < tex.sizy; y++) {
			if (y < tex.tsizy) {
				y2 = y;
			} else {
				y2 = y-tex.tsizy;
			}
			wpptr = &tex.pic[y*tex.sizx];
			fpptr = &fbtex.pic[y*tex.sizx];
			for (x = 0; x < tex.sizx; x++, wpptr++, fpptr++) {
				if ((pth->flags & PTH_CLAMPED) && (x >= tex.tsizx || y >= tex.tsizy)) {
					// Clamp texture
					wpptr->r = wpptr->g = wpptr->b = wpptr->a = 0;
					continue;
				}
				if (x < tex.tsizx) {
					x2 = x;
				} else {
					// wrap around to fill the repeated region
					x2 = x-tex.tsizx;
				}
				dacol = (long) (*(unsigned char *)(waloff[pth->picnum]+x2*tex.tsizy+y2));
				if (dacol == 255) {
					wpptr->a = 0;
					hasalpha = 1;
				} else {
					wpptr->a = 255;
					dacol = (long) ((unsigned char)palookup[pth->palnum][dacol]);
				}
				if (gammabrightness) {
					wpptr->r = curpalette[dacol].r;
					wpptr->g = curpalette[dacol].g;
					wpptr->b = curpalette[dacol].b;
				} else {
					wpptr->r = britable[curbrightness][ curpalette[dacol].r ];
					wpptr->g = britable[curbrightness][ curpalette[dacol].g ];
					wpptr->b = britable[curbrightness][ curpalette[dacol].b ];
				}
				
				if (dacol >= polymosttexfullbright && dacol < 255) {
					*fpptr = *wpptr;
					hasfullbright = 1;
				}
			}
		}
	}
	
	pth->scalex = 1.0;
	pth->scaley = 1.0;
	pth->flags &= ~(PTH_DIRTY | PTH_HASALPHA | PTH_SKYBOX);
	pth->flags |= (PTH_NOCOMPRESS | PTH_NOMIPLEVEL);
	tex.hasalpha = hasalpha;
	if (hasalpha) {
		pth->flags |= PTH_HASALPHA;
	}
	
	ptm_calculateid(pth, PTHPIC_BASE, id);
	pth->pic[PTHPIC_BASE] = ptm_gethead(id);
	pth->pic[PTHPIC_BASE]->tsizx = tex.tsizx;
	pth->pic[PTHPIC_BASE]->tsizy = tex.tsizy;
	pth->pic[PTHPIC_BASE]->sizx  = tex.sizx;
	pth->pic[PTHPIC_BASE]->sizy  = tex.sizy;
	pt_load_uploadtexture(pth->pic[PTHPIC_BASE], pth->flags, &tex, 0);
	
	if (hasfullbright) {
		ptm_calculateid(pth, PTHPIC_GLOW, id);
		pth->pic[PTHPIC_GLOW] = ptm_gethead(id);
		pth->pic[PTHPIC_GLOW]->tsizx = tex.tsizx;
		pth->pic[PTHPIC_GLOW]->tsizy = tex.tsizy;
		pth->pic[PTHPIC_GLOW]->sizx  = tex.sizx;
		pth->pic[PTHPIC_GLOW]->sizy  = tex.sizy;
		fbtex.hasalpha = 1;
		pt_load_uploadtexture(pth->pic[PTHPIC_GLOW], pth->flags, &fbtex, 0);
	} else {
		// it might be that after reloading an invalidated texture, the
		// glow map might not be needed anymore, so release it
		pth->pic[PTHPIC_GLOW] = 0;//FIXME should really call a disposal function
	}
	pt_load_applyparameters(pth);
	
	if (tex.pic) {
		free(tex.pic);
	}
	if (fbtex.pic) {
		free(fbtex.pic);
	}
	
	return 1;
}

/**
 * Load a Hightile texture into an OpenGL texture
 * @param pth the header to populate
 * @return !0 on success. Success is defined as all faces of a skybox being loaded,
 *   or at least the base texture of a regular replacement.
 */
static int pt_load_hightile(PTHead * pth)
{
	PTTexture tex;
	const char *filename = 0;
	long filh, picdatalen;
	char * picdata = 0;
	int hasalpha = 0;
	long x, y;
	int texture = 0, loaded[PTHPIC_SIZE] = { 0,0,0,0,0,0, };
	PTCacheTile * tdef = 0;
	unsigned char id[16];

	int cacheable = (!(pth->flags & PTH_NOCOMPRESS) && glinfo.texcompr && glusetexcache && glusetexcompr);
	int writetocache = 0;

	if (!pth->repldef) {
		return 0;
	} else if ((pth->flags & PTH_SKYBOX) && (pth->repldef->skybox == 0 || pth->repldef->skybox->ignore)) {
		return 0;
	} else if (pth->repldef->ignore) {
		return 0;
	}
	
	for (texture = 0; texture < PTHPIC_SIZE; texture++) {
		if (pth->flags & PTH_SKYBOX) {
			if (texture >= 6) {
				texture = PTHPIC_SIZE;
				continue;
			}
			filename = pth->repldef->skybox->face[texture];
		} else {
			switch (texture) {
				case PTHPIC_BASE:
					filename = pth->repldef->filename;
					break;
				default:
					// future developments may use the other indices
					texture = PTHPIC_SIZE;
					continue;
			}
		}
	
		if (!filename) {
			continue;
		}
		
		if (cacheable) {
			// don't write this texture to the cache if it already
			// exists in there. Once kplib can return mtimes, a date
			// test will be done to see if we should supersede the one
			// already there
			writetocache = ! PTCacheHasTile(filename,
					      (pth->palnum != pth->repldef->palnum)
						? hictinting[pth->palnum].f : 0,
					      pth->flags & (PTH_CLAMPED));
		}
	
		filh = kopen4load((char *) filename, 0);
		if (filh < 0) {
			if (polymosttexverbosity >= 1) {
				initprintf("PolymostTex: %s (pic %d pal %d) not found\n",
					filename, pth->picnum, pth->palnum);
			}
			continue;
		}
		picdatalen = kfilelength(filh);
		
		picdata = (char *) malloc(picdatalen);
		if (!picdata) {
			if (polymosttexverbosity >= 1) {
				initprintf("PolymostTex: %s (pic %d pal %d) out of memory\n",
					   filename, pth->picnum, pth->palnum, picdatalen);
			}
			kclose(filh);
			continue;
		}
		
		if (kread(filh, picdata, picdatalen) != picdatalen) {
			if (polymosttexverbosity >= 1) {
				initprintf("PolymostTex: %s (pic %d pal %d) truncated read\n",
					   filename, pth->picnum, pth->palnum);
			}
			kclose(filh);
			continue;
		}
		
		kclose(filh);
		
		kpgetdim(picdata, picdatalen, (long *) &tex.tsizx, (long *) &tex.tsizy);
		if (tex.tsizx == 0 || tex.tsizy == 0) {
			if (polymosttexverbosity >= 1) {
				initprintf("PolymostTex: %s (pic %d pal %d) unrecognised format\n",
					   filename, pth->picnum, pth->palnum);
			}
			free(picdata);
			continue;
		}

		if (!glinfo.texnpot || cacheable) {
			for (tex.sizx = 1; tex.sizx < tex.tsizx; tex.sizx += tex.sizx) ;
			for (tex.sizy = 1; tex.sizy < tex.tsizy; tex.sizy += tex.sizy) ;
		} else {
			tex.sizx = tex.tsizx;
			tex.sizy = tex.tsizy;
		}
		
		tex.pic = (coltype *) malloc(tex.sizx * tex.sizy * sizeof(coltype));
		if (!tex.pic) {
			continue;
		}
		memset(tex.pic, 0, tex.sizx * tex.sizy * sizeof(coltype));
		
		if (kprender(picdata, picdatalen, (long) tex.pic, tex.sizx * sizeof(coltype), tex.sizx, tex.sizy, 0, 0)) {
			if (polymosttexverbosity >= 1) {
				initprintf("PolymostTex: %s (pic %d pal %d) decode error\n",
					   filename, pth->picnum, pth->palnum);
			}
			free(picdata);
			free(tex.pic);
			continue;
		}
		
		pt_load_applyeffects(&tex,
			(pth->palnum != pth->repldef->palnum) ? hictinting[pth->palnum].f : 0,
			&hasalpha);
		tex.hasalpha = hasalpha;

		if (! (pth->flags & PTH_CLAMPED) || (pth->flags & PTH_SKYBOX)) { //Duplicate texture pixels (wrapping tricks for non power of 2 texture sizes)
			if (tex.sizx > tex.tsizx) {	//Copy left to right
				coltype * lptr = tex.pic;
				for (y = 0; y < tex.tsizy; y++, lptr += tex.sizx) {
					memcpy(&lptr[tex.tsizx], lptr, (tex.sizx - tex.tsizx) << 2);
				}
			}
			if (tex.sizy > tex.tsizy) {	//Copy top to bottom
				memcpy(&tex.pic[tex.sizx * tex.tsizy], tex.pic, (tex.sizy - tex.tsizy) * tex.sizx << 2);
			}
		}
		
		tex.rawfmt = GL_BGRA;
		if (!glinfo.bgra || glusetexcompr) {
			// texture compression requires rgba ordering for libsquish
			long j;
			for (j = tex.sizx * tex.sizy - 1; j >= 0; j--) {
				swapchar(&tex.pic[j].r, &tex.pic[j].b);
			}
			tex.rawfmt = GL_RGBA;
		}
		
		free(picdata);
		picdata = 0;

		if (texture == 0) {
			if (pth->flags & PTH_SKYBOX) {
				pth->scalex = (float)tex.tsizx / 64.0;
				pth->scaley = (float)tex.tsizy / 64.0;
			} else {
				pth->scalex = (float)tex.tsizx / (float)tilesizx[pth->picnum];
				pth->scaley = (float)tex.tsizy / (float)tilesizy[pth->picnum];
			}
			pth->flags &= ~(PTH_DIRTY | PTH_NOCOMPRESS | PTH_HASALPHA);
			if (pth->repldef->flags & 1) {
				pth->flags |= PTH_NOCOMPRESS;
			}
			if (hasalpha) {
				pth->flags |= PTH_HASALPHA;
			}
		}
		
		if (cacheable && writetocache) {
			int nmips = 0;
			while (max(1, (tex.sizx >> nmips)) > 1 ||
			       max(1, (tex.sizy >> nmips)) > 1) {
				nmips++;
			}
			nmips++;

			tdef = PTCacheAllocNewTile(nmips);
			tdef->filename = strdup(filename);
			tdef->effects = (pth->palnum != pth->repldef->palnum) ? hictinting[pth->palnum].f : 0;
			tdef->flags = pth->flags & (PTH_CLAMPED | PTH_HASALPHA);
		}
		
		ptm_calculateid(pth, 0, id);
		pth->pic[texture] = ptm_gethead(id);
		pth->pic[texture]->tsizx = tex.tsizx;
		pth->pic[texture]->tsizy = tex.tsizy;
		pth->pic[texture]->sizx  = tex.sizx;
		pth->pic[texture]->sizy  = tex.sizy;
		pt_load_uploadtexture(pth->pic[texture], pth->flags, &tex, tdef);
		
		if (cacheable && writetocache) {
			if (polymosttexverbosity >= 2) {
				initprintf("PolymostTex: writing %s (effects %d, flags %d) to cache\n",
					   tdef->filename, tdef->effects, tdef->flags);
			}
			PTCacheWriteTile(tdef);
			PTCacheFreeTile(tdef);
			tdef = 0;
		}
		
		loaded[texture] = 1;
		
		if (tex.pic) {
			free(tex.pic);
		}
	}

	pt_load_applyparameters(pth);
	
	if (pth->flags & PTH_SKYBOX) {
		int i = 0;
		for (texture = 0; texture < 6; texture++) i += loaded[texture];
		return (i == 6);
	} else {
		return loaded[PTHPIC_BASE];
	}
}

/**
 * Load a Hightile texture from the disk cache into an OpenGL texture
 * @param pth the header to populate
 * @return !0 on success. Success is defined as all faces of a skybox being loaded,
 *   or at least the base texture of a regular replacement.
 */
static int pt_load_cache(PTHead * pth)
{
	const char *filename = 0;
	int mipmap, i;
	int texture = 0, loaded[PTHPIC_SIZE] = { 0,0,0,0,0,0, };
	PTCacheTile * tdef = 0;
	unsigned char id[16];

	if (!pth->repldef) {
		return 0;
	} else if ((pth->repldef->flags & 1) || !glinfo.texcompr || !glusetexcache || !glusetexcompr) {
		return 0;
	} else if ((pth->flags & PTH_SKYBOX) && (pth->repldef->skybox == 0 || pth->repldef->skybox->ignore)) {
		return 0;
	} else if (pth->repldef->ignore) {
		return 0;
	}

	detect_texture_size();
	
	for (texture = 0; texture < PTHPIC_SIZE; texture++) {
		if (pth->flags & PTH_SKYBOX) {
			if (texture >= 6) {
				texture = PTHPIC_SIZE;
				continue;
			}
			filename = pth->repldef->skybox->face[texture];
		} else {
			switch (texture) {
				case PTHPIC_BASE:
					filename = pth->repldef->filename;
					break;
				default:
					// future developments may use the other indices
					texture = PTHPIC_SIZE;
					continue;
			}
		}
	
		if (!filename) {
			continue;
		}
		
		tdef = PTCacheLoadTile(filename, 
			      (pth->palnum != pth->repldef->palnum)
			         ? hictinting[pth->palnum].f : 0,
			      pth->flags & (PTH_CLAMPED));
		
		if (!tdef) {
			continue;
		}
		
		if (polymosttexverbosity >= 2) {
			initprintf("PolymostTex: loaded %s (effects %d, flags %d) from cache\n",
				   tdef->filename, tdef->effects, tdef->flags);
		}
		
		ptm_calculateid(pth, 0, id);
		pth->pic[texture] = ptm_gethead(id);
		if (pth->pic[texture]->glpic == 0) {
			bglGenTextures(1, &pth->pic[texture]->glpic);
		}
		pth->pic[texture]->tsizx = tdef->tsizx;
		pth->pic[texture]->tsizy = tdef->tsizy;
		pth->pic[texture]->sizx  = tdef->mipmap[0].sizx;
		pth->pic[texture]->sizy  = tdef->mipmap[0].sizy;
		bglBindTexture(GL_TEXTURE_2D, pth->pic[texture]->glpic);
		
		if (texture == 0) {
			if (pth->flags & PTH_SKYBOX) {
				pth->scalex = (float)tdef->tsizx / 64.0;
				pth->scaley = (float)tdef->tsizy / 64.0;
			} else {
				pth->scalex = (float)tdef->tsizx / (float)tilesizx[pth->picnum];
				pth->scaley = (float)tdef->tsizy / (float)tilesizy[pth->picnum];
			}
			pth->flags &= ~(PTH_DIRTY | PTH_NOCOMPRESS | PTH_HASALPHA);
			pth->flags |= tdef->flags;
		}
		
		mipmap = 0;
		if (! (pth->flags & PTH_NOMIPLEVEL)) {
			// if we aren't instructed to preserve all mipmap levels,
			// immediately throw away gltexmiplevel mipmaps
			mipmap = max(0, gltexmiplevel);
		}
		while (tdef->mipmap[mipmap].sizx > (1 << gltexmaxsize) ||
		       tdef->mipmap[mipmap].sizy > (1 << gltexmaxsize)) {
			// throw away additional mipmaps until the texture fits within
			// the maximum size permitted by the GL driver
			mipmap++;
		}
		
		for (i = 0; i + mipmap < tdef->nummipmaps; i++) {
			bglCompressedTexImage2DARB(GL_TEXTURE_2D, i,
					tdef->format,
					tdef->mipmap[i + mipmap].sizx,
					tdef->mipmap[i + mipmap].sizy,
					0, tdef->mipmap[i + mipmap].length,
					(const GLvoid *) tdef->mipmap[i + mipmap].data);
		}
		
		PTCacheFreeTile(tdef);
		
		loaded[texture] = 1;
	}

	pt_load_applyparameters(pth);
	
	if (pth->flags & PTH_SKYBOX) {
		int i = 0;
		for (texture = 0; texture < 6; texture++) i += loaded[texture];
		return (i == 6);
	} else {
		return loaded[PTHPIC_BASE];
	}
}

/**
 * Applies a filter to transparent pixels to improve their appearence when bilinearly filtered
 * @param tex the texture to process
 * @param clamped whether the texture is to be used clamped
 */
static void pt_load_fixtransparency(PTTexture * tex, int clamped)
{
	coltype *wpptr;
	long j, x, y, r, g, b;
	long dox, doy, naxsiz2;
	long daxsiz = tex->tsizx, daysiz = tex->tsizy;
	long daxsiz2 = tex->sizx, daysiz2 = tex->sizy;
	
	dox = daxsiz2-1;
	doy = daysiz2-1;
	if (clamped) {
		dox = min(dox,daxsiz);
		doy = min(doy,daysiz);
	} else {
		// Make repeating textures duplicate top/left parts
		daxsiz = daxsiz2;
		daysiz = daysiz2;
	}
	
	daxsiz--; daysiz--; naxsiz2 = -daxsiz2; // Hacks for optimization inside loop
	
	// Set transparent pixels to average color of neighboring opaque pixels
	// Doing this makes bilinear filtering look much better for masked textures (I.E. sprites)
	for (y = doy; y >= 0; y--) {
		wpptr = &tex->pic[y*daxsiz2+dox];
		for (x = dox; x >= 0; x--, wpptr--) {
			if (wpptr->a) {
				continue;
			}
			r = g = b = j = 0;
			if ((x>     0) && (wpptr[     -1].a)) {
				r += (long)wpptr[     -1].r;
				g += (long)wpptr[     -1].g;
				b += (long)wpptr[     -1].b;
				j++;
			}
			if ((x<daxsiz) && (wpptr[     +1].a)) {
				r += (long)wpptr[     +1].r;
				g += (long)wpptr[     +1].g;
				b += (long)wpptr[     +1].b;
				j++;
			}
			if ((y>     0) && (wpptr[naxsiz2].a)) {
				r += (long)wpptr[naxsiz2].r;
				g += (long)wpptr[naxsiz2].g;
				b += (long)wpptr[naxsiz2].b;
				j++;
			}
			if ((y<daysiz) && (wpptr[daxsiz2].a)) {
				r += (long)wpptr[daxsiz2].r;
				g += (long)wpptr[daxsiz2].g;
				b += (long)wpptr[daxsiz2].b;
				j++;
			}
			switch (j) {
				case 1: wpptr->r =   r            ;
					wpptr->g =   g            ;
					wpptr->b =   b            ;
					break;
				case 2: wpptr->r = ((r   +  1)>>1);
					wpptr->g = ((g   +  1)>>1);
					wpptr->b = ((b   +  1)>>1);
					break;
				case 3: wpptr->r = ((r*85+128)>>8);
					wpptr->g = ((g*85+128)>>8);
					wpptr->b = ((b*85+128)>>8);
					break;
				case 4: wpptr->r = ((r   +  2)>>2);
					wpptr->g = ((g   +  2)>>2);
					wpptr->b = ((b   +  2)>>2);
					break;
				default: break;
			}
		}
	}
}

/**
 * Applies brightness (if no gammabrightness is available) and other hightile
 * effects to a texture. As a bonus, it also checks if there is any transparency
 * in the texture.
 * @param tex the texture
 * @param effects the effects
 * @param hasalpha receives whether the texture has transparency
 */
static void pt_load_applyeffects(PTTexture * tex, int effects, int* hasalpha)
{
	int alph = 255;
	int x, y;
	coltype * tptr = tex->pic;
	
	if (effects == 0 && gammabrightness) {
		// use a quicker scan for alpha since we don't need
		// to be swizzling texel components
		for (y = tex->tsizy - 1; y >= 0; y--, tptr += tex->sizx) {
			for (x = tex->tsizx - 1; x >= 0; x--) {
				alph &= tptr[x].a;
			}
		}
	} else {
		unsigned char *brit = &britable[gammabrightness ? 0 : curbrightness][0];
		coltype tcol;
		
		for (y = tex->tsizy - 1; y >= 0; y--, tptr += tex->sizx) {
			for (x = tex->tsizx - 1; x >= 0; x--) {
				tcol.b = brit[tptr[x].b];
				tcol.g = brit[tptr[x].g];
				tcol.r = brit[tptr[x].r];
				tcol.a = tptr[x].a;
				alph &= tptr[x].a;
				
				if (effects & HICEFFECT_GREYSCALE) {
					float y;
					y  = 0.3  * (float)tcol.r;
					y += 0.59 * (float)tcol.g;
					y += 0.11 * (float)tcol.b;
					tcol.b = (unsigned char)max(0.0, min(255.0, y));
					tcol.g = tcol.r = tcol.b;
				}
				if (effects & HICEFFECT_INVERT) {
					tcol.b = 255-tcol.b;
					tcol.g = 255-tcol.g;
					tcol.r = 255-tcol.r;
				}
				
				tptr[x] = tcol;
			}
		}
	}

	*hasalpha = (alph != 255);
}


/**
 * Scales down the texture by half in-place
 * @param tex the texture
 */
static void pt_load_mipscale(PTTexture * tex)
{
	GLsizei newx, newy;
	GLsizei x, y;
	coltype *wpptr, *rpptr;
	long r, g, b, a, k;
	
	newx = max(1, (tex->sizx >> 1));
	newy = max(1, (tex->sizy >> 1));
	
	for (y = 0; y < newy; y++) {
		wpptr = &tex->pic[y * newx];
		rpptr = &tex->pic[(y << 1) * tex->sizx];
		
		for (x = 0; x < newx; x++, wpptr++, rpptr += 2) {
			r = g = b = a = k = 0;
			if (rpptr[0].a) {
				r += (long)rpptr[0].r;
				g += (long)rpptr[0].g;
				b += (long)rpptr[0].b;
				a += (long)rpptr[0].a;
				k++;
			}
			if ((x+x+1 < tex->sizx) && (rpptr[1].a)) {
				r += (long)rpptr[1].r;
				g += (long)rpptr[1].g;
				b += (long)rpptr[1].b;
				a += (long)rpptr[1].a;
				k++;
			}
			if (y+y+1 < tex->sizy) {
				if (rpptr[tex->sizx].a) {
					r += (long)rpptr[tex->sizx  ].r;
					g += (long)rpptr[tex->sizx  ].g;
					b += (long)rpptr[tex->sizx  ].b;
					a += (long)rpptr[tex->sizx  ].a;
					k++;
				}
				if ((x+x+1 < tex->sizx) && (rpptr[tex->sizx+1].a)) {
					r += (long)rpptr[tex->sizx+1].r;
					g += (long)rpptr[tex->sizx+1].g;
					b += (long)rpptr[tex->sizx+1].b;
					a += (long)rpptr[tex->sizx+1].a;
					k++;
				}
			}
			switch(k) {
				case 0:
				case 1: wpptr->r = r;
					wpptr->g = g;
					wpptr->b = b;
					wpptr->a = a;
					break;
				case 2: wpptr->r = ((r+1)>>1);
					wpptr->g = ((g+1)>>1);
					wpptr->b = ((b+1)>>1);
					wpptr->a = ((a+1)>>1);
					break;
				case 3: wpptr->r = ((r*85+128)>>8);
					wpptr->g = ((g*85+128)>>8);
					wpptr->b = ((b*85+128)>>8);
					wpptr->a = ((a*85+128)>>8);
					break;
				case 4: wpptr->r = ((r+2)>>2);
					wpptr->g = ((g+2)>>2);
					wpptr->b = ((b+2)>>2);
					wpptr->a = ((a+2)>>2);
					break;
				default: break;
			}
			//if (wpptr->a) wpptr->a = 255;
		}
	}
	
	tex->sizx = newx;
	tex->sizy = newy;
}


/**
 * Sends texture data to GL
 * @param ptm the texture management header
 * @param flags extra flags to modify how the texture is uploaded
 * @param tex the texture to upload
 * @param tdef the polymosttexcache definition to receive compressed mipmaps, or null
 */
static void pt_load_uploadtexture(PTMHead * ptm, unsigned short flags, PTTexture * tex, PTCacheTile * tdef)
{
	int i;
	GLint mipmap;
	GLint intexfmt;
	int compress = 0;
	unsigned char * comprdata = 0;
	int tdefmip = 0, comprsize = 0;
	int starttime;

	detect_texture_size();
	
	if (!(flags & PTH_NOCOMPRESS) && glinfo.texcompr && glusetexcompr) {
		intexfmt = tex->hasalpha
		         ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
		         : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		compress = 1;
	} else {
		intexfmt = tex->hasalpha
		         ? GL_RGBA
		         : GL_RGB;
	}

	if (compress && tdef) {
		tdef->format = intexfmt;
		tdef->tsizx  = tex->tsizx;
		tdef->tsizy  = tex->tsizy;
	}

	if (ptm->glpic == 0) {
		bglGenTextures(1, &ptm->glpic);
	}
	bglBindTexture(GL_TEXTURE_2D, ptm->glpic);

	pt_load_fixtransparency(tex, (flags & PTH_CLAMPED));
	
	mipmap = 0;
	if (! (flags & PTH_NOMIPLEVEL)) {
		// if we aren't instructed to preserve all mipmap levels,
		// immediately throw away gltexmiplevel mipmaps
		mipmap = max(0, gltexmiplevel);
	}
	while ((tex->sizx >> mipmap) > (1 << gltexmaxsize) ||
	       (tex->sizy >> mipmap) > (1 << gltexmaxsize)) {
		// throw away additional mipmaps until the texture fits within
		// the maximum size permitted by the GL driver
		mipmap++;
	}
	
	for ( ;
	     mipmap > 0 && (tex->sizx > 1 || tex->sizy > 1);
	     mipmap--) {
		if (compress && tdef) {
			comprsize = squish_GetStorageRequirements(tex->sizx, tex->sizy, intexfmt);
			comprdata = (unsigned char *) malloc(comprsize);
			
			starttime = getticks();
			squish_CompressImage(tex->pic, tex->sizx, tex->sizy, comprdata, intexfmt);
			if (polymosttexverbosity >= 2) {
				initprintf("PolymostTex: squish_CompressImage (%dx%d, DXT%d) took %f sec\n",
					   tex->sizx, tex->sizy,
					   (intexfmt == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ? 5 : 1),
					   (float)(getticks() - starttime) / 1000.f);
			}
			
			tdef->mipmap[tdefmip].sizx = tex->sizx;
			tdef->mipmap[tdefmip].sizy = tex->sizy;
			tdef->mipmap[tdefmip].length = comprsize;
			tdef->mipmap[tdefmip].data = comprdata;
			tdefmip++;
			
			comprdata = 0;
		}
		
		pt_load_mipscale(tex);
		pt_load_fixtransparency(tex, (flags & PTH_CLAMPED));
	}
	
	if (compress) {
		comprsize = squish_GetStorageRequirements(tex->sizx, tex->sizy, intexfmt);
		comprdata = (unsigned char *) malloc(comprsize);
		
		starttime = getticks();
		squish_CompressImage(tex->pic, tex->sizx, tex->sizy, comprdata, intexfmt);
		if (polymosttexverbosity >= 2) {
			initprintf("PolymostTex: squish_CompressImage (%dx%d, DXT%d) took %f sec\n",
				   tex->sizx, tex->sizy,
				   (intexfmt == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ? 5 : 1),
				   (float)(getticks() - starttime) / 1000.f);
		}
		
		if (tdef) {
			tdef->mipmap[tdefmip].sizx = tex->sizx;
			tdef->mipmap[tdefmip].sizy = tex->sizy;
			tdef->mipmap[tdefmip].length = comprsize;
			tdef->mipmap[tdefmip].data = comprdata;
			tdefmip++;
		}
		
		bglCompressedTexImage2DARB(GL_TEXTURE_2D, 0,
			intexfmt, tex->sizx, tex->sizy, 0,
			comprsize, (const GLvoid *) comprdata);
		
		if (tdef) {
			// we need to retain each mipmap for the tdef struct, so
			// force each mipmap to be allocated afresh in the loop below
			comprdata = 0;
		}
	} else {
		bglTexImage2D(GL_TEXTURE_2D, 0,
			intexfmt, tex->sizx, tex->sizy, 0, tex->rawfmt,
			GL_UNSIGNED_BYTE, (const GLvoid *) tex->pic);
	}
	
	for (mipmap = 1; tex->sizx > 1 || tex->sizy > 1; mipmap++) {
		pt_load_mipscale(tex);
		pt_load_fixtransparency(tex, (flags & PTH_CLAMPED));

		if (compress) {
			comprsize = squish_GetStorageRequirements(tex->sizx, tex->sizy, intexfmt);
			if (tdef) {
				comprdata = (unsigned char *) malloc(comprsize);
			}

			starttime = getticks();
			squish_CompressImage(tex->pic, tex->sizx, tex->sizy, comprdata, intexfmt);
			if (polymosttexverbosity >= 2) {
				initprintf("PolymostTex: squish_CompressImage (%dx%d, DXT%d) took %f sec\n",
					   tex->sizx, tex->sizy,
					   (intexfmt == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ? 5 : 1),
					   (float)(getticks() - starttime) / 1000.f);
			}
			
			if (tdef) {
				tdef->mipmap[tdefmip].sizx = tex->sizx;
				tdef->mipmap[tdefmip].sizy = tex->sizy;
				tdef->mipmap[tdefmip].length = comprsize;
				tdef->mipmap[tdefmip].data = comprdata;
				tdefmip++;
			}
			
			bglCompressedTexImage2DARB(GL_TEXTURE_2D, mipmap,
						intexfmt, tex->sizx, tex->sizy, 0,
						comprsize, (const GLvoid *) comprdata);

			if (tdef) {
				// we need to retain each mipmap for the tdef struct, so
				// force each mipmap to be allocated afresh in this loop
				comprdata = 0;
			}
		} else {
			bglTexImage2D(GL_TEXTURE_2D, mipmap,
				intexfmt, tex->sizx, tex->sizy, 0, tex->rawfmt,
				GL_UNSIGNED_BYTE, (const GLvoid *) tex->pic);
		}
	}
	
	if (comprdata) {
		free(comprdata);
	}
}

/**
 * Applies the global texture filter parameters to the given texture
 * @param pth the cache header
 */
static void pt_load_applyparameters(PTHead * pth)
{
	int i;

	for (i = 0; i < PTHPIC_SIZE; i++) {
		if (pth->pic[i] == 0 || pth->pic[i]->glpic == 0) {
			continue;
		}

		bglBindTexture(GL_TEXTURE_2D, pth->pic[i]->glpic);

		if (gltexfiltermode < 0) {
			gltexfiltermode = 0;
		} else if (gltexfiltermode >= (long)numglfiltermodes) {
			gltexfiltermode = numglfiltermodes-1;
		}
		bglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glfiltermodes[gltexfiltermode].mag);
		bglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glfiltermodes[gltexfiltermode].min);
		
		if (glinfo.maxanisotropy > 1.0) {
			if (glanisotropy <= 0 || glanisotropy > glinfo.maxanisotropy) {
				glanisotropy = (long)glinfo.maxanisotropy;
			}
			bglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, glanisotropy);
		}
		
		if (! (pth->flags & PTH_CLAMPED)) {
			bglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			bglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		} else {     //For sprite textures, clamping looks better than wrapping
			GLint c = glinfo.clamptoedge ? GL_CLAMP_TO_EDGE : GL_CLAMP;
			bglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, c);
			bglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, c);
		}
	}
}


/**
 * Prepare for priming by sweeping through the textures and marking them as all unused
 */
void PTBeginPriming(void)
{
	PTHash * pth;
	int i;
	
	for (i=PTHASHHEADSIZ-1; i>=0; i--) {
		pth = pthashhead[i];
		while (pth) {
			pth->primecnt = 0;
			pth = pth->next;
		}
	}
	
	primecnt = 0;
	primedone = 0;
	primepos = 0;
}

/**
 * Flag a texture as required for priming
 */
void PTMarkPrime(long picnum, long palnum, unsigned short flags)
{
	PTHash * pth;
	
	pth = pt_findhash(picnum, palnum, flags, 1);
	if (pth) {
		if (pth->primecnt == 0) {
			primecnt++;
		}
		pth->primecnt++;
	}
}

/**
 * Runs a cycle of the priming process. Call until nonzero is returned.
 * @param done receives the number of textures primed so far
 * @param total receives the total number of textures to be primed
 * @return 0 when priming is complete
 */
int PTDoPrime(int* done, int* total)
{
	PTHash * pth;
	
	if (primepos >= PTHASHHEADSIZ) {
		// done
		return 0;
	}
	
	if (primepos == 0) {
		int i;
		
		// first, unload all the textures that are not marked
		for (i=PTHASHHEADSIZ-1; i>=0; i--) {
			pth = pthashhead[i];
			while (pth) {
				if (pth->primecnt == 0) {
					pt_unload(pth);
				}
				pth = pth->next;
			}
		}
	}
	
	pth = pthashhead[primepos];
	while (pth) {
		if (pth->primecnt > 0) {
			primedone++;
			pt_load(pth);
		}
		pth = pth->next;
	}
	
	*done = primedone;
	*total = primecnt;
	primepos++;
	
	return (primepos < PTHASHHEADSIZ);
}

/**
 * Resets the texture hash but leaves the headers in memory
 */
void PTReset()
{
	PTHash * pth;
	int i;
	
	for (i=PTHASHHEADSIZ-1; i>=0; i--) {
		pth = pthashhead[i];
		while (pth) {
			pt_unload(pth);
			pth = pth->next;
		}
	}
}

/**
 * Clears the texture hash of all content
 */
void PTClear()
{
	PTHash * pth, * next;
	PTMHash * ptmh, * mnext;
	int i;
	
	for (i=PTHASHHEADSIZ-1; i>=0; i--) {
		pth = pthashhead[i];
		while (pth) {
			next = pth->next;
			pt_unload(pth);
			free(pth);
			pth = next;
		}
		pthashhead[i] = 0;
	}
	
	for (i=PTMHASHHEADSIZ-1; i>=0; i--) {
		ptmh = ptmhashhead[i];
		while (ptmh) {
			mnext = ptmh->next;
			free(ptmh);
			ptmh = mnext;
		}
		ptmhashhead[i] = 0;
	}
}



/**
 * Fetches a texture header ready for rendering
 * @param picnum
 * @param palnum
 * @param flags
 * @param peek if !0, does not try and create a header if none exists
 * @return pointer to the header, or null if peek!=0 and none exists
 */
PTHead * PT_GetHead(long picnum, long palnum, unsigned short flags, int peek)
{
	PTHash * pth;
	
	pth = pt_findhash(picnum, palnum, flags, peek == 0);
	if (pth == 0) {
		return 0;
	}
	
	if (!pt_load(pth)) {
		return 0;
	}
	
	while (pth->deferto) {
		// this might happen if pt_load needs to defer to ART
		pth = pth->deferto;
	}
	
	return &pth->head;
}





static inline int ptiter_matches(PTIter iter)
{
	if (iter->match == 0) {
		return 1;	// matching every item
	}
	if ((iter->match & PTITER_PICNUM) && iter->pth->head.picnum != iter->picnum) {
		return 0;
	}
	if ((iter->match & PTITER_PALNUM) && iter->pth->head.palnum != iter->palnum) {
		return 0;
	}
	if ((iter->match & PTITER_FLAGS) && (iter->pth->head.flags & iter->flagsmask) != iter->flags) {
		return 0;
	}
	return 1;
}

static void ptiter_seekforward(PTIter iter)
{
	while (1) {
		if (iter->pth && ptiter_matches(iter)) {
			break;
		}
		if (iter->pth == 0) {
			if ((iter->match & PTITER_PICNUM)) {
				// because the hash key is based on picture number,
				// reaching the end of the hash chain means we need
				// not look further
				break;
			}
			if (iter->i >= PTHASHHEADSIZ) {
				// end of hash
				iter->pth = 0;
				break;
			}
			iter->i++;
			iter->pth = pthashhead[iter->i];
		} else {
			iter->pth = iter->pth->next;
		}
	}
}

/**
 * Creates a new iterator for walking the header hash looking for particular
 * parameters that match.
 * @param match flags indicating which parameters to test
 * @param picnum when (match&PTITER_PICNUM), specifies the picnum
 * @param palnum when (match&PTITER_PALNUM), specifies the palnum
 * @param flagsmask when (match&PTITER_FLAGS), specifies the mask to apply to flags
 * @param flags when (match&PTITER_FLAGS), specifies the flags to test
 * @return an iterator
 */
PTIter PTIterNewMatch(int match, long picnum, long palnum, unsigned short flagsmask, unsigned short flags)
{
	PTIter iter;
	
	iter = (PTIter) malloc(sizeof(struct PTIter_typ));
	if (!iter) {
		return 0;
	}
	
	iter->i = 0;
	iter->pth = pthashhead[0];
	iter->match = match;
	iter->picnum = picnum;
	iter->palnum = palnum;
	iter->flagsmask = flagsmask;
	iter->flags = flags;
	
	if ((match & PTITER_PICNUM)) {
		iter->i = pt_gethashhead(picnum);
		iter->pth = pthashhead[iter->i];
		if (iter->pth == 0) {
			iter->pth = 0;
			return iter;
		}
	}
	
	ptiter_seekforward(iter);
	
	return iter;
}

/**
 * Creates a new iterator for walking the header hash
 * @return an iterator
 */
PTIter PTIterNew(void)
{
	return PTIterNewMatch(0, 0, 0, 0, 0);
}

/**
 * Gets the next header from an iterator
 * @param iter the iterator
 * @return the next header, or null if at the end
 */
PTHead * PTIterNext(PTIter iter)
{
	PTHead * found = 0;
	
	if (!iter) return 0;
	
	if (iter->pth == 0) {
		return 0;
	}
	
	found = &iter->pth->head;
	iter->pth = iter->pth->next;

	ptiter_seekforward(iter);
	
	return found;
}

/**
 * Frees an iterator
 * @param iter the iterator
 */
void PTIterFree(PTIter iter)
{
	if (!iter) return;
	free(iter);
}

#endif //USE_OPENGL
