// mmulti.h

#ifndef __mmulti_h__
#define __mmulti_h__

#include "mmultimsgs.h"

#define MAXMULTIPLAYERS 16


typedef struct {
	unsigned short ping;		// round-trip time in game tics
	unsigned char  loss;		// percentage value of a rough guage of packet loss
	unsigned short retransmittime;	// used to delay retransmission
	unsigned char  overlapcount;	// how many recent messages to retransmit each time
} multipeerstats;


#ifndef IN_MMULTI
// to keep the representation of network addresses transparent to the game we
// have to get crafty.
// this is a 32 byte array in order to allocate enough space to hold the internal
// format of this structure, yet keep its internals hidden from the outside world.
typedef char multiaddr[32];
#endif

int multiaddrtostring(multiaddr *addr, char *buf, int len);
int multistringtoaddr(char *str, multiaddr *addr);
int multicompareaddr(multiaddr *addr1, multiaddr *addr2);


extern short myconnectindex, numplayers;
extern int   myuniqueid;
extern int   ipport;

void initmultiplayers(char damultioption, char dacomrateoption, char dapriority);
void setpackettimeout(long datimeoutcount, long daresendagaincount);
void uninitmultiplayers(void);
void sendlogon(void);
void sendlogoff(void);
long getoutputcirclesize(void);
void setsocket(short newsocket);
void sendpacket(long other, char *bufptr, long messleng);
short getpacket(long *other, char *bufptr);
void flushpackets(void);
void genericmultifunction(long other, char *bufptr, long messleng, long command);

int whichpeer(multiaddr *);
int fetchoobmessage(multiaddr *addr, char *bufptr, long *messleng);
void sendoobpacket(long other, char *bufptr, long messleng);
void sendoobpacketto(multiaddr *other, char *bufptr, long messleng);

int multigetpeerstats(long other, multipeerstats *stats);
int multigetpeeraddr(long other, multiaddr *addr, int *uniqueid);

int multiaddplayer(char *str, int id);
int multicollectplayers(void);
void multidumpstats(void);
void multiidle(void);

#endif	// __mmulti_h__

