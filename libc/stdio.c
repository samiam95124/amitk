/** **************************************************************************

\file

\brief Module stdio - standard I/O

Copyright 2007 (C) S. A. Moore

Description

Implements whitebook I/O. The functions are designed to funnel all I/O down to
the direct I/O procedures fread and fwrite. These are then implemented via calls
to the Unix format functions:

read(fd, buf, len);
write(fd, buf, len);
open(name, flags);
close(fd);
unlink(name);
lseek(fd, off, mode);

The integer length modifiers hh, h, l, ll and z are honored by both the print
and scan functions. The L (long double) modifier is accepted but treated as
double, since extended precision is not implemented. Floating point conversions
(f, e, E, g, G) are supported in both the print and scan functions.

Two stdios in one program

This module and the system's stdio can both be present in a program, and a
FILE belongs to one or the other: their layouts differ, and a FILE of one
handed to the other is misread. The build keeps the two apart in one of two
ways, chosen in the Makefile (STDIO_SOURCE):

Override mode: this module defines the standard names, and the static link
resolves the program and the petit ami archives to it, with the symbols
hidden from the dynamic table so that shared libraries bind the system's.
The two worlds split at the dynamic boundary. This is the Linux static
configuration.

Coined mode (STDIO_BYPASS): this module's functions are named stdio_<name>,
and stdio.h rewrites the calls of every petit ami source to them. The
standard names then belong to the system throughout the image, and every
archive linked in, prebuilt ones included, runs on the system's FILEs. This
is the configuration wherever archives link statically beside this module:
Windows, FreeBSD and the dynamic Linux configuration. On Windows the
override mode had OpenSSL's fread bound here while its FILE came from the C
runtime, and a valid certificate file failed to parse (issue #133).

In either mode one rule holds for the program: a FILE of this module is
never given to a library that will read or write it through the system's
stdio, png_init_io being the classic case; such a library is given a
callback or a descriptor instead, as windows/screen_capture.c does.

Actual use has shown that there are programs that call the stdio functions
before any constructor for this module can be run. This results in a serious
error. Thus we have moved the initialization to compile time definitions, or at
worst, runtime initialization that is "triggered" by NULL values in the data.
This means stdio is either "self initialized" or "inherently initialized" if you
prefer. This solves the error, since it does not matter when the stdio calls
occur.

Exports

The full set of stdio calls. They are listed here:

FILE *fopen(const char *filename, const char *mode) - Open a file by name.
FILE *freopen(const char* filename, const char* mode, FILE* stream)
    - Reopen a file by name.
FILE *fdopen(int fd, const char *mode) - Open file descriptor as file.
int fflush(FILE *stream) - Flush pending I/O from file.
int fclose(FILE *stream) - Close open file.
int remove(const char *filename) - Remove file by name.
int rename(const char *oldname, const char *newname) - Rename file.
FILE *tmpfile(void) - Create a temporary file.
char *tmpnam(char s[]) - Create a temporary name.
int setvbuf(FILE* stream, char *buf, int mode, size_t size) - Set file
    buffering characteristics.
void setbuf(FILE* stream, char *buf) - Set buffer for file.
int fprintf(FILE* stream, const char *format, ...) - Print to file.
int printf(const char* format, ...) - Print to stdout.
int sprintf(char* s, const char *format, ...) - Print to string.
int snprintf(char* s, size_t n, const char *format, ...) - Bounded print to
    string.
int vprintf(const char* format, va_list arg) - Print to stdout with variable
    list.
int vfprintf(FILE* stream, const char *format, va_list arg) - Print to file
    with variable list.
int vsprintf(char* s, const char *format, va_list arg) - Print to string with
    variable list.
int vsnprintf(char* s, size_t n, const char *format, va_list arg) - Bounded
    print to string with variable list.
int fscanf(FILE* stream, const char *format, ...) - Scan from file.
int scanf(const char* format, ...) - Scan from stdin.
int sscanf(const char* s, const char *format, ...) - Scan from string.
int vscanf(const char* format, va_list arg) - Scan from stdin with variable
    list.
int vfscanf(FILE* stream, const char *format, va_list arg) - Scan from file
    with variable list.
int vsscanf(const char* s, const char *format, va_list arg) - Scan from string
    with variable list.
int fgetc(FILE *stream) - Get character from file.
int getc(FILE *stream) - Get character from file.
char *fgets(char *s, int n, FILE *stream) - Get string from file.
int fputc(int c, FILE *stream) - Put character to file.
int fputs(const char *s, FILE *stream) - Put string to file.
int putc(int c, FILE *stream) - Put character to file.
int getchar(void) - Get character from stdin.
char *gets(char *s) - Get string from stdin.
int putchar(int c) - Put character to stdout.
int puts(const char *s) - Put string to stdout.
int ungetc(int c, FILE *stream) - Put back single character to file.
size_t fread(void *ptr, size_t size, size_t nobj, FILE *stream) - Read blocks
    from file.
size_t fwrite(const void *ptr, size_t size, size_t nobj, FILE *stream) - Write
    blocks to file.
int fseek(FILE* stream, long offset, int origin) - Seek file location.
long ftell(FILE* stream) - Find file location.
void rewind(FILE* stream) - Go to file beginning.
int fgetpos(FILE* stream, fpos_t *ptr) - Find file location.
int fsetpos(FILE* stream, const fpos_t *ptr) - Set file location.
void clearerr(FILE* stream) - Clear pending errors on file.
int feof(FILE* stream) - Check end of file.
int ferror(FILE* stream) - Check error on file.
void perror(const char *s) - Print error message.
int fileno(FILE* stream) - Find descriptor for file.

A test suite for the module is provided in tests/stdio_test.c.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#include <localdefs.h>

/* We use a custom stdio.h */

#define FALSE 0
#define TRUE 1

/* file modes */

#define STDIO_MREAD  0 /* read only */
#define STDIO_MWRITE 1 /* write only */
#define STDIO_MRDWR  2 /* read and write */

/* length of standard buffer block */

#define STDIO_STDBUF 256 /* length of standard buffer block */

/* include floating point input and output */

#define STDIO_FLOAT 1

/* Size of printf/fprintf output buffer to use. This must typically be at least
   as many characters as a user may pass to prevent buffer overruns, but should
   not be so long as to cause issues with local thread stacks. */

#define STDIO_OUTBUF 1000

/* types of system vectors for override calls */

typedef ssize_t (*vt_read_t)(int, void*, size_t);
typedef ssize_t (*vt_write_t)(int, const void*, size_t);
typedef int     (*vt_open_t)(const char*, int, int);
typedef int     (*vt_close_t)(int);
typedef int     (*vt_unlink_t)(const char*);
typedef off_t   (*vt_lseek_t)(int, off_t, int);

/* standard files definitions */

static FILE stdinfe = {

    ._flags  = _IO_NO_WRITES | _IO_UNBUFFERED | _IO_IS_FILEBUF, /* read, unbuffered */
    ._fileno = 0,    /* logical file 0 */
    .text    = TRUE, /* text mode */
    .name    = NULL  /* no name attached */

};

static FILE stdoutfe = {

    ._flags  = _IO_NO_READS | _IO_LINE_BUF | _IO_IS_FILEBUF, /* write, line buffered */
    ._fileno = 1,    /* logical file 1 */
    .text    = TRUE, /* text mode */
    .name    = NULL  /* no name attached */

};

static FILE stderrfe = {

    ._flags  = _IO_NO_READS | _IO_UNBUFFERED | _IO_IS_FILEBUF, /* write, unbuffered */
    ._fileno = 2,    /* logical file 2 */
    .text    = TRUE, /* text mode */
    .name    = NULL  /* no name attached */

};

/* standard in, out and error files */

FILE *stdin = &stdinfe;
FILE *stdout = &stdoutfe;
FILE *stderr = &stderrfe;

/* open files table. The first 0, 1, and 2 entries are tied to stdin, stdout
   and stderr. This does not have to be, but it makes the system more
   organized. */

/* The file table lock. The table of open files is a block of data threads
   share: two threads opening at once must not be handed the same entry.
   The allocation of an entry is under the lock, and the entry is marked
   claimed before the lock drops, since the descriptor goes in only after.
   Windows builds the module without pthreads and keeps the table as it
   was. */
#ifndef _WIN32
static pthread_mutex_t filtablock = PTHREAD_MUTEX_INITIALIZER;
#define FILTABLOCK()   pthread_mutex_lock(&filtablock)
#define FILTABUNLOCK() pthread_mutex_unlock(&filtablock)
#else
#define FILTABLOCK()
#define FILTABUNLOCK()
#endif

static FILE *opnfil[FOPEN_MAX] = {

    &stdinfe,   /* standard input */
    &stdoutfe, /* standard output */
    &stderrfe  /* standard error */

    /* remaining entries are NULL */

};

/* top powers table, we precalculate this to save runtime. These are the widest
   integer type so that all of the length modifiers (l, ll, z, ...) can be
   handled by a single conversion engine. */

static unsigned long long power8; /* octal */
static unsigned long long power10; /* decimal */
static unsigned long long power16; /* hexadecimal */

/* length modifier codes shared by the formatted input and output engines */

#define LM_NONE 0 /* no modifier: int */
#define LM_HH   1 /* hh: char */
#define LM_H    2 /* h: short */
#define LM_L    3 /* l: long */
#define LM_LL   4 /* ll: long long */
#define LM_CAP  5 /* L: long double (treated as double, no extended support) */
#define LM_Z    6 /* z: size_t */

/* declare vector functions in advance */

static ssize_t wread(int fd, void* buff, size_t count);
static ssize_t wwrite(int fd, const void* buff, size_t count);
static int wopen(const char* pathname, int flags, int perm);
static int wclose(int fd);
static int wunlink(const char* pathname);
static off_t wlseek(int fd, off_t offset, int whence);

/*
 * Vectors to system calls. These vectors point to the system equivalent calls,
 * but can be hooked or overridden by higher level layers. These are initialized
 * to the default handlers here in stdio.c.
 */
static vt_read_t   vt_read   = wread;
static vt_write_t  vt_write  = wwrite;
static vt_open_t   vt_open   = wopen;
static vt_close_t  vt_close  = wclose;
static vt_unlink_t vt_unlink = wunlink;
static vt_lseek_t  vt_lseek  = wlseek;

static vt_read_t   vt_read_nocancel   = wread;
static vt_write_t  vt_write_nocancel  = wwrite;
static vt_open_t   vt_open_nocancel   = wopen;
static vt_close_t  vt_close_nocancel  = wclose;

/* counters to generate temp files */

static int tmpcnt; /* temp name counter, advanced for each name generated */

/*******************************************************************************

System call vector linkers

Each call links the system call via a vector. We define a series of wrappers
to serve as the function pointer targets, because the host system may or may
not implement the function directly as a function. It could be a macro.

*******************************************************************************/

static ssize_t wread(
    /** file descriptor */ int fd,
    /** data buffer */     void* buff,
    /** byte count */      size_t count
)
    { return read(fd, buff, count); }
static ssize_t wwrite(
    /** file descriptor */ int fd,
    /** data buffer */     const void* buff,
    /** byte count */      size_t count
)
    { return write(fd, buff, count); }
static int wopen(
    /** file path */       const char* pathname,
    /** open flags */      int flags,
    /** permission bits */ int perm
)
#ifdef __MINGW32__
    /* The Ami stdio is unix modeled: a file is a byte stream with LF line
       ends on every platform, so the same text (intermediate decks, error
       files) interchanges between the windows and unix built tools. Open
       binary so the windows C runtime does not impose CRLF translation. */
    { return open(pathname, flags | O_BINARY, perm); }
#else
    { return open(pathname, flags, perm); }
#endif
static int wclose(
    /** file descriptor */ int fd
)
    { return close(fd); }
static int wunlink(
    /** file path */ const char* pathname
)
    { return unlink(pathname); }
static off_t wlseek(
    /** file descriptor */ int fd,
    /** seek offset */     off_t offset,
    /** seek origin */     int whence
)
    { return lseek(fd, offset, whence); }

static ssize_t vread(
    /** file descriptor */ int fd,
    /** data buffer */     void* buff,
    /** byte count */      size_t count
)
    { return (*vt_read)(fd, buff, count); }
static ssize_t vwrite(
    /** file descriptor */ int fd,
    /** data buffer */     const void* buff,
    /** byte count */      size_t count
)
    { return (*vt_write)(fd, buff, count); }
static int vopen(
    /** file path */       const char* pathname,
    /** open flags */      int flags,
    /** permission bits */ int perm
)
    { return (*vt_open)(pathname, flags, perm); }
static int vclose(
    /** file descriptor */ int fd
)
    { return (*vt_close)(fd); }
static int vunlink(
    /** file path */ const char* pathname
)
    { return (*vt_unlink)(pathname); }
static off_t vlseek(
    /** file descriptor */ int fd,
    /** seek offset */     off_t offset,
    /** seek origin */     int whence
)
    { return (*vt_lseek)(fd, offset, whence); }

/*******************************************************************************

System call overriders

Each overrider call takes both a new vector and a place to store the old vector.
The overrider, if it receives a call it does not want to handle, "passes down"
the call by calling the stored vector. This chain continues until it reaches the
original handler established, which goes back to the raw system call.

*******************************************************************************/

void ovr_read(
    /** new vector function */                  vt_read_t nfp,
    /** returns the previous vector function */ vt_read_t* ofp
) { *ofp = vt_read; vt_read = nfp; }
void ovr_write(
    /** new vector function */                  vt_write_t nfp,
    /** returns the previous vector function */ vt_write_t* ofp
) { *ofp = vt_write; vt_write = nfp; }
void ovr_open(
    /** new vector function */                  vt_open_t nfp,
    /** returns the previous vector function */ vt_open_t* ofp
) { *ofp = vt_open; vt_open = nfp; }
void ovr_close(
    /** new vector function */                  vt_close_t nfp,
    /** returns the previous vector function */ vt_close_t* ofp
) { *ofp = vt_close; vt_close = nfp; }
void ovr_unlink(
    /** new vector function */                  vt_unlink_t nfp,
    /** returns the previous vector function */ vt_unlink_t* ofp
)
    { *ofp = vt_unlink; vt_unlink = nfp; }
void ovr_lseek(
    /** new vector function */                  vt_lseek_t nfp,
    /** returns the previous vector function */ vt_lseek_t* ofp
) { *ofp = vt_lseek; vt_lseek = nfp; }

/* nocancel overrides use separate vectors to avoid stomping the regular ones */
void ovr_read_nocancel(
    /** new vector function */                  vt_read_t nfp,
    /** returns the previous vector function */ vt_read_t* ofp
)
    { *ofp = vt_read_nocancel; vt_read_nocancel = nfp; }
void ovr_write_nocancel(
    /** new vector function */                  vt_write_t nfp,
    /** returns the previous vector function */ vt_write_t* ofp
)
    { *ofp = vt_write_nocancel; vt_write_nocancel = nfp; }
void ovr_open_nocancel(
    /** new vector function */                  vt_open_t nfp,
    /** returns the previous vector function */ vt_open_t* ofp
)
    { *ofp = vt_open_nocancel; vt_open_nocancel = nfp; }
void ovr_close_nocancel(
    /** new vector function */                  vt_close_t nfp,
    /** returns the previous vector function */ vt_close_t* ofp
)
    { *ofp = vt_close_nocancel; vt_close_nocancel = nfp; }

/*******************************************************************************

Buffering engine

These routines implement read and write buffering for streams. The stream state
is held in the GNU libc style pointer fields, so that code reaching directly
into
the structure sees what it expects:

    reading: _IO_read_base <= _IO_read_ptr <= _IO_read_end, with the unconsumed
             read ahead being [_IO_read_ptr, _IO_read_end). The put pointers are
             held equal (no pending writes).
    writing: _IO_CURRENTLY_PUTTING is set, _IO_write_base <= _IO_write_ptr <=
             _IO_write_end, with pending output being [_IO_write_base,
             _IO_write_ptr). The get area is held empty.

The single buffer [_IO_buf_base, _IO_buf_end) is used for reading OR writing,
never both at once. Switching direction flushes or discards as required, and the
seek/tell routines compensate for read ahead and pending data so the logical
position is always correct.

An unbuffered stream (_IO_UNBUFFERED, the default for stdin/stdout/stderr) never
allocates a buffer and falls through to direct single call I/O.

*******************************************************************************/

/* size of the allocated buffer in bytes */
#define iobufsize(s) ((int)((s)->_IO_buf_end-(s)->_IO_buf_base))

/* forward reference to the public flush */
int fflush(FILE *stream);

/* flush all streams at program exit, so buffered output is not lost */
static void stdio_flushall(void) { fflush((FILE *)NULL); }
static int  atxset = FALSE; /* atexit handler has been registered */

/** **************************************************************************

Function bufalloc

Description

Allocate the stream buffer on demand. If the stream is buffered but has no
buffer yet, one is allocated. If allocation fails, the stream silently falls
back to unbuffered operation.

\returns

Zero on success, EOF on allocation failure.

******************************************************************************/

static int bufalloc(
    /** file to operate on */ FILE *s
)

{

    char *b; /* new buffer */

    if (s->_IO_buf_base) return (0); /* already have a buffer */
    if (s->_flags & _IO_UNBUFFERED) return (0); /* unbuffered, none needed */
    b = malloc(BUFSIZ); /* get the buffer */
    if (!b) { s->_flags |= _IO_UNBUFFERED; return (EOF); } /* fall back to none */
    s->_IO_buf_base = b; /* set buffer base */
    s->_IO_buf_end = b+BUFSIZ; /* set buffer end */
    s->_flags &= ~_IO_USER_BUF; /* we own this buffer */
    /* start with empty get and put areas at the buffer base */
    s->_IO_read_base = s->_IO_read_ptr = s->_IO_read_end = b;
    s->_IO_write_base = s->_IO_write_ptr = s->_IO_write_end = b;

    return (0);

}

/** **************************************************************************

Function wflush

Description

Flush pending write data. Writes out any bytes held in the put area. The stream
stays in writing mode with an empty put area.

\returns

Zero on success, EOF on a write error.

******************************************************************************/

static int wflush(
    /** file to operate on */ FILE *s
)

