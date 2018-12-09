#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef POLYMOSTTEXCACHE_H
#define POLYMOSTTEXCACHE_H

#ifdef __cplusplus
extern "C" {
#endif

struct PTCacheTileMip_typ {
	int sizx, sizy;
	int length;
	unsigned char * data;
};
typedef struct PTCacheTileMip_typ PTCacheTileMip;

struct PTCacheTile_typ {
	char * filename;
	int effects;
	int flags;
	int format;	// OpenGL format code
	int tsizx, tsizy;
	int nummipmaps;
	PTCacheTileMip mipmap[1];
};
typedef struct PTCacheTile_typ PTCacheTile;

/**
 * Loads the cache index file into memory
 */
void PTCacheLoadIndex(void);

/**
 * Unloads the cache index from memory
 */
void PTCacheUnloadIndex(void);

/**
 * Loads a tile from the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @param flags the flags bits
 * @return a PTCacheTile entry fully completed
 */
PTCacheTile * PTCacheLoadTile(const char * filename, int effects, int flags);

/**
 * Checks to see if a tile exists in the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @param flags the flags bits
 * @return !0 if it exists
 */
int PTCacheHasTile(const char * filename, int effects, int flags);

/**
 * Disposes of the resources allocated for a PTCacheTile
 * @param tdef a PTCacheTile entry
 */
void PTCacheFreeTile(PTCacheTile * tdef);

/**
 * Allocates the skeleton of a PTCacheTile entry for you to complete.
 * Memory for filenames and mipmap data should be allocated using malloc()
 * @param nummipmaps allocate mipmap entries for nummipmaps items
 * @return a PTCacheTile entry
 */
PTCacheTile * PTCacheAllocNewTile(int nummipmaps);

/**
 * Stores a PTCacheTile into the cache.
 * @param tdef a PTCacheTile entry fully completed
 * @return !0 on success
 */
int PTCacheWriteTile(PTCacheTile * tdef);

/**
 * Forces the cache to be rebuilt.
 */
void PTCacheForceRebuild(void);

#ifdef __cplusplus
}
#endif

#endif
