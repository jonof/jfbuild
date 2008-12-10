#ifndef POLYMOSTTEX_PRIV_H
#define POLYMOSTTEX_PRIV_H

enum {
	PTH_CLAMPED = 1,
	PTH_HIGHTILE = 2,
	PTH_SKYBOX = 4,
	PTH_HASALPHA = 8,
};

struct pthead {
	GLuint glpic;
	long picnum;
	long palnum;
	unsigned char effects;
	unsigned char flags;
	unsigned char skyface;
	unsigned char filler;
	
	hicreplctyp *repldef;

	unsigned short sizx, sizy;
	float scalex, scaley;
};

struct ptiter;	// an opaque iterator type for walking the internal hash

/**
 * Resets the texture hash but leaves the headers in memory
 */
void ptreset();

/**
 * Clears the texture hash of all content
 */
void ptclear();

/**
 * Fetches a texture header. This also means loading the texture from
 * disk if need be (if peek!=0).
 * @param picnum
 * @param palnum
 * @param peek if !0, does not try and create a header if none exists
 * @return pointer to the header, or null if peek!=0 and none exists
 */
struct pthead * ptgethead(long picnum, long palnum, int peek);

/**
 * Creates a new iterator for walking the header hash
 * @return an iterator
 */
struct ptiter * ptiternew(void);

/**
 * Gets the next header from an iterator
 * @param iter the iterator
 * @return the next header, or null if at the end
 */
struct pthead * ptiternext(struct ptiter *iter);

/**
 * Frees an iterator
 * @param iter the iterator
 */
void ptiterfree(struct ptiter *iter);

#endif