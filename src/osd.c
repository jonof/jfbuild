// On-screen Display (ie. console)
// for the Build Engine
// by Jonathon Fowler (jonof@edgenetwk.com)

#include "build.h"
#include "osd.h"
#include "compat.h"
#include "baselayer.h"


typedef struct _symbol {
	const char *name;
	struct _symbol *next, *nexttype;

	int type;
	union {
		struct {
			const char *help;
			int (*func)(const osdfuncparm_t *);
		} func;
		struct {
			int         type;
			void       *var;
			int         extra;
			int (*validator)(void *);
		} var;
	} i;
} symbol_t;

#define SYMBTYPE_FUNC 0
#define SYMBTYPE_VAR  1

static symbol_t *symbols = NULL, *symbols_func = NULL, *symbols_var = NULL;
static symbol_t *addnewsymbol(int type, const char *name);
static symbol_t *findsymbol(const char *name, symbol_t *startingat);
static symbol_t *findexactsymbol(const char *name);

static int _validate_osdlines(void *);

static int _internal_osdfunc_listsymbols(const osdfuncparm_t *);
static int _internal_osdfunc_help(const osdfuncparm_t *);
static int _internal_osdfunc_dumpbuildinfo(const osdfuncparm_t *);
static int _internal_osdfunc_setrendermode(const osdfuncparm_t *);

static int white=-1;			// colour of white (used by default display routines)
static void _internal_drawosdchar(int, int, char, int, int);
static void _internal_drawosdstr(int, int, char*, int, int, int);
static void _internal_drawosdcursor(int,int,int,int);
static int _internal_getcolumnwidth(int);
static int _internal_getrowheight(int);
static void _internal_clearbackground(int,int);
static int _internal_gettime(void);
static void _internal_onshowosd(int);

#define TEXTSIZE 16384

// history display
static char osdtext[TEXTSIZE];
static int  osdpos=0;			// position next character will be written at
static int  osdlines=1;			// # lines of text in the buffer
static int  osdrows=20;			// # lines of the buffer that are visible
static int  osdcols=60;			// width of onscreen display in text columns
static int  osdmaxrows=20;		// maximum number of lines which can fit on the screen
static int  osdmaxlines=TEXTSIZE/60;	// maximum lines which can fit in the buffer
static char osdvisible=0;		// onscreen display visible?
static int  osdhead=0; 			// topmost visible line number
static BFILE *osdlog=NULL;		// log filehandle
static char osdinited=0;		// text buffer initialised?
static int  osdkey=0x45;		// numlock shows the osd
static int  keytime=0;

// command prompt editing
#define EDITLENGTH 512
static int  osdovertype=0;		// insert (0) or overtype (1)
static char osdeditbuf[EDITLENGTH+1];	// editing buffer
static char osdedittmp[EDITLENGTH+1];	// editing buffer temporary workspace
static int  osdeditlen=0;		// length of characters in edit buffer
static int  osdeditcursor=0;		// position of cursor in edit buffer
static int  osdeditshift=0;		// shift state
static int  osdeditcontrol=0;		// control state
static int  osdeditcaps=0;		// capslock
static int  osdeditwinstart=0;
static int  osdeditwinend=60-1-3;
#define editlinewidth (osdcols-1-3)

// command processing
#define HISTORYDEPTH 16
static int  osdhistorypos=-1;		// position we are at in the history buffer
static int  osdhistorybuf[HISTORYDEPTH][EDITLENGTH+1];	// history strings
static int  osdhistorysize=0;		// number of entries in history

// execution buffer
// the execution buffer works from the command history
static int  osdexeccount=0;		// number of lines from the head of the history buffer to execute

// presentation parameters
static int  osdpromptshade=0;
static int  osdpromptpal=0;
static int  osdeditshade=0;
static int  osdeditpal=0;
static int  osdtextshade=0;
static int  osdtextpal=0;
static int  osdcursorshade=0;
static int  osdcursorpal=0;

// application callbacks
static void (*drawosdchar)(int, int, char, int, int) = _internal_drawosdchar;
static void (*drawosdstr)(int, int, char*, int, int, int) = _internal_drawosdstr;
static void (*drawosdcursor)(int, int, int, int) = _internal_drawosdcursor;
static int (*getcolumnwidth)(int) = _internal_getcolumnwidth;
static int (*getrowheight)(int) = _internal_getrowheight;
static void (*clearbackground)(int,int) = _internal_clearbackground;
static int (*gettime)(void) = _internal_gettime;
static void (*onshowosd)(int) = _internal_onshowosd;


