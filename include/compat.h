// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __compat_h__
#define __compat_h__

#if defined(__cplusplus)
# if (__cplusplus < 199711L)
#  error Requires a C++98 compiler
# endif
#elif defined(__STDC__) && defined(__STDC_VERSION__)
# if (__STDC_VERSION__ < 199901L)
#  error Requires a C99 compiler
# endif
#elif defined(_MSC_VER)
# if (_MSC_VER < 1900)
#  error Requires MSVC 2015 or newer
# endif
#else
# error Requires a C99 compiler
#endif

#ifndef USE_COMPAT_H_BMACROS
#define USE_COMPAT_H_BMACROS 1
#endif

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 700
#endif
#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
# define _DARWIN_C_SOURCE 1
#endif

#if defined(_MSC_VER)
#define _CRT_DECLARE_NONSTDC_NAMES 0
#endif

#ifdef __cplusplus
# include <cerrno>
# include <climits>
# include <cstdarg>
# include <cstdio>
# include <cstring>
# include <cstdlib>
# include <ctime>
#else
# include <errno.h>
# include <limits.h>
# include <stdarg.h>
# include <stddef.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# ifndef _MSC_VER
#  include <strings.h>
# endif
#endif

#include <stdint.h>     // cstdint in C++11
#include <inttypes.h>   // cinttypes in C++11
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32)
# include <io.h>
# include <direct.h>   // for _getdcwd, _mkdir
#else
# include <unistd.h>
#endif

#ifdef EFENCE
# include <efence.h>
#endif

#if defined(_MSC_VER)
# ifdef _WIN64
typedef __int64 ssize_t;
# else
typedef int     ssize_t;
# endif
typedef long off_t;
#elif defined(__WATCOMC__)
# define inline __inline
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
# define INT64_C(x) x##i64
#endif

#ifndef NULL
# define NULL ((void *)0)
#endif

#if defined(__linux) || defined(__HAIKU__)
# include <endian.h>
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define B_LITTLE_ENDIAN 1
#  define B_BIG_ENDIAN    0
# elif __BYTE_ORDER == __BIG_ENDIAN
#  define B_LITTLE_ENDIAN 0
#  define B_BIG_ENDIAN    1
# endif
# define B_ENDIAN_C_INLINE 1

#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
# include <sys/endian.h>
# if _BYTE_ORDER == _LITTLE_ENDIAN
#  define B_LITTLE_ENDIAN 1
#  define B_BIG_ENDIAN    0
# elif _BYTE_ORDER == _BIG_ENDIAN
#  define B_LITTLE_ENDIAN 0
#  define B_BIG_ENDIAN    1
# endif
# define B_SWAP64(x) __bswap64(x)
# define B_SWAP32(x) __bswap32(x)
# define B_SWAP16(x) __bswap16(x)

#elif defined(__APPLE__)
# if defined(__LITTLE_ENDIAN__)
#  define B_LITTLE_ENDIAN 1
#  define B_BIG_ENDIAN    0
# elif defined(__BIG_ENDIAN__)
#  define B_LITTLE_ENDIAN 0
#  define B_BIG_ENDIAN    1
# endif
# include <libkern/OSByteOrder.h>
# define B_SWAP64(x) OSSwapConstInt64(x)
# define B_SWAP32(x) OSSwapConstInt32(x)
# define B_SWAP16(x) OSSwapConstInt16(x)

#elif defined(_WIN32)
# define B_LITTLE_ENDIAN 1
# define B_BIG_ENDIAN    0
# define B_ENDIAN_C_INLINE 1
#endif

#if !defined(B_LITTLE_ENDIAN) || !defined(B_BIG_ENDIAN)
# error Unknown endianness
#endif

#ifdef __cplusplus

using namespace std;

