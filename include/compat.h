// Compatibility declarations for things which might not be present in
// certain build environments

#ifndef __compat_h__
#define __compat_h__

// define to rewrite all 'B' versions to library functions
#define __compat_h_macrodef__

#include <stdarg.h>

#ifdef __compat_h_macrodef__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(LINUX) || defined(BSD)
#include <unistd.h>
#endif
#endif

#ifndef FP_OFF
#define FP_OFF(__p) ((unsigned)(__p))
#endif

#ifdef __compat_h_macrodef__

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define BO_BINARY O_BINARY
#define BO_TEXT   O_TEXT
#define	BO_RDONLY O_RDONLY
#define BO_WRONLY O_WRONLY
#define BO_RDWR   O_RDWR
#define	BO_APPEND O_APPEND
#define	BO_CREAT  O_CREAT
#define BO_TRUNC  O_TRUNC
#define BS_IRGRP  S_IRGRP
#define BS_IWGRP  S_IWGRP
#define	BS_IEXEC  S_IEXEC
#define	BS_IWRITE S_IWRITE
#define	BS_IREAD  S_IREAD
#define	BS_IFIFO  S_IFIFO
#define	BS_IFCHR  S_IFCHR
#define	BS_IFBLK  S_IFBLK
#define	BS_IFDIR  S_IFDIR
#define	BS_IFREG  S_IFREG
#define BSEEK_SET SEEK_SET
#define BSEEK_CUR SEEK_CUR
#define BSEEK_END SEEK_END
#else
#define BO_BINARY 0
#define BO_TEXT   1
#define	BO_RDONLY 2
#define BO_WRONLY 4
#define BO_RDWR   6
#define	BO_APPEND 8
#define	BO_CREAT  16
#define BO_TRUNC  32
#define BS_IRGRP  0
#define BS_IWGRP  0
#define	BS_IEXEC  1
#define	BS_IWRITE 2
#define	BS_IREAD  4
#define	BS_IFIFO  0x1000
#define	BS_IFCHR  0x2000
#define	BS_IFBLK  0x3000
#define	BS_IFDIR  0x4000
#define	BS_IFREG  0x8000
#define BSEEK_SET 0
#define BSEEK_CUR 1
#define BSEEK_END 2
#endif

#ifdef UNDERSCORES
#define ASMSYM(x) "_" x
#else
#define ASMSYM(x) x
#endif

#ifndef min
#define min(a,b) ( ((a) < (b)) ? (a) : (b) )
#endif

#ifndef max
#define max(a,b) ( ((a) > (b)) ? (a) : (b) )
#endif


#if defined(__WATCOMC__)

#define inline __inline
#define int64 __int64
#define longlong(x) x##i64

#elif defined(_MSC_VER)

#define inline __inline
#define int64 __int64
#define longlong(x) x##i64

#else

#define longlong(x) x##ll
typedef long long int64;

#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define BMAX_PATH 260


struct Bdirent {
	unsigned short namlen;
	char *name;
	unsigned mode;
	unsigned size;
	unsigned mtime;
};
typedef void BDIR;

BDIR*		Bopendir(const char *name);
struct Bdirent*	Breaddir(BDIR *dir);
int		Bclosedir(BDIR *dir);


#ifdef __compat_h_macrodef__
#define BFILE FILE
#define bsize_t size_t
#define bssize_t ssize_t
#else
typedef void          BFILE;
typedef unsigned long bsize_t;
typedef signed   long bssize_t;
#endif


