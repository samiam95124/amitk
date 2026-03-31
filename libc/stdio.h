/*******************************************************************************
*                                                                              *
*                            STANDARD I/O HEADER                               *
*                                                                              *
*                          COPYRIGHT 2007 (C) S. A. MOORE                      *
*                                                                              *
* FILE NAME: stdio.h                                                           *
*                                                                              *
* DESCRIPTION:                                                                 *
*                                                                              *
* Defines the serial I/O platform for whitebook C.                             *
*                                                                              *
* BUGS/ISSUES:                                                                 *
*                                                                              *
*******************************************************************************/

#ifndef _STDIO_H_
#define _STDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdarg.h>

/*
 * This block of defines will "coin" all of the plain labeled stdio calls and
 * make them stdio_ coined names. This allows the stio calls to exist in 
 * parallel with the built-in stio package.
 *
 * Some systems don't need this (Windows, Mac OS X) because they can override
 * at link time.
 */

#ifdef STDIO_BYPASS

/*
* Functions
*/
#define fopen       stdio_fopen
#define freopen     stdio_freopen
#define fdopen      stdio_fdopen
#define fflush      stdio_fflush
#define fclose      stdio_fclose
#define remove      stdio_remove
#define rename      stdio_rename
#define tmpfile     stdio_tmpfile
#define tmpnam      stdio_tmpnam
#define setvbuf     stdio_setvbuf
#define setbuf      stdio_setbuf
#define fprintf     stdio_fprintf
#define printf      stdio_printf
#define sprintf     stdio_sprintf
#define vprintf     stdio_vprintf
#define vfprintf    stdio_vfprintf
#define vsprintf    stdio_vsprintf
#define fscanf      stdio_fscanf
#define scanf       stdio_scanf
#define sscanf      stdio_sscanf
#define fgetc       stdio_fgetc
#define getc        stdio_getc
#define fgets       stdio_fgets
#define fputc       stdio_fputc
#define fputs       stdio_fputs
#define putc        stdio_putc
#define getchar     stdio_getchar
#define gets        stdio_gets
#define putc        stdio_putc
#define putchar     stdio_putchar
#define puts        stdio_puts
#define ungetc      stdio_ungetc
#define fread       stdio_fread
#define fwrite      stdio_fwrite
#define fseek       stdio_fseek
#define ftell       stdio_ftell
#define rewind      stdio_rewind
#define fgetpos     stdio_fgetpos
#define fsetpos     stdio_fsetpos
#define clearerr    stdio_clearerr
#define feof        stdio_feof
#define ferror      stdio_ferror
#define perror      stdio_perror
#define fileno      stdio_fileno

/*
 * Declarations
 */
#define FILE        STDIO_FILE
#define stdin       stdio_stdin
#define stdout      stdio_stdout
#define stderr      stdio_err

#endif

/* enable this next define for testing mode */

#define L_tmpnam 9
#define L_TMP_MAX 100
#define FOPEN_MAX 100

#define EOF (-1)

/* buffering modes */

#define _IOFBF 1 /* full buffering */
#define _IOLBF 2 /* line buffering */
#define _IONBF 3 /* no buffering */

#define BUFSIZ 512 /* standard buffer size */

/* this switch allows getc() and putc() to be macros. This is the way LIBC does
   it, and the ability is included here for possible compatability issues */
//#define USEMACRO 1 /* use macros for getc and putc */

typedef long fpos_t;

/* standard file descriptor */

typedef struct {

    int  fid;    /* file logical id, <0 means unused */
    char *name;  /* name holder for error/diagnostics */
    int  text;   /* text/binary mode flag */
    int  mode;   /* r/w mode, 0=read, 1=write, 2=read/write */
    int  append; /* append mode */
    int  pback;  /* pushback character, only a single is implemented */
    int  flags;  /* state flags, 0=EOF */

} FILE;

/* error/status flags */

#define _EFEOF 0x0001 /* stream EOF */

/* standard in, out and error files */

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE *fopen(const char* filename, const char* mode);
FILE *freopen(const char* filename, const char* mode, FILE* stream);
FILE *fdopen(int fd, const char *mode);
int fflush(FILE* stream);
int fclose(FILE* stream);
int remove(const char *filename);
int rename(const char *oldname, const char *newname);
FILE *tmpfile(void);
char *tmpnam(char s[]);
int setvbuf(FILE* stream, char *buf, int mode, size_t size);
void setbuf(FILE* stream, char *buf);
int fprintf(FILE* stream, const char *format, ...);
int printf(const char* format, ...);
int sprintf(char* s, const char *format, ...);
int vprintf(const char* format, va_list arg);
int vfprintf(FILE* stream, const char *format, va_list arg);
int vsprintf(char* s, const char *format, va_list arg);
int fscanf(FILE* stream, const char *format, ...);
int scanf(const char* format, ...);
int sscanf(const char* s, const char *format, ...);
int fgetc(FILE *stream);
char *fgets(char *s, int n, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int getchar(void);
char *gets(char *s);
int putc(int c, FILE *stream);
int putchar(int c);
int puts(const char *s);
int ungetc(int c, FILE *stream);
size_t fread(void *ptr, size_t size, size_t nobj, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nobj, FILE *stream);
int fseek(FILE* stream, long offset, int origin);
long ftell(FILE* stream);
void rewind(FILE* stream);
int fgetpos(FILE* stream, fpos_t *ptr);
int fsetpos(FILE* stream, const fpos_t *ptr);
void clearerr(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void perror(const char *s);
int fileno(FILE* stream);

#ifdef __cplusplus
}
#endif

#endif /* stdio.h */
