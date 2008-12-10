#include "compat.h"
#include "build.h"
#include "glbuild.h"
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
};

static int primecnt   = 0;	// expected number of textures to load during priming
static int primedone  = 0;	// running total of how many textures have been primed
static int primepos   = 0;	// the position in pthashhead where we are up to in priming

#define PTHASHHEADSIZ 8192
static struct pthash *pthashhead[PTHASHHEADSIZ];

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
	int i = picnum & (PTHASHHEADSIZ-1);
	struct pthash * pth;
	struct pthash * basepth;	// palette 0 in case we find we need it
	
	long flagmask = flags;
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

/**
 * Loads a texture into memory from disk
 * @param pth pointer to the pthash of the texture to load
 * @return !0 on success
 */
static int pt_load(struct pthash * pth)
{
	if (pth->head.glpic != 0) return 1;	// loaded
	
	//FIXME
	if (pth->head.flags & PTH_HIGHTILE) {
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
	// try and load from the ART file
	// if that failed, we're SOL
	return 1;
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
 * @return !0 when priming is complete
 */
int ptdoprime(int* done, int* total)
{
	struct pthash * pth;
	
	if (primepos >= PTHASHHEADSIZ) {
		// done
		return 1;
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
	
	return (primepos >= PTHASHHEADSIZ);
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
	
	return &pth->head;
}

/**
 * Creates a new iterator for walking the header hash
 * @return an iterator
 */
struct ptiter * ptiternew(void)
{
	struct ptiter * iter;
	
	iter = (struct ptiter *) malloc(sizeof(struct ptiter));
	if (!iter) {
		return 0;
	}
	
	iter->i = 0;
	iter->pth = pthashhead[0];
	
	return iter;
}

/**
 * Gets the next header from an iterator
 * @param iter the iterator
 * @return the next header, or null if at the end
 */
struct pthead * ptiternext(struct ptiter *iter)
{
	if (!iter) return 0;
	
	while (1) {
		if (iter->pth) {
			struct pthash * pth = iter->pth;
			iter->pth = iter->pth->next;
			return &pth->head;
		}
		if (iter->pth == 0) {
			if (iter->i >= PTHASHHEADSIZ) {
				// end of hash
				return 0;
			}
			iter->i++;
			iter->pth = pthashhead[iter->i];
		}
	}
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

