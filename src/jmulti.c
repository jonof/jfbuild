// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jonof@edgenetwk.com)

//#define CHATTY
//#define DUMPEACHPACKET

// define this to enable artificial errors
//#define INCOMPETENCE

#ifdef INCOMPETENCE
#define CORRUPTAGE(i) ((i & 7) == 6)
#define LOSSAGE(i) ((i & 7) == 1)
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "osd.h"
#include "crc32.h"
#include "baselayer.h"

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
//#include <winsock2.h>
#include <winsock.h>
#define ER(x) WSA##x
#define ERS(x) "WSA" x
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define ER(x) x
#define ERS(x) x
#endif

extern long pow2long[32];	// in zee engine


// engine internal OOB message identifier bytes
#define OOBMSG_PINGREQ	1
#define OOBMSG_PINGREP	2


// protocol-agnostic address structure
#define ADDR_NONE (0)
#define ADDR_IPV4 (1)
#define ADDR_IPV6 (2)
typedef struct {
	unsigned short type;
	union {
		struct sockaddr_in ipv4;
		//struct sockaddr_in6 ipv6;
	} addr;
} multiaddr;


#define IN_MMULTI	// include this here so we define multiaddr beforehand
#include "mmulti.h"

typedef unsigned long sequencenum;
#define NUM_SEQUENCES 65536
#define SIZE_SEQUENCENUM 2
#define SEQUENCE_MASK (NUM_SEQUENCES-1)

#define MIN_RETRANSMIT_THRESH	3
#define MAX_RETRANSMIT_THRESH	(5*120)
#define RETRANSMIT_SCALE(x)		((x)+((x)>>4))	// 1+(1/16)

char multioption = 0;
short myconnectindex, numplayers;
int myuniqueid = -1;
short connecthead, connectpoint2[MAXMULTIPLAYERS];
char syncstate = 0;
int ipport = 19014;

static int ipsocket = -1;
#ifdef WINDOWS
static char winsockinited = 0;
#endif

static short fetchhead = 0;
#define MAX_MESSAGE_LEN	 256
#define PACKET_HEAD_LEN  (4+1)
#define MESSAGE_HEAD_LEN (1+1+SIZE_SEQUENCENUM)

#define MAX_PACKET_LEN	((MAX_MESSAGE_LEN + MESSAGE_HEAD_LEN) * 4 + PACKET_HEAD_LEN + 16)


#define BACKLOGSZ 16384
static char           outmsgbuf[BACKLOGSZ];
static unsigned long  outmsghead = 0;
static char           inmsgbuf[BACKLOGSZ];
static unsigned long  inmsghead = 0;

#define ALLOC_IN  0
#define ALLOC_OUT 1

#define OOBQUEUESZ    16
static struct {
	char          *ptr;
	unsigned short len;
	multiaddr      addr;
} oobmsgqueue[OOBQUEUESZ];
static unsigned char oobmsgqueuehead = 0, oobmsgqueuetail = 0;


static struct {
	// outgoing messages
	sequencenum nextsequence;
	sequencenum acksequence;

	unsigned char *outptr[NUM_SEQUENCES];
	unsigned short outlen[NUM_SEQUENCES];
	unsigned char  outage[NUM_SEQUENCES];

	// outgoing oob queue
	unsigned char *oobptr[OOBQUEUESZ];
	unsigned short ooblen[OOBQUEUESZ];
	unsigned char  oobhead,oobtail;

	// incoming messages
	sequencenum insequence;
	
	unsigned char *inptr[NUM_SEQUENCES];
	unsigned short inlen[NUM_SEQUENCES];
	unsigned char  inflags[NUM_SEQUENCES];

	// other peer info
	multiaddr address;
	int uniqueid;
	unsigned char flags;
	multipeerstats stats;

#define LOSSOMETERSZ       10
	struct {
		unsigned long sentat;
		unsigned long returnedat;
	} lossometer[LOSSOMETERSZ];
	int lossometerhead;
	int lossometerlast;
} peers[MAXMULTIPLAYERS];

#define PEERFLAG_FORCETX 1

#define MSGTYPE_SYNC     0
#define MSGTYPE_OOB      1
#define MSGTYPE_INTERNAL 2

extern long totalclock;
static long lastclock;

static struct {
	unsigned long txbytes;
	unsigned long rxbytes;
	unsigned long txpackets;
	unsigned long rxpackets;
	unsigned long txerrors;
	unsigned long rxerrors;
} stats = { 0,0,0,0,0,0 };

static int startnetwork(void);
static void stopnetwork(void);
static void networksend(multiaddr *addr, void *buf, unsigned long len);
static int networkrecv(multiaddr *addr, void *buf, unsigned long *len);

static void _sendpacket(long other, char *bufptr, long messleng, unsigned char fl);
static void _sendoobpacket(long other, char *bufptr, long messleng, unsigned char fl);
static void _sendoobpacketto(multiaddr *other, char *bufptr, long messleng, unsigned char fl);


/*
 * Sequence number handling
 */

static sequencenum wrapsequencenum(sequencenum num)
{
	return num & SEQUENCE_MASK;
}

// function to unwrap a single sequence number in a possibly-wrapped span
// normalizes the sequence numbers it is given
static void unwrapsequencenums(sequencenum *head, sequencenum *tail, sequencenum *num)
{
	*head &= SEQUENCE_MASK;
	*tail &= SEQUENCE_MASK;
	*num  &= SEQUENCE_MASK;
	if (*head > *tail) {
		if (*num <= *tail) *num += NUM_SEQUENCES;
		*tail += NUM_SEQUENCES;
	}
}

// function to unwrap a span of sequence numbers, also normalizes them if unwrapped
static void unwrapsequencespan(sequencenum *head, sequencenum *tail)
{
	*head &= SEQUENCE_MASK;
	*tail &= SEQUENCE_MASK;
	if (*head > *tail) *tail += NUM_SEQUENCES;
}

