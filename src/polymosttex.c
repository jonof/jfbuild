#include "compat.h"
#include "baselayer.h"
#include "build.h"
#include "glbuild.h"
#include "engine_priv.h"
#include "polymost_priv.h"
#include "hightile_priv.h"
#include "polymosttex_priv.h"

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
};
typedef struct PTTexture_typ PTTexture;

static int primecnt   = 0;	// expected number of textures to load during priming
static int primedone  = 0;	// running total of how many textures have been primed
static int primepos   = 0;	// the position in hashhead where we are up to in priming

#define PTHASHHEADSIZ 8192
static PTHash * hashhead[PTHASHHEADSIZ];

static inline int gethashhead(long picnum)
{
	return picnum & (PTHASHHEADSIZ-1);
}

/**
 * Finds the pthash entry for a tile, possibly creating it if one doesn't exist
 * @param picnum tile number
 * @param palnum palette number
 * @param skyface skybox face number (0 = none)
 * @param flags PTH_HIGHTILE = try for hightile, PTH_CLAMPED
 * @param create !0 = create if none found
 * @return the PTHash item, or null if none was found
 */
static PTHash * pt_findhash(long picnum, long palnum, long skyface, unsigned short flags, int create)
{
	int i = gethashhead(picnum);
	PTHash * pth;
	PTHash * basepth;	// palette 0 in case we find we need it
	
	unsigned short flagmask = flags & (PTH_HIGHTILE | PTH_CLAMPED | PTH_SKYBOX);
	if (skyface > 0) {
		flagmask |= PTH_SKYBOX;
	}
	
	// first, try and find an existing match for our parameters
	pth = hashhead[i];
	while (pth) {
		if (pth->head.picnum == picnum &&
		    pth->head.palnum == palnum &&
		    (pth->head.flags & (PTH_HIGHTILE | PTH_CLAMPED | PTH_SKYBOX)) == flagmask &&
		    pth->head.skyface == skyface
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
			replc = hicfindsubst(picnum, palnum, skyface);
			if (replc) {
				if (replc->flags & 1) {
					flagmask |= PTH_NOCOMPRESS;
				}
			}
		}

		pth = (PTHash *) malloc(sizeof(PTHash));
		if (!pth) {
			return 0;
		}
		memset(pth, 0, sizeof(PTHash));
		
		pth->next = hashhead[i];
		pth->head.picnum  = picnum;
		pth->head.palnum  = palnum;
		pth->head.flags   = flagmask;
		pth->head.skyface = skyface;
		pth->head.repldef = replc;
		
		hashhead[i] = pth;
		
		if (replc && replc->palnum != palnum) {
			// we were given a substitute by hightile, so what we just
			// created needs to defer to the substitute
			pth->deferto = pt_findhash(picnum, replc->palnum, skyface, flags, create);
		} else if ((flags & PTH_HIGHTILE) && !replc) {
			// there is no replacement, so defer to ART
			pth->deferto = pt_findhash(picnum, replc->palnum, skyface, (flags & ~PTH_HIGHTILE), create);
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
	if (pth->head.glpic == 0) return;	// not loaded
	
	bglDeleteTextures(1, &pth->head.glpic);
	pth->head.glpic = 0;
}

static int pt_load_art(PTHead * pth);
static int pt_load_hightile(PTHead * pth);
static int pt_load_cache(PTHead * pth);
static void pt_load_fixtransparency(PTTexture * tex, int clamped);
static void pt_load_mipscale(PTTexture * tex);
static void pt_load_uploadtexture(PTHead * pth, PTTexture * tex);
static void pt_load_applyparameters(PTHead * pth);

/**
 * Loads a texture into memory from disk
 * @param pth pointer to the pthash of the texture to load
 * @return !0 on success
 */
static int pt_load(PTHash * pth)
{
	if (pth->head.glpic != 0 && (pth->head.flags & PTH_DIRTY) == 0) {
		return 1;	// loaded
	}
	
	//FIXME
	if ((pth->head.flags & PTH_HIGHTILE)) {
		// try and load from a cached replacement
		// if that failed, try and load from a raw replacement
		// if that failed, get the hash for the ART version and
		//   defer this to there
		pth->deferto = pt_findhash(
				pth->head.picnum, pth->head.palnum,
				pth->head.skyface, (pth->head.flags & ~PTH_HIGHTILE),
				1);
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
	// TODO: implement fullbrights by copying fullbright texels to
	//   a second buffer and uploading to a secondary texture
	PTTexture tex;
	coltype * wpptr;
	long x, y, x2, y2;
	long dacol;
	int hasalpha = 0;
	
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
	
	tex.pic = (coltype *) malloc(tex.sizx * tex.sizy * sizeof(coltype));
	if (!tex.pic) {
		return 0;
	}
	
	tex.rawfmt = GL_RGBA;
	
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
			for (x = 0; x < tex.sizx; x++, wpptr++) {
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
			}
		}
	}
	
	pth->sizx = tilesizx[pth->picnum];
	pth->sizy = tilesizy[pth->picnum];
	pth->scalex = 1.0;
	pth->scaley = 1.0;
	pth->flags &= ~(PTH_DIRTY);
	pth->flags |= (PTH_NOCOMPRESS | PTH_NOMIPLEVEL);
	if (hasalpha) {
		pth->flags |= PTH_HASALPHA;
	}
	
	pt_load_uploadtexture(pth, &tex);
	pt_load_applyparameters(pth);
	
	if (tex.pic) {
		free(tex.pic);
	}
	
	return 1;
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
 * @param pth the cache header
 * @param tex the texture to upload
 */
static void pt_load_uploadtexture(PTHead * pth, PTTexture * tex)
{
	int replace = 0;
	int i;
	GLint mipmap;
	GLint intexfmt;
	
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
		
	if ((pth->flags & PTH_NOCOMPRESS) || !glinfo.texcompr || !glusetexcompr) {
		intexfmt = (pth->flags & PTH_HASALPHA)
		         ? GL_RGBA
			 : GL_RGB;
	} else {
		intexfmt = (pth->flags & PTH_HASALPHA)
		         ? GL_COMPRESSED_RGBA_ARB
			 : GL_COMPRESSED_RGB_ARB;
	}
	
	if (pth->glpic == 0) {
		bglGenTextures(1, &pth->glpic);
	} else {
		replace = 1;
	}
	bglBindTexture(GL_TEXTURE_2D, pth->glpic);

	pt_load_fixtransparency(tex, (pth->flags & PTH_CLAMPED));

	mipmap = 0;
	if (! (pth->flags & PTH_NOMIPLEVEL)) {
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
		pt_load_mipscale(tex);
		pt_load_fixtransparency(tex, (pth->flags & PTH_CLAMPED));
	}
	
	if (replace) {
		bglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			tex->sizx, tex->sizy, tex->rawfmt,
			GL_UNSIGNED_BYTE, (const GLvoid *) tex->pic);
	} else {
		bglTexImage2D(GL_TEXTURE_2D, 0,
			intexfmt, tex->sizx, tex->sizy, 0, tex->rawfmt,
			GL_UNSIGNED_BYTE, (const GLvoid *) tex->pic);
	}
	
	for (mipmap = 1; tex->sizx > 1 || tex->sizy > 1; mipmap++) {
		pt_load_mipscale(tex);
		pt_load_fixtransparency(tex, (pth->flags & PTH_CLAMPED));

		if (replace) {
			bglTexSubImage2D(GL_TEXTURE_2D, mipmap, 0, 0,
				tex->sizx, tex->sizy, tex->rawfmt,
				GL_UNSIGNED_BYTE, (const GLvoid *) tex->pic);
		} else {
			bglTexImage2D(GL_TEXTURE_2D, mipmap,
				intexfmt, tex->sizx, tex->sizy, 0, tex->rawfmt,
				GL_UNSIGNED_BYTE, (const GLvoid *) tex->pic);
		}
	}
}

/**
 * Applies the global texture filter parameters to the given texture
 * @param pth the cache header
 */
static void pt_load_applyparameters(PTHead * pth)
{
	if (pth->glpic == 0) {
		return;
	}
	bglBindTexture(GL_TEXTURE_2D, pth->glpic);

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


/**
 * Prepare for priming by sweeping through the textures and marking them as all unused
 */
void PTBeginPriming(void)
{
	PTHash * pth;
	int i;
	
	for (i=PTHASHHEADSIZ-1; i>=0; i--) {
		pth = hashhead[i];
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
void PTMarkPrime(long picnum, long palnum)
{
	PTHash * pth;
	
	pth = pt_findhash(picnum, palnum, /*FIXME skyface*/0, /*FIXME flags*/0, 1);
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
			pth = hashhead[i];
			while (pth) {
				if (pth->primecnt == 0) {
					pt_unload(pth);
				}
				pth = pth->next;
			}
		}
	}
	
	pth = hashhead[primepos];
	while (pth) {
		if (pth->primecnt == 0) {
			// was not in the set to prime
			continue;
		}
		primedone++;
		if (pth->head.glpic == 0) {
			// load the texture
			pt_load(pth);
		}
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
		pth = hashhead[i];
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
	int i;
	
	for (i=PTHASHHEADSIZ-1; i>=0; i--) {
		pth = hashhead[i];
		while (pth) {
			next = pth->next;
			pt_unload(pth);
			free(pth);
			pth = next;
		}
		hashhead[i] = 0;
	}
}



/**
 * Fetches a texture header ready for rendering
 * @param picnum
 * @param palnum
 * @param skyface
 * @param flags
 * @param peek if !0, does not try and create a header if none exists
 * @return pointer to the header, or null if peek!=0 and none exists
 */
PTHead * PT_GetHead(long picnum, long palnum, unsigned char skyface, unsigned short flags, int peek)
{
	PTHash * pth;
	
	pth = pt_findhash(picnum, palnum, skyface, flags, peek == 0);
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

static int ptiter_matches(PTIter iter)
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
			iter->pth = hashhead[iter->i];
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
	iter->pth = hashhead[0];
	iter->match = match;
	iter->picnum = picnum;
	iter->palnum = palnum;
	iter->flagsmask = flagsmask;
	iter->flags = flags;
	
	if ((match & PTITER_PICNUM)) {
		iter->i = gethashhead(picnum);
		iter->pth = hashhead[iter->i];
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

