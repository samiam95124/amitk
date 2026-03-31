/*******************************************************************************
*                                                                              *
*                     WINDOWS EXTENDED FUNCTION LIBRARY                        *
*                                                                              *
*                              Created 2020                                    *
*                                                                              *
*                               S. A. FRANCO                                   *
*                                                                              *
* Contains various system oriented library functions, including files,         *
* directories, time, program execution, environment, and random numbers.       *
* This implementation is specific to the Unix system, but services tends to    *
* have processing elements that are universal.                                 *
*                                                                              *
* To Do:                                                                       *
*                                                                              *
* 1. This version is US english only. Need translations according to locale.   *
*                                                                              *
* 2. Functions to be changed to translations: pa_dateorder(), pa_datesep(),    *
* pa_timesep(), pa_currchar(), pa_timeorder(), pa_numbersep(), pa_decimal(),   *
* pa_time24hour().                                                             *
*
* 3. The Unix emulation layer treats .exe and similar endings as having set    *
* the executable flag, which is a good goal. Similarly, it treats '.' and '..' *
* directory entries as setting the "hidden" flag, although notably, windows    *
* itself does not (they are visible in dir listings).                          *
*                                                                              *
* 4. Could use more work on crossover attributes. For example, visible, which  *
* is in permissions, can set the "hidden" attribute in windows.                *
*                                                                              *
* Notes:                                                                       *
*                                                                              *
* 1. Unlike the original implementation, this Windows version pulls the        *
* complete environment from Windows and maintains it in local store. If this   *
* causes issues with conflicts with calls outside of this library, it can be   *
* reinstated to keep the environment within Windows (or even cache them).      *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
*******************************************************************************/

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00
#define _WIN32_IE 0xA00

#include <windows.h>
#include <limits.h>

#include <services.h> /* the header for this file */

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); \
                                fflush(stderr); } while (0)

/* contains the program invocation path */
extern char *program_invocation_name;

/* contains the entire environment strings array */
extern char **environ;

#define HOURSEC         3600   /* number of seconds in an hour */
#define DAYSEC          (HOURSEC * 24)   /* number of seconds in a day */
#define YEARSEC         (DAYSEC * 365)   /* number of seconds in year */
/* Unix time adjustment for 1970 */
#define UNIXADJ         (YEARSEC * 30 + DAYSEC * 7)

#define MAXSTR 500  /* maximum size of holding buffers */
#define MAXPTH 5000 /* max size of path */

#define MAXTHREAD 100   /* maximum number of threads */
#define MAXSEMA   100   /* maximum number of semaphores */
#define MAXLOCK   100   /* maximum number of locks */

typedef char bufstr[MAXSTR]; /* standard string buffer */

static char pthstr[MAXPTH]; /* buffer for execution path */
static bufstr langstr;  /* buffer for language country string (locale) */

static pa_envrec *envlst;   /* our environment list */

static int language;    /* current language */
static int country;     /* current country */

void pa_brknam(char *fn, char *p, int pl, char *n, int nl, char *e, int el);
void pa_maknam(char *fn, int fnl, char *p, char *n, char *e);

static CRITICAL_SECTION    thdtbllck;            /* thread table lock */
static HANDLE              threadtbl[MAXTHREAD]; /* thread id table */
static CRITICAL_SECTION    semtbllck;            /* semaphore table lock */
static PCONDITION_VARIABLE sematbl[MAXSEMA];     /* semaphore table */
static CRITICAL_SECTION    lcktbllck;            /* lock table lock */
static CRITICAL_SECTION*   locktbl[MAXLOCK];     /* lock table */

/********************************************************************************

Process string library error

Outputs an error message using the special syslib function, then halts.

********************************************************************************/

static void error(char *s)
{

    fprintf(stderr, "\nError: Services: %s\n", s);

    exit(1);

}


/********************************************************************************

Handle Windows error

Only called if the last error variable is set. The text string for the error
is output, and then the program halted.

********************************************************************************/

static void winerr(void)
{

    int e;
    char *p;
    LPVOID lpMsgBuf;

    e = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, e,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf,
                  0, NULL);
    fprintf(stderr, "\n*** Windows error: %s\n", lpMsgBuf);

    exit(1);

}

/********************************************************************************

Check file exists

Checks if the named file exists. Returns true if so.

********************************************************************************/

static int exists(char *fn)
{

    DWORD atb;

    atb = GetFileAttributes(fn);

    return (atb != INVALID_FILE_ATTRIBUTES &&
            !(atb & FILE_ATTRIBUTE_DIRECTORY));

}

/********************************************************************************

Extract string

Extracts a substring. The characters in the source string indicated are extracted
and placed into the destination string. It is an error if the number of
characters in the substring are too large for the destination.

********************************************************************************/

static void extract(char *d, int l, char *s, int st, int ed)
{

    char *ds = d;

    if (ed-st+1 > l) error("String too large for destination");
    while (st <= ed) *d++ = s[st++];
    *d = 0;

}

/********************************************************************************

Trim leading and trailing spaces off string.

Removes leading and trailing spaces from a given string. Since the string will
shrink by definition, no length is needed.

********************************************************************************/

static void trim(char *s)
{

    char* p;
    char* d;

    /* trim front */
    p = s;
    while (*p == ' ') p++;
    d = s;
    if (p != s) while (*d++ = *p++);

    /* trim back */
    p = s;
    while (*p && *p == ' ') p++; /* find end of string */
    if (p > s) {

        p--;
        while (p > s && *p == ' ') p--; /* back up to first non-space */
        p++; /* reindex */
        *p = 0; /* terminate */

    }

}

/*******************************************************************************

Place character in string

Places the given character in the space padded string buffer, with full error
checking.

*******************************************************************************/

void plcstr(char* s,  /* string to place */
            int   sl, /* length */
            int*  i,  /* index */
            char  c)  /* character to place */

{

   /* check overflow */
   if (*i > sl-1) error("Name too long for buffer");
   s[*i] = c; /* place character */
   (*i)++; /* next */

}

/******************************************************************************

Extract 1st word

Gets the first space terminated word from the given string. If it is quoted,
it will get all of the contents within the quotes, including spaces.

******************************************************************************/

void fstwrd(char* s,  /* string containing word */
            char* d,  /* word */
            int   dl) /* length of word buffer */

{

    int i, x; /* string indexes */

    i = 0; /* index 1st character of destination */
    while (*s && *s == ' ') s++; /* skip leading spaces */
    if (*s == '"') { /* quoted string */

        s++; /* skip leading quote */
        if (*s) { /* still in valid string */

            while (*s && *s != '"') { /* transfer non-quote */

                plcstr(d, dl, &i, *s); /* place character */
                s++; /* next */

            }
            if (*s != '"') plcstr(d, dl, &i, *s);

        }

    } else { /* use space delimited method */

        while (*s && *s != ' ') { /* transfer non-space */

            plcstr(d, dl, &i, *s); /* place character */
            s++; /* next */

        }
        if (*s && *s != ' ') plcstr(d, dl, &i, *s); /* place last */

    }
    plcstr(d, dl, &i, 0); /* terminate string */

}

/********************************************************************************

Convert file time to S2000 time

Converts the windows 64 bit time format to an integer seconds format. This
routine is in assembly because we don't have 64 bit capability in our
Pascal.

The seconds time format is a count, in seconds, of the current time from or
to the beginning of the year 2000. As this program is written, that is a
negative number which is counting up to 0. After the year 2000, it will be
counting up.

The result time is 32 bits on a 32 bit machine and 64 bits on a 64 bit machine.

********************************************************************************/

long filetimetoseconds(FILETIME* ft)

{

    ULARGE_INTEGER t;

    t.LowPart = ft->dwLowDateTime;
    t.HighPart = ft->dwHighDateTime;
    if (!t.QuadPart) t.QuadPart = -INT_MAX;
    else t.QuadPart = (t.QuadPart-0x01bf53eb256d4000ull)/10000000ull;

    return (t.QuadPart);

}

/********************************************************************************

Create file list

Accepts a filename, that may include wildcards. All of the matching files are
found, and a list of file entries is returned. The file entries are in standard
directory format. The path may not contain wildcards.

The entries are allocated from general storage, and both the entry and the
filename should be disposed of by the caller when they are no longer needed.
If no files are matched, the returned list is nil.

********************************************************************************/

void pa_list(
    /** file to search for */ char *fn,
    /** file list returned */ pa_filrec **l
)

{

    WIN32_FIND_DATA fd;  /* windows file information structure */
    HANDLE          hdl; /* handle for files */
    int             r;  /* result code */
    pa_filrec*      fp; /* file entry pointer */
    pa_filrec*      lp; /* last entry pointer */
    int             e;  /* error code */

    *l = NULL; /* clear destination list */
    lp = NULL; /* clear last pointer */
    r = 1; /* set ok */
    hdl = FindFirstFile(fn, &fd); /* find first file reference */
    if (hdl == INVALID_HANDLE_VALUE) {

        r = 0; /* set do not continue */
        /* note this counts on repeated calls to geterror() not affecting error
           status */
        e = GetLastError(); /* get resulting error */
        /* send all errors besides not found to stop handler */
        if (e != ERROR_FILE_NOT_FOUND) winerr();

    }
    while (r == 1) { /* gather matching files */

        fp = malloc(sizeof(pa_filrec)); /* create a new file entry */
        fp->name = malloc(strlen(fd.cFileName)+1); /* allocate filename */
        strcpy(fp->name, fd.cFileName);
        fp->size = (DWORDLONG)fd.nFileSizeHigh*(MAXDWORD+1)+(DWORDLONG)fd.nFileSizeLow;
        fp->alloc = fp->size;
        /* clear permissions to all is allowed */
        fp->user = BIT(pa_pmread) | BIT(pa_pmwrite) | BIT(pa_pmexec) | BIT(pa_pmdel) |
                   BIT(pa_pmvis) | BIT(pa_pmcopy) | BIT(pa_pmren);
        fp->other = BIT(pa_pmread) | BIT(pa_pmwrite) | BIT(pa_pmexec) | BIT(pa_pmdel) |
                    BIT(pa_pmvis) | BIT(pa_pmcopy) | BIT(pa_pmren);
        fp->group = BIT(pa_pmread) | BIT(pa_pmwrite) | BIT(pa_pmexec) | BIT(pa_pmdel) |
                    BIT(pa_pmvis) | BIT(pa_pmcopy) | BIT(pa_pmren);
        fp->attr = 0;   /* clear attributes */
        /* check and set archive attribute */
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) fp->attr |= BIT(pa_atarc);
        /* check and set system attribute */
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) fp->attr |= BIT(pa_atsys);
        /* check and set directory attribute */
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) fp->attr |= BIT(pa_atdir);
        /* in windows, the permissions are part of the attributes, and there are
          no permission classes. so we distribute the permission bits to all
          classes */
        /* check and set writeable */
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {

            /* read only removes write, delete privileges */
            fp->user &= ~(BIT(pa_pmwrite) | BIT(pa_pmdel));
            fp->other &= ~(BIT(pa_pmwrite) | BIT(pa_pmdel));
            fp->group &= ~(BIT(pa_pmwrite) | BIT(pa_pmdel));

        }
        /* check and set visable */
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {

            /* system removes write and delete priveledges */
            fp->user |= (BIT(pa_pmvis) | BIT(pa_pmdel));
            fp->other |= (BIT(pa_pmvis) | BIT(pa_pmdel));
            fp->group |= (BIT(pa_pmvis) | BIT(pa_pmdel));

        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {

            /* hidden removes visiblity, delete, rename and copy privledges */
            fp->user &= ~(BIT(pa_pmvis) | BIT(pa_pmdel) | BIT(pa_pmren) | BIT(pa_pmcopy));
            fp->other &= ~(BIT(pa_pmvis) | BIT(pa_pmdel) | BIT(pa_pmren) | BIT(pa_pmcopy));
            fp->group &= ~(BIT(pa_pmvis) | BIT(pa_pmdel) | BIT(pa_pmren) | BIT(pa_pmcopy));

        }
        /* find and flag heriarchy loops '.' and '..' */
        if (!strcmp(fp->name, ".") || !strcmp(fp->name, "..")) fp->attr |= BIT(pa_atloop);
        /* convert 64 bit times to 32 bit S2000 times */
        fp->create = filetimetoseconds(&fd.ftCreationTime);
        fp->access = filetimetoseconds(&fd.ftLastAccessTime);
        fp->modify = filetimetoseconds(&fd.ftLastWriteTime);
        /* clear backup, which is not available */
        fp->backup = -LONG_MAX;
        /* insert entry to list */
        if (!*l) *l = fp; /* insert new top */
        else lp->next = fp; /* insert next entry */
        lp = fp; /* set new last */
        fp->next = 0; /* clear next */
        r = FindNextFile(hdl, &fd); /* find the next file entry */
        if (r != 1) { /* not successful */

            e = GetLastError(); /* get error cause */
            /* send all errors besides end of files to stop handler */
            if (e != ERROR_NO_MORE_FILES) winerr();
            r = FindClose(hdl); /* close search handle */
            if (!r) winerr(); /* stop on any error */
            r = 0; /* terminate search */

        }

    }

}

