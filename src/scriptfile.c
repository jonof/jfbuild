/*
 * File Tokeniser/Parser/Whatever
 * by Jonathon Fowler
 * Remixed completely by Ken Silverman
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "build.h"
#include "scriptfile.h"
#include "cache1d.h"
#include <math.h>


#define ISWS(x) ((x == ' ') || (x == '\t') || (x == '\r') || (x == '\n'))
static void skipoverws(scriptfile *sf) { if ((sf->textptr < sf->eof) && (!sf->textptr[0])) sf->textptr++; }
static void skipovertoken(scriptfile *sf) { while ((sf->textptr < sf->eof) && (sf->textptr[0])) sf->textptr++; }

char *scriptfile_gettoken(scriptfile *sf)
{
	char *start;

	skipoverws(sf);
	if (sf->textptr >= sf->eof) return NULL;

	start = sf->ltextptr = sf->textptr;
	skipovertoken(sf);
	return start;
}

char *scriptfile_peektoken(scriptfile *sf)
{
	scriptfile dupe;

	memcpy(&dupe, sf, sizeof(scriptfile));

	skipoverws(&dupe);
	if (dupe.textptr >= dupe.eof) return NULL;
	return dupe.textptr;
}

int scriptfile_getstring(scriptfile *sf, char **retst)
{
	(*retst) = scriptfile_gettoken(sf);
	if (*retst == NULL)
	{
		buildprintf("Error on line %s:%d: unexpected eof\n",sf->filename,scriptfile_getlinum(sf,sf->textptr));
		return(-2);
	}
	return(0);
}

static int scriptfile_getnumber_radix(scriptfile *sf, int *num, int radix)
{
	skipoverws(sf);
	if (sf->textptr >= sf->eof)
	{
		buildprintf("Error on line %s:%d: unexpected eof\n",sf->filename,scriptfile_getlinum(sf,sf->textptr));
		return -1;
	}

	while ((sf->textptr[0] == '0') && (sf->textptr[1] >= '0') && (sf->textptr[1] <= '9'))
		sf->textptr++; //hack to treat octal numbers like decimal
	
	sf->ltextptr = sf->textptr;
	(*num) = (int)strtol((const char *)sf->textptr,&sf->textptr,radix);
	if (!ISWS(*sf->textptr) && *sf->textptr) {
		char *p = sf->textptr;
		skipovertoken(sf);
		buildprintf("Error on line %s:%d: expecting int, got \"%s\"\n",sf->filename,scriptfile_getlinum(sf,sf->ltextptr),p);
		return -2;
	}
	return 0;
}

int scriptfile_getnumber(scriptfile *sf, int *num)
{
	return scriptfile_getnumber_radix(sf, num, 0);
}

int scriptfile_gethex(scriptfile *sf, int *num)
{
	return scriptfile_getnumber_radix(sf, num, 16);
}

static double parsedouble(char *ptr, char **end)
{
	int beforedecimal = 1, negative = 0, dig;
	int exposgn = 0, expo = 0;
	double num = 0.0, decpl = 0.1;
	char *p;
	
	p = ptr;
	if (*p == '-') negative = 1, p++;
	else if (*p == '+') p++;
	for (;; p++) {
		if (*p >= '0' && *p <= '9') {
			dig = *p - '0';
			if (beforedecimal) num = num * 10.0 + dig;
			else if (exposgn) expo = expo*10 + dig;
			else {
				num += (double)dig * decpl;
				decpl /= 10.0;
			}
		} else if (*p == '.') {
			if (beforedecimal) beforedecimal = 0;
			else break;
		} else if ((*p == 'E') || (*p == 'e')) {
			exposgn = 1;
			if (p[1] == '-') { exposgn = -1; p++; }
			else if (p[1] == '+') p++;
		} else break;
	}
	
	if (end) *end = p;
	if (exposgn) num *= pow(10.0,(double)(expo*exposgn));
	return negative ? -num : num;
}

int scriptfile_getdouble(scriptfile *sf, double *num)
{
	skipoverws(sf);
	if (sf->textptr >= sf->eof)
	{
		buildprintf("Error on line %s:%d: unexpected eof\n",sf->filename,scriptfile_getlinum(sf,sf->textptr));
		return -1;
	}
	
	sf->ltextptr = sf->textptr;

	// On Linux, locale settings interfere with interpreting x.y format numbers
	//(*num) = strtod((const char *)sf->textptr,&sf->textptr);
	(*num) = parsedouble(sf->textptr, &sf->textptr);
	
	if (!ISWS(*sf->textptr) && *sf->textptr) {
		char *p = sf->textptr;
		skipovertoken(sf);
		buildprintf("Error on line %s:%d: expecting float, got \"%s\"\n",sf->filename,scriptfile_getlinum(sf,sf->ltextptr),p);
		return -2;
	}
	return 0;
}

int scriptfile_getsymbol(scriptfile *sf, int *num)
{
	char *t, *e;
	int v;

	t = scriptfile_gettoken(sf);
	if (!t) return -1;

	v = (int)strtol(t, &e, 10);
	if (*e) {
		// looks like a string, so find it in the symbol table
		if (scriptfile_getsymbolvalue(t, num)) return 0;
		buildprintf("Error on line %s:%d: expecting symbol, got \"%s\"\n",sf->filename,scriptfile_getlinum(sf,sf->ltextptr),t);
		return -2;   // not found
	}

	*num = v;
	return 0;
}

int scriptfile_getbraces(scriptfile *sf, char **braceend)
{
	int bracecnt;
	char *bracestart;

	skipoverws(sf);
	if (sf->textptr >= sf->eof)
	{
		buildprintf("Error on line %s:%d: unexpected eof\n",sf->filename,scriptfile_getlinum(sf,sf->textptr));
		return -1;
	}

	if (sf->textptr[0] != '{') {
		buildprintf("Error on line %s:%d: expecting '{'\n",sf->filename,scriptfile_getlinum(sf,sf->textptr));
		return -1;
	}
	bracestart = ++sf->textptr; bracecnt = 1;
	while (1)
	{
		if (sf->textptr >= sf->eof) return(0);
		if (sf->textptr[0] == '{') bracecnt++;
		if (sf->textptr[0] == '}') { bracecnt--; if (!bracecnt) break; }
		sf->textptr++;
	}
	(*braceend) = sf->textptr;
	sf->textptr = bracestart;
	return 0;
}


int scriptfile_getlinum (scriptfile *sf, char *ptr)
{
	int i, stp;
	ptrdiff_t ind;

	//for(i=0;i<sf->linenum;i++) if (sf->lineoffs[i] >= ind) return(i+1); //brute force algo

	ind = ((intptr_t)ptr) - ((intptr_t)sf->textbuf);

	for(stp=1;stp+stp<sf->linenum;stp+=stp); //stp = highest power of 2 less than sf->linenum
	for(i=0;stp;stp>>=1)
		if ((i+stp < sf->linenum) && (sf->lineoffs[i+stp] < ind)) i += stp;
	return(i+1); //i = index to highest lineoffs which is less than ind; convert to 1-based line numbers
}

void scriptfile_preparse (scriptfile *sf, char *tx, size_t flen)
{
	int cr, numcr, nflen, ws, cs, inquote;
	size_t i;

		//Count number of lines
	numcr = 1;
	for(i=0;i<flen;i++)
	{
			//detect all 4 types of carriage return (\r, \n, \r\n, \n\r :)
		cr=0;if (tx[i] == '\r') { i += (tx[i+1] == '\n'); cr = 1; }
		else if (tx[i] == '\n') { i += (tx[i+1] == '\r'); cr = 1; }
		if (cr) { numcr++; continue; }
	}

	sf->linenum = numcr;
	sf->lineoffs = (int *)malloc(sf->linenum*sizeof(int));

		//Preprocess file for comments (// and /*...*/, and convert all whitespace to single spaces)
	nflen = 0; ws = 0; cs = 0; numcr = 0; inquote = 0;
	for(i=0;i<flen;i++)
	{
			//detect all 4 types of carriage return (\r, \n, \r\n, \n\r :)
		cr=0;if (tx[i] == '\r') { i += (tx[i+1] == '\n'); cr = 1; }
		else if (tx[i] == '\n') { i += (tx[i+1] == '\r'); cr = 1; }
		if (cr)
		{
				//Remember line numbers by storing the byte index at the start of each line
				//Line numbers can be retrieved by doing a binary search on the byte index :)
			sf->lineoffs[numcr++] = nflen;
			if (cs == 1) cs = 0;
			ws = 1; continue; //strip CR/LF
		}

		if ((!inquote) && ((tx[i] == ' ') || (tx[i] == '\t'))) { ws = 1; continue; } //strip Space/Tab
		if ((tx[i] == ';') && (!cs)) cs = 1;	// ; comment
		if ((tx[i] == '/') && (tx[i+1] == '/') && (!cs)) cs = 1;
		if ((tx[i] == '/') && (tx[i+1] == '*') && (!cs)) { ws = 1; cs = 2; }
		if ((tx[i] == '*') && (tx[i+1] == '/') && (cs == 2)) { cs = 0; i++; continue; }
		if (cs) continue;

		if (ws) { tx[nflen++] = 0; ws = 0; }

			//quotes inside strings: \"
		if ((tx[i] == '\\') && (tx[i+1] == '\"')) { i++; tx[nflen++] = '\"'; continue; }
		if (tx[i] == '\"') { inquote ^= 1; continue; }
		tx[nflen++] = tx[i];
	}
	tx[nflen++] = 0; sf->lineoffs[numcr] = nflen;
	tx[nflen++] = 0;

