#include "build.h"

#if USE_POLYMOST && USE_OPENGL

#include "baselayer.h"
#include "glbuild.h"
#include "kplib.h"
#include "cache1d.h"
#include "pragmas.h"
#include "crc32.h"
#include "engine_priv.h"
#include "polymost_priv.h"
#include "hightile_priv.h"
#include "polymosttex_priv.h"
#include "polymosttexcache.h"
#include "polymosttexcompress.h"

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
	unsigned int idcrc;   // crc32 of the id below for quick comparisons
    PTMIdent id;
};
typedef struct PTMHash_typ PTMHash;

/** an iterator for walking the hash */
struct PTIter_typ {
	int i;
	PTHash *pth;

	// criteria for doing selective matching
	int match;
	int picnum;
	int palnum;
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

#define PTHASHHEADSIZ 4096
static PTHash * pthashhead[PTHASHHEADSIZ];	// will be initialised 0 by .bss segment

#define PTMHASHHEADSIZ 4096
static PTMHash * ptmhashhead[PTMHASHHEADSIZ];	// will be initialised 0 by .bss segment

static const char *compressfourcc[] = {
	"NONE",
	"DXT1",
	"DXT5",
	"ETC1",
};

static void ptm_fixtransparency(PTTexture * tex, int clamped);
static void ptm_applyeffects(PTTexture * tex, int effects);
static void ptm_mipscale(PTTexture * tex);
static void ptm_uploadtexture(PTMHead * ptm, unsigned short flags, PTTexture * tex, PTCacheTile * tdef);


static inline int pt_gethashhead(const int picnum)
{
	return picnum & (PTHASHHEADSIZ-1);
}

static inline int ptm_gethashhead(const unsigned int idcrc)
{
	return idcrc & (PTMHASHHEADSIZ-1);
}

static void detect_texture_size()
{
	if (gltexmaxsize <= 0) {
		GLint siz = glinfo.maxtexsize;
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
 * Calculates a texture id from the information in a PTHead structure
 * @param id the PTMIdent to initialise using...
 * @param pth the PTHead structure
 */
void PTM_InitIdent(PTMIdent *id, PTHead *pth)
{
    memset(id, 0, sizeof(PTMIdent));

    if (!pth) {
        return;
    }

	if (pth->flags & PTH_HIGHTILE) {
        id->type = PTMIDENT_HIGHTILE;

		if (!pth->repldef) {
			if (polymosttexverbosity >= 1) {
				buildprintf("PolymostTex: cannot calculate texture id for pth with no repldef\n");
			}
			memset(id, 0, 16);
			return;
		}

		id->flags = pth->flags & (PTH_CLAMPED | PTH_SKYBOX);
		id->effects = (pth->palnum != pth->repldef->palnum)
            ? hictinting[pth->palnum].f : 0;
		if (pth->flags & PTH_SKYBOX) {
			// hightile + skybox + picnum ought to cover it
		    id->picnum = pth->picnum;
		} else {
		    strncpy(id->filename, pth->repldef->filename, BMAX_PATH);
		}
	} else {
		id->type = PTMIDENT_ART;
		id->flags = pth->flags & (PTH_CLAMPED);
		id->palnum = pth->palnum;
		id->picnum = pth->picnum;
	}
}


/**
 * Returns a PTMHead pointer for the given texture id
 * @param id the identifier of the texture
 * @return the PTMHead item, or null if it couldn't be created
 */
PTMHead * PTM_GetHead(const PTMIdent *id)
{
	PTMHash * ptmh;
	int i;
    unsigned int idcrc;

    idcrc = crc32once((unsigned char *)id, sizeof(PTMIdent));

	i = ptm_gethashhead(idcrc);
	ptmh = ptmhashhead[i];

	while (ptmh) {
        if (ptmh->idcrc == idcrc &&
            memcmp(&ptmh->id, id, sizeof(PTMIdent)) == 0) {
			return &ptmh->head;
		}
		ptmh = ptmh->next;
	}

	ptmh = (PTMHash *) malloc(sizeof(PTMHash));
	if (ptmh) {
		memset(ptmh, 0, sizeof(PTMHash));

        ptmh->idcrc = idcrc;
		memcpy(&ptmh->id, id, sizeof(PTMIdent));
		ptmh->next = ptmhashhead[i];
		ptmhashhead[i] = ptmh;
	}

	return &ptmh->head;
}


/**
 * Loads a texture file into OpenGL from the PolymostTex cache
 * @param filename the texture filename
 * @param ptmh the PTMHead structure to receive the texture details
 * @param flags PTH_* flags to tune the load process
 * @param effects HICEFFECT_* effects
 * @return 0 on success, <0 on error
 */
static int ptm_loadcachedtexturefile(const char* filename, PTMHead* ptmh, int flags, int effects)
{
	int mipmap = 0, i = 0;
	PTCacheTile * tdef = 0;
	int compress = PTCOMPRESS_NONE;

	tdef = PTCacheLoadTile(filename, effects, flags & (PTH_CLAMPED));
	if (!tdef) {
		return -1;
	}

	switch (tdef->format) {
#if GL_EXT_texture_compression_dxt1 || GL_EXT_texture_compression_s3tc
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			compress = PTCOMPRESS_DXT1;
			if (!glinfo.texcomprdxt1) goto incompatible;
			break;
#endif
#if GL_EXT_texture_compression_s3tc
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			compress = PTCOMPRESS_DXT5;
			if (!glinfo.texcomprdxt5) goto incompatible;
			break;
#endif
#if GL_OES_compressed_ETC1_RGB8_texture
		case GL_ETC1_RGB8_OES:
			compress = PTCOMPRESS_ETC1;
			if (!glinfo.texcompretc1) goto incompatible;
			break;
#endif
		default:
incompatible:
			if (polymosttexverbosity >= 2) {
				buildprintf("PolymostTex: cached %s (effects %d, flags %d) has incompatible format %s (0x%x)\n",
						   tdef->filename, tdef->effects, tdef->flags,
						   compress ? compressfourcc[compress] : "?",
						   tdef->format);
			}
			PTCacheFreeTile(tdef);
			return -1;
	}

	if (polymosttexverbosity >= 2) {
		buildprintf("PolymostTex: loaded %s (effects %d, flags %d, %s) from cache\n",
				   tdef->filename, tdef->effects, tdef->flags, compressfourcc[compress]);
	}

	if (ptmh->glpic == 0) {
		glfunc.glGenTextures(1, &ptmh->glpic);
	}
	ptmh->tsizx = tdef->tsizx;
	ptmh->tsizy = tdef->tsizy;
	ptmh->sizx  = tdef->mipmap[0].sizx;
	ptmh->sizy  = tdef->mipmap[0].sizy;
	ptmh->flags = tdef->flags & PTH_HASALPHA;
	glfunc.glBindTexture(GL_TEXTURE_2D, ptmh->glpic);

	mipmap = 0;
	if (! (flags & PTH_NOMIPLEVEL)) {
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
		glfunc.glCompressedTexImage2D(GL_TEXTURE_2D, i,
								   tdef->format,
								   tdef->mipmap[i + mipmap].sizx,
								   tdef->mipmap[i + mipmap].sizy,
								   0, tdef->mipmap[i + mipmap].length,
								   (const GLvoid *) tdef->mipmap[i + mipmap].data);
	}

	PTCacheFreeTile(tdef);

	return 0;
}


/**
 * Loads a texture file into OpenGL
 * @param filename the texture filename
 * @param ptmh the PTMHead structure to receive the texture details
 * @param flags PTH_* flags to tune the load process
 * @param effects HICEFFECT_* effects to apply
 * @return 0 on success, <0 on error
 */
int PTM_LoadTextureFile(const char* filename, PTMHead* ptmh, int flags, int effects)
{
	PTTexture tex;
	int filh, picdatalen;
	int x, y;
	char * picdata = 0;
	PTCacheTile * tdef = 0;
	int writetocache = 0, iscached = 0;

	if (!(flags & PTH_NOCOMPRESS) && glusetexcache && glusetexcompr) {
		iscached = PTCacheHasTile(filename, effects, (flags & PTH_CLAMPED));

		// if the texture exists in the cache but the original file is newer,
		// ignore what's in the cache and overwrite it
		/*if (iscached && filemtime(filename) > filemtime(cacheitem)) {
			iscached = 0;
		}*/

		if (!iscached) {
			writetocache = 1;
		}
	}

	if (iscached) {
		if (ptm_loadcachedtexturefile(filename, ptmh, flags, effects) == 0) {
			return 0;
		}
	}

	filh = kopen4load((char *) filename, 0);
	if (filh < 0) {
		return -1;
	}
	picdatalen = kfilelength(filh);

	picdata = (char *) malloc(picdatalen);
	if (!picdata) {
		kclose(filh);
		return -2;
	}

	if (kread(filh, picdata, picdatalen) != picdatalen) {
		kclose(filh);
		return -3;
	}

	kclose(filh);

	kpgetdim(picdata, picdatalen, (int *) &tex.tsizx, (int *) &tex.tsizy);
	if (tex.tsizx == 0 || tex.tsizy == 0) {
		free(picdata);
		return -4;
	}

	if (!glinfo.texnpot || writetocache) {
		for (tex.sizx = 1; tex.sizx < tex.tsizx; tex.sizx += tex.sizx) ;
		for (tex.sizy = 1; tex.sizy < tex.tsizy; tex.sizy += tex.sizy) ;
	} else {
		tex.sizx = tex.tsizx;
		tex.sizy = tex.tsizy;
	}

	tex.pic = (coltype *) malloc(tex.sizx * tex.sizy * sizeof(coltype));
	if (!tex.pic) {
		return -2;
	}
	memset(tex.pic, 0, tex.sizx * tex.sizy * sizeof(coltype));

	if (kprender(picdata, picdatalen, tex.pic, tex.sizx * sizeof(coltype), tex.sizx, tex.sizy, 0, 0)) {
		free(picdata);
		free(tex.pic);
		return -5;
	}

	free(picdata);
	picdata = 0;

	ptm_applyeffects(&tex, effects);	// updates tex.hasalpha

	ptmh->flags = 0;
	if (tex.hasalpha) {
		ptmh->flags |= PTH_HASALPHA;
	}

	if (! (flags & PTH_CLAMPED) || (flags & PTH_SKYBOX)) { //Duplicate texture pixels (wrapping tricks for non power of 2 texture sizes)
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
	if (!glinfo.bgra) {
		int j;
		for (j = tex.sizx * tex.sizy - 1; j >= 0; j--) {
			swapchar(&tex.pic[j].r, &tex.pic[j].b);
		}
		tex.rawfmt = GL_RGBA;
	}

	ptmh->tsizx = tex.tsizx;
	ptmh->tsizy = tex.tsizy;
	ptmh->sizx  = tex.sizx;
	ptmh->sizy  = tex.sizy;

	if (writetocache) {
		int nmips = 0;
		while (max(1, (tex.sizx >> nmips)) > 1 ||
			   max(1, (tex.sizy >> nmips)) > 1) {
			nmips++;
		}
		nmips++;

		tdef = PTCacheAllocNewTile(nmips);
		tdef->filename = strdup(filename);
		tdef->effects = effects;
		tdef->flags = (flags | ptmh->flags) & (PTH_CLAMPED | PTH_HASALPHA);
	}

	ptm_uploadtexture(ptmh, flags, &tex, tdef);

	if (writetocache) {
		if (polymosttexverbosity >= 2) {
			buildprintf("PolymostTex: writing %s (effects %d, flags %d) to cache\n",
					   tdef->filename, tdef->effects, tdef->flags);
		}
		PTCacheWriteTile(tdef);
		PTCacheFreeTile(tdef);
		tdef = 0;
	}


	if (tex.pic) {
		free(tex.pic);
		tex.pic = 0;
	}

	return 0;
}

/**
 * Returns a string describing the error returned by PTM_LoadTextureFile
 * @param err the error code
 * @return the error string
 */
const char * PTM_GetLoadTextureFileErrorString(int err)
{
	switch (err) {
		case 0:
			return "no error";
		case -1:
			return "not found";
		case -2:
			return "out of memory";
		case -3:
			return "truncated read";
		case -4:
			return "unrecognised format";
		case -5:
			return "decode error";
		default:
			return "unknown error";
	}
}


/**
 * Finds the pthash entry for a tile, possibly creating it if one doesn't exist
 * @param picnum tile number
 * @param palnum palette number
 * @param flags PTH_HIGHTILE = try for hightile, PTH_CLAMPED
 * @param create !0 = create if none found
 * @return the PTHash item, or null if none was found
 */
static PTHash * pt_findhash(int picnum, int palnum, unsigned short flags, int create)
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
		if (pth->head.pic[i] && pth->head.pic[i]->glpic) {
			glfunc.glDeleteTextures(1, &pth->head.pic[i]->glpic);
			pth->head.pic[i]->glpic = 0;
		}
	}
}

static int pt_load_art(PTHead * pth);
static int pt_load_hightile(PTHead * pth);
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
		(pth->head.pic[PTHPIC_BASE]->flags & PTH_DIRTY) == 0) {
		return 1;	// loaded
	}