{

    int n;   /* write count */
    int len; /* bytes to write */

    if ((s->_flags & _IO_CURRENTLY_PUTTING) &&
        s->_IO_write_ptr > s->_IO_write_base) { /* there is data to write */

        len = s->_IO_write_ptr-s->_IO_write_base; /* pending byte count */
        n = vwrite(s->_fileno, s->_IO_write_base, len); /* write it out */
        if (n != len) { /* write failed or was short */

            s->_flags |= _IO_ERR_SEEN; /* flag the error */
            s->_IO_write_ptr = s->_IO_write_base; /* drop the data */
            return (EOF);

        }
        s->_IO_write_ptr = s->_IO_write_base; /* put area is now empty */

    }

    return (0);

}

/** **************************************************************************

Function rflush

Description

Discard read ahead. Drops any unconsumed read ahead data, rewinding the
underlying file position back to the logical position so a following write or
seek is correct. Leaves the get area empty.

\returns

none

******************************************************************************/

static void rflush(
    /** file to operate on */ FILE *s
)

{

    int ahead; /* unconsumed read ahead bytes */

    if (!(s->_flags & _IO_CURRENTLY_PUTTING)) {

        ahead = s->_IO_read_end-s->_IO_read_ptr; /* read ahead not consumed */
        if (ahead > 0) vlseek(s->_fileno, -(off_t)ahead, SEEK_CUR); /* rewind */

    }
    /* reset the get area to empty at the buffer base */
    s->_IO_read_base = s->_IO_read_ptr = s->_IO_read_end = s->_IO_buf_base;

}

/** **************************************************************************

Function getbuf

Description

Get a byte through the read buffer. Serves any pushed back data and read ahead
from the get area, refilling from the file as needed. If the stream is
unbuffered, reads a single byte directly.

\returns

The next byte from the stream, or EOF at end of file or on error.

******************************************************************************/

static int getbuf(
    /** file to operate on */ FILE *s
)

{

    int n; /* read count */

    /* before blocking for standard input, flush standard output so any
       line buffered prompt is visible to the user first */
    if (s == stdin) fflush(stdout);

    /* serve from the get area, which also serves any pushed back characters */
    if (s->_IO_read_ptr < s->_IO_read_end)
        return ((unsigned char)*s->_IO_read_ptr++);

    /* if we were writing, flush and switch to reading */
    if (s->_flags & _IO_CURRENTLY_PUTTING) {

        if (wflush(s)) return (EOF);
        s->_flags &= ~_IO_CURRENTLY_PUTTING; /* now reading */

    }

    /* set up the buffer on first use if buffering is enabled */
    if (!s->_IO_buf_base && !(s->_flags & _IO_UNBUFFERED)) bufalloc(s);

    if (!s->_IO_buf_base) { /* unbuffered: read a single byte directly */

        unsigned char b; /* byte holder */

        n = vread(s->_fileno, &b, 1); /* read one byte */
        if (n <= 0) { s->_flags |= (n == 0) ? _IO_EOF_SEEN : _IO_ERR_SEEN;
                      return (EOF); }
        return (b);

    }

    /* refill the buffer from the file */
    n = vread(s->_fileno, s->_IO_buf_base, iobufsize(s));
    if (n <= 0) { /* end of file or error */

        s->_flags |= (n == 0) ? _IO_EOF_SEEN : _IO_ERR_SEEN;
        s->_IO_read_base = s->_IO_read_ptr = s->_IO_read_end = s->_IO_buf_base;
        return (EOF);

    }
    s->_IO_read_base = s->_IO_read_ptr = s->_IO_buf_base; /* get area start */
    s->_IO_read_end = s->_IO_buf_base+n; /* end of valid data */
    /* reading: hold the put pointers empty at the base */
    s->_IO_write_base = s->_IO_write_ptr = s->_IO_buf_base;

    return ((unsigned char)*s->_IO_read_ptr++); /* return next byte */

}

/** **************************************************************************

Function putbuf

Description

Put a byte through the write buffer. Buffers a byte for output, flushing the
buffer when it fills, or at a newline for line buffered streams. If the stream
is unbuffered, writes a single byte directly.

\returns

The byte written, or EOF on error.

******************************************************************************/

static int putbuf(
    /** file to operate on */ FILE *s,
    /** byte to write */      int c
)

{

    /* set up the buffer on first use if buffering is enabled */
    if (!s->_IO_buf_base && !(s->_flags & _IO_UNBUFFERED)) bufalloc(s);

    if (!s->_IO_buf_base) { /* unbuffered: write a single byte directly */

        unsigned char b = c; /* byte holder */

        if (vwrite(s->_fileno, &b, 1) != 1) { s->_flags |= _IO_ERR_SEEN;
                                              return (EOF); }
        return (c & 0xff);

    }

    /* if we were reading, discard read ahead and switch to writing */
    if (!(s->_flags & _IO_CURRENTLY_PUTTING)) {

        rflush(s); /* discard read ahead, rewind the file */
        s->_flags |= _IO_CURRENTLY_PUTTING; /* now writing */
        s->_IO_write_base = s->_IO_write_ptr = s->_IO_buf_base; /* empty put area */
        s->_IO_write_end = s->_IO_buf_end; /* room to the end of the buffer */

    }
    *s->_IO_write_ptr++ = c; /* place the byte */
    /* flush when the put area fills, or at a newline if line buffered */
    if (s->_IO_write_ptr >= s->_IO_write_end ||
        ((s->_flags & _IO_LINE_BUF) && (char)c == '\n'))
        if (wflush(s)) return (EOF);

    return (c & 0xff);

}

/** **************************************************************************

Function maknod

Description

Create a file access node.

Gets a free file node, which is a descriptor for an open file. Descriptors are
small integers from 1 to FOPEN_MAX, and are indexes into the open files table.
This is a table that either contains the structure pointer to describe the file,
or a NULL, which marks a file entry that was never opened.

We either find a NULL entry in the array, or one that is flagged closed. If none
is found, we create a new entry in the table. This effectively means that we
recycle old entries no longer used, and hold entries that were once closed
indefiniately.

If a zero is returned, it means the file table is full, and no further files
can be allocated.

\returns

The file table index, or zero if the table is full.

******************************************************************************/

static int maknod(void)

{

    int i, f; /* file table indexs */

    /* search first free file entry */
    FILTABLOCK(); /* the table, for the allocation */
    f = 0; /* set no entry found */
    for (i = 0; i < FOPEN_MAX; i++) /* traverse table */
       if (!opnfil[i]) f = i; /* found NULL entry */
       else if (opnfil[i]->_fileno == -1) f = i; /* found closed entry */
    if (!f) { FILTABUNLOCK(); return (f); } /* file table is full, return error */
    if (!opnfil[f]) { /* not recyling a previous entry, allocate */

       opnfil[f] = malloc(sizeof(FILE)); /* get a new file tracking struct */
       if (!opnfil[f]) { FILTABUNLOCK(); return (0); } /* couldn't allocate, exit w/ error */

    }
    /* register the exit flush once, so buffered output is not lost on exit */
    if (!atxset) { atexit(stdio_flushall); atxset = TRUE; }

    /* initialize all fields to empty, then set the defaults. Files default to
       full buffering (neither the unbuffered nor line buffered flag), with the
       buffer allocated on first use. */
    memset(opnfil[f], 0, sizeof(FILE));
    opnfil[f]->_fileno = -2; /* claimed, no logical file attached yet: not
                                free to another thread, as -1 is */
    opnfil[f]->_flags = _IO_IS_FILEBUF; /* file backed, fully buffered */
    FILTABUNLOCK();

    return f; /* return file id */

}

/** **************************************************************************

Function putchrs

Description

Places a series of characters in a string.

Places a given number of characters in a string. The string pointer is advanced
past the placed characters.

\returns

none

******************************************************************************/

static void putchrs(
    /** output string position, or NULL to write to the file */ char *s[],
    /** number of characters to emit */                         int cnt,
    /** character to emit */                                    char c,
    /** running output character count */                       int *ocnt,
    /** destination file */                                     FILE *fd
)

{

    int i; /* counter */

    for (i = 0; i < cnt; i++) { /* for count */

        if (*s) { /* output to string */

            **s = c; /* place characters */
            (*s)++; /* next location */

        } else fputc(c, fd); /* output to file */

    }
    if (cnt > 0) *ocnt += cnt; /* add to count */

}

/** **************************************************************************

Function digits

Description

Find digit count.

Finds the count of digits in an unsigned number. Accepts a radix. The radix
needs to be one of 2, 8, 10 or 16, otherwise the result is nonsense.

\returns

The number of digits the value occupies in the given radix.

******************************************************************************/

static int digits(
    /** radix (number base) */ int r,
    /** value to measure */    unsigned long long l
)

{

    unsigned long long p; /* power holder */
    int cnt;              /* digit count */

    p = 1*r; /* set power */
    cnt = 1; /* set digit count */
    while (p && l >= p) { /* find digits */

        p *= r; /* find next power */
        cnt++; /* count digits */

    }

    return (cnt); /* exit with digit count */

}

/** **************************************************************************

Function toppow

Description

Find top power in unsigned long long.

Finds the top power of the given radix that will fit into an unsigned long long.

\returns

The largest power of the radix that fits an unsigned long long.

******************************************************************************/

static unsigned long long toppow(
    /** radix (number base) */ int r
)

{

    unsigned long long p; /* power */
    unsigned long long ps; /* power save */

    /* find top power */
    p = 1;
    do { /* iterate */

        ps = p; /* save last good power */
        p = p*r; /* find next power */

    } while (p/r == ps);

    return ps; /* return last good power */

}

/** **************************************************************************

Function putnum

Description

Put unsigned number.

Places an unsigned number into a string. Accepts a radix. No padding or
formatting is done. The radix must be 2, 8, 10 or 16, or garbage will result.
Also accepts a top power to use in the convertion.

\returns

none

******************************************************************************/

static void putnum(
    /** output string position, or NULL to write to the file */ char **s,
    /** value to convert */ unsigned long long l,
    /** radix (number base) */ int r,
    /** top power of the radix to start from */ unsigned long long p,
    /** upper case digits flag */ int ucase,
    /** running output character count */ int *ocnt,
    /** destination file */ FILE *fd
)

{

    int lzer; /* leading zeros in effect */
    char d;   /* digit */

    lzer = TRUE; /* set in leading zeros */
    while (p) { /* extract digit */

       d = l/p%r; /* get digit */
       if (d < 10) d += '0'; /* offset 0-9 */
       else d = d-10+(ucase?'A':'a'); /* offset A-F */
       /* check on last digit, or not zero digit, or leading digit output */
       if (p == 1 || d != '0' || !lzer) {

           if (*s) *(*s)++ = d; /* place digit */
           else fputc(d, fd); /* output to file */
           (*ocnt)++; /* count */
           lzer = FALSE; /* set leading digit output */

       }
       p = p/r; /* next power */

    }

}

/** **************************************************************************

Function getnum

Description

Get unsigned number.

Gets an unsigned number from a string. The string pointer is advanced over the
number, and the number returned in a variable. If there is no number present,
zero is returned.

\returns

none

Notes

No overflow is checked.

******************************************************************************/

static void getnum(
    /** pointer to the scan position */ const char **s,
    /** returns the parsed number */    int *i
)

{

    *i = 0; /* clear result */
    while (**s >= '0' && **s <= '9') *i = *i*10+*(*s)++-'0';

}

/** **************************************************************************

Function chkrad

Description

Check if the given character lies in the radix.

Given a character and a radix, checks if the character lies in the radix, that
is, either a digit or an alphabetical character within the radix. Returns
true if so.

\returns

Returns true if so.

******************************************************************************/

static int chkrad(
    /** character to test */   int c,
    /** radix (number base) */ int r
)

{

    /* convert character 0 based number. Only a letter stands for a digit
       above nine: anything else worked out to a value all the same, and
       for the six characters between 'Z' and 'a' that value lay under
       ten, so a decimal conversion took '[' as the digit 4 -- and a
       folder called [Gmail]/Sent Mail, read back with %lld first, lost
       its bracket and could not be found on the server again. */
    if (isdigit(c)) c -= '0';
    else if (isalpha(c)) c = tolower(c)-'a'+10;
    else return (0);

    /* check bounded by radix and return that status */
    return (c >= 0 && c < r);

}

/** **************************************************************************

Function getfstr

Description

Get character from string or file.

Given a string and a file stream, gets a character from either the string or
a file. If the string is NULL, then the file is read from. If either the string
end or the end of the file is encountered, or an error occurs in the file,
then EOF is returned. The file or the string is advanced past the read character
if not EOF.

\returns

The next character, or EOF at end of string or file.

******************************************************************************/

static int getfstr(
    /** pointer to the input string, or NULL to read the file */ const char **s,
    /** input file */                                            FILE *fd
)

{

    if (*s) { /* string */

        if (!**s) return (EOF); /* end of string */
        else return (*(*s)++); /* return next character */

    } else return (fgetc(fd)); /* get character from file */

}

/** **************************************************************************

Function chkfstr

Description

Check character from string or file.

Given a string and a file stream, gets a character from either the string or
a file. If the string is NULL, then the file is read from. If either the string
end or the end of the file is encountered, or an error occurs in the file,
then EOF is returned. The file or the string is not advanced, which means that
in case of a file, the characrter is put back.

\returns

The next character without consuming it, or EOF at end of input.

******************************************************************************/

static int chkfstr(
    /** input string, or NULL to read the file */ const char *s,
    /** input file */                             FILE *fd
)

{

    int c; /* character holder */

    if (s) { /* string */

        if (!*s) return (EOF); /* end of string */
        else return (*s); /* return next character */

    } else {

        c = fgetc(fd); /* get character from file */
        if (c != EOF) ungetc(c, fd); /* put that back */
        return (c); /* return with result */

    }

}

/** **************************************************************************

Function chkfstrlen

Description

Get character from metered string or file.

Gets the next character from a metered string. This is a string whose length
is set. If the length is 0, then we return EOF, so that the end of the string
becomes either the zero marking the end, or the length reaching zero.

\returns

The next character, or EOF when the metered length is exhausted.

******************************************************************************/

static int chkfstrlen(
    /** input string, or NULL to read the file */ const char *s,
    /** remaining metered length */               int len,
    /** input file */                             FILE *fd
)

{

    if (len) return (chkfstr(s, fd)); else return (EOF);

}

/** **************************************************************************

Function getnum

Description

Get unsigned number.

Gets an unsigned number from a string. The string pointer is advanced over the
number, and the number returned in a variable. If there is no number present,
zero is returned. The radix for the number is specified. If the number
overflows, the overflow flag will be returned true, else false. On overflow,
the number returned in garbage. All characters that fit the radix are skipped,
even if overflow becomes true.

\returns

none

******************************************************************************/

static void getnumro(
    /** pointer to the scan position */            const char **s,
    /** returns the parsed value */                unsigned long long *l,
    /** radix (number base) */                     int r,
    /** returns the overflow flag */               int *o,
    /** returns the character count consumed */    int *cnt,
    /** field width limit, counted down as read */ int *fld,
    /** input file when the string is NULL */      FILE *fd
)

{

    unsigned long long save; /* save for error check */

    *l = 0; /* clear result */
    *o = 0; /* set no overflow */
    /* gather characters while they fit the radix */
    while (chkrad(chkfstrlen(*s, *fld, fd), r)) {

        save = *l; /* save number for error check */
        if (chkfstrlen(*s, *fld, fd) >= '0' && chkfstrlen(*s, *fld, fd) <= '9')
            *l = *l*r+chkfstrlen(*s, *fld, fd)-'0';
        else
            *l = *l*r+tolower(chkfstrlen(*s, *fld, fd))-'a'+10;
        getfstr(s, fd); /* next string character */
        (*fld)--; /* decrease field */
        (*cnt)++; /* count characters */
        /* if the new calculated value overflows, it will wrap around 0. We
           detect this as being new value < old value. */
        if (*l < save) *o = 1; /* set overflow occurred */

    }

}

/** **************************************************************************

Function strtoulso

Description

String to unsigned long with sign and overflow.

Converts a string to an unsigned long with sign and overflow. The base of the
convertion is given, which is 0 if the base is to be detected by the format
of the number itself. If the base is specified, a radix specifier is allowed,
such as "0x", but only if it matches the radix it specifies. If the number
overflows, that status is returned. If a sign was given, that is returned as
a +1 or -1 according to the sign. However, this has no effect on the number.
A  maximum field is accepted that gives the length of the string to be parsed.
This or a zero termination will determine when the number stops. An error flag
is returned that indicates malformed numbers (but not overflows). This
indicates things like having no leading digits at all.

The number ends when a digit beyond the radix, or an unrelated character, or the
end of the string is encountered. This does not cause an error.

The string pointer is advanced to the terminating character or end of string.

Leading whitespace is skipped.

A count of characters passed is kept.

\returns

The parsed unsigned value; the sign, overflow and error status are returned
through pointers.

******************************************************************************/

static unsigned long long strtoulso(
    /** pointer to the scan position */         const char **s,
    /** number base */                          int base,
    /** returns the sign */                     int *sgn,
    /** returns the overflow flag */            int *o,
    /** returns the character count consumed */ int *cnt,
    /** returns the error flag */               int *err,
    /** field width limit (0 for none) */       int fld,
    /** input file when the string is NULL */   FILE *fd
)

{

    unsigned long long v; /* value */

    *sgn = TRUE; /* set positive */
    *o = FALSE; /* set no overflow */
    *err = FALSE; /* set no error */
    /* skip leading whitespace */
    while (isspace(chkfstrlen(*s, fld, fd))) { getfstr(s, fd); (*cnt)++; }
    /* Check there is no numeric leader in the string */
    if (chkfstrlen(*s, fld, fd) != '-' &&
        chkfstrlen(*s, fld, fd) != '+' &&
        !chkrad(chkfstrlen(*s, fld, fd), base?base:10)) {

        *err = TRUE; /* set error occurred */
        return (0); /* exit with no value */

    }
    if (chkfstrlen(*s, fld, fd) == '-' || chkfstrlen(*s, fld, fd) == '+') {

        if (chkfstrlen(*s, fld, fd) == '-') *sgn = -1; /* set negative sign */
        getfstr(s, fd); /* skip sign */
        fld--; /* decrease field */
        (*cnt)++; /* count characters */
        /* check digit leader */
        if (!chkrad(chkfstrlen(*s, fld, fd), base?base:10)) {

            *err = TRUE; /* set error occurred */
            return (0); /* exit with no value */

        }

    }
    if (!base) { /* try to determine radix */

        if (chkfstrlen(*s, fld, fd) == '0') base = 8; /* set base octal */
        else base = 10; /* set base decimal */
        getnumro(s, &v, base, o, cnt, &fld, fd); /* parse digits */
        if (!v && (chkfstrlen(*s, fld, fd) == 'x' ||
                   chkfstrlen(*s, fld, fd) == 'X')) { /* 0x/0X, it's hexadecimal */

            getfstr(s, fd); /* skip x/X */
            fld--; /* decrease field */
            (*cnt)++; /* count characters */
            base = 16; /* set hexadecimal */
            getnumro(s, &v, base, o, cnt, &fld, fd); /* parse digits */

        }

    } else {

        getnumro(s, &v, base, o, cnt, &fld, fd); /* parse digits in selected base */
        if (base == 16 && !v && (chkfstrlen(*s, fld, fd) == 'x' ||
                                 chkfstrlen(*s, fld, fd) == 'X')) {

            /* allow 0x/0X on hexadecimal */
            getfstr(s, fd); /* skip x/X */
            fld--; /* decrease field */
            (*cnt)++; /* count characters */
            getnumro(s, &v, base, o, cnt, &fld, fd); /* parse digits */

        }

    }
    /* set overflow error */
    if (*o) errno = ERANGE;

    return (v); /* return value */

}

