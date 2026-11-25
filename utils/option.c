/**//***************************************************************************

Functions to parse from a list of options

Parses an option or options given as a list. The format of an option is adjusted
according to the OS requirements, meaning that these functions can be used to
cross different operating systems.

We use the option introduction character from services.c. Unix "+" character
options are not supported.

The following option formats are supported:

<lead>option
<lead>option=<number>
<lead>option=<string>

The <lead> is whatever option character services says. Where that is '-' and
the caller asks for them, single character options are taken as POSIX has
them, see ami_option().

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

void ami_dequote(string s)

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

Take an option's parameter

Converts the parameter text for the option found, into each of the forms its
table entry asks for, all from the same text. The string is taken whole and
dequoted; a number must be the whole of the text, less any quotes.

Returns 0 on success, 1 if a number was asked for and the text is not one.

*******************************************************************************/

static ami_long optparam(
    /* option table entry */ ami_optptr fp,
    /* parameter text */     string     s
)

{

    string ep; /* end pointer */
    int    l;  /* length of text */
    char   q;  /* quote the number is within, or 0 */

    if (fp->str) { /* string is wanted */

        strcpy(fp->str, s); /* get string */
        ami_dequote(fp->str); /* remove any quotes */

    }
    if (fp->fval || fp->ival) { /* number is wanted */

        /* a number can be quoted as a string can */
        l = strlen(s);
        q = 0;
        if (l >= 2 && (s[0] == '"' || s[0] == '\'') && s[0] == s[l-1])
            q = *s++;
        if (fp->fval) { /* float, and integer from it if that as well */

            *fp->fval = strtof(s, &ep); /* convert */
            if (fp->ival) *fp->ival = *fp->fval;

        } else *fp->ival = strtol(s, &ep, 0); /* integer value */
        if (ep == s) return (1); /* there was no number */
        s = ep; /* set new end */
        if (q && s[0] == q && !s[1]) s++; /* skip closing quote */

        return (!!*s); /* good if end of string */

    }

    return (0);

}

/**//***************************************************************************

Parse option word

Parses the option word s as ami_option() does, with the word that follows it
on the command line, or NULL if there is none. A single character option that
takes a parameter, and has none attached, takes the following word as its
parameter, and used is set true to say that word was consumed.

Returns 0 on success, 1 on failure.

*******************************************************************************/

static ami_long optword(
    /* string to parse */                string     s,
    /* word after it, or NULL */         string     next,
    /* option table */                   ami_optrec opts[],
    /* allow single character options */ ami_long   single,
    /* following word was used */        int*       used
)

{

    ami_long  r;         /* return value */
    int       longopt;   /* option is long ("--") */
    char      buff[MAXOPT]; /* buffer for character strings */
    ami_optptr fp;        /* found option entry pointer */
    ami_optptr op;        /* option table pointer */
    int       ovf;       /* option name is too long to be one */
    int       fo;        /* an option was found */
    int       mm;        /* mismatch found */
    int       i;

    r = 1; /* set error by default */
    *used = FALSE; /* set following word not used */
    longopt = FALSE;
    if (s[0] == ami_optchr()) { /* there is an option */

        s++; /* skip option character */
        if (single && ami_optchr() == '-' && *s == '-') {

            longopt = TRUE; /* set long option */
            s++; /* skip '-' */

        } else if (ami_optchr() != '-')
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

                    fo = TRUE; /* set option found */
                    if (fp->flag) *fp->flag = TRUE; /* set encounter flag */
                    if (fp->ival || fp->fval || fp->str) {

                        /* The option takes a parameter: the rest of this
                           word if there is any, and the word after it if
                           not. Either way it ends the word. */
                        if (*s) mm |= optparam(fp, s);
                        else if (next) {

                            mm |= optparam(fp, next);
                            *used = TRUE; /* set following word used */

                        } else mm = TRUE; /* no parameter to be had */
                        while (*s) s++; /* skip the rest of the word */

                    }

                } else mm = TRUE; /* set mismatch */

           } while (*s); /* more characters */
           r = !fo || mm; /* set return error */

        } else  { /* long options */

            /* now pull the option string from the rest */
            i = 0; /* set 1st character */
            ovf = FALSE; /* set name fits */
            while (*s && *s != '=' && *s != ':') {

                if (i < MAXOPT-1) buff[i++] = *s; /* place character */
                else ovf = TRUE; /* set too long */
                s++;

            }
            buff[i] = 0; /* terminate buffer */
            fp = NULL; /* set no option found */
            op = opts; /* index options table */
            while (op->name && !ovf) { /* search options table */

                if (!strcmp(op->name, buff)) fp = op; /* set found */
                op++;

            }
            if (fp) { /* option was found */

                if (fp->flag) *fp->flag = TRUE; /* set encounter flag */
                if (*s == '=' || *s == ':') { /* there is a parameter */

                    s++; /* skip '=' or ':' */
                    r = optparam(fp, s); /* take it */

                /* no parameter, error if not end of string or if one of the
                   parameter types was asked for */
                } else r = !(!*s && !fp->ival && !fp->fval && !fp->str);

            }

        }

    }

    return (r);

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
changed to allow single character options, as POSIX has them:

-abc

Where each of a, b, and c are option characters, and any number of options can
appear (including the same option repeated). An option that takes a parameter
takes the rest of its word as that parameter, and so is the last option of its
word:

-abc42

Here c takes 42. With nothing after it in the word, the option takes the next
word on the command line:

-abc 42

That form needs the words of the command line, so it is ami_options() that
takes it; ami_option() is given one word, and for it an option left without
its parameter is an error. Short options can be used as long options:

--a

And can have parameters in that form as well:

--a=42

The user can pick either mode of Linux option, or alternately you can just use
Unix/Linux mode options entirely. Windows users are generally used to the idea
of Unix/Linux format options, and distributing programs with these options only
is more acceptable today.

Note that '+' options are not supported. I consider this to be an archaic
format.

Note that in Windows, single options will still work, but just be identical to
long mode options:

/a

Returns 0 on success, 1 on failure.

*******************************************************************************/

ami_long ami_option(
    /* string to parse */                string    s,
    /* option table */                   ami_optrec opts[],
    /* allow single character options */ ami_long  single
)

{

    int used; /* following word used, there is none */

    return (optword(s, NULL, opts, single, &used));

}

/**//***************************************************************************

Parse option series

Parses a series of arguments as option() does. The argument strings are taken
from argv[argi], and argi is advanced over the argument series. The argc or
remaining arguments counter is also maintained. A single character option that
takes a parameter, and has none attached, takes the argument after it, and
that is passed over with it.

Note there is no way to tell which pickup argument appeared first, so the caller
should not specify more than one such argument in the options table given, or
the result will be ambiguous.

Returns 0 on success, 1 on failure.

*******************************************************************************/

ami_long ami_options(
    /* argument index */                 ami_long*     argi,
    /* argument count */                 ami_long*     argc,
    /* arguments array */                char      **argv,
    /* option table */                   ami_optrec opts[],
    /* allow single character options */ ami_long  single
)

{

    ami_long r;
    int      used; /* the argument after the option was its parameter */

    r = 0; /* set no error */
    while (*argc > 1 && argv[*argi][0] == ami_optchr() && !r) {

        /* parse options, with the argument after if there is one */
        r = optword(argv[*argi], *argc > 2? argv[*argi+1]: NULL, opts, single,
                    &used);
        if (!r) { /* success */

            (*argi) += 1+used; /* advance index */
            (*argc) -= 1+used; /* count */

        }

    }

    return (r);

}
