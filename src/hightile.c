/*
 * High-colour textures support for Polymost
 * by Jonathon Fowler
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "kplib.h"

#define HICEFFECTMASK (1|2)
static palette_t hictinting[MAXPALOOKUPS];

#define HICHASHSZ 1024		// hash size
#define HICMAXREPL MAXTILES	// maximum number of replacement tiles
static unsigned short hicreplhash[HICHASHSZ];
static unsigned short hicreplnext[HICMAXREPL];	// index of next replacement in the hash chain
static long hicreplidx[HICMAXREPL];		// hash keys for replacements
struct hicskybox_t {
	char ignore;
	char *face[6];
};
static struct hicreplc_t {
	char *filename, ignore;
	struct hicskybox_t *skybox;
} *hicreplc[HICMAXREPL];
static unsigned short hicreplcnt = 0;		// number of replacements currently recorded
static char hicfirstinit = 0;

//
// find the index into hicreplc[] which contains the replacement tile particulars
//
static long hicfindsubst(short picnum, short palnum)
{
	long hashkey, hashslot;
	unsigned short i;

	if (!hicfirstinit) return -1;

	hashkey = (long)picnum + ((long)palnum * MAXTILES);
	hashslot = hashkey & (HICHASHSZ-1);		// find the pointer into the hash table to start looking

	for (i = hicreplhash[hashslot]; i != 0xffff; i = hicreplnext[i]) {	// starting at the head of the hash slot,
									// seek the index we want unless we hit
									// the end
		if (hicreplidx[i] == hashkey) {		// if the index of this replacement matches that which we
			if (hicreplc[i]) {		// seek, return it
				if (hicreplc[i]->ignore) return -1;
				return i;
			} else
				return -1;
		}
	}

	return -1;	// no replacement found
}

static long hicfindskybox(short picnum, short palnum)
{
	long hashkey, hashslot;
	unsigned short i;

	if (!hicfirstinit) return -1;

	hashkey = (long)picnum + ((long)palnum * MAXTILES);
	hashslot = hashkey & (HICHASHSZ-1);		// find the pointer into the hash table to start looking

	for (i = hicreplhash[hashslot]; i != 0xffff; i = hicreplnext[i]) {	// starting at the head of the hash slot,
									// seek the index we want unless we hit
									// the end
		if (hicreplidx[i] == hashkey) {		// if the index of this replacement matches that which we
			if (hicreplc[i] &&		// seek, return it
			    hicreplc[i]->skybox) {
				if (hicreplc[i]->skybox->ignore) return -1;
				return i;
			} else
				return -1;
		}
	}

	return -1;	// no replacement found
}


//
// hicinit()
//   Initialise the high-colour stuff to default.
//
void hicinit(void)
{
	long i,j;
	
	for (i=0;i<MAXPALOOKUPS;i++) {	// all tints should be 100%
		hictinting[i].r = hictinting[i].g = hictinting[i].b = 0xff;
		hictinting[i].f = 0;
	}

	for (i=hicreplcnt-1;i>=0;i--) {
		if (!hicreplc[i]) continue;
		if (hicreplc[i]->skybox) {
			for (j=5;j>=0;j--) {
				if (hicreplc[i]->skybox->face[j]) {
					free(hicreplc[i]->skybox->face[j]);
				}
			}
			free(hicreplc[i]->skybox);
		}
		if (hicreplc[i]->filename) free(hicreplc[i]->filename);
		free(hicreplc[i]);
	}
	memset(hicreplc,0,sizeof(hicreplc));
	hicreplcnt = 0;
	
	memset(hicreplhash,0xff,sizeof(hicreplhash));	// clear all hash slots
	memset(hicreplidx,0xff,sizeof(hicreplidx));	// reset all replacement hash keys

	hicfirstinit = 1;
}


//
// hicsetpalettetint(pal,r,g,b,effect)
//   The tinting values represent a mechanism for emulating the effect of global sector
//   palette shifts on true-colour textures and only true-colour textures.
//   effect bitset: 1 = greyscale, 2 = invert
//
void hicsetpalettetint(short palnum, unsigned char r, unsigned char g, unsigned char b, unsigned char effect)
{
	if (palnum < 0 || palnum >= MAXPALOOKUPS) return;
	if (!hicfirstinit) hicinit();

	hictinting[palnum].r = r;
	hictinting[palnum].g = g;
	hictinting[palnum].b = b;
	hictinting[palnum].f = effect & HICEFFECTMASK;
}


//
// hicsetsubsttex(picnum,pal,filen,sizx,sizy)
//   Specifies a replacement graphic file for an ART tile.
//
int hicsetsubsttex(short picnum, short palnum, char *filen, short centx, short centy, short tsizx, short tsizy)
{
	long hashkey, hashslot;
	unsigned short i;

	if (picnum < 0 || picnum >= MAXTILES) return -1;
	if (palnum < 0 || palnum > MAXPALOOKUPS) return -1;
	if (!hicfirstinit) hicinit();

	hashkey = (long)picnum + ((long)palnum * MAXTILES);	// unique hash key
	hashslot = hashkey & (HICHASHSZ-1);		// find the pointer into the hash table to start looking

	for (i = hicreplhash[hashslot]; i != 0xffff; i = hicreplnext[i]) {	// starting at the head of the hash slot,
									// seek the index we want unless we hit the end
		if (hicreplidx[i] == hashkey)		// if the index of this replacement matches that which we
			break;				// seek, break
	}

	if (i == 0xffff) {
		// no replacement yet defined

		if (hicreplcnt >= HICMAXREPL) return -1;	// maximum number of replacements defined

		hicreplidx[hicreplcnt] = hashkey;	// save the hash key
		hicreplnext[hicreplcnt] = hicreplhash[hashslot];	// point the next link in the chain to the current hash slot head
		hicreplhash[hashslot] = hicreplcnt;	// make ourselves the new head
		i = hicreplcnt;
		hicreplcnt++;				// bump up replacement count
	}

	// store into hicreplc the details for this replacement
	if (!hicreplc[i]) {
		hicreplc[i] = (struct hicreplc_t *)calloc(1,sizeof(struct hicreplc_t));
		if (!hicreplc[i]) return -1;
	} else {
		if (hicreplc[i]->filename) {
			free(hicreplc[i]->filename);
			hicreplc[i]->filename = NULL;
		}
	}

	hicreplc[i]->filename = strdup(filen);
	if (!hicreplc[i]->filename) {
		if (hicreplc[i]->skybox) return -1;	// don't free the base structure if there's a skybox defined
		free(hicreplc[i]);
		hicreplc[i] = NULL;
		return -1;
	}
	hicreplc[i]->ignore = 0;

	//printf("Replacement [%d,%d]: %s\n", picnum, palnum, hicreplc[i]->filename);

	return 0;
}


//
// hicsetskybox(picnum,pal,faces[6])
//   Specifies a graphic files making up a skybox.
//
int hicsetskybox(short picnum, short palnum, char *faces[6])
{
	long hashkey, hashslot;
	unsigned short i;
	long j;

	if (picnum < 0 || picnum >= MAXTILES) return -1;
	if (palnum < 0 || palnum > MAXPALOOKUPS) return -1;
	for (j=5;j>=0;j--) if (!faces[j]) return -1;
	if (!hicfirstinit) hicinit();

	hashkey = (long)picnum + ((long)palnum * MAXTILES);	// unique hash key
	hashslot = hashkey & (HICHASHSZ-1);		// find the pointer into the hash table to start looking

	for (i = hicreplhash[hashslot]; i != 0xffff; i = hicreplnext[i]) {	// starting at the head of the hash slot,
									// seek the index we want unless we hit the end
		if (hicreplidx[i] == hashkey)		// if the index of this replacement matches that which we
			break;				// seek, break
	}

	if (i == 0xffff) {
		// no replacement yet defined

		if (hicreplcnt >= HICMAXREPL) return -1;	// maximum number of replacements defined

		hicreplidx[hicreplcnt] = hashkey;	// save the hash key
		hicreplnext[hicreplcnt] = hicreplhash[hashslot];	// point the next link in the chain to the current hash slot head
		hicreplhash[hashslot] = hicreplcnt;	// make ourselves the new head
		i = hicreplcnt;
		hicreplcnt++;				// bump up replacement count
	}

	// store into hicreplc the details for this replacement
	if (!hicreplc[i]) {
		hicreplc[i] = (struct hicreplc_t *)calloc(1,sizeof(struct hicreplc_t));
		if (!hicreplc[i]) return -1;
	}

	if (!hicreplc[i]->skybox) {
		hicreplc[i]->skybox = (struct hicskybox_t *)calloc(1,sizeof(struct hicskybox_t));
		if (!hicreplc[i]->skybox) return -1;
	} else {
		for (j=5;j>=0;j--) {
			if (!hicreplc[i]->skybox->face[j]) continue;

			free(hicreplc[i]->skybox->face[j]);
			hicreplc[i]->skybox->face[j] = NULL;
		}
	}	

	// store each face's filename
	for (j=0;j<6;j++) {
		hicreplc[i]->skybox->face[j] = strdup(faces[j]);
		if (!hicreplc[i]->skybox->face[j]) {
			for (--j; j>=0; --j)	// free any previous faces
				free(hicreplc[i]->skybox->face[j]);
			free(hicreplc[i]->skybox);
			hicreplc[i]->skybox = NULL;
			return -1;
		}
	}
	hicreplc[i]->skybox->ignore = 0;

	return 0;
}