/** **************************************************************************

Function strtoli

Description

Convert string to signed long number.

The given string is converted to a signed long integer, see the strtoulso
function. Takes a base radix, an input count, and a field.

If the number overlows, it will be replaced with either a MAX or MIN, according
to if the sign is set or not, and no overlow is returned.

\returns

The parsed signed value, clamped to the limits on overflow.

******************************************************************************/

static long long strtoli(
    /** pointer to the scan position */         const char **s,
    /** number base */                          int base,
    /** returns the character count consumed */ int *cnt,
    /** returns the error flag */               int *err,
    /** field width limit (0 for none) */       int fld,
    /** input file when the string is NULL */   FILE *fd
)

{

    int                sgn; /* sign of number */
    unsigned long long v;   /* value */
    int                o;   /* overflow */

    /* process using universal signed/unsigned function */
    v = strtoulso(s, base, &sgn, &o, cnt, err, fld, fd);

    /* return proper value, including overflow cases */
    if (o && sgn > 0) return (LLONG_MAX); /* overflow and positive */
    else if (o && sgn < 0) return (LLONG_MIN); /* overflow and negative */
    else return ((long long)v*sgn); /* return value in correct sign */

}

/** **************************************************************************

Function strtouli

Description

Convert string to unsigned long number.

The given string is converted to a unsigned long integer, see the strtoulso
function. Takes a base radix, an input count, and a field.

If the number overlows, it will be replaced with MAX, and no overlow is
returned.

\returns

The parsed unsigned value, clamped to the maximum on overflow.

******************************************************************************/

static unsigned long long strtouli(
    /** pointer to the scan position */         const char **s,
    /** number base */                          int base,
    /** returns the character count consumed */ int *cnt,
    /** returns the error flag */               int *err,
    /** field width limit (0 for none) */       int fld,
    /** input file when the string is NULL */   FILE *fd
)

{

    int                sgn; /* sign of number */
    unsigned long long v;   /* value */
    int                o;   /* overflow */

    /* process using universal signed/unsigned function */
    v = strtoulso(s, base, &sgn, &o, cnt, err, fld, fd);

    /* return proper value, including overflow cases */
    if (o) return (ULLONG_MAX); /* overflow */
    else return (v*sgn); /* return value in correct sign */

}

/** **************************************************************************

Function strtodi

Description

Convert string to floating point number.

Parses a floating point number from a string or file. The number has the form:

[sign] digits [ . digits ] [ (e|E) [sign] digits ]

A leading sign, an integer part, a fractional part, and an exponent are all
optional, but at least one digit must appear in the integer or fractional part.
Leading whitespace is skipped. A maximum field width is accepted that limits the
number of characters parsed. The string pointer is advanced past the number, a
count of characters passed is kept, and an error flag is returned true if no
valid number was found.

This is the floating point engine shared by the formatted input functions.

\returns

The parsed floating point value; the error status is returned through a pointer.

Notes

1. Overflow and underflow of the result are not detected.

******************************************************************************/

static double strtodi(
    /** pointer to the scan position */         const char **s,
    /** returns the character count consumed */ int *cnt,
    /** returns the error flag */               int *err,
    /** field width limit (0 for none) */       int fld,
    /** input file when the string is NULL */   FILE *fd
)

{

    long double v;  /* value (extended precision: avoids overflow to inf and
                         accumulated error when scaling by large exponents) */
    long double frac; /* fractional digit scale */
    int    sgn;  /* sign of number */
    int    esgn; /* sign of exponent */
    int    ev;   /* exponent value */
    int    digs; /* at least one digit was seen */
    int    c;    /* current character */

    *err = FALSE; /* set no error */
    v = 0.0L; /* clear value */
    sgn = 1; /* set positive */
    digs = FALSE; /* no digits seen yet */

    /* skip leading whitespace */
    while (isspace(chkfstrlen(*s, fld, fd))) { getfstr(s, fd); (*cnt)++; }

    /* parse optional sign */
    c = chkfstrlen(*s, fld, fd);
    if (c == '+' || c == '-') {

        if (c == '-') sgn = -1; /* set negative */
        getfstr(s, fd); fld--; (*cnt)++; /* skip sign */

    }

    /* parse integer part */
    while ((c = chkfstrlen(*s, fld, fd)) >= '0' && c <= '9') {

        v = v*10.0L+(c-'0'); /* accumulate digit */
        getfstr(s, fd); fld--; (*cnt)++; /* skip digit */
        digs = TRUE; /* a digit was seen */

    }

    /* parse fractional part */
    if (chkfstrlen(*s, fld, fd) == '.') {

        getfstr(s, fd); fld--; (*cnt)++; /* skip decimal point */
        frac = 0.1L; /* first fractional place */
        while ((c = chkfstrlen(*s, fld, fd)) >= '0' && c <= '9') {

            v += (c-'0')*frac; /* add fractional digit */
            frac /= 10.0L; /* next fractional place */
            getfstr(s, fd); fld--; (*cnt)++; /* skip digit */
            digs = TRUE; /* a digit was seen */

        }

    }

    /* a number with no digits at all is an error */
    if (!digs) { *err = TRUE; return (0.0); }

    v *= sgn; /* apply sign */

    /* parse optional exponent */
    c = chkfstrlen(*s, fld, fd);
    if (c == 'e' || c == 'E') {

        getfstr(s, fd); fld--; (*cnt)++; /* skip exponent marker */
        esgn = 1; /* set positive exponent */
        ev = 0; /* clear exponent */
        c = chkfstrlen(*s, fld, fd);
        if (c == '+' || c == '-') {

            if (c == '-') esgn = -1; /* set negative exponent */
            getfstr(s, fd); fld--; (*cnt)++; /* skip sign */

        }
        while ((c = chkfstrlen(*s, fld, fd)) >= '0' && c <= '9') {

            ev = ev*10+(c-'0'); /* accumulate exponent digit */
            getfstr(s, fd); fld--; (*cnt)++; /* skip digit */

        }
        /* scale value by the exponent */
        while (ev > 0) {

            if (esgn > 0) v *= 10.0L; else v /= 10.0L;
            ev--;

        }

    }

    return ((double)v); /* narrow to double with a single final rounding */

}

/*******************************************************************************

External API section

This begins the external API section.

*******************************************************************************/

/** **************************************************************************

Function fopen

Description

Open a new or existing file.

Opens a new or existing file according to the given mode. The filename is given
by a zero terminated string. The mode is a zero terminated string consisting
of the following modes:

r   Open text file for read only.
w   Open text file for write only, truncate existing file.
a   Open text file to write at end.
r+  Open text file for read or write.
w+  Open text file for read or write, truncate existing file.
a+  Open text file for read or write at end.
rb  Open binary file for read only.
wb  Open binary file for write only, truncate existing file.
ab  Open binary file to write at end.
r+b Open binary file for read or write.
w+b Open binary file for read or write, truncate existing file.
a+b Open binary file for read or write at end.

In this implementation, the characters can appear in any order, and extra
characters or duplicates are ignored.

\returns

The open file, or NULL on error.

******************************************************************************/

FILE *fopen(
    /** name of the file to open */ const char *filename,
    /** access mode string */       const char *mode
)

{

    int fti;    /* file table index */
    int flags;  /* open flag settings */
    int text;   /* text/binary mode */
    int modcod; /* mode code, 0 = read, 1 = write, 2 = append */
    int append; /* append mode */
    int perm;   /* permissions */

    /* move mode attributes to flags */
    modcod = 0; /* default to read */
    text = !strchr(mode, 'b'); /* set text or binary mode */
    append = !!strchr(mode, '+'); /* set append mode */
    if (strchr(mode, 'r')) modcod = 0; /* set read */
    else if (strchr(mode, 'w')) modcod = 1; /* set write */
    else if (strchr(mode, 'a')) modcod = 2; /* set append */
    else return NULL; /* bad mode */

    /* clear flags for building */
    flags = 0;
    if (append) flags = O_RDWR; /* set append mode, read or write */
    else if (modcod == 0) flags |= O_RDONLY; /* for read, set read only */
    else flags |= O_WRONLY; /* for write or append, set write only */
    if (modcod == 1) flags |= O_CREAT | O_TRUNC; /* write: create and truncate */
    if (modcod == 2) flags |= O_CREAT | O_APPEND; /* append: create at end */

    /* permissions are: user read and write, group and others read only */
#ifdef __linux__
    perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
    perm = S_IRUSR | S_IWUSR;
#endif

    /* process file open with parameters */
    fti = maknod(); /* create or reuse file entry */
    if (!fti) return NULL; /* couldn't create node */
    opnfil[fti]->_fileno = vopen(filename, flags, perm); /* open file with flags */
    if (opnfil[fti]->_fileno < 0) return (NULL); /* return error */

    /* fill file fields with status */
    opnfil[fti]->name = (char *)malloc(strlen(filename)+1); /* get name space */
    if (!opnfil[fti]->name) return NULL; /* couldn't allocate name */
    strcpy(opnfil[fti]->name, filename); /* copy name into place */
    opnfil[fti]->text = text; /* text/binary mode */
    /* set read/write capability and append flags (full buffering by default) */
    opnfil[fti]->_flags = _IO_IS_FILEBUF; /* file backed, no status flags */
    if (!append) { /* single direction stream */

        if (modcod == 0) opnfil[fti]->_flags |= _IO_NO_WRITES; /* read only */
        else opnfil[fti]->_flags |= _IO_NO_READS; /* write or append only */

    }
    if (modcod == 2) opnfil[fti]->_flags |= _IO_IS_APPENDING; /* append mode */

    /* return new file entry */
    return (opnfil[fti]);

}

/** **************************************************************************

Function fflush

Description

Flushes output on the given file, or all files.

Causes all buffered writes to a given file, or all files, to be written
immediately. If a stream is passed, that is flushed. If NULL is passed, then all
files are flushed.

\returns

Returns EOF if an error occurs while writing to a file, otherwise 0.

******************************************************************************/

int fflush(
    /** file to flush, or NULL to flush all files */ FILE *stream
)

{

    int i; /* table index */
    int r; /* result holder */

    if (stream) { /* flush a single stream */

        if (stream->_fileno < 0) return (EOF); /* not open */
        return (wflush(stream)); /* write out any pending output */

    }

    /* a NULL stream means flush all open output streams */
    r = 0; /* set no error */
    for (i = 0; i < FOPEN_MAX; i++)
        if (opnfil[i] && opnfil[i]->_fileno >= 0)
            if (wflush(opnfil[i])) r = EOF; /* note any write error */

    return (r);

}

/** **************************************************************************

Function fclose

Description

Closes an open file.

Closes an open file entry. Returns EOF for error if the file is not in fact
open, or if the Unix close function returns an error.

\returns

Returns EOF for error if the file is not in fact open, or if the Unix close
function returns an error.

******************************************************************************/

int fclose(
    /** file to close */ FILE *stream
)

{

    int r; /* result holder */
    int w; /* flush result */

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (EOF);
    w = wflush(stream); /* flush any pending output */
    r = vclose(stream->_fileno); /* close file */
    stream->_fileno = -1; /* flag file tracking entry now free */
    free(stream->name); /* free name string */
    stream->name = 0; /* clear name string pointer */
    /* release the buffer if we allocated it */
    if (!(stream->_flags & _IO_USER_BUF) && stream->_IO_buf_base)
        free(stream->_IO_buf_base);
    stream->_IO_buf_base = stream->_IO_buf_end = NULL; /* no buffer */
    stream->_IO_read_base = stream->_IO_read_ptr = stream->_IO_read_end = NULL;
    stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_write_end = NULL;
    stream->_flags &= ~_IO_CURRENTLY_PUTTING; /* not writing */
    /* determine result code */
    if (r < 0 || w) return EOF; /* close or flush failed */
    else return 0; /* closed properly */

}

/** **************************************************************************

Function freopen

Description

Reopen an existing file under a new name and mode.

Closes the given file and reopens it under a new name and mode. Basically, it's
the same as fopen, but uses an existing file entry.

\returns

The reopened stream, or NULL on error.

******************************************************************************/

FILE *freopen(
    /** name of the file to open */ const char *filename,
    /** access mode string */       const char *mode,
    /** file to reopen */           FILE *stream
)

{

    int flags;  /* open flag settings */
    int text;   /* text/binary mode */
    int modcod; /* mode code, 0 = read, 1 = write, 2 = append */
    int update; /* update mode */
    int perm;   /* permissions */

    /* close the stream, and return error if occurs */
    if (fclose(stream) != 0) return (NULL);

    /* move mode attributes to flags */
    text = !strchr(mode, 'b'); /* set text or binary mode */
    update = !!strchr(mode, '+'); /* set update mode */
    if (strchr(mode, 'r')) modcod = 0; /* set read */
    else if (strchr(mode, 'w')) modcod = 1; /* set write */
    else if (strchr(mode, 'a')) modcod = 2; /* set append */
    else return NULL; /* bad mode */

    /* clear flags for building */
    flags = 0;
    if (update) flags = O_RDWR; /* set append mode, read or write */
    else if (modcod == 0) flags |= O_RDONLY; /* for read, set read only */
    else flags |= O_WRONLY; /* for write or append, set write only */
    if (modcod == 1) flags |= O_CREAT | O_TRUNC; /* write: create and truncate */
    if (modcod == 2) flags |= O_CREAT | O_APPEND; /* append: create at end */

    /* permissions are: user read and write, group and others read only */
#ifdef __linux__
    perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
    perm = S_IRUSR | S_IWUSR;
#endif

    /* process file open with parameters */
    stream->_fileno = vopen(filename, flags, perm); /* open file with flags */
    if (stream->_fileno < 0) return (NULL); /* return error */

    /* fill file fields with status */
    stream->name = (char *)malloc(strlen(filename)+1); /* get name space */
    if (!stream->name) return NULL; /* couldn't allocate name */
    strcpy(stream->name, filename); /* copy name into place */
    stream->text = text; /* text/binary mode */
    /* set read/write capability and append flags. The fclose above already
       released any old buffer; reset to full buffering, allocated on use */
    stream->_flags = _IO_IS_FILEBUF; /* file backed, no status flags */
    if (!update) { /* single direction stream */

        if (modcod == 0) stream->_flags |= _IO_NO_WRITES; /* read only */
        else stream->_flags |= _IO_NO_READS; /* write or append only */

    }
    if (modcod == 2) stream->_flags |= _IO_IS_APPENDING; /* append mode */
    /* clear the buffer pointers (buffer is allocated on first use) */
    stream->_IO_buf_base = stream->_IO_buf_end = NULL;
    stream->_IO_read_base = stream->_IO_read_ptr = stream->_IO_read_end = NULL;
    stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_write_end = NULL;

    /* return new file entry */
    return stream;

}

/** **************************************************************************

Function fdopen

Description

Open a stream file with existing file descriptor.

Given an existing file descriptor, creates a stream file and opens it with that
file descriptor. This essentially means to "fileofy" an existing, already open,
low level file descriptor to allow stream file operations on it.

The mode must be compatible with the mode of the file descriptor.

\returns

The open file, or NULL on error.

Notes

1. Mingw does not implement fcntl() F_GETFL command, so we cannot check modes
are compatible.

******************************************************************************/

FILE *fdopen(
    /** open file descriptor */ int fd,
    /** access mode string */   const char *mode
)

{

    int fti;    /* file table index */
    int flags;  /* open flag settings */
    int text;   /* text/binary mode */
    int modcod; /* mode code, 0 = read, 1 = write, 2 = append */
    int append; /* append mode */
    int perm;   /* permissions */
    int fsf;    /* file status flags */

    if (fd < 0) { /* invalid fd */

        errno = EBADF; /* set error */
        return (NULL); /* flag error to user */

    }

    /* move mode attributes to flags */
    modcod = 0; /* default to read */
    text = !strchr(mode, 'b'); /* set text or binary mode */
    append = !!strchr(mode, '+'); /* set update mode */
    if (strchr(mode, 'r')) modcod = 0; /* set read */
    else if (strchr(mode, 'w')) modcod = 1; /* set write */
    else if (strchr(mode, 'a')) modcod = 2; /* set append */
    else return NULL; /* bad mode */

    /* clear flags for building */
    flags = 0;
    if (append) flags = O_RDWR; /* set append mode, read or write */
    else if (modcod == 0) flags |= O_RDONLY; /* for read, set read only */
    else flags |= O_WRONLY; /* for write or append, set write only */
    if (modcod == 1) flags |= O_CREAT | O_TRUNC; /* write: create and truncate */
    if (modcod == 2) flags |= O_CREAT | O_APPEND; /* append: create at end */

    /* permissions are: user read and write, group and others read only */
#ifdef __linux__
    perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
    perm = S_IRUSR | S_IWUSR;
#endif

    /* process file open with parameters */
    fti = maknod(); /* create or reuse file entry */
    if (!fti) {

        errno = ENOMEM; /* set error */
        return (NULL); /* couldn't create node */

    }
    opnfil[fti]->_fileno = fd; /* set file id */

    /* get and match existing file parameters */
#ifndef __MINGW32__
    /* note mingw does not implement this call at present. The result will be
       that this call will not check the modes are equivalent. */
    fsf = fcntl(fd, F_GETFL);
    /* Only the access mode and O_APPEND are reported by F_GETFL; the creation
       flags O_CREAT/O_TRUNC/O_EXCL are open time only and are never returned,
       so they must not be part of the compatibility comparison. */
    if ((fsf & (O_ACCMODE | O_APPEND)) != (flags & (O_ACCMODE | O_APPEND))) {

        errno = EINVAL; /* flag error */
        return (NULL); /* return error */

    }
#endif

    /* fill file fields with status */
    opnfil[fti]->text = text; /* text/binary mode */
    /* set read/write capability and append flags (full buffering by default) */
    opnfil[fti]->_flags = _IO_IS_FILEBUF; /* file backed, no status flags */
    if (!append) { /* single direction stream */

        if (modcod == 0) opnfil[fti]->_flags |= _IO_NO_WRITES; /* read only */
        else opnfil[fti]->_flags |= _IO_NO_READS; /* write or append only */

    }
    if (modcod == 2) opnfil[fti]->_flags |= _IO_IS_APPENDING; /* append mode */

    /* return new file entry */
    return (opnfil[fti]);

}

