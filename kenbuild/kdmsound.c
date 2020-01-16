// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#define KDMSOUND_INTERNAL
#include "kdmsound.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include "compat.h"
#include "pragmas.h"
#include "cache1d.h"


#define NUMCHANNELS 16
#define MAXWAVES 256
#define MAXTRACKS 256
#define MAXNOTES 8192
#define MAXEFFECTS 16

    //Actual sound parameters after initsb was called
static int samplerate, numspeakers, bytespersample;

    //KWV wave variables
static int numwaves;
static char instname[MAXWAVES][16];
static int wavleng[MAXWAVES];
static int repstart[MAXWAVES], repleng[MAXWAVES];
static int finetune[MAXWAVES];

    //Other useful wave variables
static int wavoffs[MAXWAVES];

    //Effects array
static int eff[MAXEFFECTS][256];

    //KDM song variables:
static int numnotes, numtracks;
static unsigned char trinst[MAXTRACKS], trquant[MAXTRACKS];
static unsigned char trvol1[MAXTRACKS], trvol2[MAXTRACKS];
static int nttime[MAXNOTES];
static unsigned char nttrack[MAXNOTES], ntfreq[MAXNOTES];
static unsigned char ntvol1[MAXNOTES], ntvol2[MAXNOTES];
static unsigned char ntfrqeff[MAXNOTES], ntvoleff[MAXNOTES], ntpaneff[MAXNOTES];

    //Other useful song variables:
static int timecount, notecnt, musicstatus, musicrepeat;

static int kdmasm1, kdmasm2, kdmasm3, kdmasm4;

#define MAXBYTESPERTIC 2048
static int stemp[MAXBYTESPERTIC*2];

    //Sound reading information
static int splc[NUMCHANNELS], sinc[NUMCHANNELS], soff[NUMCHANNELS];
static int svol1[NUMCHANNELS], svol2[NUMCHANNELS];
static int volookup[NUMCHANNELS<<9];
static int swavenum[NUMCHANNELS];
static int frqeff[NUMCHANNELS], frqoff[NUMCHANNELS];
static int voleff[NUMCHANNELS], voloff[NUMCHANNELS];
static int paneff[NUMCHANNELS], panoff[NUMCHANNELS];

static int globposx, globposy, globxvect, globyvect;
static intptr_t xplc[NUMCHANNELS], yplc[NUMCHANNELS];
static int vdist[NUMCHANNELS], sincoffs[NUMCHANNELS], vol[NUMCHANNELS];
static char chanstat[NUMCHANNELS];

static int bytespertic;
static int frqtable[256];
static unsigned char qualookup[512*16];
static int ramplookup[64];

static char digistat = 0, musistat = 0;

static unsigned char *snd = NULL;


static void startwave(int wavnum, int dafreq, int davolume1, int davolume2, int dafrqeff, int davoleff, int dapaneff);
static void fsin(int *eax);
static int msqrtasm(unsigned int c);
static void bound2char(int count, int *stemp, unsigned char *charptr);
static void bound2short(int count, int *stemp, short *shortptr);
static void calcvolookupmono(int *edi, int eax, int ebx);
static void calcvolookupstereo(int *edi, int eax, int ebx, int ecx, int edx);
static int monohicomb(int unused, int *volptr, int cnt, int dasinc, int dasplc, int *stemp);
static int stereohicomb(int unused, int *volptr, int cnt, int dasinc, int dasplc, int *stemp);


