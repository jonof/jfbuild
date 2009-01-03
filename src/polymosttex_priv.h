#ifndef POLYMOSTTEX_PRIV_H
#define POLYMOSTTEX_PRIV_H

enum {
	PTH_CLAMPED = 1,
	PTH_HIGHTILE = 2,
	PTH_SKYBOX = 4,
	PTH_HASALPHA = 8,
	PTH_NOCOMPRESS = 16,	// prevents texture compression from being used
	PTH_NOMIPLEVEL = 32,	// prevents gltexmiplevel from being applied
	PTH_DIRTY = 128,
};

// indices of PTHead.glpic[]
enum {
	PTHGLPIC_BASE = 0,
	PTHGLPIC_FULLBRIGHT = 1,
	PTHGLPIC_SIZE = 6,
};

struct PTHead_typ {
	GLuint glpic[PTHGLPIC_SIZE];	// when (flags & PTH_SKYBOX), each is a face of the cube
					// when !(flags & PTH_SKYBOX), see PTHGLPIC_* constants
	long picnum;
	long palnum;
	unsigned short flags;
	
	hicreplctyp *repldef;

	long sizx, sizy;		// padded texture dimensions
	long tsizx, tsizy;		// true texture dimensions
	float scalex, scaley;		// scale factor between texture and ART tile dimensions
};

typedef struct PTHead_typ PTHead;

enum {
	PTITER_PICNUM = 1,
	PTITER_PALNUM = 2,
	PTITER_FLAGS  = 4,
};

struct PTIter_typ;	// an opaque iterator type for walking the internal hash
typedef struct PTIter_typ * PTIter;

/**
 * Prepare for priming by sweeping through the textures and marking them as all unused
 */
void PTBeginPriming(void);

/**
 * Flag a texture as required for priming
 */
void PTMarkPrime(long picnum, long palnum, unsigned short flags);

/**
 * Runs a cycle of the priming process. Call until nonzero is returned.
 * @param done receives the number of textures primed so far
 * @param total receives the total number of textures to be primed
 * @return 0 when priming is complete
 */
int PTDoPrime(int* done, int* total);

/**
 * Resets the texture hash but leaves the headers in memory
 */
void PTReset();

/**
 * Clears the texture hash of all content
 */
void PTClear();

/**
 * Fetches a texture header. This also means loading the texture from
 * disk if need be (if peek!=0).
 * @param picnum
 * @param palnum
 * @param flags
 * @param peek if !0, does not try and create a header if none exists
 * @return pointer to the header, or null if peek!=0 and none exists
 */
PTHead * PT_GetHead(long picnum, long palnum, unsigned short flags, int peek);

PTIter PTIterNewMatch(int match, long picnum, long palnum, unsigned short flagsmask, unsigned short flags);

/**
 * Creates a new iterator for walking the header hash
 * @return an iterator
 */
PTIter PTIterNew(void);

/**
 * Gets the next header from an iterator
 * @param iter the iterator
 * @return the next header, or null if at the end
 */
PTHead * PTIterNext(PTIter iter);

/**
 * Frees an iterator
 * @param iter the iterator
 */
void PTIterFree(PTIter iter);

#endif