/** **************************************************************************

Function remove

Description

Removes a file from the file system.

Removes a file, by deleting it permanently from the file system. Returns
non-zero if the attempt to remove fails, otherwise zero.

\returns

Returns non-zero if the attempt to remove fails, otherwise zero.

******************************************************************************/

int remove(
    /** name of the file to remove */ const char *filename
)

{

    /* unlink the filename, returning error */
    return (!!vunlink(filename));

}

/** **************************************************************************

Function rename

Description

Change the name of an existing file.

Changes the name of an existing file. Both the old and new names are provided.
It is an error if the old filename does not exist.

There is no rename in the low level funnel set, so we construct the operation
from a link() and an unlink(): the existing file is given a new name, then the
old name is removed. If the new name already exists it is removed first, to give
the replacement behavior required of rename(). The unlink() steps go through the
hookable vunlink() vector, the same one used by remove().

\returns

Returns non-zero if the attempt to rename fails, otherwise zero.

Notes

1. Because the operation is built from link()/unlink() rather than a true
rename() system call, it is not atomic, it cannot move a file across file
systems (link() returns EXDEV), and it generally cannot rename directories
(link() of a directory is not permitted on most systems).

******************************************************************************/

/* Windows provides no POSIX link() to build the link()/unlink() rename()
   from; its own is below. */
#ifndef __MINGW32__
int rename(
    /** existing file name */ const char *oldname,
    /** new file name */      const char *newname
)

{

    /* nothing to do if the names are identical */
    if (!strcmp(oldname, newname)) return (0);

    /* give the existing file the new name */
    if (link(oldname, newname) < 0) {

        /* if the new name already exists, replace it as rename() requires */
        if (errno != EEXIST) return (-1); /* some other error, give up */
        if (vunlink(newname) < 0) return (-1); /* cannot remove old target */
        if (link(oldname, newname) < 0) return (-1); /* still cannot link */

    }

    /* remove the old name to complete the move */
    if (vunlink(oldname) < 0) { /* cannot remove old name */

        vunlink(newname); /* back out the new name we created */
        return (-1);

    }

    return (0); /* success */

}
#endif

/* On Windows the C runtime's rename() refuses an existing target, where
   rename() is specified to replace it, so the runtime's is not used: MoveFileEx
   gives the replacing move, across volumes as well. The two calls are declared
   here rather than pulling windows.h into stdio. */
#ifdef __MINGW32__
__declspec(dllimport) int __stdcall MoveFileExA(const char* oldname,
                                                const char* newname,
                                                unsigned long flags);
__declspec(dllimport) unsigned long __stdcall GetLastError(void);
#define WINRENAME_REPLACE 0x1 /* MOVEFILE_REPLACE_EXISTING */
#define WINRENAME_COPY    0x2 /* MOVEFILE_COPY_ALLOWED */

static int winrename(const char *oldname, const char *newname)

{

    if (!strcmp(oldname, newname)) return (0); /* nothing to do */
    if (!MoveFileExA(oldname, newname, WINRENAME_REPLACE | WINRENAME_COPY)) {

        errno = GetLastError() == 2? ENOENT: EACCES; /* 2: file not found */
        return (-1);

    }

    return (0); /* success */

}

#ifndef STDIO_BYPASS
/* the custom stdio overrides the C runtime's calls directly: this rename()
   takes the place of the runtime's */
int rename(
    /** existing file name */ const char *oldname,
    /** new file name */      const char *newname
)

{

    return (winrename(oldname, newname));

}
#else
/* A STDIO_BYPASS consumer has its rename() calls coined to stdio_rename()
   (see stdio.h), so that funnel entry is the one provided. */
int stdio_rename(const char *oldname, const char *newname)

{

    return (winrename(oldname, newname));

}
#endif
#endif

/** **************************************************************************

Function tmpdir

Description

Find the temporary file directory.

The standard temporary directory environment variables are consulted in turn
(TMPDIR, then TEMP, then TMP, which covers both Unix and Windows conventions),
and if none is set the compiled in default P_tmpdir is used.

\returns

Returns the directory in which temporary files should be created.

******************************************************************************/

static const char *tmpdir(void)

{

    const char *d; /* directory holder */

    if ((d = getenv("TMPDIR")) && *d) return (d); /* Unix convention */
    if ((d = getenv("TEMP")) && *d) return (d); /* Windows convention */
    if ((d = getenv("TMP")) && *d) return (d); /* Windows convention */

    return (P_tmpdir); /* fall back to the default */

}

/** **************************************************************************

Function tmpgen

Description

Generate a candidate temporary file name.

Builds a temporary file name into the given buffer, which has the given length
limit. The name is formed from the temporary directory, the process id (so that
separate processes do not collide), and an advancing counter (so that repeated
calls within a process do not collide). If the full path including the directory
will not fit the buffer, the directory is dropped and a current directory
relative name is produced instead.

The name is checked against the file system and only a name that does not
already exist is returned. Up to TMP_MAX names are tried. Returns zero on
success with the name in the buffer, or non-zero if no free name could be
generated.

\returns

Returns zero on success with the name in the buffer, or non-zero if no free
name could be generated.

******************************************************************************/

static int tmpgen(
    /** destination buffer for the name */ char *buf,
    /** size of the buffer */              int lim
)

{

    const char *d;       /* temporary directory */
    long        pid;     /* process id */
    int         tries;   /* attempt counter */
    char        comp[40]; /* the unique name component */

    d = tmpdir(); /* get the temporary directory */
    pid = (long)getpid(); /* get the process id */
    for (tries = 0; tries < TMP_MAX; tries++) { /* search for a free name */

        tmpcnt++; /* advance the counter for a fresh name */
        sprintf(comp, "tmp.%ld.%d", pid, tmpcnt); /* build unique component */
        /* place into the directory if it fits, otherwise use a bare name */
        if ((int)(strlen(d)+1+strlen(comp)+1) <= lim)
            sprintf(buf, "%s/%s", d, comp); /* directory + name */
        else if ((int)(strlen(comp)+1) <= lim)
            strcpy(buf, comp); /* current directory relative name */
        else return (-1); /* cannot fit even the bare name */
        if (access(buf, F_OK) != 0) return (0); /* the name is free, use it */

    }

    return (-1); /* could not find a free name */

}

/** **************************************************************************

Function tmpfile

Description

Create temporary file.

Creates and opens a temporary file in mode "wb+". The file is removed
automatically when it is closed or when the program ends. This is achieved by
unlinking the name immediately after creation, so the file persists only as long
as the open descriptor does, and is reclaimed even on an abnormal exit.

\returns

Returns the open file, or NULL if a temporary file could not be created.

Notes

1. The unlink on open technique is a Unix property. On systems that do not allow
an open file to be unlinked, the file would remain until removed.

******************************************************************************/

FILE *tmpfile(void)

{

    char nm[512]; /* temporary name, large enough for any temp directory */
    int  fd;      /* file descriptor */
    int  perm;    /* permissions */
    int  tries;   /* attempt counter */
    FILE *fp;     /* opened file */

    /* permissions are: user read and write only for a temporary file */
    perm = S_IRUSR | S_IWUSR;

    for (tries = 0; tries < TMP_MAX; tries++) { /* search for a usable name */

        if (tmpgen(nm, sizeof(nm))) return (NULL); /* no free name */
        /* create the file exclusively so a racing process cannot reuse it */
        fd = vopen(nm, O_CREAT | O_EXCL | O_RDWR, perm);
        if (fd >= 0) { /* created successfully */

            vunlink(nm); /* remove the name; file lives until the fd closes */
            fp = fdopen(fd, "wb+"); /* wrap a stream around the descriptor */
            if (!fp) vclose(fd); /* could not wrap, release the descriptor */
            return (fp); /* return the stream (or NULL on fdopen failure) */

        }
        if (errno != EEXIST) return (NULL); /* a real error, give up */
        /* otherwise the name was taken in a race, try another */

    }

    return (NULL); /* could not create a temporary file */

}

/** **************************************************************************

Function tmpnam

Description

Create (coin) temporary filename.

Creates a name that does not match any existing file, suitable for use as a
temporary file name. If the buffer pointer is NULL, the name is placed in an
internal static buffer that is overwritten by each call, and a pointer to it is
returned. Otherwise the name is placed in the given buffer, which must be at
least L_tmpnam characters, and that buffer is returned.

\returns

Returns NULL if a unique name could not be generated.

Notes

1. As with all uses of tmpnam(), there is a window between generating the name
and the caller opening it in which another program could create the same name.
Use tmpfile() where that race matters.

******************************************************************************/

char *tmpnam(
    /** buffer to receive the name, or NULL for a static buffer */ char s[]
)

{

    static char ibuf[L_tmpnam]; /* internal buffer for the NULL case */
    char       *p; /* working pointer */

    p = s ? s : ibuf; /* use caller buffer or the internal one */
    if (tmpgen(p, L_tmpnam)) return (NULL); /* could not generate a name */

    return (p); /* return the name */

}

/** **************************************************************************

Function setvbuf

Description

Set buffering characteristics for file.

Determines file buffering mode for an open file. Must be called before any read
or write operations are performed on the file. The stream for the file to be
changed, the buffering mode, the buffer size, and a pointer to a buffer for
the file is provided. If the buffer pointer is NULL, then a buffer will be
allocated instead. The valid buffer modes are:

_IOFBF  Full buffering
_IOLBF  Line buffering (buffer flushed by line end)
_IONBF  No buffering

\returns

Returns non-zero for error, otherwise zero.

******************************************************************************/

int setvbuf(
    /** file to set buffering on */                  FILE *stream,
    /** user buffer, or NULL to allocate one */      char *buf,
    /** buffering mode (_IOFBF, _IOLBF or _IONBF) */ int mode,
    /** buffer size */                               size_t size
)

{

    /* validate stream and mode */
    if (!stream || stream->_fileno < 0) return (EOF);
    if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) return (EOF);

    /* this must be called before any I/O, so clear out anything pending */
    if (stream->_flags & _IO_CURRENTLY_PUTTING) wflush(stream);
    else rflush(stream);
    stream->_flags &= ~_IO_CURRENTLY_PUTTING; /* back to idle/read */

    /* release any buffer we previously allocated */
    if (!(stream->_flags & _IO_USER_BUF) && stream->_IO_buf_base)
        free(stream->_IO_buf_base);
    stream->_IO_buf_base = stream->_IO_buf_end = NULL; /* no buffer */
    stream->_IO_read_base = stream->_IO_read_ptr = stream->_IO_read_end = NULL;
    stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_write_end = NULL;

    /* set the buffering mode flags */
    stream->_flags &= ~(_IO_UNBUFFERED | _IO_LINE_BUF | _IO_USER_BUF);
    if (mode == _IONBF) { stream->_flags |= _IO_UNBUFFERED; return (0); }
    if (mode == _IOLBF) stream->_flags |= _IO_LINE_BUF; /* line buffered */

    /* set up the buffer, allocating one if the caller did not supply it */
    if (!size) size = BUFSIZ; /* default size */
    if (!buf) { /* allocate our own buffer of the requested size */

        buf = malloc(size);
        if (!buf) { stream->_flags |= _IO_UNBUFFERED; return (EOF); }

    } else stream->_flags |= _IO_USER_BUF; /* caller owns this buffer */
    stream->_IO_buf_base = buf; /* set buffer base */
    stream->_IO_buf_end = buf+size; /* set buffer end */
    /* start with empty get and put areas at the buffer base */
    stream->_IO_read_base = stream->_IO_read_ptr = stream->_IO_read_end = buf;
    stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_write_end = buf;

    return (0);

}

/** **************************************************************************

Function setbuf

Description

Sets up a buffer for the file.

Either sets up a buffer to be used with the file, or sets buffering off if the
buffer pointer is NULL.

\returns

none

******************************************************************************/

void setbuf(
    /** file to set buffering on */            FILE *stream,
    /** user buffer, or NULL for unbuffered */ char *buf
)

{

    if (!buf) setvbuf(stream, buf, _IONBF, 0); /* turn buffering off */
    else (void) setvbuf(stream, buf, _IOFBF, BUFSIZ);

}

/** **************************************************************************

Function fmtfloat

Description

Format a floating point number to a string.

Converts a double to a string according to one of the floating point format
specifiers 'f', 'e', 'E', 'g' or 'G'. The precision and the '+', ' ' and '#'
flags are honored. The resulting string, including any sign, is placed in a
buffer allocated with malloc(), which the caller must free(). Field width and
justification are left to the caller. 

The conversion rounds to the requested number of digits. Non-finite values
(infinities and NaNs) are converted to "inf"/"nan" (or "INF"/"NAN" for the
upper case specifiers).

This routine is the floating point engine shared by all of the formatted output
functions.

\returns

Returns NULL if allocation fails.

Notes

1. The whole part of an 'f' conversion is generated through a double, so very
large magnitudes lose precision in the same way the underlying double does.

******************************************************************************/

/** **************************************************************************

Exact decimal conversion support (clean room big integer)

A finite double equals m*2^k for an integer significand m and binary exponent k.
To convert it to decimal it is scaled to a rational A/B whose value is the number
divided by 10^decexp, arranged to lie in [1,10), and decimal digits are then
produced by exact big integer long division. The final kept digit is rounded to
nearest, ties to even, decided on the exact remainder. The result is therefore a
correctly rounded conversion, identical to the system library for every value and
precision, with no accumulated floating point error. This is an original
implementation; no third party conversion code is used.

******************************************************************************/

#define FF_NL 80 /* big integer size in 32 bit limbs (2560 bits) */

typedef struct { unsigned v[FF_NL]; int n; } ffbn;

static void ffnorm(ffbn* a) { while (a->n > 1 && a->v[a->n-1] == 0) a->n--; }
static int  ffzero(const ffbn* a) { return a->n == 1 && a->v[0] == 0; }

static void ffset(ffbn* a, unsigned long long x) /* a = x */
{
    memset(a->v, 0, sizeof(a->v));
    a->v[0] = (unsigned)x; a->v[1] = (unsigned)(x>>32);
    a->n = a->v[1] ? 2 : 1;
}

static int ffcmp(const ffbn* a, const ffbn* b) /* compare magnitudes */
{
    int i;
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (i = a->n-1; i >= 0; i--)
        if (a->v[i] != b->v[i]) return a->v[i] < b->v[i] ? -1 : 1;
    return 0;
}

static void ffmuls(ffbn* a, unsigned s) /* a *= s (s a small value) */
{
    unsigned long long carry = 0;
    int i;
    for (i = 0; i < a->n; i++) {
        unsigned long long t = (unsigned long long)a->v[i]*s + carry;
        a->v[i] = (unsigned)t; carry = t>>32;
    }
    while (carry && a->n < FF_NL) { a->v[a->n++] = (unsigned)carry; carry >>= 32; }
}

static void ffmul10(ffbn* a, int p) { while (p-- > 0) ffmuls(a, 10); } /* a *= 10^p */

static void ffshl(ffbn* a, int bits) /* a <<= bits (a *= 2^bits) */
{
    int word = bits/32, bit = bits%32, i;
    if (word) {
        for (i = a->n-1; i >= 0; i--) a->v[i+word] = a->v[i];
        for (i = 0; i < word; i++) a->v[i] = 0;
        a->n += word;
    }
    if (bit) {
        unsigned carry = 0;
        for (i = word; i < a->n; i++) {
            unsigned long long t = ((unsigned long long)a->v[i]<<bit) | carry;
            a->v[i] = (unsigned)t; carry = (unsigned)(t>>32);
        }
        if (carry && a->n < FF_NL) a->v[a->n++] = carry;
    }
    ffnorm(a);
}

static void ffsub(ffbn* a, const ffbn* b) /* a -= b, requires a >= b */
{
    long long borrow = 0;
    int i;
    for (i = 0; i < a->n; i++) {
        long long t = (long long)a->v[i] - (i < b->n ? b->v[i] : 0) - borrow;
        if (t < 0) { t += 0x100000000LL; borrow = 1; } else borrow = 0;
        a->v[i] = (unsigned)t;
    }
    ffnorm(a);
}

/* Build A, B and decexp so that A/B equals d/10^decexp and lies in [1,10).
   d must be finite and greater than zero. */
static void ffsetup(double d, ffbn* A, ffbn* B, int* pdexp)
{
    unsigned long long bits, mant, m;
    int ef, k, dexp, hb;
    ffbn t;

    memcpy(&bits, &d, 8);
    ef = (int)((bits>>52) & 0x7ff);
    mant = bits & 0xfffffffffffffULL;
    if (ef == 0) { m = mant; k = -1074; }                 /* subnormal */
    else { m = mant | 0x10000000000000ULL; k = ef-1075; } /* normal */

    /* estimate decexp = floor(log10(d)) from the binary exponent. The
       normalization loops below correct any error, so a rough estimate is
       sufficient and no math library is needed. */
    hb = 63; while (!((m>>hb)&1)) hb--;         /* index of top set bit of m */
    dexp = (int)((k+hb) * 0.3010299956639812);  /* * log10(2) */

    ffset(A, m); ffset(B, 1);
    if (k >= 0) ffshl(A, k); else ffshl(B, -k);
    if (dexp >= 0) ffmul10(B, dexp); else ffmul10(A, -dexp);

    /* correct into [1,10) */
    while (ffcmp(A, B) < 0) { ffmuls(A, 10); dexp--; }
    for (;;) { t = *B; ffmuls(&t, 10); if (ffcmp(A, &t) >= 0) { *B = t; dexp++; }
               else break; }
    *pdexp = dexp;
}

/* decimal exponent of the leading digit of d (floor(log10|d|)), 0 for zero */
static int ffdecexp(double d)
{
    ffbn A, B; int e;
    if (d == 0) return 0;
    if (d < 0) d = -d;
    ffsetup(d, &A, &B, &e);
    return e;
}