	if ((pth->head.flags & PTH_HIGHTILE)) {
		// try and load from a replacement
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
	int x, y, x2, y2;
	int dacol;
	int hasalpha = 0, hasfullbright = 0;
    PTMIdent id;

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
				dacol = (int) (*(unsigned char *)(waloff[pth->picnum]+x2*tex.tsizy+y2));
				if (dacol == 255) {
					wpptr->a = 0;
					hasalpha = 1;
				} else {
					wpptr->a = 255;
					dacol = (int) ((unsigned char)palookup[pth->palnum][dacol]);
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
	pth->flags &= ~(PTH_HASALPHA | PTH_SKYBOX);
	pth->flags |= (PTH_NOCOMPRESS | PTH_NOMIPLEVEL);
	tex.hasalpha = hasalpha;

    PTM_InitIdent(&id, pth);
    id.layer = PTHPIC_BASE;
	pth->pic[PTHPIC_BASE] = PTM_GetHead(&id);
	pth->pic[PTHPIC_BASE]->tsizx = tex.tsizx;
	pth->pic[PTHPIC_BASE]->tsizy = tex.tsizy;
	pth->pic[PTHPIC_BASE]->sizx  = tex.sizx;
	pth->pic[PTHPIC_BASE]->sizy  = tex.sizy;
	ptm_uploadtexture(pth->pic[PTHPIC_BASE], pth->flags, &tex, 0);

	if (hasfullbright) {
        id.layer = PTHPIC_GLOW;
		pth->pic[PTHPIC_GLOW] = PTM_GetHead(&id);
		pth->pic[PTHPIC_GLOW]->tsizx = tex.tsizx;
		pth->pic[PTHPIC_GLOW]->tsizy = tex.tsizy;
		pth->pic[PTHPIC_GLOW]->sizx  = tex.sizx;
		pth->pic[PTHPIC_GLOW]->sizy  = tex.sizy;
		fbtex.hasalpha = 1;
		ptm_uploadtexture(pth->pic[PTHPIC_GLOW], pth->flags, &fbtex, 0);
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
	const char *filename = 0;
	int hasalpha = 0;
	int effects = 0;
	int err = 0;
	int texture = 0, loaded[PTHPIC_SIZE] = { 0,0,0,0,0,0, };
    PTMIdent id;

	if (!pth->repldef) {
		return 0;
	} else if ((pth->flags & PTH_SKYBOX) && (pth->repldef->skybox == 0 || pth->repldef->skybox->ignore)) {
		return 0;
	} else if (pth->repldef->ignore) {
		return 0;
	}

	effects = (pth->palnum != pth->repldef->palnum) ? hictinting[pth->palnum].f : 0;

	pth->flags &= ~(PTH_NOCOMPRESS | PTH_HASALPHA);
	if (pth->repldef->flags & HIC_NOCOMPRESS) {
		pth->flags |= PTH_NOCOMPRESS;
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

        PTM_InitIdent(&id, pth);
        id.layer = texture;
        pth->pic[texture] = PTM_GetHead(&id);

		if ((err = PTM_LoadTextureFile(filename, pth->pic[texture], pth->flags, effects))) {
			if (polymosttexverbosity >= 1) {
				const char * errstr = PTM_GetLoadTextureFileErrorString(err);
				buildprintf("PolymostTex: %s (pic %d pal %d) %s\n",
						   filename, pth->picnum, pth->palnum, errstr);
			}
			continue;
		}

		if (texture == 0) {
			if (pth->flags & PTH_SKYBOX) {
				pth->scalex = (float)pth->pic[texture]->tsizx / 64.0;
				pth->scaley = (float)pth->pic[texture]->tsizy / 64.0;
			} else {
				pth->scalex = (float)pth->pic[texture]->tsizx / (float)tilesizx[pth->picnum];
				pth->scaley = (float)pth->pic[texture]->tsizy / (float)tilesizy[pth->picnum];
			}
		}

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

		glfunc.glBindTexture(GL_TEXTURE_2D, pth->pic[i]->glpic);

		if (gltexfiltermode < 0) {
			gltexfiltermode = 0;
		} else if (gltexfiltermode >= (int)numglfiltermodes) {
			gltexfiltermode = numglfiltermodes-1;
		}
		glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glfiltermodes[gltexfiltermode].mag);
		glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glfiltermodes[gltexfiltermode].min);

		if (glinfo.maxanisotropy > 1.0) {
			if (glanisotropy <= 0 || glanisotropy > glinfo.maxanisotropy) {
				glanisotropy = (int)glinfo.maxanisotropy;
			}
			glfunc.glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, glanisotropy);
		}

		if (! (pth->flags & PTH_CLAMPED)) {
			glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		} else {     //For sprite textures, clamping looks better than wrapping
			GLint c = glinfo.clamptoedge ? GL_CLAMP_TO_EDGE : GL_CLAMP;
			glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, c);
			glfunc.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, c);
		}
	}
}


