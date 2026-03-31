/**//***************************************************************************

Parse config file

Petit-ami pulls config files in plain text, in the order PUD or "Program, User,
current directory".

The file name is "petit_ami.cfg" or ".petit_ami.cfg". The first is visible, the
second is not. They are looked for in that order. Then, the order is to search:

Program:           The location of the program that includes petit_ami, that is,
                   the location of the binary.
User:              The location of the user main directory.
Current directory: The current directory.

Each of these are searched in the order given. Each petit_ami.cfg that is found
can have it's values overwritten by the new file, starting (typically) with an
empty config value tree. In this manner, the program installation point has the
master values, the user can override these values for their purposes, and
finally, a petit_ami.cfg file in the local directory can override that.

Petit_ami values are tree structured. The syntax of a petit_ami.cfg file is:

# comment
[<values>]...

<values> = begin <symbol>
                [<symbol> [<value>]]...
           end

<symbol> = a...z | A..Z | _ [a..z | A..Z | _ 0..9]...
<value> = ~ (space)

Example:

# This is a config file

myval "this is a string"
thisval Non-quoted string.
begin network

    # These are definitions specific to "network"
    ipaddr 192.168.1.1
    mask 255.255.255.254

end
lastval 1234

Each element of the .cfg file is on a separate line. Comment lines can be
interspersed in the file. The value of a symbol is the entire line contents
after the definition, up to eoln, minus the space that separates the symbol
from it's value. The value can be blank. This is common and means that the
appearance of the symbol itself is important (a flag).

A begin block will always have a symbol associated with it. Begin/end blocks can
nest to any level. A begin block can contain any number of values, which exist
as symbol/value pairs (including none). Thus a block can contain any number of
values or nested blocks, in any order. The block symbol has no value.

The values that are entered outside of any "begin"/"end" block have no sublist
name, and it is up to the client program to determine what this means.

Config values are passed via a tree structure:

typedef struct value {

    struct value next;
    struct value sublist;
    string name;
    string value;

} value, *valptr;

Each node can be either a block (begin/end) or a value, or both. However, in the
current implementation blocks cannot also have values.

To read config files, the existing tree is passed, which may be populated or
empty. The new values replace the old values, which are free'ed if they are
replaced. Node types cannot be changed (symbol to block or vice versa), that is
an error.

Thus typically the tree is passed NULL on first call, then for each new file
read, the values are overwritten.

Once a config tree is read, it is typically maintained by the using program,
then disposed on exit. It can be locally edited. Thus the calling program can
either provide values before the config reads, in which case they will be
overwritten, or after the reads, in which case the app is effectively
overriding, or even editing the values.

The entire tree is typically considered (but not exclusively named) petit_ami.
The first subtrees are each module, such as graphics, sound, etc. Some modules
further divide the blocks from there, such as the sound module, which maintains
a block for each plugin.

*******************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <localdefs.h>
#include <services.h>
#include <config.h>

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
    dldbg,  /* debug */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlnone;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); } while (0)

#define MAXSTR 250 /* length of string buffers */
#define MAXID  20  /* length of id words */
#define INDENT 4   /* spaces to indent by */

/**//***************************************************************************

Process error

*******************************************************************************/

static void error(
    /* filename */      string fn,
    /* line count */    int lc,
    /* error message */ string es
)

{

    fprintf(stderr, "*** Config: File: %s Line: %d %s\n", fn, lc, es);
    exit(1);

}

/**//***************************************************************************

Get label from string

Parses a label from the string. Leading spaces are discarded. The format of the
label is:

    a...z | A..Z | _ [a..z | A..Z | _ | 0..9]...

The string pointer is advanced past the label.

*******************************************************************************/

static void parlab(
    /* name of file */           string  fn,
    /* line counter */           int     lc,
    /* source string to parse */ string* s,
    /* resulting id */           string  w
)

{

    int wi;

    /* get id off line */
    wi = 0; /* set first character */
    while (isspace(**s)) (*s)++; /* skip spaces */
    while (isalnum(**s) || **s == '_') {

        if (wi > MAXID-1) error(fn, lc, "Id too long");
        w[wi++] = *(*s)++;

    }
    w[wi] = 0; /* terminate */
    if (!wi) error(fn, lc, "missing id");

}

/**//***************************************************************************

Add item to list end

Adds new list item to end of list.

*******************************************************************************/

static void addend(
    /* list */ pa_valptr* root,
    /* item */ pa_valptr  item
)

{

    pa_valptr p;
    pa_valptr l;

    l = NULL;
    p = *root;
    item->next = NULL; /* set no next */
    while (p) { l = p; p = p->next; } /* traverse to end */
    if (!l) *root = item; /* insert list top */
    else l->next = item; /* insert list last */

}

/**//***************************************************************************

Get text line from file

This is identical to libc fgets(), but tolerates any line ending:

/lf
/cr
/lf/cr
/cr/lf

Note that since fgets() places the line ending in the string, we change a /cr
first ending to /lf to match fgets().

Note that this whole routine could be placed back into stdio.c to make it OS
agnostic. If that occurs, it should be accompanied by a revision of the whole
file with respect to line endings.

*******************************************************************************/