/* Produce ndig significant decimal digits of |d| in digits[0..ndig-1],
   correctly rounded with ties to even, and set *pe to the decimal exponent of
   the leading digit. d may be zero. ndig must be at least 1. */
static void ffgen(double d, int ndig, char* digits, int* pe)
{
    ffbn A, B; int dexp, i, done = 0;

    if (d == 0) { for (i = 0; i < ndig; i++) digits[i] = '0'; *pe = 0; return; }
    if (d < 0) d = -d;
    ffsetup(d, &A, &B, &dexp);
    *pe = dexp;
    for (i = 0; i < ndig; i++) {
        int dig = 0;
        if (done) { digits[i] = '0'; continue; } /* exact value exhausted */
        while (ffcmp(&A, &B) >= 0) { ffsub(&A, &B); dig++; } /* this digit */
        digits[i] = (char)('0'+dig);
        if (ffzero(&A)) { done = 1; continue; } /* remainder zero: rest are 0 */
        if (i < ndig-1) ffmuls(&A, 10); /* leave remainder in place on last */
    }
    if (!done) { /* round to nearest, ties to even, on the exact remainder */
        int c;
        ffmuls(&A, 2); /* compare 2*remainder to B */
        c = ffcmp(&A, &B);
        if (c > 0 || (c == 0 && ((digits[ndig-1]-'0') & 1))) {
            for (i = ndig-1; i >= 0; i--) {
                if (digits[i] < '9') { digits[i]++; break; } /* carry absorbed */
                digits[i] = '0'; /* 9 rolls to 0, carry continues */
            }
            if (i < 0) { digits[0] = '1'; (*pe)++; } /* carried past the top */
        }
    }
}

/* For the 'f' boundary case (the rounded result is 0 or a single 1 in the last
   place), return nonzero if |d| rounds up at the 10^-pre place. The value rounds
   up when 2*|d| > 10^-pre; an exact half ties to even, and the kept digit is 0
   (even), so equality rounds down. Compared exactly: 2*d = m*2^(k+1), so the test
   is m*2^(k+1)*10^pre > 1. */
static int ffboundup(double d, int pre)
{
    ffbn A, B;
    unsigned long long bits, mant, m;
    int ef, k, kk;

    if (d < 0) d = -d;
    if (d == 0) return 0;
    memcpy(&bits, &d, 8);
    ef = (int)((bits>>52) & 0x7ff);
    mant = bits & 0xfffffffffffffULL;
    if (ef == 0) { m = mant; k = -1074; }
    else { m = mant | 0x10000000000000ULL; k = ef-1075; }
    ffset(&A, m); ffset(&B, 1);
    kk = k+1;
    if (kk >= 0) ffshl(&A, kk); else ffshl(&B, -kk);
    ffmul10(&A, pre);
    return ffcmp(&A, &B) > 0; /* strictly greater rounds up; equal ties to even */
}

static char *fmtfloat(
    /** value to convert */                       double d,
    /** precision (number of digits) */           int pre,
    /** precision specified flag */               int pres,
    /** conversion character (f, e, E, g or G) */ char cv,
    /** force a leading sign flag */              int sgn,
    /** leading space flag */                     int spc,
    /** alternate form flag */                    int alt
)

{

    char   *digits;  /* significant digits buffer */
    char   *buf;     /* result buffer */
    char   *p;       /* build pointer */
    char   *dpp;     /* decimal point position for zero stripping */
    int     ndig;    /* number of significant digits to generate */
    int     e;       /* decimal exponent of leading digit */
    int     e0;      /* estimated decimal exponent */
    int     sig;     /* significant digit count for 'f' */
    int     i, j;    /* indexes */
    int     idx;     /* digit index */
    int     neg;     /* number is negative */
    int     usef;    /* use fixed ('f') style */
    int     strip;   /* strip trailing zeros ('g' style) */
    int     up;      /* upper case conversion */
    char    ec;      /* exponent letter */
    int     P;       /* significant digits for 'g' */
    int     ae;      /* absolute exponent */
    char    estr[16];/* exponent digit holder */
    int     en;      /* exponent digit count */

    up = (cv == 'E' || cv == 'G'); /* set upper case form */
    ec = up ? 'E' : 'e'; /* set exponent letter */

    /* handle non-finite values (NaN and infinity) */
    if (d != d || d-d != 0) {

        buf = (char *)malloc(8); /* room for sign + "inf"/"nan" */
        if (!buf) return (NULL);
        p = buf;
        if (d == d) { /* infinity carries a sign */

            if (d < 0) *p++ = '-';
            else if (sgn) *p++ = '+';
            else if (spc) *p++ = ' ';
            strcpy(p, up ? "INF" : "inf");

        } else { /* NaN */

            if (sgn) *p++ = '+';
            else if (spc) *p++ = ' ';
            strcpy(p, up ? "NAN" : "nan");

        }

        return (buf);

    }

    /* default precision is 6 for 'f' and 'e' forms */
    if (!pres) pre = 6;

    /* extract and remove sign */
    neg = FALSE;
    if (d < 0) { neg = TRUE; d = -d; }

    /* exact decimal exponent of the leading digit (used for the 'f' digit
       count and the 'f' boundary case below) */
    e0 = ffdecexp(d);

    /* handle the 'f' boundary case where rounding occurs at or above the
       leading digit, so the result is either 0 or a single 1 in the last
       place of the requested precision */
    if (cv == 'f') {

        sig = e0+1+pre; /* significant digits down to the precision place */
        if (sig < 1) { /* rounds to 0 or 1 in the last place */

            int rup; /* round up */

            rup = ffboundup(d, pre); /* exact round to nearest, ties to even */
            buf = (char *)malloc(pre+8);
            if (!buf) return (NULL);
            p = buf;
            if (neg) *p++ = '-';
            else if (sgn) *p++ = '+';
            else if (spc) *p++ = ' ';
            if (pre > 0) { /* fractional digits present */

                *p++ = '0'; /* whole part is zero */
                *p++ = '.'; /* decimal point */
                for (j = 0; j < pre; j++)
                    *p++ = (rup && j == pre-1) ? '1' : '0';

            } else { /* no fraction */

                *p++ = rup ? '1' : '0'; /* whole part */
                if (alt) *p++ = '.'; /* alternate form keeps the point */

            }
            *p = 0; /* terminate */

            return (buf);

        }

    } else sig = 0; /* not used */

    /* determine significant digit count and style */
    strip = FALSE;
    if (cv == 'g' || cv == 'G') {

        P = pres ? (pre ? pre : 1) : 6; /* significant digits, minimum 1 */
        ndig = P;

    } else if (cv == 'f') ndig = sig; /* digits down to the precision place */
    else ndig = pre+1; /* 'e': leading digit plus precision */
    if (ndig < 1) ndig = 1;

    digits = (char *)malloc(ndig);
    if (!digits) return (NULL);

    /* generate the significant digits, correctly rounded (ties to even), via
       the exact big integer conversion, with e set to the leading exponent */
    ffgen(d, ndig, digits, &e);

    /* for 'g'/'G', choose the output style now that the exponent is known */
    if (cv == 'g' || cv == 'G') {

        if (e < -4 || e >= P) { usef = FALSE; pre = P-1; } /* exponential */
        else { usef = TRUE; pre = P-1-e; } /* fixed */
        if (pre < 0) pre = 0;
        if (!alt) strip = TRUE; /* strip trailing zeros unless alternate form */

    } else usef = (cv == 'f');

    /* allocate the result buffer with room for sign, digits, point and
       exponent */
    buf = (char *)malloc((e > 0 ? e : 0)+ndig+pre+24);
    if (!buf) { free(digits); return (NULL); }
    p = buf;
    dpp = NULL; /* no decimal point placed yet */

    /* place sign */
    if (neg) *p++ = '-';
    else if (sgn) *p++ = '+';
    else if (spc) *p++ = ' ';

    if (usef) { /* fixed point form */

        /* whole part */
        if (e < 0) *p++ = '0';
        else for (i = 0; i <= e; i++) *p++ = (i < ndig) ? digits[i] : '0';
        /* decimal point */
        if (pre > 0 || alt) { dpp = p; *p++ = '.'; }
        /* fraction */
        for (j = 0; j < pre; j++) {

            idx = e+1+j;
            *p++ = (idx >= 0 && idx < ndig) ? digits[idx] : '0';

        }
        /* strip trailing zeros for 'g'/'G' */
        if (strip && dpp) {

            while (p > dpp+1 && p[-1] == '0') p--; /* remove trailing zeros */
            if (p == dpp+1) p--; /* remove lone decimal point */

        }

    } else { /* exponential form */

        /* leading digit */
        *p++ = (ndig > 0) ? digits[0] : '0';
        /* fraction */
        if (pre > 0 || alt) {

            dpp = p; *p++ = '.';
            for (j = 0; j < pre; j++) *p++ = (1+j < ndig) ? digits[1+j] : '0';
            if (strip) { /* strip trailing zeros for 'g'/'G' */

                while (p > dpp+1 && p[-1] == '0') p--;
                if (p == dpp+1) p--; /* remove lone decimal point */

            }

        }
        /* exponent */
        *p++ = ec;
        *p++ = (e < 0) ? '-' : '+';
        ae = e < 0 ? -e : e; /* exponent magnitude */
        en = 0;
        do { estr[en++] = '0'+ae%10; ae /= 10; } while (ae); /* get digits */
        while (en < 2) estr[en++] = '0'; /* at least two exponent digits */
        while (en) *p++ = estr[--en]; /* place in order */

    }
    *p = 0; /* terminate */

    free(digits);

    return (buf);

}

/** **************************************************************************

Function vsprintfe

Description

Place converted formatted arguments in string.

Using a format string as a guide, a series of arguments are converted,
formatted, and placed in a string. The number of characters output to the
string are returned.

This extended routine can either place the output in a string, or output
directly to a file. This enables printing to a file without the need to allocate
a buffer. Using a buffer creates issues because routines based on vfprintf don't
specify a maximum length, so the buffer must be long enough to service all
such calls. The buffer size then is a compromise between using too much space
for a small task space, and not having enough space to service all calls.
Printing direct to a file solves this issue completely.

Both an output string and a file pointer are provided as parameters. If the
string is NULL, then the file parameter will be used.

The format string is copied from input to output with a series of "format
specifications" embedded of the form:

%[<modifier>][<field>[.<precision>]][<length modifier>]<format specifier>

Each format specification indicates one or more arguments from the variable
input list are to be converted, formatted, and mixed with the output to the
string. The fields are:

MODIFIER

-       Left justify
+       Print positive sign
(space) Print positive sign as space
0       Pad with leading zeros
#       Use alternate format (meaning specific to format specifier)

LENGTH MODIFIER

hh      char
h       short
l       long
ll      long long
L       long double (accepted, treated as double)
z       size_t

FORMAT SPECIFIER

d, i    Signed decimal
o       Unsigned octal
x       Unsigned hexidecimal, lower case
X       Unsigned hexidecimal, upper case
u       Unsigned decimal
c       Character
s       String
f       Float
e       Double float, lower case
E       Double float, upper case
g       Variable float, lower case
G       Variable float, upper case
p       Pointer
n       Place length output
%       % (the character %)

Note that this isn't designed to be a complete interpretation of sprintf
actions, please see the ANSI C documentation.

The actions taken on error conditions are to ignore and continue. A missing
field or precision (a '.' without a number before or after it) simply
results in a default field or precision of 1. A missing format specifier
results in the entire format specification being skipped and ignored.

As per ANSI C, there is no practical limit to the size of the output string,
and there is no real way to limit the total length output to a given buffer
size.

vsprintfe is used as a building block for all of the other formatted output
routines, fprintf, printf, sprintf, vprintf, vfprintf and vsprintf.

\returns

The number of characters output.

******************************************************************************/

static int vsprintfe(
    /** destination string, or NULL to write to the file */ char *s,
    /** format string */                                    const char *fmt,
    /** variable argument list */                           va_list ap,
    /** destination file when the string is NULL */         FILE *fd
)

{

    const char   nulmsg[] = "(null)"; /* null message */

    int           cnt;  /* number of characters processed */
    int           lft;  /* left justify field */
    int           sgn;  /* always print sign */
    int           spc;  /* space prefix sign */
    int           zer;  /* pad with leading zeros */
    int           alt;  /* alternate output form */
    int           fld;  /* minimum field width */
    int           pre;  /* precision */
    int           pres; /* precision was specified */
    int           lmod; /* length modifier code (LM_*) */
    int           i;    /* integer holding */
    long long     li;   /* signed wide integer holding */
    int           sn;   /* sign from number */
    int           dg;   /* number of digits in unsigned number */
    int           ndg;  /* net digits after precision */
    int           pe;   /* extra padding for precision */
    unsigned long long u; /* unsigned integer holding */
    int           r;    /* radix holder */
    unsigned long long p; /* power holder */
    char          *cp;  /* char pointer holder */
    int           l;    /* string length */
    double        d;    /* floating point holder */

    cnt = 0; /* clear output count */
    while (*fmt) { /* while format characters remain */

        if (*fmt == '%') { /* format specification */

            fmt++; /* skip '%' */

            /* clear convertion flags and fields */
            lft  = FALSE; /* right justify */
            sgn  = FALSE; /* print only negative signs */
            spc  = FALSE; /* don't use space for positive signs */
            zer  = FALSE; /* don't use leading zeros */
            alt  = FALSE; /* don't use alternate forms */
            fld  = 0;     /* set minimum output field */
            pre  = 1;     /* set precision */
            pres = FALSE; /* set precision not set */
            lmod = LM_NONE; /* no length modifier */

            /* check for modfiers */
            while (*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '0' ||
                   *fmt == '#') { /* process modifier */

                switch (*fmt++) { /* process and skip */

                    case '-': lft = TRUE; break;
                    case '+': sgn = TRUE; break;
                    case ' ': spc = TRUE; break;
                    case '0': zer = TRUE; break;
                    case '#': alt = TRUE; break;

                }

            }
            /* ISO 9899: if '-' is set, ignore '0' */
            if (lft) zer = FALSE;
            /* ISO 9899: if '+' is set, ignore ' ' */
            if (sgn) spc = FALSE;

            /* check for field */
            if (*fmt >= '0' && *fmt <= '9') getnum(&fmt, &fld);
            else if (*fmt == '*') { /* get field from parameter */

                fmt++; /* skip '*' */
                fld = va_arg(ap, int);

            }

            if (*fmt == '.') { /* precision specified */

                fmt++; /* skip '.' */
                pres = TRUE; /* precision was specified */

            }

            /* check for precision */
            if (*fmt >= '0' && *fmt <= '9') getnum(&fmt, &pre);
            else if (*fmt == '*') {

                fmt++; /* skip '.' */
                pre = va_arg(ap, int);

            } else pre = 0; /* ISO 9899: missing precision is 0 */

            /* check for length modifiers */
            if (*fmt == 'h') { /* short or char */

                fmt++; /* skip 'h' */
                if (*fmt == 'h') { fmt++; lmod = LM_HH; } /* hh: char */
                else lmod = LM_H; /* h: short */

            } else if (*fmt == 'l') { /* long or long long */

                fmt++; /* skip 'l' */
                if (*fmt == 'l') { fmt++; lmod = LM_LL; } /* ll: long long */
                else lmod = LM_L; /* l: long */

            } else if (*fmt == 'L') { fmt++; lmod = LM_CAP; } /* L: long double */
            else if (*fmt == 'z') { fmt++; lmod = LM_Z; } /* z: size_t */

            /* find format character */
            sn = FALSE; /* set positive */
            switch (*fmt) { /* handle each format */

                case 'd': /* signed decimal */
                case 'i': /* signed decimal alternate */
                case 'u': /* unsigned decimal */
                case 'o': /* octal */
                case 'x': /* hexadecimal, lower case */
                case 'X': /* hexadecimal, upper case */
                case 'p': /* pointer */
                    /* ISO 9899: if precision set, ignore '0' flag */
                    if (pres) zer = FALSE;
                    /* if not a signed type, kill sign controls */
                    if (*fmt != 'd' && *fmt != 'i') { sgn = FALSE; spc = FALSE; }
                    /* if not octal or hex, kill alternate format */
                    if (*fmt != 'o' && *fmt != 'x' && *fmt != 'X') alt = FALSE;
                    /* if pointer, we always use the alternate format */
                    if (*fmt == 'p') alt = TRUE;
                    /* set up radix and power */
                    if (*fmt == 'o') { /* octal */

                        r = 8; /* set radix */
                        if (!power8) power8 = toppow(8); /* find octal */
                        p = power8; /* set top power */

                    } else if (*fmt == 'x' || *fmt == 'X' || *fmt == 'p') {

                        /* hexadecimal */
                        r = 16; /* set radix */
                        if (!power16) power16 = toppow(16); /* hexadecimal */
                        p = power16; /* set top power */

                    } else { /* decimal */

                        r = 10; /* set radix */
                        if (!power10) power10 = toppow(10); /* decimal */
                        p = power10; /* set top power */

                    }
                    if (*fmt == 'd' || *fmt == 'i') { /* get signed */

                        /* fetch the signed argument at its true width */
                        switch (lmod) {

                            case LM_L:  li = va_arg(ap, long); break;
                            case LM_LL: li = va_arg(ap, long long); break;
                            case LM_Z:  li = va_arg(ap, size_t); break;
                            /* char and short are promoted to int when passed */
                            default:    li = va_arg(ap, int); break;

                        }
                        if (li < 0) { /* remove sign */

                            sn = TRUE; /* set sign */
                            u = -li; /* remove sign */

                        } else u = li; /* place unsigned */

                    } else if (*fmt == 'p') { /* pointer: fetch at pointer width */

                        u = (unsigned long long)(size_t)va_arg(ap, void *);

                    } else { /* get unsigned integer at its true width */

                        switch (lmod) {

                            case LM_L:  u = va_arg(ap, unsigned long); break;
                            case LM_LL: u = va_arg(ap, unsigned long long); break;
                            case LM_Z:  u = va_arg(ap, size_t); break;
                            default:    u = va_arg(ap, unsigned int); break;

                        }

                    }
                    dg = digits(r, u); /* find required digits */
                    /* ISO 9899: if '#' is specified with octal, we need to
                       increase the precision to add a leading '0', only if
                       there is no other option that does this already.
                       Note that there might be some conditions missing,
                       such as '0' option with a field larger than required. */
                    if (r == 8 && alt && u != 0 && pre < dg) pre = dg+1;
                    /* apply precision */
                    if (pre > dg) {

                        ndg = pre; /* set net digit count */
                        pe = pre-dg; /* set extra padding */

                    } else {

                        ndg = dg; /* set net digit count */
                        pe = 0; /* set no extra padding */

                    }
                    /* add required sign or space */
                    if (sgn || spc || sn) ndg++;
                    /* add required 0x/0X for hexadecimal */
                    if (alt && r == 16) ndg += 2;
                    /* no zero pad, if right justified, pad left to go right */
                    if (!lft && !zer) putchrs(&s, fld-ndg, ' ', &cnt, fd);
                    /* place sign or substitute for sign */
                    if (sn) putchrs(&s, 1, '-', &cnt, fd); /* place negative */
                    else if (sgn) putchrs(&s, 1, '+', &cnt, fd); /* place positive */
                    else if (spc) putchrs(&s, 1, ' ', &cnt, fd); /* place space */
                    /* output 0x or 0X prefix */
                    if (alt && r == 16) {

                        putchrs(&s, 1, '0', &cnt, fd);
                        putchrs(&s, 1, *fmt=='X'?'X':'x', &cnt, fd);

                    }
                    /* zero pad, if right justified, pad left to go right */
                    if (!lft && zer) putchrs(&s, fld-ndg, '0', &cnt, fd);
                    putchrs(&s, pe, '0', &cnt, fd); /* apply extra padding */
                    /* print number */
                    putnum(&s, u, r, p, *fmt == 'X', &cnt, fd);
                    /* if left justified, pad right to go left */
                    if (lft) putchrs(&s, fld-ndg, ' ', &cnt, fd);
                    fmt++; /* next character */
                    break;
                case 'c':
                    i = va_arg(ap, int); /* get signed integer */
                    /* if right justified, pad left to go right */
                    if (!lft) putchrs(&s, fld-1, ' ', &cnt, fd);
                    putchrs(&s, 1, i, &cnt, fd);
                    /* if left justified, pad right to go left */
                    if (lft) putchrs(&s, fld-1, ' ', &cnt, fd);
                    fmt++; /* next character */
                    break;
                case 's':
                    cp = va_arg(ap, char *); /* get character pointer */
                    if (cp) { /* not null */

                        if (pres) { /* precision was specified */

                            l = pre; /* set length from precision */
                            for (i = 0; cp[i] && i < l; i++); /* find true length */
                            if (!cp[i]) l = i; /* if shorter, use that */

                        } else l = strlen(cp); /* find length of string */
                        /* if right justified, pad left to go right */
                        if (!lft) putchrs(&s, fld-l, ' ', &cnt, fd);
                        /* output characters of string */
                        for (i = 0; i < l; i++) putchrs(&s, 1, *cp++, &cnt, fd);
                        /* if left justified, pad right to go left */
                        if (lft) putchrs(&s, fld-l, ' ', &cnt, fd);

                    } else { /* null */

                        l = strlen(nulmsg); /* find length of null message */
                        /* output null message */
                        for (i = 0; i < l; i++)
                            putchrs(&s, 1, nulmsg[i], &cnt, fd);

                    }
                    fmt++; /* next character */
                    break;
                case 'f':
                case 'e':
                case 'E':
                case 'g':
                case 'G': { /* floating point convertions */

                    char *fs; /* formatted floating point string */
                    int   fl; /* formatted length */

                    d = va_arg(ap, double); /* get value */
                    /* convert the number to a string */
                    fs = fmtfloat(d, pre, pres, *fmt, sgn, spc, alt);
                    if (fs) { /* conversion succeeded */

                        fl = strlen(fs); /* find length */
                        if (!lft) { /* right justify, pad on the left */

                            if (zer && (fs[0] == '-' || fs[0] == '+' ||
                                        fs[0] == ' ')) {

                                /* keep the sign ahead of the zero padding */
                                putchrs(&s, 1, fs[0], &cnt, fd);
                                putchrs(&s, fld-fl, '0', &cnt, fd);
                                for (i = 1; i < fl; i++)
                                    putchrs(&s, 1, fs[i], &cnt, fd);

                            } else { /* pad with zeros or spaces */

                                putchrs(&s, fld-fl, zer?'0':' ', &cnt, fd);
                                for (i = 0; i < fl; i++)
                                    putchrs(&s, 1, fs[i], &cnt, fd);

                            }

                        } else { /* left justify, pad on the right */

                            for (i = 0; i < fl; i++)
                                putchrs(&s, 1, fs[i], &cnt, fd);
                            putchrs(&s, fld-fl, ' ', &cnt, fd);

                        }
                        free(fs); /* release the string */

                    }
                    fmt++; /* next character */
                    break;

                }
                case 'n':
                    /* store the output count through a pointer of the width
                       given by the length modifier */
                    switch (lmod) {

                        case LM_HH: *va_arg(ap, char *) = cnt; break;
                        case LM_H:  *va_arg(ap, short *) = cnt; break;
                        case LM_L:  *va_arg(ap, long *) = cnt; break;
                        case LM_LL: *va_arg(ap, long long *) = cnt; break;
                        case LM_Z:  *va_arg(ap, size_t *) = cnt; break;
                        default:    *va_arg(ap, int *) = cnt; break;

                    }
                    fmt++; /* next character */
                    break;
                case '%':
                    putchrs(&s, 1, *fmt, &cnt, fd); /* place '%' character */
                    fmt++; /* next character */
                    break;

            }

        } else { /* standard character */

            putchrs(&s, 1, *fmt, &cnt, fd); /* place character */
            fmt++; /* next character */

        }

    }
    if (s) *s = 0; /* terminate string */

    return cnt; /* return output count */

}