#ifdef __compat_h_macrodef__
#define Brand rand
#define Balloca alloca
#define Bmalloc malloc
#define Bcalloc calloc
#define Brealloc realloc
#define Bfree free
#define Bopen open
#define Bclose close
#define Bwrite write
#define Bread read
#define Blseek lseek
#if defined(__GNUC__)
#define Btell(h) lseek(h,0,SEEK_CUR)
#else
#define Btell tell
#endif
#define Bfopen fopen
#define Bfclose fclose
#define Bfeof feof
#define Bfgetc fgetc
#define Brewind rewind
#define Bfgets fgets
#define Bfputc fputc
#define Bfputs fputs
#define Bfread fread
#define Bfwrite fwrite
#define Bfprintf fprintf
#define Bstrdup strdup
#define Bstrcpy strcpy
#define Bstrncpy strncpy
#define Bstrcmp strcmp
#define Bstrncmp strncmp
#if defined(__WATCOMC__) || defined(_MSC_VER)
#define Bstrcasecmp stricmp
#define Bstrncasecmp strnicmp
#else
#define Bstrcasecmp strcasecmp
#define Bstrncasecmp strncasecmp
#endif
#if defined(WINDOWS)
#define Bstrlwr strlwr
#define Bstrupr strupr
#endif
#define Bstrcat strcat
#define Bstrncat strncat
#define Bstrlen strlen
#define Bstrchr strchr
#define Bstrrchr strrchr
#define Batoi atoi
#define Batol atol
#define Bstrtol strtol
#define Bstrtoul strtoul
#define Bstrtod strtod
#define Btoupper toupper
#define Btolower tolower
#define Bmemcpy memcpy
#define Bmemmove memmove
#define Bmemchr memchr
#define Bmemset memset
#define Bmemcmp memcmp
#define Bprintf printf
#define Bsprintf sprintf
#ifdef _MSC_VER
#define Bsnprintf _snprintf
#define Bvsnprintf _vsnprintf
#else
#define Bsnprintf snprintf
#define Bvsnprintf vsnprintf
#endif
#define Bvfprintf vfprintf
#define Bgetcwd getcwd
#define Bgetenv getenv
#define Btime() time(NULL)

#else

int Brand(void);
void *Bmalloc(bsize_t size);
void Bfree(void *ptr);
int Bopen(const char *pathname, int flags, unsigned mode);
int Bclose(int fd);
bssize_t Bwrite(int fd, const void *buf, bsize_t count);
bssize_t Bread(int fd, void *buf, bsize_t count);
int Blseek(int fildes, int offset, int whence);
BFILE *Bfopen(const char *path, const char *mode);
int Bfclose(BFILE *stream);
int Bfeof(BFILE *stream);
int Bfgetc(BFILE *stream);
void Brewind(BFILE *stream);
char *Bfgets(char *s, int size, BFILE *stream);
int Bfputc(int c, BFILE *stream);
int Bfputs(const char *s, BFILE *stream);
bsize_t Bfread(void *ptr, bsize_t size, bsize_t nmemb, BFILE *stream);
bsize_t Bfwrite(const void *ptr, bsize_t size, bsize_t nmemb, BFILE *stream);
char *Bstrdup(const char *s);
char *Bstrcpy(char *dest, const char *src);
char *Bstrncpy(char *dest, const char *src, bsize_t n);
int Bstrcmp(const char *s1, const char *s2);
int Bstrncmp(const char *s1, const char *s2, bsize_t n);
int Bstrcasecmp(const char *s1, const char *s2);
int Bstrncasecmp(const char *s1, const char *s2, bsize_t n);
char *Bstrcat(char *dest, const char *src);
char *Bstrncat(char *dest, const char *src, bsize_t n);
bsize_t Bstrlen(const char *s);
char *Bstrchr(const char *s, int c);
char *Bstrrchr(const char *s, int c);
int Batoi(const char *nptr);
long Batol(const char *nptr);
long int Bstrtol(const char *nptr, char **endptr, int base);
unsigned long int Bstrtoul(const char *nptr, char **endptr, int base);
void *Bmemcpy(void *dest, const void *src, bsize_t n);
void *Bmemmove(void *dest, const void *src, bsize_t n);
void *Bmemchr(const void *s, int c, bsize_t n);
void *Bmemset(void *s, int c, bsize_t n);
int Bmemcmp(const void *s1, const void *s2, bsize_t n);
int Bprintf(const char *format, ...);
int Bsprintf(char *str, const char *format, ...);
int Bsnprintf(char *str, bsize_t size, const char *format, ...);
int Bvsnprintf(char *str, bsize_t size, const char *format, va_list ap);
char *Bgetcwd(char *buf, bsize_t size);
char *Bgetenv(const char *name);
#endif

int Bgethomedir(char *,unsigned);
int Bcorrectfilename(char *filename, int removefn);
char *Bgetsystemdrives(void);
bsize_t Bfilelength(int fd);
char *Bstrtoken(char *s, char *delim, char **ptrptr, int chop);
long wildmatch (const char *i, const char *j);

#if !defined(WINDOWS)
char *Bstrlwr(char *);
char *Bstrupr(char *);
#endif

#endif // __compat_h__