/********************************************************************************

Get time string

Converts the given time into a string.

********************************************************************************/

void pa_times(
    /** result string */           char *s,
    /** result string length */    int sl,
    /** time to convert */         int t
)

{

    int h;   /* hour */
    int m;   /* minute */
    int sec; /* second */
    int  am;  /* am flag */
    int  pm;  /* pm flag */

    if (sl < 11-(!pa_time24hour()*3)) /* string to small to hold result */
        error("String buffer to small to hold time");
    /* because leap adjustments are made in terms of days, we just remove
       the days to find the time of day in seconds. this is completely
       independent of leap adjustments */
    t %= DAYSEC;   /* find number of seconds in day */
    /* if before 2000, find from remaining seconds */
    if (t < 0) t += DAYSEC;
    h = t / HOURSEC;   /* find hours */
    t %= HOURSEC;   /* subtract hours */
    m = t / 60;   /* find minutes */
    sec = t % 60;   /* find seconds */
    pm = 0; /* clear am and pm flags */
    am = 0;
    if (!pa_time24hour()) { /* do am/pm adjustment */

        if (h == 0) h = 12; /* hour zero */
        else if (h > 12) { h -= 12; pm = 1; } /* 1 pm to 11 pm */

    }
    /* place hour:miniute:second */
    switch (pa_timeorder()) {

        case 1:
            s += sprintf(s, "%02d%c%02d%c%02d", h, pa_timesep(), m, pa_timesep(), sec);
            break;
        case 2:
            s += sprintf(s, "%02d%c%02d%c%02d", h, pa_timesep(), sec, pa_timesep(), m);
            break;
        case 3:
            s += sprintf(s, "%02d%c%02d%c%02d", m, pa_timesep(), h, pa_timesep(), sec);
            break;
        case 4:
            s += sprintf(s, "%02d%c%02d%c%02d", m, pa_timesep(), sec, pa_timesep(), h);
            break;
        case 5:
            s += sprintf(s, "%02d%c%02d%c%02d", sec, pa_timesep(), h, pa_timesep(), m);
            break;
        case 6:
            s += sprintf(s, "%02d%c%02d%c%02d", sec, pa_timesep(), m, pa_timesep(), h);
            break;

    }
    if (pm) strcat(s, " pm");
    if (am) strcat(s, " am");
    *s = 0; /* terminate string */

}

/********************************************************************************

Get date string

Converts the given date into a string.

********************************************************************************/

/*
 * Check year is a leap year
 */
#define LEAPYEAR(y) ((y & 3) == 0 && y % 100 != 0 || y % 400 == 0)

void pa_dates(
    /** string to place date into */   char *s,
    /** string to place date length */ int sl,
    /** time record to write from */   int t
)

{

    int y;   /* year holder */
    char d;   /* day holder */
    char m;   /* month holder */
    char dm;   /* temp days of month holder */
    char leap;   /* leap year adder */
    char di;   /* day counter array index */
    short yd;   /* years in day holder */
    int done;   /* loop complete flag */
    int days[12];   /* days in months */

    days[0] = 31;   /* january */
    days[1] = 28;   /* february-leap day */
    days[2] = 31;   /* march */
    days[3] = 30;   /* april */
    days[4] = 31;   /* may */
    days[5] = 30;   /* june */
    days[6] = 31;   /* july */
    days[7] = 31;   /* august */
    days[8] = 30;   /* september */
    days[9] = 31;   /* october */
    days[10] = 30;   /* november */
    days[11] = 31;   /* december */

    if (sl < 10) /* string to small to hold result */
        error("*** String to small to hold date");
    if (t < 0) y = 1999; else y = 2000; /* set initial year */
    done = 0;   /* set no loop exit */
    t = abs(t);   /* find seconds magnitude */
    do {

        yd = 365;   /* set days in this year */
        if (LEAPYEAR(y)) yd = 366; /* set leap year days */
        else  yd = 365;
        /* set normal year days */
        if (t/DAYSEC > yd) {  /* remove another year */

            if (y >= 2000) y++; else y--; /* find next year */
            t -= yd * DAYSEC; /* remove that many seconds */

        } else done = 1;

    } while (!done);   /* until year found */
    leap = 0;   /* set no leap day */
    /* check leap year, and set leap day accordingly */
    if (LEAPYEAR(y)) leap = 1;
    t = t/DAYSEC+1; /* find days into year */
    if (y < 2000) t = leap - t + 366; /* adjust for negative years */
    di = 1;   /* set 1st month */
    while (di <= 12) {  /* fit months */

        dm = days[di-1]; /* get the days of month */
        if (di == 2) dm += leap; /* february, add leap day */
        /* check remaining day falls within month */
        if (dm >= t) { m = di; d = t; di = 13; }
        else { t -= dm; di++; }

    }
    /* place year/month/day */
    switch (pa_dateorder()) { /* place according to current location format */

        case 1:
            s += sprintf(s, "%04d%c%02d%c%02d", y, pa_datesep(), m, pa_datesep(), d);
            break;
        case 2:
            s += sprintf(s, "%04d%c%02d%c%02d", y, pa_datesep(), d, pa_datesep(), m);
            break;
        case 3:
            s += sprintf(s, "%02d%c%02d%c%04d", m, pa_datesep(), d, pa_datesep(), y);
            break;
        case 4:
            s += sprintf(s, "%02d%c%04d%c%02d", m, pa_datesep(), y, pa_datesep(), d);
            break;
        case 5:
            s += sprintf(s, "%02d%c%02d%c%04d", d, pa_datesep(), m, pa_datesep(), y);
            break;
        case 6:
            s += sprintf(s, "%02d%c%04d%c%02d", d, pa_datesep(), y, pa_datesep(), m);
            break;

    }
    *s = 0; /* terminate string */

}

/********************************************************************************

Write time

Writes the time to a given file, from a time record.

********************************************************************************/

void pa_writetime(
        /** file to write to */ FILE *f,
        /** time record to write from */ int t
)

{

    bufstr s;

    pa_times(s, MAXSTR, t);   /* convert time to string form */
    fputs(s, f);   /* output */

}

/********************************************************************************

Write date

Writes the date to a given file, from a time record.
Note that this routine should check and obey the international format settings
used by windows.

********************************************************************************/

void pa_writedate(
        /* file to write to */ FILE *f,
        /* time record to write from */ int t
)

{

    char s[MAXSTR];

    pa_dates(s, MAXSTR, t);   /* convert date to string form */
    fputs(s, f);   /* output */

}

/********************************************************************************

Find current time

Finds the current time as an S2000 integer.

********************************************************************************/

long pa_time(void)

{

    SYSTEMTIME  st; /* windows system format time */
    int         t;  /* S2000 time */
    int         r;  /* result buffer */
    FILETIME    ft; /* 64 bit file time */

    GetSystemTime(&st); /* get windows time */
    r = SystemTimeToFileTime(&st, &ft); /* convert to 64 bit time */
    t = filetimetoseconds(&ft); /* and that to 32 bit time */

    return (t); /* return time */

}

/********************************************************************************

Convert to local time

Converts a GMT standard time to the local time using time zone and daylight
savings. Does not compensate for 30 minute increments in daylight savings or
timezones.

********************************************************************************/

long pa_local(long t)
{

    return t+pa_timezone()+pa_daysave()*HOURSEC;

}

/********************************************************************************

Find clock tick

Finds the time in terms of "ticks". Ticks are defined to occur at 0.1ms, or
100us intervals. The rules for this counter are:

   1. The counter will rollover as much as, but not more than, each 24 hours.
   2. The counter has no specific zero point (and cannot, for example, be used
      to determine the exact time of day).

The base time of 100us is designed specifically to fit these rules. The count
will rollover each 59 hours using 31 bits of precision (the sign bit is
unused).

Note that the rules are upward compatible such that at the 64 bit precision
level, the clock actually represents a real universal time, because it then
has more than enough precision to count from 0 AD to present.

********************************************************************************/

long pa_clock(void)

{

    return (GetTickCount() % 100000000*10); /* find tick count */

}

/********************************************************************************

Find elapsed time

Finds the time elapsed since a reference time. The reference time should be
obtained from "clock". Rollover is properly handled, but the maximum elapsed
time that can be measured is 24 hours.

********************************************************************************/

long pa_elapsed(long r)
{

    /* reference time */
    long t;

    t = pa_clock();   /* get the current time */
    if (t >= r) t -= r; /* time has not wrapped */
    else t += INT_MAX-r; /* time has wrapped */

    return t;   /* return result */

}

/********************************************************************************

Validate filename

Finds if the given string contains a valid filename. Returns true if so,
otherwise false.

There is not much that is not valid in Unix. We only error on a filename that
is null or all blanks

********************************************************************************/

int pa_validfile(
    /* string to validate */ char *s
)

{

    int r;   /* good/bad result */

    r = 1;   /* set result good by default */
    while (*s && *s == ' ') s++; /* check all blanks */
    if (!*s) r = 0; /* no filename exists */

    return r;   /* return error status */

}


/********************************************************************************

Validate pathname

Finds if the given string contains a valid pathname. Returns true if so,
otherwise false. There is not much that is not valid in Unix. We only error on a
filename that is null or all blanks

********************************************************************************/

int pa_validpath(
    /* string to validate */ char *s
)

{

    int r;   /* good/bad result */

    r = 1;   /* set result good by default */
    while (*s && *s == ' ') s++; /* check all blanks */
    if (!*s) r = 0; /* no filename exists */

    return r;   /* return error status */

}

/********************************************************************************

Check wildcarded filename

Checks if the given filename has a wildcard character, '*' or '?' imbedded.
Also checks if the filename ends in '/', which is an implied '*.*' wildcard
on that directory.

********************************************************************************/

int pa_wild(
    /* filename */ char *s
)

{

    int r;   /* result flag */
    int i;   /* index for string */
    int ln;   /* length of string */

    ln = strlen(s);   /* find length */
    r = 0;   /* set no wildcard found */
    if (ln) { /* not null */

        /* search and flag wildcard characters */
        for (i = 0; i < ln; i++) { if (s[i] == '*' || s[i] == '?') r = 1; }
        if (s[ln-1] == '/') r = 1; /* last was '/', it's wild */

    }

    return r;

}