char *fgetsale(char *s, int n, FILE *stream)

{

    int c, c1; /* input character holder */
    char *s1; /* input array holder */
    int cc;   /* character count */

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
        if (c == '\r') *s = '\n'; /* change /cr ending to /lf */
        s++; /* next character */
        cc++; /* count characters */

    } while
        (c != '\n' && c != '\r' && n--); /* not newline, and not past limit */
    *s = '\0';
    /* see if line ending is followed by an opposite and dispose if so */
    if (c == '\n' || c == '\r') {

        c1 = fgetc(stream); /* get next */
        if (!((c1 == '\r' && c == '\n') || (c1 == '\n' && c == '\r')))
            ungetc(c1, stream);

    }

    return (s1); /* exit with input array */

}

/**//***************************************************************************

Parse config list

Parses a linear list of configuration lines.

*******************************************************************************/

static void parlst(
    /* name of file */        string  fn,
    /* file handle */         FILE*   f,
    /* line counter */        int*    lc,
    /* root of config tree */ pa_valptr* root
)

{

    /* id buffer */                 char   word[MAXID];
    /* line buffer */               char   linbuf[MAXSTR];
    /* line buffer pointer */       char*  s;
    /* value constructor pointer */ pa_valptr vp;
    /* end of block */              int    end;
    /* fgets return */              string p;
    /* length of input line */      int    ll;

    end = FALSE; /* set not at end */
    while (!end) { /* not eof */

        p = fgetsale(linbuf, MAXSTR, f); /* get next line */
        if (p) { /* not EOF */

            dbg_printf(dldbg, "Next line: %s\n", linbuf);
            (*lc)++; /* increment line counter */
            ll = strlen(linbuf); /* find string length */
            /* remove trailing eoln */
            if (ll && linbuf[ll-1] == '\n') linbuf[ll-1] = 0;
            s = linbuf; /* index line */
            while (isspace(*s)) s++; /* skip any spaces */
            if (*s && *s != '\n') { /* ignore blank lines */

                if (*s == '#') { /* comment */

                    while (*s) s++; /* skip over line */

                } else if (isalnum(*s) || *s == '_') {

                    parlab(fn, *lc, &s, word); /* get id off line */
                    if (!strcmp(word, "begin")) {

                        /* nested sublist */
                        parlab(fn, *lc, &s, word); /* get symbol */
                        /* get sublist constructor */
                        vp = malloc(sizeof(pa_value));
                        if (!vp) error(fn, *lc, "Out of memory");
                        addend(root, vp); /* add to list end */
                        vp->sublist = NULL; /* set no subtree */
                        vp->name = malloc(strlen(word)+1);
                        if (!vp->name) error(fn, *lc, "Out of memory");
                        strcpy(vp->name, word); /* copy id into place */
                        vp->value = NULL;
                        /* parse sublist */
                        parlst(fn, f, lc, &(vp->sublist));
                        dbg_printf(dldbg, "branch: name: %s\n", vp->name);

                    } else if (!strcmp(word, "end"))
                        /* end of list, we simply exit */
                        end = TRUE;
                    else {

                        if (isspace(*s)) s++; /* skip space as separator */
                        /* valid id found, construct value entry */
                        vp = malloc(sizeof(pa_value));
                        if (!vp) error(fn, *lc, "Out of memory");
                        addend(root, vp); /* add to list end */
                        vp->sublist = NULL; /* set no subtree */
                        vp->name = malloc(strlen(word)+1);
                        if (!vp->name) error(fn, *lc, "Out of memory");
                        strcpy(vp->name, word); /* copy id into place */
                        vp->value = malloc(strlen(s)+1);
                        if (!vp->value) error(fn, *lc, "Out of memory");
                        strcpy(vp->value, s); /* copy value into place */
                        dbg_printf(dldbg, "value: name: %s value: %s\n",
                                   vp->name, vp->value);

                    }

                } else error(fn, *lc, "Missing id");

            }

        } else end = TRUE; /* set end of file */

    }

}

/**//***************************************************************************

Print list section

Prints one section of a config tree with indent. Prints sublists at a higher
indent level.

*******************************************************************************/

static void prtlstsub(
    /* list to print */ pa_valptr list,
    /* indent level */  int ind
)

{

    int i;

    while (list) { /* traverse the list */

        for (i = 0; i < ind; i++) fprintf(stderr, " ");
        /* if it is a branch, recurse to print at higher indent */
        if (list->sublist) {

            fprintf(stderr, "%s\n", list->name);
            prtlstsub(list->sublist, ind+INDENT);

        } else {

            fprintf(stderr, "%-20s %s\n", list->name, list->value);

        }
        list = list->next; /* next list item */

    }

}

/**//***************************************************************************

Replace entry in config list

Given a list and a pointer to an entry within the list, will remove the list
list entry and replace it with the new one. The removed entry will be recycled.

*******************************************************************************/

static void replace(
    /* old root */    pa_valptr* root,
    /* new root */    pa_valptr  match,
    /* replacement */ pa_valptr  rep
)