#if 0
		//for debugging only:
	printf("pre-parsed file:flen=%d,nflen=%d\n",flen,nflen);
	for(i=0;i<nflen;i++) { if (tx[i] < 32) printf("_"); else printf("%c",tx[i]); }
	printf("[eof]\nnumlines=%d\n",sf->linenum);
	for(i=0;i<sf->linenum;i++) printf("line %d = byte %d\n",i,sf->lineoffs[i]);
#endif
	flen = nflen;

	sf->textbuf = sf->textptr = tx;
	sf->textlength = nflen;
	sf->eof = &sf->textbuf[nflen-1];
}

scriptfile *scriptfile_fromfile(const char *fn)
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
	tx[flen] = tx[flen+1] = 0;

	kclose(fp);

	scriptfile_preparse(sf,tx,flen);
	sf->filename = strdup(fn);

	return sf;
}

scriptfile *scriptfile_fromstring(const char *string)
{
	scriptfile *sf;
	char *tx;
	size_t flen;

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
	tx[flen] = tx[flen+1] = 0;

	scriptfile_preparse(sf,tx,flen);
	sf->filename = NULL;

	return sf;
}

void scriptfile_close(scriptfile *sf)
{
	if (!sf) return;
	if (sf->lineoffs) free(sf->lineoffs);
	if (sf->textbuf) free(sf->textbuf);
	if (sf->filename) free(sf->filename);
	sf->textbuf = NULL;
	sf->filename = NULL;
	free(sf);
}