int initkdm(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample)
{
    int i, j;

    digistat = dadigistat;
    musistat = damusistat;
    samplerate = dasamplerate;
    numspeakers = danumspeakers;
    bytespersample = dabytespersample;

    // Calculate samples per 120Hz tick, not bytes as implied.
    bytespertic = (((samplerate/120)+1)&~1);
    if (bytespertic > MAXBYTESPERTIC) {
        digistat = 0;
        musistat = 0;
        return -1;
    }

    j = (((11025*2093)/samplerate)<<13);
    for(i=1;i<76;i++)
    {
        frqtable[i] = j;
        j = mulscale30(j,1137589835);  //(pow(2,1/12)<<30) = 1137589835
    }
    for(i=0;i>=-14;i--) frqtable[i&255] = (frqtable[(i+12)&255]>>1);

    timecount = notecnt = musicstatus = musicrepeat = 0;

    clearbuf(stemp,sizeof(stemp)>>2,32768L);
    for(i=0;i<256;i++)
        for(j=0;j<16;j++)
        {
            qualookup[(j<<9)+i] = (((-i)*j+8)>>4);
            qualookup[(j<<9)+i+256] = (((256-i)*j+8)>>4);
        }
    for(i=0;i<(samplerate>>11);i++)
    {
        j = 1536 - (i<<10)/(samplerate>>11);
        fsin(&j);
        ramplookup[i] = ((16384-j)<<1);
    }

    for(i=0;i<256;i++)
    {
        j = i*90; fsin(&j);
        eff[0][i] = 65536+j/9;
        eff[1][i] = min(58386+((i*(65536-58386))/30),65536);
        eff[2][i] = max(69433+((i*(65536-69433))/30),65536);
        j = (i*2048)/120; fsin(&j);
        eff[3][i] = 65536+(j<<2);
        j = (i*2048)/30; fsin(&j);
        eff[4][i] = 65536+j;
        switch((i>>1)%3)
        {
            case 0: eff[5][i] = 65536; break;
            case 1: eff[5][i] = 65536*330/262; break;
            case 2: eff[5][i] = 65536*392/262; break;
        }
        eff[6][i] = min((i<<16)/120,65536);
        eff[7][i] = max(65536-(i<<16)/120,0);
    }

    musicoff();
    return 0;
}

void uninitkdm(void)
{
    if (snd) free(snd);
    snd = NULL;

    digistat = 0;
    musistat = 0;
}

void setears(int daposx, int daposy, int daxvect, int dayvect)
{
    if (lockkdm()) return;

    globposx = daposx;
    globposy = daposy;
    globxvect = daxvect;
    globyvect = dayvect;

    unlockkdm();
}

void wsayfollow(char *dafilename, int dafreq, int davol, int *daxplc, int *dayplc, char followstat)
{
    char ch1, ch2, bad;
    int i, wavnum, chanum;

    if (digistat == 0) return;
    if (davol <= 0) return;

    for(wavnum=numwaves-1;wavnum>=0;wavnum--)
    {
        bad = 0;

        i = 0;
        while ((dafilename[i] > 0) && (i < 16))
        {
            ch1 = dafilename[i]; if ((ch1 >= 97) && (ch1 <= 123)) ch1 -= 32;
            ch2 = instname[wavnum][i]; if ((ch2 >= 97) && (ch2 <= 123)) ch2 -= 32;
            if (ch1 != ch2) {bad = 1; break;}
            i++;
        }
        if (bad != 0) continue;

        if (lockkdm()) return;

        chanum = 0;
        for(i=NUMCHANNELS-1;i>0;i--) if (splc[i] > splc[chanum]) chanum = i;

        splc[chanum] = 0;     //Disable channel temporarily for clean switch

        if (followstat == 0)
        {
            xplc[chanum] = (intptr_t)*daxplc;
            yplc[chanum] = (intptr_t)*dayplc;
        }
        else
        {
            xplc[chanum] = ((intptr_t)daxplc);
            yplc[chanum] = ((intptr_t)dayplc);
        }
        vol[chanum] = davol;
        vdist[chanum] = 0;
        sinc[chanum] = (dafreq*11025)/samplerate;
        svol1[chanum] = davol;
        svol2[chanum] = davol;
        sincoffs[chanum] = 0;
        soff[chanum] = wavoffs[wavnum]+wavleng[wavnum];
        splc[chanum] = -(wavleng[wavnum]<<12);              //splc's modified last
        swavenum[chanum] = wavnum;
        chanstat[chanum] = followstat+1;
        frqeff[chanum] = 0; frqoff[chanum] = 0;
        voleff[chanum] = 0; voloff[chanum] = 0;
        paneff[chanum] = 0; panoff[chanum] = 0;

        unlockkdm();
        return;
    }
}