// translation table for turning scancode into ascii characters
/*
static char sctoasc[2][256] = {
	{
//      0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
	0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8,   9,   // 0x00
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 13,  0,   'a', 's', // 0x10
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`', 0,   '\\','z', 'x', 'c', 'v', // 0x20
	'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   // 0x30
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   // 0x40
	0,   0,   0,   '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x50
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x60
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x70
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x80
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   13,  0,   0,   0,   // 0x90
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xa0
	0,   0,   0,   0,   0,   '/', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xb0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xc0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xd0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xe0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    // 0xf0
	},
	{
//      0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
	0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 8,   9,   // 0x00
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 13,  0,   'A', 'S', // 0x10
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V', // 0x20
	'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   // 0x30
	0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1', // 0x40
	'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x50
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x60
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x70
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x80
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   13,  0,   0,   0,   // 0x90
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xa0
	0,   0,   0,   0,   0,   '/', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xb0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xc0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xd0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xe0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    // 0xf0
	}
};
*/


static void _internal_drawosdchar(int x, int y, char ch, int shade, int pal)
{
	int i,j,k;
	char st[2] = { 0,0 };

	st[0] = ch;

	if (white<0) {
		// find the palette index closest to white
		k=0;
		for(i=0;i<256;i++)
		{
			j = ((int)palette[i*3])+((int)palette[i*3+1])+((int)palette[i*3+2]);
			if (j > k) { k = j; white = i; }
		}
	}

	printext256(4+(x<<3),4+(y<<3), white, -1, st, 0);
}

static void _internal_drawosdstr(int x, int y, char *ch, int len, int shade, int pal)
{
	int i,j,k;
	char st[1024];

	if (len>1023) len=1023;
	memcpy(st,ch,len);
	st[len]=0;
	
	if (white<0) {
		// find the palette index closest to white
		k=0;
		for(i=0;i<256;i++)
		{
			j = ((int)palette[i*3])+((int)palette[i*3+1])+((int)palette[i*3+2]);
			if (j > k) { k = j; white = i; }
		}
	}

	printext256(4+(x<<3),4+(y<<3), white, -1, st, 0);
}

static void _internal_drawosdcursor(int x, int y, int type, int lastkeypress)
{
	int i,j,k;
	char st[2] = { '_',0 };

	if (type) st[0] = '#';

	if (white<0) {
		// find the palette index closest to white
		k=0;
		for(i=0;i<256;i++)
		{
			j = ((int)palette[i*3])+((int)palette[i*3+1])+((int)palette[i*3+2]);
			if (j > k) { k = j; white = i; }
		}
	}

	printext256(4+(x<<3),4+(y<<3)+2, white, -1, st, 0);
}

static int _internal_getcolumnwidth(int w)
{
	return w/8 - 1;
}

static int _internal_getrowheight(int w)
{
	return w/8;
}

static void _internal_clearbackground(int cols, int rows)
{
}

static int _internal_gettime(void)
{
	return 0;
}

static void _internal_onshowosd(int a)
{
}

////////////////////////////

int osd_internal_validate_string(void *a)
{
	return 0;	// no problems
}

int osd_internal_validate_integer(void *a)
{
	return 0;	// no problems
}

int osd_internal_validate_boolean(void *a)
{
	long *l = (long *)a;
	if (*l) *l = 1;	// anything nonzero should be 1
	return 0;
}

static int _validate_osdlines(void *a)
{
	int *v = (int *)a;

	if (*v < 1) {
		*v = 1;
		return 1;
	}
	if (*v > osdmaxrows) {
		*v = osdmaxrows;
		return 1;
	}

	return 0;	
}

static int _internal_osdfunc_listsymbols(const osdfuncparm_t *parm)
{
	symbol_t *i;

	OSD_Printf("Symbol listing\n  Functions:\n");
	for (i=symbols_func; i!=NULL; i=i->nexttype)
		OSD_Printf("     %s\n", i->name);
	
	OSD_Printf("\n  Variables:\n");
	for (i=symbols_var; i!=NULL; i=i->nexttype)
		OSD_Printf("     %s\n", i->name);

	return OSDCMD_OK;
}

static int _internal_osdfunc_help(const osdfuncparm_t *parm)
{
	symbol_t *symb;

	if (parm->numparms != 1) return OSDCMD_SHOWHELP;
	symb = findexactsymbol(parm->parms[0]);
	if (!symb) {
		OSD_Printf("Help Error: \"%s\" is not a defined variable or function\n", parm->parms[0]);
	} else {
		if (symb->type == SYMBTYPE_FUNC) {
			OSD_Printf("%s\n", symb->i.func.help);
		} else {
			OSD_Printf("\"%s\" is a", symb->name);
			switch (symb->i.var.type) {
				case OSDVAR_INTEGER: OSD_Printf("n integer"); break;
				case OSDVAR_STRING:  OSD_Printf(" string"); break;
			}
			OSD_Printf(" variable\n");
		}
	}
	
	return OSDCMD_OK;
}



////////////////////////////


//
// OSD_Cleanup() -- Cleans up the on-screen display
//
void OSD_Cleanup(void)
{
	symbol_t *s;

	for (; symbols; symbols=s) {
		s=symbols->next;
		Bfree(symbols);
	}

	osdinited=0;
}


