// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jonof@edgenetwk.com)

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif

#define min(a,b) ( ((a) < (b)) ? (a) : (b) )

#define MAXFILES 4096

static char buf[65536];

static long numfiles;
static char filespec[MAXFILES][128], filelist[MAXFILES][16];
static long fileleng[MAXFILES];

void findfiles(char *dafilespec);

int main(short argc, char **argv)
{
	long i, j, k, l, fil, fil2;
	char stuffile[16], filename[128];

	if (argc < 3)
	{
		printf("KGROUP [grouped file][@file or filespec...]           by Kenneth Silverman\n");
		printf("   This program collects many files into 1 big uncompressed file called a\n");
		printf("   group file\n");
		printf("   Ex: kgroup stuff.dat *.art *.map *.k?? palette.dat tables.dat\n");
		printf("      (stuff.dat is the group file, the rest are the files to add)\n");
		exit(0);
	}

	numfiles = 0;
	for(i=argc-1;i>1;i--)
	{
		strcpy(filename,argv[i]);
		if (filename[0] == '@')
		{
			if ((fil = open(&filename[1],O_BINARY|O_RDONLY,S_IREAD)) != -1)
			{
				l = read(fil,buf,65536);
				j = 0;
				while ((j < l) && (buf[j] <= 32)) j++;
				while (j < l)
				{
					k = j;
					while ((k < l) && (buf[k] > 32)) k++;

					buf[k] = 0;
					findfiles(&buf[j]);
					j = k+1;

					while ((j < l) && (buf[j] <= 32)) j++;
				}
				close(fil);
			}
		}
		else
			findfiles(filename);
	}

	strcpy(stuffile,argv[1]);

	if ((fil = open(stuffile,O_BINARY|O_TRUNC|O_CREAT|O_WRONLY,S_IWRITE)) == -1)
	{
		printf("Error: %s could not be opened\n",stuffile);
		exit(0);
	}
	strcpy(filename,"KenSilverman");
	write(fil,filename,12);
	write(fil,&numfiles,4);
	write(fil,filelist,numfiles<<4);

	for(i=0;i<numfiles;i++)
	{
		printf("Adding %s...\n",filespec[i]);
		if ((fil2 = open(filespec[i],O_BINARY|O_RDONLY,S_IREAD)) == -1)
		{
			printf("Error: %s not found\n",filespec[i]);
			close(fil);
			exit(0);
		}
		for(j=0;j<fileleng[i];j+=65536)
		{
			k = min(fileleng[i]-j,65536);
			read(fil2,buf,k);
			if (write(fil,buf,k) < k)
			{
				close(fil2);
				close(fil);
				printf("OUT OF HD SPACE!\n");
				exit(0);
			}
		}
		close(fil2);
	}
	close(fil);
	printf("Saved to %s.\n",stuffile);

	return 0;
}

static char *matchstr = "*.*";
int checkmatch(const struct dirent *a)
{
	long i, j, k;
	char ch1, ch2, bad, buf1[12], buf2[12];

	if (a->d_reclen > 12) {
		printf("%s too long, skipping.\n", a->d_name);
		return 0;		// name too long, skip it
	}

	for(k=0;k<12;k++) buf1[k] = 32;
	j = 0;
	for(k=0;matchstr[k];k++)
	{
		if (matchstr[k] == '.') j = 8;
		buf1[j++] = matchstr[k];
	}

	for(k=0;k<12;k++) buf2[k] = 32;
	j = 0;
	for(k=0;a->d_name[k];k++)
	{
		if (a->d_name[k] == '.') j = 8;
		buf2[j++] = a->d_name[k];
	}

	bad = 0;
	for(j=0;j<12;j++)
	{
		ch1 = buf1[j]; if ((ch1 >= 97) && (ch1 <= 123)) ch1 -= 32;
		ch2 = buf2[j]; if ((ch2 >= 97) && (ch2 <= 123)) ch2 -= 32;
		if (ch1 == '*')
		{
			if (j < 8) j = 8; else j = 12;
			continue;
		}
		if ((ch1 != '?') && (ch1 != ch2)) { bad = 1; break; }
	}
	if (bad == 0) return 1;
	return 0;
}

long filesize(char *path, char *name)
{
	char p[1000];
	struct stat st;

	strcpy(p, path);
	strcat(p, "/");
	strcat(p, name);

	if (!stat(p, &st)) return st.st_size;
	return 0;
}

void findfiles(char *dafilespec)
{
	struct dirent *name;
	long daspeclen;
	char daspec[128], *dir;
	DIR *di;

	strcpy(daspec,dafilespec);
	daspeclen=strlen(daspec);
	while ((daspec[daspeclen] != '\\') && (daspec[daspeclen] != '/') && (daspeclen > 0)) daspeclen--;
	if (daspeclen > 0) {
		daspec[daspeclen]=0;
		dir = daspec;
		matchstr = &daspec[daspeclen+1];
	} else {
		dir = ".";
		matchstr = daspec;
	}

	di = opendir(dir);
	if (!di) return;

	while ((name = readdir(di))) {
		if (!checkmatch(name)) continue;

		strcpy(&filelist[numfiles][0],name->d_name);
		strupr(&filelist[numfiles][0]);
		fileleng[numfiles] = filesize(dir, name->d_name);
		filelist[numfiles][12] = (char)(fileleng[numfiles]&255);
		filelist[numfiles][13] = (char)((fileleng[numfiles]>>8)&255);
		filelist[numfiles][14] = (char)((fileleng[numfiles]>>16)&255);
		filelist[numfiles][15] = (char)((fileleng[numfiles]>>24)&255);

		strcpy(filespec[numfiles],dir);
		strcat(filespec[numfiles], "/");
		strcat(filespec[numfiles],name->d_name);

		numfiles++;
		if (numfiles > MAXFILES)
		{
			printf("FATAL ERROR: TOO MANY FILES SELECTED! (MAX is 4096)\n");
			exit(0);
		}
	}

	closedir(di);
}