void wsay(char *dafilename, int dafreq, int volume1, int volume2)
{
    char ch1, ch2;
    int i, j, bad;

    if (digistat == 0) return;

    i = numwaves-1;
    do
    {
        bad = 0;

        j = 0;
        while ((dafilename[j] > 0) && (j < 16))
        {
            ch1 = dafilename[j]; if ((ch1 >= 97) && (ch1 <= 123)) ch1 -= 32;
            ch2 = instname[i][j]; if ((ch2 >= 97) && (ch2 <= 123)) ch2 -= 32;
            if (ch1 != ch2) {bad = 1; break;}
            j++;
        }
        if (bad == 0)
        {
            if (lockkdm()) return;
            startwave(i,(dafreq*11025)/samplerate,volume1,volume2,0L,0L,0L);
            unlockkdm();
            return;
        }

        i--;
    } while (i >= 0);
}

void loadwaves(char *wavename)
{
    int fil, i, j, dawaversionum, totsndbytes;

    fil = kopen4load(wavename,0);
    if (fil < 0) return;

    totsndbytes = 0;
    dawaversionum = 0;
    numwaves = 0;

    kread(fil,&dawaversionum,4);
    if (B_LITTLE32(dawaversionum) != 0) { kclose(fil); return; }

    kread(fil,&numwaves,4);
    numwaves = B_LITTLE32(numwaves);
    for(i=0;i<numwaves;i++)
    {
        kread(fil,&instname[i][0],16);
        kread(fil,&wavleng[i],4);
        wavleng[i] = B_LITTLE32(wavleng[i]);
        kread(fil,&repstart[i],4);
        repstart[i] = B_LITTLE32(repstart[i]);
        kread(fil,&repleng[i],4);
        repleng[i] = B_LITTLE32(repleng[i]);
        kread(fil,&finetune[i],4);
        finetune[i] = B_LITTLE32(finetune[i]);
        wavoffs[i] = totsndbytes;
        totsndbytes += wavleng[i];
    }

    for(i=numwaves;i<MAXWAVES;i++)
    {
        for(j=0;j<16;j++) instname[i][j] = 0;
        wavoffs[i] = totsndbytes;
        wavleng[i] = 0L;
        repstart[i] = 0L;
        repleng[i] = 0L;
        finetune[i] = 0L;
    }

    if (snd) free(snd);
    snd = (unsigned char *)malloc(totsndbytes+2);
    if (!snd) {
        kclose(fil);
        numwaves = 0;
        return;
    }

    kread(fil,snd,totsndbytes);
    kclose(fil);

    snd[totsndbytes] = snd[totsndbytes+1] = 128;
}

int loadsong(char *filename)
{
    int i, fil;
    int kdmversionum;

    if (musistat != 1) return(0);
    musicoff();

    if ((fil = kopen4load(filename,0)) == -1)
    {
        return(-1);
    }
    kread(fil,&kdmversionum,4);
    if (B_LITTLE32(kdmversionum) != 0) {
        kclose(fil);
        return(-2);
    }

    kread(fil,&numnotes,4);
    numnotes = B_LITTLE32(numnotes);
    kread(fil,&numtracks,4);
    numtracks = B_LITTLE32(numtracks);

    kread(fil,trinst,numtracks);
    kread(fil,trquant,numtracks);
    kread(fil,trvol1,numtracks);
    kread(fil,trvol2,numtracks);
    kread(fil,nttime,numnotes<<2);
    for (i=numnotes-1; i>=0; i--) nttime[i] = B_LITTLE32(nttime[i]);
    kread(fil,nttrack,numnotes);
    kread(fil,ntfreq,numnotes);
    kread(fil,ntvol1,numnotes);
    kread(fil,ntvol2,numnotes);
    kread(fil,ntfrqeff,numnotes);
    kread(fil,ntvoleff,numnotes);
    kread(fil,ntpaneff,numnotes);
    kclose(fil);
    return(0);
}

