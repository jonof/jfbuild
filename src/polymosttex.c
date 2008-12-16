#include "compat.h"
#include "baselayer.h"
#include "build.h"
#include "glbuild.h"
#include "engine_priv.h"
#include "polymost_priv.h"
#include "hightile_priv.h"
#include "polymosttex_priv.h"

struct pthash {
	struct pthash *next;
	struct pthead head;
	struct pthash *deferto;	// if pt_findpthash can't find an exact match for a set of
				// parameters, it creates a header and defers it to another
				// entry that stands in its place
	int primecnt;	// a count of how many times the texture is touched when priming
};

struct ptiter {
	int i;
	struct pthash *pth;
	
	int match;
	long picnum;
	long palnum;
	unsigned char flagsmask;
	unsigned char flags;
};

static int primecnt   = 0;	// expected number of textures to load during priming
static int primedone  = 0;	// running total of how many textures have been primed
static int primepos   = 0;	// the position in pthashhead where we are up to in priming

#define PTHASHHEADSIZ 8192
static struct pthash *pthashhead[PTHASHHEADSIZ];

static inline int getpthashhead(long picnum)
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
 * @return the pthash item, or null if none was found
 */
static struct pthash * pt_findpthash(long picnum, long palnum, long skyface, long flags, int create)
{
	int i = getpthashhead(picnum);
	struct pthash * pth;
	struct pthash * basepth;	// palette 0 in case we find we need it
	
	long flagmask = flags & (PTH_HIGHTILE | PTH_CLAMPED | PTH_SKYBOX);
	if (skyface > 0) {
		flagmask |= PTH_SKYBOX;
	}
	
	// first, try and find an existing match for our parameters
	pth = pthashhead[i];
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
	}
	
	if (!create) {
		return 0;
	} else {
		// we didn't find one, so we have to create one
		hicreplctyp * replc = 0;
		
		if ((flags & PTH_HIGHTILE)) {
			replc = hicfindsubst(picnum, palnum, skyface);
		}

		pth = (struct pthash *) malloc(sizeof(struct pthash));
		if (!pth) {
			return 0;
		}
		memset(pth, 0, sizeof(struct pthash));
		
		pth->next = pthashhead[i];
		pth->head.picnum  = picnum;
		pth->head.palnum  = palnum;
		pth->head.flags   = flagmask;
		pth->head.skyface = skyface;
		pth->head.repldef = replc;
		
		pthashhead[i] = pth;
		
		if (replc && replc->palnum != palnum) {
			// we were given a substitute by hightile, so what we just
			// created needs to defer to the substitute
			pth->deferto = pt_findpthash(picnum, replc->palnum, skyface, flags, create);
		} else if ((flags & PTH_HIGHTILE) && !replc) {
			// there is no replacement, so defer to ART
			pth->deferto = pt_findpthash(picnum, replc->palnum, skyface, (flags & ~PTH_HIGHTILE), create);
		}
	}
	
	return pth;
}

/**
 * Unloads a texture from memory
 * @param pth pointer to the pthash of the loaded texture
 */
static void pt_unload(struct pthash * pth)
{
	if (pth->head.glpic == 0) return;	// not loaded
	
	bglDeleteTextures(1, &pth->head.glpic);
	pth->head.glpic = 0;
}

static int pt_load_art(struct pthead * pth);
static int pt_load_hightile(struct pthead * pth);
static int pt_load_cache(struct pthead * pth);
static void pt_load_fixtransparency(coltype * dapic, long daxsiz, long daysiz, long daxsiz2, long daysiz2, int clamped);
static int pt_load_uploadtexture(int replace);
static void pt_load_applyparameters(struct pthead * pth);

/**
 * Loads a texture into memory from disk
 * @param pth pointer to the pthash of the texture to load
 * @return !0 on success
 */
