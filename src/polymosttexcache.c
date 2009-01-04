#include "polymosttexcache.h"
#include "compat.h"
#include "baselayer.h"

/*
 PolymostTex Cache file formats
 
 INDEX (texture.cacheindex):
   signature  "PolymostTexIndx"
   version    0x00
   ENTRIES...
     filename  char[BMAX_PATH]
     effects   long
     offset    long		Offset from the start of the STORAGE file
     mtime     long		When kplib can return mtimes from ZIPs, this will be used to spot stale entries
 
 STORAGE (texture.cache):
   signature  "PolymostTexStor"
   version    0x00
   ENTRIES...
     tsizx     long		Unpadded dimensions
     tsizy     long
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
 * @param offset
 */
static void ptcache_addhash(char * filename, int effects, off_t offset)
{
	unsigned long hash = gethashhead(filename);
	
	// to reduce memory fragmentation we tack the filename onto the end of the block
	PTCacheIndex * pci = (PTCacheIndex *) malloc(sizeof(PTCacheIndex) + strlen(filename) + 1);
	
	pci->filename = (char *) pci + sizeof(PTCacheIndex);
	strcpy(pci->filename, filename);
	pci->effects = effects;
	pci->offset  = offset;
	pci->next = cachehead[hash];
	
	cachehead[hash] = pci;
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
	long offset;
	long mtime;
	
	memset(filename, 0, sizeof(filename));
	
	// first, check the cache storage file's signature.
	fh = fopen(CACHESTORAGEFILE, "rb");
	if (fh) {
		fseek(fh, 0, SEEK_SET);
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
	fh = fopen(CACHEINDEXFILE, "rb");
	if (fh) {
		fseek(fh, 0, SEEK_SET);
		if (fread(sig, 16, 1, fh) != 1 || memcmp(sig, indexsig, 16)) {
			if (!cachereplace) {
				cachereplace = 1;
				initprintf("Texture cache will be replaced\n");
			}
		}
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
	
	// now that the index is sitting at the first entry, load everything
	while (!feof(fh)) {
		if (fread(filename, BMAX_PATH, 1, fh) != 1 ||
		    fread(&effects, 4,         1, fh) != 1 ||
		    fread(&offset,  4,         1, fh) != 1 ||
		    fread(&mtime,   4,         1, fh) != 1) {
			// truncated entry, so throw the whole cache away
			cachereplace = 1;
			initprintf("Corrupt texture cache index detected, cache will be replaced\n");
			PTCacheUnloadIndex();
			break;
		}
		
		effects = B_LITTLE32(effects);
		offset = B_LITTLE32(offset);
		mtime = B_LITTLE32(mtime);
		
		ptcache_addhash(filename, (int) effects, (off_t) offset);
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
	
	if (!fread(&tsizx, 4, 1, fh) != 1 ||
	    !fread(&tsizy, 4, 1, fh) != 1 ||
	    !fread(&format, 4, 1, fh) != 1 ||
	    !fread(&nmipmaps, 4, 1, fh) != 1) {
		// truncated entry, so throw the whole cache away
		goto fail;
	}
	
	tsizx = B_LITTLE32(tsizx);
	tsizy = B_LITTLE32(tsizy);
	format = B_LITTLE32(format);
	nmipmaps = B_LITTLE32(nmipmaps);
	
	tdef = PTCacheAllocNewTile(nmipmaps);
	tdef->tsizx = tsizx;
	tdef->tsizy = tsizy;
	tdef->format = format;
	
	for (i = 0; i < nmipmaps; i++) {
		if (!fread(&sizx, 4, 1, fh) != 1 ||
		    !fread(&sizy, 4, 1, fh) != 1 ||
		    !fread(&length, 4, 1, fh) != 1) {
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
		
		if (!fread(tdef->mipmap[i].data, length, 1, fh) != 1) {
			// truncated data
			goto fail;
		}
	}
	
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
 * @return a PTCacheTile entry fully completed
 */
PTCacheTile * PTCacheLoadTile(char * filename, int effects)
{
	PTCacheIndex * pci;
	PTCacheTile * tdef;
	
	if (cachedisabled) {
		return 0;
	}
	
	pci = cachehead[ gethashhead(filename) ];
	if (!pci) {
		return 0;
	}
	
	while (pci) {
		if (effects == pci->effects &&
		    strcmp(pci->filename, filename) == 0) {
			tdef = ptcache_load(pci->offset);
			if (tdef) {
				tdef->filename = strdup(filename);
				tdef->effects  = effects;
			}
			return tdef;
		}
		pci = pci->next;
	}
	
	return 0;
}

/**
 * Checks to see if a tile exists in the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @return !0 if it exists
 */
int PTCacheHasTile(char * filename, int effects)
{
	PTCacheIndex * pci;

	if (cachedisabled) {
		return 0;
	}
	
	pci = cachehead[ gethashhead(filename) ];
	if (!pci) {
		return 0;
	}
	
	while (pci) {
		if (effects == pci->effects &&
		    strcmp(pci->filename, filename) == 0) {
			return 1;
		}
		pci = pci->next;
	}
	
	return 0;
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
 */
void PTCacheWriteTile(PTCacheTile * tdef)
{
	if (cachedisabled) {
		return;
	}
}
