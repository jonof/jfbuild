#ifndef HIGHTILE_PRIV_H
#define HIGHTILE_PRIV_H

enum {
	HICEFFECT_NONE = 0,
	HICEFFECT_GREYSCALE = 1,
	HICEFFECT_INVERT = 2,
	HICEFFECTMASK = 3,
};

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

hicreplctyp * hicfindsubst(long picnum, long palnum, long skybox);

#endif