static int pt_load(struct pthash * pth)
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
		pth->deferto = pt_findpthash(
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
static int pt_load_art(struct pthead * pth)
{
	// TODO: implement fullbrights by copying fullbright texels to
	//   a second buffer and uploading to a secondary texture
	coltype * pic, * wpptr;
	long tsizx, tsizy;	// true size of the texture
	long xsiz, ysiz;	// rounded size
	long x, y, x2, y2;
	long dacol;
	int hasalpha = 0, replace = 0;
	
	tsizx = tilesizx[pth->picnum];
	tsizy = tilesizy[pth->picnum];
	if (!glinfo.texnpot) {
		for (xsiz = 1; xsiz < tsizx; xsiz += xsiz) ;
		for (ysiz = 1; ysiz < tsizx; ysiz += ysiz) ;
	} else {
		if ((tsizx | tsizy) == 0) {
			xsiz = ysiz = 1;
		} else {
			xsiz = tsizx;
			ysiz = tsizy;
		}
	}
	
	pic = (coltype *) malloc(xsiz * ysiz * sizeof(coltype));
	if (!pic) {
		return 0;
	}
	
	if (!waloff[pth->picnum]) {
		// Force invalid textures to draw something - an almost purely transparency texture
		// This allows the Z-buffer to be updated for mirrors (which are invalidated textures)
		pic[0].r = pic[0].g = pic[0].b = 0; pic[0].a = 1;
		tsizx = tsizy = 1;
		hasalpha = 1;
	} else {
		for (y = 0; y < ysiz; y++) {
			if (y < tsizy) {
				y2 = y;
			} else {
				y2 = y-tsizy;
			}
			wpptr = &pic[y*xsiz];
			for (x = 0; x < xsiz; x++, wpptr++) {
				if ((pth->flags & PTH_CLAMPED) && (x >= tsizx || y >= tsizy)) {
					// Clamp texture
					wpptr->r = wpptr->g = wpptr->b = wpptr->a = 0;
					continue;
				}
				if (x < tsizx) {
					x2 = x;
				} else {
					// wrap around to fill the repeated region
					x2 = x-tsizx;
				}
				dacol = (long) (*(unsigned char *)(waloff[pth->picnum]+x2*tsizy+y2));
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
	
	if (pth->glpic == 0) {
		bglGenTextures(1, &pth->glpic);
	} else {
		replace = 1;
	}
	bglBindTexture(GL_TEXTURE_2D, pth->glpic);
	
	pt_load_fixtransparency(pic, tsizx, tsizy, xsiz, ysiz, (pth->flags & PTH_CLAMPED));
	pt_load_uploadtexture(replace);
	pt_load_applyparameters(pth);
	
	if (pic) {
		free(pic);
	}
	
	if (hasalpha) {
		pth->flags |= PTH_HASALPHA;
	}
	pth->flags &= ~PTH_DIRTY;
	
	return 1;
}


/**
 * Applies a filter to transparent pixels to improve their appearence when bilinearly filtered
 * @param dapic the bitmap
 * @param daxsiz the width of the unpadded region of the picture
 * @param daysiz the height of the unpadded region of the picture
 * @param daxsiz2 the padded width of the picture
 * @param daysiz2 the padded height of the picture
 * @param clamped whether the texture is to be used clamped
 */
static void pt_load_fixtransparency(coltype * dapic, long daxsiz, long daysiz, long daxsiz2, long daysiz2, int clamped)
{
	coltype *wpptr;
	long j, x, y, r, g, b, dox, doy, naxsiz2;
	
	dox = daxsiz2-1; doy = daysiz2-1;
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
		wpptr = &dapic[y*daxsiz2+dox];
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
 * Sends texture data to GL
 */
static int pt_load_uploadtexture(int replace)
{
}

/**
 * Applies the global texture filter parameters to the currently-bound texture
 * @param clamped 
 */
static void pt_load_applyparameters(struct pthead * pth)
{
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
void ptbeginpriming(void)
{
	struct pthash * pth;
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
void ptmarkprime(long picnum, long palnum)
{
	struct pthash * pth;
	
	pth = pt_findpthash(picnum, palnum, /*FIXME skyface*/0, /*FIXME flags*/0, 1);
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
int ptdoprime(int* done, int* total)
{
	struct pthash * pth;
	
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
void ptreset()
{
	struct pthash * pth;
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
void ptclear()
{
	struct pthash * pth, * next;
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
}



/**
 * Fetches a texture header ready for rendering
 * @param picnum
 * @param palnum
 * @param peek if !0, does not try and create a header if none exists
 * @return pointer to the header, or null if peek!=0 and none exists
 */
struct pthead * ptgethead(long picnum, long palnum, int peek)
{
	struct pthash * pth;
	
	pth = pt_findpthash(picnum, palnum, /*FIXME skyface*/0, /*FIXME flags*/0, peek == 0);
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

static int ptiter_matches(struct ptiter * iter)
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

static void ptiter_seekforward(struct ptiter * iter)
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
struct ptiter * ptiternewmatch(int match, long picnum, long palnum, unsigned char flagsmask, unsigned char flags)
{
	struct ptiter * iter;
	
	iter = (struct ptiter *) malloc(sizeof(struct ptiter));
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
		iter->i = getpthashhead(picnum);
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
struct ptiter * ptiternew(void)
{
	return ptiternewmatch(0, 0, 0, 0, 0);
}

/**
 * Gets the next header from an iterator
 * @param iter the iterator
 * @return the next header, or null if at the end
 */
struct pthead * ptiternext(struct ptiter *iter)
{
	struct pthead * found = 0;
	
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
void ptiterfree(struct ptiter *iter)
{
	if (!iter) return;
	free(iter);
}