void musicon(void)
{
    if (musistat != 1)
        return;

    if (lockkdm()) return;

    notecnt = 0;
    timecount = nttime[notecnt];
    musicrepeat = 1;
    musicstatus = 1;

    unlockkdm();
}

void musicoff(void)
{
    int i;

    if (musistat != 1)
        return;

    if (lockkdm()) return;

    musicstatus = 0;
    for(i=0;i<NUMCHANNELS;i++)
        splc[i] = 0;
    musicrepeat = 0;
    timecount = 0;
    notecnt = 0;

    unlockkdm();
}

void preparekdmsndbuf(unsigned char *sndoffsplc, int sndbufsiz)
{
    int i, j, k, voloffs1, voloffs2, *volptr;
    int daswave, dasinc, dacnt, sndbufsamples;
    int ox, oy, x, y;

    sndbufsamples = sndbufsiz >> (bytespersample + numspeakers - 2);

    for (i=NUMCHANNELS-1;i>=0;i--)
        if ((splc[i] < 0) && (chanstat[i] > 0))
        {
            if (chanstat[i] == 1)
            {
                ox = (int)xplc[i];
                oy = (int)yplc[i];
            }
            else
            {
                ox = *(int *)xplc[i];
                oy = *(int *)yplc[i];
            }
            ox -= globposx; oy -= globposy;
            x = dmulscale28(oy,globxvect,-ox,globyvect);
            y = dmulscale28(ox,globxvect,oy,globyvect);

            if ((klabs(x) >= 32768) || (klabs(y) >= 32768))
                { splc[i] = 0; continue; }

            j = vdist[i];
            vdist[i] = msqrtasm(x*x+y*y);
            if (j)
            {
                j = (sinc[i]<<10)/(min(max(vdist[i]-j,-768),768)+1024)-sinc[i];
                sincoffs[i] = ((sincoffs[i]*7+j)>>3);
            }

            voloffs1 = min((vol[i]<<22)/(((x+1536)*(x+1536)+y*y)+1),255);
            voloffs2 = min((vol[i]<<22)/(((x-1536)*(x-1536)+y*y)+1),255);

            if (numspeakers == 1)
                calcvolookupmono(&volookup[i<<9],-(voloffs1+voloffs2)<<6,(voloffs1+voloffs2)>>1);
            else
                calcvolookupstereo(&volookup[i<<9],-(voloffs1<<7),voloffs1,-(voloffs2<<7),voloffs2);
        }

    for(dacnt=0;dacnt<sndbufsamples;dacnt+=bytespertic)
    {
        if (musicstatus > 0)    //Gets here 120 times/second
        {
            while ((notecnt < numnotes) && (timecount >= nttime[notecnt]))
            {
                j = trinst[nttrack[notecnt]];
                k = mulscale24(frqtable[ntfreq[notecnt]],finetune[j]+748);

                if (ntvol1[notecnt] == 0)   //Note off
                {
                    for(i=NUMCHANNELS-1;i>=0;i--)
                        if (splc[i] < 0)
                            if (swavenum[i] == j)
                                if (sinc[i] == k)
                                    splc[i] = 0;
                }
                else                        //Note on
                    startwave(j,k,ntvol1[notecnt],ntvol2[notecnt],ntfrqeff[notecnt],ntvoleff[notecnt],ntpaneff[notecnt]);

                notecnt++;
                if (notecnt >= numnotes)
                    if (musicrepeat > 0)
                        { notecnt = 0; timecount = nttime[0]; }
            }
            timecount++;
        }

        for(i=NUMCHANNELS-1;i>=0;i--)
        {
            if (splc[i] >= 0) continue;

            dasinc = sinc[i]+sincoffs[i];

            if (frqeff[i] != 0)
            {
                dasinc = mulscale16(dasinc,eff[frqeff[i]-1][frqoff[i]]);
                frqoff[i]++; if (frqoff[i] >= 256) frqeff[i] = 0;
            }
            if ((voleff[i]) || (paneff[i]))
            {
                voloffs1 = svol1[i];
                voloffs2 = svol2[i];
                if (voleff[i])
                {
                    voloffs1 = mulscale16(voloffs1,eff[voleff[i]-1][voloff[i]]);
                    voloffs2 = mulscale16(voloffs2,eff[voleff[i]-1][voloff[i]]);
                    voloff[i]++; if (voloff[i] >= 256) voleff[i] = 0;
                }

                if (numspeakers == 1)
                    calcvolookupmono(&volookup[i<<9],-(voloffs1+voloffs2)<<6,(voloffs1+voloffs2)>>1);
                else
                {
                    if (paneff[i])
                    {
                        voloffs1 = mulscale16(voloffs1,131072-eff[paneff[i]-1][panoff[i]]);
                        voloffs2 = mulscale16(voloffs2,eff[paneff[i]-1][panoff[i]]);
                        panoff[i]++; if (panoff[i] >= 256) paneff[i] = 0;
                    }
                    calcvolookupstereo(&volookup[i<<9],-(voloffs1<<7),voloffs1,-(voloffs2<<7),voloffs2);
                }
            }

            daswave = swavenum[i];
            volptr = &volookup[i<<9];

            kdmasm1 = repleng[daswave];
            kdmasm2 = wavoffs[daswave]+repstart[daswave]+repleng[daswave];
            kdmasm3 = (repleng[daswave]<<12); //repsplcoff
            kdmasm4 = soff[i];
            if (numspeakers == 1)
                { splc[i] = monohicomb(0L,volptr,bytespertic,dasinc,splc[i],stemp); }
            else
                { splc[i] = stereohicomb(0L,volptr,bytespertic,dasinc,splc[i],stemp); }
            soff[i] = kdmasm4;

            if ((splc[i] >= 0)) continue;
            if (numspeakers == 1)
               { monohicomb(0L,volptr,samplerate>>11,dasinc,splc[i],&stemp[bytespertic]); }
            else
               { stereohicomb(0L,volptr,samplerate>>11,dasinc,splc[i],&stemp[bytespertic<<1]); }
        }

        if (numspeakers == 1)
        {
            for(i=(samplerate>>11)-1;i>=0;i--)
                stemp[i] += mulscale16(stemp[i+1024]-stemp[i],ramplookup[i]);
            j = bytespertic; k = (samplerate>>11);
            copybuf(&stemp[j],&stemp[1024],k);
            clearbuf(&stemp[j],k,32768);
        }
        else
        {
            for(i=(samplerate>>11)-1;i>=0;i--)
            {
                j = (i<<1);
                stemp[j+0] += mulscale16(stemp[j+1024]-stemp[j+0],ramplookup[i]);
                stemp[j+1] += mulscale16(stemp[j+1025]-stemp[j+1],ramplookup[i]);
            }
            j = (bytespertic<<1); k = ((samplerate>>11)<<1);
            copybuf(&stemp[j],&stemp[1024],k);
            clearbuf(&stemp[j],k,32768);
        }

        if (numspeakers == 1)
        {
            if (bytespersample == 1) bound2char(bytespertic>>1,stemp,(unsigned char *)&sndoffsplc[dacnt]);
            else bound2short(bytespertic>>1,stemp,(short*)&sndoffsplc[dacnt<<1]);
        }
        else
        {
            if (bytespersample == 1) bound2char(bytespertic,stemp,(unsigned char *)&sndoffsplc[dacnt<<1]);
            else bound2short(bytespertic,stemp,(short*)&sndoffsplc[dacnt<<2]);
        }
    }
}