static int putsequencenum(void *ptr, sequencenum num)
{
	char *cptr = (char *)ptr;
	
	num = wrapsequencenum(num);
	*(cptr++) = num & 255;
#if SIZE_SEQUENCENUM >= 2
	*(cptr++) = (num>>8) & 255;
#if SIZE_SEQUENCENUM == 4
	*(cptr++) = (num>>16) & 255;
	*(cptr++) = (num>>24) & 255;
#endif
#endif
	
	return SIZE_SEQUENCENUM;
}

static int getsequencenum(void *ptr, sequencenum *num)
{
	char *cptr = (char *)ptr;

	*num = 0;
	*num |= *(cptr++);
#if SIZE_SEQUENCENUM >= 2
	*num |= (sequencenum)(*(cptr++))<<8;
#if SIZE_SEQUENCENUM == 4
	*num |= (sequencenum)(*(cptr++))<<16;
	*num |= (sequencenum)(*(cptr++))<<24;
#endif
#endif

	return SIZE_SEQUENCENUM;
}


/*
 * Message storage queues
 */

static unsigned char * allocatestorage(int ring, short leng)
{
	int ptr;
	
	switch (ring) {
		case ALLOC_IN:
			ptr = inmsghead;
			inmsghead += leng;
			if (inmsghead > BACKLOGSZ) {
				ptr = 0;
				inmsghead = leng;
			}

			return (unsigned char *)(inmsgbuf + ptr);

		case ALLOC_OUT:
			ptr = outmsghead;
			outmsghead += leng;
			if (outmsghead > BACKLOGSZ) {
				ptr = 0;
				outmsghead = leng;
			}

			return (unsigned char *)(outmsgbuf + ptr);

		default: return NULL;
	}
}


/*
 * Messages
 */

static void agemessages(void)
{
	int h,delta=0,j;
	sequencenum i,seq,first,last;
	
	delta = totalclock - lastclock;
	lastclock = totalclock;
	if (delta <= 0) return;

#ifdef CHATTY
	printf("--- %d tics (now %d)\n", delta, totalclock);
#endif
	
	for (h=connecthead; h!=-1; h=connectpoint2[h]) {
		first = peers[h].acksequence;
		last  = peers[h].nextsequence;
		unwrapsequencespan(&first, &last);
		for (i=first; i<last; i++) {

			seq = wrapsequencenum(i);
			
			if (!peers[h].outlen[seq]) continue;
			j = (int)peers[h].outage[seq] + delta;
			peers[h].outage[seq] = j>MAX_RETRANSMIT_THRESH ? MAX_RETRANSMIT_THRESH : j;
		}
	}
}

static void processinternaloob(multiaddr *addr, unsigned char *buf, unsigned long len)
{
	short peer;
	int i;
	unsigned long c,d;
	unsigned long lost=0, combined=0, count=0;

	peer = whichpeer(addr);

#ifdef CHATTY
	printf("->> internal oob from %d: type %d\n", peer, buf[0]);
#endif
	
	switch (buf[0]) {
		case OOBMSG_PINGREQ:
			// echo it back to the sender
			buf[0] = OOBMSG_PINGREP;
			_sendoobpacketto(addr, buf, len, MSGTYPE_INTERNAL);
			break;

		case OOBMSG_PINGREP:
			if (peer < 0) {
				// ping reply from external peer
			} else {
				d = ((unsigned long)buf[1]) | ((unsigned long)buf[2]<<8) |
					((unsigned long)buf[3]<<16) | ((unsigned long)buf[4]<<24);
				for (i=0;i<LOSSOMETERSZ;i++) {
					if (peers[peer].lossometer[i].sentat == d)
						peers[peer].lossometer[i].returnedat = totalclock;

					if (peers[peer].lossometer[i].returnedat == 0xffffffffl) {
						if (peers[peer].lossometer[i].sentat != 0) {	// lost, or still waiting for reply
							lost++;
							count++;
						}
					} else {
						combined += peers[peer].lossometer[i].returnedat - peers[peer].lossometer[i].sentat;
						count++;
					}
				}

				peers[peer].stats.ping = c = (count-lost)>0 ? combined/(count-lost) : 0;
				peers[peer].stats.loss = count>0 ? lost*100/count : 0;
				d = max( MIN_RETRANSMIT_THRESH, RETRANSMIT_SCALE(c) - (count>0 ? c*lost/count : 0) );
				d = min( MAX_RETRANSMIT_THRESH, d );
				peers[peer].stats.retransmittime = d;
#ifdef CHATTY
				printf("peer %d: loss %d%% avgrtt %d count %d rt %d\n",
						peer, peers[peer].stats.loss, peers[peer].stats.ping, count, d);
#endif
			}
			break;
	}
}

static void processinternalmsg(short peer, unsigned char *buf, unsigned long len)
{
}

