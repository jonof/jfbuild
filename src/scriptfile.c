/*
 * File Tokeniser/Parser/Whatever
 * by Jonathon Fowler
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "scriptfile.h"
#include "compat.h"
#include "cache1d.h"


#define ISWS(x) ((x == ' ') || (x == '\t') || (x == '\r') || (x == '\n'))

static void skipoverws(scriptfile *sf) {
	while (sf->textptr - sf->textbuf < sf->textlength) {
		if (ISWS(*sf->textptr)) {
			if (*sf->textptr == '\n') sf->linenum++;
			sf->textptr++;
		} else if (sf->textptr[0] == '/' && sf->textptr[1] == '/') {
			sf->textptr += 2;
			while (*sf->textptr != '\n' && (sf->textptr - sf->textbuf < sf->textlength))
				sf->textptr++;
		} else if (sf->textptr[0] == '/' && sf->textptr[1] == '*') {
			sf->textptr += 2;
			while (sf->textptr[0] != '*' && sf->textptr[1] != '/' &&
				 (sf->textptr - sf->textbuf < sf->textlength)) {
				if (*sf->textptr == '\n') sf->linenum++;
				sf->textptr++;
			}
			sf->textptr += 2;
		} else {
			break;
		}
	}
}
static void skipovertoken(scriptfile *sf) {
	while (sf->textptr - sf->textbuf < sf->textlength && !ISWS(*sf->textptr)) sf->textptr++;
}

char *scriptfile_gettoken(scriptfile *sf)
{
	char *start;

	skipoverws(sf);
	if (sf->textptr - sf->textbuf >= sf->textlength) return NULL;   // eof

	start = sf->textptr;
	while (sf->textptr - sf->textbuf < sf->textlength) {
		if (ISWS(*sf->textptr)) {
			if (*sf->textptr == '\n') sf->linenum++;
			break;
		}
		sf->textptr++;
	}
	*(sf->textptr++) = 0;

	return start;
}

int scriptfile_getnumber(scriptfile *sf, int *num)
{
	skipoverws(sf);
	if (sf->textptr - sf->textbuf >= sf->textlength) return -1;   // eof
	(*num) = strtol((const char *)sf->textptr,&sf->textptr,0);
	if (!ISWS(*sf->textptr) && *sf->textptr) { skipovertoken(sf); return -2; }
	return 0;
}

int scriptfile_getdouble(scriptfile *sf, double *num)
{
	skipoverws(sf);
	if (sf->textptr - sf->textbuf >= sf->textlength) return -1;   // eof
	(*num) = strtod((const char *)sf->textptr,&sf->textptr);
	if (!ISWS(*sf->textptr) && *sf->textptr) { skipovertoken(sf); return -2; }
	return 0;
}

int scriptfile_getsymbol(scriptfile *sf, int *num)
{
	char *t, *e;
	int v;

	t = scriptfile_gettoken(sf);
	if (!t) return -1;

	v = Bstrtol(t, &e, 10);
	if (*e) {
		// looks like a string, so find it in the symbol table
		if (scriptfile_getsymbolvalue(t, num)) return 0;
		return -2;   // not found
	}

	*num = v;
	return 0;
}

char *scriptfile_getstring(scriptfile *sf)
{
	char *start = NULL, *gulper = NULL;
	char insidequotes = 0, swallownext = 0;

	skipoverws(sf);
	if (sf->textptr - sf->textbuf >= sf->textlength) return NULL;   // eof

	if (sf->textptr[0] == '"') {
		sf->textptr++;
		insidequotes = 1;
	}
	start = gulper = sf->textptr;

	while (sf->textptr - sf->textbuf < sf->textlength) {
		if (*sf->textptr == '\n') sf->linenum++;
		if (!insidequotes && ISWS(*sf->textptr) && !swallownext) {
			*gulper = 0;
			sf->textptr++;
			break;   // whitespace delimited bare string ends
		}

		if (*sf->textptr == '\\' && !swallownext) {
			swallownext = 1;
			sf->textptr++;
			continue;
		}

		if (*sf->textptr == '"' && !swallownext && insidequotes) {
			*gulper = 0;
			sf->textptr++;
			break;   // quoted string ends
		}

		*(gulper++) = *(sf->textptr++);
		swallownext = 0;
	}

	return start;
}

scriptfile *scriptfile_fromfile(char *fn)
{
	int fp;
	scriptfile *sf;
	char *tx;
	unsigned int flen;

	fp = kopen4load(fn,0);
	if (fp<0) return NULL;

	flen = kfilelength(fp);

	tx = (char *) malloc(flen + 2);
	if (!tx) {
		kclose(fp);
		return NULL;
	}

	sf = (scriptfile*) malloc(sizeof(scriptfile));
	if (!sf) {
		kclose(fp);
		free(tx);
		return NULL;
	}

	kread(fp, tx, flen);
	tx[flen] = 0;
	tx[flen+1] = 0;

	kclose(fp);

	sf->textbuf = tx;
	sf->textlength = flen;
	sf->textptr = tx;
	sf->filename = strdup(fn);
	sf->linenum = 0;

	return sf;
}

scriptfile *scriptfile_fromstring(char *string)
{
	scriptfile *sf;
	char *tx;
	unsigned int flen;

	if (!string) return NULL;

	flen = strlen(string);

	tx = (char *) malloc(flen + 2);
	if (!tx) return NULL;

	sf = (scriptfile*) malloc(sizeof(scriptfile));
	if (!sf) {
		free(tx);
		return NULL;
	}

	memcpy(tx, string, flen);
	tx[flen] = 0;
	tx[flen+1] = 0;

	sf->textbuf = tx;
	sf->textlength = flen;
	sf->textptr = tx;
	sf->filename = NULL;
	sf->linenum = 0;

	return sf;
}

void scriptfile_close(scriptfile *sf)
{
	if (!sf) return;
	if (sf->textbuf) free(sf->textbuf);
	if (sf->filename) free(sf->filename);
	sf->textbuf = NULL;
	sf->filename = NULL;
	free(sf);
}


#define SYMBTABSTARTSIZE 256
static int symbtablength=0, symbtaballoclength=0;
static char *symbtab = NULL;

static char * getsymbtabspace(int reqd)
{
	char *pos,*np;
	if (!symbtab) {
		int newsize = SYMBTABSTARTSIZE;
		while (newsize < reqd) newsize *= 2;

		symbtab = (char *)malloc(newsize);
		if (!symbtab) return NULL;
		symbtaballoclength = newsize;
		symbtablength = reqd;
		return symbtab;
	}
	if (symbtablength + reqd > symbtaballoclength) {
		int newsize = symbtaballoclength * 2;
		while (newsize < symbtablength + reqd) newsize *= 2;

		np = (char *)realloc(symbtab, newsize);
		if (!np) return NULL;
		symbtab = np;
		symbtaballoclength = newsize;
	}
	pos = symbtab + symbtablength;
	symbtablength += reqd;
	return pos;
}

int scriptfile_getsymbolvalue(char *name, int *val)
{
	char *scanner = symbtab;

	if (!symbtab) return 0;
	while (scanner - symbtab < symbtablength) {
		if (!Bstrcasecmp(name, scanner)) {
			*val = *(int*)(scanner + strlen(scanner) + 1);
			return 1;
		}

		scanner += strlen(scanner) + 1 + sizeof(int);
	}

	return 0;
}

int scriptfile_addsymbolvalue(char *name, int val)
{
	int x;
	char *sp;
	if (scriptfile_getsymbolvalue(name, &x)) return -1;   // already exists
	sp = getsymbtabspace(strlen(name) + 1 + sizeof(int));
	if (!sp) return 0;
	strcpy(sp, name);
	sp += strlen(name)+1;
	*(int*)sp = val;
	return 1;   // added
}

void scriptfile_clearsymbols(void)
{
	if (symbtab) free(symbtab);
	symbtab = NULL;
	symbtablength = 0;
	symbtaballoclength = 0;
}