int scriptfile_eof(scriptfile *sf)
{
	skipoverws(sf);
	if (sf->textptr >= sf->eof) return 1;
	return 0;
}

#define SYMBTABSTARTSIZE 256
static size_t symbtablength=0, symbtaballoclength=0;
static char *symbtab = NULL;

static char * getsymbtabspace(size_t reqd)
{
	char *pos,*np;
	size_t i;

	if (symbtablength + reqd > symbtaballoclength)
	{
		for(i=max(symbtaballoclength,SYMBTABSTARTSIZE);symbtablength+reqd>i;i<<=1);
		np = (char *)realloc(symbtab, i); if (!np) return NULL;
		symbtab = np; symbtaballoclength = i;
	}

	pos = &symbtab[symbtablength];
	symbtablength += reqd;
	return pos;
}

int scriptfile_getsymbolvalue(const char *name, int *val)
{
	char *scanner = symbtab;

	if (!symbtab) return 0;
	while (scanner - symbtab < (ptrdiff_t)symbtablength) {
		if (!strcasecmp(name, scanner)) {
			*val = *(int*)(scanner + strlen(scanner) + 1);
			return 1;
		}

		scanner += strlen(scanner) + 1 + sizeof(int);
	}

	return 0;
}

int scriptfile_addsymbolvalue(const char *name, int val)
{
	//int x;
	char *sp;
	//if (scriptfile_getsymbolvalue(name, &x)) return -1;   // already exists

	if (symbtab) {
		char *scanner = symbtab;
		while (scanner - symbtab < (ptrdiff_t)symbtablength) {
			if (!strcasecmp(name, scanner)) {
				*(int*)(scanner + strlen(scanner) + 1) = val;
				return 1;
			}

			scanner += strlen(scanner) + 1 + sizeof(int);
		}
	}
	
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
