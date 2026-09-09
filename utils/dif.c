/*******************************************************************************
*                                                                              *
*                          DIF - File Difference Utility                       *
*                                                                              *
* Compares two text files and outputs the minimum differences in standard      *
* diff format. Handles both Unix and Windows line endings transparently.       *
*                                                                              *
* This is a C port of the Pascal-P6 utility of the same name, and produces     *
* the same output. Its reason to exist is the -q option, which treats a '?'    *
* in either file as a wildcard matching any single character. Regression       *
* compare files use that to mask fields that legitimately change from run     *
* to run, such as times, dates and file sizes.                                 *
*                                                                              *
* Usage:                                                                       *
*                                                                              *
* dif [-w|-nw] [-q|-nq] file1 file2                                            *
*                                                                              *
* Options:                                                                     *
*                                                                              *
* -w   Ignore whitespace                                                       *
* -nw  Exact comparison (default)                                              *
* -q   Treat '?' as a wildcard matching any single character, and '*' as a     *
*      wildcard matching any run of characters (including none). Both are      *
*      honored from either file. With no '*' the lines must be the same        *
*      length to match.                                                        *
* -nq  No wildcard matching (default)                                          *
*                                                                              *
* Returns:                                                                     *
*                                                                              *
* 0    Files are the same                                                      *
* 1    Files are different                                                     *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINES   500000 /* maximum number of lines per file: a terminal
                            standard with every animation step is 70,000 */
#define MAXLINELEN 1000  /* maximum length of a single line */

/* edit operation type */
typedef enum { opkeep, opdel, opins } editop;

/* edit record for diff operations */
typedef struct editrec {

    editop          op;    /* operation type */
    long            line1; /* line number in file 1 */
    long            line2; /* line number in file 2 */
    struct editrec* next;  /* next edit record */

} editrec;

static char* lines1[MAXLINES]; /* lines from file 1 */
static char* lines2[MAXLINES]; /* lines from file 2 */
static long  nlines1;          /* number of lines in file 1 */
static long  nlines2;          /* number of lines in file 2 */
static int   ignorewhite;      /* ignore whitespace flag */
static int   ignorewild;       /* treat '?' as a wildcard flag */
static editrec* editlist;      /* head of edit list */
static editrec* editlast;      /* tail of edit list */

/*******************************************************************************

Check whitespace

Returns true if the character is a space or tab.

*******************************************************************************/

static int iswhite(char c)

{

    return (c == ' ' || c == '\t');

}

/*******************************************************************************

Strip whitespace

Returns a copy of the string with all whitespace removed.

*******************************************************************************/

static void stripwhite(const char* s, char* d)

{

    while (*s) { if (!iswhite(*s)) *d++ = *s; s++; }
    *d = 0;

}

/*******************************************************************************

Wild match

Compares two lines treating a '?' in either line as a wildcard that matches
any single character, and a '*' in either line as a wildcard that matches any
run of characters, including an empty one. Used (via -q) for output with
run-to-run variation, where the gold .cmp masks varying columns with '?', or
varying width fields, like process ids in filenames, with '*'.

*******************************************************************************/

static int wildmatch(const char* a, const char* b)

{

    /* identical lines always match, and this shortcut also keeps lines that
       legitimately contain '*' characters from invoking the expansion */
    if (!strcmp(a, b)) return (1);
    if (*a == '*' || *b == '*') {

        /* collapse runs of stars */
        while (*a == '*' && a[1] == '*') a++;
        while (*b == '*' && b[1] == '*') b++;
        if (*a == '*') {

            /* star matches empty, or eats one character of the other line */
            if (wildmatch(a+1, b)) return (1);
            if (*b && wildmatch(a, b+1)) return (1);

        }
        if (*b == '*') {

            if (wildmatch(a, b+1)) return (1);
            if (*a && wildmatch(a+1, b)) return (1);

        }

        return (0);

    }
    if (!*a || !*b) return (!*a && !*b);
    if (*a != *b && *a != '?' && *b != '?') return (0);

    return (wildmatch(a+1, b+1));

}