//
// OSD_Init() -- Initialises the on-screen display
//
void OSD_Init(void)
{
	Bmemset(osdtext, 32, TEXTSIZE);
	osdlines=1;

	osdinited=1;

	OSD_RegisterFunction("listsymbols","listsymbols: lists all the recognized symbols",_internal_osdfunc_listsymbols);
	OSD_RegisterFunction("help","help: displays help on the named symbol",_internal_osdfunc_help);
	OSD_RegisterVariable("osdrows", OSDVAR_INTEGER, &osdrows, 0, _validate_osdlines);

	atexit(OSD_Cleanup);
}


//
// OSD_SetLogFile() -- Sets the text file where printed text should be echoed
//
void OSD_SetLogFile(char *fn)
{
	if (osdlog) Bfclose(osdlog);
	osdlog = NULL;
	if (fn) osdlog = Bfopen(fn,"w");
	if (osdlog) setvbuf(osdlog, (char*)NULL, _IONBF, 0);
}


//
// OSD_SetFunctions() -- Sets some callbacks which the OSD uses to understand its world
//
void OSD_SetFunctions(
		void (*drawchar)(int,int,char,int,int),
		void (*drawstr)(int,int,char*,int,int,int),
		void (*drawcursor)(int,int,int,int),
		int (*colwidth)(int),
		int (*rowheight)(int),
		void (*clearbg)(int,int),
		int (*gtime)(void),
		void (*showosd)(int)
	)
{
	drawosdchar = drawchar;
	drawosdstr = drawstr;
	drawosdcursor = drawcursor;
	getcolumnwidth = colwidth;
	getrowheight = rowheight;
	clearbackground = clearbg;
	gettime = gtime;
	onshowosd = showosd;

	if (!drawosdchar) drawosdchar = _internal_drawosdchar;
	if (!drawosdstr) drawosdstr = _internal_drawosdstr;
	if (!drawosdcursor) drawosdcursor = _internal_drawosdcursor;
	if (!getcolumnwidth) getcolumnwidth = _internal_getcolumnwidth;
	if (!getrowheight) getrowheight = _internal_getrowheight;
	if (!clearbackground) clearbackground = _internal_clearbackground;
	if (!gettime) gettime = _internal_gettime;
	if (!onshowosd) onshowosd = _internal_onshowosd;
}


//
// OSD_SetParameters() -- Sets the parameters for presenting the text
//
void OSD_SetParameters(
		int promptshade, int promptpal,
		int editshade, int editpal,
		int textshade, int textpal
	)
{
	osdpromptshade = promptshade;
	osdpromptpal   = promptpal;
	osdeditshade   = editshade;
	osdeditpal     = editpal;
	osdtextshade   = textshade;
	osdtextpal     = textpal;
}


//
// OSD_CaptureKey() -- Sets the scancode for the key which activates the onscreen display
//
void OSD_CaptureKey(int sc)
{
	osdkey = sc;
}