/**
 * Applies a filter to transparent pixels to improve their appearence when bilinearly filtered
 * @param tex the texture to process
 * @param clamped whether the texture is to be used clamped
 */
static void ptm_fixtransparency(PTTexture * tex, int clamped)
{
	coltype *wpptr;
	int j, x, y, r, g, b;
	int dox, doy, naxsiz2;
	int daxsiz = tex->tsizx, daysiz = tex->tsizy;
	int daxsiz2 = tex->sizx, daysiz2 = tex->sizy;

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
				r += (int)wpptr[     -1].r;
				g += (int)wpptr[     -1].g;
				b += (int)wpptr[     -1].b;
				j++;
			}
			if ((x<daxsiz) && (wpptr[     +1].a)) {
				r += (int)wpptr[     +1].r;
				g += (int)wpptr[     +1].g;
				b += (int)wpptr[     +1].b;
				j++;
			}
			if ((y>     0) && (wpptr[naxsiz2].a)) {
				r += (int)wpptr[naxsiz2].r;
				g += (int)wpptr[naxsiz2].g;
				b += (int)wpptr[naxsiz2].b;
				j++;
			}
			if ((y<daysiz) && (wpptr[daxsiz2].a)) {
				r += (int)wpptr[daxsiz2].r;
				g += (int)wpptr[daxsiz2].g;
				b += (int)wpptr[daxsiz2].b;
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
 * in the texture (and updates tex->hasalpha).
 * @param tex the texture
 * @param effects the effects
 */
static void ptm_applyeffects(PTTexture * tex, int effects)
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

	tex->hasalpha = (alph != 255);
}