/** **************************************************************************

Function vsprintf

Description

Print to string with variable argument list pointer.

Prints formatted to a string. See vsprintfe. The destination string, the
format string, and the argument list pointer are provided.

\returns

The number of characters placed in the string.

******************************************************************************/

int vsprintf(
    /** destination string */     char *s,
    /** format string */          const char *fmt,
    /** variable argument list */ va_list ap
)

{

    return (vsprintfe(s, fmt, ap, (FILE *)NULL)); /* process format to string */

}

/** **************************************************************************

Function sprintf

Description

print formatted to string.

Prints formatted to a string. See vsprintfe. The destination string, the
format string, and the argument list are provided.

\returns

The number of characters placed in the string.

******************************************************************************/

int sprintf(
    /** destination string */ char *s,
    /** format string */      const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vsprintf(s, fmt, ap); /* process format to string */
    va_end(ap); /* close argument list */

    return (r);

}

/** **************************************************************************

Function vsnprintf

Description

Print to a bounded string with variable argument list pointer.

Prints formatted to a string, writing no more than n characters including the
terminating null. If the output is longer than n-1 characters it is truncated,
but the string is always terminated (unless n is zero, in which case nothing is
written). Returns the number of characters that would have been written had n
been large enough, not counting the terminating null. See vsprintfe.

This is implemented in two passes over the format: the first measures the full
length (writing nothing), and the second formats the full result into a
temporary buffer that is then copied, bounded, into the caller's buffer. This
reuses the unbounded formatting engine without it needing to know about limits.

\returns

Returns the number of characters that would have been written had n been large
enough, not counting the terminating null.

******************************************************************************/

int vsnprintf(
    /** destination string */                        char *s,
    /** maximum bytes to write including the null */ size_t n,
    /** format string */                             const char *fmt,
    /** variable argument list */                    va_list ap
)

{

    va_list ap2;  /* copy of the argument list for the measuring pass */
    int     len;  /* full formatted length */
    char    *tmp; /* temporary holding the full output */
    size_t  cpy;  /* number of characters to copy */

    /* pass 1: measure the full length, writing nothing */
    va_copy(ap2, ap);
    len = vsprintfe((char *)NULL, fmt, ap2, (FILE *)NULL);
    va_end(ap2);
    if (len < 0) return (len); /* formatting error */
    if (n == 0) return (len); /* nothing may be written */

    /* pass 2: format the full output into a temporary, then copy bounded */
    tmp = malloc(len+1); /* room for the full result plus null */
    if (!tmp) { s[0] = 0; return (len); } /* out of memory, empty result */
    vsprintfe(tmp, fmt, ap, (FILE *)NULL); /* format the full output */
    cpy = (size_t)len < n-1 ? (size_t)len : n-1; /* at most n-1 characters */
    memcpy(s, tmp, cpy); /* copy the possibly truncated text */
    s[cpy] = 0; /* always terminate */
    free(tmp); /* release the temporary */

    return (len); /* return the full length, per ISO 9899 */

}

/** **************************************************************************

Function snprintf

Description

print formatted to a bounded string.

Prints formatted to a string with a length bound. See vsnprintf. The destination
string, the size bound, the format string, and the argument list are provided.

\returns

The number of characters that would have been written, not counting the
terminating null.

******************************************************************************/

int snprintf(
    /** destination string */                        char *s,
    /** maximum bytes to write including the null */ size_t n,
    /** format string */                             const char *fmt,
    /** variable arguments */                        ...
)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vsnprintf(s, n, fmt, ap); /* process format to bounded string */
    va_end(ap); /* close argument list */

    return (r);

}

/** **************************************************************************

Function vfprintf

Description

Print formatted to file with variable argument list pointer.

Prints formatted to a file. See vsprintfe. The destination string, the
format string, and the argument list pointer are provided.

\returns

The number of characters written to the file.

******************************************************************************/

int vfprintf(
    /** destination file */       FILE *stream,
    /** format string */          const char *fmt,
    /** variable argument list */ va_list ap
)

{

    va_list ap2; /* copy of the argument list for the measuring pass */
    int     len; /* full formatted length */
    char    *tmp; /* temporary holding the full output */

    /* An unbuffered stream would take a write for every character. The call's
       output is formatted whole and written once instead, which is what a
       buffered stream carries down and what the other C libraries do, so that
       whoever sits under the descriptor, a print module or the remote client
       say, sees the call's text together. */
    if (stream && (stream->_flags & _IO_UNBUFFERED)) {

        va_copy(ap2, ap);
        len = vsprintfe((char *)NULL, fmt, ap2, (FILE *)NULL); /* measure */
        va_end(ap2);
        if (len <= 0) return (len); /* nothing to write, or a format error */
        tmp = malloc(len+1); /* room for the full result plus null */
        if (tmp) {

            vsprintfe(tmp, fmt, ap, (FILE *)NULL); /* format the full output */
            if (fwrite(tmp, 1, len, stream) != (size_t)len) len = -1;
            free(tmp);

            return (len);

        }
        /* out of memory: the character path below still serves */

    }

    return vsprintfe((char *)NULL, fmt, ap, stream); /* process */

}

/** **************************************************************************

Function fprintf

Description

Print formatted to file.

Prints formatted to a file. See vsprintfe. The destination string, the
format string, and the argument list are provided.

\returns

The number of characters written to the file.

******************************************************************************/

int fprintf(
    /** destination file */   FILE *stream,
    /** format string */      const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vfprintf(stream, fmt, ap);
    va_end(ap); /* close argument list */

    return (r);

}

/** **************************************************************************

Function vprintf

Description

Print formatted to stdout with variable argument list pointer.

Prints formatted to the standard output file. See vsprintfe. The destination
string, the format string, and the argument list pointer are provided.

\returns

The number of characters written to standard output.

******************************************************************************/

int vprintf(
    /** format string */          const char *fmt,
    /** variable argument list */ va_list ap
)

{

    return (vfprintf(stdout, fmt, ap)); /* print */

}

/** **************************************************************************

Function printf

Description

Print formatted to file.

Prints formatted to a file. See vsprintfe. The destination string, the
format string, and the argument list are provided.

\returns

The number of characters written to standard output.

******************************************************************************/

int printf(
    /** format string */      const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vprintf(fmt, ap); /* print */
    va_end(ap); /* close argument list */

    return (r);

}

/** **************************************************************************

Function vsscanfe

Description

DETAILED DESCRIPTION:.

BUGS/ISSUES:

\returns

The number of items matched and assigned, or EOF on input failure before any
conversion.

******************************************************************************/

int vsscanfe(
    /** input string, or NULL to read from the file */ const char *s,
    /** format string */                               const char *fmt,
    /** variable argument list */                      va_list ap,
    /** input file when the string is NULL */          FILE *fd
)

{

    int      ccnt;   /* number of characters processed */
    int      pcnt;   /* number of parameters processed */
    int      fld;    /* minimum field width */
    int      flds;   /* minimum field width was set */
    int      sup;    /* suppress output */
    int      lmod;   /* length modifier code (LM_*) */
    long long li;    /* signed wide integer holding */
    unsigned long long lu; /* unsigned wide integer holding */
    int      r;      /* radix */
    char     *cp;    /* character pointer holder */
    char     cflags[UCHAR_MAX+1]; /* character include/disinclude array */
    int      cneg;   /* character match negate flag */
    int      err;    /* numeric error flag */
    char     lc;     /* last and next match characters */
    int      lcs;    /* last character was set */
    int      c;
    int      x;

    pcnt = 0; /* clear parameter count */
    ccnt = 0; /* clear character count */
    while (*fmt) { /* while format characters remain */

        /* Note: end of input is detected within each conversion via getfstr/
           chkfstr returning EOF. We must not test the string pointer here,
           because in file mode the string pointer is legitimately NULL. */
        if (*fmt == '%') { /* format specification */

            fmt++; /* skip '%' */

            /* clear convertion flags and fields */
            sup  = FALSE;   /* set no supress */
            fld  = INT_MAX; /* set minimum input field */
            flds = FALSE;   /* field width not set */
            lmod = LM_NONE; /* no length modifier */
            cneg = FALSE;   /* no character match negation */

            /* check for output suppress */
            if (*fmt == '*') {

                fmt++; /* skip '*' */
                sup = TRUE; /* set supress */

            }

            /* check for field */
            if (*fmt >= '0' && *fmt <= '9') {

                getnum(&fmt, &fld); /* get field */
                flds = TRUE; /* set field was set */

            }

            /* check for length modifiers */
            if (*fmt == 'h') { /* short or char */

                fmt++; /* skip 'h' */
                if (*fmt == 'h') { fmt++; lmod = LM_HH; } /* hh: char */
                else lmod = LM_H; /* h: short */

            } else if (*fmt == 'l') { /* long or long long */

                fmt++; /* skip 'l' */
                if (*fmt == 'l') { fmt++; lmod = LM_LL; } /* ll: long long */
                else lmod = LM_L; /* l: long */

            } else if (*fmt == 'L') { fmt++; lmod = LM_CAP; } /* L: long double */
            else if (*fmt == 'z') { fmt++; lmod = LM_Z; } /* z: size_t */

            /* find format character */
            switch (*fmt) { /* handle each format */

                case 'i': /* open radix */
                case 'd': /* signed decimal */
                    if (*fmt == 'i') r = 0; /* open radix */
                    else r = 10; /* decimal radix */
                    li = strtoli(&s, r, &ccnt, &err, fld, fd); /* parse number */
                    if (err) return (pcnt); /* error */
                    if (!sup) { /* if supress isn't on, place result */

                        /* store through a pointer of the modifier's width */
                        switch (lmod) {

                            case LM_HH: *va_arg(ap, char *) = (char)li; break;
                            case LM_H:  *va_arg(ap, short *) = (short)li; break;
                            case LM_L:  *va_arg(ap, long *) = (long)li; break;
                            case LM_LL: *va_arg(ap, long long *) = li; break;
                            case LM_Z:  *va_arg(ap, size_t *) = (size_t)li; break;
                            default:    *va_arg(ap, int *) = (int)li; break;

                        }
                        pcnt++; /* count items parsed */

                    }
                    fmt++; /* next character */
                    break;
                case 'x': /* hexadecimal */
                case 'p': /* pointer */
                case 'o': /* octal */
                case 'u': /* unsigned decimal */
                    switch (*fmt) {

                        case 'x':                /* hexadecimal */
                        case 'p': r = 16; break; /* pointer */
                        case 'o': r = 8;  break; /* octal */
                        case 'u': r = 10; break; /* unsigned decimal */

                    }
                    lu = strtouli(&s, r, &ccnt, &err, fld, fd); /* parse number */
                    if (err) return (pcnt); /* error */
                    if (!sup) { /* if supress isn't on, place result */

                        if (*fmt == 'p') /* pointer target */
                            *va_arg(ap, void **) = (void *)(size_t)lu;
                        else switch (lmod) { /* width given by the modifier */

                            case LM_HH:
                                *va_arg(ap, unsigned char *) = (unsigned char)lu;
                                break;
                            case LM_H:
                                *va_arg(ap, unsigned short *) = (unsigned short)lu;
                                break;
                            case LM_L:
                                *va_arg(ap, unsigned long *) = (unsigned long)lu;
                                break;
                            case LM_LL:
                                *va_arg(ap, unsigned long long *) = lu;
                                break;
                            case LM_Z:
                                *va_arg(ap, size_t *) = (size_t)lu;
                                break;
                            default:
                                *va_arg(ap, unsigned *) = (unsigned)lu;
                                break;

                        }
                        pcnt++; /* count items parsed */

                    }
                    fmt++; /* next character */
                    break;
                case 'c':
                    if (!flds) fld = 1; /* set default field for character */
                    /* get the result pointer */
                    if (!sup) cp = va_arg(ap, char *);
                    for (x = 0; x < fld; x++) { /* gather characters */

                        c = getfstr(&s, fd); /* get next character */
                        /* check end or error */
                        if (c == EOF) return (EOF); /* return if so */
                        if (!sup) *cp++ = c; /* copy character to output */
                        ccnt++; /* count characters */

                    }
                    if (!sup) pcnt++; /* count items parsed */
                    fmt++; /* next character */
                    break;
                case 's':
                    /* get the result pointer */
                    if (!sup) cp = va_arg(ap, char *);
                    /* skip leading blanks */
                    while (isspace(chkfstr(s, fd))) {

                        c = getfstr(&s, fd); /* get next character */
                        ccnt++; /* count characters */

                    }
                    for (x = 0; x < fld && !isspace(chkfstr(s, fd)) &&
                                chkfstr(s, fd) != EOF; x++) {

                        /* gather characters until next blank */
                        c = getfstr(&s, fd); /* get next character */
                        /* check end or error */
                        if (c == EOF) return (EOF); /* return if so */
                        if (!sup) *cp++ = c; /* copy character to output */
                        ccnt++; /* count characters */

                    }
                    if (!sup) { /* not suppress */

                        *cp = 0; /* terminate string */
                        pcnt++; /* count items parsed */

                    }
                    fmt++; /* next character */
                    break;
                case 'f':
                case 'e':
                case 'E':
                case 'g':
                case 'G': { /* floating point convertions */

                    double dv; /* parsed value */

                    dv = strtodi(&s, &ccnt, &err, fld, fd); /* parse number */
                    if (err) return (pcnt); /* error */
                    if (!sup) { /* if supress isn't on, place result */

                        /* 'l' or 'L' selects double, otherwise float */
                        if (lmod == LM_L || lmod == LM_CAP) {

                            double *dp = va_arg(ap, double *); /* get parameter */
                            *dp = dv; /* place result */

                        } else {

                            float *fp = va_arg(ap, float *); /* get parameter */
                            *fp = (float)dv; /* place result */

                        }
                        pcnt++; /* count items parsed */

                    }
                    fmt++; /* next character */
                    break;

                }
                case 'n':
                    if (!sup) { /* if supress isn't on, place result */

                        /* store the count at the right width */
                        switch (lmod) {

                            case LM_HH: *va_arg(ap, char *) = ccnt; break;
                            case LM_H:  *va_arg(ap, short *) = ccnt; break;
                            case LM_L:  *va_arg(ap, long *) = ccnt; break;
                            case LM_LL: *va_arg(ap, long long *) = ccnt; break;
                            case LM_Z:  *va_arg(ap, size_t *) = ccnt; break;
                            default:    *va_arg(ap, int *) = ccnt; break;

                        }

                    }
                    fmt++; /* next character */
                    break;
                case '[': /* match specification */
                    fmt++; /* skip '[' */
                    lcs = FALSE; /* set no last character */
                    if (*fmt == '^') {

                        cneg = TRUE; /* set match negate */
                        fmt++; /* next character */

                    }
                    /* clear character flags array */
                    for (x = 0; x <= UCHAR_MAX; x++) cflags[x] = 0;
                    /* allow first ']' to be included in match set */
                    if (*fmt == ']') {

                        cflags[*fmt] = TRUE; /* set match this character */
                        fmt++; /* next character */

                    }
                    while (*fmt != ']') { /* gather match characters */

                        if (!*fmt) return (EOF); /* end of format, error */
                        if (*fmt == '-') { /* it's a range */

                            if (!lcs) return (EOF); /* no last character */
                            fmt++; /* next character */
                            if (!*fmt) return (EOF); /* end of format, error */
                            /* set characters in range */
                            for (x = lc; x <= *fmt; x++) cflags[x] = TRUE;
                            lcs = FALSE; /* no last character now */
                            fmt++; /* next character */

                        } else { /* normal */

                           lc = *fmt; /* save last character */
                           lcs = TRUE; /* set last character set */
                           cflags[*fmt] = TRUE; /* set match this character */
                           fmt++; /* next character */

                        }

                    }
                    fmt++; /* skip ']' */
                    /* get the result pointer */
                    if (!sup) cp = va_arg(ap, char *);
                    for (x = 0; x < fld && (cneg != cflags[chkfstr(s, fd)]) &&
                                chkfstr(s, fd) != EOF; x++) {

                        /* gather characters that match */
                        c = getfstr(&s, fd); /* get next character */
                        /* check end or error */
                        if (c == EOF) return (EOF); /* return if so */
                        if (!sup) *cp++ = c; /* copy character to output */
                        ccnt++; /* count characters */

                    }
                    if (!x) return (pcnt); /* error (nothing processed) */
                    if (!sup) { /* not suppress */

                        *cp = 0; /* terminate string */
                        pcnt++; /* count items parsed */

                    }
                    break;
                case '%':
                    /* skip any spaces */
                    while ((c = isspace(chkfstr(s, fd)))) {

                        getfstr(&s, fd); /* get next character */
                        ccnt++; /* count characters */

                    }
                    if (chkfstr(s, fd) != '%') return (pcnt); /* no match, exit */
                    getfstr(&s, fd); /* skip '%' character */
                    ccnt++; /* count characters */
                    if (!sup) pcnt++; /* count items parsed */
                    fmt++; /* next format character */
                    break;

            }

        } else { /* standard character */

            if (isspace(*fmt)) { /* directive is a whitespace character */

                /* skip any spaces */
                while (isspace(chkfstr(s, fd))) {

                    getfstr(&s, fd); /* get next character */
                    ccnt++; /* count characters */

                }
                fmt++; /* next format character */

            } else { /* match ordinary character */

                if (chkfstr(s, fd) != *fmt) return (EOF); /* no match, exit */
                getfstr(&s, fd); /* skip matched character */
                ccnt++; /* count characters */
                fmt++; /* next format character */

            }

        }

    }

    return pcnt; /* return output count */

}