static void startwave(int wavnum, int dafreq, int davolume1, int davolume2, int dafrqeff, int davoleff, int dapaneff)
{
    int i, chanum;

    if ((davolume1|davolume2) == 0) return;

    chanum = 0;
    for(i=NUMCHANNELS-1;i>0;i--)
        if (splc[i] > splc[chanum])
            chanum = i;

    splc[chanum] = 0;     //Disable channel temporarily for clean switch

    if (numspeakers == 1)
        calcvolookupmono(&volookup[chanum<<9],-(davolume1+davolume2)<<6,(davolume1+davolume2)>>1);
    else
        calcvolookupstereo(&volookup[chanum<<9],-(davolume1<<7),davolume1,-(davolume2<<7),davolume2);

    sinc[chanum] = dafreq;
    svol1[chanum] = davolume1;
    svol2[chanum] = davolume2;
    soff[chanum] = wavoffs[wavnum]+wavleng[wavnum];
    splc[chanum] = -(wavleng[wavnum]<<12);              //splc's modified last
    swavenum[chanum] = wavnum;
    frqeff[chanum] = dafrqeff; frqoff[chanum] = 0;
    voleff[chanum] = davoleff; voloff[chanum] = 0;
    paneff[chanum] = dapaneff; panoff[chanum] = 0;
    chanstat[chanum] = 0; sincoffs[chanum] = 0;
}