/********************************************************************************

Find environment string

Finds the environment string by name, and returns that. Returns nil if not
found.

********************************************************************************/

static void fndenv(
    /* string name */                      char*       esn,
    /* returns environment string entry */ pa_envptr*  ep
)

{

    pa_envptr p; /* pointer to environment entry */

    p = envlst; /* index top of environment list */
    *ep = NULL; /* set no string found */
    while (p && *ep == NULL) {  /* traverse */

        if (!strcmp(esn, p->name)) *ep = p; /* found */
        else p = p->next;/* next string */

    }

}

/*******************************************************************************

Get environment string

Returns an environment string by name.

*******************************************************************************/

void pa_getenv(
    /** string name */        char* esn,
    /** string data */        char* esd,
    /** string data length */ int esdl
)
{

    pa_envrec *p;

    *esd = 0;
    fndenv(esn, &p);
    if (p) {

        if (strlen(p->data)+1 > esdl) error("String too large for destination");
        if (p) strcpy(esd, p->data);

    }

}

/********************************************************************************

Set environment string

Sets an environment string by name.

********************************************************************************/

void pa_setenv(
    /* name of string */ char *sn,
    /* value of string */char *sd
)

{

    pa_envrec *p;   /* pointer to environment entry */

    fndenv(sn, &p); /* find environment string */
    if (p) { /* found */

        free(p->data);   /* release last contents */
        /* create new data string */
        p->data = (char *) malloc(strlen(sd));
        if (!p) error("Could not allocate string");
        strcpy(p->data, sd);

    } else {

        p = malloc(sizeof(pa_envrec)); /* get a new environment entry */
        if (!p) error("Could not allocate structure");
        p->next = envlst; /* push onto environment list */
        envlst = p;
        /* create new name string and place */
        p->name = (char *) malloc(strlen(sn)+1);
        if (!p) error("Could not allocate string");
        strcpy(p->name, sn);
        /* create new data string and place */
        p->data = (char *) malloc(strlen(sd)+1);
        if (!p) error("Could not allocate string");
        strcpy(p->data, sd);

    }

}

/********************************************************************************

Remove environment string

Removes an environment string by name.

********************************************************************************/

void pa_remenv(
        /* name of string */ char *sn
)

{

    pa_envrec *p, *l; /* pointer to environment entry */

    fndenv(sn, &p);   /* find environment string */
    if (p != NULL) { /* found */

        /* remove entry from list */
        if (envlst == p) envlst = p->next; /* gap from list top */
        else { /* search */

            /* find last entry that indexes this one */
            l = envlst;  /* index top of list */
            while (l->next != p && l != NULL) l = l->next; /* search */
            if (l == NULL) error("Bad environment list");
            l->next = p->next; /* gap out of list */

        }
        free(p->name); /* release name */
        free(p->data); /* release data */
        free(p); /* release entry */

    }

}

/********************************************************************************

Get environment strings all

Returns a table with the entire environment string set in it.

********************************************************************************/

void pa_allenv(
    /* environment table */ pa_envrec **el
)

{

    pa_envrec *p, *lp, *tp; /* environment pointers */

    /* copy current environment list */
    lp = envlst; /* index top of environment list */
    tp = NULL; /* clear destination */
    while (lp != NULL) {  /* copy entries */

        p = malloc(sizeof(pa_envrec)); /* create a new entry */
        p->next = tp;   /* push onto list */
        tp = p;
        p->name = (char *) malloc(strlen(lp->name)+1);
        strcpy(p->name, lp->name);   /* place name */
        p->data = (char *) malloc(strlen(lp->data)+1);
        strcpy(p->data, lp->data);   /* place data */
        lp = lp->next;   /* next entry */

    }
    /* reverse to destination */
    *el = NULL; /* clear destination */
    while (tp) {

        p = tp;
        tp = tp->next;
        p->next = *el;
        *el = p;

    }

}

/********************************************************************************

Path program name

Given a program with possible path, checks it exists and tries to path it if it
does not exist and no path is provided. Gives and error if not successful.
Returns the properly pathed command if found.

Note that the source and destination can be the same, since the operation is
buffered.

********************************************************************************/

static void cmdpth(
    /* command to search for */           char *cn,
    /* result correctly pathed command */ char *pcn,
    /* result length */                   int  pcnl
)
{

    bufstr p, n, e;    /* filename components */
    char   pc[MAXPTH]; /* path copy */
    bufstr ncn;
    char*  cp;

    strcpy(ncn, cn); /* copy command to temp */
    if (!exists(cn)) {  /* does not exist in current form */

        /* perform pathing search */
        pa_brknam(cn, p, MAXSTR, n, MAXSTR, e, MAXSTR); /* break down the name */
        *ncn = 0;
        if (*p == 0 && *pthstr != 0) {

            strcpy(pc, pthstr);   /* make a copy of the path */
            trim(pc);   /* make sure left aligned */
            while (*pc != 0) {  /* match path components */

                cp = strchr(pc, ';'); /* find next path separator */
                if (!cp) {  /* none left, use entire remaining */

                    strcpy(p, pc); /* none left, use entire remaining */
                    pc[0] = 0; /* clear the rest */

                } else { /* copy partial */

                    extract(p, MAXSTR, pc, 0, cp-pc-1);   /* get left side to path */
                    extract(pc, MAXPTH, pc, cp-pc+1, strlen(pc)); /* remove from path */
                    trim(pc); /* make sure left aligned */

                }
                pa_maknam(ncn, MAXSTR, p, n, e);   /* create filename */
                if (exists(ncn)) *pc = 0;  /* found, indicate stop */

            }
            if (!exists(ncn)) error("Command does not exist");

        } else error("Command does not exist");

    }
    if (strlen(ncn)+1 > pcnl) error("String too large for destination\n");
    strcpy(pcn, ncn); /* copy to result */

}

/******************************************************************************

Execute program with Windows environment

Base function for exec calls. Takes a windows format environment list.

The Windows CreateProcess does not appear to match its documentation well. To
make it work, we take the command line, separate off the command, and then
pass that as a separate parameter. We also add the .exe, which also appears
to be required.

The "working directory" is set as the current directory.

The pathing is broken, we need to add this manually.

******************************************************************************/

static void execwin(char* cmd,   /* command to execute */
             char* el,    /* environment list */
             int   wait,  /* wait for completion */
             int*  ec)    /* return error */

{

    PROCESS_INFORMATION pi;      /* process information */
    STARTUPINFOA        si;      /* startup information */
    DWORD               eb;      /* error buffer */
    int                 ri;      /* result */
    bufstr              fn;      /* filespec string */
    bufstr              p, n, e; /* filespec components */
    int                 rb;      /* result */
    bufstr              cmdb;    /* buffer for command line */

    if (!*cmd) error("Command string null");
    pi.hProcess = 0;
    pi.hThread = 0;
    pi.dwProcessId = 0;
    pi.dwThreadId = 0;
    si.cb = 68;
    si.lpReserved = 0;
    si.lpDesktop = 0;
    si.lpTitle = 0;
    si.dwX = 0;
    si.dwY = 0;
    si.dwXSize = 0;
    si.dwYSize = 0;
    si.dwXCountChars = 0;
    si.dwYCountChars = 0;
    si.dwFillAttribute = 0;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    si.cbReserved2 = 0;
    si.lpReserved2 = 0;
    si.hStdInput = 0;
    si.hStdOutput = 0;
    si.hStdError = 0;
    fstwrd(cmd, fn, MAXSTR); /* get filespec from command line */
    pa_brknam(fn, p, MAXSTR, n, MAXSTR, e, MAXSTR); /* break down filespec */
    if (!*e) strcpy(e, "exe"); /* add back missing command extension */
    pa_maknam(fn, MAXSTR, p, n, e); /* reconstruct */
    cmdpth(fn, fn, MAXSTR); /* complete the command path */
    if (!CreateProcess(fn, cmd, 0, 0, 0, 0, el, 0, &si, &pi))
        winerr(); /* process extended error */
    /* wait for the process to complete */
    if (wait) { /* perform process wait and error check */

        ri = WaitForSingleObject(pi.hProcess, -1);
        if (ri == WAIT_FAILED) winerr(); /* process error */
        rb = GetExitCodeProcess(pi.hProcess, &eb); /* get exit code */
        if (!rb) winerr(); /* process error */
        *ec = eb; /* copyback buffer */

    } else *ec = 0; /* clear error return */
    if (!CloseHandle(pi.hThread)) winerr();
    if (!CloseHandle(pi.hProcess)) winerr();

}

/********************************************************************************

Execute program

Executes a program by name. Does not wait for the program to complete.

********************************************************************************/

void pa_exec(
    /* program name to execute */ char *cmd
)

{

    int e;              /* result code */

    execwin(cmd, 0, 0, &e); /* execute with no environment */

}

/********************************************************************************

Execute program with wait

Executes a program by name. Waits for the program to complete.

********************************************************************************/

void pa_execw(
    /* program name to execute */ char *cmd,
    /* return error */            int *err
)

{

    execwin(cmd, 0, 1, err); /* execute with no environment and wait */

}

/******************************************************************************

Translate environment

Translates the environment from our format to Windows format.

******************************************************************************/

void trnenv(pa_envptr el,      /* our internal format environment */
            char**    evsptr)  /* windows format environent */

{

    int       sz;  /* total environment block size */
    pa_envptr p;   /* environment list pointer */
    char*     elb; /* environment block pointer */

    /* count total space in the environment */
    sz = 0;
    p = el;
    while (p) {

        sz += strlen(p->name)+1+strlen(p->data)+1;
        p = p->next;

    }
    sz++; /* add ending 0 */
    *evsptr = malloc(sz); /* get environment block */

    /* now lay down the environment strings */
    p = el;
    elb = *evsptr;
    while (p) {

        strcpy(elb, p->name); /* copy string */
        elb += strlen(p->name); /* advance over it */
        *elb++ = '='; /* place = char */
        strcpy(elb, p->data); /* copy string */
        elb += strlen(p->data)+1; /* advance over it and termination */
        p = p->next; /* go next entry */

    }
    *elb = 0; /* terminate table */

}

/********************************************************************************

Execute program with environment

Executes a program by name. Does not wait for the program to complete. Supplies
the program environment.

********************************************************************************/

void pa_exece(
    /* program name to execute */ char      *cmd,
    /* environment */             pa_envrec *el
)

{

    char* evstbl; /* windows environment table */
    int e;

    trnenv(el, &evstbl); /* translate environment */
    execwin(cmd, evstbl, 0, &e); /* execute */
    free(evstbl); /* free the windows environment block */

}


/********************************************************************************

Execute program with environment and wait

Executes a program by name. Waits for the program to complete. Supplies the
program environment.

********************************************************************************/

void pa_execew(
        /* program name to execute */ char*      cmd,
        /* environment */             pa_envrec* el,
        /* return error */            int*       err
)

{

    char* evstbl; /* windows environment table */

    trnenv(el, &evstbl); /* translate environment */
    execwin(cmd, evstbl, 1, err); /* execute */
    free(evstbl); /* free the windows environment block */

}

/********************************************************************************

Get current path

Returns the current path in the given padded string.

********************************************************************************/

void pa_getcur(
        /** buffer to get path */ char *pn,
        /** length of buffer */   int l
)