/** **************************************************************************

Function vsscanf

Description

Scan from string with variable argument list pointer.

Scans formatted input from a string using an argument list pointer. Returns the
number of items successfully matched and assigned, or EOF on input failure
before any conversion. See vsscanfe.

\returns

Returns the number of items successfully matched and assigned, or EOF on input
failure before any conversion.

******************************************************************************/

int vsscanf(
    /** input string */           const char *s,
    /** format string */          const char *fmt,
    /** variable argument list */ va_list ap
)

{

    /* process format from string */
    return (vsscanfe(s, fmt, ap, (FILE *)NULL));

}

/** **************************************************************************

Function vfscanf

Description

Scan from file with variable argument list pointer.

Scans formatted input from a file using an argument list pointer. Returns the
number of items successfully matched and assigned, or EOF on input failure
before any conversion. See vsscanfe.

\returns

Returns the number of items successfully matched and assigned, or EOF on input
failure before any conversion.

******************************************************************************/

int vfscanf(
    /** input file */             FILE *stream,
    /** format string */          const char *fmt,
    /** variable argument list */ va_list ap
)

{

    /* process format from file */
    return (vsscanfe((char *)NULL, fmt, ap, stream));

}

/** **************************************************************************

Function vscanf

Description

Scan from standard input with variable argument list pointer.

Scans formatted input from standard input using an argument list pointer.
Returns the number of items successfully matched and assigned, or EOF on input
failure before any conversion. See vsscanfe.

\returns

Returns the number of items successfully matched and assigned, or EOF on input
failure before any conversion.

******************************************************************************/

int vscanf(
    /** format string */          const char *fmt,
    /** variable argument list */ va_list ap
)

{

    /* process format from standard input */
    return (vsscanfe((char *)NULL, fmt, ap, stdin));

}

/** **************************************************************************

Function sscanf

Description

DETAILED DESCRIPTION:.

BUGS/ISSUES:

\returns

The number of items matched and assigned, or EOF on input failure before any
conversion.

******************************************************************************/

int sscanf(
    /** input string */       const char *s,
    /** format string */      const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vsscanf(s, fmt, ap);
    va_end(ap); /* close argument list */

    return (r);

}

/** **************************************************************************

Function fscanf

Description

DETAILED DESCRIPTION:.

BUGS/ISSUES:

\returns

The number of items matched and assigned, or EOF on input failure before any
conversion.

******************************************************************************/

int fscanf(
    /** input file */         FILE *stream,
    /** format string */      const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vfscanf(stream, fmt, ap);
    va_end(ap); /* close argument list */

    return (r);

}

/** **************************************************************************

Function scanf

Description

DETAILED DESCRIPTION:.

BUGS/ISSUES:

\returns

The number of items matched and assigned, or EOF on input failure before any
conversion.

******************************************************************************/

int scanf(
    /** format string */      const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vfscanf(stdin, fmt, ap);
    va_end(ap); /* close argument list */

    return (r);

}

/** **************************************************************************

Function fgetc

Description

Gets a single character from a stream.

The next character in a stream is returned, or EOF if end of file or error is
encountered. In case of error, errno must be used to determine the exact
type of error.

\returns

The next character, or EOF at end of file or on error.

******************************************************************************/

int fgetc(
    /** file to read */ FILE *stream
)

{

    if (!stream || stream->_fileno < 0) return EOF; /* check stream is open */

    return (getbuf(stream)); /* read through the buffer (serves pushback too) */

}

/** **************************************************************************

Function fgets

Description

Get line from file with limit.

Gets a line of text from the text file, with limit. Characters are read from the
input file to the array until the limit is reached, or a newline is encountered,
or EOF is encountered. If a newline is encountered, it is read and discarded.
Note that EOF can also signify an error.

\returns

The string buffer, or NULL at end of file with no characters read.

******************************************************************************/

char *fgets(
    /** destination buffer */                    char *s,
    /** maximum characters including the null */ int n,
    /** file to read */                          FILE *stream
)

{

    int c; /* input character holder */
    char *s1; /* input array holder */
    int cc;   /* character count */

    if (!stream || stream->_fileno < 0) return (NULL);
    s1 = s; /* save array to return */
    cc = 0; /* clear character count */
    do { /* read characters */

        c = fgetc(stream); /* get next character */
        if (c == EOF) { /* end of file */

            *s = '\0'; /* for neatness, we terminate the string */
            if (cc) return (s1); /* characters were read */
            else return NULL; /* no characters read, return null string */

        }
        *s = c; /* place character */
        s++; /* next character */
        cc++; /* count characters */

    } while (c != '\n' && n--); /* not newline, and not past limit */
    *s = '\0';

    return (s1); /* exit with input array */

}

/** **************************************************************************

Function fputc

Description

Output character to file.

Outputs a single character to a file. 

\returns

Returns EOF if there is an error.

******************************************************************************/

int fputc(
    /** character to write */ int c,
    /** file to write */      FILE *stream
)

{

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (EOF);

    return (putbuf(stream, (unsigned char)c)); /* write through the buffer */

}

/** **************************************************************************

Function fputs

Description

Output string to file.

Outputs a string to a file. Note that a newline is NOT automatically output for
the string.

The string goes down whole, as one fwrite, and not a character at a time. On an
unbuffered stream a character at a time is a write for every character, and
whoever sits under the descriptor, a print module or the remote client say,
would see the string in pieces; fwrite carries it together, as vfprintf does
and as the other C libraries do. The compiler folds fprintf(f, "%s", s) into
fputs, so that call arrives here.

\returns

Zero on success, or EOF on error.

******************************************************************************/

int fputs(
    /** string to write */ const char *s,
    /** file to write */   FILE *stream
)

{

    size_t l; /* string length */

    l = strlen(s); /* find length of string */
    if (!l) return (0); /* nothing to write */
    if (fwrite(s, 1, l, stream) != l) return (EOF); /* write whole */

    return (0); /* return no error */

}

/** **************************************************************************

Function getc

Description

Gets a single character from a stream.

The next character in a stream is returned, or EOF if end of file or error is
encountered. In case of error, errno must be used to determine the exact
type of error.

This routine is identical to fgetc. It isn't a macro, as most modern compilers
have automatic inlining of functions.

\returns

The next character, or EOF at end of file or on error.

******************************************************************************/

#ifndef USEMACRO

int getc(
    /** file to read */ FILE *stream
)

{

    return fgetc(stream); /* process get */

}

#endif

/** **************************************************************************

Function getchar

Description

Get single character from standard input.

Gets a single character from the standard input. Equivalent to getc(stdin).
It isn't a macro, because most modern compilers can perform automatic inlining.

\returns

The next character from standard input, or EOF at end of file or on error.

******************************************************************************/

int getchar(void)

{

    return getc(stdin); /* get from standard input */

}

/** **************************************************************************

Function gets

Description

Get input line from standard input.

Gets a full line of text from the standard input into the given array. The
terminating newline is read and discarded. If EOF is encountered in the file,
the line is terminated immediately. Note that EOF can also signify an error.

\returns

The string buffer, or NULL at end of file with no characters read.

******************************************************************************/

char *gets(
    /** destination buffer */ char *s
)

{

    int c; /* input character holder */
    char *s1; /* input array holder */

    s1 = s; /* save array to return */
    do { /* read characters */

        c = fgetc(stdin); /* get next character */
        if (c == EOF) { /* end of file */

            *s = '\0'; /* for neatness, we terminate the string */
            return NULL; /* eof, return null string */

        }
        *s = c; /* place character */
        s++; /* next character */

    } while (c != '\n'); /* not newline */
    *s = '\0';

    return (s1); /* exit with input array */

}

/** **************************************************************************

Function putc

Description

Put a single character to a file.

Puts a single character to the given file. It isn't a macro, because most modern
compilers can perform automatic inlining.

\returns

The character written, or EOF on error.

******************************************************************************/

int putc(
    /** character to write */ int c,
    /** file to write */      FILE *stream
)

{

    return fputc(c, stream); /* process put */

}

/** **************************************************************************

Function putchar

Description

Output character to standard output.

Outputs a single character to the standard output. It isn't a macro, because
most modern compilers can perform automatic inlining.

\returns

The character written, or EOF on error.

******************************************************************************/

int putchar(
    /** character to write */ int c
)

{

    return putc(c, stdout); /* put standard output character */

}

/** **************************************************************************

Function puts

Description

Output string to standard output.

Outputs a string to the standard output.

\returns

A non-negative value on success, or EOF on error.

******************************************************************************/

int puts(
    /** string to write */ const char *s
)

{

    int rc; /* return code */

    rc = fputs(s, stdout); /* output string */
    if (rc) return EOF; /* exit with error */
    return fputs("\n", stdout); /* output end of line */

}

/** **************************************************************************

Function ungetc

Description

Put character back to input.

Places a single character into the putback buffer for the input file.

\returns

The character pushed back, or EOF if it could not be pushed back.

******************************************************************************/

int ungetc(
    /** character to push back */ int c,
    /** file to push back to */   FILE *stream
)

{

    char *cell; /* one byte holding area */

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (EOF);
    if (c == EOF) return (EOF); /* cannot push back EOF */

    /* if currently writing, flush and switch to reading first */
    if (stream->_flags & _IO_CURRENTLY_PUTTING) {

        if (wflush(stream)) return (EOF);
        stream->_flags &= ~_IO_CURRENTLY_PUTTING;

    }

    if (stream->_IO_read_ptr && stream->_IO_read_ptr > stream->_IO_read_base) {

        /* there is room in the get area, back the read pointer up in place */
        stream->_IO_read_ptr--;
        *stream->_IO_read_ptr = (char)c;

    } else {

        /* empty get area: place the character at the buffer base, or in the one
           byte shortbuf when the stream is unbuffered */
        cell = stream->_IO_buf_base ? stream->_IO_buf_base : stream->_shortbuf;
        cell[0] = (char)c;
        stream->_IO_read_base = cell; /* get area is the single cell */
        stream->_IO_read_ptr = cell;
        stream->_IO_read_end = cell+1;

    }
    stream->_flags &= ~_IO_EOF_SEEN; /* pushing a character back clears EOF */

    return ((unsigned char)c);

}

/** **************************************************************************

Function fread

Description

Read direct from file.

Reads a number of objects from the given file. The number and size of the
objects are specified.

\returns

The number of complete objects read.

******************************************************************************/

size_t fread(
    /** destination buffer */  void *ptr,
    /** size of each object */ size_t size,
    /** number of objects */   size_t nobj,
    /** file to read */        FILE *stream
)

{

    size_t total; /* total bytes requested */
    size_t got;   /* total bytes obtained */
    unsigned char *p; /* output pointer */
    int    avail; /* bytes available in buffer */
    int    take;  /* bytes to copy this pass */
    int    n;     /* read count */

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (0);
    if (size == 0 || nobj == 0) return (0); /* nothing to read */
    if (stream == stdin) fflush(stdout); /* show prompts before blocking */
    total = size*nobj; /* total bytes wanted */
    got = 0; /* nothing obtained yet */
    p = (unsigned char *)ptr; /* set output pointer */

    /* if we were writing, flush and switch to reading */
    if (stream->_flags & _IO_CURRENTLY_PUTTING) {

        if (wflush(stream)) return (0);
        stream->_flags &= ~_IO_CURRENTLY_PUTTING;

    }

    /* set up the buffer on first use if buffering is enabled */
    if (!stream->_IO_buf_base && !(stream->_flags & _IO_UNBUFFERED))
        bufalloc(stream);

    if (!stream->_IO_buf_base) { /* unbuffered: read directly, but serve any
                                    pushed back data from the get area first */

        while (got < total && stream->_IO_read_ptr < stream->_IO_read_end)
            *p++ = (unsigned char)*stream->_IO_read_ptr++, got++;
        if (got < total) {

            n = vread(stream->_fileno, p, total-got); /* bulk read */
            if (n > 0) got += n; /* count bytes read */
            else if (n == 0) stream->_flags |= _IO_EOF_SEEN; /* end of file */
            else stream->_flags |= _IO_ERR_SEEN; /* error */

        }
        return (got/size); /* return complete objects */

    }

    while (got < total) { /* until request satisfied */

        avail = stream->_IO_read_end-stream->_IO_read_ptr; /* unread in buffer */
        if (avail > 0) { /* copy what the get area holds */

            take = total-got; /* bytes still wanted */
            if (take > avail) take = avail; /* limit to what is available */
            memcpy(p, stream->_IO_read_ptr, take); /* copy out */
            stream->_IO_read_ptr += take; p += take; got += take; /* advance */

        } else if (total-got >= (size_t)iobufsize(stream)) {

            /* large remainder: read directly into the caller's buffer */
            n = vread(stream->_fileno, p, total-got); /* bulk read */
            if (n <= 0) { /* end of file or error */

                stream->_flags |= (n == 0) ? _IO_EOF_SEEN : _IO_ERR_SEEN;
                break;

            }
            p += n; got += n; /* advance */

        } else { /* small remainder: refill the buffer and copy from it */

            n = vread(stream->_fileno, stream->_IO_buf_base, iobufsize(stream));
            if (n <= 0) { /* end of file or error */

                stream->_flags |= (n == 0) ? _IO_EOF_SEEN : _IO_ERR_SEEN;
                stream->_IO_read_base = stream->_IO_read_ptr =
                    stream->_IO_read_end = stream->_IO_buf_base;
                break;

            }
            stream->_IO_read_base = stream->_IO_read_ptr = stream->_IO_buf_base;
            stream->_IO_read_end = stream->_IO_buf_base+n;
            stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_buf_base;

        }

    }

    /* return the count of complete objects read, not the byte count */
    return (got/size);

}

/** **************************************************************************

Function fwrite

Description

Write direct to file.

Writes a number of objects to the given file. The number and size of the objects
are specified.

\returns

The number of complete objects written.

******************************************************************************/

