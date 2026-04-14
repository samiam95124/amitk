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
 *
 * These are function-like macros so bare identifiers (format-attribute
 * arguments like `__attribute__((format(printf, 2, 3)))` in third-party
 * headers, address-of expressions, etc.) are left alone — only actual
 * `name(...)` calls get rewritten to `stdio_name(...)`.
 */
#define fopen(...)    stdio_fopen(__VA_ARGS__)
#define freopen(...)  stdio_freopen(__VA_ARGS__)
#define fdopen(...)   stdio_fdopen(__VA_ARGS__)
#define fflush(...)   stdio_fflush(__VA_ARGS__)
#define fclose(...)   stdio_fclose(__VA_ARGS__)
#define remove(...)   stdio_remove(__VA_ARGS__)
#define rename(...)   stdio_rename(__VA_ARGS__)
#define tmpfile(...)  stdio_tmpfile(__VA_ARGS__)
#define tmpnam(...)   stdio_tmpnam(__VA_ARGS__)
#define setvbuf(...)  stdio_setvbuf(__VA_ARGS__)
#define setbuf(...)   stdio_setbuf(__VA_ARGS__)
#define fprintf(...)  stdio_fprintf(__VA_ARGS__)
#define printf(...)   stdio_printf(__VA_ARGS__)
#define sprintf(...)  stdio_sprintf(__VA_ARGS__)
#define vprintf(...)  stdio_vprintf(__VA_ARGS__)
#define vfprintf(...) stdio_vfprintf(__VA_ARGS__)
#define vsprintf(...) stdio_vsprintf(__VA_ARGS__)
#define fscanf(...)   stdio_fscanf(__VA_ARGS__)
#define scanf(...)    stdio_scanf(__VA_ARGS__)
#define sscanf(...)   stdio_sscanf(__VA_ARGS__)
#define fgetc(...)    stdio_fgetc(__VA_ARGS__)
#define getc(...)     stdio_getc(__VA_ARGS__)
#define fgets(...)    stdio_fgets(__VA_ARGS__)
#define fputc(...)    stdio_fputc(__VA_ARGS__)
#define fputs(...)    stdio_fputs(__VA_ARGS__)
#define putc(...)     stdio_putc(__VA_ARGS__)
#define getchar(...)  stdio_getchar(__VA_ARGS__)
#define gets(...)     stdio_gets(__VA_ARGS__)
#define putchar(...)  stdio_putchar(__VA_ARGS__)
#define puts(...)     stdio_puts(__VA_ARGS__)
#define ungetc(...)   stdio_ungetc(__VA_ARGS__)
#define fread(...)    stdio_fread(__VA_ARGS__)
#define fwrite(...)   stdio_fwrite(__VA_ARGS__)
#define fseek(...)    stdio_fseek(__VA_ARGS__)
#define ftell(...)    stdio_ftell(__VA_ARGS__)
#define rewind(...)   stdio_rewind(__VA_ARGS__)
#define fgetpos(...)  stdio_fgetpos(__VA_ARGS__)
#define fsetpos(...)  stdio_fsetpos(__VA_ARGS__)
#define clearerr(...) stdio_clearerr(__VA_ARGS__)
#define feof(...)     stdio_feof(__VA_ARGS__)
#define ferror(...)   stdio_ferror(__VA_ARGS__)
#define perror(...)   stdio_perror(__VA_ARGS__)
#define fileno(...)   stdio_fileno(__VA_ARGS__)

/*
 * Type and global object-like renames (cannot be function-like since
 * they are not invoked).
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
int getc(FILE *stream);
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
