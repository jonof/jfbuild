// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.

#ifndef __editor_h__
#define __editor_h__

int loadsetup(const char *fn);	// from config.c

void editinput(void);
void clearmidstatbar16(void);

short getnumber256(char namestart[80], short num, long maxnumber, char sign);
short getnumber16(char namestart[80], short num, long maxnumber);
void printmessage256(char name[82]);
void printmessage16(char name[82]);

void getpoint(long searchxe, long searchye, long *x, long *y);
long getpointhighlight(long xplc, long yplc);

#endif
