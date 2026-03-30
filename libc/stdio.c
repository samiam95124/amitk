/*******************************************************************************
*                                                                              *
*                            STANDARD I/O SOURCE                               *
*                                                                              *
*                        COPYRIGHT 2007 (C) S. A. MOORE                        *
*                                                                              *
* FILE NAME: stdio.c                                                           *
*                                                                              *
* DESCRIPTION:                                                                 *
*                                                                              *
* Implements whitebook I/O. The functions are designed to funnel all I/O       *
* down to the direct I/O procedures fread and fwrite. These are then           *
* implemented via calls to the Unix format functions:                          *
*                                                                              *
* read(fd, buf, len);                                                          *
* write(fd, buf, len);                                                         *
* open(name, flags);                                                           *
* close(fd);                                                                   *
* unlink(name);                                                                *
* lseek(fd, off, mode);                                                        *
*                                                                              *
* BUGS/ISSUES:                                                                 *
*                                                                              *
* 1. Printf functions do nothing with the length modifiers.                    *
*                                                                              *
* 2. Note there is no floating point support in this version.                  *
*                                                                              *
* 3. Check that printf and scanf uses longs throughout.                        *
*                                                                              *
* 4. Actual use has shown that there are programs that call the stdio          *
* functions before any constructor for this module can be run. This results in *
* a serious error. Thus we have moved the initialization to compile time       *
* definitions, or at worst, runtime initialization that is "triggered" by NULL *
* values in the data. This means stdio is either "self initialized" or         *
* "inherently initialized" if you prefer. This solves the error, since it does *
* not matter when the stdio calls occur.                                       *
*                                                                              *
* TO DOs                                                                       *
*                                                                              *
* 1. Fill out the definition of rename().                                      *
*                                                                              *
* 2. Add floating point handling to scan and print functions.                  *
*                                                                              *
* 3. Need a test suite for stdio.                                              *
*                                                                              *
* API implemented.                                                             *
*                                                                              *
* The API implemented is the full set of stdio calls. They are listed here:    *
*                                                                              *
* FILE *fopen(const char *filename, const char *mode) - Open a file by name.   *
* FILE *freopen(const char* filename, const char* mode, FILE* stream)          *
*     - Reopen a file by name.                                                 *
* FILE *fdopen(int fd, const char *mode) - Open file descriptor as file.       *
* int fflush(FILE *stream) - Flush pending I/O from file.                      *
* int fclose(FILE *stream) - Close open file.                                  *
* int remove(const char *filename) - Remove file by name.                      *
* int rename(const char *oldname, const char *newname) - Rename file.          *
* FILE *tmpfile(void) - Create a temporary file.                               *
* char *tmpnam(char s[]) - Create a temporary name.                            *
* int setvbuf(FILE* stream, char *buf, int mode, size_t size) - Set file       *
*     buffering characteristics.                                               *
* void setbuf(FILE* stream, char *buf) - Set buffer for file.                  *
* int fprintf(FILE* stream, const char *format, ...) - Print to file.          *
* int printf(const char* format, ...) - Print to stdout.                       *
* int sprintf(char* s, const char *format, ...) - Print to string.             *
* int vprintf(const char* format, va_list arg) - Print to stdout with          *
*     variable list.                                                           *
* int vfprintf(FILE* stream, const char *format, va_list arg) - Print to file  *
*     with variable list.                                                      *
* int vsprintf(char* s, const char *format, va_list arg) - Print to string     *
*     with variable list.                                                      *
* int fscanf(FILE* stream, const char *format, ...) - Scan from file.          *
* int scanf(const char* format, ...) - Scan from stdin.                        *
* int sscanf(const char* s, const char *format, ...) - Scan from string.       *
* int fgetc(FILE *stream) - Get character from file.                           *
* int getc(FILE *stream) - Get character from file.                            *
* char *fgets(char *s, int n, FILE *stream) - Get string from file.            *
* int fputc(int c, FILE *stream) - Put character to file.                      *
* int fputs(const char *s, FILE *stream) - Put string to file.                 *
* int putc(int c, FILE *stream) - Put character to stdout.                     *
* int getchar(void) - Get character from stdin.                                *
* char *gets(char *s) - Get string from stdin.                                 *
* int putc(int c, FILE *stream) - Put character to file.                       *
* int putchar(int c) - Put character to stdout.                                *
* int puts(const char *s) - Put string to stdout.                              *
* int ungetc(int c, FILE *stream) - Put back single character to file.         *
* size_t fread(void *ptr, size_t size, size_t nobj, FILE *stream) - Read       *
*     blocks from file.                                                        *
* size_t fwrite(const void *ptr, size_t size, size_t nobj, FILE *stream)       *
*     Write blocks to file.                                                    *
* int fseek(FILE* stream, long offset, int origin) - Seek file location.       *
* long ftell(FILE* stream) - Find file location.                               *
* void rewind(FILE* stream) - Go to file beginning.                            *
* int fgetpos(FILE* stream, fpos_t *ptr) - Find file location.                 *
* int fsetpos(FILE* stream, const fpos_t *ptr) - Set file location.            *
* void clearerr(FILE* stream) - Clear pending errors on file.                  *
* int feof(FILE* stream) - Check end of file.                                  *
* int ferror(FILE* stream) - Check error on file.                              *
* void perror(const char *s) - Print error message.                            *
* int fileno(FILE* stream) - Find descriptor for file.                         *
*                                                                              *
*******************************************************************************/

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

    /* fid */    0,           /* set logical file 0 */
    /* name */   NULL,        /* set no name attached */
    /* text */   TRUE,        /* set text */
    /* mode */   STDIO_MREAD, /* set read only */
    /* append */ 0,           /* set not append */
    /* pback */  EOF,         /* nothing in the pushback buffer */
    /* flags */  0            /* no state flags active */

};

static FILE stdoutfe = {

    /* fid */    1,            /* set logical file 0 */
    /* name */   NULL,         /* set no name attached */
    /* text */   TRUE,         /* set text */
    /* mode */   STDIO_MWRITE, /* set read only */
    /* append */ 0,            /* set not append */
    /* pback */  EOF,          /* nothing in the pushback buffer */
    /* flags */  0             /* no state flags active */

};


static FILE stderrfe = {

    /* fid */    2,            /* set logical file 0 */
    /* name */   NULL,         /* set no name attached */
    /* text */   TRUE,         /* set text */
    /* mode */   STDIO_MWRITE, /* set read only */
    /* append */ 0,            /* set not append */
    /* pback */  EOF,          /* nothing in the pushback buffer */
    /* flags */  0             /* no state flags active */

};

/* standard in, out and error files */

FILE *stdin = &stdinfe;
FILE *stdout = &stdoutfe;
FILE *stderr = &stderrfe;

