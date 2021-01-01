// mmulti.h

#ifndef __mmulti_h__
#define __mmulti_h__

#ifdef __cplusplus
extern "C" {
#endif

#define MAXMULTIPLAYERS 16

#define MMULTI_MODE_MS  0
#define MMULTI_MODE_P2P 1

extern int myconnectindex, numplayers, networkmode;
extern int connecthead, connectpoint2[MAXMULTIPLAYERS];
extern unsigned char syncstate;

void initsingleplayers(void);
void initmultiplayers(int argc, char const * const argv[]);
int initmultiplayersparms(int argc, char const * const argv[]);
int initmultiplayerscycle(void);

void setpackettimeout(int datimeoutcount, int daresendagaincount);
void uninitmultiplayers(void);
void sendlogon(void);
void sendlogoff(void);
int getoutputcirclesize(void);
void setsocket(int newsocket);
void sendpacket(int other, unsigned char *bufptr, int messleng);
int getpacket(int *other, unsigned char *bufptr);
void flushpackets(void);
void genericmultifunction(int other, unsigned char *bufptr, int messleng, int command);

#ifdef __cplusplus
}
#endif

#endif	// __mmulti_h__