extern "C" {
#endif

#ifdef B_ENDIAN_C_INLINE
static inline uint16_t B_SWAP16(uint16_t s) { return (s>>8)|(s<<8); }
static inline uint32_t B_SWAP32(uint32_t l) { return ((l>>8)&0xff00)|((l&0xff00)<<8)|(l<<24)|(l>>24); }
static inline uint64_t B_SWAP64(uint64_t l) { return (l>>56)|((l>>40)&0xff00)|((l>>24)&0xff0000)|((l>>8)&0xff000000)|((l&255)<<56)|((l&0xff00)<<40)|((l&0xff0000)<<24)|((l&0xff000000)<<8); }
#endif

static inline float B_SWAPFLOAT(float f) {
    union {
        float f;
        uint32_t i;
    } x;
    x.f = f;
    x.i = B_SWAP32(x.i);
    return x.f;
}

#if B_LITTLE_ENDIAN == 1
# define B_LITTLE64(x) (x)
# define B_BIG64(x)    B_SWAP64(x)
# define B_LITTLE32(x) (x)
# define B_BIG32(x)    B_SWAP32(x)
# define B_LITTLE16(x) (x)
# define B_BIG16(x)    B_SWAP16(x)
# define B_LITTLEFLOAT(x) (x)
# define B_BIGFLOAT(x) B_SWAPFLOAT(x)
#elif B_BIG_ENDIAN == 1
# define B_LITTLE64(x) B_SWAP64(x)
# define B_BIG64(x)    (x)
# define B_LITTLE32(x) B_SWAP32(x)
# define B_BIG32(x)    (x)
# define B_LITTLE16(x) B_SWAP16(x)
# define B_BIG16(x)    (x)
# define B_LITTLEFLOAT(x) B_SWAPFLOAT(x)
# define B_BIGFLOAT(x) (x)
#endif

#ifdef __GNUC__
# define PRINTF_FORMAT(stringindex, firstargindex) __attribute__((format (printf, stringindex, firstargindex)))
#else
# define PRINTF_FORMAT(stringindex, firstargindex)
#endif

#ifndef min
# define min(a,b) ( ((a) < (b)) ? (a) : (b) )
#endif

#ifndef max
# define max(a,b) ( ((a) > (b)) ? (a) : (b) )
#endif

#define Barraylen(s) (sizeof s / sizeof s[0])

// On Windows, _MAX_PATH is 260, null included.
// POSIX says 256 is the most it will write into a user buffer of unspecified size, null included.
// X/Open says 1024 for the same purposes as POSIX.
#define BMAX_PATH 1024

// Definitely not BSD/POSIX dirent.h.
struct Bdirent {
    size_t namlen;
    char *name;
    unsigned int mode;
    off_t size;
    time_t mtime;
};
typedef void BDIR;

BDIR* Bopendir(const char *name);
struct Bdirent* Breaddir(BDIR *dir);
int Bclosedir(BDIR *dir);

#if defined(__WATCOMC__)
# define strcasecmp stricmp
# define strncasecmp strnicmp
#endif

#ifdef __MINGW32__
# define mkdir(s,x) _mkdir(s)
#endif

#ifdef _MSC_VER
// When using the /Zl switch (or /Za to disable MS extensions),
// the POSIXy functions in the CRT need to be referenced with
// leading underscores.

# define access    _access
# define alloca    _alloca
# define close     _close
# define chdir     _chdir
# define fdopen    _fdopen
# define fstat     _fstat
# define getcwd    _getcwd
# define lseek     _lseek
# define mkdir(s,x) _mkdir(s)
# define open      _open
# define read      _read
# define snprintf  _snprintf
# define stat      _stat
# define strcasecmp _stricmp
# define strdup    _strdup
# define strlwr    _strlwr
# define strupr    _strupr
# define strncasecmp _strnicmp
# define vsnprintf _vsnprintf
# define write     _write

# define O_BINARY  _O_BINARY
# define O_TEXT    _O_TEXT
# define O_CREAT   _O_CREAT
# define O_EXCL    _O_EXCL
# define O_RDONLY  _O_RDONLY
# define O_RDWR    _O_RDWR
# define O_TRUNC   _O_TRUNC
# define O_WRONLY  _O_WRONLY
# define S_IFDIR   _S_IFDIR
# define S_IFREG   _S_IFREG
# define S_IREAD   _S_IREAD
# define S_IWRITE  _S_IWRITE
# define S_IRWXU   (_S_IREAD|_S_IWRITE)
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif
#ifndef O_TEXT
# define O_TEXT 0
#endif
#ifndef F_OK
# define F_OK 0
#endif
#ifndef S_IREAD
# define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
# define S_IWRITE S_IWUSR
#endif
#ifndef S_IEXEC
# define S_IEXEC 0
#endif

#if !defined(_WIN32)
char *strlwr(char *);
char *strupr(char *);
#endif

int Bvasprintf(char **ret, const char *format, va_list ap);
char *Bgethomedir(void);
char *Bgetappdir(void);
char *Bgetsupportdir(int global);
size_t Bgetsysmemsize(void);
int Bcorrectfilename(char *filename, int removefn);
int Bcanonicalisefilename(char *filename, int removefn);
char *Bgetsystemdrives(void);
off_t Bfilelength(int fd);
char *Bstrtoken(char *s, char *delim, char **ptrptr, int chop);
int Bwildmatch (const char *i, const char *j);

// One day, when all these are flushed from existence, this can be removed.
#if defined(USE_COMPAT_H_BMACROS) && USE_COMPAT_H_BMACROS
# define bsize_t size_t
# define BFILE FILE
# define BO_BINARY O_BINARY
# define BO_TEXT   O_TEXT
# define BO_RDONLY O_RDONLY
# define BO_WRONLY O_WRONLY
# define BO_RDWR   O_RDWR
# define BO_APPEND O_APPEND
# define BO_CREAT  O_CREAT
# define BO_TRUNC  O_TRUNC
# define BS_IRGRP  S_IRGRP
# define BS_IWGRP  S_IWGRP
# define BS_IEXEC  S_IEXEC
# define BS_IWRITE S_IWRITE
# define BS_IREAD  S_IREAD
# define BS_IFIFO  S_IFIFO
# define BS_IFCHR  S_IFCHR
# define BS_IFBLK  S_IFBLK
# define BS_IFDIR  S_IFDIR
# define BS_IFREG  S_IFREG
# define BSEEK_SET SEEK_SET
# define BSEEK_CUR SEEK_CUR
# define BSEEK_END SEEK_END
# define Brand rand
# define Balloca alloca
# define Bmalloc malloc
# define Bcalloc calloc
# define Brealloc realloc
# define Bfree free
# define Bopen open
# define Bclose close
# define Bwrite write
# define Bread read
# define Blseek lseek
# define Bstat stat
# define Bfopen fopen
# define Bfclose fclose
# define Bfeof feof
# define Bfgetc fgetc
# define Brewind rewind
# define Bfgets fgets
# define Bfputc fputc
# define Bfputs fputs
# define Bfread fread
# define Bfwrite fwrite
# define Bfprintf fprintf
# define Bstrdup strdup
# define Bstrcpy strcpy
# define Bstrncpy strncpy
# define Bstrcmp strcmp
# define Bstrncmp strncmp
# define Bstrcasecmp strcasecmp
# define Bstrncasecmp strncasecmp
# define Bstrlwr strlwr
# define Bstrupr strupr
# define Bmkdir mkdir
# define Bstrcat strcat
# define Bstrncat strncat
# define Bstrlen strlen
# define Bstrchr strchr
# define Bstrrchr strrchr
# define Batoi atoi
# define Batol atol
# define Bstrtol strtol
# define Bstrtoul strtoul
# define Bstrtod strtod
# define Btoupper toupper
# define Btolower tolower
# define Bmemcpy memcpy
# define Bmemmove memmove
# define Bmemchr memchr
# define Bmemset memset
# define Bmemcmp memcmp
# define Bprintf printf
# define Bsprintf sprintf
# define Bsnprintf snprintf
# define Bvsnprintf vsnprintf
# define Bvfprintf vfprintf
# define Bgetcwd getcwd
# define Bgetenv getenv
# define Btime time
#endif

#ifdef __cplusplus
}
#endif

#endif // __compat_h__