/* open files table. The first 0, 1, and 2 entries are tied to stdin, stdout
   and stderr. This does not have to be, but it makes the system more
   organized. */

static FILE *opnfil[FOPEN_MAX] = {

    &stdinfe,   /* standard input */
    &stdoutfe, /* standard output */
    &stderrfe  /* standard error */

    /* remaining entries are NULL */

};

/* top powers table, we precalculate this to save runtime */

static unsigned long power8; /* octal */
static unsigned long power10; /* decimal */
static unsigned long power16; /* hexadecimal */

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

static int tmpcnt; /* temp counter, starting at 0 */
static int tmpnamc; /* current internal stored temp name */
static char tmpstr[L_TMP_MAX][L_tmpnam]; /* temp filename array */

/*******************************************************************************

System call vector linkers

Each call links the system call via a vector. We define a series of wrappers
to serve as the function pointer targets, because the host system may or may
not implement the function directly as a function. It could be a macro.

*******************************************************************************/

static ssize_t wread(int fd, void* buff, size_t count)
    { return read(fd, buff, count); }
static ssize_t wwrite(int fd, const void* buff, size_t count)
    { return write(fd, buff, count); }
static int wopen(const char* pathname, int flags, int perm)
    { return open(pathname, flags, perm); }
static int wclose(int fd)
    { return close(fd); }
static int wunlink(const char* pathname)
    { return unlink(pathname); }
static off_t wlseek(int fd, off_t offset, int whence)
    { return lseek(fd, offset, whence); }

static ssize_t vread(int fd, void* buff, size_t count)
    { return (*vt_read)(fd, buff, count); }
static ssize_t vwrite(int fd, const void* buff, size_t count)
    { return (*vt_write)(fd, buff, count); }
static int vopen(const char* pathname, int flags, int perm)
    { return (*vt_open)(pathname, flags, perm); }
static int vclose(int fd)
    { return (*vt_close)(fd); }
static int vunlink(const char* pathname)
    { return (*vt_unlink)(pathname); }
static off_t vlseek(int fd, off_t offset, int whence)
    { return (*vt_lseek)(fd, offset, whence); }

/*******************************************************************************

System call overriders

Each overrider call takes both a new vector and a place to store the old vector.
The overrider, if it receives a call it does not want to handle, "passes down"
the call by calling the stored vector. This chain continues until it reaches the
original handler established, which goes back to the raw system call.

*******************************************************************************/

void ovr_read(vt_read_t nfp, vt_read_t* ofp) { *ofp = vt_read; vt_read = nfp; }
void ovr_write(vt_write_t nfp, vt_write_t* ofp) { *ofp = vt_write; vt_write = nfp; }
void ovr_open(vt_open_t nfp, vt_open_t* ofp) { *ofp = vt_open; vt_open = nfp; }
void ovr_close(vt_close_t nfp, vt_close_t* ofp) { *ofp = vt_close; vt_close = nfp; }
void ovr_unlink(vt_unlink_t nfp, vt_unlink_t* ofp)
    { *ofp = vt_unlink; vt_unlink = nfp; }
void ovr_lseek(vt_lseek_t nfp, vt_lseek_t* ofp) { *ofp = vt_lseek; vt_lseek = nfp; }

/* nocancel is a glibc thing. We equate it to the regular calls */
void ovr_read_nocancel(vt_read_t nfp, vt_read_t* ofp)
    { *ofp = vt_read; vt_read = nfp; }
void ovr_write_nocancel(vt_write_t nfp, vt_write_t* ofp)
    { *ofp = vt_write; vt_write = nfp; }
void ovr_open_nocancel(vt_open_t nfp, vt_open_t* ofp)
    { *ofp = vt_open; vt_open = nfp; }
void ovr_close_nocancel(vt_close_t nfp, vt_close_t* ofp)
    { *ofp = vt_close; vt_close = nfp; }

/*******************************************************************************

FUNCTION NAME: maknod

SHORT DESCRIPTION: Create a file access node

DETAILED DESCRIPTION:

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

BUGS/ISSUES:

*******************************************************************************/

static int maknod(void)

{

    int i, f; /* file table indexs */

    /* search first free file entry */
    f = 0; /* set no entry found */
    for (i = 0; i < FOPEN_MAX; i++) /* traverse table */
       if (!opnfil[i]) f = i; /* found NULL entry */
       else if (opnfil[i]->fid < 0) f = i; /* found closed entry */
    if (!f) return (f); /* file table is full, return error */
    if (!opnfil[f]) { /* not recyling a previous entry, allocate */

       opnfil[f] = malloc(sizeof(FILE)); /* get a new file tracking struct */
       if (!opnfil[f]) return (0); /* couldn't allocate, exit w/ error */

    }
    /* initalize file access fields */
    opnfil[f]->fid = 0; /* set no logical file attached */
    opnfil[f]->name = NULL; /* set no name attached */
    opnfil[f]->text = FALSE; /* set not text */
    opnfil[f]->mode = 0; /* set read only */
    opnfil[f]->append = 0; /* set not append */
    opnfil[f]->pback = EOF; /* nothing in the pushback buffer */

    return f; /* return file id */

}

/*******************************************************************************

FUNCTION NAME: putchrs

SHORT DESCRIPTION: Places a series of characters in a string

DETAILED DESCRIPTION:

Places a given number of characters in a string. The string pointer is advanced
past the placed characters.

BUGS/ISSUES:

*******************************************************************************/

static void putchrs(char *s[], int cnt, char c, int *ocnt, FILE *fd)

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

/*******************************************************************************

FUNCTION NAME: digits

SHORT DESCRIPTION: Find digit count

DETAILED DESCRIPTION:

Finds the count of digits in an unsigned number. Accepts a radix. The radix
needs to be one of 2, 8, 10 or 16, otherwise the result is nonsense.

BUGS/ISSUES:

*******************************************************************************/

static int digits(int r, unsigned long l)

{

    unsigned long p; /* power holder */
    int cnt;         /* digit count */

    p = 1*r; /* set power */
    cnt = 1; /* set digit count */
    while (p && l >= p) { /* find digits */

        p *= r; /* find next power */
        cnt++; /* count digits */

    }

    return (cnt); /* exit with digit count */

}

/*******************************************************************************

FUNCTION NAME: toppow

SHORT DESCRIPTION: Find top power in unsigned long

DETAILED DESCRIPTION:

Finds the top power of the given radix that will fit into an unsigned long.

BUGS/ISSUES:

*******************************************************************************/

static unsigned long toppow(int r)

{

    unsigned long p; /* power */
    unsigned long ps; /* power save */

    /* find top power */
    p = 1;
    do { /* iterate */

        ps = p; /* save last good power */
        p = p*r; /* find next power */

    } while (p/r == ps);

    return ps; /* return last good power */

}

