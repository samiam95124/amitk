/** **************************************************************************

\file

\brief Module stdio_test - standard I/O library test

Description

Exercises as many aspects of the standard I/O library as is practical in an
automated, non-interactive program. Each test prints its result followed by an
"s/b" (should be) line giving the correct value, so the output can be eyeballed
or diffed against a known good run.

The tests are grouped:

  1xx  Formatted output (printf/fprintf/sprintf/vsprintf) conversions.
  2xx  Formatted input (sscanf) conversions.
  3xx  File open/close/read/write and block I/O.
  4xx  File positioning (fseek/ftell/rewind/fgetpos/fsetpos).
  5xx  Character and line I/O, pushback, status and misc.
  6xx  Buffering.
  7xx  Temporary files.
  8xx  Direct FILE access (backdoor accessors).

Note that stdin based calls (scanf, getchar, gets) are not exercised, since this
test is meant to run without operator interaction.

Exports

int main(void) - Runs the test suite and returns non-zero if any test failed.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h> /* for SEEK_SET/SEEK_CUR/SEEK_END */

#include <localdefs.h> /* ami_long: the l modifier's width */

#define MAXSTR 100         /* general string buffer length */
#define TMPNAME "stdio_test.tmp" /* scratch file name */
#define TMPNAME2 "stdio_test2.tmp" /* second scratch file name */

/* count of failures detected by the self checking tests */
static int fails = 0;

/*******************************************************************************

Check a produced string against its expected value

Compares a result string to an expected string, and bumps the failure counter
if they do not match. Used by the formatted output tests, which are completely
deterministic and so can be checked by the program itself.

*******************************************************************************/

static void chks(
    /** test number */     int n,
    /** produced string */ const char *got,
    /** expected string */ const char *exp
)

{

    printf("test %d: \"%s\" s/b \"%s\"", n, got, exp);
    if (strcmp(got, exp) != 0) { printf("  *** FAIL ***"); fails++; }
    putchar('\n');

}

/*******************************************************************************

Check an integer against its expected value

*******************************************************************************/

static void chki(
    /** test number */    int n,
    /** produced value */ ami_long got,
    /** expected value */ ami_long exp
)

{

    printf("test %d: %lld s/b %lld", n, AMI_LONG_CAST(got), AMI_LONG_CAST(exp));
    if (got != exp) { printf("  *** FAIL ***"); fails++; }
    putchar('\n');

}

/* helper that funnels a variable argument list into vsprintf for test */
static void vtest(
    /** destination string */ char *s,
    /** format string */      const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap;

    va_start(ap, fmt);
    vsprintf(s, fmt, ap);
    va_end(ap);

}

/* helper that funnels a variable argument list into vsnprintf for test */
static int vntest(
    /** destination string */ char *s,
    /** buffer size limit */  size_t n,
    /** format string */      const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vsnprintf(s, n, fmt, ap);
    va_end(ap);

    return (r);

}

/* helper that funnels a variable argument list into vsscanf for test */
static int vsctest(
    /** input string */       const char *in,
    /** scan format string */ const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vsscanf(in, fmt, ap);
    va_end(ap);

    return (r);

}

/* helper that funnels a variable argument list into vfscanf for test */
static int vfsctest(
    /** input file */         FILE *fp,
    /** scan format string */ const char *fmt,
    /** variable arguments */ ...
)

{

    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vfscanf(fp, fmt, ap);
    va_end(ap);

    return (r);

}

/* Read the bytes actually present on disk for a file, via an independent
   stream. Because this is a separate descriptor, it sees only data that has
   been flushed out of any writer's buffer, which lets the buffering tests prove
   that unflushed data is genuinely held back. Returns the byte count and null
   terminates the result. */
static int diskread(
    /** name of the file to read */       const char *name,
    /** destination buffer */             char *out,
    /** size of the destination buffer */ int max
)

{

    FILE *f;     /* independent read stream */
    int   n = 0; /* byte count */

    f = fopen(name, "rb");
    if (f) { n = fread(out, 1, max-1, f); fclose(f); }
    if (n < 0) n = 0;
    out[n] = 0; /* null terminate */

    return (n);

}

/* "Backdoor" accessors, written against the GNU libc _IO_FILE field names just
   as gnulib modules (freadahead, __fpending, freadable, ...) do, but operating
   on a petit_ami FILE. These confirm the structure is laid out and maintained
   the way such code expects. They are guarded by _IO_NO_READS, which the
   petit_ami stdio.h defines but the public glibc headers do not, so this test
   still compiles against stock glibc (where Group 8 is simply skipped). */
