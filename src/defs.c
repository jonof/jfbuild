/*
 * Definitions file parser for Build
 * by Jonathon Fowler (jonof@edgenetwork.org)
 * Remixed substantially by Ken Silverman
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
	T_DEFINESKYBOX,
	T_DEFINETINT,
	T_DEFINEMODEL,
	T_DEFINEMODELFRAME,
	T_DEFINEMODELANIM,
	T_DEFINEMODELSKIN,
	T_SELECTMODELSKIN,
	T_DEFINEVOXEL,
	T_DEFINEVOXELTILES,
	T_MODEL,
	T_FILE,
	T_SCALE,
	T_SHADE,
	T_FRAME,
	T_ANIM,
	T_SKIN,
	T_TILE0,
	T_TILE1,
	T_FRAME0,
	T_FRAME1,
	T_FPS,
	T_FLAGS,
	T_PAL,
};

typedef struct { char *text; int tokenid; } tokenlist;
static tokenlist basetokens[] = {
	{ "include",         T_INCLUDE          },
	{ "#include",        T_INCLUDE          },
	{ "define",          T_DEFINE           },
	{ "#define",         T_DEFINE           },
	{ "definetexture",   T_DEFINETEXTURE    },
	{ "defineskybox",    T_DEFINESKYBOX     },
	{ "definetint",      T_DEFINETINT       },
	{ "definemodel",     T_DEFINEMODEL      },
	{ "definemodelframe",T_DEFINEMODELFRAME },
	{ "definemodelanim", T_DEFINEMODELANIM  },
	{ "definemodelskin", T_DEFINEMODELSKIN  },
	{ "selectmodelskin", T_SELECTMODELSKIN  },
	{ "definevoxel",     T_DEFINEVOXEL      },
	{ "definevoxeltiles",T_DEFINEVOXELTILES },
	{ "model",           T_MODEL            },
};

static tokenlist modeltokens[] = {
	{ "file",   T_FILE   },
	{ "name",   T_FILE   },
	{ "scale",  T_SCALE  },
	{ "shade",  T_SHADE  },
	{ "frame",  T_FRAME  },
	{ "anim",   T_ANIM   },
	{ "skin",   T_SKIN   },
};

static tokenlist modelframetokens[] = {
	{ "frame",  T_FRAME   },
	{ "name",   T_FRAME   },
	{ "tile0",  T_TILE0  },
	{ "tile1",  T_TILE1  },
};

static tokenlist modelanimtokens[] = {
	{ "frame0", T_FRAME0 },
	{ "frame1", T_FRAME1 },
	{ "fps",    T_FPS    },
	{ "flags",  T_FLAGS  },
};               

static tokenlist modelskintokens[] = {
	{ "pal",    T_PAL    },
	{ "file",   T_FILE   },
};


static int getatoken(scriptfile *sf, tokenlist *tl, int ntokens)
{
	char *tok;
	int i;

	if (!sf) return T_ERROR;
	tok = scriptfile_gettoken(sf);
	if (!tok) return T_EOF;

	for(i=0;i<ntokens;i++) {
		if (!Bstrcasecmp(tok, tl[i].text))
			return tl[i].tokenid;
	}

	return T_ERROR;
}

static int lastmodelid = -1, lastvoxid = -1, modelskin = -1, lastmodelskin = -1, seenframe = 0;
extern int nextvoxid;

static int defsparser(scriptfile *script)
{
	while (1) {
		switch (getatoken(script,basetokens,sizeof(basetokens)/sizeof(tokenlist))) {
			case T_ERROR:
				initprintf("Error on line %s:%d.\n", script->filename,script->linenum);
				break;
			case T_EOF:
				return(0);
			case T_INCLUDE:
				{
					char *fn;
					if (!scriptfile_getstring(script,&fn)) {
						scriptfile *included;

						included = scriptfile_fromfile(fn);
						if (!included) {
							initprintf("Warning: Failed including %s on line %s:%d\n",
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

					if (scriptfile_getstring(script,&name)) break;
					if (scriptfile_getsymbol(script,&number)) break;

					if (scriptfile_addsymbolvalue(name,number) < 0)
						initprintf("Warning: Symbol %s was NOT redefined to %d on line %s:%d\n",
								name,number,script->filename,script->linenum);
					break;
				}
			case T_DEFINETEXTURE:
				{
					int tile,pal,cx,cy,sx,sy;
					char *fn;

					if (scriptfile_getsymbol(script,&tile)) break;
					if (scriptfile_getsymbol(script,&pal)) break;
					if (scriptfile_getnumber(script,&cx)) break; //x-center
					if (scriptfile_getnumber(script,&cy)) break; //y-center
					if (scriptfile_getnumber(script,&sx)) break; //x-size
					if (scriptfile_getnumber(script,&sy)) break; //y-size
					if (scriptfile_getstring(script,&fn)) break;
					hicsetsubsttex(tile,pal,fn,cx,cy,sx,sy);
				}
				break;
			case T_DEFINESKYBOX:
				{
					int tile,pal,i;
					char *fn[6];
					const char *faces[6] = { //would be nice to use these strings for error messages :)
						"front face", "right face", "back face",
						"left face", "top face", "bottom face"
					};

					if (scriptfile_getsymbol(script,&tile)) break;
					if (scriptfile_getsymbol(script,&pal)) break;
					if (scriptfile_getsymbol(script,&i)) break; //future expansion
					for (i=0;i<6;i++) if (scriptfile_getstring(script,&fn[i])) break; //grab the 6 faces
					if (i < 6) break;
					hicsetskybox(tile,pal,fn);
				}
				break;
			case T_DEFINETINT:
				{
					int pal, r,g,b,f;

					if (scriptfile_getsymbol(script,&pal)) break;
					if (scriptfile_getnumber(script,&r)) break;
					if (scriptfile_getnumber(script,&g)) break;
					if (scriptfile_getnumber(script,&b)) break;
					if (scriptfile_getnumber(script,&f)) break; //effects
					hicsetpalettetint(pal,r,g,b,f);
				}
				break;
			case T_DEFINEMODEL:
				{
					char *modelfn;
					double scale;
					int shadeoffs;

					if (scriptfile_getstring(script,&modelfn)) break;
					if (scriptfile_getdouble(script,&scale)) break;
					if (scriptfile_getnumber(script,&shadeoffs)) break;

#if defined(POLYMOST) && defined(USE_OPENGL)
					lastmodelid = md2_loadmodel(modelfn);
					if (lastmodelid < 0) {
						initprintf("Failure loading MD2 model \"%s\"\n", modelfn);
						break;
					}
					md2_setmisc(lastmodelid,(float)scale, shadeoffs);
#endif
					modelskin = lastmodelskin = 0;
					seenframe = 0;
				}
				break;
			case T_DEFINEMODELFRAME:
				{
					char *framename, happy=1;
					int ftilenume, ltilenume, tilex;

					if (scriptfile_getstring(script,&framename)) break;
					if (scriptfile_getnumber(script,&ftilenume)) break; //first tile number
					if (scriptfile_getnumber(script,&ltilenume)) break; //last tile number (inclusive)
					if (ltilenume < ftilenume) {
						initprintf("Warning: backwards tile range on line %s:%d\n", script->filename, script->linenum);
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

					if (scriptfile_getstring(script,&startframe)) break;
					if (scriptfile_getstring(script,&endframe)) break;
					if (scriptfile_getdouble(script,&dfps)) break; //animation frame rate
					if (scriptfile_getnumber(script,&flags)) break;

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
					
					if (scriptfile_getsymbol(script,&palnum)) break;
					if (scriptfile_getstring(script,&skinfn)) break; //skin filename

					// if we see a sequence of definemodelskin, then a sequence of definemodelframe,
					// and then a definemodelskin, we need to increment the skin counter.
					//
					// definemodel "mymodel.md2" 1 1
					// definemodelskin 0 "normal.png"   // skin 0
					// definemodelskin 21 "normal21.png"
					// definemodelframe "foo" 1000 1002   // these use skin 0
					// definemodelskin 0 "wounded.png"   // skin 1
					// definemodelskin 21 "wounded21.png"
					// definemodelframe "foo2" 1003 1004   // these use skin 1
					// selectmodelskin 0         // resets to skin 0
					// definemodelframe "foo3" 1005 1006   // these use skin 0
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
					if (scriptfile_getsymbol(script,&modelskin)) break;
				}
				break;
			case T_DEFINEVOXEL:
				{
					char *fn;

					if (scriptfile_getstring(script,&fn)) break; //voxel filename

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

					if (scriptfile_getnumber(script,&ftilenume)) break; //1st tile #
					if (scriptfile_getnumber(script,&ltilenume)) break; //last tile #

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
			case T_MODEL:
				{
#if 0
					Old method:

					 definemodel "models/pig.md2" 17 0
					 definemodelskin 0 "normal.png"
					 definemodelskin 21 "normal21.png"
					 definemodelanim "stand00" "stand01"
					 definemodelanim "stand00" "stand00"
					 definemodelframe "walk1" 2000 2019

					New method:

					 model "models/pig.md2" {
					  scale 17 //shade 0
					  skin { pal 0 file "normal.png" }
					  skin { file "normal21.png" pal 21}
					  anim { name "ATROOPSTAND" frame0 "stand00" frame1 "stand01" }
					  anim { name "ATROOPFROZEN" frame0 "stand00" frame1 "stand00" skin { pal 1 file "frozen.png" } }
					  frame { name "walk1" tile0 2000 tile1 2019 }
					 }
#endif
					char *modelend, *modelfn;
					double scale=1.0;
					int shadeoffs=0;

					if (scriptfile_getstring(script,&modelfn)) break;

#if defined(POLYMOST) && defined(USE_OPENGL)
					lastmodelid = md2_loadmodel(modelfn);
					if (lastmodelid < 0) {
						initprintf("Failure loading MD2 model \"%s\"\n", modelfn);
						break;
					}
#endif
					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend)
						switch (getatoken(script,modeltokens,sizeof(modeltokens)/sizeof(tokenlist))) {
							//case T_ERROR: initprintf("Error on line %s:%d in model tokens\n", script->filename,script->linenum); break;
							case T_SCALE: scriptfile_getdouble(script,&scale); break;
							case T_SHADE: scriptfile_getnumber(script,&shadeoffs); break;
							case T_FRAME:
							{
								char *frameend, *framename = 0, happy=1;
								int ftilenume = 0, ltilenume = 0, tilex = 0;

								if (scriptfile_getbraces(script,&frameend)) break;
								while (script->textptr < frameend)
									switch(getatoken(script,modelframetokens,sizeof(modelframetokens)/sizeof(tokenlist))) {
										case T_FRAME: scriptfile_getstring(script,&framename); break;
										case T_TILE0: scriptfile_getnumber(script,&ftilenume); break; //first tile number
										case T_TILE1: scriptfile_getnumber(script,&ltilenume); break; //last tile number (inclusive)
									}

								if (ltilenume < ftilenume) {
									initprintf("Warning: backwards tile range on line %s:%d\n", script->filename, script->linenum);
									tilex = ftilenume;
									ftilenume = ltilenume;
									ltilenume = tilex;
								}

								if (lastmodelid < 0) {
									initprintf("Warning: Ignoring frame definition.\n");
									break;
								}
#if defined(POLYMOST) && defined(USE_OPENGL)
								for (tilex = ftilenume; tilex <= ltilenume && happy; tilex++)
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
#endif
								seenframe = 1;
								}
								break;
							case T_ANIM:
							{
								char *animend, *startframe = 0, *endframe = 0;
								int flags = 0;
								double dfps = 1.0;

								if (scriptfile_getbraces(script,&animend)) break;
								while (script->textptr < animend)
									switch(getatoken(script,modelanimtokens,sizeof(modelanimtokens)/sizeof(tokenlist))) {
										case T_FRAME0: scriptfile_getstring(script,&startframe); break;
										case T_FRAME1: scriptfile_getstring(script,&endframe); break;
										case T_FPS: scriptfile_getdouble(script,&dfps); break; //animation frame rate
										case T_FLAGS: scriptfile_getnumber(script,&flags); break;
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
							} break;
							case T_SKIN:
							{
								char *skinend, *skinfn = 0;
								int palnum = 0;

								if (scriptfile_getbraces(script,&skinend)) break;
								while (script->textptr < skinend)
									switch(getatoken(script,modelskintokens,sizeof(modelskintokens)/sizeof(tokenlist))) {
										case T_PAL: scriptfile_getsymbol(script,&palnum); break;
										case T_FILE: scriptfile_getstring(script,&skinfn); break; //skin filename
									}

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
							} break;
						}


					md2_setmisc(lastmodelid,(float)scale, shadeoffs);

					modelskin = lastmodelskin = 0;
					seenframe = 0;

				}
				break;
			default:
				initprintf("Unknown token."); break;
		}
	}
	return 0;
}


int loaddefinitionsfile(char *fn)
{
	scriptfile *script;

	script = scriptfile_fromfile(fn);
	if (!script) return -1;

	//numerrors = 0;
	defsparser(script);
	//initprintf("%s read with %d error(s)\n",fn,numerrors);

	scriptfile_close(script);
	scriptfile_clearsymbols();

	return 0;
}