/*******************************************************************************

FUNCTION NAME: putnum

SHORT DESCRIPTION: Put unsigned number

DETAILED DESCRIPTION:

Places an unsigned number into a string. Accepts a radix. No padding or
formatting is done. The radix must be 2, 8, 10 or 16, or garbage will result.
Also accepts a top power to use in the convertion.

BUGS/ISSUES:

*******************************************************************************/

static void putnum(char **s, unsigned long l, int r, unsigned long p, int ucase,
                   int *ocnt, FILE *fd)

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

/*******************************************************************************

FUNCTION NAME: getnum

SHORT DESCRIPTION: Get unsigned number

DETAILED DESCRIPTION:

Gets an unsigned number from a string. The string pointer is advanced over the
number, and the number returned in a variable. If there is no number present,
zero is returned.

BUGS/ISSUES:

No overflow is checked.

*******************************************************************************/

static void getnum(const char **s, int *i)

{

    *i = 0; /* clear result */
    while (**s >= '0' && **s <= '9') *i = *i*10+*(*s)++-'0';

}

/*******************************************************************************

FUNCTION NAME: chkrad

SHORT DESCRIPTION: Check if the given character lies in the radix

DETAILED DESCRIPTION:

Given a character and a radix, checks if the character lies in the radix, that
is, either a digit or an alphabetical character within the radix. Returns
true if so.

BUGS/ISSUES:

*******************************************************************************/

static int chkrad(int c, int r)

{

    /* convert character 0 based number */
    if (isdigit(c)) c -= '0'; else c = tolower(c)-'a'+10;

    /* check bounded by radix and return that status */
    return (c >= 0 && c < r);

}

/*******************************************************************************

FUNCTION NAME: getfstr

SHORT DESCRIPTION: Get character from string or file

DETAILED DESCRIPTION:

Given a string and a file stream, gets a character from either the string or
a file. If the string is NULL, then the file is read from. If either the string
end or the end of the file is encountered, or an error occurs in the file,
then EOF is returned. The file or the string is advanced past the read character
if not EOF.

BUGS/ISSUES:

*******************************************************************************/

static int getfstr(const char **s, FILE *fd)

{

    if (*s) { /* string */

        if (!**s) return (EOF); /* end of string */
        else return (*(*s)++); /* return next character */

    } else return (fgetc(fd)); /* get character from file */

}

/*******************************************************************************

FUNCTION NAME: chkfstr

SHORT DESCRIPTION: Check character from string or file

DETAILED DESCRIPTION:

Given a string and a file stream, gets a character from either the string or
a file. If the string is NULL, then the file is read from. If either the string
end or the end of the file is encountered, or an error occurs in the file,
then EOF is returned. The file or the string is not advanced, which means that
in case of a file, the characrter is put back.

BUGS/ISSUES:

*******************************************************************************/

static int chkfstr(const char *s, FILE *fd)

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

/*******************************************************************************

FUNCTION NAME: chkfstrlen

SHORT DESCRIPTION: Get character from metered string or file

DETAILED DESCRIPTION:

Gets the next character from a metered string. This is a string whose length
is set. If the length is 0, then we return EOF, so that the end of the string
becomes either the zero marking the end, or the length reaching zero.

BUGS/ISSUES:

*******************************************************************************/

static int chkfstrlen(const char *s, int len, FILE *fd)

{

    if (len) return (chkfstr(s, fd)); else return (EOF);

}

/*******************************************************************************

FUNCTION NAME: getnum

SHORT DESCRIPTION: Get unsigned number

DETAILED DESCRIPTION:

Gets an unsigned number from a string. The string pointer is advanced over the
number, and the number returned in a variable. If there is no number present,
zero is returned. The radix for the number is specified. If the number
overflows, the overflow flag will be returned true, else false. On overflow,
the number returned in garbage. All characters that fit the radix are skipped,
even if overflow becomes true.

BUGS/ISSUES:

*******************************************************************************/

static void getnumro(const char **s, unsigned long *l, int r, int *o, int *cnt,
                     int *fld, FILE *fd)

