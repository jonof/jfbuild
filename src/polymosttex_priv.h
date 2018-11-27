#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef POLYMOSTTEX_PRIV_H
#define POLYMOSTTEX_PRIV_H

enum {
	PTH_CLAMPED = 1,
	PTH_HIGHTILE = 2,
	PTH_SKYBOX = 4,
	PTH_HASALPHA = 8,		// NOTE: only seen in PTMHead.flags, not in PTHead.flags
	PTH_NOCOMPRESS = 16,	// prevents texture compression from being used
	PTH_NOMIPLEVEL = 32,	// prevents gltexmiplevel from being applied
	PTH_DIRTY = 128,		// NOTE: only seen in PTMHead.flags, not in PTHead.flags
};

// indices of PTHead.pic[]
enum {
	PTHPIC_BASE = 0,
	PTHPIC_GLOW = 1,
	PTHPIC_DETAIL = 2,
	PTHPIC_SIZE = 6,
};


/** PolymostTex texture manager header */
struct PTMHead_typ {
	GLuint glpic;

	int flags;
	int sizx, sizy;		// padded texture dimensions
	int tsizx, tsizy;		// true texture dimensions
};

typedef struct PTMHead_typ PTMHead;

/** identifying information for a PolymostTex texture manager entry */
struct PTMIdent_typ {
    int type;       //see PTMIDENT_* constants
    int flags;
    int palnum;
    int picnum;
    int layer;      //see PTHPIC_* constants
	int effects;    //see HICEFFECT_* constants
	char filename[BMAX_PATH];
};

typedef struct PTMIdent_typ PTMIdent;

enum {
    PTMIDENT_ART = 0,
    PTMIDENT_HIGHTILE = 1,
    PTMIDENT_MDSKIN = 2,
};

/** PolymostTex texture header */
struct PTHead_typ {
	PTMHead *pic[PTHPIC_SIZE];	// when (flags & PTH_SKYBOX), each is a face of the cube
					// when !(flags & PTH_SKYBOX), see PTHPIC_* constants
	int picnum;
	int palnum;
	unsigned short flags;

	hicreplctyp *repldef;

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

extern int polymosttexverbosity;	// 0 = none, 1 = errors (default), 2 = all

/**
 * Prepare for priming by sweeping through the textures and marking them as all unused
 */
void PTBeginPriming(void);

/**
 * Flag a texture as required for priming
 */
void PTMarkPrime(int picnum, int palnum, unsigned short flags);

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
void PTReset(void);

/**
 * Clears the texture hash of all content
 */
void PTClear(void);

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
PTIter PTIterNewMatch(int match, int picnum, int palnum, unsigned short flagsmask, unsigned short flags);

/**
 * Creates a new iterator for walking the entire header hash
 * @return an iterator
 */
PTIter PTIterNew(void);

/**
 * Gets the next matching header from an iterator
 * @param iter the iterator
 * @return the next header, or null if at the end
 */
PTHead * PTIterNext(PTIter iter);

/**
 * Frees an iterator
 * @param iter the iterator
 */
void PTIterFree(PTIter iter);



/**
 * Fetches a texture header. This also means loading the texture from
 * disk if need be (if peek!=0).
 * @param picnum
 * @param palnum
 * @param flags
 * @param peek if !0, does not try and create a header if none exists
 * @return pointer to the header, or null if peek!=0 and none exists
 *
 * Shared method for polymost.c to call.
 */
PTHead * PT_GetHead(int picnum, int palnum, unsigned short flags, int peek);


/**
 * Initialise a PTMIdent structure from a PTHead
 * @param id the PTMIdent to initialise from...
 * @param pth the PTHead structure
 */
void PTM_InitIdent(PTMIdent *id, PTHead *pth);

/**
 * Returns a PTMHead pointer for the given texture id
 * @param id the texture identifier struct
 * @return the PTMHead item, or null if it couldn't be created
 *
 * Shared method for polymost.c and mdsprite.c to call.
 */
PTMHead * PTM_GetHead(const PTMIdent *id);

/**
 * Loads a texture file into OpenGL
 * @param filename the texture filename
 * @param ptmh the PTMHead structure to receive the texture details
 * @param flags PTH_* flags to tune the load process
 * @param effects HICEFFECT_* effects to apply
 * @return 0 on success, <0 on error
 *
 * Shared method for mdsprite.c to call.
 */
int PTM_LoadTextureFile(const char* filename, PTMHead* ptmh, int flags, int effects);

/**
 * Returns a string describing the error returned by PTM_LoadTextureFile
 * @param err the error code
 * @return the error string
 *
 * Shared method for mdsprite.c to call.
 */
const char * PTM_GetLoadTextureFileErrorString(int err);

#endif