static void processincoming(void)
{
	multiaddr addr;
	short peer;
	unsigned char workbuf[MAX_PACKET_LEN], *workptr;
	unsigned long workbuflen;

	int rv;
	unsigned char *p;
	int i,j,k;

	unsigned long crc;
	char messagesleft;

	unsigned char flags;
	unsigned short length;
	sequencenum seq,wseq;
	

again:
	workbuflen = MAX_PACKET_LEN;
	if ((rv = networkrecv(&addr, workbuf, &workbuflen)) <= 0) return;

#ifdef CHATTY
	printf(">>> received %d bytes\n", workbuflen);
#endif
	if (workbuflen <= PACKET_HEAD_LEN) {
#ifdef CHATTY
		printf("!!! packet was incomplete\n");
#endif
		stats.rxerrors++;
		return;
	}

	crc = (unsigned long)workbuf[0] | (unsigned long)workbuf[1]<<8 |
	      (unsigned long)workbuf[2]<<16 | (unsigned long)workbuf[3]<<24;
	if (crc != crc32(workbuf+4,workbuflen-4)) {
#ifdef CHATTY
		printf("!!! packet was corrupt\n");
#endif
		stats.rxerrors++;
		goto again;
	}

	peer = whichpeer(&addr);
#ifdef CHATTY
	if (peer < 0) {
		char temp[64];
		multiaddrtostring(&addr, temp, 64);
		printf(">>> host %s\n", temp);
	} else
		printf(">>> peer %d\n", peer);
#endif
	
	workptr = workbuf + PACKET_HEAD_LEN;
	for (messagesleft = workbuf[4]; messagesleft > 0; messagesleft--, workptr+=length) {
		flags = *(workptr++);
		length = (unsigned short)(*(workptr++)) + 1;

		switch (flags & (MSGTYPE_SYNC|MSGTYPE_OOB)) {
			case MSGTYPE_SYNC:
				workptr += getsequencenum(workptr, &seq);
				
				if (peer < 0) {
#ifdef CHATTY
					printf("!!! alien sync message %d dropped\n", seq);
#endif
					continue;
				}

				if (peers[peer].inlen[seq] > 0)	{
					// duplicate
#ifdef CHATTY
					printf("!!! duplicate %d\n", seq);
#endif
					break;
				} else if (seq >= peers[peer].insequence) {
					// message we want
					// FIXME: wrapping might kill us here!
					//   Or maybe it won't, because once insequence wraps, future tests here
					//   won't fail anymore. We might be safe!
					p = allocatestorage(ALLOC_IN, length);
					if (!p) continue;

					memcpy(p, workptr, length);

					peers[peer].inptr[seq] = p;
					peers[peer].inlen[seq] = length;
					peers[peer].inflags[seq] = flags;
					
					peers[peer].flags |= PEERFLAG_FORCETX;		// make sure the ack goes out

#ifdef CHATTY
					printf(">>> accepted msg %d\n", seq);
#endif
				} else {
					// late message
#ifdef CHATTY
					printf("!!! late %d\n", seq);
#endif
					break;
				}
				break;

			case MSGTYPE_OOB:
				if (flags & MSGTYPE_INTERNAL) {
					// OOB internal to engine, so process it right now
					processinternaloob(&addr, workptr, length);
					break;
				}
				
				if ((oobmsgqueuetail + 1) % OOBQUEUESZ == oobmsgqueuehead) {
#ifdef CHATTY
					printf(">>> OOB ingress queue full.\n");
#endif
					break;	// full
				}

				p = allocatestorage(ALLOC_IN, length);
				if (!p) break;

				memcpy(p, workptr, length);
				
				oobmsgqueue[oobmsgqueuetail].ptr = p;
				oobmsgqueue[oobmsgqueuetail].len = length;
				memcpy(&oobmsgqueue[oobmsgqueuetail].addr, &addr, sizeof(multiaddr));

				oobmsgqueuetail = (oobmsgqueuetail + 1) % OOBQUEUESZ;

#ifdef CHATTY
				printf(">>> accepted OOB message into queue.\n");
#endif
				break;

			default:
#ifdef CHATTY
				printf("!!! peculiar message type\n");
#endif
				goto again;
		}
	}

	if ((workptr - workbuf) >= workbuflen) goto again;

	// we have a trailer
	// FIXME: Now is really where wrapping will screw us over

	workptr += getsequencenum(workptr, &seq);
	
	if (peers[peer].acksequence < seq) {	// FIXME: wrap!
		for (wseq=peers[peer].acksequence; wseq!=seq; wseq=wrapsequencenum(wseq+1)) {
			peers[peer].outptr[wseq] = 0;
			peers[peer].outlen[wseq] = 0;
		}
	}
	
	if (seq < peers[peer].acksequence) goto again;
	peers[peer].acksequence = seq;
#ifdef CHATTY
	printf("--- peer awaits %d, we await %d\n", seq, peers[peer].insequence);
#endif
	if (peers[peer].outlen[seq] > 0) {
		peers[peer].outage[seq] = MAX_RETRANSMIT_THRESH;
	}
	/*
	i = workbuflen - (workptr-workbuf);
	seq = wrapsequencenum(seq+1);
	while (i>0) {
		k = *(workptr++);
		for (j=0;j<8;j++) {
			if (!(k & pow2long[j]) && peers[peer].outlen[seq] > 0) {
				peers[peer].outage[seq] = MAX_RETRANSMIT_THRESH;
			}
			seq = wrapsequencenum(seq+1);
		}
		i--;
	}*/
	
	goto again;
}

