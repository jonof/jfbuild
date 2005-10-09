#include "jfaud.hpp"

#include <ctype.h>

#include "compat.h"
#include "baselayer.h"
#include "cache1d.h"

extern "C" {
void initsb(char,char,long,char,char,char,char);
void uninitsb(void);
void setears(long,long,long,long);
void wsayfollow(char *,long,long,long *,long *,char);
void wsay(char *,long,long,long);
void loadwaves(void);
void loadsong(char *);
void musicon(void);
void musicoff(void);
void refreshaudio(void);
}
	
#define NUMCHANNELS 16
#define UNITSPERMTR 640.0
#define WAVESFILE "WAVES.KWV"

static char musistat = 0;
static char songname[BMAX_PATH] = "";
static JFAud *jfaud = NULL;

static struct {
	long *posx, *posy;
	JFAudMixerChannel *handle;
} sfxchans[NUMCHANNELS];

class KenFile : public JFAudFile {
private:
	int fh;
public:
	KenFile(const char *filename, const char *subfilename)
		: JFAudFile(filename, subfilename)
	{
		fh = kopen4load(const_cast<char*>(filename), 0);
	}

	virtual ~KenFile()
	{
		if (fh >= 0) kclose(fh);
	}

	virtual bool IsOpen(void) const { return fh >= 0; }

	virtual int Read(int nbytes, void *buf)
	{
		if (fh < 0) return -1;
		return kread(fh, buf, nbytes);
	}
	virtual int Seek(int pos, SeekFrom where)
	{
		int when;
		if (fh < 0) return -1;
		switch (where) {
			case JFAudFile::Set: when = SEEK_SET; break;
			case JFAudFile::Cur: when = SEEK_CUR; break;
			case JFAudFile::End: when = SEEK_END; break;
			default: return -1;
		}
		return klseek(fh, pos, when);
	}
	virtual int Tell(void) const
	{
		if (fh < 0) return -1;
		return klseek(fh, 0, SEEK_CUR);
	}
	virtual int Length(void) const
	{
		if (fh < 0) return -1;
		return kfilelength(fh);
	}
};

class KWVFile : public JFAudFile {
private:
	int fh;
	int datastart, datalen, datapos;
public:
	KWVFile(const char *filename, const char *subfilename = NULL)
		: JFAudFile(filename, NULL),
		  datastart(-1), datalen(-1), datapos(-1)
	{
		long i,j, numwaves, wavleng, repstart, repleng, finetune;
		char instname[16];
		bool found = false;

		if (!subfilename) return;
		fh = kopen4load(const_cast<char *>(filename), 0);
		if (fh < 0) return;

		if (kread(fh, &i,        4) != 4 || i != 0) return;
		if (kread(fh, &numwaves, 4) != 4) return; numwaves = B_LITTLE32(numwaves);
		datastart = (4+4) + numwaves * (16+4*4);
		for (i=0;i<numwaves;i++) {
			if (kread(fh, instname, 16) != 16) return;
			if (kread(fh, &wavleng, 4)  != 4) return; wavleng  = B_LITTLE32(wavleng);
			if (kread(fh, &repstart, 4) != 4) return; repstart = B_LITTLE32(repstart);
			if (kread(fh, &repleng, 4)  != 4) return; repleng  = B_LITTLE32(repleng);
			if (kread(fh, &finetune, 4) != 4) return; finetune = B_LITTLE32(finetune);
			for (j=0;j<16 && subfilename[j];j++) {
				if (tolower(subfilename[j]) != tolower(instname[j])) {
					found = false;
					break;
				} else found = true;
			}
			if (found) break;
			datastart += wavleng;
		}
		if (!found) return;
		klseek(fh, datastart, SEEK_SET);
		datalen = wavleng;
		datapos = 0;
	}
	
	virtual ~KWVFile()
	{
		if (fh >= 0) kclose(fh);
	}
	virtual bool IsOpen(void) const
	{
		return !(datalen < 0 || fh < 0);
	}

	virtual int Read(int nbytes, void *buf)
	{
		if (!IsOpen()) return -1;
		if (datalen - datapos < nbytes) nbytes = datalen - datapos;
		if (nbytes <= 0) return 0;
		nbytes = kread(fh, buf, nbytes);
		if (nbytes > 0) datapos += nbytes;
		return nbytes;
	}
	virtual int Seek(int pos, SeekFrom where)
	{
		int newpos;
		if (!IsOpen()) return -1;
		switch (where) {
			case JFAudFile::Set: newpos = pos; break;
			case JFAudFile::Cur: newpos = datapos + pos; break;
			case JFAudFile::End: newpos = datalen + pos; break;
			default: return -1;
		}
		if (newpos < 0) newpos = 0;
		else if (newpos > datalen) newpos = datalen;
		return klseek(fh, datastart + newpos, SEEK_SET);
	}
	virtual int Tell(void) const
	{
		if (!IsOpen()) return -1;
		return datapos;
	}
	virtual int Length(void) const
	{
		if (!IsOpen()) return -1;
		return datalen;
	}
};