#ifdef _IO_NO_READS
static int bd_freadahead(
    /** file to query */ FILE *fp
)
{
    if (fp->_IO_write_ptr > fp->_IO_write_base) return (0);
    return (fp->_IO_read_end-fp->_IO_read_ptr);
}
static int bd_fpending(
    /** file to query */ FILE *fp
)  { return (fp->_IO_write_ptr-fp->_IO_write_base); }
static int bd_freadable(
    /** file to query */ FILE *fp
) { return ((fp->_flags & _IO_NO_READS) == 0); }
static int bd_fwritable(
    /** file to query */ FILE *fp
) { return ((fp->_flags & _IO_NO_WRITES) == 0); }
static void bd_fseterr(
    /** file to mark in error */ FILE *fp
)  { fp->_flags |= _IO_ERR_SEEN; }
#endif

int main(void)

{

    char   s[MAXSTR];      /* general purpose string */
    char   s2[MAXSTR];     /* second string */
    int    i, j, k;        /* integer scan targets */
    unsigned u;            /* unsigned scan target */
    float  f;              /* float scan target */
    double d;              /* double scan target */
    char   c;              /* character scan target */
    int    n;              /* return value/count holder */
    FILE   *fp;            /* file pointer */
    ami_long pos;          /* file position */
    fpos_t fpos;           /* file position record */
    char   buf[64];        /* block I/O buffer */

    printf("Stdio library test v1.0\n\n");

    /***************************************************************************

    Group 1: formatted output conversions

    ***************************************************************************/

    printf("Group 1: formatted output (printf family)\n\n");

    /* integer conversions */
    sprintf(s, "%d", 12345);            chks(100, s, "12345");
    sprintf(s, "%d", -12345);           chks(101, s, "-12345");
    sprintf(s, "%i", 42);               chks(102, s, "42");
    sprintf(s, "%u", 4000000000U);      chks(103, s, "4000000000");
    sprintf(s, "%o", 64);               chks(104, s, "100");
    sprintf(s, "%x", 255);              chks(105, s, "ff");
    sprintf(s, "%X", 255);              chks(106, s, "FF");
    sprintf(s, "%#x", 255);             chks(107, s, "0xff");
    sprintf(s, "%#o", 64);              chks(108, s, "0100");

    /* field width, justification and padding */
    sprintf(s, "%5d", 42);              chks(110, s, "   42");
    sprintf(s, "%-5d|", 42);            chks(111, s, "42   |");
    sprintf(s, "%05d", 42);             chks(112, s, "00042");
    sprintf(s, "%+d", 42);              chks(113, s, "+42");
    sprintf(s, "% d", 42);              chks(114, s, " 42");
    sprintf(s, "%8.3d", 5);             chks(115, s, "     005");
    sprintf(s, "%*d", 6, 42);           chks(116, s, "    42");
    sprintf(s, "%.*d", 4, 42);          chks(117, s, "0042");

    /* character and string */
    sprintf(s, "%c", 'A');              chks(120, s, "A");
    sprintf(s, "%5c|", 'A');            chks(121, s, "    A|");
    sprintf(s, "%s", "hello");          chks(122, s, "hello");
    sprintf(s, "%8s|", "hi");           chks(123, s, "      hi|");
    sprintf(s, "%-8s|", "hi");          chks(124, s, "hi      |");
    sprintf(s, "%.3s", "truncated");    chks(125, s, "tru");
    { char *np = NULL; sprintf(s, "[%s]", np); } chks(126, s, "[(null)]");

    /* multiple arguments and literal percent */
    sprintf(s, "%d%% of %s", 50, "x");  chks(130, s, "50% of x");
    sprintf(s, "%d,%d,%d", 1, 2, 3);    chks(131, s, "1,2,3");

    /* integer length modifiers (l, ll, z, h), exercising the full width;
       l is the C long, 32 bits on some hosts, so its values fit that */
    sprintf(s, "%ld", 2000000000L);            chks(132, s, "2000000000");
    sprintf(s, "%lld", 123456789012345LL);     chks(133, s, "123456789012345");
    sprintf(s, "%lu", 4000000000UL);           chks(134, s, "4000000000");
    sprintf(s, "%llx", 0xDEADBEEFCAFEULL);     chks(135, s, "deadbeefcafe");
    sprintf(s, "%zu", (size_t)4000000000U);    chks(136, s, "4000000000");
    sprintf(s, "%hd", (short)-7);              chks(137, s, "-7");
    /* a wide argument must not disturb the ones that follow it */
    sprintf(s, "%ld then %d", 2000000000L, 42); chks(138, s, "2000000000 then 42");

    /* floating point */
    sprintf(s, "%f", 3.14159);          chks(140, s, "3.141590");
    sprintf(s, "%.2f", 3.14159);        chks(141, s, "3.14");
    sprintf(s, "%.0f", 2.4);            chks(142, s, "2");
    sprintf(s, "%.0f", 2.6);            chks(143, s, "3");
    sprintf(s, "%+.2f", 3.14159);       chks(144, s, "+3.14");
    sprintf(s, "%8.2f", 3.14159);       chks(145, s, "    3.14");
    sprintf(s, "%-8.2f|", 3.14159);     chks(146, s, "3.14    |");
    sprintf(s, "%08.2f", -3.14159);     chks(147, s, "-0003.14");
    sprintf(s, "%e", 31415.9);          chks(148, s, "3.141590e+04");
    sprintf(s, "%.2E", 0.00031415);     chks(149, s, "3.14E-04");
    sprintf(s, "%g", 0.0001);           chks(150, s, "0.0001");
    sprintf(s, "%g", 0.00001);          chks(151, s, "1e-05");
    sprintf(s, "%g", 100000.0);         chks(152, s, "100000");
    sprintf(s, "%g", 1000000.0);        chks(153, s, "1e+06");
    sprintf(s, "%.1f", 9.99);           chks(154, s, "10.0");

    /* sprintf return value is the count of characters placed */
    n = sprintf(s, "abc%d", 99);        chki(160, n, 5);

    /* vsprintf through a va_list */
    vtest(s, "%d-%s-%c", 7, "mid", 'Z'); chks(161, s, "7-mid-Z");

    /* snprintf: full output fits */
    n = snprintf(s, sizeof(s), "%d-%s", 42, "hi");
    printf("test 163: \"%s\" n=%d s/b \"42-hi\" 5\n", s, n);
    if (strcmp(s, "42-hi") || n != 5) fails++;

    /* snprintf: output truncated to the size, always terminated, returns the
       length that would have been written. The truncation is the test, so
       the compiler's truncation warning does not apply. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    n = snprintf(s, 4, "%d-%s", 42, "hi");
#pragma GCC diagnostic pop
    printf("test 164: \"%s\" n=%d s/b \"42-\" 5\n", s, n);
    if (strcmp(s, "42-") || n != 5) fails++;

    /* snprintf: size zero writes nothing but still returns the length */
    n = snprintf(NULL, 0, "abc%d", 99);
    printf("test 165: n=%d s/b 5 (measure with size 0)\n", n);
    if (n != 5) fails++;

    /* vsnprintf through a va_list */
    n = vntest(s, sizeof(s), "%05d", 7);
    printf("test 166: \"%s\" n=%d s/b \"00007\" 5\n", s, n);
    if (strcmp(s, "00007") || n != 5) fails++;

    /* fprintf to stdout (visual) */
    printf("test 162: ");
    fprintf(stdout, "%d %s %.1f", 1, "two", 3.0);
    printf(" s/b 1 two 3.0\n");

    /***************************************************************************

    Group 2: formatted input conversions

    ***************************************************************************/

    printf("\nGroup 2: formatted input (sscanf family)\n\n");

    n = sscanf("123", "%d", &i);
    printf("test 200: n=%d i=%d s/b n=1 i=123\n", n, i);
    if (n != 1 || i != 123) fails++;

    n = sscanf("-45 67", "%d %d", &i, &j);
    printf("test 201: n=%d i=%d j=%d s/b n=2 i=-45 j=67\n", n, i, j);
    if (n != 2 || i != -45 || j != 67) fails++;

    n = sscanf("ff", "%x", &u);
    printf("test 202: n=%d u=%u s/b n=1 u=255\n", n, u);
    if (n != 1 || u != 255) fails++;

    n = sscanf("777", "%o", &u);
    printf("test 203: n=%d u=%u s/b n=1 u=511\n", n, u);
    if (n != 1 || u != 511) fails++;

    n = sscanf("0x1A", "%i", &i);
    printf("test 204: n=%d i=%d s/b n=1 i=26\n", n, i);
    if (n != 1 || i != 26) fails++;

    n = sscanf("hello world", "%s", s);
    printf("test 205: n=%d s=\"%s\" s/b n=1 s=\"hello\"\n", n, s);
    if (n != 1 || strcmp(s, "hello")) fails++;

    n = sscanf("abcdef", "%3s", s);
    printf("test 206: n=%d s=\"%s\" s/b n=1 s=\"abc\"\n", n, s);
    if (n != 1 || strcmp(s, "abc")) fails++;

    n = sscanf("Q", "%c", &c);
    printf("test 207: n=%d c=%c s/b n=1 c=Q\n", n, c);
    if (n != 1 || c != 'Q') fails++;

    n = sscanf("3.14159", "%f", &f);
    sprintf(s, "%.5f", f);
    printf("test 208: n=%d f=%s s/b n=1 f=3.14159\n", n, s);
    if (n != 1 || strcmp(s, "3.14159")) fails++;

    n = sscanf("-2.5e3", "%lf", &d);
    sprintf(s, "%.1f", d);
    printf("test 209: n=%d d=%s s/b n=1 d=-2500.0\n", n, s);
    if (n != 1 || strcmp(s, "-2500.0")) fails++;

    n = sscanf("12 34 56", "%d %*d %d", &i, &j);
    printf("test 210: n=%d i=%d j=%d s/b n=2 i=12 j=56\n", n, i, j);
    if (n != 2 || i != 12 || j != 56) fails++;

    n = sscanf("abc123", "%[a-z]%d", s, &i);
    printf("test 211: n=%d s=\"%s\" i=%d s/b n=2 s=\"abc\" i=123\n", n, s, i);
    if (n != 2 || strcmp(s, "abc") || i != 123) fails++;

    n = sscanf("key=value", "%[^=]=%s", s, s2);
    printf("test 212: n=%d s=\"%s\" s2=\"%s\" s/b n=2 s=\"key\" s2=\"value\"\n",
           n, s, s2);
    if (n != 2 || strcmp(s, "key") || strcmp(s2, "value")) fails++;

    n = sscanf("42abc", "%d%n", &i, &k);
    printf("test 213: n=%d i=%d k=%d s/b n=1 i=42 k=2\n", n, i, k);
    if (n != 1 || i != 42 || k != 2) fails++;

    /* integer length modifiers on input, storing through wide pointers */
    {
        long      lo;  /* long target */
        long long llo; /* long long target */
        short     sh;  /* short target */
        size_t    z;   /* size_t target */

        n = sscanf("2000000000", "%ld", &lo);
        sprintf(s, "%ld", lo);
        printf("test 214: n=%d v=%s s/b n=1 v=2000000000\n", n, s);
        if (n != 1 || strcmp(s, "2000000000")) fails++;

        n = sscanf("123456789012345", "%lld", &llo);
        sprintf(s, "%lld", llo);
        printf("test 215: n=%d v=%s s/b n=1 v=123456789012345\n", n, s);
        if (n != 1 || strcmp(s, "123456789012345")) fails++;

        sh = 0;
        n = sscanf("-7", "%hd", &sh);
        printf("test 216: n=%d v=%d s/b n=1 v=-7\n", n, (int)sh);
        if (n != 1 || sh != -7) fails++;

        n = sscanf("4000000000", "%zu", &z);
        sprintf(s, "%zu", z);
        printf("test 217: n=%d v=%s s/b n=1 v=4000000000\n", n, s);
        if (n != 1 || strcmp(s, "4000000000")) fails++;
    }

    /* vsscanf through a va_list */
    n = vsctest("3 4", "%d %d", &i, &j);
    printf("test 218: n=%d i=%d j=%d s/b n=2 i=3 j=4\n", n, i, j);
    if (n != 2 || i != 3 || j != 4) fails++;

    /* vfscanf through a va_list, reading from a file */
    fp = fopen(TMPNAME, "w");
    if (fp) { fputs("55 hi", fp); fclose(fp); }
    fp = fopen(TMPNAME, "r");
    if (fp) {

        n = vfsctest(fp, "%d %s", &i, s);
        printf("test 219: n=%d i=%d s=\"%s\" s/b n=2 i=55 s=\"hi\"\n", n, i, s);
        if (n != 2 || i != 55 || strcmp(s, "hi")) fails++;
        fclose(fp);

    }
    remove(TMPNAME);

    /***************************************************************************

    Group 3: file open/close and block I/O

    ***************************************************************************/

    printf("\nGroup 3: file open/close and block I/O\n\n");

    /* write a file with fputs and fprintf */
    fp = fopen(TMPNAME, "w");
    printf("test 300: %d s/b 1 (fopen for write)\n", fp != NULL);
    if (!fp) fails++;
    else {

        fputs("line one\n", fp);
        fprintf(fp, "value=%d\n", 42);
        fputc('X', fp);
        n = fclose(fp);
        printf("test 301: %d s/b 0 (fclose)\n", n);
        if (n) fails++;

    }

    /* read it back with fgets and fgetc */
    fp = fopen(TMPNAME, "r");
    if (!fp) { printf("test 302: could not reopen file *** FAIL ***\n"); fails++; }
    else {

        fgets(s, MAXSTR, fp);
        chks(302, s, "line one\n");
        fgets(s, MAXSTR, fp);
        chks(303, s, "value=42\n");
        i = fgetc(fp);
        printf("test 304: %c s/b X (fgetc)\n", i);
        if (i != 'X') fails++;
        i = fgetc(fp);
        printf("test 305: %d s/b -1 (EOF at end)\n", i);
        if (i != EOF) fails++;
        printf("test 306: %d s/b 1 (feof set)\n", feof(fp) != 0);
        if (!feof(fp)) fails++;
        fclose(fp);

    }

    /* read the file back with fscanf */
    fp = fopen(TMPNAME, "r");
    if (fp) {

        fscanf(fp, "%s %s", s, s2);
        printf("test 307: \"%s\" \"%s\" s/b \"line\" \"one\"\n", s, s2);
        if (strcmp(s, "line") || strcmp(s2, "one")) fails++;
        fscanf(fp, "%s", s);
        n = sscanf(s, "value=%d", &i);
        printf("test 308: i=%d s/b 42\n", i);
        if (i != 42) fails++;
        fclose(fp);

    }

    /* block I/O with fwrite/fread */
    fp = fopen(TMPNAME, "wb");
    if (fp) {

        n = fwrite("ABCDEFGH", 1, 8, fp);
        printf("test 310: %d s/b 8 (fwrite count)\n", n);
        if (n != 8) fails++;
        fclose(fp);

    }
    fp = fopen(TMPNAME, "rb");
    if (fp) {

        memset(buf, 0, sizeof(buf));
        n = fread(buf, 1, 8, fp);
        printf("test 311: %d \"%s\" s/b 8 \"ABCDEFGH\"\n", n, buf);
        if (n != 8 || strcmp(buf, "ABCDEFGH")) fails++;
        /* fread of multi byte objects */
        rewind(fp);
        memset(buf, 0, sizeof(buf));
        n = fread(buf, 4, 2, fp);
        printf("test 312: %d \"%s\" s/b 2 \"ABCDEFGH\"\n", n, buf);
        if (n != 2 || strcmp(buf, "ABCDEFGH")) fails++;
        fclose(fp);

    }

    /* rename a file to a new name, then verify content survived the move */
    remove(TMPNAME2); /* ensure target does not exist */
    fp = fopen(TMPNAME, "w");
    if (fp) { fputs("moved", fp); fclose(fp); }
    n = rename(TMPNAME, TMPNAME2);
    printf("test 313: %d s/b 0 (rename succeeds)\n", n);
    if (n) fails++;
    fp = fopen(TMPNAME, "r");
    printf("test 314: %d s/b 1 (old name gone)\n", fp == NULL);
    if (fp) { fails++; fclose(fp); }
    fp = fopen(TMPNAME2, "r");
    if (fp) { s[0] = 0; fgets(s, MAXSTR, fp); fclose(fp); } else s[0] = 0;
    chks(315, s, "moved");

    /* rename onto an existing target replaces it */
    fp = fopen(TMPNAME, "w");
    if (fp) { fputs("fresh", fp); fclose(fp); }
    n = rename(TMPNAME, TMPNAME2); /* TMPNAME2 already exists */
    printf("test 316: %d s/b 0 (rename replaces target)\n", n);
    if (n) fails++;
    fp = fopen(TMPNAME2, "r");
    if (fp) { s[0] = 0; fgets(s, MAXSTR, fp); fclose(fp); } else s[0] = 0;
    chks(317, s, "fresh");

    /* renaming a nonexistent file is an error */
    remove(TMPNAME);
    n = rename(TMPNAME, TMPNAME2);
    printf("test 318: %d s/b 1 (rename of missing file fails)\n", n != 0);
    if (n == 0) fails++;
    remove(TMPNAME2);

    /***************************************************************************

    Group 4: file positioning

    ***************************************************************************/

    printf("\nGroup 4: file positioning\n\n");

    fp = fopen(TMPNAME, "wb");
    if (fp) { fwrite("0123456789", 1, 10, fp); fclose(fp); }
    fp = fopen(TMPNAME, "rb");
    if (fp) {

        fseek(fp, 5, SEEK_SET);
        pos = ftell(fp);
        printf("test 400: %lld s/b 5 (ftell after seek)\n", AMI_LONG_CAST(pos));
        if (pos != 5) fails++;
        i = fgetc(fp);
        printf("test 401: %c s/b 5 (char at offset 5)\n", i);
        if (i != '5') fails++;

        fseek(fp, 2, SEEK_CUR);
        i = fgetc(fp);
        printf("test 402: %c s/b 8 (char after relative seek)\n", i);
        if (i != '8') fails++;

        fseek(fp, -3, SEEK_END);
        i = fgetc(fp);
        printf("test 403: %c s/b 7 (char from end seek)\n", i);
        if (i != '7') fails++;

        rewind(fp);
        pos = ftell(fp);
        printf("test 404: %lld s/b 0 (ftell after rewind)\n", AMI_LONG_CAST(pos));
        if (pos != 0) fails++;

        fseek(fp, 4, SEEK_SET);
        fgetpos(fp, &fpos);
        fseek(fp, 0, SEEK_SET);
        fsetpos(fp, &fpos);
        i = fgetc(fp);
        printf("test 405: %c s/b 4 (char after fsetpos)\n", i);
        if (i != '4') fails++;

        fclose(fp);

    }

    /***************************************************************************

    Group 5: character/line I/O, pushback, status, misc

    ***************************************************************************/

    printf("\nGroup 5: character I/O, pushback, status and misc\n\n");

    /* ungetc pushes a character back for the next read */
    fp = fopen(TMPNAME, "rb");
    if (fp) {

        i = fgetc(fp);
        ungetc(i, fp);
        j = fgetc(fp);
        printf("test 500: %c %c s/b 0 0 (ungetc replays char)\n", i, j);
        if (i != '0' || j != '0') fails++;

        /* ungetc of an arbitrary character */
        ungetc('Z', fp);
        j = fgetc(fp);
        printf("test 501: %c s/b Z (ungetc arbitrary)\n", j);
        if (j != 'Z') fails++;
        fclose(fp);

    }

    /* getc/putc are the macro forms of fgetc/fputc */
    fp = fopen(TMPNAME, "wb");
    if (fp) { putc('Q', fp); fclose(fp); }
    fp = fopen(TMPNAME, "rb");
    if (fp) {

        i = getc(fp);
        printf("test 502: %c s/b Q (putc/getc)\n", i);
        if (i != 'Q') fails++;
        fclose(fp);

    }

    /* clearerr resets the end of file indicator */
    fp = fopen(TMPNAME, "rb");
    if (fp) {

        while (fgetc(fp) != EOF); /* run to end */
        printf("test 503: %d s/b 1 (feof before clear)\n", feof(fp) != 0);
        if (!feof(fp)) fails++;
        clearerr(fp);
        printf("test 504: %d s/b 0 (feof after clearerr)\n", feof(fp) != 0);
        if (feof(fp)) fails++;
        printf("test 505: %d s/b 0 (ferror clean)\n", ferror(fp) != 0);
        if (ferror(fp)) fails++;
        fclose(fp);

    }

    /* fileno returns the underlying descriptor */
    printf("test 506: %d s/b 1 (fileno of stdout)\n", fileno(stdout));
    if (fileno(stdout) != 1) fails++;

    /* freopen redirects an existing stream to a new file */
    fp = fopen(TMPNAME, "wb");
    if (fp) {

        fputs("alpha", fp);
        fp = freopen(TMPNAME, "rb", fp);
        if (fp) {

            fgets(s, MAXSTR, fp);
            chks(507, s, "alpha");
            fclose(fp);

        } else { printf("test 507: freopen failed *** FAIL ***\n"); fails++; }

    }

    /* remove deletes the file */
    n = remove(TMPNAME);
    printf("test 508: %d s/b 0 (remove)\n", n);
    if (n) fails++;
    fp = fopen(TMPNAME, "rb");
    printf("test 509: %d s/b 1 (file gone after remove)\n", fp == NULL);
    if (fp) { fails++; fclose(fp); }

    /***************************************************************************

    Group 6: buffering

    These tests use a second, independent stream (diskread) to observe what has
    actually reached the file, proving that buffered data is held back until a
    flush and that unbuffered data is written through immediately.

    ***************************************************************************/

    printf("\nGroup 6: buffering\n\n");

    {
        char dbuf[2200]; /* disk content holder */
        char ubuf[64];   /* user supplied buffer */
        char big[2000];  /* large I/O payload */
        int  m;          /* byte count */

        /* full buffering: written data is held until flushed */
        fp = fopen(TMPNAME, "wb"); /* files default to full buffering */
        if (fp) {

            fputs("hello", fp); /* goes into the buffer, not the file */
            m = diskread(TMPNAME, dbuf, sizeof(dbuf));
            printf("test 600: %d s/b 0 (full buffered, nothing on disk yet)\n", m);
            if (m != 0) fails++;
            fflush(fp); /* now force it out */
            m = diskread(TMPNAME, dbuf, sizeof(dbuf));
            printf("test 601: %d \"%s\" s/b 5 \"hello\" (visible after flush)\n",
                   m, dbuf);
            if (m != 5 || strcmp(dbuf, "hello")) fails++;
            fclose(fp);

        }

        /* fclose flushes automatically */
        fp = fopen(TMPNAME, "wb");
        if (fp) { fputs("bye", fp); fclose(fp); }
        m = diskread(TMPNAME, dbuf, sizeof(dbuf));
        printf("test 602: %d \"%s\" s/b 3 \"bye\" (fclose flushed)\n", m, dbuf);
        if (m != 3 || strcmp(dbuf, "bye")) fails++;

        /* unbuffered: data is written through immediately */
        fp = fopen(TMPNAME, "wb");
        if (fp) {

            setvbuf(fp, NULL, _IONBF, 0); /* turn buffering off */
            fputs("now", fp); /* should hit the file at once */
            m = diskread(TMPNAME, dbuf, sizeof(dbuf));
            printf("test 603: %d \"%s\" s/b 3 \"now\" (unbuffered, immediate)\n",
                   m, dbuf);
            if (m != 3 || strcmp(dbuf, "now")) fails++;
            fclose(fp);

        }

        /* line buffering: flush occurs at a newline */
        fp = fopen(TMPNAME, "wb");
        if (fp) {

            setvbuf(fp, NULL, _IOLBF, 0); /* line buffering */
            fputs("ab", fp); /* no newline, stays buffered */
            m = diskread(TMPNAME, dbuf, sizeof(dbuf));
            printf("test 604: %d s/b 0 (line buffered, no newline yet)\n", m);
            if (m != 0) fails++;
            fputs("c\n", fp); /* newline triggers a flush */
            m = diskread(TMPNAME, dbuf, sizeof(dbuf));
            printf("test 605: %d \"%s\" s/b 4 \"abc\\n\" (flushed at newline)\n",
                   m, dbuf);
            if (m != 4 || strcmp(dbuf, "abc\n")) fails++;
            fclose(fp);

        }

        /* a caller supplied buffer */
        fp = fopen(TMPNAME, "wb");
        if (fp) {

            n = setvbuf(fp, ubuf, _IOFBF, sizeof(ubuf));
            printf("test 606: %d s/b 0 (setvbuf user buffer)\n", n);
            if (n) fails++;
            fputs("user buffered data", fp);
            fclose(fp); /* flushes through the user buffer */
            m = diskread(TMPNAME, dbuf, sizeof(dbuf));
            chks(607, dbuf, "user buffered data");

        }

        /* large write and read, exceeding the buffer size, exercising the bulk
           path that bypasses the buffer */
        for (i = 0; i < (int)sizeof(big); i++) big[i] = 'A'+(i%26);
        fp = fopen(TMPNAME, "wb");
        if (fp) {

            n = fwrite(big, 1, sizeof(big), fp);
            printf("test 608: %d s/b %d (large fwrite count)\n", n,
                   (int)sizeof(big));
            if (n != (int)sizeof(big)) fails++;
            fclose(fp);

        }
        fp = fopen(TMPNAME, "rb");
        if (fp) {

            memset(dbuf, 0, sizeof(dbuf));
            n = fread(dbuf, 1, sizeof(big), fp);
            printf("test 609: %d s/b %d (large fread count)\n", n,
                   (int)sizeof(big));
            if (n != (int)sizeof(big) || memcmp(dbuf, big, sizeof(big)))
                fails++;
            fclose(fp);

        }

        /* read/write switching on an update stream, with seeks between */
        fp = fopen(TMPNAME, "w+b");
        if (fp) {

            fwrite("0123456789", 1, 10, fp); /* buffered write */
            fseek(fp, 3, SEEK_SET); /* flushes write, then seeks */
            i = fgetc(fp); /* switches to reading */
            printf("test 610: %c s/b 3 (read after buffered write)\n", i);
            if (i != '3') fails++;
            fseek(fp, 0, SEEK_SET); /* discard read ahead */
            fputc('X', fp); /* switch back to writing */
            fseek(fp, 0, SEEK_SET); /* flush the write */
            i = fgetc(fp);
            printf("test 611: %c s/b X (write after read)\n", i);
            if (i != 'X') fails++;
            fclose(fp);

        }

        remove(TMPNAME);

    }

    /***************************************************************************

    Group 7: temporary files

    ***************************************************************************/

    printf("\nGroup 7: temporary files\n\n");

    {
        char t1[L_tmpnam]; /* first temp name */
        char t2[L_tmpnam]; /* second temp name */
        char *tp;          /* internal temp name */

        /* successive names are distinct */
        tmpnam(t1);
        tmpnam(t2);
        printf("test 700: \"%s\" \"%s\" distinct=%d s/b 1\n", t1, t2,
               strcmp(t1, t2) != 0);
        if (!strcmp(t1, t2)) fails++;

        /* the generated name does not already exist */
        printf("test 701: %d s/b 1 (name does not pre-exist)\n",
               access(t1, F_OK) != 0);
        if (access(t1, F_OK) == 0) fails++;

        /* the name fits the L_tmpnam buffer */
        printf("test 702: %d s/b 1 (length %d < L_tmpnam %d)\n",
               (int)strlen(t1) < L_tmpnam, (int)strlen(t1), L_tmpnam);
        if ((int)strlen(t1) >= L_tmpnam) fails++;

        /* tmpnam(NULL) returns its internal buffer */
        tp = tmpnam(NULL);
        printf("test 703: %d s/b 1 (tmpnam(NULL) returns a name)\n", tp != NULL);
        if (!tp) fails++;

        /* tmpfile opens a working read/write temporary */
        fp = tmpfile();
        printf("test 704: %d s/b 1 (tmpfile opens)\n", fp != NULL);
        if (!fp) fails++;
        else {

            fputs("scratch data", fp); /* write to it */
            rewind(fp); /* back to the start */
            s[0] = 0; fgets(s, MAXSTR, fp); /* read it back */
            chks(705, s, "scratch data");
            fclose(fp); /* file is removed automatically */

        }

    }

    /***************************************************************************

    Group 8: direct FILE access (backdoor accessors)

    Confirms the FILE structure is laid out and maintained with glibc _IO_FILE
    semantics, so code that reaches into the structure directly (gnulib style
    freadahead, __fpending, freadable, fwritable, fseterr) gets correct results.

    ***************************************************************************/

    printf("\nGroup 8: direct FILE access (backdoor accessors)\n\n");

#ifndef _IO_NO_READS
    printf("(skipped: not built against petit_ami stdio)\n");
#else
    fp = fopen(TMPNAME, "wb+");
    if (fp) {

        fwrite("0123456789ABCDEF", 1, 16, fp); /* fill */
        fflush(fp);
        printf("test 800: %d s/b 0 (fpending after flush)\n", bd_fpending(fp));
        if (bd_fpending(fp) != 0) fails++;

        fwrite("XYZ", 1, 3, fp); /* 3 bytes buffered, unflushed */
        printf("test 801: %d s/b 3 (fpending tracks pending writes)\n",
               bd_fpending(fp));
        if (bd_fpending(fp) != 3) fails++;

        printf("test 802: %d s/b 1 (fwritable on wb+)\n", bd_fwritable(fp));
        if (!bd_fwritable(fp)) fails++;

        fflush(fp);
        fseek(fp, 0, SEEK_SET);
        fgetc(fp); /* triggers a buffered refill, leaving read ahead */
        n = bd_freadahead(fp); /* unread bytes in the buffer */
        printf("test 803: %d s/b >0 (freadahead reports read ahead)\n", n);
        if (n <= 0) fails++;

        /* draining exactly freadahead() more bytes reaches the buffer end */
        k = 0;
        while (k < n && fgetc(fp) != EOF) k++;
        printf("test 804: %d s/b %d (freadahead == bytes actually buffered)\n",
               k, n);
        if (k != n) fails++;

        bd_fseterr(fp); /* set the error bit directly */
        printf("test 805: %d s/b 1 (ferror sees a directly set error)\n",
               ferror(fp) != 0);
        if (!ferror(fp)) fails++;

        /* The raw descriptor number depends on what else the program
           has open (the dynamic build's always-resident sound holds one
           more than the static build's), so the printed result is the
           comparison, not the numbers. */
        printf("test 806: %d s/b 1 (_fileno matches fileno())\n",
               fp->_fileno == fileno(fp));
        if (fp->_fileno != fileno(fp)) fails++;

        fclose(fp);

    }

    /* a read only stream reports not writable */
    fp = fopen(TMPNAME, "rb");
    if (fp) {

        printf("test 807: %d s/b 0 (fwritable on rb)\n", bd_fwritable(fp));
        if (bd_fwritable(fp)) fails++;
        printf("test 808: %d s/b 1 (freadable on rb)\n", bd_freadable(fp));
        if (!bd_freadable(fp)) fails++;
        fclose(fp);

    }
    remove(TMPNAME);
#endif

    /***************************************************************************

    Summary

    ***************************************************************************/

    printf("\n");
    if (fails) printf("*** %d self checking tests FAILED ***\n", fails);
    else printf("All self checking tests passed.\n");

    return (fails ? 1 : 0);

}