static void processoutgoing(void)
{
	short peer;
	unsigned char workbuf[MAX_PACKET_LEN], *workptr;
	unsigned char msgcount;
	
	int i;
	unsigned long crc;
	unsigned long myclock;
	sequencenum seq;

	unsigned char resume=0, todo=0xff;


	myclock = lastclock;
	agemessages();

	for (peer=connecthead; peer!=-1; peer=connectpoint2[peer]) {
		// add an extra number of recent unacknowledged messages as well
		if (peers[peer].nextsequence != peers[peer].acksequence) {
			i = peers[peer].stats.overlapcount;
			seq = wrapsequencenum(peers[peer].nextsequence-1);
			do {
				if (peers[peer].outlen[seq] > 0) {
					peers[peer].outage[seq] = MAX_RETRANSMIT_THRESH;
					i--;
				}
				seq = wrapsequencenum(seq-1);
			} while (seq != peers[peer].acksequence && i>0);
		}

		resume = 0;
		todo = 0xff;

resumepoint:
		memset(workbuf, 0, MAX_PACKET_LEN);
		workptr = workbuf + PACKET_HEAD_LEN;
		msgcount = 0;
		
		if (todo & 1) {
			// if any messages flagged for transmission add them to the packet
			if (!resume) seq = peers[peer].acksequence;
			for (; seq != peers[peer].nextsequence; seq = wrapsequencenum(seq+1)) {
				if (peers[peer].outlen[seq] == 0) continue;
				if (peers[peer].outage[seq] < peers[peer].stats.retransmittime) continue;
				if ((workptr - workbuf) + peers[peer].outlen[seq] > MAX_PACKET_LEN) {
					// data won't fit, so send the current packet on its way
#ifdef CHATTY
					printf("<<< packet full\n");
#endif
					resume = 1;
					goto dispatch;
				} else {
					// pack the data in
					memcpy(workptr, peers[peer].outptr[seq], peers[peer].outlen[seq]);
					workptr += peers[peer].outlen[seq];
					msgcount++;
					peers[peer].outage[seq] = 0;
#ifdef CHATTY
					printf("<<< added msg %d (%d bytes)\n",
							seq, peers[peer].outlen[seq]);
#endif
				}
			}
			resume = 0;
			todo &= ~1;
		}

		if (todo & 2) {
			// if any oob messages queued add them to the packet
			while (peers[peer].oobhead != peers[peer].oobtail) {
				if ((workptr - workbuf) + peers[peer].ooblen[ peers[peer].oobhead ] > MAX_PACKET_LEN) {
					// message won't fit, so send the current packet on its way
#ifdef CHATTY
					printf("<<< packet full\n");
#endif
					resume = 1;
					goto dispatch;
				} else {
					// pack the data in
					memcpy(workptr, peers[peer].oobptr[ peers[peer].oobhead ],
							peers[peer].ooblen[ peers[peer].oobhead ]);
					workptr += peers[peer].ooblen[ peers[peer].oobhead ];

					msgcount++;
#ifdef CHATTY
					printf("<<< added oob (%d bytes)\n",
							peers[peer].ooblen[ peers[peer].oobhead ]);
#endif
					peers[peer].oobhead = (peers[peer].oobhead + 1) % OOBQUEUESZ;
				}
			}
			resume = 0;
			todo &= ~2;
		}

dispatch:
		// if any data was added to the packet, checksum and send the packet on its way,
		// however, we only send it if some messages were added, or, the clock moved,
		// or we explicitly want the packet to be transmitted (ie. if we got a msg this frame)
		// otherwise we succeed only in flooding the network every frame with extra packets
		if ((workptr - workbuf) > PACKET_HEAD_LEN &&
		    ((msgcount > 0) || (myclock != lastclock) || (peers[peer].flags & PEERFLAG_FORCETX))) {

			if ((workptr - workbuf) + SIZE_SEQUENCENUM <= MAX_PACKET_LEN) {
				// sending insequence will fit
				seq = peers[peer].insequence;
				while (peers[peer].inlen[seq] > 0)	// seek to the first message we're missing
					seq = wrapsequencenum(seq + 1);

				workptr += putsequencenum(workptr, seq);
			}
			
			workbuf[4] = msgcount;
			
			crc = crc32(workbuf + 4, workptr - workbuf - 4);
			workbuf[0] = crc & 255;
			workbuf[1] = (crc>>8) & 255;
			workbuf[2] = (crc>>16) & 255;
			workbuf[3] = (crc>>24) & 255;

			networksend(&peers[peer].address, workbuf, workptr - workbuf);

			if (resume) goto resumepoint;

			peers[peer].flags &= ~PEERFLAG_FORCETX;
		}
	}
}


/*
 * Peers
 */

static void resetpeer(int p)
{
	int i;

	for (i=0;i<NUM_SEQUENCES;i++) {
		peers[p].outptr[i] = 0;
		peers[p].outlen[i] = 0;
		peers[p].outage[i] = 0;
			
		peers[p].inptr[i] = 0;
		peers[p].inlen[i] = 0;
	}

	peers[p].nextsequence = 0;
	peers[p].acksequence = 0;

	peers[p].oobhead = 0;
	peers[p].oobtail = 0;
	
	peers[p].insequence = 0;

	peers[p].address.type = ADDR_NONE;
	peers[p].uniqueid = -1;

	peers[p].flags = 0;

	peers[p].stats.ping = 65535;
	peers[p].stats.retransmittime = (MIN_RETRANSMIT_THRESH+MAX_RETRANSMIT_THRESH)>>1;
	peers[p].stats.overlapcount = 3;

	for (i=0;i<LOSSOMETERSZ;i++) {
		peers[p].lossometer[i].sentat = 0l;
		peers[p].lossometer[i].returnedat = 0xffffffffl;
	}
	peers[p].lossometerhead = 0;
	peers[p].lossometerlast = 0;
}


/*
 * Public interface
 */

void initmultiplayers(char damultioption, char dacomrateoption, char dapriority)
{
	int i,j;

	if (damultioption == 0 && multioption > 0)
		uninitmultiplayers();	// going back to singleplayer
	
	numplayers = 1; myconnectindex = 0;
	if (damultioption == 0) {
		connecthead = 0; connectpoint2[0] = -1;
	} else {
		connecthead = -1;
	}

	outmsghead = 0;
	inmsghead = 0;
	oobmsgqueuehead = oobmsgqueuetail = 0;
	fetchhead = 0;
	for (i=0;i<MAXMULTIPLAYERS;i++)
		resetpeer(i);

	multioption = damultioption;
	if (damultioption == 0) return;
	
	if (startnetwork() < 0) {
		initprintf("Network system initialization failed!\n");
		multioption = 0;	// failed initialising network so fall back to singleplayer
	}

	initcrc32table();

	lastclock = totalclock;

#ifdef INCOMPETENCE
	srand((long)time(NULL));
#endif
}

void setpackettimeout(long datimeoutcount, long daresendagaincount)
{
}

void uninitmultiplayers(void)
{
	stopnetwork();

	multidumpstats();
	
	numplayers = 1; myconnectindex = 0;
	connecthead = 0; connectpoint2[0] = -1;
	multioption = 0;
}

void sendlogon(void)
{
}

void sendlogoff(void)
{
	long i;
	char tempbuf[2];

	tempbuf[0] = 255;
	tempbuf[1] = myconnectindex;
	for(i=connecthead;i>=0;i=connectpoint2[i])
		if (i != myconnectindex)
			sendpacket(i,tempbuf,2L);
}