size_t fwrite(
    /** source buffer */       const void *ptr,
    /** size of each object */ size_t size,
    /** number of objects */   size_t nobj,
    /** file to write */       FILE *stream
)

{

    size_t total; /* total bytes to write */
    size_t done;  /* total bytes written */
    const unsigned char *p; /* input pointer */
    int    room;  /* space left in buffer */
    int    take;  /* bytes to copy this pass */
    int    n;     /* write count */

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (0);
    if (size == 0 || nobj == 0) return (0); /* nothing to write */
    total = size*nobj; /* total bytes wanted */
    done = 0; /* nothing written yet */
    p = (const unsigned char *)ptr; /* set input pointer */

    /* set up the buffer on first use if buffering is enabled */
    if (!stream->_IO_buf_base && !(stream->_flags & _IO_UNBUFFERED))
        bufalloc(stream);

    if (!stream->_IO_buf_base) { /* unbuffered: write directly */

        n = vwrite(stream->_fileno, p, total); /* bulk write */
        if (n <= 0) { stream->_flags |= _IO_ERR_SEEN; return (0); }
        return (n/size); /* return complete objects */

    }

    /* if we were reading, discard read ahead and switch to writing */
    if (!(stream->_flags & _IO_CURRENTLY_PUTTING)) {

        rflush(stream); /* discard read ahead, rewind the file */
        stream->_flags |= _IO_CURRENTLY_PUTTING; /* now writing */
        stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_buf_base;
        stream->_IO_write_end = stream->_IO_buf_end;

    }

    while (done < total) { /* until all written */

        room = stream->_IO_write_end-stream->_IO_write_ptr; /* space in put area */
        if (room == 0) { /* put area is full, flush it */

            if (wflush(stream)) break; /* write error */
            room = stream->_IO_write_end-stream->_IO_write_base; /* all free now */

        }
        take = total-done; /* bytes still to write */
        if (take > room) take = room; /* limit to put area space */
        memcpy(stream->_IO_write_ptr, p, take); /* copy into the put area */
        stream->_IO_write_ptr += take; p += take; done += take; /* advance */
        /* flush a full put area, or at a newline if line buffered */
        if (stream->_IO_write_ptr >= stream->_IO_write_end ||
            ((stream->_flags & _IO_LINE_BUF) &&
             memchr(stream->_IO_write_base, '\n',
                    stream->_IO_write_ptr-stream->_IO_write_base))) {

            if (wflush(stream)) break; /* write error */

        }

    }

    /* return the count of complete objects written */
    return (done/size);

}

/** **************************************************************************

Function fseek

Description

Seek to position in file.

Seeks to a given position in the indicated file. The seek modes, indicated by
the origin are:

SEEK_SET    Seek relative to beginning of file (0)
SEEK_CUR    Seek relative to current position in file
SEEK_END    Seek relative to end of file

The offset is a signed offset for the relative position in the file.

\returns

Return non-zero on error, otherwise zero.

******************************************************************************/

int fseek(
    /** file to position */                             FILE *stream,
    /** new offset */                                   long offset,
    /** seek origin (SEEK_SET, SEEK_CUR or SEEK_END) */ int origin
)

{

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (EOF);

    /* deal with the buffer before moving the file position */
    if (stream->_flags & _IO_CURRENTLY_PUTTING) { /* flush pending output */

        if (wflush(stream)) return (EOF); /* write error */
        stream->_flags &= ~_IO_CURRENTLY_PUTTING; /* back to idle */

    } else if (origin == SEEK_CUR) {

        /* a relative seek must account for the unconsumed read ahead, since the
           underlying file position is ahead of the logical position */
        offset -= stream->_IO_read_end-stream->_IO_read_ptr;

    }
    /* discard the get area and clear end of file */
    stream->_IO_read_base = stream->_IO_read_ptr = stream->_IO_read_end =
        stream->_IO_buf_base;
    stream->_flags &= ~_IO_EOF_SEEN; /* reset any EOF indication */

    /* process seek, returning zero on success and non-zero on error */
    return (vlseek(stream->_fileno, offset, origin) < 0);

}

/** **************************************************************************

Function ftell

Description

Give the current position in a file.

\returns

The current position in the file, or -1L on error.

******************************************************************************/

long ftell(
    /** file to query */ FILE *stream
)

{

    long pos; /* position holder */

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (0);

    if ((stream->_flags & _IO_IS_APPENDING) &&
        !(!(stream->_flags & _IO_CURRENTLY_PUTTING) &&
          stream->_IO_read_end > stream->_IO_read_ptr)) {

        /* An append stream writes at the end of the file whatever the
           descriptor's position says, and until its first write goes out
           that position is still 0 from the open. Asked from there, the
           mail store learned that every message began 55 bytes into its
           mailbox -- the length of the separator line -- and ran to the
           end of the file. The end of the file is where the pending
           output will land, so that is where the position is measured
           from -- unless the stream is in the middle of reading, which
           an "a+" stream may be, when the descriptor's position is the
           one that counts. */
        struct stat st;

        if (fstat(stream->_fileno, &st) < 0) return (-1);
        pos = st.st_size;

    } else {

        pos = vlseek(stream->_fileno, 0, SEEK_CUR); /* get underlying position */
        if (pos < 0) return (-1); /* seek error */

    }

    /* adjust for buffered data the underlying position does not reflect */
    if (stream->_flags & _IO_CURRENTLY_PUTTING)
        pos += stream->_IO_write_ptr-stream->_IO_write_base; /* pending output */
    else
        pos -= stream->_IO_read_end-stream->_IO_read_ptr; /* unconsumed read ahead */

    return (pos); /* return logical position */

}

/** **************************************************************************

Function rewind

Description

Rewind to file beginning.

Positions the given file to the start.

\returns

none

******************************************************************************/

void rewind(
    /** file to rewind */ FILE *stream
)

{

    (void) fseek(stream, 0l, SEEK_SET); /* perform seek */
    clearerr(stream); /* clear any error */

}

/** **************************************************************************

Function fgetpos

Description

Get (mark) file position.

Gets the current file position into a sized pointer. The current file position
is placed into the given pointer. 

\returns

Returns non-zero on error, otherwise zero.

******************************************************************************/

int fgetpos(
    /** file to query */                FILE *stream,
    /** returns the current position */ fpos_t *ptr
)

{

    *ptr = ftell(stream); /* get current position */

    return (!!(*ptr < 0));

}

/** **************************************************************************

Function fsetpos

Description

Set marked file position.

Sets the current file position from a save marker pointer. Returns non-zero
on error, otherwise zero.

\returns

Returns non-zero on error, otherwise zero.

******************************************************************************/

int fsetpos(
    /** file to position */    FILE *stream,
    /** position to restore */ const fpos_t *ptr
)

{

    return (fseek(stream, *ptr, SEEK_SET));

}

/** **************************************************************************

Function clearerr

Description

Clear errors on stream.

Clears any outstanding error indication on the given stream.

\returns

none

******************************************************************************/

void clearerr(
    /** file to clear */ FILE *stream
)

{

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return;
    /* clear only the end of file and error indicators, leaving the mode and
       buffering flags intact */
    stream->_flags &= ~(_IO_EOF_SEEN | _IO_ERR_SEEN);

}

/** **************************************************************************

Function feof

Description

Check end of file.

Checks if the end of file condition exists on a file. Returns non-zero if the
end of file is true for the stream.

\returns

Returns non-zero if the end of file is true for the stream.

******************************************************************************/

int feof(
    /** file to test */ FILE *stream
)

{

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (0);
    return (!!(stream->_flags &_EFEOF)); /* return EOF status */

}

/** **************************************************************************

Function ferror

Description

Check file has error.

Checks if the given stream is indicating an error. Returns non-zero if an
error is pending on the file, otherwise zero. The error indication is set when a
read or write through the buffering engine fails, and is cleared by clearerr().

\returns

Returns non-zero if an error is pending on the file, otherwise zero.

******************************************************************************/

int ferror(
    /** file to test */ FILE *stream
)

{

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) return (0);
    return (!!(stream->_flags & _IO_ERR_SEEN)); /* return error status */

}

/** **************************************************************************

Function perror

Description

Print error message.

Prints the given string as an error message, along with the error string for
the current errno value, on the standard error file.

\returns

none

******************************************************************************/

void perror(
    /** message prefix, or NULL for none */ const char *s
)

{

    fprintf(stderr, "%s: %s\n", s, strerror(errno));

}

/** **************************************************************************

Function fileno

Description

Return integer descriptor for file.

Checks if the file is open, and if so returns the integer file id. Otherwise
returns -1.

\returns

Return integer descriptor for file Checks if the file is open, and if so
returns the integer file id.

******************************************************************************/

int fileno(
    /** file to query */ FILE* stream
)

{

    int r;

    /* check file is allocated and open */
    if (!stream || stream->_fileno < 0) {

        r = -1;
        errno = EBADF;

    } else r = stream->_fileno;

    return (r);

}

/*******************************************************************************

stdio shutdown

Unlike the init, we don't have to worry about shutdown race conditions (or as
much). We set ourselves for last priority.

The shutdown task is to flush any buffered output so it is not lost on exit.
This runs in addition to the atexit flush registered when files are opened, and
covers programs that only ever write to the standard streams (and so never
trigger that registration). Temporary files created by tmpfile() clean
themselves up, since they are unlinked when opened.

*******************************************************************************/

static void deinit_stdio (void) __attribute__((destructor (101)));
static void deinit_stdio()

{

    fflush((FILE *)NULL); /* flush all buffered output streams */

}

/*******************************************************************************

Override-mode compatibility surface (Linux)

In override mode (no STDIO_BYPASS) this module's definitions preempt
glibc's for every caller in the process, foreign libraries included. Any
stdio entry point a foreign library imports that this module does not
define falls through to glibc, which then receives a petit ami FILE and
aborts on its vtable validation. This section closes the audited gaps
(tools/stdioaudit lists a binary's imports against what is provided):

- the fortified variants (__fprintf_chk and kin) that FILE-touching
  callers compiled with _FORTIFY_SOURCE import, forwarded to the plain
  implementations;
- the large-file aliases (fopen64 and kin), identities on a 64 bit
  system;
- the _unlocked family, aliases here since this stdio does not lock;
- the old glibc internal entry points (_IO_getc and kin) that binaries
  compiled against older glibc headers call;
- getline/getdelim and popen/pclose (libasound imports popen),
  implemented;
- loud failures for the FILE-creating entries that are not implemented
  (fmemopen and kin), so a gap diagnoses itself in one line instead of
  a crash inside glibc.

*******************************************************************************/

#if !defined(STDIO_BYPASS) && defined(__linux__)

#include <sys/wait.h>


/* wint_t/wchar_t without wchar.h, which would drag glibc's FILE in */
typedef unsigned int wint_t_;
typedef int wchar_t_;
#define wint_t wint_t_
#define wchar_t wchar_t_

/* minimal fopencookie type, matching glibc's shape for the stub */
typedef struct {

    void* read;
    void* write;
    void* seek;
    void* close;

} cookie_io_functions_t;

/* fortified print family: the checking is glibc's affair; the calls
   forward to the plain implementations */
int __fprintf_chk(FILE* f, int flag, const char* fmt, ...)

{

    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vfprintf(f, fmt, ap);
    va_end(ap);

    return (r);

}

int __vfprintf_chk(FILE* f, int flag, const char* fmt, va_list ap)

{

    return (vfprintf(f, fmt, ap));

}

int __printf_chk(int flag, const char* fmt, ...)

{

    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vfprintf(stdout, fmt, ap);
    va_end(ap);

    return (r);

}

int __vprintf_chk(int flag, const char* fmt, va_list ap)

{

    return (vfprintf(stdout, fmt, ap));

}

char *__fgets_chk(char* s, size_t size, int n, FILE* f)

{

    if (size != (size_t)-1 && (size_t)n > size) n = size;

    return (fgets(s, n, f));

}

size_t __fread_chk(void* p, size_t plen, size_t size, size_t n, FILE* f)

{

    return (fread(p, size, n, f));

}

size_t __fread_unlocked_chk(void* p, size_t plen, size_t size, size_t n,
                            FILE* f)

{

    return (fread(p, size, n, f));

}

/* large file aliases: off_t is 64 bits on a 64 bit system, so these are
   the plain calls under the names LFS-compiled callers import */
FILE *fopen64(const char* fn, const char* mode) { return (fopen(fn, mode)); }
FILE *freopen64(const char* fn, const char* mode, FILE* f)
    { return (freopen(fn, mode, f)); }
FILE *tmpfile64(void) { return (tmpfile()); }
int fseeko(FILE* f, long off, int whence) { return (fseek(f, off, whence)); }
long ftello(FILE* f) { return (ftell(f)); }
int fseeko64(FILE* f, long off, int whence) { return (fseek(f, off, whence)); }
long ftello64(FILE* f) { return (ftell(f)); }

/* the unlocked family: this stdio does not lock, so they are the plain
   calls */
int getc_unlocked(FILE* f) { return (fgetc(f)); }
int fgetc_unlocked(FILE* f) { return (fgetc(f)); }
int putc_unlocked(int c, FILE* f) { return (fputc(c, f)); }
int fputc_unlocked(int c, FILE* f) { return (fputc(c, f)); }
char *fgets_unlocked(char* s, int n, FILE* f) { return (fgets(s, n, f)); }
int fputs_unlocked(const char* s, FILE* f) { return (fputs(s, f)); }
size_t fread_unlocked(void* p, size_t size, size_t n, FILE* f)
    { return (fread(p, size, n, f)); }
size_t fwrite_unlocked(const void* p, size_t size, size_t n, FILE* f)
    { return (fwrite(p, size, n, f)); }
int feof_unlocked(FILE* f) { return (feof(f)); }
int ferror_unlocked(FILE* f) { return (ferror(f)); }
void clearerr_unlocked(FILE* f) { clearerr(f); }
int fileno_unlocked(FILE* f) { return (fileno(f)); }
int fflush_unlocked(FILE* f) { return (fflush(f)); }

/* old glibc internal entry points, from binaries compiled against older
   glibc headers where getc and putc were macros over these */
int _IO_getc(FILE* f) { return (fgetc(f)); }
int _IO_putc(int c, FILE* f) { return (fputc(c, f)); }
int __uflow(FILE* f) { return (fgetc(f)); }
int __overflow(FILE* f, int c)
    { return (c == EOF? fflush(f): fputc(c, f)); }

/* line input */
ssize_t getdelim(char** lineptr, size_t* n, int delim, FILE* f)

{

    size_t len = 0;
    int c;

    if (!lineptr || !n || !f) return (-1);
    if (!*lineptr || !*n) {

        *n = 128;
        *lineptr = realloc(*lineptr, *n);
        if (!*lineptr) return (-1);

    }
    for (;;) {

        c = fgetc(f);
        if (c == EOF) break;
        if (len+2 > *n) {

            *n *= 2;
            *lineptr = realloc(*lineptr, *n);
            if (!*lineptr) return (-1);

        }
        (*lineptr)[len++] = c;
        if (c == delim) break;

    }
    (*lineptr)[len] = 0;

    return (len? (ssize_t)len: -1);

}

ssize_t getline(char** lineptr, size_t* n, FILE* f)

{

    return (getdelim(lineptr, n, '\n', f));

}

/* piped commands over this stdio's own files (libasound imports popen) */
static int popen_pids[FOPEN_MAX];

FILE *popen(const char* command, const char* mode)

{

    int   fds[2];
    int   pid;
    int   rd = mode && mode[0] == 'r';
    FILE* f;

    if (pipe(fds) < 0) return (NULL);
    pid = fork();
    if (pid < 0) {

        close(fds[0]);
        close(fds[1]);

        return (NULL);

    }
    if (!pid) { /* child: wire the pipe to the shell */

        if (rd) dup2(fds[1], 1); else dup2(fds[0], 0);
        close(fds[0]);
        close(fds[1]);
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        _exit(127);

    }
    close(rd? fds[1]: fds[0]);
    f = fdopen(rd? fds[0]: fds[1], rd? "r": "w");
    if (!f) close(rd? fds[0]: fds[1]);
    else if (fileno(f) >= 0 && fileno(f) < FOPEN_MAX)
        popen_pids[fileno(f)] = pid;

    return (f);

}

int pclose(FILE* f)

{

    int fd = fileno(f);
    int pid = fd >= 0 && fd < FOPEN_MAX? popen_pids[fd]: 0;
    int status = -1;

    if (fd >= 0 && fd < FOPEN_MAX) popen_pids[fd] = 0;
    fclose(f);
    if (pid > 0) while (waitpid(pid, &status, 0) < 0 && errno == EINTR);

    return (status);

}

/* the C99 scanf aliases, what modern compiles bind fscanf and scanf to */
int __isoc99_fscanf(FILE* f, const char* fmt, ...)

{

    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vfscanf(f, fmt, ap);
    va_end(ap);

    return (r);

}

int __isoc99_vfscanf(FILE* f, const char* fmt, va_list ap)

{

    return (vfscanf(f, fmt, ap));

}

int __isoc99_scanf(const char* fmt, ...)

{

    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vfscanf(stdin, fmt, ap);
    va_end(ap);

    return (r);

}

int __isoc99_vscanf(const char* fmt, va_list ap)

{

    return (vfscanf(stdin, fmt, ap));

}

/* The FILE-creating entries not implemented fail in one loud line
   rather than handing glibc a petit ami FILE (or the reverse) to crash
   on. Implement them here if a library ever wants one. */
static void stdio_noimp(const char* who)

{

    write(2, "petit_ami stdio: ", 17);
    write(2, who, strlen(who));
    write(2, " is not implemented\n", 20);
    abort();

}

FILE *fmemopen(void* buf, size_t size, const char* mode)
    { stdio_noimp("fmemopen"); return (NULL); }
FILE *open_memstream(char** ptr, size_t* sizeloc)
    { stdio_noimp("open_memstream"); return (NULL); }
FILE *fopencookie(void* cookie, const char* mode, cookie_io_functions_t io)
    { stdio_noimp("fopencookie"); return (NULL); }
int fwide(FILE* f, int mode)
    { stdio_noimp("fwide (wide character streams)"); return (0); }
wint_t fgetwc(FILE* f)
    { stdio_noimp("fgetwc (wide character streams)"); return (0); }
wint_t fputwc(wchar_t c, FILE* f)
    { stdio_noimp("fputwc (wide character streams)"); return (0); }

#endif /* !STDIO_BYPASS && __linux__ */
