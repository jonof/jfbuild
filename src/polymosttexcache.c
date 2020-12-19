#include "build.h"

#if USE_POLYMOST && USE_OPENGL

#include "polymosttexcache.h"
#include "baselayer.h"
#include "glbuild.h"
#include "hightile_priv.h"
#include "polymosttex_priv.h"

/*
 PolymostTex Cache file formats

 INDEX (texture.cacheindex):
   signature  "PolymostTexIndx"
   version    CACHEVER
   ENTRIES...
     filename  char[BMAX_PATH]
     effects   int32
     flags     int32		PTH_CLAMPED
     offset    int32		Offset from the start of the STORAGE file
     mtime     int32		When kplib can return mtimes from ZIPs, this will be used to spot stale entries

 STORAGE (texture.cache):
   signature  "PolymostTexStor"
   version    CACHEVER
   ENTRIES...
     tsizx     int32		Unpadded dimensions
     tsizy     int32
     flags     int32		PTH_CLAMPED | PTH_HASALPHA
     format    int32		OpenGL compressed format code
     nmipmaps  int32		The number of mipmaps following
     MIPMAPS...
       sizx    int32		Padded dimensions
       sizy    int32
       length  int32
       data    char[length]

 All multibyte values are little-endian.
 */

struct PTCacheIndex_typ {
	char * filename;
	int effects;
	int flags;
	off_t offset;
	struct PTCacheIndex_typ * next;
};
typedef struct PTCacheIndex_typ PTCacheIndex;
#define PTCACHEHASHSIZ 512
static PTCacheIndex * cachehead[PTCACHEHASHSIZ];	// will be initialized 0 by .bss segment

static const char * CACHEINDEXFILE = "texture.cacheindex";
static const char * CACHESTORAGEFILE = "texture.cache";
static const int CACHEVER = 0;

static int cachedisabled = 0, cachereplace = 0;

static unsigned int gethashhead(const char * filename)
{
	// implements the djb2 hash, constrained to the hash table size
	// http://www.cse.yorku.ca/~oz/hash.html
	unsigned long hash = 5381;
	int c;

    while ((c = (unsigned char)*filename++)) {
		hash = ((hash << 5) + hash) ^ c; /* hash * 33 ^ c */
	}

    return hash & (PTCACHEHASHSIZ-1);
}

/**
 * Adds an item to the cachehead hash.
 * @param filename
 * @param effects
 * @param flags
 * @param offset
 */
static void ptcache_addhash(const char * filename, int effects, int flags, off_t offset)
{
	unsigned int hash = gethashhead(filename);

	// to reduce memory fragmentation we tack the filename onto the end of the block
	PTCacheIndex * pci = (PTCacheIndex *) malloc(sizeof(PTCacheIndex) + strlen(filename) + 1);

	pci->filename = (char *) pci + sizeof(PTCacheIndex);
	strcpy(pci->filename, filename);
	pci->effects = effects;
	pci->flags   = flags & (PTH_CLAMPED);
	pci->offset  = offset;
	pci->next = cachehead[hash];

	cachehead[hash] = pci;
}

/**
 * Locates an item in the cachehead hash.
 * @param filename
 * @param effects
 * @param flags
 * @return the PTCacheIndex item, or null
 */
static PTCacheIndex * ptcache_findhash(const char * filename, int effects, int flags)
{
	PTCacheIndex * pci;

	pci = cachehead[ gethashhead(filename) ];
	if (!pci) {
		return 0;
	}

	flags &= PTH_CLAMPED;

	while (pci) {
		if (effects == pci->effects &&
		    flags == pci->flags &&
		    strcmp(pci->filename, filename) == 0) {
			return pci;
		}
		pci = pci->next;
	}

	return 0;
}

/**
 * Loads the cache index file into memory
 */
