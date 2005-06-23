// cache1d.h

#ifndef __cache1d_h__
#define __cache1d_h__

void	initcache(long dacachestart, long dacachesize);
void	allocache(long *newhandle, long newbytes, char *newlockptr);
void	suckcache(long *suckptr);
void	agecache(void);

int     addsearchpath(const char *p);
int     openfrompath(const char *fn, int flags, int mode);
BFILE  *fopenfrompath(const char *fn, const char *mode);

long	initgroupfile(char *filename);
void	uninitsinglegroupfile(long grphandle);
void	uninitgroupfile(void);
long	kopen4load(char *filename, char searchfirst);	// searchfirst: 0 = anywhere, 1 = first group, 2 = any group
long	kread(long handle, void *buffer, long leng);
long	klseek(long handle, long offset, long whence);
long	kfilelength(long handle);
long	ktell(long handle);
void	kclose(long handle);

int	beginsearchgroup(char *ext);
int	getsearchgroupnext(char *name, long *size);

int	kdfread(void *buffer, bsize_t dasizeof, bsize_t count, long fil);
int	dfread(void *buffer, bsize_t dasizeof, bsize_t count, BFILE *fil);
void	dfwrite(void *buffer, bsize_t dasizeof, bsize_t count, BFILE *fil);

long	lzwcompress(char *lzwinbuf, long uncompleng, char *lzwoutbuf);
long	lzwuncompress(char *lzwinbuf, long compleng, char *lzwoutbuf);

#endif // __cache1d_h__