{

   int    r;  /* error return */
   bufstr pb;

   r = GetCurrentDirectory(MAXSTR, pb);/* get current directory */
   /* getcurrentdirectory returns 0 for error, which could be ambiguous. However,
     we ignore that, since having a zero length current directory should be
     impossible. */
   if (!r) winerr(); /* process windows error */
   if (r+1 > l) error("Current directory too long for string");
   strcpy(pn, pb); /* copy into place */

}

/********************************************************************************

Set current path

Sets the current path from the given string.

********************************************************************************/

void pa_setcur(
        /* path to set */ char *fn
)

{

    int r;   /* result code */

    r = SetCurrentDirectory(fn); /* set current directory */
    if (!r) winerr(); /* process error */

}

/********************************************************************************

Break file specification

Breaks a filespec down into its components, the path, name and extension.
Note that we don't validate file specifications here. Note that any part of
the file specification could be returned blank.

For Windows, we trim leading and trailing spaces, but leave any embedded spaces
or ".".

The path is straightforward, and consists of any number of /x sections. The
Presence of a trailing "\" without a name means the entire thing gets parsed
as a path, including any embedded spaces or "." characters.

Windows allows any number of "." characters, so we consider the extension to be
only the last such section, which could be null. Windows does not technically
consider "." to be a special character, but if the brknam and maknam procedures
are properly paired, it will effectively be treated the same as if the "."
were a normal character.

********************************************************************************/

void pa_brknam(
        /* file specification */ char *fn,
        /* path */               char *p, int pl,
        /* name */               char *n, int nl,
        /* extention */          char *e, int el
)

{

    int i, s, f, t; /* string indexes */
    char *s1, *s2;

    /* clear all strings */
    *p = 0;
    *n = 0;
    *e = 0;
    if (!strlen(fn)) error("File specification is empty");
    s1 = fn; /* index file spec */
    /* skip spaces */
    while (*s1 && *s1 == ' ') s1++;
    /* find last '/' that will mark the path */
    s2 = strrchr(s1, pa_pthchr());
    if (s2) {

        /* there was a path, store that */
        while (s1 <= s2) {

            if (!pl) error("String to large for destination\n");
            *p++ = *s1++;
            pl--;

        }
        if (!nl) error("String to large for destination\n");
        *p = 0;
        /* now s1 points after path */

    }
    /* keep any leading '.' characters from fooling extension finder */
    s2 = s1; /* copy start point in name */
    while (*s2 == '.') s2++;
    /* find last '.' that will mark the extension */
    s2 = strrchr(s2, '.');
    if (s2) {

        /* there was a name, store that */
        while (s1 < s2) {

            if (!nl) error("String to large for destination\n");
            *n++ = *s1++;
            nl--;

        }
        if (!nl) error("String to large for destination\n");
        *n = 0;
        /* now s1 points to extension or nothing */

    } else {

        /* no extension */
        if (strlen(s1)+1 > nl) error("String to large for destination\n");
        strcpy(n, s1);
        while (*s1) s1++;

    }
    if (*s1 == '.') {

        /* there is an extension */
        if (strlen(s1)+1 > el) error("String to large for destination\n");
        s1++; /* skip '.' */
        strcpy(e, s1);

    }

}

/********************************************************************************

Make specification

Creates a file specification from its components, the path, name and extention.
We make sure that the path is properly terminated with ':' or '\' before
concatenating.

********************************************************************************/

void pa_maknam(
    /** file specification to build */ char *fn,
    /** file specification length */   int fnl,
    /** path */                        char *p,
    /** filename */                    char *n,
    /** extension */                   char *e
)
{

    int  i;    /* index for string */
    int  fsi;  /* index for output filename */
    char s[2]; /* path character buffer */

    if (strlen(p) > fnl) error("String too large for destination");
    strcpy(fn, p); /* place path */
    /* check path properly terminated */
    i = strlen(p);   /* find length */
    if (*p) /* not null */
        if (p[i-1] != pa_pthchr()) {

        if (strlen(fn)+1 > fnl) error("String too large for destination");
        s[0] = pa_pthchr(); /* set up path character as string */
        s[1] = 0;
        strcat(fn, s); /* add path separator */

    }
    /* terminate path */
    if (strlen(fn)+strlen(n) > fnl) error("String too large for destination");
    strcat(fn, n); /* place name */
    if (*e) {  /* there is an extension */

        if (strlen(fn)+1+strlen(e) > fnl)
            error("String too large for destination");
        strcat(fn, "."); /* place '.' */
        strcat(fn, e); /* place extension */

    }

}

/********************************************************************************

Make full file specification

If the given file specification has a default path (the current path), then
the current path is added to it. Essentially "normalizes" file specifications.
No validity check is done. Garbage in, garbage out.

********************************************************************************/

void pa_fulnam(
    /** filename */        char *fn,
    /** filename length */ int fnl
)
{

    /* file specification */
    bufstr p, n, e, ps;   /* filespec components */

    pa_brknam(fn, p, MAXSTR, n, MAXSTR, e, MAXSTR);   /* break spec down */
    /* if the path is blank, then default to current */
    if (!*p) strcpy(p, ".");
    pa_getcur(ps, MAXSTR);   /* save current path */
    pa_setcur(p);   /* set candidate path */
    pa_getcur(p, MAXSTR);   /* get washed path */
    pa_setcur(ps);   /* reset old path */
    /* reassemble */
    pa_maknam(fn, fnl, p, n, e);

}

/********************************************************************************

Get program path

There is no direct call for program path. So we get the command line, and
extract the program path from that.

********************************************************************************/

void pa_getpgm(
    /** program path */        char* p,
    /** program path length */ int   pl
)
{

    char*  cp;           /* command line holder */
    bufstr cb;           /* command buffer */
    bufstr n, e;         /* path components */
    char   path[MAXPTH]; /* execution path */
    int    i;            /* index for path */
    int    f;            /* path found */

    cp = GetCommandLine(); /* get the command line */
    fstwrd(cp, cb, MAXSTR); /* get command */
    pa_brknam(cb, p, pl, n, MAXSTR, e, MAXSTR); /* break off the path */
    if (!*p) { /* no path provided, we must search for it */

        /* try current directory */
        pa_getcur(p, pl); /* get current path */
        pa_maknam(cb, MAXSTR, p, n, "exe"); /* construct name with that path */
        if (!exists(cb)) { /* try search path */

            pa_getenv("Path", path, MAXPTH);
            f = 0; /* set path not found */
            while (*path && !f) { /* search path */

                i = strchr(path, ';')-path; /* find next path separator */
                if (!i) { /* none, the rest is the path */

                    strcpy(p, path); /* place all in path */
                    *path = 0; /* clear the path */

                } else { /* break off next segment */

                    extract(p, pl, path, 0, i-1); /* place path */
                    /* extract the rest as remainer */
                    extract(path, MAXPTH, path, i+1, strlen(path));

                }
                pa_maknam(cb, MAXSTR, p, n, "exe"); /* construct name with that path */
                f = exists(cb); /* check that exists */

            }
            if (!f) error("Cannot determine program path");

        }

    }

}

/********************************************************************************

Get user path

There is no direct call for user path. We create it from the environment
variables as follows.

1. If there is a "home", "userhome" or "userdir" string, the path is taken from
that.

2. If there is a "user" or "username" string, the path becomes "/home/name"
(no drive).

3. If none of these environmental variables are found, the user path is
returned identical to the program path.

The caller should check if the path exists. If not, then the program path
should be used instead, or the current path as required. The filenames used
with program and user paths should be unique in case they end up in the same
directory.

********************************************************************************/

void pa_getusr(
    /** pathname */        char *fn,
    /** pathname length */ int fnl
)

{

    bufstr b, b1; /* buffer for result */

    pa_getenv("USERPROFILE", b, MAXSTR);
    if (!*b) { /* not found */

        pa_getenv("HOMEPATH", b, MAXSTR);
        if (*b) {

            strcpy(b1, b); /* save that */
            pa_getenv("HOMEDRIVE", b, MAXSTR);
            if (*b) strcat(b, b1); /* combine */

        }
        if (!*b) { /* not found */

            pa_getenv("USERNAME", b, MAXSTR);
            if (!*b) { /* path that */

                strcpy(b1, b); /* copy */
                strcpy(b, "\\users\\"); /* set prefix */
                strcat(b, b1); /* combine */

            } else
                pa_getpgm(b, MAXSTR); /* all fails, set to program path */

        }

    }
    if (strlen(b) > fnl+1) error("String too large for destination");
    strcpy(fn, b); /* copy into place */

}

/********************************************************************************

Set attributes on file

Sets any of several attributes on a file. Set directory attribute is not
possible. This is done with makpth.

********************************************************************************/

void pa_setatr(char *fn, pa_attrset a)

{

    int fa; /* attribute words */
    int r;  /* result holder */

    fa = GetFileAttributes(fn); /* get existing attributes on file */
    if (fa < 0) winerr(); /* error, process */
    /* built attributes equivalent word */
    fa = 0;
    if (INISET(a, pa_atarc)) fa |= FILE_ATTRIBUTE_ARCHIVE;
    if (INISET(a, pa_atsys)) fa |= FILE_ATTRIBUTE_SYSTEM;
    r = SetFileAttributes(fn, fa); /* set attributes */
    if (!r) winerr(); /* error, process */

}

/********************************************************************************

Reset attributes on file

Resets any of several attributes on a file. Reset directory attribute is not
possible.

********************************************************************************/

void pa_resatr(char *fn, pa_attrset a)
{

    int fa; /* attribute words */
    int r;  /* result holder */

    fa = GetFileAttributes(fn); /* get existing attributes on file */
    if (fa < 0) winerr(); /* error, process */
    /* built attributes equivalent word */
    fa = 0;
    if (INISET(a, pa_atarc)) fa &= ~FILE_ATTRIBUTE_ARCHIVE;
    if (INISET(a, pa_atsys)) fa = fa & ~FILE_ATTRIBUTE_SYSTEM;
    r = SetFileAttributes(fn, fa); /* set attributes */
    if (!r) winerr(); /* error, process */

}

/********************************************************************************

Reset backup time

There is no backup time in Unix. Instead, we reset the archive bit,
which effectively means "back this file up now".

********************************************************************************/

void pa_bakupd(char *fn)
{

    pa_setatr(fn, BIT(pa_atarc));

}

/********************************************************************************

Set user permissions

Sets user permisions

********************************************************************************/

void pa_setuper(char *fn, pa_permset p)

{

    int fa; /* attribute words */
    int r;  /* result holder */

    fa = GetFileAttributes(fn); /* get existing attributes on file */
    if (fa < 0) winerr(); /* error, process */
    /* built attributes equivalent word */
    fa = 0;
    if (INISET(p, pa_pmwrite)) fa = fa & !FILE_ATTRIBUTE_READONLY;
    if (INISET(p, pa_pmvis)) {

        /* remove hidden and system bits, if set */
        if (fa & FILE_ATTRIBUTE_HIDDEN) fa = fa & ~FILE_ATTRIBUTE_HIDDEN;
        if (fa & FILE_ATTRIBUTE_SYSTEM) fa = fa & ~FILE_ATTRIBUTE_SYSTEM;

    }
    r = SetFileAttributes(fn, fa); /* set attributes */
    if (!r) winerr(); /* error, process */

}


/********************************************************************************

Reset user permissions

Resets user permissions.

********************************************************************************/

void pa_resuper(char *fn, pa_permset p)