{

    unsigned long save; /* save for error check */

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

/*******************************************************************************

FUNCTION NAME: strtoulso

SHORT DESCRIPTION: String to unsigned long with sign and overflow

DETAILED DESCRIPTION:

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

BUGS/ISSUES:

*******************************************************************************/

static unsigned long strtoulso(const char **s, int base, int *sgn, int *o,
                               int *cnt, int *err, int fld, FILE *fd)

{

    unsigned long v; /* value */

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
        if (base == 16 && !v && (**s == 'x' || **s == 'X')) {

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

/*******************************************************************************

FUNCTION NAME: strtoli

SHORT DESCRIPTION: Convert string to signed long number

DETAILED DESCRIPTION:

The given string is converted to a signed long integer, see the strtoulso
function. Takes a base radix, an input count, and a field.

If the number overlows, it will be replaced with either a MAX or MIN, according
to if the sign is set or not, and no overlow is returned.

BUGS/ISSUES:

*******************************************************************************/

static long strtoli(const char **s, int base, int *cnt, int *err, int fld,
                    FILE *fd)

{

    int           sgn;  /* sign of number */
    unsigned long v;    /* value */
    int           o;    /* overflow */

    /* process using universal signed/unsigned function */
    v = strtoulso(s, base, &sgn, &o, cnt, err, fld, fd);

    /* return proper value, including overflow cases */
    if (o && sgn > 0) return (LONG_MAX); /* overflow and positive */
    else if (o && sgn < 0) return (LONG_MIN); /* overflow and negative */
    else return (v*sgn); /* return value in correct sign */

}

/*******************************************************************************

FUNCTION NAME: strtouli

SHORT DESCRIPTION: Convert string to unsigned long number

DETAILED DESCRIPTION:

The given string is converted to a unsigned long integer, see the strtoulso
function. Takes a base radix, an input count, and a field.

If the number overlows, it will be replaced with MAX, and no overlow is
returned.

BUGS/ISSUES:

*******************************************************************************/

static unsigned long strtouli(const char **s, int base, int *cnt, int *err, int fld,
                              FILE *fd)

{

    int           sgn;  /* sign of number */
    unsigned long v;    /* value */
    int           o;    /* overflow */

    /* process using universal signed/unsigned function */
    v = strtoulso(s, base, &sgn, &o, cnt, err, fld, fd);

    /* return proper value, including overflow cases */
    if (o) return (ULONG_MAX); /* overflow and positive */
    else return (v*sgn); /* return value in correct sign */

}

/*******************************************************************************

External API section

This begins the external API section.

*******************************************************************************/

/*******************************************************************************

FUNCTION NAME: fopen

SHORT DESCRIPTION: Open a new or existing file

DETAILED DESCRIPTION:

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

BUGS/ISSUES:

*******************************************************************************/

FILE *fopen(const char *filename, const char *mode)

{

    int fti;    /* file table index */
    int flags;  /* open flag settings */
    int text;   /* text/binary mode */
    int modcod; /* mode code, 0 = read, 1 = write, 2 = append */
    int append; /* append mode */
    int perm;   /* permissions */

    /* move mode attributes to flags */
    modcod = 0; /* default to read */
    text = !!strchr(mode, 'b'); /* set text or binary mode */
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
    if (modcod == 2) flags |= O_APPEND; /* set append mode */
    if (modcod == 1) flags |= O_CREAT; /* allow writes to create new file */

    /* permissions are: user read and write, group and others read only */
#ifdef __linux__
    perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
    perm = S_IRUSR | S_IWUSR;
#endif

    /* process file open with parameters */
    fti = maknod(); /* create or reuse file entry */
    if (!fti) return NULL; /* couldn't create node */
    opnfil[fti]->fid = vopen(filename, flags, perm); /* open file with flags */
    if (opnfil[fti]->fid < 0) return (NULL); /* return error */

    /* fill file fields with status */
    opnfil[fti]->name = (char *)malloc(strlen(filename)+1); /* get name space */
    if (!opnfil[fti]->name) return NULL; /* couldn't allocate name */
    strcpy(opnfil[fti]->name, filename); /* copy name into place */
    opnfil[fti]->text = text; /* text/binary mode */
    /* set read/write mode for update */
    if (append) opnfil[fti]->mode = STDIO_MRDWR;
    else if (modcod == 0) opnfil[fti]->mode = STDIO_MREAD; /* set read only */
    else opnfil[fti]->mode = STDIO_MWRITE; /* set write only */
    opnfil[fti]->append = append; /* set append mode */
    opnfil[fti]->flags = 0; /* clear status/error flags */

    /* return new file entry */
    return (opnfil[fti]);

}

/*******************************************************************************

FUNCTION NAME: fflush

SHORT DESCRIPTION: Flushes output on the given file, or all files

DETAILED DESCRIPTION:

Causes all buffered writes to a given file, or all files, to be written
immediately. If a stream is passed, that is flushed. If NULL is passed, then all
files are flushed.

Returns EOF if an error occurs while writing to a file, otherwise 0.

BUGS/ISSUES:

1. Buffering is not implemented at the present time.

*******************************************************************************/

int fflush(FILE *stream)

{

    return (0); /* no op */

}

/*******************************************************************************

FUNCTION NAME: fclose

SHORT DESCRIPTION: Closes an open file

DETAILED DESCRIPTION:

Closes an open file entry. Returns EOF for error if the file is not in fact
open, or if the Unix close function returns an error.

BUGS/ISSUES:

*******************************************************************************/

int fclose(FILE *stream)

{

    int r; /* result holder */

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (EOF);
    r = vclose(stream->fid); /* close file */
    stream->fid = -1; /* flag file tracking entry now free */
    free(stream->name); /* free name string */
    stream->name = 0; /* clear name string pointer */
    /* determine result code */
    if (r < 0) return EOF; /* didn't close */
    else return 0; /* closed properly */

}

/*******************************************************************************

FUNCTION NAME: freopen

SHORT DESCRIPTION: Reopen an existing file under a new name and mode.

DETAILED DESCRIPTION:

Closes the given file and reopens it under a new name and mode. Basically, it's
the same as fopen, but uses an existing file entry.

BUGS/ISSUES:

*******************************************************************************/

FILE *freopen(const char *filename, const char *mode, FILE *stream)

{

    int flags;  /* open flag settings */
    int text;   /* text/binary mode */
    int modcod; /* mode code, 0 = read, 1 = write, 2 = append */
    int update; /* update mode */
    int perm;   /* permissions */

    /* close the stream, and return error if occurs */
    if (fclose(stream) != 0) return (NULL);

    /* move mode attributes to flags */
    text = !!strchr(mode, 'b'); /* set text or binary mode */
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
    if (modcod == 2) flags |= O_APPEND; /* set append mode */
    if (modcod == 1) flags |= O_CREAT; /* allow writes to create new file */

    /* permissions are: user read and write, group and others read only */
#ifdef __linux__
    perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
    perm = S_IRUSR | S_IWUSR;
#endif

    /* process file open with parameters */
    stream->fid = vopen(filename, flags, perm); /* open file with flags */
    if (stream->fid < 0) return (NULL); /* return error */

    /* fill file fields with status */
    stream->name = (char *)malloc(strlen(filename)+1); /* get name space */
    if (stream->name) return NULL; /* couldn't allocate name */
    strcpy(stream->name, filename); /* copy name into place */
    stream->text = text; /* text/binary mode */
    /* set read/write mode for update */
    if (update) stream->mode = STDIO_MRDWR;
    else if (modcod == 0) stream->mode = STDIO_MREAD; /* set read only */
    else stream->mode = STDIO_MWRITE; /* set write only */
    stream->append = update; /* set append mode */
    stream->flags = 0; /* clear status/error flags */

    /* return new file entry */
    return stream;

}

/*******************************************************************************

FUNCTION NAME: fdopen

SHORT DESCRIPTION: Open a stream file with existing file descriptor.

DETAILED DESCRIPTION:

Given an existing file descriptor, creates a stream file and opens it with that
file descriptor. This essentially means to "fileofy" an existing, already open,
low level file descriptor to allow stream file operations on it.

The mode must be compatible with the mode of the file descriptor.

BUGS/ISSUES:

1. Mingw does not implement fcntl() F_GETFL command, so we cannot check modes
are compatible.

*******************************************************************************/

FILE *fdopen(int fd, const char *mode)

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
    text = !!strchr(mode, 'b'); /* set text or binary mode */
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
    if (modcod == 2) flags |= O_APPEND; /* set append mode */
    if (modcod == 1) flags |= O_CREAT; /* allow writes to create new file */

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
    opnfil[fti]->fid = fd; /* set file id */

    /* get and match existing file parameters */
#ifndef __MINGW32__
    /* note mingw does not implement this call at present. The result will be
       that this call will not check the modes are equivalent. */
    fsf = fcntl(fd, F_GETFL);
    if ((fsf & (O_APPEND | O_TRUNC | O_CREAT | O_RDWR | O_WRONLY)) !=
        (flags & (O_APPEND | O_TRUNC | O_CREAT | O_RDWR | O_WRONLY))) {

        errno = EINVAL; /* flag error */
        return (NULL); /* return error */

    }
#endif

    /* fill file fields with status */
    opnfil[fti]->text = text; /* text/binary mode */
    /* set read/write mode for update */
    if (append) opnfil[fti]->mode = STDIO_MRDWR;
    else if (modcod == 0) opnfil[fti]->mode = STDIO_MREAD; /* set read only */
    else opnfil[fti]->mode = STDIO_MWRITE; /* set write only */
    opnfil[fti]->append = append; /* set append mode */
    opnfil[fti]->flags = 0; /* clear status/error flags */

    /* return new file entry */
    return (opnfil[fti]);

}

/*******************************************************************************

FUNCTION NAME: remove

SHORT DESCRIPTION: Removes a file from the file system.

DETAILED DESCRIPTION:

Removes a file, by deleting it permanently from the file system. Returns
non-zero if the attempt to remove fails, otherwise zero.

BUGS/ISSUES:

*******************************************************************************/

int remove(const char *filename)

{

    /* unlink the filename, returning error */
    return (!!vunlink(filename));

}

/*******************************************************************************

FUNCTION NAME: rename

SHORT DESCRIPTION: change the name of and existing file.

DETAILED DESCRIPTION:

Changes the name of an existing file. Both the old and new names are provided.
Returns non-zero if the attempt to rename fails, otherwise zero. It is an error
if the old filename does not exist.

BUGS/ISSUES:

*******************************************************************************/

int rename(const char *oldname, const char *newname)

{

    /* fprintf(stderr, "*** Function 'rename' not implemented\n"); */
    exit(1);

}

/*******************************************************************************

FUNCTION NAME: tmpfile

SHORT DESCRIPTION: Create temporary file

DETAILED DESCRIPTION:

Create a temporary file of mode "wb+" that will automatically be closed and
removed when the program ends.

BUGS/ISSUES:

1. These names are only unique to the program running. Different programs could
collide.

*******************************************************************************/

FILE *tmpfile(void)

{

    char ts[L_tmpnam];

    tmpnam(ts); /* create a temp name */

    /* note we don't need to hold on to the name */
    return (fopen(ts, "wb+"));

}

/*******************************************************************************

FUNCTION NAME: tmpnam

SHORT DESCRIPTION: Create (coin) temporary filename

DETAILED DESCRIPTION:

Creates a name that, when opened, will give a unique filename that duplicates
no other existing filename.

BUGS/ISSUES:

1. This does not really generate unique names, and so is suitable for testing
only.

*******************************************************************************/

char *tmpnam(char s[])

{

    if (!s) s = &tmpstr[tmpnamc++][0]; /* index the temp string */
    sprintf(s, "temp%4d", tmpcnt);

    return (s);

}

/*******************************************************************************

FUNCTION NAME: setvbuf

SHORT DESCRIPTION: Set buffering characteristics for file

DETAILED DESCRIPTION:

Determines file buffering mode for an open file. Must be called before any read
or write operations are performed on the file. The stream for the file to be
changed, the buffering mode, the buffer size, and a pointer to a buffer for
the file is provided. If the buffer pointer is NULL, then a buffer will be
allocated instead. The valid buffer modes are:

_IOFBF  Full buffering
_IOLBF  Line buffering (buffer flushed by line end)
_IONBF  No buffering

Returns non-zero for error, otherwise zero.

BUGS/ISSUES:

1. We don't do buffering at this time, so this routine is a no-op.

*******************************************************************************/

int setvbuf(FILE *stream, char *buf, int mode, size_t size)

{

    return (0);

}

/*******************************************************************************

FUNCTION NAME: setbuf

SHORT DESCRIPTION: Sets up a buffer for the file

DETAILED DESCRIPTION:

Either sets up a buffer to be used with the file, or sets buffering off if the
buffer pointer is NULL.

BUGS/ISSUES:

*******************************************************************************/

void setbuf(FILE *stream, char *buf)

{

    if (!buf) setvbuf(stream, buf, _IONBF, 0); /* turn buffering off */
    else (void) setvbuf(stream, buf, _IOFBF, BUFSIZ);

}

/*******************************************************************************

FUNCTION NAME: vsprintfe

SHORT DESCRIPTION: Place converted formatted arguments in string

DETAILED DESCRIPTION:

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

h       Short
l       Long
L       Double (float)

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

BUGS/ISSUES:

*******************************************************************************/

static int vsprintfe(char *s, const char *fmt, va_list ap, FILE *fd)

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
    int           sht;  /* short length modifier */
    int           lng;  /* long length modifier */
    int           dbl;  /* double length modifier */
    int           i;    /* integer holding */
    int           sn;   /* sign from number */
    int           dg;   /* number of digits in unsigned number */
    int           ndg;  /* net digits after precision */
    int           pe;   /* extra padding for precision */
    unsigned long u;    /* unsigned integer holding */
    int           r;    /* radix holder */
    unsigned long p;    /* power holder */
    int           *ip;  /* int pointer holder */
    char          *cp;  /* char pointer holder */
    int           l;    /* string length */
    double        d;    /* floating point holder */
    int           dp;   /* decimal point is printed */

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
            sht  = FALSE; /* no short length modifier */
            lng  = FALSE; /* no long length modifier */
            dbl  = FALSE; /* no double length modifier */

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
            if (*fmt == 'h') { fmt++; sht = TRUE; } /* short */
            else if (*fmt == 'l') { fmt++; lng = TRUE; } /* long */
            else if (*fmt == 'L') { fmt++; dbl = TRUE; } /* double */

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

                        i = va_arg(ap, int); /* get signed integer */
                        if (i < 0) { /* remove sign */

                            sn = TRUE; /* set sign */
                            u = -i; /* remove sign */

                        } else u = i; /* place unsigned */

                    } else /* get unsigned integer */
                        u = va_arg(ap, unsigned int);
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
                    d = va_arg(ap, double);
                    /* validate power */
                    if (!power10) power10 = toppow(10); /* decimal */
                    if (!pres) pre = 6; /* set default precision */
                    dp = TRUE; /* set print decimal point */
                    if (!pre)
                        dp = FALSE; /* precision is zero, kill decimal point */
                    if (d < 0) { /* signed number */

                        sn = TRUE; /* set sign */
                        d = -d; /* remove from number */

                    }
                    u = d; /* find whole part of number */
                    d -= u; /* and remove it */
                    dg = digits(10, u); /* find digits in whole part */
                    /* find total of sign whole digits, decimal, fraction */
                    ndg = sn+dg+dp+pre;
                    /* if right justified, pad left to go right */
                    if (!lft) putchrs(&s, fld-ndg, ' ', &cnt, fd);
                    /* place sign */
                    if (sn) putchrs(&s, 1, '-', &cnt, fd); /* place negative */
                    /* print whole number */
                    putnum(&s, u, 10, power10, FALSE, &cnt, fd);
                    if (dp)
                        putchrs(&s, 1, '.', &cnt, fd); /* place decimal point */
                    /* scale number by precision */
                    for (i = 0; i < pre; i++) d *= 10;
                    u = d; /* get that */
                    dg = digits(10, u); /* find digits in that */
                    putchrs(&s, pre-dg, '0', &cnt, fd);
                    putnum(&s, u, 10, power10, FALSE, &cnt, fd);
                    /* if left justified, pad right to go left */
                    if (lft) putchrs(&s, fld-ndg, ' ', &cnt, fd);
                    fmt++; /* next character */
                    break;
                case 'e':
                case 'E':
                case 'g':
                case 'G': /* floating point convertions */
                    /* fprintf(stderr, "*** Floating point not implemented\n"); */
                    exit(1);
                    break;
                case 'n':
                    ip = va_arg(ap, int *); /* get signed integer pointer */
                    *ip = cnt; /* place output count */
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

/*******************************************************************************

FUNCTION NAME: vsprintf

SHORT DESCRIPTION: Print to string with variable argument list pointer

DETAILED DESCRIPTION:

Prints formatted to a string. See vsprintfe. The destination string, the
format string, and the argument list pointer are provided.

BUGS/ISSUES:

*******************************************************************************/

int vsprintf(char *s, const char *fmt, va_list ap)

{

    return (vsprintfe(s, fmt, ap, (FILE *)NULL)); /* process format to string */

}

/*******************************************************************************

FUNCTION NAME: sprintf

SHORT DESCRIPTION: print formatted to string

DETAILED DESCRIPTION:

Prints formatted to a string. See vsprintfe. The destination string, the
format string, and the argument list are provided.

BUGS/ISSUES:

*******************************************************************************/

int sprintf(char *s, const char *fmt, ...)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vsprintf(s, fmt, ap); /* process format to string */
    va_end(ap); /* close argument list */

    return (r);

}

