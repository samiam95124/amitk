/*******************************************************************************
*                                                                              *
*                                PETIT-AMI                                     *
*                                                                              *
*                             SERVICES MODULE                                  *
*                                                                              *
*                              Created 2020                                    *
*                                                                              *
*                               S. A. FRANCO                                   *
*                                                                              *
* This is a stubbed version. It exists so that Petit-Ami can be compiled and   *
* linked without all of the modules being complete.                            *
*                                                                              *
* All of the calls in this library print an error and exit.                    *
*                                                                              *
* One use of the stubs module is to serve as a prototype for a new module      *
* implementation.                                                              *
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

#include <stdlib.h>
#include <stdio.h>
#include <services.h> /* the header for this file */

/********************************************************************************

Process library error

Outputs an error message, then halts.

********************************************************************************/

static void error(char *s)
{

    fprintf(stderr, "\nError: Services: %s\n", s);

    exit(1);

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

void services_list(
    /** file to search for */ char *fn,
    /** file list returned */ services_filrec **l
)

{

    error("services_list: Is not implemented");

}

/********************************************************************************

Get time string

Converts the given time into a string.

********************************************************************************/

void services_times(
    /** result string */           char *s,
    /** result string length */    int sl,
    /** time to convert */         int t
)

{

    error("services_times: Is not implemented");

}

/********************************************************************************

Get date string

Converts the given date into a string.

********************************************************************************/

void services_dates(
    /** string to place date into */   char *s,
    /** string to place date length */ int sl,
    /** time record to write from */   int t
)

{

    error("services_dates: Is not implemented");

}

/********************************************************************************

Write time

Writes the time to a given file, from a time record.

********************************************************************************/

void services_writetime(
        /** file to write to */ FILE *f,
        /** time record to write from */ int t
)

{

    error("services_writetime: Is not implemented");

}

/********************************************************************************

Write date

Writes the date to a given file, from a time record.
Note that this routine should check and obey the international format settings
used by windows.

********************************************************************************/

void services_writedate(
        /* file to write to */ FILE *f,
        /* time record to write from */ int t
)

{

    error("services_writedate: Is not implemented");

}

/********************************************************************************

Find current time

Finds the current time as an S2000 integer.

********************************************************************************/

long services_time(void)

{

    error("services_time: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/********************************************************************************

Convert to local time

Converts a GMT standard time to the local time using time zone and daylight
savings. Does not compensate for 30 minute increments in daylight savings or
timezones.

********************************************************************************/

long services_local(long t)
{

    error("services_local: Is not implemented");

    return (1); /* this just shuts up compiler */

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

long services_clock(void)

{

    error("services_clock: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/********************************************************************************

Find elapsed time

Finds the time elapsed since a reference time. The reference time should be
obtained from "clock". Rollover is properly handled, but the maximum elapsed
time that can be measured is 24 hours.

********************************************************************************/

long services_elapsed(long r)
{

    error("services_elapsed: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/********************************************************************************

Validate filename

Finds if the given string contains a valid filename. Returns true if so,
otherwise false.

There is not much that is not valid in Unix. We only error on a filename that
is null or all blanks

********************************************************************************/

int services_validfile(
    /* string to validate */ char *s
)

{

    error("services_validfile: Is not implemented");

    return (1); /* this just shuts up compiler */

}


/********************************************************************************

Validate pathname

Finds if the given string contains a valid pathname. Returns true if so,
otherwise false. There is not much that is not valid in Unix. We only error on a
filename that is null or all blanks

********************************************************************************/

int services_validpath(
    /* string to validate */ char *s
)

{

    error("services_validpath: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/********************************************************************************

Check wildcarded filename

Checks if the given filename has a wildcard character, '*' or '?' imbedded.
Also checks if the filename ends in '/', which is an implied '*.*' wildcard
on that directory.

********************************************************************************/

int services_wild(
    /* filename */ char *s
)

{

    error("services_wild: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get environment string

Returns an environment string by name.

*******************************************************************************/

void services_getenv(
    /** string name */        char* esn,
    /** string data */        char* esd,
    /** string data length */ int esdl
)

{

    error("services_getenv: Is not implemented");

}

/********************************************************************************

Set environment string

Sets an environment string by name.

********************************************************************************/

void services_setenv(
    /* name of string */ char *sn,
    /* value of string */char *sd
)

{

    error("services_setenv: Is not implemented");

}

/********************************************************************************

Remove environment string

Removes an environment string by name.

********************************************************************************/

void services_remenv(
        /* name of string */ char *sn
)

{

    error("services_remenv: Is not implemented");

}

/********************************************************************************

Get environment strings all

Returns a table with the entire environment string set in it.

********************************************************************************/

void services_allenv(
    /* environment table */ services_envrec **el
)

{

    error("services_allenv: Is not implemented");

}

/********************************************************************************

Execute program

Executes a program by name. Does not wait for the program to complete.

********************************************************************************/

void services_exec(
    /* program name to execute */ char *cmd
)

{

    error("services_exec: Is not implemented");

}

/********************************************************************************

Execute program with wait

Executes a program by name. Waits for the program to complete.

********************************************************************************/

void services_execw(
    /* program name to execute */ char *cmd,
    /* return error */            int *err
)

{

    error("services_execw: Is not implemented");

}

/********************************************************************************

Execute program with environment

Executes a program by name. Does not wait for the program to complete. Supplies
the program environment.

********************************************************************************/

void services_exece(
    /* program name to execute */ char      *cmd,
    /* environment */             services_envrec *el
)

{

    error("services_exece: Is not implemented");
}


/********************************************************************************

Execute program with environment and wait

Executes a program by name. Waits for the program to complete. Supplies the
program environment.

********************************************************************************/

void services_execew(
        /* program name to execute */ char*      cmd,
        /* environment */             services_envrec* el,
        /* return error */            int*       err
)

{

    error("services_execew: Is not implemented");

}

/********************************************************************************

Get current path

Returns the current path in the given padded string.

********************************************************************************/

void services_getcur(
        /** buffer to get path */ char *pn,
        /** length of buffer */   int l
)

{

    error("services_getcur: Is not implemented");

}

/********************************************************************************

Set current path

Sets the current path from the given string.

********************************************************************************/

void services_setcur(
        /* path to set */ char *fn
)

{

    error("services_setcur: Is not implemented");

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

void services_brknam(
        /* file specification */ char *fn,
        /* path */               char *p, int pl,
        /* name */               char *n, int nl,
        /* extention */          char *e, int el
)

{

    error("services_brknam: Is not implemented");

}

/********************************************************************************

Make specification

Creates a file specification from its components, the path, name and extention.
We make sure that the path is properly terminated with ':' or '\' before
concatenating.

********************************************************************************/

void services_maknam(
    /** file specification to build */ char *fn,
    /** file specification length */   int fnl,
    /** path */                        char *p,
    /** filename */                    char *n,
    /** extension */                   char *e
)

{

    error("services_maknam: Is not implemented");

}

/********************************************************************************

Make full file specification

If the given file specification has a default path (the current path), then
the current path is added to it. Essentially "normalizes" file specifications.
No validity check is done. Garbage in, garbage out.

********************************************************************************/

void services_fulnam(
    /** filename */        char *fn,
    /** filename length */ int fnl
)
{

    error("services_fulnam: Is not implemented");

}

/********************************************************************************

Get program path

There is no direct call for program path. So we get the command line, and
extract the program path from that.

********************************************************************************/

void services_getpgm(
    /** program path */        char* p,
    /** program path length */ int   pl
)
{

    error("services_getpgm: Is not implemented");

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

void services_getusr(
    /** pathname */        char *fn,
    /** pathname length */ int fnl
)

{

    error("services_getusr: Is not implemented");

}

/********************************************************************************

Set attributes on file

Sets any of several attributes on a file. Set directory attribute is not
possible. This is done with makpth.

********************************************************************************/

void services_setatr(char *fn, services_attrset a)

{

    error("services_setatr: Is not implemented");

}

/********************************************************************************

Reset attributes on file

Resets any of several attributes on a file. Reset directory attribute is not
possible.

********************************************************************************/

void services_resatr(char *fn, services_attrset a)
{

    error("services_resatr: Is not implemented");

}

/********************************************************************************

Reset backup time

There is no backup time in Unix. Instead, we reset the archive bit,
which effectively means "back this file up now".

********************************************************************************/

void services_bakupd(char *fn)
{

    error("services_bakupd: Is not implemented");

}

/********************************************************************************

Set user permissions

Sets user permisions

********************************************************************************/

void services_setuper(char *fn, services_permset p)

{

    error("services_setuper: Is not implemented");

}


/********************************************************************************

Reset user permissions

Resets user permissions.

********************************************************************************/

void services_resuper(char *fn, services_permset p)

{

    error("services_resuper: Is not implemented");

}

/********************************************************************************

Set group permissions

Sets group permissions.

********************************************************************************/

void services_setgper(char *fn, services_permset p)

{

    error("services_setgper: Is not implemented");

}

/********************************************************************************

Reset group permissions

Resets group permissions.

********************************************************************************/

void services_resgper(char *fn, services_permset p)

{

    error("services_resgper: Is not implemented");

}

/********************************************************************************

Set other (global) permissions

Sets other permissions.

********************************************************************************/

void services_setoper(char *fn, services_permset p)

{

    error("services_setoper: Is not implemented");

}

/********************************************************************************

Reset other (global) permissions

Resets other permissions.

********************************************************************************/

void services_resoper(char *fn, services_permset p)
{

    error("services_resoper: Is not implemented");

}

/********************************************************************************

Make path

Create a new path. Only one new level at a time may be created.

********************************************************************************/

void services_makpth(char *fn)
{

    error("services_makpth: Is not implemented");

}

/********************************************************************************

Remove path

Create a new path. Only one new level at a time may be deleted.

********************************************************************************/

void services_rempth(char *fn)
{

    error("services_rempth: Is not implemented");

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

void services_filchr(services_chrset fc)
{

    error("services_chrset: Is not implemented");

}

/********************************************************************************

Find option character

Returns the character used to introduce a command line option.
In unix this is "-". Unix sometimes uses "+" for add/subtract option, but this
is overly cute and not common.

********************************************************************************/

char services_optchr(void)
{

    error("services_optchr: Is not implemented");

    return (' '); /* this just shuts up compiler */

}

/*******************************************************************************

Find path separator character

Returns the character used to separate filename path sections.
In windows/dos this is "\", in Unix/Linux it is '/'. One possible solution to
pathing is to accept both characters as a path separator. This means that
systems that use the '\' as a forcing character would need to represent the
separator as '\\'.

*******************************************************************************/

char services_pthchr(void)
{

    error("services_pthchr: Is not implemented");

    return (' '); /* this just shuts up compiler */

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

int services_latitude(void)

{

    error("services_latitude: Is not implemented");

    return (1); /* this just shuts up compiler */

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

int services_longitude(void)

{

    error("services_longitude: Is not implemented");

    return (1); /* this just shuts up compiler */

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

int services_altitude(void)

{

    error("services_altitude: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/** ****************************************************************************

Find country code

Gives the ISO 3166-1 1 to 3 digit numeric code for the country of the host
computer. Note that the country of host may be set by the user, or may be
determined by latitude/longitude.

*******************************************************************************/

int services_country(void)

{

    error("services_country: Is not implemented");

    return (1); /* this just shuts up compiler */

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

void services_countrys(
    /** string buffer */           char* s,
    /** length of buffer */        int len,
    /** ISO 3166-1 country code */ int c)

{

    error("services_countrys: Is not implemented");

}

/** ****************************************************************************

Find timezone offset

Finds the host location offset for the GMT to local time in seconds. It is
negative for zones west of the prime meridian, and positive for zones east.

*******************************************************************************/

int services_timezone(void)

{

    error("services_timezone: Is not implemented");

    return (1); /* this just shuts up compiler */

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

int services_daysave(void)


{

    error("services_daysave: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/** ****************************************************************************

Find if 12 or 24 hour time is in effect

Returns true if 24 hour time is in use in the current host location.

*******************************************************************************/

int services_time24hour(void)

{

    error("services_time24hour: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/** ****************************************************************************

Find language code

Finds a numeric code for the host language using the ISO 639-1 language list.
639-1 does not prescribe a numeric code for languages, so the exact code is
defined by the Petit Ami standard from an alphabetic list of the 639-1
languages. This unfortunately means that any changes or additions must
necessarily be added at the end, and thus out of order.

*******************************************************************************/

int services_language(void)

{

    error("services_language: Is not implemented");

    return (1); /* this just shuts up compiler */

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

void services_languages(char* s, int len, int l)

{

    error("services_languages: Is not implemented");

}

/** ****************************************************************************

Find the current decimal point character

Finds the decimal point character of the host, which is generally '.' or ','.

*******************************************************************************/

char services_decimal(void)

{

    error("services_decimal: Is not implemented");

    return (' '); /* this just shuts up compiler */

}

/** ****************************************************************************

Finds the number separator

finds the number separator of the host, which is generally ',' or '.', and is
generally used to mark 3 digit groups, ie., 3,000,000.

*******************************************************************************/

char services_numbersep(void)

{

    error("services_numbersep: Is not implemented");

    return (' '); /* this just shuts up compiler */

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

int services_timeorder(void)

{

    error("services_timeorder: Is not implemented");

    return (1); /* this just shuts up compiler */

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

int services_dateorder(void)

{

    error("services_dateorder: Is not implemented");

    return (1); /* this just shuts up compiler */

}
/** ****************************************************************************

Find date separator character

Finds the date separator character of the host.

Note that dates() uses this character.

*******************************************************************************/

char services_datesep(void)

{

    error("services_datesep: Is not implemented");

    return (' '); /* this just shuts up compiler */

}

/** ****************************************************************************

Find time separator character

Finds the time separator character of the host.

Note that times() uses this character.

*******************************************************************************/

char services_timesep(void)

{

    error("services_timesep: Is not implemented");

    return (' '); /* this just shuts up compiler */

}

/** ****************************************************************************

Find the currency marker character

Finds the currency symbol of the host country.

*******************************************************************************/

char services_currchr(void)

{

    error("services_currchr: Is not implemented");

    return (' '); /* this just shuts up compiler */

}
