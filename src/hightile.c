/*
 * High-colour textures support for Polymost
 * by Jonathon Fowler
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "kplib.h"

static palette_t hictinting[MAXPALOOKUPS];

#define HICHASHSZ 1024		// hash size
#define HICMAXREPL MAXTILES	// maximum number of replacement tiles
static unsigned short hicreplhash[HICHASHSZ];
static unsigned short hicreplnext[HICMAXREPL];	// index of next replacement in the hash chain
static long hicreplidx[HICMAXREPL];		// hash keys for replacements
static struct hicreplc_t {
	char *filename, ignore;
	short centx, centy;
	unsigned short sizx, sizy;
	short tsizx, tsizy;
	double scalex, scaley;
} *hicreplc[HICMAXREPL];
static unsigned short hicreplcnt = 0;		// number of replacements currently recorded
static char hicfirstinit = 0;

static char *hicreplcnames = 0, *hicreplcnamesnext = 0;
static long hicreplcnameslen = 0;

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


//
// hicinit()
//   Initialise the high-colour stuff to default.
//
void hicinit(void)
{
	long i;
	
	for (i=0;i<MAXPALOOKUPS;i++) {	// all tints should be 100%
		hictinting[i].r = hictinting[i].g = hictinting[i].b = 0xff;
		hictinting[i].f = 0;
	}
	
	if (hicreplcnames) {
		free(hicreplcnames);
		hicreplcnames = 0;
		hicreplcnamesnext = 0;
		hicreplcnameslen = 0;
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
	hictinting[palnum].f = effect;
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

	// check out the texture to see if it's kosher before we do anything more
	
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
		long ofs, newlen, j;
		char *newnames;

		ofs = (long)(hicreplcnamesnext - hicreplcnames);
		if (ofs + sizeof(struct hicreplc_t) + Bstrlen(filen)+1 > hicreplcnameslen) {
			if (hicreplcnameslen == 0) newlen = 16 * (sizeof(struct hicreplc_t) + 260);
			else newlen = hicreplcnameslen * 2;
			
			newnames = (char *)realloc(hicreplcnames,newlen);
			if (!newnames) return -1;

			// correct the new location of hicreplcnames
			for (j=0; j<hicreplcnt; j++) {
				if (j==i) continue;
				hicreplc[j] = (struct hicreplc_t *)(newnames + ((char*)hicreplc[j] - hicreplcnames));
				hicreplc[j]->filename = newnames + (hicreplc[j]->filename - hicreplcnames);
			}

			hicreplcnameslen = newlen;
			hicreplcnames = newnames;
			hicreplcnamesnext = newnames + ofs;
		}
		
		// "allocate" entry
		hicreplc[i] = (struct hicreplc_t *)hicreplcnamesnext;
		hicreplcnamesnext += sizeof(struct hicreplc_t);
		hicreplc[i]->filename = hicreplcnamesnext;
		hicreplcnamesnext += Bstrlen(filen) + 1;
	} else {
		if (Bstrlen(filen) > Bstrlen(hicreplc[i]->filename)) {
			// new name won't fit, so allocate it again
			hicreplc[i]->filename = hicreplcnamesnext;
			hicreplcnamesnext += Bstrlen(filen) + 1;
		}
	}

	strcpy(hicreplc[i]->filename, filen);
	hicreplc[i]->ignore = 0;
	hicreplc[i]->centx = centx;
	hicreplc[i]->centy = centy;
	hicreplc[i]->tsizx = tsizx;
	hicreplc[i]->tsizy = tsizy;

	printf("Replacement [%d,%d]: %s\n", picnum, palnum, hicreplc[i]->filename);

	return 0;
}