static void fsin(int *eax)
{
    const float oneshl14 = 16384.f;
    const float oneshr10 = 0.0009765625f;

    *eax = sinf(M_PI * (*eax) * oneshr10) * oneshl14;
}

static int msqrtasm(unsigned int c)
{
    unsigned int a,b;

    a = 0x40000000l;    // mov eax, 0x40000000
    b = 0x20000000l;    // mov ebx, 0x20000000
    do {                // begit:
        if (c >= a) {   // cmp ecx, eax  /  jl skip
            c -= a;     // sub ecx, eax
            a += b*4;   // lea eax, [eax+ebx*4]
        }               // skip:
        a -= b;         // sub eax, ebx
        a >>= 1;        // shr eax, 1
        b >>= 2;        // shr ebx, 2
    } while (b);        // jnz begit
    if (c >= a)         // cmp ecx, eax
        a++;            // sbb eax, -1
    a >>= 1;            // shr eax, 1
    return a;
}

static void bound2char(int count, int *stemp, unsigned char *charptr)
{
    int i, j;

    count <<= 1;
    for(i=0;i<count;i++)
    {
        j = (stemp[i]>>8);
        if (j < 0) j = 0;
        if (j > 255) j = 255;
        charptr[i] = (unsigned char)j;
        stemp[i] = 32768;
    }
}

static void bound2short(int count, int *stemp, short *shortptr)
{
    int i, j;

    count <<= 1;
    for(i=0;i<count;i++)
    {
        j = stemp[i];
        if (j < 0) j = 0;
        if (j > 65535) j = 65535;
        shortptr[i] = (short)(j^0x8000);
        stemp[i] = 32768;
    }
}

static void calcvolookupmono(int *edi, int eax, int ebx)
{
    int ecx, edx;

    ecx = 64;           // mov ecx, 64
    edx = eax+ebx;      // lea edx, [eax+ebx]
    ebx += ebx;         // add ebx, ebx
    do {                // begit:
        edi[0] = eax;   // mov dword ptr [edi], eax
        edi[1] = edx;   // mov dword ptr [edi+4], edx
        eax += ebx;     // add eax, ebx
        edx += ebx;     // add edx, ebx
        edi[2] = eax;   // mov dword ptr [edi+8], eax
        edi[3] = edx;   // mov dword ptr [edi+12], edx
        eax += ebx;     // add eax, ebx
        edx += ebx;     // add edx, ebx
        edi += 4;       // add edi, 16
        ecx--;          // dec ecx
    } while (ecx);      // jnz begit
}