/**
 * Scales down the texture by half in-place
 * @param tex the texture
 */
static void ptm_mipscale(PTTexture * tex)
{
	GLsizei newx, newy;
	GLsizei x, y;
	coltype *wpptr, *rpptr;
	int r, g, b, a, k;

	newx = max(1, (tex->sizx >> 1));
	newy = max(1, (tex->sizy >> 1));

	for (y = 0; y < newy; y++) {
		wpptr = &tex->pic[y * newx];
		rpptr = &tex->pic[(y << 1) * tex->sizx];

		for (x = 0; x < newx; x++, wpptr++, rpptr += 2) {
			r = g = b = a = k = 0;
			if (rpptr[0].a) {
				r += (int)rpptr[0].r;
				g += (int)rpptr[0].g;
				b += (int)rpptr[0].b;
				a += (int)rpptr[0].a;
				k++;
			}
			if ((x+x+1 < tex->sizx) && (rpptr[1].a)) {
				r += (int)rpptr[1].r;
				g += (int)rpptr[1].g;
				b += (int)rpptr[1].b;
				a += (int)rpptr[1].a;
				k++;
			}
			if (y+y+1 < tex->sizy) {
				if (rpptr[tex->sizx].a) {
					r += (int)rpptr[tex->sizx  ].r;
					g += (int)rpptr[tex->sizx  ].g;
					b += (int)rpptr[tex->sizx  ].b;
					a += (int)rpptr[tex->sizx  ].a;
					k++;
				}
				if ((x+x+1 < tex->sizx) && (rpptr[tex->sizx+1].a)) {
					r += (int)rpptr[tex->sizx+1].r;
					g += (int)rpptr[tex->sizx+1].g;
					b += (int)rpptr[tex->sizx+1].b;
					a += (int)rpptr[tex->sizx+1].a;
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
static void ptm_uploadtexture(PTMHead * ptm, unsigned short flags, PTTexture * tex, PTCacheTile * tdef)
{
	int i;
	GLint mipmap;
	GLint intexfmt;
	int compress = PTCOMPRESS_NONE;
	unsigned char * comprdata = 0;
	int tdefmip = 0, comprsize = 0;
	int starttime;

	detect_texture_size();

#if USE_OPENGL == USE_GLES2
	// GLES permits BGRA as an internal format.
    intexfmt = tex->rawfmt;
#else
    intexfmt = GL_RGBA;
#endif
	if (!(flags & PTH_NOCOMPRESS) && glusetexcompr) {
#if GL_EXT_texture_compression_dxt1 || GL_EXT_texture_compression_s3tc
		if (!compress && !tex->hasalpha && glinfo.texcomprdxt1) {
			intexfmt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			compress = PTCOMPRESS_DXT1;
		}
#endif
#if GL_OES_compressed_ETC1_RGB8_texture
		if (!compress && !tex->hasalpha && glinfo.texcompretc1) {
			intexfmt = GL_ETC1_RGB8_OES;
			compress = PTCOMPRESS_ETC1;
		}
#endif
#if GL_EXT_texture_compression_s3tc
		if (!compress && tex->hasalpha && glinfo.texcomprdxt5) {
			intexfmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			compress = PTCOMPRESS_DXT5;
		}
#endif
	}

	if (compress && tdef) {
		tdef->format = intexfmt;
		tdef->tsizx  = tex->tsizx;
		tdef->tsizy  = tex->tsizy;
	}

	if (ptm->glpic == 0) {
		glfunc.glGenTextures(1, &ptm->glpic);
	}
	glfunc.glBindTexture(GL_TEXTURE_2D, ptm->glpic);

	ptm_fixtransparency(tex, (flags & PTH_CLAMPED));

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
			comprsize = ptcompress_getstorage(tex->sizx, tex->sizy, compress);
			comprdata = (unsigned char *) malloc(comprsize);

			starttime = getticks();
			ptcompress_compress(tex->pic, tex->sizx, tex->sizy, comprdata, compress);
			if (polymosttexverbosity >= 2) {
				buildprintf("PolymostTex: ptcompress_compress (%dx%d, %s) took %f sec\n",
					   tex->sizx, tex->sizy, compressfourcc[compress],
					   (float)(getticks() - starttime) / 1000.f);
			}

			tdef->mipmap[tdefmip].sizx = tex->sizx;
			tdef->mipmap[tdefmip].sizy = tex->sizy;
			tdef->mipmap[tdefmip].length = comprsize;
			tdef->mipmap[tdefmip].data = comprdata;
			tdefmip++;

			comprdata = 0;
		}

		ptm_mipscale(tex);
		ptm_fixtransparency(tex, (flags & PTH_CLAMPED));
	}

	if (compress) {
		comprsize = ptcompress_getstorage(tex->sizx, tex->sizy, compress);
		comprdata = (unsigned char *) malloc(comprsize);

		starttime = getticks();
		ptcompress_compress(tex->pic, tex->sizx, tex->sizy, comprdata, compress);
		if (polymosttexverbosity >= 2) {
			buildprintf("PolymostTex: ptcompress_compress (%dx%d, %s) took %f sec\n",
				   tex->sizx, tex->sizy, compressfourcc[compress],
				   (float)(getticks() - starttime) / 1000.f);
		}

		if (tdef) {
			tdef->mipmap[tdefmip].sizx = tex->sizx;
			tdef->mipmap[tdefmip].sizy = tex->sizy;
			tdef->mipmap[tdefmip].length = comprsize;
			tdef->mipmap[tdefmip].data = comprdata;
			tdefmip++;
		}

		glfunc.glCompressedTexImage2D(GL_TEXTURE_2D, 0,
			intexfmt, tex->sizx, tex->sizy, 0,
			comprsize, (const GLvoid *) comprdata);

		if (tdef) {
			// we need to retain each mipmap for the tdef struct, so
			// force each mipmap to be allocated afresh in the loop below
			comprdata = 0;
		}
	} else {
		glfunc.glTexImage2D(GL_TEXTURE_2D, 0,
			intexfmt, tex->sizx, tex->sizy, 0, tex->rawfmt,
			GL_UNSIGNED_BYTE, (const GLvoid *) tex->pic);
	}

	for (mipmap = 1; tex->sizx > 1 || tex->sizy > 1; mipmap++) {
		ptm_mipscale(tex);
		ptm_fixtransparency(tex, (flags & PTH_CLAMPED));

		if (compress) {
			comprsize = ptcompress_getstorage(tex->sizx, tex->sizy, compress);
			if (tdef) {
				comprdata = (unsigned char *) malloc(comprsize);
			}

			starttime = getticks();
			ptcompress_compress(tex->pic, tex->sizx, tex->sizy, comprdata, compress);
			if (polymosttexverbosity >= 2) {
				buildprintf("PolymostTex: ptcompress_compress (%dx%d, %s) took %f sec\n",
					   tex->sizx, tex->sizy, compressfourcc[compress],
					   (float)(getticks() - starttime) / 1000.f);
			}

			if (tdef) {
				tdef->mipmap[tdefmip].sizx = tex->sizx;
				tdef->mipmap[tdefmip].sizy = tex->sizy;
				tdef->mipmap[tdefmip].length = comprsize;
				tdef->mipmap[tdefmip].data = comprdata;
				tdefmip++;
			}

			glfunc.glCompressedTexImage2D(GL_TEXTURE_2D, mipmap,
						intexfmt, tex->sizx, tex->sizy, 0,
						comprsize, (const GLvoid *) comprdata);

			if (tdef) {
				// we need to retain each mipmap for the tdef struct, so
				// force each mipmap to be allocated afresh in this loop
				comprdata = 0;
			}
		} else {
			glfunc.glTexImage2D(GL_TEXTURE_2D, mipmap,
				intexfmt, tex->sizx, tex->sizy, 0, tex->rawfmt,
				GL_UNSIGNED_BYTE, (const GLvoid *) tex->pic);
		}
	}

	ptm->flags = 0;
	ptm->flags |= (tex->hasalpha ? PTH_HASALPHA : 0);

	if (comprdata) {
		free(comprdata);
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
void PTMarkPrime(int picnum, int palnum, unsigned short flags)
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
PTHead * PT_GetHead(int picnum, int palnum, unsigned short flags, int peek)
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
			iter->i++;
			if (iter->i >= PTHASHHEADSIZ) {
				// end of hash
				iter->pth = 0;
				iter->i = PTHASHHEADSIZ;
				break;
			}
			iter->pth = pthashhead[iter->i];
		} else {
			iter->pth = iter->pth->next;
		}
	}
}

/**
 * Creates a new iterator for walking the header hash looking for particular
 * parameters that match.
 * @param match PTITER_* flags indicating which parameters to test
 * @param picnum when (match&PTITER_PICNUM), specifies the picnum
 * @param palnum when (match&PTITER_PALNUM), specifies the palnum
 * @param flagsmask when (match&PTITER_FLAGS), specifies the mask to apply to flags
 * @param flags when (match&PTITER_FLAGS), specifies the flags to test
 * @return an iterator
 */
PTIter PTIterNewMatch(int match, int picnum, int palnum, unsigned short flagsmask, unsigned short flags)
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