long getoutputcirclesize(void)
{
	return 0;
}

void setsocket(short newsocket)
{
}

void sendpacket(long other, char *bufptr, long messleng) { _sendpacket(other,bufptr,messleng,0); }
static void _sendpacket(long other, char *bufptr, long messleng, unsigned char fl)
{
	unsigned char *p;
	int all;

	if (multioption == 0) return;
	if (messleng > MAX_MESSAGE_LEN || messleng <= 0 || !bufptr) return;

	if (other < 0) {
#ifdef CHATTY
		printf("<<< broadcasting\n");
#endif
		all = 1;
		other = connecthead;
	} else {
		all = 0;
	}

	for (; other != -1; other = connectpoint2[other]) {
		if (all && other == myconnectindex) continue;
		if (peers[other].address.type == ADDR_NONE) continue;	// can't send to a null address

		if (wrapsequencenum(peers[other].nextsequence + 1) == peers[other].acksequence) {
#ifdef CHATTY
			printf("<<< queue full for peer %d\n", other);
#endif
			if (!all) break;
			continue;	// full!
		}

		p = allocatestorage(ALLOC_OUT, MESSAGE_HEAD_LEN + messleng);
		if (!p) break;

		p[0] = fl|MSGTYPE_SYNC;			// flag as synchronous
		p[1] = (messleng - 1) & 255;	// message length
		putsequencenum(p+2,peers[other].nextsequence);	// sequence

		memcpy(p+MESSAGE_HEAD_LEN, bufptr, messleng);

		peers[other].outptr[ peers[other].nextsequence ] = p;
		peers[other].outlen[ peers[other].nextsequence ] = MESSAGE_HEAD_LEN + messleng;
		peers[other].outage[ peers[other].nextsequence ] = MAX_RETRANSMIT_THRESH;

#ifdef CHATTY
		printf("<<< queued %d bytes to peer %d\n", messleng, other);
#endif

		peers[other].nextsequence = wrapsequencenum(peers[other].nextsequence + 1);

		if (!all) break;
	}
}

void sendoobpacket(long other, char *bufptr, long messleng) { _sendoobpacket(other,bufptr,messleng,0); }
static void _sendoobpacket(long other, char *bufptr, long messleng, unsigned char fl)
{
	unsigned char *p;
	int all;
	
	if (multioption == 0) return;
	if (messleng > MAX_MESSAGE_LEN || messleng <= 0 || !bufptr) return;
	
	if (other < 0) {
#ifdef CHATTY
		printf("<<- broadcasting\n");
#endif
		all = 1;
		other = connecthead;
	} else {
		all = 0;
	}
	
	for (; other != -1; other = connectpoint2[other]) {
		if (all && other == myconnectindex) continue;
		if (peers[other].address.type == ADDR_NONE) continue;	// can't send to a null address

		if ((peers[ other ].oobtail + 1) % OOBQUEUESZ == peers[ other ].oobhead) {
#ifdef CHATTY
			printf("<<- queue full for peer %d\n", other);
#endif
			if (!all) break;
			continue;	// full
		}

		p = allocatestorage(ALLOC_OUT, 2 + messleng);
		if (!p) break;

		p[0] = fl|MSGTYPE_OOB;			// flag as OOB
		p[1] = (messleng - 1) & 255;	// message length

		memcpy(p+2, bufptr, messleng);
	
		peers[ other ].oobptr[ peers[ other ].oobtail ] = p;
		peers[ other ].ooblen[ peers[ other ].oobtail ] = 2 + messleng;
		peers[ other ].oobtail = (peers[ other ].oobtail + 1) % OOBQUEUESZ;

#ifdef CHATTY
		printf("<<- queued %d OOB bytes to peer %d\n", messleng, other);
#endif

		if (!all) break;
	}
}

void sendoobpacketto(multiaddr *other, char *bufptr, long messleng) { _sendoobpacketto(other,bufptr,messleng,0); }
static void _sendoobpacketto(multiaddr *other, char *bufptr, long messleng, unsigned char fl)
{
	unsigned char workbuf[ PACKET_HEAD_LEN + MESSAGE_HEAD_LEN + MAX_MESSAGE_LEN ];
	unsigned char *workptr;
	long i;
#ifdef CHATTY
	char temp[64];
#endif

	if (multioption == 0) return;
	if (messleng > MAX_MESSAGE_LEN || messleng <= 0 || !other || !bufptr) return;
	
	workptr = workbuf + 4;	// leave out the CRC for a moment
	
	*(workptr++) = 1;	// message count

	*(workptr++) = fl|MSGTYPE_OOB;			// flag as OOB
	*(workptr++) = (messleng - 1) & 255;	// message length

	memcpy(workptr, bufptr, messleng);
	workptr += messleng;

	// finally, the CRC
	i = crc32(workbuf + 4, workptr - workbuf - 4);
	workbuf[0] = i & 255;
	workbuf[1] = (i>>8) & 255;
	workbuf[2] = (i>>16) & 255;
	workbuf[3] = (i>>24) & 255;

#ifdef CHATTY
	multiaddrtostring(other, temp, 64);
	printf("<-- sending %d bytes to %s\n",workptr-workbuf,temp);
#endif

	networksend(other, workbuf, workptr - workbuf);
}