void PTCacheLoadIndex(void)
{
	FILE * fh = 0;
	int8_t sig[16];
	const int8_t indexsig[16] = { 'P','o','l','y','m','o','s','t','T','e','x','I','n','d','x',CACHEVER };
	const int8_t storagesig[16] = { 'P','o','l','y','m','o','s','t','T','e','x','S','t','o','r',CACHEVER };

	int8_t filename[BMAX_PATH+1];
	int32_t effects;
	int32_t flags;
	int32_t offset;
	int32_t mtime;
	PTCacheIndex * pci;

	int total = 0, dups = 0;
	int haveindex = 0, havestore = 0;

	memset(filename, 0, sizeof(filename));

	// first, check the cache storage file's signature.
	// we open for reading and writing to test permission
	fh = fopen(CACHESTORAGEFILE, "r+b");
	if (fh) {
		havestore = 1;

		if (fread(sig, 16, 1, fh) != 1 || memcmp(sig, storagesig, 16)) {
			cachereplace = 1;
		}
		fclose(fh);
	} else {
		if (errno == ENOENT) {
			// file doesn't exist, which is fine
			;
		} else {
			buildprintf("PolymostTexCache: error opening %s, texture cache disabled\n", CACHESTORAGEFILE);
			cachedisabled = 1;
			return;
		}
	}

	// next, check the index
	fh = fopen(CACHEINDEXFILE, "r+b");
	if (fh) {
		haveindex = 1;

		if (fread(sig, 16, 1, fh) != 1 || memcmp(sig, indexsig, 16)) {
			cachereplace = 1;
		}
	} else {
		if (errno == ENOENT) {
			// file doesn't exist, which is fine
			return;
		} else {
			buildprintf("PolymostTexCache: error opening %s, texture cache disabled\n", CACHEINDEXFILE);
			cachedisabled = 1;
			return;
		}
	}

	// if we're missing either the index or the store, but not both at the same
	// time, the cache is broken and should be replaced
	if ((!haveindex || !havestore) && !(!haveindex && !havestore)) {
		cachereplace = 1;
	}

	if (cachereplace) {
		buildprintf("PolymostTexCache: texture cache will be replaced\n");
		if (fh) {
			fclose(fh);
		}
		return;
	}

	// now that the index is sitting at the first entry, load everything
	while (!feof(fh)) {
		if (fread(filename, BMAX_PATH, 1, fh) != 1 && feof(fh)) {
			break;
		}
		if (fread(&effects, 4,         1, fh) != 1 ||
		    fread(&flags,   4,         1, fh) != 1 ||
		    fread(&offset,  4,         1, fh) != 1 ||
		    fread(&mtime,   4,         1, fh) != 1) {
			// truncated entry, so throw the whole cache away
			buildprintf("PolymostTexCache: corrupt texture cache index detected, cache will be replaced\n");
			cachereplace = 1;
			PTCacheUnloadIndex();
			break;
		}

		effects = B_LITTLE32(effects);
		flags   = B_LITTLE32(flags);
		offset  = B_LITTLE32(offset);
		mtime   = B_LITTLE32(mtime);

		pci = ptcache_findhash((char *) filename, (int) effects, (int) flags);
		if (pci) {
			// superseding an old hash entry
			pci->offset = (off_t) offset;
			dups++;
		} else {
			ptcache_addhash((char *) filename, (int) effects, (int) flags, (off_t) offset);
		}
		total++;
	}

	fclose(fh);

	buildprintf("PolymostTexCache: cache index loaded (%d entries, %d old entries skipped)\n", total, dups);
}

/**
 * Unloads the cache index from memory
 */
void PTCacheUnloadIndex(void)
{
	PTCacheIndex * pci, * next;
	int i;

	for (i = 0; i < PTCACHEHASHSIZ; i++) {
		pci = cachehead[i];
		while (pci) {
			next = pci->next;
			// we needn't free pci->filename since it was alloced with pci
			free(pci);
			pci = next;
		}
		cachehead[i] = 0;
	}

	buildprintf("PolymostTexCache: cache index unloaded\n");
}

/**
 * Does the task of loading a tile from the cache
 * @param offset the starting offset
 * @return a PTCacheTile entry fully completed
 */