/*******************************************************************************

FUNCTION NAME: vfprintf

SHORT DESCRIPTION: Print formatted to file with variable argument list pointer

DETAILED DESCRIPTION:

Prints formatted to a file. See vsprintfe. The destination string, the
format string, and the argument list pointer are provided.

BUGS/ISSUES:

*******************************************************************************/

int vfprintf(FILE *stream, const char *fmt, va_list ap)

{

    return vsprintfe((char *)NULL, fmt, ap, stream); /* process */

}

/*******************************************************************************

FUNCTION NAME: fprintf

SHORT DESCRIPTION: Print formatted to file

DETAILED DESCRIPTION:

Prints formatted to a file. See vsprintfe. The destination string, the
format string, and the argument list are provided.

BUGS/ISSUES:

*******************************************************************************/

int fprintf(FILE *stream, const char *fmt, ...)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vfprintf(stream, fmt, ap);
    va_end(ap); /* close argument list */

    return (r);

}

/*******************************************************************************

FUNCTION NAME: vprintf

SHORT DESCRIPTION: Print formatted to stdout with variable argument list pointer

DETAILED DESCRIPTION:

Prints formatted to the standard output file. See vsprintfe. The destination
string, the format string, and the argument list pointer are provided.

BUGS/ISSUES:

*******************************************************************************/

