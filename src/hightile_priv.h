#ifndef HIGHTILE_PRIV_H
#define HIGHTILE_PRIV_H

#define HICEFFECTMASK (1|2)

struct hicskybox_t {
	long ignore;
	char *face[6];
};

typedef struct hicreplc_t {
	struct hicreplc_t *next;
	char palnum, ignore, flags, filler;
	char *filename;
	float alphacut;
	struct hicskybox_t *skybox;
} hicreplctyp;

extern palette_t hictinting[MAXPALOOKUPS];
extern hicreplctyp *hicreplc[MAXTILES];
extern char hicfirstinit;

typedef struct {
	char magic[8];	// 'Polymost'
	long xdim, ydim;	// of image, unpadded
	long flags;		// 1 = !2^x, 2 = has alpha, 4 = lzw compressed
} texcacheheader;
typedef struct {
	long size;
	long format;
	long xdim, ydim;	// of mipmap (possibly padded)
	long border, depth;
} texcachepicture;


hicreplctyp * hicfindsubst(long picnum, long palnum, long skybox);

#endif