{

    int fa; /* attribute words */
    int r;  /* result holder */

   fa = GetFileAttributes(fn); /* get existing attributes on file */
   if (fa < 0) winerr(); /* error, process */
   /* built attributes equivalent word */
   fa = 0;
   if (INISET(p, pa_pmwrite)) fa = fa | FILE_ATTRIBUTE_READONLY;
   if (INISET(p, pa_pmvis)) fa = fa | FILE_ATTRIBUTE_HIDDEN;
   r = SetFileAttributes(fn, fa); /* set attributes */
   if (!r) winerr(); /* error, process */

}


/********************************************************************************

Set group permissions

Sets group permissions. This is a no-op in Windows

********************************************************************************/

void pa_setgper(char *fn, pa_permset p)

{

}


/********************************************************************************

Reset group permissions

Resets group permissions. This is a no-op in Windows.

********************************************************************************/

void pa_resgper(char *fn, pa_permset p)

{

}


/********************************************************************************

Set other (global) permissions

Sets other permissions. This is a no-op in Windows.

********************************************************************************/

void pa_setoper(char *fn, pa_permset p)

{

}


/********************************************************************************

Reset other (global) permissions

Resets other permissions. This is a no-op in Windows.

********************************************************************************/

void pa_resoper(char *fn, pa_permset p)
{

}

/********************************************************************************

Make path

Create a new path. Only one new level at a time may be created.

********************************************************************************/

void pa_makpth(char *fn)
{

    int r; /* result */

    r = CreateDirectory(fn, 0); /* create the directory */
    if (!r) winerr(); /* process error */

}

/********************************************************************************

Remove path

Create a new path. Only one new level at a time may be deleted.

********************************************************************************/

void pa_rempth(char *fn)
{

    int r;   /* result code */

    r = RemoveDirectory(fn); /* create the directory */
    if (r < 0) winerr(); /* process unix error */


}

/********************************************************************************

Find valid filename characters

Returns the set of characters allowed in a file specification. This allows a
specification to be gathered by the user.

Virtually anything can be stuffed into a Windows name. We don't differentiate
shell special characters because names can be escaped (quoted), and shells
have different special characters anyway.

As a result, we only exclude the file characters that would cause problems
with common procedures:

1. Space, because most command line names are space delimited.

2. Non printing, so we don't create names that cannot be seen as well as
removed.

3. '\', because that is the Windows/DOS option character.

Unfortunately, this can create the inability to access filenames with spaces.
For such reasons, the program will probably have to determine its own
specials in these cases.

********************************************************************************/

void pa_filchr(pa_chrset fc)
{

    int i;

    /* clear set */
    for (i = 0; i < CSETLEN; i++) fc[i] = 0;

    /* add everything but control characters and space */
    for (i = ' '+1; i <= 0x7e; i++) ADDCSET(fc, i);
    SUBCSET(fc, pa_optchr()); /* remove option character */
    SUBCSET(fc, pa_pthchr()); /* remove path character */
    SUBCSET(fc, '"'); /* remove quote characters */
    SUBCSET(fc, '\'');

}

/********************************************************************************

Find option character

Returns the character used to introduce a command line option.
In unix this is "-". Unix sometimes uses "+" for add/subtract option, but this
is overly cute and not common.

********************************************************************************/

char pa_optchr(void)
{
    return '/';

}

/*******************************************************************************

Find path separator character

Returns the character used to separate filename path sections.
In windows/dos this is "\", in Unix/Linux it is '/'. One possible solution to
pathing is to accept both characters as a path separator. This means that
systems that use the '\' as a forcing character would need to represent the
separator as '\\'.

*******************************************************************************/

char pa_pthchr(void)
{

    return ('\\');

}

/** ****************************************************************************

Find latitude

Finds the latitude of the host. Returns the latitude as a ratioed integer:

0           Equator
INT_MAX     North pole
-INT_MAX    South pole

This means each increment equals 0.0000000419 degrees or about 0.00465 meters
(approximate because it is an angular measurement on an elipsiod).

Note that implementations are generally divided into stationary and mobile
hosts. A stationary host like a desktop computer can be set once on install.
A mobile host is constantly reading its location (usually from a GPS).

There are algorithims that can find the approximate location of a host from
network connections. This would effectively allow the automatic setting of a
host location.

*******************************************************************************/

int pa_latitude(void)

{

    return (0);

}

/** ****************************************************************************

Find longitude

Finds the longitude of the host. Returns the longitude as a ratioed integer:

0           The prime meridian (Greenwitch)
INT_MAX     The prime meridian eastward around the world
-INT_MAX    The prime meridian westward around the world

This means that each increment equals 0.0000000838 degrees or about 0.00933
meters (approximate because it is an angular measurement on an elipsoid).

Note that implementations are generally divided into stationary and mobile
hosts. A stationary host like a desktop computer can be set once on install.
A mobile host is constantly reading its location (usually from a GPS).

*******************************************************************************/

int pa_longitude(void)

{

    return 0;

}

/** ****************************************************************************

Find altitude

Finds the altitude of the host. Returns the altitude as a ratioed integer:

0           MSL
INT_MAX     100km high
-INT_MAX    100km depth

This means that each increment is 0.0000465 meters. MSL is determined by WGS84
(World Geodetic System of 1984), which estalishes an ideal elipsoid as an
approximation of the shape of the world. This is the basis for GPS and
establishes GPS as the main reference MSL surface, which typically must be
corrected for the exact local system in use, which could be:

1. Local MSL.
2. Presure altitude.

Or another system.

Note that implementations are generally divided into stationary and mobile
hosts. A stationary host like a desktop computer can be set once on install.
A mobile host is constantly reading its location (usually from a GPS).

*******************************************************************************/

int pa_altitude(void)

{

    return 0;

}

/** ****************************************************************************

Find country code

Gives the ISO 3166-1 1 to 3 digit numeric code for the country of the host
computer. Note that the country of host may be set by the user, or may be
determined by latitude/longitude.

*******************************************************************************/

int pa_country(void)

{

    return country; /* USA */

}

/** ****************************************************************************

Find country identifier string

Finds the identifier string for the given ISO 3166-1 country code. If the string
does not fit into the string provided, an error results.

3166-1 country codes are both numeric codes, 2 letter country codes, and 3
letter country codes. We only use the 2 letter codes.

Note that the 2 letter codes happen to also be the Internet location codes
(like company.us or company.au).

*******************************************************************************/

typedef struct {

    char *countrystr;   /* name of counry (full) */
    char countrya2c[2]; /* 2 letter country code */
    int  countrynum;    /* numeric code for country */

} countryety;