/*******************************************************************************

Lines match

Compares two lines, optionally ignoring whitespace or honoring '?' wildcards.

*******************************************************************************/

static int linesmatch(const char* a, const char* b)

{

    char sa[MAXLINELEN+1], sb[MAXLINELEN+1];

    if (!a && !b) return (1);
    if (!a || !b) return (0);
    if (ignorewild) return (wildmatch(a, b));
    if (ignorewhite) {

        stripwhite(a, sa);
        stripwhite(b, sb);

        return (!strcmp(sa, sb));

    }

    return (!strcmp(a, b));

}

/*******************************************************************************

Read file

Reads a file into a line array. Strips both Unix and Windows line endings.

*******************************************************************************/

static void readfile(FILE* f, char* lines[], long* nlines)

{

    char buf[MAXLINELEN+1];
    long l;

    *nlines = 0;
    while (fgets(buf, MAXLINELEN+1, f)) {

        l = strlen(buf);
        while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = 0;
        if (*nlines >= MAXLINES) {

            printf("*** Error: File too large (>%d lines)\n", MAXLINES);
            exit(1);

        }
        lines[*nlines] = strdup(buf);
        if (!lines[*nlines]) { printf("*** Error: Out of memory\n"); exit(1); }
        (*nlines)++;

    }

}

/*******************************************************************************

Add edit

Adds an edit operation to the edit list.

*******************************************************************************/

static void addedit(editop op, long l1, long l2)

{

    editrec* e;

    e = malloc(sizeof(editrec));
    if (!e) { printf("*** Error: Out of memory\n"); exit(1); }
    e->op = op;
    e->line1 = l1;
    e->line2 = l2;
    e->next = NULL;
    if (!editlist) editlist = e;
    else editlast->next = e;
    editlast = e;

}

/*******************************************************************************

Match length

Returns the number of consecutive matching lines starting at the given
positions in both files. Positions are 1 based.

*******************************************************************************/

static long matchlen(long i1, long i2)

{

    long n;

    n = 0;
    while (i1+n <= nlines1 && i2+n <= nlines2 &&
           linesmatch(lines1[i1+n-1], lines2[i2+n-1])) n++;

    return (n);

}

/*******************************************************************************

Find match

Finds the best matching position in file 2 for the current position in file 1.

*******************************************************************************/

static void findmatch(long i1, long i2, long* best2, long* bestlen)

{

    long j, mlen;

    *best2 = 0;
    *bestlen = 0;
    for (j = i2; j <= nlines2; j++) {

        mlen = matchlen(i1, j);
        if (mlen > *bestlen) { *bestlen = mlen; *best2 = j; }

    }

}

/*******************************************************************************

Compute diff

Computes the difference between the two files and builds the edit list.

*******************************************************************************/

static void computediff(void)

{

    long i1, i2, best2, bestlen, j;

    editlist = NULL;
    editlast = NULL;
    i1 = 1;
    i2 = 1;
    while (i1 <= nlines1 || i2 <= nlines2) {

        if (i1 <= nlines1 && i2 <= nlines2 &&
            linesmatch(lines1[i1-1], lines2[i2-1])) {

            addedit(opkeep, i1, i2);
            i1++;
            i2++;

        } else {

            findmatch(i1, i2, &best2, &bestlen);
            if (bestlen > 0) {

                /* a match exists ahead in file 2; lines before it are
                   insertions (best2 > i2 always holds here, since a match
                   at i2 itself would have been consumed above) */
                while (i2 < best2) { addedit(opins, i1-1, i2); i2++; }

            } else {

                j = i1+1;
                while (j <= nlines1 && i2 <= nlines2 &&
                       !linesmatch(lines1[j-1], lines2[i2-1])) j++;
                if (j <= nlines1 && i2 <= nlines2) {

                    while (i1 < j) { addedit(opdel, i1, i2-1); i1++; }

                } else {

                    if (i1 <= nlines1) { addedit(opdel, i1, i2-1); i1++; }
                    if (i2 <= nlines2) { addedit(opins, i1-1, i2); i2++; }

                }

            }

        }

    }

}