//
// OSD_HandleKey() -- Handles keyboard input when capturing input.
// 	Returns 0 if the key was handled internally, or the scancode if it should
// 	be passed on to the game.
//
int OSD_HandleKey(int sc, int press)
{
	char ch;
	int i,j;
	symbol_t *tabc = NULL;
	static symbol_t *lastmatch = NULL;
	
	if (!osdinited) return sc;

	if (sc == osdkey) {
		if (press) {
			OSD_ShowDisplay(osdvisible ^ 1);
			bflushchars();
		}
		return 0;//sc;
	} else if (!osdvisible) {
		return sc;
	}

	if (!press) {
		if (sc == 42 || sc == 54) // shift
			osdeditshift = 0;
		if (sc == 29 || sc == 157)	// control
			osdeditcontrol = 0;
		return 0;//sc;
	}

	keytime = gettime();

	if (sc != 15) lastmatch = NULL;		// tab

	while ( (ch = bgetchar()) ) {
		if (ch == 1) {	// control a. jump to beginning of line
		} else if (ch == 2) {	// control b, move one character left
		} else if (ch == 5) {	// control e, jump to end of line
		} else if (ch == 6) {	// control f, move one character right
		} else if (ch == 8) {	// control h, backspace
			if (!osdeditcursor || !osdeditlen) return 0;
			if (!osdovertype) {
				if (osdeditcursor < osdeditlen)
					Bmemmove(osdeditbuf+osdeditcursor-1, osdeditbuf+osdeditcursor, osdeditlen-osdeditcursor);
				osdeditlen--;
			}
			osdeditcursor--;
			if (osdeditcursor<osdeditwinstart) osdeditwinstart--,osdeditwinend--;
		} else if (ch == 9) {	// tab
			if (!lastmatch) {
				for (i=osdeditcursor;i>0;i--) if (osdeditbuf[i-1] == ' ') break;
				for (j=0;osdeditbuf[i] != ' ' && i < osdeditlen;j++,i++)
					osdedittmp[j] = osdeditbuf[i];
				osdedittmp[j] = 0;

				if (j > 0)
					tabc = findsymbol(osdedittmp, NULL);
			} else {
				tabc = findsymbol(osdedittmp, lastmatch->next);
				if (!tabc && lastmatch)
					tabc = findsymbol(osdedittmp, NULL);	// wrap
			}

			if (tabc) {
				for (i=osdeditcursor;i>0;i--) if (osdeditbuf[i-1] == ' ') break;
				osdeditlen = i;
				for (j=0;tabc->name[j] && osdeditlen <= EDITLENGTH;i++,j++,osdeditlen++)
					osdeditbuf[i] = tabc->name[j];
				osdeditcursor = osdeditlen;
				osdeditwinend = osdeditcursor;
				osdeditwinstart = osdeditwinend-editlinewidth;
				if (osdeditwinstart<0) {
					osdeditwinstart=0;
					osdeditwinend = editlinewidth;
				}
			
				lastmatch = tabc;
			}
		} else if (ch == 11) {	// control k, delete all to end of line
		} else if (ch == 12) {	// control l, clear screen
		} else if (ch == 13) {	// control m, enter
			if (osdeditlen>0) {
				osdeditbuf[osdeditlen] = 0;
				Bmemmove(osdhistorybuf[1], osdhistorybuf[0], HISTORYDEPTH*(EDITLENGTH+1));
				Bmemmove(osdhistorybuf[0], osdeditbuf, EDITLENGTH+1);
				if (osdhistorysize < HISTORYDEPTH) osdhistorysize++;
				if (osdexeccount == HISTORYDEPTH)
					OSD_Printf("Command Buffer Warning: Failed queueing command "
							"for execution. Buffer full.\n");
				else
					osdexeccount++;
				osdhistorypos=-1;
			}

			osdeditlen=0;
			osdeditcursor=0;
			osdeditwinstart=0;
			osdeditwinend=editlinewidth;
		} else if (ch == 16) {	// control p, previous (ie. up arrow)
		} else if (ch == 20) {	// control t, swap previous two chars
		} else if (ch == 21) {	// control u, delete all to beginning
			if (osdeditcursor>0 && osdeditlen) {
				if (osdeditcursor<osdeditlen)
					Bmemmove(osdeditbuf, osdeditbuf+osdeditcursor, osdeditlen-osdeditcursor);
				osdeditlen-=osdeditcursor;
				osdeditcursor = 0;
				osdeditwinstart = 0;
				osdeditwinend = editlinewidth;
			}
		} else if (ch == 23) {	// control w, delete one word back
			if (osdeditcursor>0 && osdeditlen>0) {
				i=osdeditcursor;
				while (i>0 && osdeditbuf[i-1]==32) i--;
				while (i>0 && osdeditbuf[i-1]!=32) i--;
				if (osdeditcursor<osdeditlen)
					Bmemmove(osdeditbuf+i, osdeditbuf+osdeditcursor, osdeditlen-osdeditcursor);
				osdeditlen -= (osdeditcursor-i);
				osdeditcursor = i;
				if (osdeditcursor < osdeditwinstart) {
					osdeditwinstart=osdeditcursor;
					osdeditwinend=osdeditwinstart+editlinewidth;
				}
			}
		} else if (ch >= 32) {	// text char
			if (!osdovertype && osdeditlen == EDITLENGTH)	// buffer full, can't insert another char
				return 0;

			if (!osdovertype) {
				if (osdeditcursor < osdeditlen) 
					Bmemmove(osdeditbuf+osdeditcursor+1, osdeditbuf+osdeditcursor, osdeditlen-osdeditcursor);
				osdeditlen++;
			} else {
				if (osdeditcursor == osdeditlen)
					osdeditlen++;
			}
			osdeditbuf[osdeditcursor] = ch;
			osdeditcursor++;
			if (osdeditcursor>osdeditwinend) osdeditwinstart++,osdeditwinend++;
		}
	}

	if (sc == 15) {		// tab
	} else if (sc == 1) {		// escape
		OSD_ShowDisplay(0);
	} else if (sc == 201) {	// page up
		if (osdhead < osdlines-1)
			osdhead++;
	} else if (sc == 209) {	// page down
		if (osdhead > 0)
			osdhead--;
	} else if (sc == 199) {	// home
		if (osdeditcontrol) {
			osdhead = osdlines-1;
		} else {
			osdeditcursor = 0;
			osdeditwinstart = osdeditcursor;
			osdeditwinend = osdeditwinstart+editlinewidth;
		}
	} else if (sc == 207) {	// end
		if (osdeditcontrol) {
			osdhead = 0;
		} else {
			osdeditcursor = osdeditlen;
			osdeditwinend = osdeditcursor;
			osdeditwinstart = osdeditwinend-editlinewidth;
			if (osdeditwinstart<0) {
				osdeditwinstart=0;
				osdeditwinend = editlinewidth;
			}
		}
	} else if (sc == 210) {	// insert
		osdovertype ^= 1;
	} else if (sc == 203) {	// left
		if (osdeditcursor>0) {
			if (osdeditcontrol) {
				while (osdeditcursor>0) {
					if (osdeditbuf[osdeditcursor-1] != 32) break;
					osdeditcursor--;
				}
				while (osdeditcursor>0) {
					if (osdeditbuf[osdeditcursor-1] == 32) break;
					osdeditcursor--;
				}
			} else osdeditcursor--;
		}
		if (osdeditcursor<osdeditwinstart)
			osdeditwinend-=(osdeditwinstart-osdeditcursor),
			osdeditwinstart-=(osdeditwinstart-osdeditcursor);
	} else if (sc == 205) {	// right
		if (osdeditcursor<osdeditlen) {
			if (osdeditcontrol) {
				while (osdeditcursor<osdeditlen) {
					if (osdeditbuf[osdeditcursor] == 32) break;
					osdeditcursor++;
				}
				while (osdeditcursor<osdeditlen) {
					if (osdeditbuf[osdeditcursor] != 32) break;
					osdeditcursor++;
				}
			} else osdeditcursor++;
		}
		if (osdeditcursor>=osdeditwinend)
			osdeditwinstart+=(osdeditcursor-osdeditwinend),
			osdeditwinend+=(osdeditcursor-osdeditwinend);
	} else if (sc == 200) {	// up
		if (osdhistorypos < osdhistorysize-1) {
			osdhistorypos++;
			memcpy(osdeditbuf, osdhistorybuf[osdhistorypos], EDITLENGTH+1);
			osdeditlen = osdeditcursor = 0;
			while (osdeditbuf[osdeditcursor]) osdeditlen++, osdeditcursor++;
			if (osdeditcursor<osdeditwinstart) {
				osdeditwinend = osdeditcursor;
				osdeditwinstart = osdeditwinend-editlinewidth;
				
				if (osdeditwinstart<0)
					osdeditwinend-=osdeditwinstart,
					osdeditwinstart=0;
			} else if (osdeditcursor>=osdeditwinend)
				osdeditwinstart+=(osdeditcursor-osdeditwinend),
				osdeditwinend+=(osdeditcursor-osdeditwinend);
		}
	} else if (sc == 208) {	// down
		if (osdhistorypos >= 0) {
			if (osdhistorypos == 0) {
				osdeditlen=0;
				osdeditcursor=0;
				osdeditwinstart=0;
				osdeditwinend=editlinewidth;
				osdhistorypos = -1;
			} else {
				osdhistorypos--;
				memcpy(osdeditbuf, osdhistorybuf[osdhistorypos], EDITLENGTH+1);
				osdeditlen = osdeditcursor = 0;
				while (osdeditbuf[osdeditcursor]) osdeditlen++, osdeditcursor++;
				if (osdeditcursor<osdeditwinstart) {
					osdeditwinend = osdeditcursor;
					osdeditwinstart = osdeditwinend-editlinewidth;
					
					if (osdeditwinstart<0)
						osdeditwinend-=osdeditwinstart,
						osdeditwinstart=0;
				} else if (osdeditcursor>=osdeditwinend)
					osdeditwinstart+=(osdeditcursor-osdeditwinend),
					osdeditwinend+=(osdeditcursor-osdeditwinend);
			}
		}
	} else if (sc == 42 || sc == 54) {	// shift
		osdeditshift = 1;
	} else if (sc == 29 || sc == 157) {	// control
		osdeditcontrol = 1;
	} else if (sc == 58) {	// capslock
		osdeditcaps ^= 1;
	} else if (sc == 28 || sc == 156) {	// enter
	} else if (sc == 14) {		// backspace
	} else if (sc == 211) {	// delete
		if (osdeditcursor == osdeditlen || !osdeditlen) return 0;
		if (osdeditcursor <= osdeditlen-1) Bmemmove(osdeditbuf+osdeditcursor, osdeditbuf+osdeditcursor+1, osdeditlen-osdeditcursor-1);
		osdeditlen--;
	}
	
	return 0;
}