int vprintf(const char *fmt, va_list ap)

{

    return (vfprintf(stdout, fmt, ap)); /* print */

}

/*******************************************************************************

FUNCTION NAME: printf

SHORT DESCRIPTION: Print formatted to file

DETAILED DESCRIPTION:

Prints formatted to a file. See vsprintfe. The destination string, the
format string, and the argument list are provided.

BUGS/ISSUES:

*******************************************************************************/

int printf(const char *fmt, ...)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vprintf(fmt, ap); /* print */
    va_end(ap); /* close argument list */

    return (r);

}

/*******************************************************************************

FUNCTION NAME: vsscanfe

SHORT DESCRIPTION:

DETAILED DESCRIPTION:

BUGS/ISSUES:

*******************************************************************************/

int vsscanfe(const char *s, const char *fmt, va_list ap, FILE *fd)

{

    int      ccnt;   /* number of characters processed */
    int      pcnt;   /* number of parameters processed */
    int      fld;    /* minimum field width */
    int      flds;   /* minimum field width was set */
    int      sup;    /* suppress output */
    int      sht;    /* short length modifier */
    int      lng;    /* long length modifier */
    int      dbl;    /* double length modifier */
    int      i;      /* integer holding */
    int      *ip;    /* int pointer holder */
    int      r;      /* radix */
    unsigned u;      /* unsigned holding */
    unsigned *up;    /* unsigned pointer holder */
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

        if (!s) return (EOF); /* out of input, return end */
        if (*fmt == '%') { /* format specification */

            fmt++; /* skip '%' */

            /* clear convertion flags and fields */
            sup  = FALSE;   /* set no supress */
            fld  = INT_MAX; /* set minimum input field */
            flds = FALSE;   /* field width not set */
            sht  = FALSE;   /* no short length modifier */
            lng  = FALSE;   /* no long length modifier */
            dbl  = FALSE;   /* no double length modifier */
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
            if (*fmt == 'h') { fmt++; sht = TRUE; } /* short */
            else if (*fmt == 'l') { fmt++; lng = TRUE; } /* long */
            else if (*fmt == 'L') { fmt++; dbl = TRUE; } /* double */

            /* find format character */
            switch (*fmt) { /* handle each format */

                case 'i': /* open radix */
                case 'd': /* signed decimal */
                    if (*fmt == 'i') r = 0; /* open radix */
                    else r = 10; /* decimal radix */
                    i = strtoli(&s, r, &ccnt, &err, fld, fd); /* parse number */
                    if (err) return (pcnt); /* error */
                    if (!sup) { /* if supress isn't on, place result */

                        ip = va_arg(ap, int *); /* get parameter */
                        *ip = i; /* place result */
                        if (!err) pcnt++; /* count items parsed */

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
                    u = strtouli(&s, r, &ccnt, &err, fld, fd); /* parse number */
                    if (err) return (pcnt); /* error */
                    if (!sup) { /* if supress isn't on, place result */

                        up = va_arg(ap, unsigned *); /* get parameter */
                        *up = u; /* place result */
                        if (!err) pcnt++; /* count items parsed */

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
                case 'g': /* floating point convertions */
                    /* fprintf(stderr, "*** Floating point not implemented\n"); */
                    exit(1);
                    break;
                case 'n':
                    if (!sup) { /* if supress isn't on, place result */

                        ip = va_arg(ap, int *); /* get parameter */
                        *ip = ccnt; /* set character count */

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

/*******************************************************************************

FUNCTION NAME: vsscanf

SHORT DESCRIPTION:

DETAILED DESCRIPTION:

BUGS/ISSUES:

*******************************************************************************/

static int vsscanf(const char *s, const char *fmt, va_list ap)

{

    /* process format from string */
    return (vsscanfe(s, fmt, ap, (FILE *)NULL));

}

/*******************************************************************************

FUNCTION NAME: vfscanf

SHORT DESCRIPTION:

DETAILED DESCRIPTION:

BUGS/ISSUES:

*******************************************************************************/

static int vfscanf(FILE *stream, const char *fmt, va_list ap)

{

    /* process format from string */
    return (vsscanfe((char *)NULL, fmt, ap, stream));

}

/*******************************************************************************

FUNCTION NAME: sscanf

SHORT DESCRIPTION:

DETAILED DESCRIPTION:

BUGS/ISSUES:

*******************************************************************************/

int sscanf(const char *s, const char *fmt, ...)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vsscanf(s, fmt, ap);
    va_end(ap); /* close argument list */

    return (r);

}

/*******************************************************************************

FUNCTION NAME: fscanf

SHORT DESCRIPTION:

DETAILED DESCRIPTION:

BUGS/ISSUES:

*******************************************************************************/

int fscanf(FILE *stream, const char *fmt, ...)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vfscanf(stream, fmt, ap);
    va_end(ap); /* close argument list */

    return (r);

}

/*******************************************************************************

FUNCTION NAME: scanf

SHORT DESCRIPTION:

DETAILED DESCRIPTION:

BUGS/ISSUES:

*******************************************************************************/

int scanf(const char *fmt, ...)

{

    va_list ap; /* argument list pointer */
    int r;

    va_start(ap, fmt); /* open argument list */
    r = vfscanf(stdin, fmt, ap);
    va_end(ap); /* close argument list */

    return (r);

}

/*******************************************************************************

FUNCTION NAME: fgetc

SHORT DESCRIPTION: Gets a single character from a stream

DETAILED DESCRIPTION:

The next character in a stream is returned, or EOF if end of file or error is
encountered. In case of error, errno must be used to determine the exact
type of error.

BUGS/ISSUES:

*******************************************************************************/

int fgetc(FILE *stream)

{

    unsigned char b; /* byte buffer */
    int rc;          /* read count */
    int c;           /* character buffer */

    if (!stream || stream->fid < 0) return EOF; /* check stream is open */
    if (stream->pback != EOF) { /* there is a pushback character */

        c = stream->pback; /* get the buffered character */
        stream->pback = EOF; /* clear the buffer */
        return (c); /* return character */

    } else { /* standard read */

        rc = vread(stream->fid, &b, 1); /* read one byte */
        if (!rc) stream->flags |= _EFEOF; /* set end of file if true */
        if (rc != 1) return EOF; /* return EOF or other error */
        else return (b); /* return character */

    }

}

/*******************************************************************************

FUNCTION NAME: fgets

SHORT DESCRIPTION: Get line from file with limit

DETAILED DESCRIPTION:

Gets a line of text from the text file, with limit. Characters are read from the
input file to the array until the limit is reached, or a newline is encountered,
or EOF is encountered. If a newline is encountered, it is read and discarded.
Note that EOF can also signify an error.

BUGS/ISSUES:

*******************************************************************************/

char *fgets(char *s, int n, FILE *stream)

{

    int c; /* input character holder */
    char *s1; /* input array holder */
    int cc;   /* character count */

    if (!stream || stream->fid < 0) return (NULL);
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

/*******************************************************************************

FUNCTION NAME: fputc

SHORT DESCRIPTION: Output character to file

DETAILED DESCRIPTION:

Outputs a single character to a file. Returns EOF if there is an error.

BUGS/ISSUES:

*******************************************************************************/

int fputc(int c, FILE *stream)

{

    unsigned char b; /* byte buffer */
    int wc;          /* write count */

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (EOF);
    b = c; /* place character value in buffer */
    wc = vwrite(stream->fid, &b, 1); /* write one byte */
    if (wc != 1) return (EOF); /* return EOF for error */
    else return (c); /* return character written */

}

/*******************************************************************************

FUNCTION NAME: fputs

SHORT DESCRIPTION: Output string to file

DETAILED DESCRIPTION:

Outputs a string to a file. Note that a newline is NOT automatically output for
the string.

BUGS/ISSUES:

*******************************************************************************/

int fputs(const char *s, FILE *stream)

{

    while (*s) { /* output string */

       /* output character */
       if (fputc(*s, stream) == EOF) return EOF;
       s++; /* next character */

    }

    return (0); /* return no error */

}

/*******************************************************************************

FUNCTION NAME: getc

SHORT DESCRIPTION: Gets a single character from a stream

DETAILED DESCRIPTION:

The next character in a stream is returned, or EOF if end of file or error is
encountered. In case of error, errno must be used to determine the exact
type of error.

This routine is identical to fgetc. It isn't a macro, as most modern compilers
have automatic inlining of functions.

BUGS/ISSUES:

*******************************************************************************/

#ifndef USEMACRO

int getc(FILE *stream)

{

    return fgetc(stream); /* process get */

}

#endif

/*******************************************************************************

FUNCTION NAME: getchar

SHORT DESCRIPTION: Get single character from standard input

DETAILED DESCRIPTION:

Gets a single character from the standard input. Equivalent to getc(stdin).
It isn't a macro, because most modern compilers can perform automatic inlining.

BUGS/ISSUES:

*******************************************************************************/

int getchar(void)

{

    return getc(stdin); /* get from standard input */

}

/*******************************************************************************

FUNCTION NAME: gets

SHORT DESCRIPTION: Get input line from standard input

DETAILED DESCRIPTION:

Gets a full line of text from the standard input into the given array. The
terminating newline is read and discarded. If EOF is encountered in the file,
the line is terminated immediately. Note that EOF can also signify an error.

BUGS/ISSUES:

*******************************************************************************/

char *gets(char *s)

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

/*******************************************************************************

FUNCTION NAME: putc

SHORT DESCRIPTION: Put a single character to a file

DETAILED DESCRIPTION:

Puts a single character to the given file. It isn't a macro, because most modern
compilers can perform automatic inlining.

BUGS/ISSUES:

*******************************************************************************/

int putc(int c, FILE *stream)

{

    return fputc(c, stream); /* process put */

}

/*******************************************************************************

FUNCTION NAME: putchar

SHORT DESCRIPTION: Output character to standard output

DETAILED DESCRIPTION:

Outputs a single character to the standard output. It isn't a macro, because
most modern compilers can perform automatic inlining.

BUGS/ISSUES:

*******************************************************************************/

int putchar(int c)

{

    return putc(c, stdout); /* put standard output character */

}

/*******************************************************************************

FUNCTION NAME: puts

SHORT DESCRIPTION: Output string to standard output

DETAILED DESCRIPTION:

Outputs a string to the standard output.

BUGS/ISSUES:

*******************************************************************************/

int puts(const char *s)

{

    int rc; /* return code */

    rc = fputs(s, stdout); /* output string */
    if (rc) return EOF; /* exit with error */
    return fputs("\n", stdout); /* output end of line */

}

/*******************************************************************************

FUNCTION NAME: ungetc

SHORT DESCRIPTION: Put character back to input

DETAILED DESCRIPTION:

Places a single character into the putback buffer for the input file.

BUGS/ISSUES:

*******************************************************************************/

int ungetc(int c, FILE *stream)

{


    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (EOF);
    stream->pback = c; /* put back single character */

    return (c);

}

/*******************************************************************************

FUNCTION NAME: fread

SHORT DESCRIPTION: Read direct from file

DETAILED DESCRIPTION:

Reads a number of objects from the given file. The number and size of the
objects are specified.

BUGS/ISSUES:

*******************************************************************************/

size_t fread(void *ptr, size_t size, size_t nobj, FILE *stream)

{

    int r;

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (0);
    r = vread(stream->fid, ptr, size*nobj); /* process read */
    if (!r) stream->flags |= _EFEOF; /* set EOF encountered */

    return r;

}

/*******************************************************************************

FUNCTION NAME: fwrite

SHORT DESCRIPTION: Write direct to file

DETAILED DESCRIPTION:

Writes a number of objects to the given file. The number and size of the objects
are specified.

BUGS/ISSUES:

*******************************************************************************/

size_t fwrite(const void *ptr, size_t size, size_t nobj, FILE *stream)

{

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (0);

    return (vwrite(stream->fid, ptr, size*nobj)); /* process write */

}

/*******************************************************************************

FUNCTION NAME: fseek

SHORT DESCRIPTION: Seek to position in file

DETAILED DESCRIPTION:

Seeks to a given position in the indicated file. The seek modes, indicated by
the origin are:

SEEK_SET    Seek relative to beginning of file (0)
SEEK_CUR    Seek relative to current position in file
SEEK_END    Seek relative to end of file

The offset is a signed offset for the relative position in the file.

Return non-zero on error, otherwise zero.

BUGS/ISSUES:

*******************************************************************************/

int fseek(FILE *stream, long offset, int origin)

{

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (0);
    stream->flags &= ~_EFEOF; /* reset any EOF indication */

    return (!(vlseek(stream->fid, offset, origin) < 0)); /* process seek */

}

/*******************************************************************************

FUNCTION NAME: ftell

SHORT DESCRIPTION: Give current position in file

DETAILED DESCRIPTION:

Returns the current position in a file. Returns -1l for error.

BUGS/ISSUES:

*******************************************************************************/

long ftell(FILE *stream)

{

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (0);

    return (vlseek(stream->fid, 0, SEEK_CUR)); /* process seek */

}

/*******************************************************************************

FUNCTION NAME: rewind

SHORT DESCRIPTION: Rewind to file beginning

DETAILED DESCRIPTION:

Positions the given file to the start.

BUGS/ISSUES:

*******************************************************************************/

void rewind(FILE *stream)

{

    (void) fseek(stream, 0l, SEEK_SET); /* perform seek */
    clearerr(stream); /* clear any error */

}

/*******************************************************************************

FUNCTION NAME: fgetpos

SHORT DESCRIPTION: Get (mark) file position

DETAILED DESCRIPTION:

Gets the current file position into a sized pointer. The current file position
is placed into the given pointer. Returns non-zero on error, otherwise zero.

BUGS/ISSUES:

*******************************************************************************/

int fgetpos(FILE *stream, fpos_t *ptr)

{

    *ptr = ftell(stream); /* get current position */

    return (!!(*ptr < 0));

}

/*******************************************************************************

FUNCTION NAME: fsetpos

SHORT DESCRIPTION: Set marked file position

DETAILED DESCRIPTION:

Sets the current file position from a save marker pointer. Returns non-zero
on error, otherwise zero.

BUGS/ISSUES:

*******************************************************************************/

int fsetpos(FILE *stream, const fpos_t *ptr)

{

    return (fseek(stream, *ptr, SEEK_SET));

}

/*******************************************************************************

FUNCTION NAME: clearerr

SHORT DESCRIPTION: Clear errors on stream

DETAILED DESCRIPTION:

Clears any outstanding error indication on the given stream.

BUGS/ISSUES:

*******************************************************************************/

void clearerr(FILE *stream)

{

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return;
    stream->flags = 0; /* clear all flags */

}

/*******************************************************************************

FUNCTION NAME: feof

SHORT DESCRIPTION: Check end of file

DETAILED DESCRIPTION:

Checks if the end of file condition exists on a file. Returns non-zero if the
end of file is true for the stream.

BUGS/ISSUES:

*******************************************************************************/

int feof(FILE *stream)

{

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (0);
    return (!!(stream->flags &_EFEOF)); /* return EOF status */

}

/*******************************************************************************

FUNCTION NAME: ferror

SHORT DESCRIPTION: Check file has error

DETAILED DESCRIPTION:

Checks if the given stream is indicating an error. Returns non-zero if an
error is pending on the file, otherwise zero.

BUGS/ISSUES:

1. Currently there are no error indications kept.

*******************************************************************************/

int ferror(FILE *stream)

{

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) return (0);
    return (!!(stream->flags & ~_EFEOF));

}

/*******************************************************************************

FUNCTION NAME: perror

SHORT DESCRIPTION: Print error message

DETAILED DESCRIPTION:

Prints the given string as an error message, along with the error string for
the current errno value, on the standard error file.

BUGS/ISSUES:

*******************************************************************************/

void perror(const char *s)

{

    fprintf(stderr, "%s: %s\n", s, strerror(errno));

}

/*******************************************************************************

FUNCTION NAME: fileno

SHORT DESCRIPTION: Return integer descriptor for file

DETAILED DESCRIPTION:

Checks if the file is open, and if so returns the integer file id. Otherwise
returns -1
Checks if the given stream is indicating an error. Returns non-zero if an
error is pending on the file, otherwise zero.

BUGS/ISSUES:

1. Currently there are no error indications kept.

*******************************************************************************/

int fileno(FILE* stream)

{

    int r;

    /* check file is allocated and open */
    if (!stream || stream->fid < 0) {

        r = -1;
        errno = EBADF;

    } else r = stream->fid;

    return (r);

}

/*******************************************************************************

stdio shutdown

Unlike the init, we don't have to worry about shutdown race conditions (or as
much). We set ourselves for last priority.

The only shutdown task is to attempt to remove any temp files created, which is
non critical. If there were another program, that opened temp files, left them
open, and also set it's destructor to 101 (perform last), it's possible to
cause problems, IE., you would really have to try to make this fail.

You can remove the temp delete if you want to look at your temp files.

*******************************************************************************/

static void deinit_stdio (void) __attribute__((destructor (101)));
static void deinit_stdio()

{

    int i;

    for (i = 0; i < tmpnamc; i++) remove(tmpstr[i]);

}