countryety countrytab[] = {

    /* Name                                                    A2   Country-code */
    { "Afghanistan",                                          "AF", 4   },
    { "Aland Islands",                                        "AX", 248 },
    { "Albania",                                              "AL", 8   },
    { "Algeria",                                              "DZ", 12  },
    { "American Samoa",                                       "AS", 16  },
    { "Andorra",                                              "AD", 20  },
    { "Angola",                                               "AO", 24  },
    { "Anguilla",                                             "AI", 660 },
    { "Antarctica",                                           "AQ", 10  },
    { "Antigua and Barbuda",                                  "AG", 28  },
    { "Argentina",                                            "AR", 32  },
    { "Armenia",                                              "AM", 51  },
    { "Aruba",                                                "AW", 533 },
    { "Australia",                                            "AU", 36  },
    { "Austria",                                              "AT", 40  },
    { "Azerbaijan",                                           "AZ", 31  },
    { "Bahamas",                                              "BS", 44  },
    { "Bahrain",                                              "BH", 48  },
    { "Bangladesh",                                           "BD", 50  },
    { "Barbados",                                             "BB", 52  },
    { "Belarus",                                              "BY", 112 },
    { "Belgium",                                              "BE", 56  },
    { "Belize",                                               "BZ", 84  },
    { "Benin",                                                "BJ", 204 },
    { "Bermuda",                                              "BM", 60  },
    { "Bhutan",                                               "BT", 64  },
    { "Bolivia (Plurinational State of)",                     "BO", 68  },
    { "Bonaire, Sint Eustatius and Saba",                     "BQ", 535 },
    { "Bosnia and Herzegovina",                               "BA", 70  },
    { "Botswana",                                             "BW", 72  },
    { "Bouvet Island",                                        "BV", 74  },
    { "Brazil",                                               "BR", 76  },
    { "British Indian Ocean Territory",                       "IO", 86  },
    { "Brunei Darussalam",                                    "BN", 96  },
    { "Bulgaria",                                             "BG", 100 },
    { "Burkina Faso",                                         "BF", 854 },
    { "Burundi",                                              "BI", 108 },
    { "Cabo Verde",                                           "CV", 132 },
    { "Cambodia",                                             "KH", 116 },
    { "Cameroon",                                             "CM", 120 },
    { "Canada",                                               "CA", 124 },
    { "Cayman Islands",                                       "KY", 136 },
    { "Central African Republic",                             "CF", 140 },
    { "Chad",                                                 "TD", 148 },
    { "Chile",                                                "CL", 152 },
    { "China",                                                "CN", 156 },
    { "Christmas Island",                                     "CX", 162 },
    { "Cocos (Keeling) Islands",                              "CC", 166 },
    { "Colombia",                                             "CO", 170 },
    { "Comoros",                                              "KM", 174 },
    { "Congo",                                                "CG", 178 },
    { "Congo, Democratic Republic of the",                    "CD", 180 },
    { "Cook Islands",                                         "CK", 184 },
    { "Costa Rica",                                           "CR", 188 },
    { "Cote d'Ivoire",                                        "CI", 384 },
    { "Croatia",                                              "HR", 191 },
    { "Cuba",                                                 "CU", 192 },
    { "Curacao",                                              "CW", 531 },
    { "Cyprus",                                               "CY", 196 },
    { "Czechia",                                              "CZ", 203 },
    { "Denmark",                                              "DK", 208 },
    { "Djibouti",                                             "DJ", 262 },
    { "Dominica",                                             "DM", 212 },
    { "Dominican Republic",                                   "DO", 214 },
    { "Ecuador",                                              "EC", 218 },
    { "Egypt",                                                "EG", 818 },
    { "El Salvador",                                          "SV", 222 },
    { "Equatorial Guinea",                                    "GQ", 226 },
    { "Eritrea",                                              "ER", 232 },
    { "Estonia",                                              "EE", 233 },
    { "Eswatini",                                             "SZ", 748 },
    { "Ethiopia",                                             "ET", 231 },
    { "Falkland Islands (Malvinas)",                          "FK", 238 },
    { "Faroe Islands",                                        "FO", 234 },
    { "Fiji",                                                 "FJ", 242 },
    { "Finland",                                              "FI", 246 },
    { "France",                                               "FR", 250 },
    { "French Guiana",                                        "GF", 254 },
    { "French Polynesia",                                     "PF", 258 },
    { "French Southern Territories",                          "TF", 260 },
    { "Gabon",                                                "GA", 266 },
    { "Gambia",                                               "GM", 270 },
    { "Georgia",                                              "GE", 268 },
    { "Germany",                                              "DE", 276 },
    { "Ghana",                                                "GH", 288 },
    { "Gibraltar",                                            "GI", 292 },
    { "Greece",                                               "GR", 300 },
    { "Greenland",                                            "GL", 304 },
    { "Grenada",                                              "GD", 308 },
    { "Guadeloupe",                                           "GP", 312 },
    { "Guam",                                                 "GU", 316 },
    { "Guatemala",                                            "GT", 320 },
    { "Guernsey",                                             "GG", 831 },
    { "Guinea",                                               "GN", 324 },
    { "Guinea-Bissau",                                        "GW", 624 },
    { "Guyana",                                               "GY", 328 },
    { "Haiti",                                                "HT", 332 },
    { "Heard Island and McDonald Islands",                    "HM", 334 },
    { "Holy See",                                             "VA", 336 },
    { "Honduras",                                             "HN", 340 },
    { "Hong Kong",                                            "HK", 344 },
    { "Hungary",                                              "HU", 348 },
    { "Iceland",                                              "IS", 352 },
    { "India",                                                "IN", 356 },
    { "Indonesia",                                            "ID", 360 },
    { "Iran (Islamic Republic of)",                           "IR", 364 },
    { "Iraq",                                                 "IQ", 368 },
    { "Ireland",                                              "IE", 372 },
    { "Isle of Man",                                          "IM", 833 },
    { "Israel",                                               "IL", 376 },
    { "Italy",                                                "IT", 380 },
    { "Jamaica",                                              "JM", 388 },
    { "Japan",                                                "JP", 392 },
    { "Jersey",                                               "JE", 832 },
    { "Jordan",                                               "JO", 400 },
    { "Kazakhstan",                                           "KZ", 398 },
    { "Kenya",                                                "KE", 404 },
    { "Kiribati",                                             "KI", 296 },
    { "Korea (Democratic People's Republic of)",              "KP", 408 },
    { "Korea, Republic of",                                   "KR", 410 },
    { "Kuwait",                                               "KW", 414 },
    { "Kyrgyzstan",                                           "KG", 417 },
    { "Lao People's Democratic Republic",                     "LA", 418 },
    { "Latvia",                                               "LV", 428 },
    { "Lebanon",                                              "LB", 422 },
    { "Lesotho",                                              "LS", 426 },
    { "Liberia",                                              "LR", 430 },
    { "Libya",                                                "LY", 434 },
    { "Liechtenstein",                                        "LI", 438 },
    { "Lithuania",                                            "LT", 440 },
    { "Luxembourg",                                           "LU", 442 },
    { "Macao",                                                "MO", 446 },
    { "Madagascar",                                           "MG", 450 },
    { "Malawi",                                               "MW", 454 },
    { "Malaysia",                                             "MY", 458 },
    { "Maldives",                                             "MV", 462 },
    { "Mali",                                                 "ML", 466 },
    { "Malta",                                                "MT", 470 },
    { "Marshall Islands",                                     "MH", 584 },
    { "Martinique",                                           "MQ", 474 },
    { "Mauritania",                                           "MR", 478 },
    { "Mauritius",                                            "MU", 480 },
    { "Mayotte",                                              "YT", 175 },
    { "Mexico",                                               "MX", 484 },
    { "Micronesia (Federated States of)",                     "FM", 583 },
    { "Moldova, Republic of",                                 "MD", 498 },
    { "Monaco",                                               "MC", 492 },
    { "Mongolia",                                             "MN", 496 },
    { "Montenegro",                                           "ME", 499 },
    { "Montserrat",                                           "MS", 500 },
    { "Morocco",                                              "MA", 504 },
    { "Mozambique",                                           "MZ", 508 },
    { "Myanmar",                                              "MM", 104 },
    { "Namibia",                                              "NA", 516 },
    { "Nauru",                                                "NR", 520 },
    { "Nepal",                                                "NP", 524 },
    { "Netherlands",                                          "NL", 528 },
    { "New Caledonia",                                        "NC", 540 },
    { "New Zealand",                                          "NZ", 554 },
    { "Nicaragua",                                            "NI", 558 },
    { "Niger",                                                "NE", 562 },
    { "Nigeria",                                              "NG", 566 },
    { "Niue",                                                 "NU", 570 },
    { "Norfolk Island",                                       "NF", 574 },
    { "North Macedonia",                                      "MK", 807 },
    { "Northern Mariana Islands",                             "MP", 580 },
    { "Norway",                                               "NO", 578 },
    { "Oman",                                                 "OM", 512 },
    { "Pakistan",                                             "PK", 586 },
    { "Palau",                                                "PW", 585 },
    { "Palestine, State of",                                  "PS", 275 },
    { "Panama",                                               "PA", 591 },
    { "Papua New Guinea",                                     "PG", 598 },
    { "Paraguay",                                             "PY", 600 },
    { "Peru",                                                 "PE", 604 },
    { "Philippines",                                          "PH", 608 },
    { "Pitcairn",                                             "PN", 612 },
    { "Poland",                                               "PL", 616 },
    { "Portugal",                                             "PT", 620 },
    { "Puerto Rico",                                          "PR", 630 },
    { "Qatar",                                                "QA", 634 },
    { "RÃ©union",                                              "RE", 638 },
    { "Romania",                                              "RO", 642 },
    { "Russian Federation",                                   "RU", 643 },
    { "Rwanda",                                               "RW", 646 },
    { "Saint BarthÃ©lemy",                                     "BL", 652 },
    { "Saint Helena, Ascension and Tristan da Cunha",         "SH", 654 },
    { "Saint Kitts and Nevis",                                "KN", 659 },
    { "Saint Lucia",                                          "LC", 662 },
    { "Saint Martin (French part)",                           "MF", 663 },
    { "Saint Pierre and Miquelon",                            "PM", 666 },
    { "Saint Vincent and the Grenadines",                     "VC", 670 },
    { "Samoa",                                                "WS", 882 },
    { "San Marino",                                           "SM", 674 },
    { "Sao Tome and Principe",                                "ST", 678 },
    { "Saudi Arabia",                                         "SA", 682 },
    { "Senegal",                                              "SN", 686 },
    { "Serbia",                                               "RS", 688 },
    { "Seychelles",                                           "SC", 690 },
    { "Sierra Leone",                                         "SL", 694 },
    { "Singapore",                                            "SG", 702 },
    { "Sint Maarten (Dutch part)",                            "SX", 534 },
    { "Slovakia",                                             "SK", 703 },
    { "Slovenia",                                             "SI", 705 },
    { "Solomon Islands",                                      "SB", 90  },
    { "Somalia",                                              "SO", 706 },
    { "South Africa",                                         "ZA", 710 },
    { "South Georgia and the South Sandwich Islands",         "GS", 239 },
    { "South Sudan",                                          "SS", 728 },
    { "Spain",                                                "ES", 724 },
    { "Sri Lanka",                                            "LK", 144 },
    { "Sudan",                                                "SD", 729 },
    { "Suriname",                                             "SR", 740 },
    { "Svalbard and Jan Mayen",                               "SJ", 744 },
    { "Sweden",                                               "SE", 752 },
    { "Switzerland",                                          "CH", 756 },
    { "Syrian Arab Republic",                                 "SY", 760 },
    { "Taiwan, Province of China",                            "TW", 158 },
    { "Tajikistan",                                           "TJ", 762 },
    { "Tanzania, United Republic of",                         "TZ", 834 },
    { "Thailand",                                             "TH", 764 },
    { "Timor-Leste",                                          "TL", 626 },
    { "Togo",                                                 "TG", 768 },
    { "Tokelau",                                              "TK", 772 },
    { "Tonga",                                                "TO", 776 },
    { "Trinidad and Tobago",                                  "TT", 780 },
    { "Tunisia",                                              "TN", 788 },
    { "Turkey",                                               "TR", 792 },
    { "Turkmenistan",                                         "TM", 795 },
    { "Turks and Caicos Islands",                             "TC", 796 },
    { "Tuvalu",                                               "TV", 798 },
    { "Uganda",                                               "UG", 800 },
    { "Ukraine",                                              "UA", 804 },
    { "United Arab Emirates",                                 "AE", 784 },
    { "United Kingdom of Great Britain and Northern Ireland", "GB", 826 },
    { "United States of America",                             "US", 840 },
    { "United States Minor Outlying Islands",                 "UM", 581 },
    { "Uruguay",                                              "UY", 858 },
    { "Uzbekistan",                                           "UZ", 860 },
    { "Vanuatu",                                              "VU", 548 },
    { "Venezuela (Bolivarian Republic of)",                   "VE", 862 },
    { "Viet Nam",                                             "VN", 704 },
    { "Virgin Islands (British)",                             "VG", 92  },
    { "Virgin Islands (U.S.)",                                "VI", 850 },
    { "Wallis and Futuna",                                    "WF", 876 },
    { "Western Sahara",                                       "EH", 732 },
    { "Yemen",                                                "YE", 887 },
    { "Zambia",                                               "ZM", 894 },
    { "Zimbabwe",                                             "ZW", 716 },
    { 0,                                                      0,    0   }

};

void pa_countrys(
    /** string buffer */           char* s,
    /** length of buffer */        int len,
    /** ISO 3166-1 country code */ int c)

{

    countryety* p; /* pointer to country entry */

    p = countrytab;
    while (p->countrynum && p->countrynum != c) p++;
    if (!p->countrynum) error("Country number invalid");
    if (strlen(p->countrystr) > len) error("Country string too large for buffer");
    strncpy(s, p->countrystr, len);

}

/** ****************************************************************************

Find timezone offset

Finds the host location offset for the GMT to local time in seconds. It is
negative for zones west of the prime meridian, and positive for zones east.

*******************************************************************************/

int pa_timezone(void)

{

    TIME_ZONE_INFORMATION tz; /* time zone information structure */
    DWORD r;

    r = GetTimeZoneInformation(&tz);
    if (r == TIME_ZONE_ID_INVALID) winerr(); /* flag stop */

    /* change the direction of bias to UTC to local and return */
    return (-(tz.Bias*60));

}

/** ****************************************************************************

Find daylight savings time

Finds if daylight savings time is in effect. It returns true if daylight savings
time is in effect at the present time, which in the majority of locations means
to add one hour to the local time (some locations offset by 30 minutes).

daysave() is automatically adjusted for time of year. That is, if the location
uses daylight savings time, but it is not currently in effect, the function
returns false.

Note that local() already takes daylight savings into account.

*******************************************************************************/

int pa_daysave(void)


{

    TIME_ZONE_INFORMATION tz; /* time zone information structure */
    DWORD r;

    r = GetTimeZoneInformation(&tz);
    if (r == TIME_ZONE_ID_INVALID) winerr(); /* flag stop */

    /* change the direction of bias to UTC to local and return */
    return (r == TIME_ZONE_ID_DAYLIGHT);

}

/** ****************************************************************************

Find if 12 or 24 hour time is in effect

Returns true if 24 hour time is in use in the current host location.

*******************************************************************************/

int pa_time24hour(void)

{

    return 0;

}

/** ****************************************************************************

Find language code

Finds a numeric code for the host language using the ISO 639-1 language list.
639-1 does not prescribe a numeric code for languages, so the exact code is
defined by the Petit Ami standard from an alphabetic list of the 639-1
languages. This unfortunately means that any changes or additions must
necessarily be added at the end, and thus out of order.

*******************************************************************************/

int pa_language(void)

{

    return language;

}

/** ****************************************************************************

Find language identifier string from language code

Finds a language identifier string from a given language code. If the identifier
string is too long for the string buffer, an error results.

The language codes are from the ISO 639-1 standard. It describes languages with
2 and 3 letter codes. We use only the two letter codes here.

The ISO 639-1 standard does not assign logical numbers to the languages
(unlike the ISO 3166-1 standard), so the numbering here is simply a sequential
numbering of the languages. However, we will keep the numbering system for any
additions. Once a language is assigned a number it keeps it.

*******************************************************************************/

typedef struct {

    int langnum;
    char *langstr;
    char langnamea2c[2];

} langety;