//
// OSD_ResizeDisplay() -- Handles readjustment of the display when the screen resolution
// 	changes on us.
//
void OSD_ResizeDisplay(int w, int h)
{
	int newcols;
	int newmaxlines;
	char newtext[TEXTSIZE];
	int i,j,k;

	newcols = getcolumnwidth(w);
	newmaxlines = TEXTSIZE / newcols;

	j = min(newmaxlines, osdmaxlines);
	k = min(newcols, osdcols);

	memset(newtext, 32, TEXTSIZE);
	for (i=0;i<j;i++) {
		memcpy(newtext+newcols*i, osdtext+osdcols*i, k);
	}

	memcpy(osdtext, newtext, TEXTSIZE);
	osdcols = newcols;
	osdmaxlines = newmaxlines;
	osdmaxrows = getrowheight(h)-2;
	
	if (osdrows > osdmaxrows) osdrows = osdmaxrows;
	
	osdpos = 0;
	osdhead = 0;
	osdeditwinstart = 0;
	osdeditwinend = editlinewidth;
}


//
// OSD_ShowDisplay() -- Shows or hides the onscreen display
//
void OSD_ShowDisplay(int onf)
{
	osdvisible = (onf != 0);
	osdeditcontrol = 0;
	osdeditshift = 0;

	grabmouse(osdvisible == 0);
	onshowosd(osdvisible);
	if (osdvisible) releaseallbuttons();
}