static void calcvolookupstereo(int *edi, int eax, int ebx, int ecx, int edx)
{
    int esi;

    esi = 128;          // mov esi, 128
    do {                // begit:
        edi[0] = eax;   // mov dword ptr [edi], eax
        edi[1] = ecx;   // mov dword ptr [edi+4], ecx
        eax += ebx;     // add eax, ebx
        ecx += edx;     // add ecx, edx
        edi[2] = eax;   // mov dword ptr [edi+8], eax
        edi[3] = ecx;   // mov dword ptr [edi+12], ecx
        eax += ebx;     // add eax, ebx
        ecx += edx;     // add ecx, edx
        edi += 4;       // add edi, 16
        esi--;          // dec esi
    } while (esi);      // jnz begit
}

    // Fixed point = 20:12
    // kdmasm1 = Loop length, bytes. Used merely to know if looping is used.
    // kdmasm2 = Loop end byte offset in snd.
    // kdmasm3 = Loop length, fixed-point.
    // kdmasm4 = Byte offset of the end of the sound sample data in snd.
    // cnt     = Count of samples to render.
    // dasinc  = Sample byte increment, fixed-point.
    // dasplc  = Sample play cursor, fixed-point. Negative byte offset from end of the sample.
    // stemp   = Output buffer.
static int monohicomb(int unused, int *volptr, int cnt, int dasinc, int dasplc, int *stemp)
{
    unsigned char al, ah, bl, bh, dl, cf;
    int ocnt = 0;

    while (cnt > 0) {
        bl = snd[kdmasm4 + (dasplc >> 12) + 0];     // mov bx, word ptr [esi+88888888h]
        bh = snd[kdmasm4 + (dasplc >> 12) + 1];

        ah = (dasplc & 0xf00) >> 8;                 // mov eax, ecx; shr eax, 20
        dl = bl;                                    // mov dl, bl
        al = bl;                                    // mov al, bl

        cf = (int)al - bh < 0;                      // sub al, bh; adc ah, ah
        al -= bh;
        ah += ah + cf;
        dl += qualookup[((int)ah << 8) | al];       // add dl, byte ptr _qualookup[eax]

        stemp[ocnt] += volptr[(int)dl];

        dasplc += dasinc;
        cnt -= 1;
        ocnt += 1;

        if (dasplc >= 0) { // Reached the loop point.
            if (kdmasm1 == 0) break;
            kdmasm4 = kdmasm2;
            dasplc -= kdmasm3;
        }
    }

    return dasplc;
}

static int stereohicomb(int unused, int *volptr, int cnt, int dasinc, int dasplc, int *stemp)
{
    unsigned char al, ah, bl, bh, dl, cf;
    int ocnt = 0;

    while (cnt > 0) {
        bl = snd[kdmasm4 + (dasplc >> 12) + 0];     // mov bx, word ptr [esi+88888888h]
        bh = snd[kdmasm4 + (dasplc >> 12) + 1];

        ah = (dasplc & 0xf00) >> 8;                 // mov eax, ecx; shr eax, 20
        dl = bl;                                    // mov dl, bl
        al = bl;                                    // mov al, bl

        cf = (int)al - bh < 0;                      // sub al, bh; adc ah, ah
        al -= bh;
        ah += ah + cf;
        dl += qualookup[((int)ah << 8) | al];       // add dl, byte ptr _qualookup[eax]

        stemp[ocnt+0] += volptr[0+(int)dl*2];
        stemp[ocnt+1] += volptr[1+(int)dl*2];

        dasplc += dasinc;
        cnt -= 1;
        ocnt += 2;

        if (dasplc >= 0) { // Reached the loop point.
            if (kdmasm1 == 0) break;
            kdmasm4 = kdmasm2;
            dasplc -= kdmasm3;
        }
    }

    return dasplc;
}
