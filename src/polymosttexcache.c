#include "polymosttexcache.h"
#include "compat.h"
#include "baselayer.h"

#include "glbuild.h"
#include "build.h"
#include "hightile_priv.h"
#include "polymosttex_priv.h"

/*
 PolymostTex Cache file formats
 
 INDEX (texture.cacheindex):
   signature  "PolymostTexIndx"
   version    0x00
   ENTRIES...
     filename  char[BMAX_PATH]
     effects   long
     flags     long		PTH_CLAMPED
     offset    long		Offset from the start of the STORAGE file
     mtime     long		When kplib can return mtimes from ZIPs, this will be used to spot stale entries
 
 STORAGE (texture.cache):
   signature  "PolymostTexStor"
   version    0x00
   ENTRIES...
     tsizx     long		Unpadded dimensions
     tsizy     long
     flags     long		PTH_CLAMPED | PTH_HASALPHA
     format    long		OpenGL compressed format code
     nmipmaps  long		The number of mipmaps following
     MIPMAPS...
       sizx    long		Padded dimensions
       sizy    long
       length  long
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

static unsigned long gethashhead(const char * filename)
{
	// implements the djb2 hash, constrained to the hash table size
	// http://www.cse.yorku.ca/~oz/hash.html
	unsigned long hash = 5381;
	int c;
	
        while ((c = *filename++)) {
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
	unsigned long hash = gethashhead(filename);
	
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
	char sig[16];
	const char indexsig[16] = { 'P','o','l','y','m','o','s','t','T','e','x','I','n','d','x',CACHEVER };
	const char storagesig[16] = { 'P','o','l','y','m','o','s','t','T','e','x','S','t','o','r',CACHEVER };
	
	char filename[BMAX_PATH+1];
	long effects;
	long flags;
	long offset;
	long mtime;
	PTCacheIndex * pci;
	
	memset(filename, 0, sizeof(filename));
	
	// first, check the cache storage file's signature.
	// we open for reading and writing to test permission
	fh = fopen(CACHESTORAGEFILE, "r+b");
	if (fh) {
		if (fread(sig, 16, 1, fh) != 1 || memcmp(sig, storagesig, 16)) {
			cachereplace = 1;
			initprintf("Texture cache will be replaced\n");
		}
		fclose(fh);
	} else {
		if (errno == ENOENT) {
			// it's cool
			;
		} else {
			initprintf("Error opening %s, texture cache disabled\n", CACHESTORAGEFILE);
			cachedisabled = 1;
			return;
		}
	}
		
	// next, check the index
	fh = fopen(CACHEINDEXFILE, "r+b");
	if (fh) {
		if (fread(sig, 16, 1, fh) != 1 || memcmp(sig, indexsig, 16)) {
			if (!cachereplace) {
				cachereplace = 1;
				initprintf("Texture cache will be replaced\n");
			}
		}
	} else {
		if (errno == ENOENT) {
			// it's cool
			return;
		} else {
			initprintf("Error opening %s, texture cache disabled\n", CACHESTORAGEFILE);
			cachedisabled = 1;
			return;
		}
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
			cachereplace = 1;
			initprintf("Corrupt texture cache index detected, cache will be replaced\n");
			PTCacheUnloadIndex();
			break;
		}
		
		effects = B_LITTLE32(effects);
		flags   = B_LITTLE32(flags);
		offset  = B_LITTLE32(offset);
		mtime   = B_LITTLE32(mtime);
		
		pci = ptcache_findhash(filename, (int) effects, (int) flags);
		if (pci) {
			// superceding an old hash entry
			pci->offset = (off_t) offset;
		} else {
			ptcache_addhash(filename, (int) effects, (int) flags, (off_t) offset);
		}
	}
	
	fclose(fh);
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
}

/**
 * Does the task of loading a tile from the cache
 * @param offset the starting offset
 * @return a PTCacheTile entry fully completed
 */
static PTCacheTile * ptcache_load(off_t offset)
{
	long tsizx, tsizy;
	long sizx, sizy;
	long flags;
	long format;
	long nmipmaps, i;
	long length;
	
	PTCacheTile * tdef = 0;
	FILE * fh;
	
	if (cachereplace) {
		// cache is in a broken state, so don't try loading
		return 0;
	}
	
	fh = fopen(CACHESTORAGEFILE, "rb");
	if (!fh) {
		cachedisabled = 1;
		initprintf("Error opening %s, texture cache disabled\n", CACHESTORAGEFILE);
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
	initprintf("Corrupt texture cache detected, cache will be replaced\n");
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
		initprintf("Error opening %s, texture cache disabled\n", CACHESTORAGEFILE);
		return 0;
	}
	
	offset = ftell(fh);
	
	if (offset == 0) {
		// new file
		const char storagesig[16] = { 'P','o','l','y','m','o','s','t','T','e','x','S','t','o','r',CACHEVER };
		if (fwrite(storagesig, 16, 1, fh) != 1) {
			goto fail;
		}
		
		offset = 16;
	}
	
	{
		long tsizx, tsizy;
		long format, flags, nmipmaps;

		tsizx = B_LITTLE32(tdef->tsizx);
		tsizy = B_LITTLE32(tdef->tsizy);
		flags = B_LITTLE32(tdef->flags);
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
		long sizx, sizy;
		long length;

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
		initprintf("Error opening %s, texture cache disabled\n", CACHEINDEXFILE);
		return 0;
	}
	
	if (ftell(fh) == 0) {
		// new file
		const char indexsig[16] = { 'P','o','l','y','m','o','s','t','T','e','x','I','n','d','x',CACHEVER };
		if (fwrite(indexsig, 16, 1, fh) != 1) {
			goto fail;
		}
	}
	
	{
		char filename[BMAX_PATH];
		long effects, offsett, flags, mtime;
		
		memset(filename, 0, sizeof(filename));
		strncpy(filename, tdef->filename, sizeof(filename));
		effects = B_LITTLE32(tdef->effects);
		flags   = B_LITTLE32(tdef->flags);
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
		// superceding an old hash entry
		pci->offset = offset;
	} else {
		ptcache_addhash(tdef->filename, tdef->effects, tdef->flags, offset);
	}
	
	return 1;
fail:
	cachedisabled = 1;
	initprintf("Error writing to cache, texture cache disabled\n");
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
