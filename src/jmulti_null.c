// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jonof@edgenetwk.com)


#include "mmulti.h"

#define MAXPLAYERS 16


short myconnectindex, numplayers;
int   myuniqueid;
short connecthead, connectpoint2[MAXPLAYERS];
char syncstate = 0;
int ipport = 19014;


void initmultiplayers(char damultioption, char dacomrateoption, char dapriority)
{
	numplayers = 1; myconnectindex = 0;
	connecthead = 0; connectpoint2[0] = -1;
}

void setpackettimeout(long datimeoutcount, long daresendagaincount)
{
}

void uninitmultiplayers(void)
{
}

void sendlogon(void)
{
}

void sendlogoff(void)
{
}

long getoutputcirclesize(void)
{
	return 0;
}

void setsocket(short newsocket)
{
}

void sendpacket(long other, char *bufptr, long messleng)
{
}

short getpacket (long *other, char *bufptr)
{
	return 0;
}

void flushpackets(void)
{
}

void genericmultifunction(long other, char *bufptr, long messleng, long command)
{
}

int whichpeer(multiaddr *addr)
{
	return -1;
}

int fetchoobmessage(multiaddr *addr, char *bufptr, long *messleng)
{
	return 0;
}

void sendoobpacket(long other, char *bufptr, long messleng)
{
}

void sendoobpacketto(multiaddr *other, char *bufptr, long messleng)
{
}

void multiidle(void)
{
}

int multigetpeerstats(long other, multipeerstats *stats)
{
	return -1;
}

int multigetpeeraddr(long other, multiaddr *addr, int *uniqueid)
{
	return -1;
}

int multiaddplayer(char *str, int id)
{
}

int multicollectplayers(void)
{
	return 0;
}

int multiaddrtostring(multiaddr *addr, char *buf, int len)
{
	return 0;
}

int multistringtoaddr(char *str, multiaddr *addr)
{
	return 0;
}

int multicompareaddr(multiaddr *addr1, multiaddr *addr2)
{
	return 0;
}


