#include "jfaud.h"

#include <ctype.h>

#include "osd.h"
#include "baselayer.h"
#include "compat.h"
#include "cache1d.h"

#define NUMCHANNELS 16
#define UNITSPERMTR 640.0
#define WAVESFILE "WAVES.KWV"

static char musistat = 0, inited = 0;
static char songname[BMAX_PATH];

static struct {
	long *posx, *posy;
	int handle;
} sfxchans[NUMCHANNELS];
	


typedef struct {
	int realstart, reallen, realpos;
	JFAudRawFormat fmt;
} Kwv;

static int call_open(const char *fn, const char *subfn, JFAudFH *h, JFAudRawFormat **raw)
{
	Kwv kwv;
	const char *f;
	*raw = NULL;
	memset(h,0,sizeof(JFAudFH));
	h->fh = kopen4load(fn, 0);
	if (h->fh < 0) { return h->fh; }
	f = fn; while (*f) f++;

	if (tolower(*(--f)) != 'v' || tolower(*(--f)) != 'w' || tolower(*(--f)) != 'k' || *(--f) != '.')
		{ return h->fh; }

	// the file extension suggests a KWV file, so try and find the subfn in it
	if (!subfn) { return h->fh; }

	kwv.realstart = 0;
	{
		int i,j, numwaves, wavleng, repstart, repleng, finetune;
		char instname[16], found = 0;

		if (kread(h->fh, &i, 4) != 4) goto error;
		if (i != 0) goto error;
		if (kread(h->fh, &numwaves, 4) != 4) goto error;
		kwv.realstart = numwaves * (16+4*4);
		for (i=0;i<numwaves;i++) {
			if (kread(h->fh, instname, 16) != 16) goto error;
			if (kread(h->fh, &wavleng, 4) != 4) goto error;
			if (kread(h->fh, &repstart, 4) != 4) goto error;
			if (kread(h->fh, &repleng, 4) != 4) goto error;
			if (kread(h->fh, &finetune, 4) != 4) goto error;
			for (j=0;j<16 && subfn[j];j++) {
				if (tolower(subfn[j]) != tolower(instname[j])) {
					found = 0;
					break;
				} else found = 1;
			}
			if (found) break; else kwv.realstart += wavleng;
		}
		if (!found) goto error;
		kwv.reallen = wavleng;
		klseek(h->fh, kwv.realstart, SEEK_SET);
	}

	kwv.realpos = 0;
	kwv.fmt.samplerate = 11025;
	kwv.fmt.channels   = 1;
	kwv.fmt.bitspersample = 8;
	kwv.fmt.bigendian = 0;

	h->user = (void*)malloc(sizeof(kwv));
	if (!h->user) {
		kclose(h->fh);
		h->fh = -1;
		return -1;
	}
	memcpy(h->user, &kwv, sizeof(kwv));
	*raw = &((Kwv*)h->user)->fmt;
	return h->fh;

error:
	klseek(h->fh,0,SEEK_SET);
	return h->fh;
}
static int call_close(JFAudFH *h)
{
	if (h->user) free(h->user);
	if (h->fh >= 0) { kclose(h->fh); return 0; }
	return -1;
}
static int call_read(JFAudFH *h, void *buf, int len)
{
	int r;
	Kwv *kwv = (Kwv *)h->user;
	if (kwv) len = min( kwv->reallen - (kwv->realpos - kwv->realstart), len);
	if (len < 0) len = 0;
	r = kread(h->fh, buf, len);
	if (kwv) kwv->realpos += len;
	return r;
}
static int call_seek(JFAudFH *h, int off, int whence)
{
	int s;
	Kwv *kwv = (Kwv *)h->user;
	if (kwv) {
		switch (whence) {
			case SEEK_CUR:
				s = klseek(h->fh, off, SEEK_CUR);
				break;
			case SEEK_SET:
				off = min(off, kwv->reallen);
				s = klseek(h->fh, kwv->realstart + off, SEEK_SET);
				break;
			case SEEK_END:
				if (off > 0) off = 0;
				s = klseek(h->fh, kwv->realstart + kwv->reallen + off, SEEK_SET);
				break;
			default: return -1;
		}
		s -= kwv->realstart;
		kwv->realpos = s;
		return s;
	}
	return klseek(h->fh, off, whence);
}
static int call_tell(JFAudFH *h)
{
	Kwv *kwv = (Kwv *)h->user;
	if (kwv) return kwv->realpos;
	return klseek(h->fh,0,SEEK_CUR);
}
static int call_filelen(JFAudFH *h)
{
	Kwv *kwv = (Kwv *)h->user;
	if (kwv) return kwv->reallen;
	return kfilelength(h->fh);
}

static void call_logmsg(const char *str) { initprintf("%s\n",str); }


void loadwaves(void)
{
}