//
// OSD_Draw() -- Draw the onscreen display
//
void OSD_Draw(void)
{
	unsigned topoffs;
	int row, lines, x, len;
	
	if (!osdvisible || !osdinited) return;

	topoffs = osdhead * osdcols;
	row = osdrows-1;
	lines = min( osdlines-osdhead, osdrows );
	
	begindrawing();

	clearbackground(osdcols,osdrows+1);

	for (; lines>0; lines--, row--) {
		drawosdstr(0,row,osdtext+topoffs,osdcols,osdtextshade,osdtextpal);
		topoffs+=osdcols;
	}

	drawosdchar(2,osdrows,'>',osdpromptshade,osdpromptpal);
	if (osdeditcaps) drawosdchar(0,osdrows,'C',osdpromptshade,osdpromptpal);
	if (osdeditshift) drawosdchar(1,osdrows,'H',osdpromptshade,osdpromptpal);
	
	len = min(osdcols-1-3, osdeditlen-osdeditwinstart);
	for (x=0; x<len; x++)
		drawosdchar(3+x,osdrows,osdeditbuf[osdeditwinstart+x],osdeditshade,osdeditpal);
	
	drawosdcursor(3+osdeditcursor-osdeditwinstart,osdrows,osdovertype,keytime);

	enddrawing();
}


//
// OSD_Printf() -- Print a string to the onscreen display
//   and write it to the log file
//

static inline void linefeed(void)
{
	Bmemmove(osdtext+osdcols, osdtext, TEXTSIZE-osdcols);
	Bmemset(osdtext, 32, osdcols);

	if (osdlines < osdmaxlines) osdlines++;
}

void OSD_Printf(const char *fmt, ...)
{
	char tmpstr[1024], *chp;
	va_list va;
		
	if (!osdinited) OSD_Init();

	va_start(va, fmt);
	Bvsnprintf(tmpstr, 1024, fmt, va);
	va_end(va);

	if (osdlog) Bfputs(tmpstr, osdlog);

	for (chp = tmpstr; *chp; chp++) {
		if (*chp == '\r') osdpos=0;
		else if (*chp == '\n') {
			osdpos=0;
			linefeed();
		} else {
			osdtext[osdpos++] = *chp;
			if (osdpos == osdcols) {
				osdpos = 0;
				linefeed();
			}
		}
	}
}


//
// OSD_DispatchQueued() -- Executes any commands queued in the buffer
//
void OSD_DispatchQueued(void)
{
	int cmd;
	
	if (!osdexeccount) return;

	cmd=osdexeccount-1;
	osdexeccount=0;

	for (; cmd>=0; cmd--) {
		OSD_Dispatch((const char *)osdhistorybuf[cmd]);
	}
}


//
// OSD_Dispatch() -- Executes a command string
//

static char *strtoken(char *s, char **ptrptr)
{
	char *p, *p2, *start;

	if (!ptrptr) return NULL;
	
	if (s) p = s;
	else p = *ptrptr;

	if (!p) return NULL;

	while (*p != 0 && *p != ';' && *p == ' ') p++;
	if (*p == 0 || *p == ';') {
		*ptrptr = NULL;
		return NULL;
	}
	if (*p == '\"') {
		// quoted string
		start = ++p;
		p2 = p;
		while (*p != 0 && *p != ';') {
			if (*p == '\"') {
				p++;
				break;
			} else if (*p == '\\') {
				switch (*(++p)) {
					case 'n': *p2 = '\n'; break;
					case 'r': *p2 = '\r'; break;
					default: *p2 = *p; break;
				}
			} else {
				*p2 = *p;
			}
			p2++, p++;
		}
		*p2 = 0;
	} else {
		start = p;
		while (*p != 0 && *p != ';' && *p != ' ') p++;
	}
	
	if (*p == 0 || *p == ';') *ptrptr = NULL;
	else {
		*(p++) = 0;
		*ptrptr = p;
	}

	return start;
}


