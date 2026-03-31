/**//***************************************************************************

Functions to parse from a list of options

Parses an option or options given as a list. The format of an option is adjusted
according to the OS requirements, meaning that these functions can be used to
cross different operating systems.

We use the option introduction character from services.c. This means that Unix/
linux single character options, and Unix "+" character options are not
supported.

The following option formats are supported:

<lead>option
<lead>option=<number>
<lead>option=<string>

The <lead> is whatever option character services says.

*******************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <localdefs.h>
#include <services.h>

#include <option.h>

#define MAXOPT 100 /* maximum option size */

/**//***************************************************************************

Dequote

Remove any quotes from the specified string. Accepts either matching " or '
quotes, and the string is shrunk by 2. If the quotes are not matched or no
quotes are present, nothing is done.

*******************************************************************************/

void pa_dequote(string s)

{

    int l;
    int i;

    l = strlen(s);
    if (l >=2 && (s[0] == '"' || s[0] == '\'') && s[0] == s[l-1]) {

        /* quoted and matched */
        for (i = 0; i < l-1; i++) s[i] = s[i+1]; /* move down */
        s[i] = 0; /* terminate */

    }

}

/**//***************************************************************************

Parse option from string

Parse a single option from the string using an options table. If the option
matches an entry in the table, its fields are filled based on the option. These
are:

1. The exists flag, set to true if the option was found.
2. The integer value of the option.
3. The floating point value of the option.
4. The entire string of the option.

Note that normally, either option form according to operating system is allowed,
and only a single option introduction character is needed:

-option - Linux or MAC OS X
/option - Windows

The format -option can use any number of characters, which is not exactly Linux
standard, but is used. An example of this form is java.

If the single flag is used and the option character is '-', then the behavior is
changed to allow single character options:

-abc

Where each of a, b, and c are option characters. In this mode, no parameters are
parsed nor allowed, and any number of options can appear (including the same
option repeated). Short options can be used as long options:

--a

And can have parameters in this mode as well:

--a=42

The user can pick either mode of Linux option, or alternately you can just use
Unix/Linux mode options entirely. Windows users are generally used to the idea
of Unix/Linux format options, and distributing programs with these options only
is more acceptable today. There is no "Unix/Linux" only option/parameter,
because I think it would be better to just use existing Unix/linux option
parsing support like GNU getopt() or similar library routines.

Note that '+' options are not supported. I consider this to be an archaic
format.

Note that in Windows, single options will still work, but just be identical to
long mode options:

/a

*******************************************************************************/

int pa_option(
    /* string to parse */                string    s,
    /* option table */                   pa_optrec opts[],
    /* allow single character options */ int       single
)

{

    int       r;         /* return value */
    int       longopt;   /* option is long ("--") */
    char      buff[100]; /* buffer for character strings */
    pa_optptr fp;        /* found option entry pointer */
    string    ep;        /* end pointer */
    pa_optptr op;        /* option table pointer */
    int       fo;        /* an option was found */
    int       mm;        /* mismatch found */
    int       i;

    r = 1; /* set error by default */
    longopt = FALSE;
    if (s[0] == pa_optchr()) { /* there is an option */

        s++; /* skip option character */
        if (single && pa_optchr() == '-' && *s == '-') {

            longopt = TRUE; /* set long option */
            s++; /* skip '-' */

        } else if (pa_optchr() != '-')
            /* all other option characters besides '-' signify long */
            longopt = TRUE;
        if (*s && single && !longopt) { /* parse singles */

            fo = FALSE; /* no option found */
            mm = FALSE; /* no mismatch found */
            do {

                buff[0] = *s; /* get single character (or null) */
                buff[1] = 0; /* terminate */
                if (*s) s++; /* not end, advance */
                fp = NULL; /* set no option found */
                op = opts; /* index options table */
                while (op->name) { /* search options table */

                    if (!strcmp(op->name, buff)) fp = op; /* set found */
                    op++;

                }
                if (fp) { /* option was found */

                    if (!fp->ival && !fp->fval && !fp->str) {

                        /* no parameters required */
                        fo = TRUE; /* set option found */
                        if (fp->flag) *fp->flag = TRUE; /* set encounter flag */

                    }

                } else mm = TRUE; /* set mismatch */

           } while (*s); /* more characters */
           r = !fo || mm; /* set return error */

        } else  { /* long options */

            /* now pull the option string from the rest */
            i = 0; /* set 1st character */
            while (*s && *s != '=' && *s != ':') {

                buff[i++] = *s++; /* place character */

            }
            buff[i] = 0; /* terminate buffer */
            fp = NULL; /* set no option found */
            op = opts; /* index options table */
            while (op->name) { /* search options table */

                if (!strcmp(op->name, buff)) fp = op; /* set found */
                op++;

            }
            if (fp) { /* option was found */

                if (fp->flag) *fp->flag = TRUE; /* set encounter flag */
                if (*s == '=' || *s == ':') { /* there is a parameter */

                    s++; /* skip '=' or ':' */
                    /* if string is wanted do that conversion first */
                    if (fp->str) {

                        strcpy(fp->str, s); /* get string */
                        pa_dequote(fp->str); /* remove any quotes */
                        while (*s) s++; /* skip contents */

                    }
                    /* if float is asked for, convert that first */
                    if (fp->fval) {

                        *fp->fval = strtof(s, &ep); /* convert */
                        s = ep; /* set new end */
                        /* if also integer, convert that */
                        if (fp->ival) *fp->ival = *fp->fval;

                    } else if (fp->ival) {

                        /* integer value */
                        *fp->ival = strtol(s, &ep, 0); /* convert */
                        s = ep; /* set new end */

                    }
                    r = !!*s; /* return good if end of string */

                /* no parameter, error if not end of string or if one of the
                   parameter types was asked for */
                } else r = !(!*s && !fp->ival && !fp->fval && !fp->str);

            }

        }

    }

    return (r);

}

/**//***************************************************************************

Parse option series

Parses a series of arguments using options(). The argument strings are taken
from argv[argi], and argi is advanced over the argument series. Each argument
is parsed individually. The argc or remaining arguments counter is also
maintained.

Note there is no way to tell which pickup argument appeared first, so the caller
should not specify more than one such argument in the options table given, or
the result will be ambiguous.

Returns 0 on success, 1 on failure.

*******************************************************************************/

int pa_options(
    /* argument index */                 int*      argi,
    /* argument count */                 int*      argc,
    /* arguments array */                char      **argv,
    /* option table */                   pa_optrec opts[],
    /* allow single character options */ int       single
)

{

    int r;

    r = 0; /* set no error */
    while (*argc > 1 && argv[*argi][0] == pa_optchr() && !r) {

        r = pa_option(argv[*argi], opts, single); /* parse options */
        if (!r) { /* success */

            (*argi)++; /* advance index */
            (*argc)--; /* count */

        }

    }

    return (r);

}