static PTCacheTile * ptcache_load(off_t offset)
{
	int32_t tsizx, tsizy;
	int32_t sizx, sizy;
	int32_t flags;
	int32_t format;
	int32_t nmipmaps, i;
	int32_t length;

	PTCacheTile * tdef = 0;
	FILE * fh;

	if (cachereplace) {
		// cache is in a broken state, so don't try loading
		return 0;
	}

	fh = fopen(CACHESTORAGEFILE, "rb");
	if (!fh) {
		cachedisabled = 1;
		buildprintf("PolymostTexCache: error opening %s, texture cache disabled\n", CACHESTORAGEFILE);
		return 0;
	}

	fseek(fh, offset, SEEK_SET);

	if (fread(&tsizx, 4, 1, fh) != 1 ||
	    fread(&tsizy, 4, 1, fh) != 1 ||
	    fread(&flags, 4, 1, fh) != 1 ||
	    fread(&format, 4, 1, fh) != 1 ||
	    fread(&nmipmaps, 4, 1, fh) != 1) {
		// truncated entry, so throw the whole cache away
		goto fail;
	}

	tsizx = B_LITTLE32(tsizx);
	tsizy = B_LITTLE32(tsizy);
	flags = B_LITTLE32(flags);
	format = B_LITTLE32(format);
	nmipmaps = B_LITTLE32(nmipmaps);

	tdef = PTCacheAllocNewTile(nmipmaps);
	tdef->tsizx = tsizx;
	tdef->tsizy = tsizy;
	tdef->flags = flags;
	tdef->format = format;

	for (i = 0; i < nmipmaps; i++) {
		if (fread(&sizx, 4, 1, fh) != 1 ||
		    fread(&sizy, 4, 1, fh) != 1 ||
		    fread(&length, 4, 1, fh) != 1) {
			// truncated entry, so throw the whole cache away
			goto fail;
		}

		sizx = B_LITTLE32(sizx);
		sizy = B_LITTLE32(sizy);
		length = B_LITTLE32(length);

		tdef->mipmap[i].sizx = sizx;
		tdef->mipmap[i].sizy = sizy;
		tdef->mipmap[i].length = length;
		tdef->mipmap[i].data = (unsigned char *) malloc(length);

		if (fread(tdef->mipmap[i].data, length, 1, fh) != 1) {
			// truncated data
			goto fail;
		}
	}

	fclose(fh);

	return tdef;
fail:
	cachereplace = 1;
	buildprintf("PolymostTexCache: corrupt texture cache detected, cache will be replaced\n");
	PTCacheUnloadIndex();
	fclose(fh);
	if (tdef) {
		PTCacheFreeTile(tdef);
	}
	return 0;
}

/**
 * Loads a tile from the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @param flags the flags bits
 * @return a PTCacheTile entry fully completed
 */
PTCacheTile * PTCacheLoadTile(const char * filename, int effects, int flags)
{
	PTCacheIndex * pci;
	PTCacheTile * tdef;

	if (cachedisabled) {
		return 0;
	}

	pci = ptcache_findhash(filename, effects, flags);
	if (!pci) {
		return 0;
	}

	tdef = ptcache_load(pci->offset);
	if (tdef) {
		tdef->filename = strdup(filename);
		tdef->effects  = effects;
	}
	return tdef;
}

/**
 * Checks to see if a tile exists in the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @param flags the flags bits
 * @return !0 if it exists
 */
int PTCacheHasTile(const char * filename, int effects, int flags)
{
	if (cachedisabled) {
		return 0;
	}

	return (ptcache_findhash(filename, effects, flags) != 0);
}

/**
 * Disposes of the resources allocated for a PTCacheTile
 * @param tdef a PTCacheTile entry
 */
void PTCacheFreeTile(PTCacheTile * tdef)
{
	int i;

	if (tdef->filename) {
		free(tdef->filename);
	}
	for (i = 0; i < tdef->nummipmaps; i++) {
		if (tdef->mipmap[i].data) {
			free(tdef->mipmap[i].data);
		}
	}
	free(tdef);
}

/**
 * Allocates the skeleton of a PTCacheTile entry for you to complete.
 * Memory for filenames and mipmap data should be allocated using malloc()
 * @param nummipmaps allocate mipmap entries for nummipmaps items
 * @return a PTCacheTile entry
 */
PTCacheTile * PTCacheAllocNewTile(int nummipmaps)
{
	int size = sizeof(PTCacheTile) + (nummipmaps-1) * sizeof(PTCacheTileMip);
	PTCacheTile * tdef;

	tdef = (PTCacheTile *) malloc(size);
	memset(tdef, 0, size);

	tdef->nummipmaps = nummipmaps;

	return tdef;
}

/**
 * Stores a PTCacheTile into the cache.
 * @param tdef a PTCacheTile entry fully completed
 * @return !0 on success
 */
