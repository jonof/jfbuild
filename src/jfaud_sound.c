#include "jfaud.h"

#include "osd.h"
#include "baselayer.h"
#include "compat.h"
#include "cache1d.h"

#define NUMCHANNELS 16
#define UNITSPERMTR 640.0
#define WAVESFILE "WAVES.KWV"

static char musistat = 0, inited = 0;
static char songname[MAX_PATH];

static struct {
	long *posx, *posy;
	int handle;
} sfxchans[NUMCHANNELS];
	

static int call_open(const char *fn) { return kopen4load(fn,0); }
static int call_close(int h) { kclose(h); return 0; }
static int call_read(int h, void *buf, int len) { return kread(h,buf,len); }
static int call_seek(int h, int off, int whence) { return klseek(h,off,whence); }
static int call_tell(int h) { return ktell(h); }
static void call_logmsg(const char *str) { initprintf("%s\n",str); }


void loadwaves(void)
{
}

void initsb(char dadigistat, char damusistat, long dasamplerate, char danumspeakers, 
		char dabytespersample, char daintspersec, char daquality)
{
	struct jfaud_init_params parms = {
		"", "", "",
		NUMCHANNELS, 0,
		call_open, call_close, call_read, call_seek, call_tell,
		call_logmsg
	};
	jfauderr err;
	int i;

	parms.samplerate = dasamplerate;
	inited = 0;
	musistat = 0;

	err = jfaud_init(&parms);
	if (err != jfauderr_ok) {
		initprintf("jfaud_init() returned %s\n", jfauderr_getstring(err));
		return;
	}

	inited = 1;
	musistat = damusistat;

	for (i=NUMCHANNELS-1;i>=0;i--) sfxchans[i].handle = -1;
}

void uninitsb(void)
{
	jfauderr err;
	
	if (!inited) return;

	err = jfaud_uninit();
	if (err != jfauderr_ok) {
		initprintf("jfaud_uninit() returned %s\n", jfauderr_getstring(err));
		return;
	}

	inited = 0;
}

void setears(long daposx, long daposy, long daxvect, long dayvect)
{
	if (!inited) return;
//daposx=daposy=daxvect=dayvect=0;
/*	OSD_Printf("ears x=%g y=%g dx=%g dy=%g\n",
			(float)daposx/UNITSPERMTR, (float)daposy/UNITSPERMTR,
			(float)daxvect/UNITSPERMTR, (float)dayvect/UNITSPERMTR);
*/	jfaud_setpos((float)daposx/UNITSPERMTR,(float)daposy/UNITSPERMTR,0.0,
			0.0,0.0,0.0, // vel
			(float)daxvect/UNITSPERMTR,(float)dayvect/UNITSPERMTR,0.0, // at
			0.0,0.0,-1.0  // up
		    );
}

static void storehandle(int handle, long *daxplc, long *dayplc)
{
	int i,empty = -1;

	for (i=NUMCHANNELS-1;i>=0;i--) {
		if (sfxchans[i].handle<0 && empty<0) empty = i;
		if (sfxchans[i].handle == handle) {
			empty = i;
			break;
		}
	}

	if (empty < 0) return;

	sfxchans[empty].handle = handle;
	sfxchans[empty].posx = daxplc;
	sfxchans[empty].posy = dayplc;
}

void wsayfollow(char *dafilename, long dafreq, long davol, long *daxplc, long *dayplc, char followstat)
{
	char workfn[MAX_PATH+1+18];
	int handl;
	jfauderr err;
	
	if (!inited) return;

	Bsprintf(workfn,"%s%c%s",WAVESFILE,JFAUD_SUBFN_CHAR,dafilename);

/*	OSD_Printf("playfollow %s vol=%g pitch=%g x=%g y=%g follows=%d\n",
			workfn,(float)davol/256.0, (float)dafreq/4096.0,
			(float)(*daxplc)/UNITSPERMTR, (float)(*dayplc)/UNITSPERMTR,
			followstat);
*/	
	err = jfaud_playsound(&handl, workfn,
		(float)davol/256.0, (float)dafreq/4096.0, 0,
		(float)(*daxplc)/UNITSPERMTR, (float)(*dayplc)/UNITSPERMTR, 0.0,
//		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
		0, 1, 1, jfaud_playing);
	if (err != jfauderr_ok) {
		if (err != jfauderr_notinited)
			OSD_Printf("jfaud_playsound() error %s\n", jfauderr_getstring(err));
	} else {
		if (followstat) storehandle(handl, daxplc, dayplc);
		else storehandle(handl, NULL, NULL);
	}
}

void wsay(char *dafilename, long dafreq, long volume1, long volume2)
{
	char workfn[MAX_PATH+1+18];
	int handl;
	jfauderr err;
	
	if (!inited) return;
	
	Bsprintf(workfn,"%s%c%s",WAVESFILE,JFAUD_SUBFN_CHAR,dafilename);

/*	OSD_Printf("play %s vol=%g/%g pitch=%g\n",
			workfn,(float)volume1/256.0,(float)volume2/256.0, (float)dafreq/4096.0);
*/	
	err = jfaud_playsound(&handl, workfn,
		(float)volume1/256.0, (float)dafreq/4096.0, 0,
		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
		1, 0, 1, jfaud_playing);
	if (err != jfauderr_ok) {
		if (err != jfauderr_notinited)
			OSD_Printf("jfaud_playsound() error %s\n", jfauderr_getstring(err));
	} else storehandle(handl, NULL, NULL);
}

void loadsong(char *filename)
{
	if (!inited) return;
	strcpy(songname,filename);
}

void musicon(void)
{
	jfauderr err;
	
	if (!inited || !musistat) return;

	err = jfaud_playmusic(songname, 1.0, 1, jfaud_playing);
	if (err != jfauderr_ok && err != jfauderr_notinited) {
		OSD_Printf("jfaud_playmusic() error %s\n", jfauderr_getstring(err));
		return;
	}
}

void musicoff(void)
{
	if (!inited || !musistat) return;

	jfaud_setmusicplaymode(jfaud_stopped);	
}

void refreshaudio(void)
{
	int i;
	if (!inited) return;
	jfaud_update();
	for (i=NUMCHANNELS-1;i>=0;i--) {
		if (sfxchans[i].handle < 0) continue;
		if (!sfxchans[i].posx || !sfxchans[i].posy) continue;
		jfaud_movesound(sfxchans[i].handle,
				(float)(*sfxchans[i].posx)/UNITSPERMTR, (float)(*sfxchans[i].posy)/UNITSPERMTR, 0.0,
				0.0, 0.0, 0.0, 0);
	}
}