short getpacket (long *other, char *bufptr)
{
	short h, l=0;
	
	if (multioption == 0) return 0;

	processincoming();

	h = fetchhead;
	do {
#ifdef CHATTY
		//printf(">>> from peer %d we expect msg %d and need ack for msg %d\n",
		//				h, peers[h].insequence, peers[h].acksequence);
#endif
		if (peers[h].inlen[ peers[h].insequence ] > 0) {
			if (peers[h].inflags[ peers[h].insequence ] & MSGTYPE_INTERNAL) {
#ifdef CHATTY
				printf(">>> internal %d byte msg %d\n", peers[h].inlen[ peers[h].insequence ], peers[h].insequence);
#endif

				processinternalmsg(h, peers[h].inptr[ peers[h].insequence ], peers[h].inlen[ peers[h].insequence ]);
			} else {
				*other = h;
				l = peers[h].inlen[ peers[h].insequence ];
				memcpy(bufptr, peers[h].inptr[ peers[h].insequence ], l);

#ifdef CHATTY
				printf(">>> dequeued %d byte msg %d\n", l, peers[h].insequence);
#endif
			}
			
			peers[h].inptr[ peers[h].insequence ] = 0;
			peers[h].inlen[ peers[h].insequence ] = 0;

			peers[h].insequence = wrapsequencenum(peers[h].insequence + 1);
		}

		h = connectpoint2[h];
		if (h == -1) h = connecthead;
	} while (h != fetchhead && l == 0);

	fetchhead = h;

	processoutgoing();
	
	return l;
}

int fetchoobmessage(multiaddr *addr, char *bufptr, long *messleng)
{
	int rv=0;
#ifdef CHATTY
	char temp[64];
#endif

	if (multioption == 0) return 0;

	processincoming();
	
	if (!addr || !bufptr || !messleng) {
		rv = -2;
		goto finish;
	}
	if (oobmsgqueuehead == oobmsgqueuetail) {
		rv = 0;
		goto finish;
	}

	if (*messleng < oobmsgqueue[oobmsgqueuehead].len) rv = -1;
	else {
		memcpy(addr, &oobmsgqueue[oobmsgqueuehead].addr, sizeof(multiaddr));
		memcpy(bufptr, oobmsgqueue[oobmsgqueuehead].ptr, oobmsgqueue[oobmsgqueuehead].len);
		*messleng = oobmsgqueue[oobmsgqueuehead].len;
		rv = 1;
	}

	oobmsgqueuehead = (oobmsgqueuehead + 1) % OOBQUEUESZ;

#ifdef CHATTY
	multiaddrtostring(addr, temp, 64);
	printf("->> dequeued %d byte message from %s\n",*messleng,temp);
#endif

finish:
	processoutgoing();

	return rv;
}

void flushpackets(void)
{
}

#define  CMD_SEND               1
#define  CMD_GET                2
#define  CMD_SENDTOALL          3
#define  CMD_SENDTOALLOTHERS    4
#define  CMD_SCORE              5
void genericmultifunction(long other, char *bufptr, long messleng, long command)
{
	short p;
	
	switch (command) {
		case CMD_GET:	// kinda unused
			break;
				
		case CMD_SEND:
			if (other == 0) other = myconnectindex;
			else other--;
			sendpacket(other, bufptr, messleng);
			break;
			
		case CMD_SENDTOALL:
			for (p = connecthead; p != -1; p = connectpoint2[p])
				sendpacket(p, bufptr, messleng);
			break;
			
		case CMD_SENDTOALLOTHERS:
			for (p = connecthead; p != -1; p = connectpoint2[p])
				if (p != myconnectindex) sendpacket(p, bufptr, messleng);
			break;
			
		case CMD_SCORE:
			break;
	}
}

void multiidle(void)
{
	unsigned long ticks;
	short h;
	unsigned char workbuf[5];

	if (multioption == 0) return;

	ticks = totalclock;

	workbuf[0] = OOBMSG_PINGREQ;
	workbuf[1] = ticks & 255;
	workbuf[2] = (ticks >> 8) & 255;
	workbuf[3] = (ticks >> 16) & 255;
	workbuf[4] = (ticks >> 24) & 255;

	for (h=connecthead; h!=-1; h=connectpoint2[h]) {
		if (h==myconnectindex) continue;
		if (peers[h].address.type == ADDR_NONE) continue;
		if ((ticks - peers[h].lossometerlast) >= 70) {	// 1sec in KenBuild
			_sendoobpacket(h, workbuf, 5, MSGTYPE_INTERNAL);
			peers[h].lossometer[ peers[h].lossometerhead ].sentat = ticks;
			peers[h].lossometer[ peers[h].lossometerhead ].returnedat = 0xffffffffl;

			peers[h].lossometerhead++;
			if (peers[h].lossometerhead == LOSSOMETERSZ) peers[h].lossometerhead = 0;

			peers[h].lossometerlast = ticks;
		}
	}
}

int multiaddplayer(char *str, int id)
{
	short p;
	multiaddr addr;
	char temp[64];

	struct {
		int point;
		int unique;
	} sortbuf[MAXMULTIPLAYERS], tempsort;
	int q;

	if (str) {
		if (multistringtoaddr(str, &addr)) {
			//initprintf("Failed adding %s to game.\n", str);
			return -1;
		}
	} else {
		addr.type = ADDR_NONE;
	}

	//multiaddrtostring(&addr,temp,64);

	if (connecthead == -1) {
		connecthead = 0;
		connectpoint2[0] = -1;
		p = 0;
	} else {
		for (p = connecthead; connectpoint2[p] != -1; p = connectpoint2[p]) ;
		connectpoint2[p] = p+1;
		connectpoint2[p+1] = -1;
		numplayers++;
		p++;
	}

	memcpy(&peers[p].address, &addr, sizeof(multiaddr));
	peers[p].uniqueid = id;

	// sort the player list by uniqueid
	q=0;
	for (p = connecthead; p != -1; p = connectpoint2[p]) {
		sortbuf[q].point = p;
		sortbuf[q].unique = peers[p].uniqueid;
		q++;
	}

	for (q=numplayers;q>0;q--) {	// bubble-sort for life!
		for (p=0; p<numplayers-1; p++) {
			if (sortbuf[p].unique > sortbuf[p+1].unique) {
				tempsort = sortbuf[p];
				sortbuf[p] = sortbuf[p+1];
				sortbuf[p+1] = tempsort;
			}
		}
	}

	for (q=0;q<numplayers;q++) {
		if (q==0) p = connecthead = sortbuf[q].point;
		else { connectpoint2[p] = p = sortbuf[q].point; }
		if (peers[p].uniqueid == myuniqueid) myconnectindex = p;
	}
	connectpoint2[p]=-1;

	//initprintf("Added %s (%s) to game as player %d.\n", temp, str, numplayers-1);

	return 0;
}