void initsb(char dadigistat, char damusistat, long dasamplerate, char danumspeakers, 
		char dabytespersample, char daintspersec, char daquality)
{
	JFAudCfg parms = {
		"", "", NULL,
		NUMCHANNELS, 0, 1,
		call_open, call_close, call_read, call_seek, call_tell, call_filelen,
		call_logmsg
	};
	jfauderr err;
	int i;

	//JFAud_setdebuglevel(DEBUGLVL_ALL);
	
	parms.samplerate = dasamplerate;
	inited = 0;
	musistat = 0;

	err = JFAud_init(&parms);
	if (err != jfauderr_ok) {
		initprintf("JFAud_init() returned %s\n", JFAud_errstr(err));
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

	err = JFAud_uninit();
	if (err != jfauderr_ok) {
		initprintf("JFAud_uninit() returned %s\n", JFAud_errstr(err));
		return;
	}

	inited = 0;
}

void setears(long daposx, long daposy, long daxvect, long dayvect)
{
	JFAudProp props[2];
	jfauderr err;

	if (!inited) return;

	props[0].prop = JFAudProp_position;
	props[0].val.v[0] = (float)daposx/UNITSPERMTR;
	props[0].val.v[1] = (float)daposy/UNITSPERMTR;
	props[0].val.v[2] = 0.0;
	props[1].prop = JFAudProp_orient;
	props[1].val.v2[0] = (float)daxvect/UNITSPERMTR;
	props[1].val.v2[1] = (float)dayvect/UNITSPERMTR;
	props[1].val.v2[2] = 0.0;
	props[1].val.v2[3] = 0.0;
	props[1].val.v2[4] = 0.0;
	props[1].val.v2[5] = -1.0;	// OpenAL's up is Build's down
	
//daposx=daposy=daxvect=dayvect=0;
/*	OSD_Printf("ears x=%g y=%g dx=%g dy=%g\n",
			(float)daposx/UNITSPERMTR, (float)daposy/UNITSPERMTR,
			(float)daxvect/UNITSPERMTR, (float)dayvect/UNITSPERMTR);
*/
	err = JFAud_setlistenerprops(2, props);
	if (err != jfauderr_ok) {
		if (err != jfauderr_notinited)
			OSD_Printf("JFAud_setlistenerprops() error %s\n", JFAud_errstr(err));
	}
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
	int handl;
	jfauderr err;
	JFAudProp props[5];

	if (!inited) return;

/*	OSD_Printf("playfollow %s vol=%g pitch=%g x=%g y=%g follows=%d\n",
			dafilename,(float)davol/256.0, (float)dafreq/4096.0,
			(float)(*daxplc)/UNITSPERMTR, (float)(*dayplc)/UNITSPERMTR,
			followstat);
*/
	props[0].prop = JFAudProp_pitch;
	props[0].val.f = (float)dafreq / 4096.0;
	props[1].prop = JFAudProp_gain;
	props[1].val.f = (float)davol / 256.0;
	props[2].prop = JFAudProp_position;
	props[2].val.v[0] = (float)(*daxplc) / UNITSPERMTR;
	props[2].val.v[1] = (float)(*dayplc) / UNITSPERMTR;
	props[2].val.v[2] = 0.0;
	props[3].prop = JFAudProp_posrel;
	props[3].val.i = 1;	// world-relative
	props[4].prop = JFAudProp_rolloff;
	props[4].val.f = 1.0;

	err = JFAud_playsound(&handl, WAVESFILE, dafilename, 1, JFAudPlayMode_playing, 5, props);
	if (err != jfauderr_ok) {
		if (err != jfauderr_notinited)
			OSD_Printf("JFAud_playsound() error %s\n", JFAud_errstr(err));
	} else {
		if (followstat) storehandle(handl, daxplc, dayplc);
		else storehandle(handl, NULL, NULL);
	}
}

void wsay(char *dafilename, long dafreq, long volume1, long volume2)
{
	int handl;
	jfauderr err;
	JFAudProp props[4];

	if (!inited) return;
	
/*	OSD_Printf("play %s vol=%g/%g pitch=%g\n",
			dafilename,(float)volume1/256.0,(float)volume2/256.0, (float)dafreq/4096.0);
*/
	props[0].prop = JFAudProp_pitch;
	props[0].val.f = (float)dafreq / 4096.0;
	props[1].prop = JFAudProp_gain;
	props[1].val.f = (float)volume1 / 256.0;
	props[2].prop = JFAudProp_position;
	props[2].val.v[0] = 0.0;
	props[2].val.v[1] = 0.0;
	props[2].val.v[2] = 0.0;
	props[3].prop = JFAudProp_posrel;
	props[3].val.i = 0;	// player-relative

	err = JFAud_playsound(&handl, WAVESFILE, dafilename, 1, JFAudPlayMode_playing, 4, props);
	if (err != jfauderr_ok) {
		if (err != jfauderr_notinited)
			OSD_Printf("JFAud_playsound() error %s\n", JFAud_errstr(err));
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

	err = JFAud_playmusic(songname, NULL, JFAudPlayMode_playing, 0, NULL);
	if (err != jfauderr_ok && err != jfauderr_notinited) {
		OSD_Printf("JFAud_playmusic() error %s\n", JFAud_errstr(err));
	}
}

void musicoff(void)
{
	if (!inited || !musistat) return;

	JFAud_setmusicplaymode(JFAudPlayMode_stopped);	
}

void refreshaudio(void)
{
	JFAudProp props[1];
	jfauderr err;

	int i;
	if (!inited) return;
	JFAud_update();
	for (i=NUMCHANNELS-1;i>=0;i--) {
		if (sfxchans[i].handle < 0) continue;
		if (!sfxchans[i].posx || !sfxchans[i].posy) continue;

		props[0].prop = JFAudProp_position;
		props[0].val.v[0] = (float)(*sfxchans[i].posx)/UNITSPERMTR;
		props[0].val.v[1] = (float)(*sfxchans[i].posy)/UNITSPERMTR;
		props[0].val.v[2] = 0.0;
		
		err = JFAud_setsoundprops(sfxchans[i].handle, 1, props);
		if (err != jfauderr_ok && err != jfauderr_notinited) {
			OSD_Printf("JFAud_setchannelprops() error %s\n", JFAud_errstr(err));
		}
	}
}
