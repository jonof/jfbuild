/*
 * Definitions file parser for Build
 * by Jonathon Fowler (jonof@edgenetwork.org)
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "build.h"
#include "compat.h"
#include "baselayer.h"
#include "scriptfile.h"

enum {
	T_EOF = -2,
	T_ERROR = -1,
	T_INCLUDE = 0,
	T_DEFINE,
	T_DEFINETEXTURE,
	T_DEFINETINT,
	T_DEFINEMODEL,
	T_DEFINEMODELFRAME,
	T_DEFINEMODELANIM,
	T_DEFINEMODELSKIN,
	T_SELECTMODELSKIN,
	T_DEFINEVOXEL,
	T_DEFINEVOXELTILES
};

static struct {
	char *text;
	int tokenid;
} legaltokens[] = {
	{ "include", T_INCLUDE },
	{ "#include", T_INCLUDE },
	{ "define", T_DEFINE },
	{ "#define", T_DEFINE },
	{ "definetexture", T_DEFINETEXTURE },
	{ "definetint", T_DEFINETINT },
	{ "definemodel", T_DEFINEMODEL },
	{ "definemodelframe", T_DEFINEMODELFRAME },
	{ "definemodelanim", T_DEFINEMODELANIM },
	{ "definemodelskin", T_DEFINEMODELSKIN },
	{ "selectmodelskin", T_SELECTMODELSKIN },
	{ "definevoxel", T_DEFINEVOXEL },
	{ "definevoxeltiles", T_DEFINEVOXELTILES }
};
#define numlegaltokens (sizeof(legaltokens)/sizeof(legaltokens[0]))

static int getatoken(scriptfile *sf)
{
	char *tok;
	int i;
	
	if (!sf) return T_ERROR;
	tok = scriptfile_gettoken(sf);
	if (!tok) return T_EOF;

	for(i=0;i<(int)numlegaltokens;i++) {
		if (!Bstrcasecmp(tok, legaltokens[i].text))
			return legaltokens[i].tokenid;
	}

	return T_ERROR;
}


static int numerrors = 0;

static int lastmodelid = -1, lastvoxid = -1, modelskin = -1, lastmodelskin = -1, seenframe = 0;
extern int nextvoxid;

static int defsparser(scriptfile *script)
{
	int tok;

	do {
		tok = getatoken(script);
		switch (tok) {
			case T_ERROR:
				initprintf("Error on line %s:%d.\n", script->filename,script->linenum);
				break;
			case T_EOF:
				break;
			case T_INCLUDE:
				{
					char *fn;
					fn = scriptfile_getstring(script);
					if (!fn) {
						initprintf("Unexpected EOF in T_INCLUDE on line %s:%d\n",
								script->filename,script->linenum);
						tok = T_EOF;
						numerrors++;
						break;
					} else {
						scriptfile *included;

						included = scriptfile_fromfile(fn);
						if (!included) {
							initprintf("Failure including %s on line %s:%d\n",
									fn, script->filename,script->linenum);
						} else {
							defsparser(included);
							scriptfile_close(included);
						}
					}
					break;
				}
			case T_DEFINE:
				{
					char *name;
					int number;
					name = scriptfile_getstring(script);
					if (!name) {
						initprintf("Unexpected EOF in T_DEFINE on line %s:%d\n",
								script->filename,script->linenum);
						tok = T_EOF;
						numerrors++;
						break;
					}
					if (scriptfile_getsymbol(script, &number)) {
						initprintf("Invalid symbol name or numeric constant on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}

					if (scriptfile_addsymbolvalue(name,number) < 0)
						initprintf("Symbol %s was not redefined to %d on line %s:%d\n",
								name,number,script->filename,script->linenum);
					break;
				}
			case T_DEFINETEXTURE:
				{
					int tile,pal;
					int cx,cy,sx,sy;
					char *fn,happy=1;

					int tilerv,palrv;

					tilerv = scriptfile_getsymbol(script,&tile);
					palrv = scriptfile_getsymbol(script,&pal);
					if (tilerv) {
						initprintf("Invalid symbol name or numeric constant for tile number "
									  "on line %s:%d\n", script->filename,script->linenum);
						numerrors++;
						happy=0;
					}
					if (palrv) {
						initprintf("Invalid symbol name or numeric constant for palette number "
									  "on line %s:%d\n", script->filename,script->linenum);
						numerrors++;
						happy=0;
					}
					if (scriptfile_getnumber(script,&cx)) {
						initprintf("Invalid numeric constant for x-center on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&cy)) {
						initprintf("Invalid numeric constant for y-center on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&sx)) {
						initprintf("Invalid numeric constant for x-size on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&sy)) {
						initprintf("Invalid numeric constant for y-size on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if ((fn = scriptfile_getstring(script)) == NULL) {
						initprintf("Invalid string constant for filename on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (happy) hicsetsubsttex(tile,pal,fn,cx,cy,sx,sy);
				}
				break;
			case T_DEFINETINT:
				{
					int pal;
					int r,g,b,f;
					char happy=1;

					if (scriptfile_getsymbol(script,&pal)) {
						initprintf("Invalid symbol name or numeric constant for palette number "
									  "on line %s:%d\n", script->filename,script->linenum);
						numerrors++;
						happy=0;
					}
					if (scriptfile_getnumber(script,&r)) {
						initprintf("Invalid numeric constant for red value on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&g)) {
						initprintf("Invalid numeric constant for green blue on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&b)) {
						initprintf("Invalid numeric constant for blue value on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&f)) {
						initprintf("Invalid numeric constant for effects value on line %s:%d\n",
								script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (happy) hicsetpalettetint(pal,r,g,b,f);
				}
				break;
			case T_DEFINEMODEL:
				{
					char *modelfn;
					double scale;
					int shadeoffs;

					modelfn = scriptfile_getstring(script);
					if (!modelfn) {
						initprintf("Invalid string constant for model filename on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}

					if (scriptfile_getdouble(script,&scale)) {
						initprintf("Invalid numeric constant for scale on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}

					if (scriptfile_getnumber(script,&shadeoffs)) {
						initprintf("Invalid numeric constant for shadeoffs on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}

#if defined(POLYMOST) && defined(USE_OPENGL)
					lastmodelid = md2_loadmodel(modelfn, (float)scale, shadeoffs);
					if (lastmodelid < 0) {
						initprintf("Failure loading MD2 model \"%s\"\n", modelfn);
						break;
					}
#endif
					modelskin = lastmodelskin = 0;
					seenframe = 0;
				}
				break;
			case T_DEFINEMODELFRAME:
				{
					char *framename, happy=1;
					int ftilenume, ltilenume, tilex;

					framename = scriptfile_getstring(script);
					if (!framename) {
						initprintf("Invalid string constant for frame name on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&ftilenume)) {
						initprintf("Invalid numeric constant for first tile number on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&ltilenume)) {
						initprintf("Invalid numeric constant for last tile number on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}
					if (ltilenume < ftilenume) {
						initprintf("Warning: backwards tile range on line %s:%d\n",
								script->filename, script->linenum);
						tilex = ftilenume;
						ftilenume = ltilenume;
						ltilenume = tilex;
					}

					if (lastmodelid < 0) {
						initprintf("Warning: Ignoring frame definition.\n");
						break;
					}
#if defined(POLYMOST) && defined(USE_OPENGL)
					for (tilex = ftilenume; tilex <= ltilenume && happy; tilex++) {
						switch (md2_defineframe(lastmodelid, framename, tilex, max(0,modelskin))) {
							case 0: break;
							case -1: happy = 0; break; // invalid model id!?
							case -2: initprintf("Invalid tile number on line %s:%d\n",
										 script->filename, script->linenum);
								 happy = 0;
								 break;
							case -3: initprintf("Invalid frame name on line %s:%d\n",
										 script->filename, script->linenum);
								 happy = 0;
								 break;
						}
					}
#endif
					seenframe = 1;
				}
				break;
			case T_DEFINEMODELANIM:
				{
					char *startframe, *endframe;
					int flags;
					double dfps;

					startframe = scriptfile_getstring(script);
					if (!startframe) {
						initprintf("Invalid string constant for start frame name on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}
					endframe = scriptfile_getstring(script);
					if (!endframe) {
						initprintf("Invalid string constant for end frame name on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getdouble(script,&dfps)) {
						initprintf("Invalid double constant for frame rate on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&flags)) {
						initprintf("Invalid numeric constant for flags on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}

					if (lastmodelid < 0) {
						initprintf("Warning: Ignoring animation definition.\n");
						break;
					}
#if defined(POLYMOST) && defined(USE_OPENGL)
					switch (md2_defineanimation(lastmodelid, startframe, endframe, (int)(dfps*(65536.0*.001)), flags)) {
						case 0: break;
						case -1: break; // invalid model id!?
						case -2: initprintf("Invalid starting frame name on line %s:%d\n",
									 script->filename, script->linenum);
							 break;
						case -3: initprintf("Invalid ending frame name on line %s:%d\n",
									 script->filename, script->linenum);
							 break;
						case -4: initprintf("Out of memory on line %s:%d\n",
									 script->filename, script->linenum);
							 break;
					}
#endif
				}
				break;
			case T_DEFINEMODELSKIN:
				{
					int palnum, palnumer;
					char *skinfn;
					
					palnumer = scriptfile_getsymbol(script,&palnum);
					skinfn = scriptfile_getstring(script);
					if (palnumer) {
						initprintf("Invalid symbol name or numeric constant for palette number "
									  "on line %s:%d\n", script->filename,script->linenum);
						numerrors++;
						break;
					}
					if (!skinfn) {
						initprintf("Invalid string constant for skin filename on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}

					// if we see a sequence of definemodelskin, then a sequence of definemodelframe,
					// and then a definemodelskin, we need to increment the skin counter.
					//
					// definemodel "mymodel.md2" 1 1
					// definemodelskin 0 "normal.png"	// skin 0
					// definemodelskin 21 "normal21.png"
					// definemodelframe "foo" 1000 1002	// these use skin 0
					// definemodelskin 0 "wounded.png"	// skin 1
					// definemodelskin 21 "wounded21.png"
					// definemodelframe "foo2" 1003 1004	// these use skin 1
					// selectmodelskin 0			// resets to skin 0
					// definemodelframe "foo3" 1005 1006	// these use skin 0
					if (seenframe) { modelskin = ++lastmodelskin; }
					seenframe = 0;

#if defined(POLYMOST) && defined(USE_OPENGL)
					switch (md2_defineskin(lastmodelid, skinfn, palnum, max(0,modelskin))) {
						case 0: break;
						case -1: break; // invalid model id!?
						case -2: initprintf("Invalid skin filename on line %s:%d\n",
									 script->filename, script->linenum);
							 break;
						case -3: initprintf("Invalid palette number on line %s:%d\n",
									 script->filename, script->linenum);
							 break;
						case -4: initprintf("Out of memory on line %s:%d\n",
									 script->filename, script->linenum);
							 break;
					}
#endif					
				}
				break;
			case T_SELECTMODELSKIN:
				{
					if (scriptfile_getsymbol(script,&modelskin)) {
						initprintf("Invalid symbol name or numeric constant for skin number "
									  "on line %s:%d\n", script->filename,script->linenum);
						numerrors++;
					}
				}
				break;
			case T_DEFINEVOXEL:
				{
					char *fn;

					fn = scriptfile_getstring(script);
					if (!fn) {
						initprintf("Invalid string constant for voxel filename on line %s:%d\n",
								script->filename, script->linenum);
						break;
					}
					if (nextvoxid == MAXVOXELS) {
						initprintf("Maximum number of voxels already defined.\n");
						break;
					}

					if (qloadkvx(nextvoxid, fn)) {
						initprintf("Failure loading voxel file \"%s\"\n",fn);
						break;
					}

					lastvoxid = nextvoxid++;
				}
				break;
			case T_DEFINEVOXELTILES:
				{
					int ftilenume, ltilenume, tilex;

					if (scriptfile_getnumber(script,&ftilenume)) {
						initprintf("Invalid numeric constant for first tile number on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}
					if (scriptfile_getnumber(script,&ltilenume)) {
						initprintf("Invalid numeric constant for last tile number on line %s:%d\n",
								script->filename, script->linenum);
						numerrors++;
						break;
					}
					if (ltilenume < ftilenume) {
						initprintf("Warning: backwards tile range on line %s:%d\n",
								script->filename, script->linenum);
						tilex = ftilenume;
						ftilenume = ltilenume;
						ltilenume = tilex;
					}
					if (ltilenume < 0 || ftilenume >= MAXTILES) {
						initprintf("Invalid tile range on line %s:%d\n",
								script->filename, script->linenum);
						break;
					}

					if (lastvoxid < 0) {
						initprintf("Warning: Ignoring voxel tiles definition.\n");
						break;
					}
					for (tilex = ftilenume; tilex <= ltilenume; tilex++) {
						tiletovox[tilex] = lastvoxid;
					}
				}
				break;
			default:
				initprintf("Unknown token."); break;
		}
	} while (tok != T_EOF);

	return 0;
}


int loaddefinitionsfile(char *fn)
{
	scriptfile *script;
	
	script = scriptfile_fromfile(fn);
	if (!script) return -1;

	numerrors = 0;
	defsparser(script);
	initprintf("%s read with %d error(s)\n",fn,numerrors);

	scriptfile_close(script);
	scriptfile_clearsymbols();

	return 0;
}