#define MAXPARMS 512
int OSD_Dispatch(const char *cmd)
{
	char *workbuf, *wp, *wtp;
	char *parms[MAXPARMS];
	int  numparms;
	osdfuncparm_t ofp;
	symbol_t *symb;
	//int i;
	
	int intvar;
	char *strvar;

	workbuf = Bstrdup(cmd);
	if (!workbuf) return -1;

	numparms = 0;
	Bmemset(parms, 0, sizeof(parms));
	wp = strtoken(workbuf, &wtp);
	
	symb = findexactsymbol(wp);
	if (!symb) {
		OSD_Printf("Error: \"%s\" is not a defined variable or function\n", wp);
		free(workbuf);
		return -1;
	}

	while (wtp) {
		wp = strtoken(NULL, &wtp);
		if (wp && numparms < MAXPARMS) parms[numparms++] = wp;
	}

	//OSD_Printf("Symbol: %s\nParameters:\n",symb->name);
	//for (i=0;i<numparms;i++) OSD_Printf("Parm %d: %s\n",i,parms[i]);

	if (symb->type == SYMBTYPE_FUNC) {
		ofp.numparms = numparms;
		ofp.parms    = (const char **)parms;
		ofp.raw      = cmd;
		switch (symb->i.func.func(&ofp)) {
			case OSDCMD_OK: break;
			case OSDCMD_SHOWHELP: OSD_Printf("%s\n", symb->i.func.help); break;
		}
	} else if (symb->type == SYMBTYPE_VAR) {
		if (numparms >= 1) {
			switch (symb->i.var.type) {
				case OSDVAR_STRING: {
					strvar = (char *)Bmalloc(symb->i.var.extra);
					Bstrncpy(strvar, parms[0], symb->i.var.extra-1);
					strvar[symb->i.var.extra-1] = 0;

					if (symb->i.var.validator(strvar) >= 0)
						Bmemcpy(symb->i.var.var, strvar, symb->i.var.extra);
					free(strvar);
					break;
				}
				
				case OSDVAR_INTEGER:
					if (symb->i.var.extra)
						intvar = Bstrtol(parms[0], &strvar, 0);
					else
						intvar = Bstrtoul(parms[0], &strvar, 0);
					if (!strvar[0])
						if (symb->i.var.validator(&intvar) >= 0)
							Bmemcpy(symb->i.var.var, &intvar, 4);
					break;
			}
		}
		
		// display variable value
		switch (symb->i.var.type) {
			case OSDVAR_STRING:
				OSD_Printf("%s[%d] = \"%s\"\n", symb->name, symb->i.var.extra, symb->i.var.var);
				break;
			
			case OSDVAR_INTEGER:
				if (symb->i.var.extra)
					OSD_Printf("%s = %d\n", symb->name, *((signed int*)symb->i.var.var));
				else
					OSD_Printf("%s = %u\n", symb->name, *((unsigned int*)symb->i.var.var));
				break;
		}
	}
	
	free(workbuf);
	
	return 0;
}


//
// OSD_RegisterFunction() -- Registers a new function
//
int OSD_RegisterFunction(const char *name, const char *help, int (*func)(const osdfuncparm_t*))
{
	symbol_t *symb;
	const char *cp;

	if (!osdinited) OSD_Init();

	if (!name) {
		Bprintf("OSD_RegisterFunction(): may not register a function with a null name\n");
		return -1;
	}
	if (!name[0]) {
		Bprintf("OSD_RegisterFunction(): may not register a function with no name\n");
		return -1;
	}

	// check for illegal characters in name
	for (cp = name; *cp; cp++) {
		if ((cp == name) && (*cp >= '0') && (*cp <= '9')) {
			Bprintf("OSD_RegisterFunction(): first character of function name \"%s\" must not be a numeral\n", name);
			return -1;
		}
		if ((*cp < '0') ||
		    (*cp > '9' && *cp < 'A') ||
		    (*cp > 'Z' && *cp < 'a' && *cp != '_') ||
		    (*cp > 'z')) {
			Bprintf("OSD_RegisterFunction(): illegal character in function name \"%s\"\n", name);
			return -1;
		}
	}

	if (!help) help = "(no description for this function)";
	if (!func) {
		Bprintf("OSD_RegisterFunction(): may not register a null function\n");
		return -1;
	}

	symb = findexactsymbol(name);
	if (symb) {
		const char *s;
		switch (symb->type) {
			case SYMBTYPE_FUNC: s = "function"; break;
			case SYMBTYPE_VAR:  s = "variable"; break;
			default: s = "?"; break;
		}
		Bprintf("OSD_RegisterFunction(): \"%s\" is already defined as a %s\n", name, s);
		return -1;
	}
	
	symb = addnewsymbol(SYMBTYPE_FUNC, name);
	if (!symb) {
		Bprintf("OSD_RegisterFunction(): Failed registering function \"%s\"\n", name);
		return -1;
	}

	symb->name = name;
	symb->type = SYMBTYPE_FUNC;
	symb->i.func.help = help;
	symb->i.func.func = func;

	return 0;
}