/*******************************************************************************

Show diff

Outputs the differences in standard diff format.

*******************************************************************************/

static void showdiff(void)

{

    editrec* ep;
    long delcount, inscount, delstart, insstart, l1pos, l2pos, i;

    ep = editlist;
    l1pos = 0;
    l2pos = 0;
    while (ep) {

        /* skip keep entries, tracking position */
        while (ep && ep->op == opkeep) {

            l1pos = ep->line1;
            l2pos = ep->line2;
            ep = ep->next;

        }
        if (ep) {

            /* count dels and ins in this change block */
            delcount = 0;
            inscount = 0;
            delstart = 0;
            insstart = 0;
            while (ep && ep->op != opkeep) {

                if (ep->op == opdel) {

                    if (!delcount) delstart = ep->line1;
                    delcount++;

                } else {

                    if (!inscount) insstart = ep->line2;
                    inscount++;

                }
                ep = ep->next;

            }
            /* output diff header */
            if (delcount > 0 && inscount > 0) {

                if (delcount == 1) printf("%ld", delstart);
                else printf("%ld,%ld", delstart, delstart+delcount-1);
                putchar('c');
                if (inscount == 1) printf("%ld\n", insstart);
                else printf("%ld,%ld\n", insstart, insstart+inscount-1);

            } else if (delcount > 0) {

                if (delcount == 1) printf("%ld", delstart);
                else printf("%ld,%ld", delstart, delstart+delcount-1);
                printf("d%ld\n", l2pos);

            } else if (inscount > 0) {

                printf("%lda", l1pos);
                if (inscount == 1) printf("%ld\n", insstart);
                else printf("%ld,%ld\n", insstart, insstart+inscount-1);

            }
            /* output deleted lines */
            for (i = delstart; i < delstart+delcount; i++)
                printf("< %s\n", lines1[i-1]);
            /* separator */
            if (delcount > 0 && inscount > 0) printf("---\n");
            /* output inserted lines */
            for (i = insstart; i < insstart+inscount; i++)
                printf("> %s\n", lines2[i-1]);

        }

    }

}

/*******************************************************************************

Has differences

Returns true if there are any differences between the files.

*******************************************************************************/

static int hasdiff(void)

{

    editrec* ep;

    for (ep = editlist; ep; ep = ep->next)
        if (ep->op != opkeep) return (1);

    return (0);

}

int main(int argc, char* argv[])

{

    FILE* f1;
    FILE* f2;
    char* file1;
    char* file2;
    int   i;

    ignorewhite = 0;
    ignorewild = 0;
    file1 = NULL;
    file2 = NULL;
    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "-w")) ignorewhite = 1;
        else if (!strcmp(argv[i], "-nw")) ignorewhite = 0;
        else if (!strcmp(argv[i], "-q")) ignorewild = 1;
        else if (!strcmp(argv[i], "-nq")) ignorewild = 0;
        else if (!file1) file1 = argv[i];
        else if (!file2) file2 = argv[i];

    }
    if (!file1 || !file2) {

        printf("Usage: dif [-w|-nw] [-q|-nq] file1 file2\n");
        printf("  -w   Ignore whitespace\n");
        printf("  -nw  Exact comparison (default)\n");
        printf("  -q   Treat '?' as a single character wildcard, and '*' as\n");
        printf("       a wildcard matching any run of characters, from\n");
        printf("       either file\n");
        printf("  -nq  No wildcard matching (default)\n");

        return (1);

    }
    f1 = fopen(file1, "r");
    if (!f1) { printf("*** Error: Cannot open %s\n", file1); return (1); }
    f2 = fopen(file2, "r");
    if (!f2) { printf("*** Error: Cannot open %s\n", file2); return (1); }
    readfile(f1, lines1, &nlines1);
    fclose(f1);
    readfile(f2, lines2, &nlines2);
    fclose(f2);
    computediff();
    showdiff();

    return (hasdiff());

}