static JFAudFile *openfile(const char *fn, const char *subfn)
{
	char *ext;
	bool loadkwv = false;
	
	ext = Bstrrchr(fn,'.');
	if (!ext || Bstrcasecmp(ext, ".kwv"))
		return static_cast<JFAudFile*>(new KenFile(fn,subfn));
	if (!subfn) return NULL;	// KWV files need a sub name

	return static_cast<JFAudFile*>(new KWVFile(fn, subfn));
}

static void logfunc(const char *s) { initprintf("%s", s); }

void loadwaves(void)
{
}

void initsb(char dadigistat, char damusistat, long dasamplerate, char danumspeakers, 
		char dabytespersample, char daintspersec, char daquality)
{
	int i;

	if (jfaud) return;

	JFAud_SetLogFunc(logfunc);

	jfaud = new JFAud();
	if (!jfaud) return;

	jfaud->SetUserOpenFunc(openfile);

	musistat = 0;
	if (!jfaud->InitWave(NULL, NUMCHANNELS, dasamplerate)) {
		delete jfaud;
		jfaud = NULL;
		return;
	}

	musistat = damusistat;

	for (i=NUMCHANNELS-1;i>=0;i--) sfxchans[i].handle = NULL;
}

void uninitsb(void)
{
	if (!jfaud) return;

	delete jfaud;
	jfaud = NULL;
}

void setears(long daposx, long daposy, long daxvect, long dayvect)
{
	JFAudMixer *mixer;

	if (!jfaud) return;
	mixer = jfaud->GetWave();
	if (!mixer) return;

	mixer->SetListenerPosition((float)daposx/UNITSPERMTR, (float)daposy/UNITSPERMTR, 0.0);
	mixer->SetListenerOrientation((float)daxvect/UNITSPERMTR, (float)dayvect/UNITSPERMTR, 0.0,
			0.0, 0.0, -1.0);	// OpenAL's up is Build's down
}

static void storehandle(JFAudMixerChannel *handle, long *daxplc, long *dayplc)
{
	int i,empty = -1;

	for (i=NUMCHANNELS-1;i>=0;i--) {
		if (!sfxchans[i].handle && empty<0) empty = i;
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
	JFAudMixerChannel *handl;

	if (!jfaud) return;

	handl = jfaud->PlayRawSound(WAVESFILE, dafilename, 1, 11025, 1, 1, false);
	if (!handl) return;

	if (followstat) storehandle(handl, daxplc, dayplc);
	else storehandle(handl, NULL, NULL);

	handl->SetPitch((float)dafreq / 4096.0);
	handl->SetGain((float)davol / 256.0);
	handl->SetPosition((float)(*daxplc) / UNITSPERMTR, (float)(*dayplc) / UNITSPERMTR, 0.0);
	handl->SetFollowListener(false);
	handl->SetRolloff(1.0);

	handl->Play();
}

void wsay(char *dafilename, long dafreq, long volume1, long volume2)
{
	JFAudMixerChannel *handl;

	if (!jfaud) return;
	
	handl = jfaud->PlayRawSound(WAVESFILE, dafilename, 1, 11025, 1, 1, false);
	if (!handl) return;

	storehandle(handl, NULL, NULL);
	
	handl->SetPitch((float)dafreq / 4096.0);
	handl->SetGain((float)volume1 / 256.0);
	handl->SetPosition(0.0, 0.0, 0.0);
	handl->SetFollowListener(true);
	handl->SetRolloff(0.0);

	handl->Play();
}

void loadsong(char *filename)
{
	if (!jfaud) return;
	strcpy(songname,filename);
}

void musicon(void)
{
	if (!jfaud || !musistat || !songname[0]) return;

	jfaud->PlayMusic(songname);
}

void musicoff(void)
{
	if (!jfaud || !musistat) return;

	jfaud->StopMusic();
}

void refreshaudio(void)
{
	int i;

	if (!jfaud) return;
	for (i=NUMCHANNELS-1;i>=0;i--) {
		if (!sfxchans[i].handle || !jfaud->IsValidSound(sfxchans[i].handle)) continue;
		if (!sfxchans[i].posx || !sfxchans[i].posy) continue;

		sfxchans[i].handle->SetPosition(
				(float)(*sfxchans[i].posx)/UNITSPERMTR,
				(float)(*sfxchans[i].posy)/UNITSPERMTR,
				0.0);
	}
	jfaud->Update();
}