//
// OSD_RegisterVariable() -- Registers a new variable
//
int OSD_RegisterVariable(const char *name, int type, void *var, int extra, int (*validator)(void*))
{
	symbol_t *symb;
	const char *cp;

	if (!osdinited) OSD_Init();

	if (!name) {
		Bprintf("OSD_RegisterVariable(): may not register a variable with a null name\n");
		return -1;
	}
	if (!name[0]) {
		Bprintf("OSD_RegisterVariable(): may not register a variable with no name\n");
		return -1;
	}

	// check for illegal characters in name
	for (cp = name; *cp; cp++) {
		if ((cp == name) && (*cp >= '0') && (*cp <= '9')) {
			Bprintf("OSD_RegisterVariable(): first character of variable name \"%s\" must not be a numeral\n", name);
			return -1;
		}
		if ((*cp < '0') ||
		    (*cp > '9' && *cp < 'A') ||
		    (*cp > 'Z' && *cp < 'a' && *cp != '_') ||
		    (*cp > 'z')) {
			Bprintf("OSD_RegisterVariable(): illegal character in variable name \"%s\"\n", name);
			return -1;
		}
	}

	if (type != OSDVAR_INTEGER && type != OSDVAR_STRING) {
		Bprintf("OSD_RegisterVariable(): unrecognised variable type for \"%s\"\n", name);
		return -1;
	}
	if (!var) {
		Bprintf("OSD_RegisterVariable(): may not register a null variable\n");
		return -1;
	}
	if (!validator) {
		switch (type) {
			case OSDVAR_INTEGER: validator = osd_internal_validate_integer; break;
			case OSDVAR_STRING:  validator = osd_internal_validate_string; break;
		}
	}
	
	symb = findexactsymbol(name);
	if (symb) {
		const char *s;
		switch (symb->type) {
			case SYMBTYPE_FUNC: s = "function"; break;
			case SYMBTYPE_VAR:  s = "variable"; break;
			default: s = "?"; break;
		}
		Bprintf("OSD_RegisterVariable(): \"%s\" is already defined as a %s\n", name, s);
		return -1;
	}
	
	symb = addnewsymbol(SYMBTYPE_VAR, name);
	if (!symb) {
		Bprintf("OSD_RegisterVariable(): Failed registering variable \"%s\"\n", name);
		return -1;
	}

	symb->name = name;
	symb->type = SYMBTYPE_VAR;
	symb->i.var.type = type;
	symb->i.var.var  = var;
	symb->i.var.extra = extra;
	symb->i.var.validator = validator;

	return 0;
}


//
// addnewsymbol() -- Allocates space for a new symbol and attaches it
//   appropriately to the lists, sorted.
//
static symbol_t *addnewsymbol(int type, const char *name)
{
	symbol_t *newsymb, *s, *t;

	if (type != SYMBTYPE_FUNC && type != SYMBTYPE_VAR) { return NULL; }

	newsymb = (symbol_t *)Bmalloc(sizeof(symbol_t));
	if (!newsymb) { return NULL; }

	Bmemset(newsymb, 0, sizeof(symbol_t));
	newsymb->type = type;

	// link it to the main chain
	if (!symbols) {
		symbols = newsymb;
	} else {
		if (Bstrcasecmp(name, symbols->name) <= 0) {
			t = symbols;
			symbols = newsymb;
			symbols->next = t;
		} else {
			s = symbols;
			while (s->next) {
				if (Bstrcasecmp(s->next->name, name) > 0) break;
				s=s->next;
			}
			t = s->next;
			s->next = newsymb;
			newsymb->next = t;
		}
	}

	// link it to the appropriate type chain
	if (type == SYMBTYPE_FUNC) {
		if (!symbols_func) {
			symbols_func = newsymb;
		} else {
			if (Bstrcasecmp(name, symbols_func->name) <= 0) {
				t = symbols_func;
				symbols_func = newsymb;
				symbols_func->nexttype = t;
			} else {
				s = symbols_func;
				while (s->nexttype) {
					if (Bstrcasecmp(s->nexttype->name, name) > 0) break;
					s=s->nexttype;
				}
				t = s->nexttype;
				s->nexttype = newsymb;
				newsymb->nexttype = t;
			}
		}
	} else {
		if (!symbols_var) {
			symbols_var = newsymb;
		} else {
			if (Bstrcasecmp(name, symbols_var->name) <= 0) {
				t = symbols_var;
				symbols_var = newsymb;
				symbols_var->nexttype = t;
			} else {
				s = symbols_var;
				while (s->nexttype) {
					if (Bstrcasecmp(s->nexttype->name, name) > 0) break;
					s=s->nexttype;
				}
				t = s->nexttype;
				s->nexttype = newsymb;
				newsymb->nexttype = t;
			}
		}
	}

	return newsymb;
}


//
// findsymbol() -- Finds a symbol, possibly partially named
// 
static symbol_t *findsymbol(const char *name, symbol_t *startingat)
{
	if (!startingat) startingat = symbols;
	if (!startingat) return NULL;

	for (; startingat; startingat=startingat->next)
		if (!Bstrncasecmp(name, startingat->name, Bstrlen(name))) return startingat;

	return NULL;
}


//
// findexactsymbol() -- Finds a symbol, complete named
// 
static symbol_t *findexactsymbol(const char *name)
{
	symbol_t *startingat;
	if (!symbols) return NULL;

	startingat = symbols;

	for (; startingat; startingat=startingat->next)
		if (!Bstrcasecmp(name, startingat->name)) return startingat;

	return NULL;
}