{

    pa_valptr p;
    pa_valptr l;

    l = NULL; /* set no last */
    p = *root; /* index root */
    while (p) {

        if (p == match) {

            /* move list links to new entry */
            if (!l) *root = rep;
            else l->next = rep;
            rep->next = match->next;
            /* free old entry */
            free(match);
            p = rep; /* set new entry */

        }
        l = p;
        p = p->next;

    }


}

/**//***************************************************************************

Print tree

A diagnostic, prints an indented table representing the given tree. Note that
since config trees are symmetrical, you can print the tree at any level.

*******************************************************************************/

void pa_prttre(
    /* list to print */ pa_valptr list
)

{

    prtlstsub(list, 0); /* print the tree */

}

/**//***************************************************************************

Search list

Searches a list of values for a match. Returns the first matching entry, or
NULL if not found. Note that this will find either a value or a sublist branch.

*******************************************************************************/

pa_valptr pa_schlst(
    /* id to match */ string id,
    /* list to search */ pa_valptr root
)

{

    while (root && strcmp(root->name, id)) root = root->next;

    return (root);

}

/**//***************************************************************************

Merge roots

Merges a new root tree with an old one. If the new tree has leaves that aren't
represented in the old tree, then they are placed in the old tree. If the new
tree has leaves the duplicate entries in the old tree, then the new definitions
replace the old ones.

By definition, all the entries in the new tree are used, and there is no need
to recycle the new tree entries. All entries that are removed from the old tree
are recycled.

Warning: the new tree is returned to you intact, but it must be discarded, since
all of the entries have been freed!

*******************************************************************************/

void pa_merge(
    /* old root */ pa_valptr* root,
    /* new root */ pa_valptr newroot
)

{

    pa_valptr match;
    pa_valptr p;

    while (newroot) { /* the new root is not at end */

        /* find any matching entry to this new one */
        match = pa_schlst(newroot->name, *root);
        if (match) { /* found an entry */

            /* merge new sublist with old sublist */
            pa_merge(&match->sublist, newroot->sublist);
            /* copy under new entry */
            newroot->sublist = match->sublist;
            p = newroot->next; /* save next entry */
            /* replace with new entry */
            replace(root, match, newroot);
            newroot = p; /* go next */

        } else { /* insert new entry */

            p = newroot->next; /* save next entry */
            addend(root, newroot); /* add new entry */
            newroot = p; /* go next */

        }

    }

}

/**//***************************************************************************

Parse config tree from filename

Parses a configuration tree from the given filename/path to the given root tree.
The values are merged with the contents of the root passed.

This routine can be called directly to use alternative config file names.

The caller is responsible for freeing the entries in the tree after use.

*******************************************************************************/

void pa_configfile(string fn, pa_valptr* root)

{

    /* file id */          FILE*  f;
    /* line counter */     int    lc;
    /* new root pointer */ pa_valptr np;

    dbg_printf(dldbg, "filename: %s\n", fn);

    f = fopen(fn, "r");
    lc = 0; /* clear line count */
    np = NULL; /* clear new root */
    if (f) { /* file exists/can be opened */

        /* parse list of values */
        parlst(fn, f, &lc, &np);
        fclose(f);
        if (dlinfo >= dbglvl) {

            /* print intermediate tree */
            dbg_printf(dldbg, "Intermediate tree:\n");
            pa_prttre(np);

        }
        /* now merge old and new trees */
        pa_merge(root, np);

    }

}

/**//***************************************************************************

Parse config

This is the standard Petit-ami configuration parse. It visits each of the
standard PA configuration files in the order:

Program path:

These are typically fixed definitions. Since multiple users can use the same
program, these are options that are expected to be set for all invocations of
the program.

User path:

Holds settings for the current user.

Current directory:

Holds options to only be applied locally.

The files in each directory are:

petit_ami.cfg   The visible config file.
.petit_ami.cfg  The invisible config file.

Each new tree is merged with the last, with any duplicate values replacing the
previous ones. Thus user definitions can override program definitions, local
defintions can override those, etc.

If a root with existing definitions is passed in, the files here will be merged
with those. You can also perform a merge with other definitions after this
call. Thus you can either put extra definitions that will be overridden, or
new definitions that will override.

The caller is responsible for freeing the entries in the tree after use.

*******************************************************************************/

void pa_config(pa_valptr* root)

{

    char pgmpth[MAXSTR];
    char filnam[MAXSTR];

    /* config from program path */
    pa_getpgm(pgmpth, MAXSTR); /* get program path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    pa_configfile(filnam, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    pa_configfile(filnam, root);

    /* config from user path */
    pa_getusr(pgmpth, MAXSTR); /* get user path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    pa_configfile(filnam, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    pa_configfile(filnam, root);

    /* config from current directory */
    pa_getcur(pgmpth, MAXSTR); /* get current path */
    /* try both visible and invisible names */
    pa_maknam(filnam, MAXSTR, pgmpth, "petit_ami", "cfg");
    pa_configfile(filnam, root);
    pa_maknam(filnam, MAXSTR, pgmpth, ".petit_ami", "cfg");
    pa_configfile(filnam, root);

}