int PTCacheWriteTile(PTCacheTile * tdef)
{
	long i;
	PTCacheIndex * pci;

	FILE * fh;
	off_t offset;
	char createmode[] = "ab";

	if (cachedisabled) {
		return 0;
	}

	if (cachereplace) {
		createmode[0] = 'w';
		cachereplace = 0;
	}

	// 1. write the tile data to the storage file
	fh = fopen(CACHESTORAGEFILE, createmode);
	if (!fh) {
		cachedisabled = 1;
		buildprintf("PolymostTexCache: error opening %s, texture cache disabled\n", CACHESTORAGEFILE);
		return 0;
	}

	// apparently opening in append doesn't actually put the
	// file pointer at the end of the file like you would
	// imagine, so the ftell doesn't return the length of the
	// file like you'd expect it should
	fseek(fh, 0, SEEK_END);
	offset = ftell(fh);

	if (offset == 0) {
		// new file
		const int8_t storagesig[16] = { 'P','o','l','y','m','o','s','t','T','e','x','S','t','o','r',CACHEVER };
		if (fwrite(storagesig, 16, 1, fh) != 1) {
			goto fail;
		}

		offset = 16;
	}

	{
		int32_t tsizx, tsizy;
		int32_t format, flags, nmipmaps;

		tsizx = B_LITTLE32(tdef->tsizx);
		tsizy = B_LITTLE32(tdef->tsizy);
		flags = tdef->flags & (PTH_CLAMPED | PTH_HASALPHA);
		flags = B_LITTLE32(flags);
		format = B_LITTLE32(tdef->format);
		nmipmaps = B_LITTLE32(tdef->nummipmaps);

		if (fwrite(&tsizx, 4, 1, fh) != 1 ||
		    fwrite(&tsizy, 4, 1, fh) != 1 ||
		    fwrite(&flags, 4, 1, fh) != 1 ||
		    fwrite(&format, 4, 1, fh) != 1 ||
		    fwrite(&nmipmaps, 4, 1, fh) != 1) {
			goto fail;
		}
	}

	for (i = 0; i < tdef->nummipmaps; i++) {
		int32_t sizx, sizy;
		int32_t length;

		sizx = B_LITTLE32(tdef->mipmap[i].sizx);
		sizy = B_LITTLE32(tdef->mipmap[i].sizy);
		length = B_LITTLE32(tdef->mipmap[i].length);

		if (fwrite(&sizx, 4, 1, fh) != 1 ||
		    fwrite(&sizy, 4, 1, fh) != 1 ||
		    fwrite(&length, 4, 1, fh) != 1) {
			goto fail;
		}

		if (fwrite(tdef->mipmap[i].data, tdef->mipmap[i].length, 1, fh) != 1) {
			// truncated data
			goto fail;
		}
	}

	fclose(fh);

	// 2. append to the index
	fh = fopen(CACHEINDEXFILE, createmode);
	if (!fh) {
		cachedisabled = 1;
		buildprintf("PolymostTexCache: error opening %s, texture cache disabled\n", CACHEINDEXFILE);
		return 0;
	}

	fseek(fh, 0, SEEK_END);
	if (ftell(fh) == 0) {
		// new file
		const int8_t indexsig[16] = { 'P','o','l','y','m','o','s','t','T','e','x','I','n','d','x',CACHEVER };
		if (fwrite(indexsig, 16, 1, fh) != 1) {
			goto fail;
		}
	}

	{
		int8_t filename[BMAX_PATH];
		int32_t effects, offsett, flags, mtime;

		strncpy((char *) filename, tdef->filename, sizeof(filename)-1);
		filename[sizeof(filename)-1] = 0;
		effects = B_LITTLE32(tdef->effects);
		flags   = tdef->flags & (PTH_CLAMPED);	// we don't want the informational flags in the index
		flags   = B_LITTLE32(flags);
		offsett = B_LITTLE32(offset);
		mtime = 0;

		if (fwrite(filename, sizeof(filename), 1, fh) != 1 ||
		    fwrite(&effects, 4, 1, fh) != 1 ||
		    fwrite(&flags, 4, 1, fh) != 1 ||
		    fwrite(&offsett, 4, 1, fh) != 1 ||
		    fwrite(&mtime, 4, 1, fh) != 1) {
			goto fail;
		}
	}

	fclose(fh);

	// stow the data into the index in memory
	pci = ptcache_findhash(tdef->filename, tdef->effects, tdef->flags);
	if (pci) {
		// superseding an old hash entry
		pci->offset = offset;
	} else {
		ptcache_addhash(tdef->filename, tdef->effects, tdef->flags, offset);
	}

	return 1;
fail:
	cachedisabled = 1;
	buildprintf("PolymostTexCache: error writing to cache, texture cache disabled\n");
	if (fh) fclose(fh);
	return 0;
}

/**
* Forces the cache to be rebuilt.
 */
void PTCacheForceRebuild(void)
{
	PTCacheUnloadIndex();
	cachedisabled = 0;
	cachereplace = 1;
}

#endif //USE_OPENGL