int multicollectplayers(void)
{
	return 0;
}

void multidumpstats(void)
{
	int i;
	char temp[64];
	
	printf("Networking statistics summary\n");

	if (multioption != 0) {
		for (i=connecthead; i!=-1; i=connectpoint2[i]) {
			multiaddrtostring(&peers[i].address, temp, 64);
			printf("Player %d: %s\n", i, temp);
		}
	}
	
	printf("TX Bytes/Packets/Errors:  %d/%d/%d\nRX Bytes/Packets/Errors:  %d/%d/%d\n",
			stats.txbytes,stats.txpackets,stats.txerrors,
			stats.rxbytes,stats.rxpackets,stats.rxerrors);
}

int whichpeer(multiaddr *a)
{
	int peer;

	if (multioption == 0) return -1;

	for (peer=connecthead; peer!=-1; peer=connectpoint2[peer])
		if (!multicompareaddr(&peers[peer].address, a))
			return peer;

	return -1;
}

int multigetpeerstats(long other, multipeerstats *stats)
{
	if (!stats) return -1;

	Bmemcpy(stats, &peers[other].stats, sizeof(multipeerstats));
	return 0;
}

int multigetpeeraddr(long other, multiaddr *addr, int *uniqueid)
{
	int rv = -1;
	if (addr) { Bmemcpy(addr, &peers[other].address, sizeof(multiaddr)); rv = 0; }
	if (uniqueid) { *uniqueid = peers[other].uniqueid; rv = 0; }
	return rv;
}



/*
 * Internal network transport stuff
 */

static int startnetwork(void)
{
	int rv;
	struct sockaddr_in sadr;
#ifdef WINDOWS
	WSADATA wsa;
	char *e;

	if (!winsockinited) {
		rv = WSAStartup(MAKEWORD(1,1), &wsa);
		if (rv) {
			switch (rv) {
				case WSASYSNOTREADY: e = "Networking subsystem not ready (WSASYSNOTREADY)"; break;
				case WSAVERNOTSUPPORTED: e = "Requested Winsock version not available (WSAVERNOTSUPPORTED)"; break;
				case WSAEINPROGRESS: e = "Operation in progress (WSAINPROGRESS)"; break;
				case WSAEPROCLIM: e = "Task limit reached (WSAPROCLIM)"; break;
				case WSAEFAULT: e = "Invalid data pointer passed (WSAEFAULT)"; break;
				default: e = "Unknown Winsock error"; break;
			}
			initprintf("Windows Sockets startup error: %s\n", e);
			return -1;
		}
	
		if (wsa.wVersion != MAKEWORD(1,1)) {
			initprintf("Windows Sockets startup error: Incorrect Winsock version returned (%d.%d)\n",
					LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));
			WSACleanup();
			return -1;
		}

		initprintf("Windows Sockets initialised\n"
	                   "  Available version:     %d.%d\n"
		           "  Max supported version: %d.%d\n"
		           "  Driver description:    %s\n"
		           "  System status:         %s\n"
		           "  Max sockets:           %d\n"
		           "  Max UDP datagram size: %d\n",
			   LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion),
			   LOBYTE(wsa.wHighVersion), HIBYTE(wsa.wHighVersion),
			   wsa.szDescription, wsa.szSystemStatus, wsa.iMaxSockets, wsa.iMaxUdpDg);
		winsockinited = 1;
	}
#endif

	if (ipsocket == -1) {
		ipsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (ipsocket < 0) return -1;
	}

	// enable non-blocking IO
	rv = 1;
#ifdef WINDOWS
	if (ioctlsocket(ipsocket, FIONBIO, (u_long*)&rv) < 0) {
#else
	if (ioctl(ipsocket, FIONBIO, &rv) < 0) {
#endif
		stopnetwork();
		return -1;
	}
/*
	// enable broadcast ability
	rv = 1;
	if (setsockopt(ipsocket, SOL_SOCKET, SO_BROADCAST, (char *)&rv, sizeof(rv)) < 0) {
		stopnetwork();
		return -1;
	}
*/	
	sadr.sin_family = AF_INET;
	sadr.sin_addr.s_addr = htonl(INADDR_ANY);
	sadr.sin_port = htons(ipport);
	if (bind(ipsocket, (struct sockaddr *)&sadr, sizeof(sadr)) < 0) {
		stopnetwork();
		return -1;
	}
	
	return 0;
}

static void stopnetwork(void)
{
	if (ipsocket != -1) {
		shutdown(ipsocket, SHUT_RDWR);
#ifdef WINDOWS
		closesocket(ipsocket);
#else
		close(ipsocket);
#endif
		ipsocket = -1;
	}

#ifdef WINDOWS
	if (winsockinited) {
		WSACleanup();
		winsockinited = 0;
	}
#endif
}

static void networksend(multiaddr *addr, void *buf, unsigned long len)
{
#ifdef DUMPEACHPACKET
	char straddr[64];
	unsigned i;
#endif
#ifdef INCOMPETENCE
	unsigned j;
#endif
	int err;

	if (!addr || !buf) return;

#ifdef DUMPEACHPACKET
	multiaddrtostring(addr, straddr, 64);
	printf("To:     %s\n"
	       "Length: %d\n"
	       "Contents:\n",
	       straddr, len);
	for (i=0;i<len;i++) {
		if (i % 16 == 0) printf("   %4x: ", i);
		printf("%02x ", ((unsigned char *)buf)[i]);
		if (i % 16 == 15) printf("\n");
	}
	if (len&15) printf("\n");
	printf("\n");
#endif

#ifdef INCOMPETENCE
	j=rand();
	if (CORRUPTAGE(j)) {
#ifdef CHATTY
		printf("networksend() is spiking the drink\n");
#endif
		((char*)buf)[rand() % len] = rand()&255;
	} else if (LOSSAGE(j)) {
#ifdef CHATTY
		printf("networksend() has slippery fingers\n");
#endif
		return;
	}
#endif
	
	if (addr->type == ADDR_NONE) return;
	if (addr->type == ADDR_IPV4) {
		if ((err = sendto(ipsocket, buf, len, 0, (struct sockaddr *)&addr->addr.ipv4, sizeof(struct sockaddr_in)))< 0) {
#ifdef CHATTY
#ifdef WINDOWS
			err = WSAGetLastError();
#else
			err = errno;
#endif
			printf("sendto() error %d\n", err);
#endif
			stats.txerrors++;
			return;
		}
#ifdef CHATTY
		printf("sendto() returned %d\n",err);
#endif

		stats.txbytes += err;
		stats.txpackets++;

		return;
	}
}