static langety langtab[] = {

    /* # name                      639-1 */
    { 1,   "Abkhaz",                                              "ab" },
    { 2,   "Afar",                                                "aa" },
    { 3,   "Afrikaans",                                           "af" },
    { 4,   "Akan",                                                "ak" },
    { 5,   "Albanian",                                            "sq" },
    { 6,   "Amharic",                                             "am" },
    { 7,   "Arabic",                                              "ar" },
    { 8,   "Aragonese",                                           "an" },
    { 9,   "Armenian",                                            "hy" },
    { 10,  "Assamese",                                            "as" },
    { 11,  "Avaric",                                              "av" },
    { 12,  "Avestan",                                             "ae" },
    { 13,  "Aymara",                                              "ay" },
    { 14,  "Azerbaijani",                                         "az" },
    { 15,  "Bambara",                                             "bm" },
    { 16,  "Bashkir",                                             "ba" },
    { 17,  "Basque",                                              "eu" },
    { 18,  "Belarusian",                                          "be" },
    { 19,  "Bengali, Bangla",                                     "bn" },
    { 20,  "Bihari",                                              "bh" },
    { 21,  "Bislama",                                             "bi" },
    { 22,  "Bosnian",                                             "bs" },
    { 23,  "Breton",                                              "br" },
    { 24,  "Bulgarian",                                           "bg" },
    { 25,  "Burmese",                                             "my" },
    { 26,  "Catalan",                                             "ca" },
    { 27,  "Chamorro",                                            "ch" },
    { 28,  "Chechen",                                             "ce" },
    { 29,  "Chichewa, Chewa, Nyanja",                             "ny" },
    { 30,  "Chinese",                                             "zh" },
    { 31,  "Chuvash",                                             "cv" },
    { 32,  "Cornish",                                             "kw" },
    { 33,  "Corsican",                                            "co" },
    { 34,  "Cree",                                                "cr" },
    { 35,  "Croatian",                                            "hr" },
    { 36,  "Czech",                                               "cs" },
    { 37,  "Danish",                                              "da" },
    { 38,  "Divehi, Dhivehi, Maldivian",                          "dv" },
    { 39,  "Dutch",                                               "nl" },
    { 40,  "Dzongkha",                                            "dz" },
    { 41,  "English",                                             "en" },
    { 42,  "Esperanto",                                           "eo" },
    { 43,  "Estonian",                                            "et" },
    { 44,  "Ewe",                                                 "ee" },
    { 45,  "Faroese",                                             "fo" },
    { 46,  "Fijian",                                              "fj" },
    { 47,  "Finnish",                                             "fi" },
    { 48,  "French",                                              "fr" },
    { 49,  "Fula, Fulah, Pulaar, Pular",                          "ff" },
    { 50,  "Galician",                                            "gl" },
    { 51,  "Georgian",                                            "ka" },
    { 52,  "German",                                              "de" },
    { 53,  "Greek (modern)",                                      "el" },
    { 54,  "Guarani",                                             "gn" },
    { 55,  "Gujarati",                                            "gu" },
    { 56,  "Haitian, Haitian Creole",                             "ht" },
    { 57,  "Hausa",                                               "ha" },
    { 58,  "Hebrew (modern)",                                     "he" },
    { 59,  "Herero",                                              "hz" },
    { 60,  "Hindi",                                               "hi" },
    { 61,  "Hiri Motu",                                           "ho" },
    { 62,  "Hungarian",                                           "hu" },
    { 63,  "Interlingua",                                         "ia" },
    { 64,  "Indonesian",                                          "id" },
    { 65,  "Interlingue",                                         "ie" },
    { 66,  "Irish",                                               "ga" },
    { 67,  "Igbo",                                                "ig" },
    { 68,  "Inupiaq",                                             "ik" },
    { 69,  "Ido",                                                 "io" },
    { 70,  "Icelandic",                                           "is" },
    { 71,  "Italian",                                             "it" },
    { 72,  "Inuktitut",                                           "iu" },
    { 73,  "Japanese",                                            "ja" },
    { 74,  "Javanese",                                            "jv" },
    { 75,  "Kalaallisut, Greenlandic",                            "kl" },
    { 76,  "Kannada",                                             "kn" },
    { 77,  "Kanuri",                                              "kr" },
    { 78,  "Kashmiri",                                            "ks" },
    { 79,  "Kazakh",                                              "kk" },
    { 80,  "Khmer",                                               "km" },
    { 81,  "Kikuyu, Gikuyu",                                      "ki" },
    { 82,  "Kinyarwanda",                                         "rw" },
    { 83,  "Kyrgyz",                                              "ky" },
    { 84,  "Komi",                                                "kv" },
    { 85,  "Kongo",                                               "kg" },
    { 86,  "Korean",                                              "ko" },
    { 87,  "Kurdish",                                             "ku" },
    { 88,  "Kwanyama, Kuanyama",                                  "kj" },
    { 89,  "Latin",                                               "la" },
    { 90,  "Luxembourgish, Letzeburgesch",                        "lb" },
    { 91,  "Ganda",                                               "lg" },
    { 92,  "Limburgish, Limburgan, Limburger",                    "li" },
    { 93,  "Lingala",                                             "ln" },
    { 94,  "Lao",                                                 "lo" },
    { 95,  "Lithuanian",                                          "lt" },
    { 96,  "Luba-Katanga",                                        "lu" },
    { 97,  "Latvian",                                             "lv" },
    { 98,  "Manx",                                                "gv" },
    { 99,  "Macedonian",                                          "mk" },
    { 100, "Malagasy",                                            "mg" },
    { 101, "Malay",                                               "ms" },
    { 102, "Malayalam",                                           "ml" },
    { 103, "Maltese",                                             "mt" },
    { 104, "Maori",                                               "mi" },
    { 105, "Marathi",                                             "mr" },
    { 106, "Marshallese",                                         "mh" },
    { 107, "Mongolian",                                           "mn" },
    { 108, "Nauruan",                                             "na" },
    { 109, "Navajo, Navaho",                                      "nv" },
    { 110, "Northern Ndebele",                                    "nd" },
    { 111, "Nepali",                                              "ne" },
    { 112, "Ndonga",                                              "ng" },
    { 113, "Norwegian Bokmal",                                    "nb" },
    { 114, "Norwegian Nynorsk",                                   "nn" },
    { 115, "Norwegian",                                           "no" },
    { 116, "Nuosu",                                               "ii" },
    { 117, "Southern Ndebele",                                    "nr" },
    { 118, "Occitan",                                             "oc" },
    { 119, "Ojibwe, Ojibwa",                                      "oj" },
    { 120, "Old Church Slavonic, Church Slavonic, Old Bulgarian", "cu" },
    { 121, "Oromo",                                               "om" },
    { 122, "Oriya",                                               "or" },
    { 123, "Ossetian, Ossetic",                                   "os" },
    { 124, "(Eastern) Punjabi",                                   "pa" },
    { 125, "Pali",                                                "pi" },
    { 126, "Persian (Farsi)",                                     "fa" },
    { 127, "Polish",                                              "pl" },
    { 128, "Pashto, Pushto",                                      "ps" },
    { 129, "Portuguese",                                          "pt" },
    { 130, "Quechua",                                             "qu" },
    { 131, "Romansh",                                             "rm" },
    { 132, "Kirundi",                                             "rn" },
    { 133, "Romanian",                                            "ro" },
    { 134, "Russian",                                             "ru" },
    { 135, "Sanskrit",                                            "sa" },
    { 136, "Sardinian",                                           "sc" },
    { 137, "Sindhi",                                              "sd" },
    { 138, "Northern Sami",                                       "se" },
    { 139, "Samoan",                                              "sm" },
    { 140, "Sango",                                               "sg" },
    { 141, "Serbian",                                             "sr" },
    { 142, "Scottish Gaelic, Gaelic",                             "gd" },
    { 143, "Shona",                                               "sn" },
    { 144, "Sinhalese, Sinhala",                                  "si" },
    { 145, "Slovak",                                              "sk" },
    { 146, "Slovene",                                             "sl" },
    { 147, "Somali",                                              "so" },
    { 148, "Southern Sotho",                                      "st" },
    { 149, "Spanish",                                             "es" },
    { 150, "Sundanese",                                           "su" },
    { 151, "Swahili",                                             "sw" },
    { 152, "Swati",                                               "ss" },
    { 153, "Swedish",                                             "sv" },
    { 154, "Tamil",                                               "ta" },
    { 155, "Telugu",                                              "te" },
    { 156, "Tajik",                                               "tg" },
    { 157, "Thai",                                                "th" },
    { 158, "Tigrinya",                                            "ti" },
    { 159, "Tibetan Standard, Tibetan, Central",                  "bo" },
    { 160, "Turkmen",                                             "tk" },
    { 161, "Tagalog",                                             "tl" },
    { 162, "Tswana",                                              "tn" },
    { 163, "Tonga (Tonga Islands)",                               "to" },
    { 164, "Turkish",                                             "tr" },
    { 165, "Tsonga",                                              "ts" },
    { 166, "Tatar",                                               "tt" },
    { 167, "Twi",                                                 "tw" },
    { 168, "Tahitian",                                            "ty" },
    { 169, "Uyghur",                                              "ug" },
    { 170, "Ukrainian",                                           "uk" },
    { 171, "Urdu",                                                "ur" },
    { 172, "Uzbek",                                               "uz" },
    { 173, "Venda",                                               "ve" },
    { 174, "Vietnamese",                                          "vi" },
    { 175, "Volapuk",                                             "vo" },
    { 176, "Walloon",                                             "wa" },
    { 177, "Welsh",                                               "cy" },
    { 178, "Wolof",                                               "wo" },
    { 179, "Western Frisian",                                     "fy" },
    { 180, "Xhosa",                                               "xh" },
    { 181, "Yiddish",                                             "yi" },
    { 182, "Yoruba",                                              "yo" },
    { 183, "Zhuang, Chuang",                                      "za" },
    { 184, "Zulu",                                                "zu" },
    { 0,   0,                                                     0    }

};

void pa_languages(char* s, int len, int l)

{

    langety* p; /* pointer to language entry */

    p = langtab;
    while (p->langnum && p->langnum != l) p++;
    if (!p->langnum) error("Language number invalid");
    if (strlen(p->langstr) > len) error("Language string too larg e for buffer");
    strncpy(s, p->langstr, len);

}

/** ****************************************************************************

Find the current decimal point character

Finds the decimal point character of the host, which is generally '.' or ','.

*******************************************************************************/

char pa_decimal(void)

{

    return '.';

}

/** ****************************************************************************

Finds the number separator

finds the number separator of the host, which is generally ',' or '.', and is
generally used to mark 3 digit groups, ie., 3,000,000.

*******************************************************************************/

char pa_numbersep(void)

{

    return ',';

}

/** ****************************************************************************

Find the time order

Returns a code for order of time presentation, which is:

1   hour:minute:second
2   hour:second:minute
3   minute:hour:second
4   minute:second:hour
5   second:minute:hour
6   second:minute:hour

The #1 format is the recommended standard for international exchange and is
compatible with computer sorting. Thus it can be common to override the local
presentation with it for archival use.

Note that times() compensates for this.

*******************************************************************************/

int pa_timeorder(void)

{

    return 1;

}

/** ****************************************************************************

Find the date order

Returns a code for order of date presentation, which is:

1   year-month-day
2   year-day-month
3   month-day-year
4   month-year-day
5   day-month-year
6   day-year-month

The #1 format is the recommended standard for international exchange and is
compatible with computer sorting. Thus it can be common to override the local
presentation with it for archival use.

The representation of year as 2 digits is depreciated, due to year 2000 issues
and because it makes the ordering of y-m-d more obvious.

Note that dates() compensates for this.

*******************************************************************************/

int pa_dateorder(void)

{

    return 1;

}
/** ****************************************************************************

Find date separator character

Finds the date separator character of the host.

Note that dates() uses this character.

*******************************************************************************/

