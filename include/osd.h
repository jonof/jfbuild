// On-screen display (ie. console)
// for the Build Engine
// by Jonathon Fowler (jonof@edgenetwk.com)

#ifndef __osd_h__
#define __osd_h__


typedef struct {
	int numparms;
	const char **parms;
	const char *raw;
} osdfuncparm_t;

#define OSDCMD_OK	0
#define OSDCMD_SHOWHELP 1

#define OSDVAR_INTEGER	0
#define OSDVAR_STRING	1


int osd_internal_validate_string(void *);
int osd_internal_validate_integer(void *);
int osd_internal_validate_boolean(void *);


// initializes things
void OSD_Init(void);

// sets the file to echo output to
void OSD_SetLogFile(char *fn);

// sets the functions the OSD will call to interrogate the environment
void OSD_SetFunctions(
		void (*drawchar)(int,int,char,int,int),
		void (*drawstr)(int,int,char*,int,int,int),
		void (*drawcursor)(int,int,int,int),
		int (*colwidth)(int),
		int (*rowheight)(int),
		void (*clearbg)(int,int),
		int (*gettime)(void),
		void (*onshow)(int)
	);

// sets the parameters for presenting the text
void OSD_SetParameters(
		int promptshade, int promptpal,
		int editshade, int editpal,
		int textshade, int textpal
	);

// sets the scancode for the key which activates the onscreen display
void OSD_CaptureKey(int sc);

// handles keyboard input when capturing input. returns 0 if key was handled
// or the scancode if it should be handled by the game.
int  OSD_HandleKey(int sc, int press);

// handles the readjustment when screen resolution changes
void OSD_ResizeDisplay(int w,int h);

// shows or hides the onscreen display
void OSD_ShowDisplay(int onf);

// draw the osd to the screen
void OSD_Draw(void);

// just like printf
void OSD_Printf(const char *fmt, ...);
#define printOSD OSD_Printf

// executes buffered commands
void OSD_DispatchQueued(void);

// executes a string
int OSD_Dispatch(const char *cmd);

// registers a function
//   name = name of the function
//   minparms = minimum number of parameters
//   help = a short help string
//   func = the entry point to the function
int OSD_RegisterFunction(const char *name, int minparams, const char *help, int (*func)(const osdfuncparm_t*));

// registers a variable
//   name  = name of variable
//   type  = one of the OSDVAR_* constants
//   var   = pointer to the variable
//   extra = extra information pertaining to the variable
//      if type is OSDVAR_STRING, this is the char array size
//      if type is OSDVAR_INTEGER, this is the signed-ness of the variable
//   validator = a function which validates the value before assigning it.
//   	may be null for the default function relevant to the type be used.
//   	gets passed a pointer to the value which is potentially to be set
//   	and returns an error code and may modify the value.
int OSD_RegisterVariable(const char *name, int type, void *var, int extra, int (*validator)(void *));

#endif // __osd_h__