static int networkrecv(multiaddr *addr, void *buf, unsigned long *len)
{
#ifdef DUMPEACHPACKET
	char straddr[64];
	unsigned i;
#endif
	int fromlen;
	struct sockaddr *sad;
	int rv;
#ifdef INCOMPETENCE
	int j;
#endif
	
	if (!addr) return -1;

	sad = (struct sockaddr *)&addr->addr;

	fromlen = sizeof(addr->addr);
	rv = recvfrom(ipsocket, buf, *len, 0, sad, &fromlen);

	if (sad->sa_family == AF_INET)
		addr->type = ADDR_IPV4;
	else
		addr->type = ADDR_NONE;

	if (rv > 0) {
		*len = rv;
		stats.rxbytes += rv;
		stats.rxpackets++;

#ifdef DUMPEACHPACKET
		multiaddrtostring(addr, straddr, 64);
		printf("From:   %s\n"
		       "Length: %d\n"
		       "Contents:\n",
		       straddr, rv);
		for (i=0;i<rv;i++) {
			if (i % 16 == 0) printf("   %4x: ", i);
			printf("%02x ", ((unsigned char *)buf)[i]);
			if (i % 16 == 15) printf("\n");
		}
		if (rv&15) printf("\n");
		printf("\n");
#endif
	
#ifdef INCOMPETENCE
		j=rand();
		if (CORRUPTAGE(j)) {
#ifdef CHATTY
			printf("networkrecv() has spiked the drink\n");
#endif
			((char*)buf)[rand() % rv] = rand()&255;
		} else if (LOSSAGE(j)) {
#ifdef CHATTY
			printf("networkrecv() had slippery fingers\n");
#endif
			*len = 0;
		}
#endif
	} else {
		*len = 0;
	}

	return rv;
}



/*
 * Game-land extras
 */

//
// multiaddrtostring() -- Converts the multiaddr address passed into a string
//   which is more human-readable
//
int multiaddrtostring(multiaddr *addr, char *buf, int len)
{
	if (!addr) return -1;
	
	if (addr->type == ADDR_IPV4) {
#ifdef WINDOWS
//		if (!WSAAddressToString((LPSOCKADDR)&addr->addr.ipv4, sizeof(struct sockaddr_in), NULL, buf, (LPDWORD)&len))
//			return 0;
		char *t;
		t = inet_ntoa(addr->addr.ipv4.sin_addr);
		if (t) {
			Bsnprintf(buf,len,"%s:%d",t,ntohs(addr->addr.ipv4.sin_port));
			return 0;
		}
#else
		char t[64];
		int l=64;
	
		if (inet_ntop(AF_INET, &addr->addr.ipv4, t, l)) {
			Bsnprintf(buf,len,"%s:%d",t,ntohs(addr->addr.ipv4.sin_port));
			return 0;
		}
#endif
	} else {
		Bstrncpy(buf,"Invalid",len);
	}

	return -1;
}


//
// multistringtoaddr() -- Resolves a hostname and stores the result in a multiaddr
//
int multistringtoaddr(char *str, multiaddr *addr)
{
	struct hostent *he;
	int port = ipport, t;
	char *host, *p, *p2;

	host = Balloca(strlen(str)+1);
	strcpy(host,str);

	p = strchr(host,':');
	if (p) {
		*(p++) = 0;
		t = strtol(p, &p2, 10);
		if (p != p2 && *p2 == 0) port = t;
	}

	memset(addr, 0, sizeof(multiaddr));
	he = gethostbyname(host);
	if (!he) {
		switch (h_errno) {
			case HOST_NOT_FOUND: OSD_Printf("Name resolution error: %s: Host not found\n", host); break;
			case NO_DATA: OSD_Printf("Name resolution error: %s: No address for this hostname\n", host); break;
			case NO_RECOVERY: OSD_Printf("Name resolution error: %s: Unrecoverable error\n", host); break;
			case TRY_AGAIN: OSD_Printf("Name resolution error: %s: Temporary name server error\n", host); break;
		}
		
		return -1;
	}

	if (he->h_addrtype == AF_INET) {
		addr->type = ADDR_IPV4;
		addr->addr.ipv4.sin_family = AF_INET;
		memcpy(&addr->addr.ipv4.sin_addr, he->h_addr_list[0], he->h_length);
		addr->addr.ipv4.sin_port = htons(port);
	}

	return 0;
}


//
// multicompareaddr() -- Compares two addresses. Returns nonzero on difference.
//
int multicompareaddr(multiaddr *addr1, multiaddr *addr2)
{
	if (addr1->type != addr2->type) return 1;
	switch (addr1->type) {
		case ADDR_NONE:
			return 0;
		case ADDR_IPV4:
			if (addr1->addr.ipv4.sin_addr.s_addr != addr2->addr.ipv4.sin_addr.s_addr)
				return 1;
			if (addr1->addr.ipv4.sin_port != addr2->addr.ipv4.sin_port)
				return 1;
			return 0;
		default:
			return 1;
	}
}



/*
 * vim:ts=4:sw=4:
 */