char pa_datesep(void)

{

    return '/';

}

/** ****************************************************************************

Find time separator character

Finds the time separator character of the host.

Note that times() uses this character.

*******************************************************************************/

char pa_timesep(void)

{

    return ':';

}

/** ****************************************************************************

Find the currency marker character

Finds the currency symbol of the host country.

*******************************************************************************/

char pa_currchr(void)

{

    return '$';

}

/** ****************************************************************************

Start logical thread

Start new thread. Expects a function to start as the new thread. A logical
thread number is created from 1 to N, where N is the maximum number of threads
supported. A new thread number is allocated and returned to the caller. The
thread is created and executes at the given function. The thread will run until
it returns from the called function or is killed.

*******************************************************************************/

static DWORD WINAPI dummystart(LPVOID function)

{

    void (*fp)(void) = function;

    /* call thread main function */
    fp();

    return (ERROR_SUCCESS);

}

int pa_newthread(void (*threadmain)(void))

{

    int   i;
    HANDLE th;

    EnterCriticalSection(&thdtbllck); /* lock thread table */
    /* Find free table entry. Note that we reserve thread 1 as the thread for
       the process. This has no use at the moment. */
    i = 1;
    while (threadtbl[i] && i < MAXTHREAD) i++;
    if (threadtbl[i]) {

        LeaveCriticalSection(&thdtbllck); /* unlock thread table */
        error("Thread table full");

    }
    th = CreateThread(NULL, 0, dummystart, threadmain, 0, NULL);
    if (!th) {
        
        LeaveCriticalSection(&thdtbllck); /* unlock thread table */
        winerr(); /* process error */

    }
    threadtbl[i] = th;
    LeaveCriticalSection(&thdtbllck); /* unlock thread table */

    return (i+1); /* return the thread logical id to caller */

}

/** ****************************************************************************

Create concurrency lock

Creates a new concurrency lock and returns the logical id for it.

*******************************************************************************/

int pa_initlock(void)

{

    int i;
    int r;
    CRITICAL_SECTION* lp;

    lp = malloc(sizeof(CRITICAL_SECTION)); /* get new lock entry out of lock */
    if (!lp) error("Out of memory"); /* couldn't allocate */
    EnterCriticalSection(&lcktbllck); /* lock lock table */
    /* find free table entry */
    i = 0;
    while (locktbl[i] && i < MAXLOCK) i++;
    if (locktbl[i]) {

        LeaveCriticalSection(&lcktbllck); /* unlock thread table */
        error("Concurrency lock table full");

    }
    /* allocate lock data block */
    locktbl[i] = lp;
    LeaveCriticalSection(&lcktbllck); /* unlock thread table */
    /* Now we own the table entry, initialize the lock */
    InitializeCriticalSection(lp);

    return (i+1); /* return the lock logical id to caller */

}

/** ****************************************************************************

Destroy concurrency lock

Releases a concurrency lock by logical id.

*******************************************************************************/

void pa_deinitlock(int ln)

{

    CRITICAL_SECTION* lp;

    if (ln < 1 || ln > MAXLOCK) error("Invalid concurrency lock logical id");
    EnterCriticalSection(&lcktbllck); /* lock lock table */
    lp = locktbl[ln-1]; /* get lock pointer */
    locktbl[ln-1] = NULL; /* flag entry free */
    LeaveCriticalSection(&lcktbllck); /* unlock thread table */
    if (!lp) error("Concurrency lock by logical id is not active");
    DeleteCriticalSection(lp); /* release the lock */

}

/** ****************************************************************************

Lock concurrency

The given lock by logical id is aquired. Generally fairlocking can be assumed,
which means that for N waiters on the lock, they will be given the lock first
come first served.

*******************************************************************************/

void pa_lock(int ln)

{

    CRITICAL_SECTION* lp;

    if (ln < 1 || ln > MAXLOCK) error("Invalid concurrency lock logical id");
    /* assume this is an atomic access (single word) */
    lp = locktbl[ln-1]; /* get lock pointer */
    if (!lp) error("Concurrency lock by logical id is not active");
    /* acquire lock */
    EnterCriticalSection(lp);

}

/** ****************************************************************************

Unlock concurrency

The concurrency lock by logical id is released. The next thread queued for the
lock and that is in a runnable state is set to run.

*******************************************************************************/

void pa_unlock(int ln)

{

    CRITICAL_SECTION* lp;

    if (ln < 1 || ln > MAXLOCK) error("Invalid concurrency lock logical id");
    /* assume this is an atomic access (single word) */
    lp = locktbl[ln-1]; /* get lock pointer */
    if (!lp) error("Concurrency lock by logical id is not active");
    /* release lock */
    LeaveCriticalSection(lp);

}

/** ****************************************************************************

Create concurrency signal

Creates a new concurrency signal and returns the logical id for it.

*******************************************************************************/

int pa_initsig(void)

{

    int i;

    EnterCriticalSection(&semtbllck); /* lock semaphore table */
    /* find free table entry */
    i = 0;
    while (sematbl[i] && i < MAXSEMA) i++;
    if (sematbl[i]) {

        LeaveCriticalSection(&semtbllck); /* unlock semaphore table */
        error("Semaphore table full");

    }
    /* create condition variable */
    InitializeConditionVariable(sematbl[i]);
    LeaveCriticalSection(&semtbllck); /* unlock semaphore table */

    return (i+1); /* return the lock logical id to caller */

}

/** ****************************************************************************

Destroy concurrency signal

Releases a concurrency lock by logical id.

*******************************************************************************/

int pa_deinitsig(int sn)

{

    PCONDITION_VARIABLE sp;

    if (sn < 1 || sn > MAXSEMA) error("Semaphore logical id");
    EnterCriticalSection(&semtbllck); /* lock semaphore table */
    sp = sematbl[sn-1]; /* get pointer to conditional variable */
    sematbl[sn-1] = NULL; /* flag entry free */
    LeaveCriticalSection(&semtbllck); /* unlock semaphore table */
    if (!sp) error("Semaphore by logical id is not active");
    /* Windows does not have a delete conditional variable routine */

}

/** ****************************************************************************

Signal event

Flags an event to the given signal by logical id. Either all waiters on the
signal or just one is set to run by a signal.

*******************************************************************************/

void pa_sendsig(int sn)

{

    PCONDITION_VARIABLE sp;

    if (sn < 1 || sn > MAXSEMA) error("Semaphore logical id");
    /* assume this is an atomic access (single word) */
    sp = sematbl[sn-1]; /* get semaphore pointer */
    if (!sp) error("Semaphore by logical id is not active");
    /* signal single event waiter */
    WakeAllConditionVariable(sp);

}

/** ****************************************************************************

Signal single event

Flags an event to the given signal by logical id. Only one thread is signaled
to run. This version of signal is used when only one waiter can use the
event signaled.

Note that it is possible for this call to be equivalent to pa_signal() on a
given implementation. Thus programs should check if the signaled event is
still active, and not just assume it.

*******************************************************************************/

void pa_sendsigone(int sn)

{

    PCONDITION_VARIABLE sp;

    if (sn < 1 || sn > MAXSEMA) error("Semaphore logical id");
    /* assume this is an atomic access (single word) */
    sp = sematbl[sn-1]; /* get semaphore pointer */
    if (!sp) error("Semaphore by logical id is not active");
    /* signal single event waiter */
    WakeConditionVariable(sp);

}

/** ****************************************************************************

Wait signal

Wait for a given signal. wait() accepts a currency lock logical id, and a signal
logical id. Releases the concurrency lock and waits for the given signal, then
returns when the signal flags.

The purpose of accepting a lock number is that it is released and the signal is
checked atomically. This means other threads that are not signaling will not
run, and thus the wait and signal operations are synchronized together.

*******************************************************************************/

void pa_waitsig(int ln, int sn)

{

    int r;
    PCONDITION_VARIABLE sp;
    CRITICAL_SECTION* lp;

    if (ln < 1 || ln > MAXLOCK) error("Invalid concurrency lock logical id");
    /* assume this is an atomic access (single word) */
    lp = locktbl[ln-1]; /* get lock pointer */
    if (!lp) error("Concurrency lock by logical id is not active");
    if (sn < 1 || sn > MAXSEMA) error("Semaphore logical id");
    /* assume this is an atomic access (single word) */
    sp = sematbl[ln-1]; /* get semaphore pointer */
    if (!sp) error("Semaphore by logical id is not active");
    /* wait on signal with lock release */
    r = SleepConditionVariableCS(sp, lp, INFINITE);
    if (!r) winerr(); /* fails, process error */

}

/** ****************************************************************************

Initialize services

We initialize all variables and tables.

The Linux environment string table is fixed in length, and thus cannot be
Edited. We load it to a dynamic table, and thus may edit it. It can then be
passed on to a subprogram, either in the edited form, or unmodified.

Note the environment is unordered.

*******************************************************************************/

static void pa_init_services (void) __attribute__((constructor (102)));
static void pa_init_services()

{

    int         envlen; /* number of environment strings */
    char**      ep;     /* unix environment string table */
    int         ei;     /* index for string table */
    int         si;     /* index for strings */
    pa_envrec*  p;      /* environment entry pointer */
    langety*    lp;     /* pointer to language entry */
    countryety* ctp;    /* pointer to language entry */
    char*       evstbl; /* Windows environment string block pointer */
    bufstr      name;   /* save for variable name */
    pa_envrec*  p1;
    char*       cp;
    char*       np;
    int         l;
    int         i;

    /* Copy environment to local */
    envlst = NULL;   /* clear environment strings */
    evstbl = GetEnvironmentStrings(); /* get windows environment string block */
    while (*evstbl) { /* not end of table */

        cp = evstbl; /* index string */
        np = name; /* index name buffer */
        if (*cp == '=') cp++; /* skip "system" environmental strings marker */
        while (*cp && *cp != '=') *np++ = *cp++; /* skip to '=' */
        *np = 0; /* terminate name */
        if (cp && *cp == '=') cp++; /* skip '=' */
        pa_setenv(name, cp); /* register that variable */
        while (*cp++);
        evstbl = cp; /* move to next entry or end */

    }

    /* reverse the environment to original order for neatness */
    p = envlst;
    envlst = NULL;
    while (p) {

        p1 = p;
        p = p->next;
        p1->next = envlst;
        envlst = p1;

    }

    pa_getenv("Path", pthstr, MAXPTH); /* load up the current path */
    trim(pthstr); /* make sure left aligned */

    /* set default language and country */
    language = 41; /* english */
    country = 840; /* USA */

    /* clear thread table */
    for (i = 0; i < MAXTHREAD; i++) threadtbl[i] = NULL;

    /* clear semaphore table */
    for (i = 0; i < MAXSEMA; i++) sematbl[i] = NULL;

    /* clear lock table */
    for (i = 0; i < MAXLOCK; i++) locktbl[i] = NULL;

    /* initialize the locks */
    InitializeCriticalSection(&thdtbllck);
    InitializeCriticalSection(&semtbllck);
    InitializeCriticalSection(&lcktbllck);

}

/** ****************************************************************************

Deinitialize services

Not used at present

*******************************************************************************/

static void pa_deinit_services (void) __attribute__((destructor (102)));
static void pa_deinit_services()

{

    int        ti; /* index for timers */
    pa_envrec* p;  /* environment entry pointer */

    while (envlst) {

        p = envlst;
        envlst = envlst->next;
        free(p);

    }

    /* deinitialize the locks */
    DeleteCriticalSection(&thdtbllck);
    DeleteCriticalSection(&semtbllck);
    DeleteCriticalSection(&lcktbllck);

}
