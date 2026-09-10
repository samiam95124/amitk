/*******************************************************************************
*                                                                              *
*                                MAIL READER                                   *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
* Reads mail from an IMAP server and shows it, in the manner of the web mail   *
* readers: the folders down the left, the messages of the chosen folder on     *
* the right, one line each, with the sender, the subject, the start of the     *
* message and when it arrived.                                                 *
*                                                                              *
* Nothing on the server is changed. The folder is opened with EXAMINE, which   *
* the protocol defines as read only, and the message is asked for with         *
* BODY.PEEK[], which is the form that does not mark it read. Either alone      *
* would do; both are used because a mail reader that quietly marks a thousand  *
* messages read is a mail reader nobody will trust twice.                      *
*                                                                              *
* What comes off the server is written to a file as it arrived, headers and    *
* all, in mbox format: the message is not taken apart to be stored, only to    *
* be shown. Everything the reader displays it finds by searching that file     *
* again, so the file is the truth and the display is a view of it. A file      *
* that this program wrote can be read by any other mail program, and one       *
* written by another mail program can be read by this.                         *
*                                                                              *
* The mail is kept in a directory of its own, ~/.amimail by default, one       *
* .mbox file per folder and a small state file beside it recording how far     *
* the folder was read, so that fetching again fetches only what is new.        *
*                                                                              *
* Usage:                                                                       *
*                                                                              *
*     mail [--fetch|-f] [--diag|-d] [--limit=<n>]                              *
*                                                                              *
* --fetch fetches on startup rather than waiting for Mail/Fetch. --diag        *
* reports the conversation with the server to stderr, with the password        *
* held back.                                                                   *
*                                                                              *
* The account is given in the Petit-Ami configuration, under a branch of its   *
* own, which puts it in petit_ami.cfg beside the program, in the user's path,  *
* or in the current directory, as config defines:                              *
*                                                                              *
*     begin mail                                                               *
*         imap     imap.gmail.com                                              *
*         imapport 993                                                         *
*         smtp     smtp.gmail.com                                              *
*         smtpport 465                                                         *
*         user     someone@gmail.com                                           *
*         pass     abcdefghijklmnop                                            *
*         limit    200                                                         *
*         store    /home/someone/.amimail                                      *
*     end                                                                      *
*                                                                              *
* For Gmail the password is not the account password but an application        *
* password, which Google issues per program to an account that has two step    *
* verification turned on. Since that file then holds a password, this warns    *
* if it is one that anybody but its owner can read.                            *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <limits.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/evp.h>

#include <localdefs.h>
#include <graphics.h>
#include <network.h>
#include <services.h>
#include <option.h>

#include <mailcore.h>

#define MAXSTR    500  /* the usual string */
#define MAXLINE   4000 /* longest protocol or message line taken whole */
#define MAXFOLDER 200  /* folders on the server */
#define MAXMSG    20000 /* messages indexed in one folder */
#define SNIPPET   400  /* characters of the message kept for the list */
#define DEFLIMIT  200  /* messages fetched from a folder, most recent first */

/* the windows: the panes are children of the main window, the reader is a
   window of its own */
#define MAINWIN   1 /* the main window, which is stdout */
#define FOLDWIN   2 /* the folder pane */
#define LISTWIN   3 /* the message list pane */
#define READWIN   4 /* a message, opened to be read */
#define SRVWIN    5 /* the server form */
#define POPWIN    6 /* the message menu, on the second mouse button */
#define HELPWIN   7 /* the help window */
#define BANWIN    8 /* the banner across the top */
#define CMPWIN    9 /* the compose window */
#define SRCWIN   10 /* the search window */
#define OPTWIN   11 /* the options box */
#define BANPIC    1 /* the picture in it */

/* its widgets, numbered within it */
#define HELPFIND  1 /* the search entry */
#define HELPLIST  2 /* the topic list */
#define HELPCLOSE 3 /* the close button */

/* The help text is a file, not something built into the program, so
   that it can be as long as it deserves and can be corrected without a
   compiler. It is markdown of the plain kind; see the file itself. */
#define HELPFILE  "mail.md"

/* the scroll bars, which are widgets of the pane they are in */
#define SBLIST    1 /* the message list */
#define SBREAD    1 /* the reader */

/* the widgets of the server form, numbered within its own window */
#define SRVIMAP   1 /* the imap server */
#define SRVIPORT  2 /* and its port */
#define SRVSMTP   3 /* the sending server */
#define SRVSPORT  4 /* and its port */
#define SRVUSER   5 /* the user name */
#define SRVPASS   6 /* the password */
#define SRVLIMIT  7 /* messages fetched per folder */
#define SRVNAME   8 /* what this account is called */
#define SRVPOLL   9 /* seconds between looks at the servers */
#define SRVOK    10 /* save and close */
#define SRVCAN   11 /* close and keep what was there */
#define SRVNEXT  12 /* show the next account */
#define SRVNEW   13 /* start another one */
#define SRVDEL   14 /* take this one away */
#define SRVSEND  15 /* mail is sent from this account */

/* the compose window's widgets */
#define CMPTO     1 /* who it is to */
#define CMPCC     2 /* and who else */
#define CMPSUB    3 /* what it is about */
#define CMPSEND   4 /* send it */
#define CMPCAN    5 /* and think better of it */
#define CMPSB     6 /* the bar down the side of the body */

/* the search form */
#define SRCFROM   1 /* the sender holds */
#define SRCTO     2 /* the To line holds */
#define SRCSUBJ   3 /* the subject holds */
#define SRCWORDS  4 /* it has the words */
#define SRCNOT    5 /* and not these */
#define SRCSIZEOP 6 /* greater or less than */
#define SRCSIZE   7 /* this many */
#define SRCUNIT   8 /* of these */
#define SRCWITHIN 9 /* dated within this */
#define SRCDATE  10 /* of this day */
#define SRCFOLD  11 /* the folder searched */
#define SRCATT   12 /* it has an attachment */
#define SRCGO    13 /* search */
#define SRCCLR   14 /* clear the form */
#define SRCCLOSE 15 /* close it */
#define SRCSB    16 /* the bar beside what was found */
#define SRCWILD  17 /* * and ? in the terms stand for anything */

static void newmenu(ami_menuptr* mp, int onoff, int bar, int select,
                    int id, char* face);
static void appendmenu(ami_menuptr* list, ami_menuptr m);
static void kickworker(void);

/* menu ids of our own, after the standard ones */
#define MENUMAIL  (AMI_SMMAX+1) /* the mail menu itself */
#define MENUFETCH (AMI_SMMAX+2) /* fetch new mail */
#define MENUCOMP  (AMI_SMMAX+6) /* write a message */
#define MENUREPLY (AMI_SMMAX+7) /* answer the message being read */
#define MENUREPALL (AMI_SMMAX+8) /* and everybody it went to */
#define MENUFWD   (AMI_SMMAX+9) /* pass it on */
#define MENUFOLD  (AMI_SMMAX+3) /* fetch the folder list again */
#define MENUCHECK (AMI_SMMAX+4) /* check that mail could be sent */
#define MENUSRV   (AMI_SMMAX+5) /* the server form */
#define MENUSEARCH (AMI_SMMAX+10) /* the search form */
#define MENUOPT   (AMI_SMMAX+11) /* the options box */

/* the options box */
#define OPTTHREAD 1 /* messages shown by thread */
#define OPTCLOSE  2 /* close it */

/* What a notch of the wheel moves. One message, because a message is a
   thing and a notch is a step, and the list is read by stepping through
   it. Text is not read that way, so the reader keeps three: a message of
   any length would be tedious at one line a notch. */
#define DIGLEN    65 /* a sha-256 as hex, and its terminator */
#define MAXSRV    8  /* servers this program will gather from */
#define DEFPOLL   15 /* seconds between looks at the servers */
#define NETWAIT   45 /* seconds to wait on a server before giving up */

#define WHEELMSGS 1 /* messages the wheel moves per notch, in the list */
#define WHEELROWS 3 /* lines it moves per notch, in the reader */

#define TIMFETCH  1 /* the timer that steps a fetch along */
#define TIMPOLL   2 /* and the one that starts one now and then */

#define OFF 0
#define ON  1

/* a folder on the server, and the file it is kept in */





static int     listshown;       /* the first one actually on the screen */

/* the account */

static int    srvedit;  /* the one the form is showing */
/* Which account mail is sent from. One of them, and only one: a message
   goes out over one connection with one name on it, so the box that
   says which is a choice among the accounts, not a setting each of them
   carries. */

/* A message waiting to go. The sending is network, and network is the
   worker's, so what is written is left here and taken by it. */

/* the one being talked to just now, which the protocol routines use */


/* the windows and their measurements */
static FILE* foldwf;            /* the folder pane */
static FILE* listwf;            /* the message list pane */
static FILE* readwf;            /* the reader, NULL when closed */
static FILE* srvwf;             /* the server form, NULL when closed */
static int   chrh;              /* the height of a line, in pixels */
static int   rowh;              /* the height of a message line */
/* The size everything is drawn at. Control-+ and control-- move it a
   point at a time, and every window takes its size from it, so the whole
   program grows and shrinks together. The window is not touched: what is
   in it changes size, and the user sets the window themselves. */
#define BASEPOINT 12.0 /* the size it starts at, and the picture was made for */
#define MINPOINT  6.0  /* as small and as large as it will go */
#define MAXPOINT  36.0
static float pointsz = BASEPOINT;
static int   foldw;             /* the width of the folder pane */
static int   listw;             /* and of the message list */
static int   banw;              /* and of the banner, as last laid out */
static int   banpart;           /* the banner's right strip is drawn already */
static int   listpart;          /* and the list's */
static ami_long sbw;               /* scroll bar thickness */
static int   listrows;          /* message lines the list holds */
static ami_long clickms;        /* when the list was last clicked */
static char  movnote[MAXSTR];   /* the move asked for, while it waits its turn */
static char  keepsaid[MAXSTR];  /* what the worker said, kept for when it is done */
static int   foldy[MAXFOLDER];  /* where each folder was drawn, for clicks */
static int   foldon[MAXFOLDER]; /* and whether it is drawn at all */
static int   foldcnt[MAXFOLDER]; /* the count each line shows */
static int   listx, listy;      /* where the list pane sits on the window */
static int   fromx;             /* where the sender column ends */
static int   catx;              /* and where the category column ends */
static int   datex;             /* where the date column begins */
static int   mpx, mpy;          /* the mouse, in pixels of its own window */

/* the reader */
static char* readtext;          /* the message being read, decoded */
static int   readtop;           /* the first line of it shown */
static int   readshown;         /* the first line actually on the screen */
static char** readline;         /* it, wrapped to the window */
static int   readlines;
static int   readmax;

/* the connection to the server */

static void drawlist(void);     /* forward */
static void drawfolders(void);
static void showfolder(int i);
static void drawread(void);
static void layout(int whole);
static void drawfolders(void);
static void drawlist(void);

/*******************************************************************************

Odds and ends

*******************************************************************************/

/* A colour named the way everybody names one, in parts of 255.

   The library takes each part as a fraction of the whole range of a
   long, not as a byte: 245 out of LONG_MAX is not a light grey, it is
   black, and every "quiet grey" in this program was black until this
   was noticed. Written once, here, so it cannot be got wrong twice.

   It answers an ami_long, which is what the colour calls take: answered
   as an int it was cut to the low bits of the long, and the banner and
   the status strip were black again. */
static ami_long rgb(int c)

{

    if (c < 0) c = 0;
    if (c > 255) c = 255;

    return ((ami_long)c*(LONG_MAX/255));

}

/* the time now, in milliseconds of no particular epoch: for telling a
   second click on a row from a first */
static ami_long msnow(void)

{

    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);

    return ((ami_long)t.tv_sec*1000+t.tv_nsec/1000000);

}

#define DBLMS 400 /* a second click within this many is a double click */

/* say something went wrong, in a way the user can see */
/* A fetch the timer started is a fetch nobody asked for, and a server
   that is down will fail every one of them. Putting a box on the screen
   for each is a program that cannot be left alone: come back after an
   hour away and there is an hour of boxes to dismiss. Failures nobody
   asked for go to the status line, where they can be read and ignored. */
static int quietfail;


/* Something the worker ran into. It cannot put a box on the screen --
   nothing on that thread may call the graphics at all -- so it leaves
   the words here and the main thread shows them on its next tick. Only
   one is kept: a fetch that has gone wrong goes wrong the same way over
   and over, and the reader wants to be told once. */
static int fetchasked; /* somebody asked for this fetch and is waiting */

void fail(const char* what)

{

    if (wrkgo || quietfail) { /* a fetch is running, or nobody asked */

        copystr(failsaid, what, sizeof(failsaid));
        failwait = TRUE;

        return;

    }
    ami_alert("Mail", (char*)what);

}


/*******************************************************************************

The message menu

The second mouse button on a message opens a small menu beside it. The
menu is a window like everything else here, serviced from the same event
loop by its id, and everything it offers is local: the server is not
touched, not even asked.

*******************************************************************************/

static FILE* popwf;      /* the menu, NULL when closed */
static int   popmsg;     /* the message it is for */
static int   poprow = -1; /* the entry under the mouse */
static int   poprowh;    /* the height of an entry */
static int   popx, popy, popw, poph; /* where it stands in the main window */
static char  poplab[3][MAXSTR]; /* the entries' faces */

/* The part of an address that says who sent it, for gathering their
   mail together. That is the domain and not the whole address: LinkedIn
   writes from messages-noreply@, notifications-noreply@, inmail-hit-
   reply@ and linkedin@em.linkedin.com, and a reader who asks for a
   folder of LinkedIn mail means all of it.

   Not for the domains people have their own addresses at, though. Two
   friends at gmail.com are two people, and sweeping every message from
   gmail.com into one folder is not what anybody meant. */
static void senderkey(const char* addr, char* key, int kl)

{

    static const char* personal[] = {

        "gmail.com", "googlemail.com", "yahoo.com", "hotmail.com",
        "outlook.com", "live.com", "icloud.com", "me.com", "aol.com",
        "protonmail.com", "proton.me", "gmx.com", "mail.com", NULL

    };
    const char* at = strchr(addr, '@');
    int         i;

    if (!at || !at[1]) { copystr(key, addr, kl); return; }
    for (i = 0; personal[i]; i++)
        if (!strcasecmp(at+1, personal[i]))
            { copystr(key, addr, kl); return; } /* a person, not a sender */
    /* The domain, less whatever it is a part of: selections.aliexpress.com
       and notice.aliexpress.com are one sender, aliexpress.com. The last
       two labels are kept, or three where the second is itself a suffix
       under a country, as co.uk. */
    {

        const char* d = at+1;
        const char* labels[64];
        int         n = 0;
        const char* p = d;

        labels[n++] = p;
        while (*p && n < 64) { if (*p == '.') labels[n++] = p+1; p++; }
        if (n >= 3) {

            const char* last = labels[n-1];
            const char* second = labels[n-2];
            int keep = 2;

            if (strlen(last) == 2 &&
                (!strncasecmp(second, "co.", 3) || !strncasecmp(second, "com.", 4) ||
                 !strncasecmp(second, "org.", 4) || !strncasecmp(second, "net.", 4) ||
                 !strncasecmp(second, "ac.", 3) || !strncasecmp(second, "gov.", 4) ||
                 !strncasecmp(second, "edu.", 4))) keep = 3;
            d = labels[n-keep];

        }
        copystr(key, d, kl);

    }

}


/* What to call a sender's folder: a domain by its own name, aliexpress
   for aliexpress.com; a person, whose key is their address, by the name
   they write under. */
static void sendername(const char* key, const char* from, char* name, int nl)

{

    const char* dot;

    if (strchr(key, '@')) { copystr(name, from, nl); return; }
    dot = strchr(key, '.');
    if (dot && dot-key < nl-1) { memcpy(name, key, dot-key); name[dot-key] = 0; }
    else copystr(name, key, nl);

}

/* does this message come from that sender? */
static int fromsender(const msgrec* m, const char* key)

{

    char k[100];

    senderkey(m->addr, k, sizeof(k));

    return (!strcasecmp(k, key));

}

static void popclose(void)

{

    if (popwf) { fclose(popwf); popwf = NULL; }
    poprow = -1;

}

static void popdraw(void)

{

    int i;
    ami_long w = ami_maxxg(popwf);
    ami_long h = ami_maxyg(popwf);

    fprintf(popwf, "\f");
    /* the edge, so it reads as a card over the list */
    ami_fcolorc(popwf, rgb(120), rgb(120), rgb(120));
    ami_line(popwf, 0, 0, w-1, 0);
    ami_line(popwf, 0, h-1, w-1, h-1);
    ami_line(popwf, 0, 0, 0, h-1);
    ami_line(popwf, w-1, 0, w-1, h-1);
    ami_fcolor(popwf, ami_black);
    for (i = 0; i < 3; i++) {

        ami_cursorg(popwf, 8, 3+i*poprowh+(poprowh-chrh)/2);
        fprintf(popwf, "%s", poplab[i]);

    }

}

/* open the menu for a message, beside the mouse */
static void popopen(int i, int x, int y)

{

    int w, h;
    char nm[60];

    popclose();
    popmsg = i;
    copystr(nm, msgs[i].from, sizeof(nm));
    snprintf(poplab[0], sizeof(poplab[0]), "Move to local Trash");
    {

        char key[100];
        int n;

        /* Say what it will gather, since the sender's name and what
           their mail comes from are not always the same word. */
        senderkey(msgs[i].addr, key, sizeof(key));
        n = 0;
        {

            int m;

            for (m = 0; m < msgct; m++) if (fromsender(&msgs[m], key)) n++;

        }
        {

            char name[60];

            sendername(key, msgs[i].from, name, sizeof(name));
            snprintf(poplab[1], sizeof(poplab[1]),
                     "Local folder for %s (%d here)", name, n);

        }
        /* And by the name they show, which is not the same thing: a
           domain gathers eight sorts of Facebook notice into one folder,
           and a name keeps a person who writes through LinkedIn out of
           the LinkedIn folder. Both are offered, with what each would
           take, because which is wanted depends on the sender. */
        n = 0;
        {

            int m;

            for (m = 0; m < msgct; m++)
                if (!strcmp(msgs[m].from, msgs[i].from)) n++;

        }
        snprintf(poplab[2], sizeof(poplab[2]),
                 "Local folder for \"%s\" (%d here)", nm, n);

    }
    poprowh = chrh+10;
    w = ami_strsiz(listwf, poplab[0]);
    if (ami_strsiz(listwf, poplab[1]) > w) w = ami_strsiz(listwf, poplab[1]);
    if (ami_strsiz(listwf, poplab[2]) > w) w = ami_strsiz(listwf, poplab[2]);
    w += 20;
    h = poprowh*3+6;
    /* The menu is a child of the main window, not of the list: a child
       is clipped by its parent, and a menu opened near the bottom of
       the list would be cut off by it. The mouse position arrives in
       the list's coordinates, so it is shifted by where the list sits. */
    x += listx;
    y += listy;
    if (x+w > ami_maxxg(stdout)) x = ami_maxxg(stdout)-w;
    if (y+h > ami_maxyg(stdout)) y = ami_maxyg(stdout)-h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    popx = x; popy = y; popw = w; poph = h;
    ami_openwin(&stdin, &popwf, stdout, POPWIN);
    ami_frame(popwf, FALSE);
    ami_auto(popwf, FALSE);
    ami_curvis(popwf, FALSE);
    ami_font(popwf, AMI_FONT_SIGN);
    ami_setpoints(popwf, pointsz);
    ami_binvis(popwf);
    ami_setsizg(popwf, w, h);
    ami_setposg(popwf, x, y);
    /* in front of the panes: it is a sibling of them, and a menu behind
       the list it belongs to is a menu nobody can see */
    ami_front(popwf);
    popdraw();

}

/* what was picked: 0 is trash, 1 is a folder for the sender */
static void popact(int row)

{

    char* set;
    int   dst;
    int   i = popmsg;
    int   m, n;
    char  msg[MAXSTR];
    char  who[60];

    popclose();
    if (foldsel < 0 || i < 0 || i >= msgct) return;
    set = getmem(msgct);
    memset(set, 0, msgct);
    if (row == 0) { /* this one message, to the local trash */

        set[i] = TRUE;
        dst = localfolder("Trash");
        copystr(who, "Trash", sizeof(who));

    } else if (row == 1) { /* everything from that place */

        char key[100];

        senderkey(msgs[i].addr, key, sizeof(key));
        for (m = 0; m < msgct; m++)
            if (fromsender(&msgs[m], key)) set[m] = TRUE;
        sendername(key, msgs[i].from, who, sizeof(who));
        dst = localfolder(who);

    } else { /* everything from that name */

        for (m = 0; m < msgct; m++)
            if (!strcmp(msgs[m].from, msgs[i].from)) set[m] = TRUE;
        copystr(who, msgs[i].from, sizeof(who));
        dst = localfolder(who);

    }
    if (dst < 0) { free(set); fail("No room for another folder"); return; }
    /* The worker moves them: a mailbox of gigabytes takes a while to
       write out again, and the display stays live while it does. The
       folder is read again after, and the worker says how many went. */
    for (m = 0, n = 0; m < msgct; m++) n += set[m];
    /* the set is over the list's rows; the worker wants it over the
       folder's index, which the threads have put in another order */
    if (viewidx) {

        char* iset = getmem(folders[foldsel].idxct? folders[foldsel].idxct: 1);

        memset(iset, 0, folders[foldsel].idxct);
        for (m = 0; m < msgct; m++) if (set[m]) iset[viewidx[m]] = TRUE;
        movask(foldsel, dst, iset, folders[foldsel].idxct);
        free(iset);

    } else movask(foldsel, dst, set, msgct);
    free(set);
    msgsel = -1;
    msgct = 0;
    idxfold = -1;
    idxwant = foldsel;
    kickworker();
    drawlist();
    drawfolders();
    snprintf(movnote, sizeof(movnote), "%d message%s to %s", n,
             n == 1? "": "s", who);
    snprintf(msg, sizeof(msg), "Moving %s...", movnote);
    status(msg);

}


/*******************************************************************************

The display

Three windows. The folders are a pane down the left, the messages a pane
on the right, and a message opened to be read gets a window of its own.
They are all serviced from the one event loop: every event names the
window it came from, so there is nothing to arrange.

*******************************************************************************/

/* The scroll bar runs from 0 to LONG_MAX. The arithmetic is integer on
   purpose: LONG_MAX does not survive a trip through a double -- it
   rounds up to 2^63 and comes back negative -- and the bar answers an
   out of range position with an error. */
static ami_long fullscale(int num, int den)

{

    if (den < 1 || num <= 0) return (0);
    if (num >= den) return (LONG_MAX);

    return (LONG_MAX/den*num); /* num < den, so this cannot overflow */

}

/* Set while a bar's own position event is being answered. The bar has
   been echoed where the user put it; setting it again from the view
   would drag the thumb back to the nearest whole line, which on a bar
   with little travel is most of the way back where it started. */
static int fromdrag;

/* and back, from a bar position to a line number */
static int scaleback(ami_long pos, int travel)

{

    if (travel < 1 || pos <= 0) return (0);
    if (pos >= LONG_MAX) return (travel);

    return ((int)((double)travel*((double)pos/LONG_MAX)+0.5));

}

/* There is no status line any more: everything it said is said in the
   window itself -- the folder counts climb while a fetch runs, the list
   says what an empty folder is, and the forms speak for themselves. The
   calls remain as markers of where a quieter program once spoke. */
static int fitchars(FILE* f, char* s, int w);
static void clipstr(FILE* f, char* s, int w);
static void commas(int n, char* s, int sl);
static void divider(FILE* f, int x1, int y1, int x2, int y2);
static int progw; /* how wide the bar is */
static int progh; /* and how tall */

/*******************************************************************************

Writing a message

A window with the three fields a message needs and a body to type in.
The fields are edit boxes, which the library gives us; the body is not,
since there is no widget for more than one line of text, so it is kept
and drawn here.

The body is a list of lines and a place in one of them, which is the
model the editor in this kit uses and the simplest one that behaves the
way a person expects. Everything that types into it comes to the window
as an event: a click on an edit box takes the keys away, and a click on
the body gives them back, which is what the library does with focus and
what makes the two kinds of field live together.

*******************************************************************************/

#define CMPMAX  20000 /* the longest line worth calling a line */

static FILE*  cmpwf;            /* the window, when it is open */
static char** cmpline;          /* the body, a line at a time */
static int    cmpct;            /* how many lines */
static int    cmpmax;           /* room for how many */
static int    cmpcl, cmpcc;     /* the caret: which line, which column */
static int    cmptop;           /* the first line shown */
static int    cmprows;          /* how many fit */
static int    cmpy0;            /* where the body starts down the window */
static ami_long cmpsbw;           /* the bar beside it */
static int    cmpfocus;         /* the body has the keys */
static int    cmpmx, cmpmy;     /* where the mouse is in the window */
static char   cmpinreply[MAXSTR]; /* what this answers, if it answers */
static char   cmprefs[MAXSTR*2];

/* The message on show in the reader, in the pieces a reply is made of.
   Kept when it is opened, since the reply is written from the headers
   and the text rather than from the file again. */
static char   redfrom[MAXSTR];
static char   redto[MAXSTR];
static char   redcc[MAXSTR];
static char   redsubj[MAXSTR];
static char   reddate[MAXSTR];
static char   redid[MAXSTR];
static char   redrefs[MAXSTR*2];
static char*  redtext;

/* room for one more line */
static void cmproom(int n)

{

    if (n < cmpmax) return;
    cmpmax = cmpmax? cmpmax*2: 64;
    while (cmpmax <= n) cmpmax *= 2;
    cmpline = realloc(cmpline, cmpmax*sizeof(char*));
    if (!cmpline) { fail("Out of memory"); exit(1); }

}

static void cmpclear(void)

{

    int i;

    for (i = 0; i < cmpct; i++) free(cmpline[i]);
    cmpct = 0;
    cmpcl = 0;
    cmpcc = 0;
    cmptop = 0;

}

/* put a line in, at a place */
static void cmpput(int at, const char* text)

{

    int i;

    cmproom(cmpct+1);
    for (i = cmpct; i > at; i--) cmpline[i] = cmpline[i-1];
    cmpline[at] = getmem(strlen(text)+1);
    strcpy(cmpline[at], text);
    cmpct++;

}

static void cmptake(int at)

{

    int i;

    if (at < 0 || at >= cmpct) return;
    free(cmpline[at]);
    for (i = at; i < cmpct-1; i++) cmpline[i] = cmpline[i+1];
    cmpct--;

}

/* the whole of it, as one string, which is what sending wants */
static char* cmptext(void)

{

    int   n = 1;
    int   i;
    char* t;

    for (i = 0; i < cmpct; i++) n += strlen(cmpline[i])+1;
    t = getmem(n);
    *t = 0;
    for (i = 0; i < cmpct; i++) { strcat(t, cmpline[i]); strcat(t, "\n"); }

    return (t);

}

/*******************************************************************************

The compose window: laying it out, drawing it, and answering it

*******************************************************************************/

static void cmpclose(void)

{

    if (!cmpwf) return;
    ami_killwidget(cmpwf, CMPTO);
    ami_killwidget(cmpwf, CMPCC);
    ami_killwidget(cmpwf, CMPSUB);
    ami_killwidget(cmpwf, CMPSEND);
    ami_killwidget(cmpwf, CMPCAN);
    ami_killwidget(cmpwf, CMPSB);
    fclose(cmpwf); /* which is how a window is closed here */
    cmpwf = NULL;
    cmpclear();

}

/* how many lines of the body are on show */
static int cmpvis(void)

{

    ami_long n = (ami_maxyg(cmpwf)-cmpy0-8)/chrh;

    return (n < 1? 1: n);

}

static void cmpbar(void)

{

    if (cmpct > cmpvis())
        ami_scrollsiz(cmpwf, CMPSB, fullscale(cmpvis(), cmpct));
    else ami_scrollsiz(cmpwf, CMPSB, INT_MAX);
    ami_scrollpos(cmpwf, CMPSB, fullscale(cmptop, cmpct-1));

}

/* the body, and the caret in it */
static void cmpdraw(void)

{

    int i;
    int y;

    if (!cmpwf) return;
    ami_fcolor(cmpwf, ami_white);
    ami_frect(cmpwf, 1, cmpy0, ami_maxxg(cmpwf)-cmpsbw-2, ami_maxyg(cmpwf));
    ami_fcolor(cmpwf, ami_black);
    cmprows = cmpvis();
    if (cmpcl < cmptop) cmptop = cmpcl;
    if (cmpcl >= cmptop+cmprows) cmptop = cmpcl-cmprows+1;
    if (cmptop < 0) cmptop = 0;
    y = cmpy0+4;
    for (i = cmptop; i < cmpct && i < cmptop+cmprows; i++) {

        ami_cursorg(cmpwf, 8, y);
        fprintf(cmpwf, "%s", cmpline[i]);
        if (i == cmpcl && cmpfocus) { /* the caret, where the typing goes */

            char  upto[CMPMAX];
            int   x;

            copystr(upto, cmpline[i], sizeof(upto));
            if (cmpcc < (int)strlen(upto)) upto[cmpcc] = 0;
            x = 8+ami_strsiz(cmpwf, upto);
            ami_fcolorc(cmpwf, rgb(200), rgb(40), rgb(40));
            ami_linewidth(cmpwf, 2);
            ami_line(cmpwf, x, y, x, y+chrh);
            ami_linewidth(cmpwf, 1);
            ami_fcolor(cmpwf, ami_black);

        }
        y += chrh;

    }
    cmpbar();

}

static void cmplay(void)

{

    ami_long chrw = ami_strsiz(cmpwf, "0");
    ami_long labw = ami_strsiz(cmpwf, "Subject  ");
    ami_long ew, eh, bw, bh;
    int y;
    static const char* lab[] = { "To", "Cc", "Subject" };
    static const int   wid[] = { CMPTO, CMPCC, CMPSUB };
    int i;

    ami_editboxsizg(cmpwf, "0", &ew, &eh);
    ami_buttonsizg(cmpwf, "Cancel", &bw, &bh);
    fprintf(cmpwf, "\f");
    ami_fcolor(cmpwf, ami_black);
    y = chrh/2;
    for (i = 0; i < 3; i++) {

        ami_cursorg(cmpwf, chrw, y+(eh-chrh)/2);
        fprintf(cmpwf, "%s", lab[i]);
        ami_poswidgetg(cmpwf, wid[i], chrw+labw, y);
        ami_sizwidgetg(cmpwf, wid[i], ami_maxxg(cmpwf)-labw-chrw*2, eh);
        y += eh+chrh/4;

    }
    y += chrh/4;
    ami_poswidgetg(cmpwf, CMPSEND, chrw+labw, y);
    ami_sizwidgetg(cmpwf, CMPSEND, bw, bh);
    ami_poswidgetg(cmpwf, CMPCAN, chrw+labw+bw+chrw*2, y);
    ami_sizwidgetg(cmpwf, CMPCAN, bw, bh);
    y += bh+chrh/2;
    /* the line that says the message begins here */
    divider(cmpwf, 1, y, ami_maxxg(cmpwf), y);
    cmpy0 = y+2;
    ami_poswidgetg(cmpwf, CMPSB, ami_maxxg(cmpwf)-cmpsbw, cmpy0);
    ami_sizwidgetg(cmpwf, CMPSB, cmpsbw, ami_maxyg(cmpwf)-cmpy0);
    cmpdraw();

}

/* Open it, with whatever is already known filled in: a reply comes with
   everything but the words. */
static void cmpopen(const char* to, const char* cc, const char* subject,
                    const char* body, const char* inreply, const char* refs)

{

    ami_long wx, wy;
    const char* p;

    if (cmpwf) { ami_front(cmpwf); return; } /* one at a time */
    ami_openwin(&stdin, &cmpwf, NULL, CMPWIN);
    ami_title(cmpwf, "Write a message");
    ami_auto(cmpwf, FALSE);
    ami_curvis(cmpwf, FALSE);
    ami_font(cmpwf, AMI_FONT_SIGN);
    ami_setpoints(cmpwf, pointsz);
    ami_binvis(cmpwf);
    ami_winclientg(cmpwf, ami_strsiz(cmpwf, "0")*90, chrh*34, &wx, &wy,
                   BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(cmpwf, wx, wy);
    {

        ami_long ew, eh, bw, bh;

        ami_editboxsizg(cmpwf, "0", &ew, &eh);
        ami_buttonsizg(cmpwf, "Cancel", &bw, &bh);
        ami_editboxg(cmpwf, 1, 1, 100, 1+eh, CMPTO);
        ami_editboxg(cmpwf, 1, 1, 100, 1+eh, CMPCC);
        ami_editboxg(cmpwf, 1, 1, 100, 1+eh, CMPSUB);
        ami_buttong(cmpwf, 1, 1, 1+bw, 1+bh, "Send", CMPSEND);
        ami_buttong(cmpwf, 1, 1, 1+bw, 1+bh, "Cancel", CMPCAN);
        ami_scrollvertsizg(cmpwf, &cmpsbw, &eh);
        ami_scrollvertg(cmpwf, 1, 1, cmpsbw, chrh*10, CMPSB);

    }
    ami_putwidgettext(cmpwf, CMPTO, (char*)(to? to: ""));
    ami_putwidgettext(cmpwf, CMPCC, (char*)(cc? cc: ""));
    ami_putwidgettext(cmpwf, CMPSUB, (char*)(subject? subject: ""));
    copystr(cmpinreply, inreply? inreply: "", sizeof(cmpinreply));
    copystr(cmprefs, refs? refs: "", sizeof(cmprefs));
    /* the body, broken where its lines break */
    cmpclear();
    for (p = body? body: ""; ; ) {

        const char* q = p;
        char        one[CMPMAX];
        int         n;

        while (*q && *q != '\n') q++;
        n = q-p;
        if (n > CMPMAX-1) n = CMPMAX-1;
        memcpy(one, p, n);
        one[n] = 0;
        while (n && (one[n-1] == '\r')) one[--n] = 0;
        cmpput(cmpct, one);
        if (!*q) break;
        p = q+1;

    }
    if (!cmpct) cmpput(0, "");
    cmpcl = 0;
    cmpcc = 0;
    cmpfocus = TRUE;
    cmplay();

}

/* one character into the line the caret is on */
static void cmpins(char c)

{

    char* l = cmpline[cmpcl];
    int   n = strlen(l);
    char* d;

    if (n+2 > CMPMAX) return;
    d = getmem(n+2);
    memcpy(d, l, cmpcc);
    d[cmpcc] = c;
    strcpy(d+cmpcc+1, l+cmpcc);
    free(cmpline[cmpcl]);
    cmpline[cmpcl] = d;
    cmpcc++;

}

/* the line broken where the caret is */
static void cmpsplit(void)

{

    char* l = cmpline[cmpcl];
    char* rest = getmem(strlen(l+cmpcc)+1);

    strcpy(rest, l+cmpcc);
    l[cmpcc] = 0;
    cmpput(cmpcl+1, rest);
    free(rest);
    cmpcl++;
    cmpcc = 0;

}

/* and joined to the one after it */
static void cmpjoin(int at)

{

    char* a;
    char* b;
    char* d;

    if (at < 0 || at+1 >= cmpct) return;
    a = cmpline[at];
    b = cmpline[at+1];
    d = getmem(strlen(a)+strlen(b)+1);
    strcpy(d, a);
    strcat(d, b);
    free(cmpline[at]);
    cmpline[at] = d;
    cmptake(at+1);

}

/* Everything typed into the body. The fields beside it are edit boxes
   and answer for themselves; this is the part with no widget to do it. */
static void cmpkey(ami_evtrec* er)

{

    int n = (int)strlen(cmpline[cmpcl]);

    switch (er->etype) {

        case ami_etchar:
            if (er->echar >= ' ' && er->echar < 0x7f) cmpins(er->echar);
            break;
        case ami_etenter: cmpsplit(); break;
        case ami_ettab: { int i; for (i = 0; i < 4; i++) cmpins(' '); break; }
        case ami_etdelcb: /* backspace: within the line, or the break above */
            if (cmpcc > 0) {

                char* l = cmpline[cmpcl];

                memmove(l+cmpcc-1, l+cmpcc, strlen(l+cmpcc)+1);
                cmpcc--;

            } else if (cmpcl > 0) {

                cmpcc = strlen(cmpline[cmpcl-1]);
                cmpjoin(cmpcl-1);
                cmpcl--;

            }
            break;
        case ami_etdelcf:
            if (cmpcc < n) {

                char* l = cmpline[cmpcl];

                memmove(l+cmpcc, l+cmpcc+1, strlen(l+cmpcc+1)+1);

            } else cmpjoin(cmpcl);
            break;
        case ami_etdell:
            if (cmpct > 1) { cmptake(cmpcl); if (cmpcl >= cmpct) cmpcl = cmpct-1; }
            else { free(cmpline[0]); cmpline[0] = getmem(1); *cmpline[0] = 0; }
            cmpcc = 0;
            break;
        case ami_etleft:
            if (cmpcc > 0) cmpcc--;
            else if (cmpcl > 0) { cmpcl--; cmpcc = strlen(cmpline[cmpcl]); }
            break;
        case ami_etright:
            if (cmpcc < n) cmpcc++;
            else if (cmpcl < cmpct-1) { cmpcl++; cmpcc = 0; }
            break;
        case ami_etup: if (cmpcl > 0) cmpcl--; break;
        case ami_etdown: if (cmpcl < cmpct-1) cmpcl++; break;
        case ami_etpagu: cmpcl -= cmpvis()-1; if (cmpcl < 0) cmpcl = 0; break;
        case ami_etpagd:
            cmpcl += cmpvis()-1;
            if (cmpcl > cmpct-1) cmpcl = cmpct-1;
            break;
        case ami_ethomel: case ami_ethomes: cmpcc = 0; break;
        case ami_etendl: case ami_etends: cmpcc = strlen(cmpline[cmpcl]); break;
        case ami_ethome: cmpcl = 0; cmpcc = 0; break;
        case ami_etend:
            cmpcl = cmpct-1;
            cmpcc = strlen(cmpline[cmpcl]);
            break;
        default: return;

    }
    /* the caret cannot stand past the end of the line it is on */
    n = (int)strlen(cmpline[cmpcl]);
    if (cmpcc > n) cmpcc = n;
    cmpdraw();

}

/* Send what has been written. The sending itself is network and slow,
   so it is handed to the fetch thread; this only gathers it up. */
static void cmpdosend(void)

{

    char  to[MAXSTR], cc[MAXSTR], sub[MAXSTR];
    char* body;

    ami_getwidgettext(cmpwf, CMPTO, to, sizeof(to));
    ami_getwidgettext(cmpwf, CMPCC, cc, sizeof(cc));
    ami_getwidgettext(cmpwf, CMPSUB, sub, sizeof(sub));
    trim(to);
    if (!*to) { fail("There is nobody to send it to."); return; }
    body = cmptext();
    /* No lock here: this runs while the display is handling an event,
       and the display holds the lock for the whole of that. Taking it
       again is a thread waiting for itself, which is what the library
       calls a deadlock avoided and what it stops the program for. */
    copystr(outto, to, sizeof(outto));
    copystr(outcc, cc, sizeof(outcc));
    copystr(outsub, sub, sizeof(outsub));
    free(outbody);
    outbody = body;
    copystr(outinreply, cmpinreply, sizeof(outinreply));
    copystr(outrefs, cmprefs, sizeof(outrefs));
    sendwant = TRUE;
    kickworker();
    status("Sending...");
    cmpclose();

}

static void cmpevent(ami_evtrec* er)

{

    switch (er->etype) {

        case ami_etterm: cmpclose(); break;
        case ami_etresize:
            ami_sizbufg(cmpwf, er->rszxg, er->rszyg);
            cmplay();
            break;
        case ami_etredraw: cmplay(); break;
        case ami_etbutton:
            if (er->butid == CMPSEND) cmpdosend();
            else if (er->butid == CMPCAN) cmpclose();
            break;
        case ami_etmouba:
            /* A click in the body is what gives the keys back to the
               window after an edit box has had them, and it puts the
               caret where it was clicked. */
            if (er->amoubn == 1 && cmpmy >= cmpy0) {

                int l = cmptop+(cmpmy-cmpy0-4)/chrh;

                cmpfocus = TRUE;
                if (l < 0) l = 0;
                if (l > cmpct-1) l = cmpct-1;
                cmpcl = l;
                { /* the column the click landed nearest */

                    char upto[CMPMAX];
                    int i;

                    copystr(upto, cmpline[cmpcl], sizeof(upto));
                    for (i = 0; upto[i]; i++) {

                        char c = upto[i+1];

                        upto[i+1] = 0;
                        if (8+ami_strsiz(cmpwf, upto) > cmpmx) break;
                        upto[i+1] = c;

                    }
                    cmpcc = i;

                }
                cmpdraw();

            } else if (er->amoubn == 1) cmpfocus = FALSE;
            break;
        case ami_etmoumovg: cmpmx = er->moupxg; cmpmy = er->moupyg; break;
        case ami_etsclull: cmptop--; if (cmptop < 0) cmptop = 0; cmpdraw(); break;
        case ami_etscldrl: cmptop++; cmpdraw(); break;
        case ami_etsclulp: cmptop -= cmpvis()-1; cmpdraw(); break;
        case ami_etscldrp: cmptop += cmpvis()-1; cmpdraw(); break;
        case ami_etsclpos:
            cmptop = scaleback(er->sclpos, cmpct-1);
            cmpdraw();
            break;
        default: cmpkey(er); break;

    }

}

/*******************************************************************************

The banner

A band across the top under the menu: the program's name at the left in
large type, and at the right the picture that goes with it. It is a
window of its own, like the panes, so it is placed and drawn in one
space and answers for its own redrawing.

The picture is a bitmap beside the program, since that is the one form
the library reads. It is loaded once; if it is not found the banner is
the name alone, which is a banner still.

*******************************************************************************/

static FILE* banwf;    /* the banner is a pane, like the others */
static int   banh;     /* how tall it is */
static int   picw;     /* the picture, at the size it was made */
static int   pich;
static int   havepic;
static int   picdw;    /* and at the size it is drawn now */
static int   picdh;

/* How big the banner is at the size the display is drawn at

   The picture goes with the rest: ami_picture fits it to whatever box it
   is given, so it is scaled from the size it was made by however far the
   point size has moved from the size the program starts at. The banner is
   then as tall as the picture wants.

   It is not let past a third of the window. The mail is what the program
   is for, and a cat that took the window at thirty-six points would leave
   nothing to read -- worse, a banner taller than the window would give
   the panes below it a negative height. Where it would go past, the
   picture is fitted to the room there is instead. */
static void banmeasure(void)

{

    int max = ami_maxyg(stdout)/3;

    picdw = picw*pointsz/BASEPOINT;
    picdh = pich*pointsz/BASEPOINT;
    banh = havepic? picdh+16: ami_chrsizy(banwf)+16;
    if (banh > max) {

        if (havepic && picdh > 0) { /* the picture takes what is left */

            int fit = max-16;

            if (fit < 1) fit = 1;
            picdw = picdw*fit/picdh;
            picdh = fit;

        }
        banh = max;

    }

}



/* The banner, or the part of it from x0 to the right edge. A resize
   moves the picture, which keeps to the right edge, and nothing else:
   the name stays where it is, so from the picture's old place or its
   new one, whichever is further left, to the edge is what is cleared
   and drawn again. Asked from 0 it is drawn whole. */
static void drawbanner(int x0)

{

    int y;

    if (!banwf) return;
    if (x0 > 0) {

        char nm[MAXSTR];

        /* the name runs under the picture only when the band is too
           narrow for both; then it is all drawn again */
        copystr(nm, "Ami Mail", sizeof(nm));
        if (16+ami_strsiz(banwf, nm) > x0) x0 = 0;

    }
    if (x0 > 0) {

        ami_fcolor(banwf, ami_white);
        ami_frect(banwf, x0, 0, ami_maxxg(banwf), banh);
        ami_fcolor(banwf, ami_black);

    } else {

        ami_bcolorc(banwf, rgb(255), rgb(255), rgb(255));
        fprintf(banwf, "\f");

    }
    /* the name, at the left, set in the middle of the band */
    if (!x0) {

        ami_fcolorc(banwf, rgb(40), rgb(40), rgb(60));
        ami_cursorg(banwf, 16, (banh-ami_chrsizy(banwf))/2);
        {   /* it keeps to the room the picture leaves it: at a large
               point size in a small window the name is wider than the
               band, and it ran in under the cat */

            char nm[MAXSTR];

            copystr(nm, "Ami Mail", sizeof(nm));
            clipstr(banwf, nm, ami_maxxg(banwf)-(havepic? picdw+16: 0)-32);
            fprintf(banwf, "%s", nm);

        }
        ami_fcolor(banwf, ami_black);

    }
    /* and the picture at the right, at the size the display is drawn at */
    if (havepic)
        ami_picture(banwf, BANPIC, ami_maxxg(banwf)-picdw-16, (banh-picdh)/2,
                    ami_maxxg(banwf)-16, (banh-picdh)/2+picdh);
    /* Two lines under it, which is what says the banner is not part of
       what is below it. */
    y = banh-4;
    ami_fcolorc(banwf, rgb(120), rgb(120), rgb(140));
    ami_linewidth(banwf, 2);
    ami_line(banwf, x0? x0: 1, y, ami_maxxg(banwf), y);
    ami_line(banwf, x0? x0: 1, y+4, ami_maxxg(banwf), y+4);
    ami_linewidth(banwf, 1);
    ami_fcolor(banwf, ami_black);

}

/*******************************************************************************

The status bar

Anything that takes longer than an instant says so. The line at the foot
of the window carries what is being done and, when the work has a known
size, a bar showing how far along it is.

The measure is the event horizon: about a thirtieth of a second, the
point at which a delay stops being instantaneous and becomes a wait. A
program that goes quiet across a wait cannot be told from one that has
fallen over -- which is exactly what a fetch of seventy thousand
messages looked like before there was a thread to fetch on and a line to
say so.

*******************************************************************************/

static char statsaid[MAXSTR]; /* what the line says now */
static int stath;            /* how tall the strip is */
static int statmax;          /* the size of the job, or none if not known */
static int statpos;          /* and how far into it */

/* the strip's own drawing, text and all */
/* The strip is a band at the foot of the main window, in the room the
   panes are laid out to leave.

   Everything in it is drawn -- the band, the words and the bar -- and
   nothing in it is a widget. A bar drawn beside the words it belongs to
   is four lines of code, needs no window of its own, and is placed by
   the same arithmetic as everything else in the strip. Everything in it is drawn -- the band,
   the words and the bar -- and nothing in it is a widget.

   The bar was a widget to begin with, and it cost two days to learn why
   it should not be. A widget is a window of its own, placed in its own
   right, and in a window carrying a menu what is placed and what is
   drawn do not land in the same place. Drawing a bar is four lines of
   code and puts it exactly where the words beside it are. */
static void drawstatus(void)

{

    int y = ami_maxyg(stdout)-stath;         /* at the foot */
    int bx = ami_maxxg(stdout)-progw-8;
    int by = y+(stath-progh)/2;

    /* the band, laid down as one line as thick as the strip: a filled
       rectangle does not paint in this window, a line does */
    ami_fcolorc(stdout, rgb(245), rgb(245), rgb(245));
    ami_linewidth(stdout, stath);
    ami_line(stdout, 1, y+stath/2, ami_maxxg(stdout), y+stath/2);
    ami_linewidth(stdout, 1);
    ami_fcolor(stdout, ami_black);
    divider(stdout, 1, y, ami_maxxg(stdout), y);
    if (*statsaid) {

        char t[MAXSTR];

        copystr(t, statsaid, sizeof(t));
        /* it shares the strip with the bar, and never writes over it */
        fitchars(stdout, t, bx-16);
        ami_cursorg(stdout, 8, y+(stath-chrh)/2+1);
        ami_fcolorc(stdout, rgb(80), rgb(80), rgb(80));
        fprintf(stdout, "%s", t);
        ami_fcolor(stdout, ami_black);

    }
    /* the bar: what is done, then what is left, then a line round both */
    if (statmax > 0) {

        int w = progw*statpos/statmax;

        if (w > 0) {

            ami_fcolorc(stdout, rgb(120), rgb(60), rgb(130));
            ami_linewidth(stdout, progh-2);
            ami_line(stdout, bx+1, by+progh/2, bx+w, by+progh/2);

        }
        if (w < progw) {

            ami_fcolorc(stdout, rgb(225), rgb(225), rgb(225));
            ami_linewidth(stdout, progh-2);
            ami_line(stdout, bx+w+1, by+progh/2, bx+progw, by+progh/2);

        }
        ami_linewidth(stdout, 1);
        ami_fcolorc(stdout, rgb(150), rgb(150), rgb(150));
        ami_rect(stdout, bx, by, bx+progw, by+progh);
        ami_fcolor(stdout, ami_black);

    }

}

void status(const char* s)

{

    if (!s) s = "";
    if (!strcmp(s, statsaid)) return; /* it already says that */
    copystr(statsaid, s, sizeof(statsaid));
    drawstatus();

}

/* How far along, from nothing to all of it. The widget takes the whole
   range of a long, so the fraction is worked out in that range rather
   than in percent, which would step the bar in hundredths. */
void statprog(ami_long pos, ami_long max)

{

    if (max <= 0) { /* no size to it: there is no bar */

        if (statmax) { statmax = 0; statpos = 0; drawstatus(); }

        return;

    }
    if (pos < 0) pos = 0;
    if (pos > max) pos = max;
    if (statmax == max && statpos == pos) return; /* it is already there */
    statmax = max;
    statpos = pos;
    drawstatus();

}

/* How many characters of this string fit in the width, found by
   halving. Taking a character off and measuring the whole string again
   until it fits measures the string as many times as there are
   characters to lose, and every measurement walks the string: a few
   hundred characters cost a few hundred measurements, and a message
   list redraws every row of forty on every click. Halving costs the
   logarithm of that -- eight or nine measurements instead of four
   hundred. */
/* the width of an average character of the font in use, for guessing
   with; measured once, since the font does not change under us */
static int avgchrw(FILE* f)

{

    static int w;

    if (!w) w = ami_strsiz(f, "abcdefghijklmnopqrstuvwxyz")/26;
    if (w < 1) w = 1;

    return (w);

}

static int fitchars(FILE* f, char* s, int w)

{

    int n = strlen(s);
    int lo = 0, hi;
    int cap;
    char c;

    /* No room at all, and nothing of it fits. A width arrives negative
       when a column is squeezed past what there is to give it, and the
       cap below would then be negative -- s[cap] writes in front of the
       string, which is a caller's stack. */
    if (w <= 0) return (0);
    /* Cut it down before measuring it. Measuring costs a walk of the
       whole string, so the four hundred characters of a snippet are
       walked by every measurement even though sixty of them will fit.
       Three times the average character's worth is far more than can
       fit and costs nothing to work out. */
    cap = w/avgchrw(f)*3+8;
    if (n > cap) { s[cap] = 0; n = cap; }
    hi = n;
    if (ami_strsiz(f, s) <= w) return (n); /* it all fits */
    while (lo < hi) {

        int mid = (lo+hi+1)/2;

        c = s[mid];
        s[mid] = 0;
        if (ami_strsiz(f, s) <= w) lo = mid; else hi = mid-1;
        s[mid] = c;

    }

    return (lo);

}

/* The dividers between the parts of the display, all drawn alike: a
   quiet grey, so they mark the columns off without shouting over the
   text the way black rules would. */
static void divider(FILE* f, int x1, int y1, int x2, int y2)

{

    ami_fcolorc(f, rgb(180), rgb(180), rgb(180));
    ami_linewidth(f, 2);
    ami_line(f, x1, y1, x2, y2);
    ami_linewidth(f, 1);
    ami_fcolor(f, ami_black);

}

/* A number written the way a mail reader writes it, in threes: eleven
   thousand messages reads as 11,421 and not as 11421. */
static void commas(int n, char* s, int sl)

{

    char b[40];
    int i, o = 0, l;

    snprintf(b, sizeof(b), "%d", n);
    l = strlen(b);
    for (i = 0; i < l && o < sl-2; i++) {

        if (i && (l-i)%3 == 0) s[o++] = ',';
        s[o++] = b[i];

    }
    s[o] = 0;

}

/* cut a string to fit a width, with an ellipsis if it had to be cut */
static void clipstr(FILE* f, char* s, int w)

{

    int n = fitchars(f, s, w);

    if (n >= (int)strlen(s)) return; /* nothing to cut */
    s[n] = 0;
    if (n > 3) strcpy(s+n-3, "...");

}

/* One folder's line: its ground, its name and its count. Drawn by the
   pane, and again on its own when its count changes. */
static void drawfoldline(int i, int y, int w)

{

    char     nm[MAXSTR];
    char     cnt[40];
    ami_long cw;

    /* its own ground: the mark if it is the one being read, white if it
       is not */
    ami_fcolor(foldwf, i == foldsel? ami_cyan: ami_white);
    ami_frect(foldwf, 2, y-2, w-2, y+chrh);
    ami_fcolor(foldwf, ami_black);
    copystr(nm, folders[i].show, MAXSTR);
    cnt[0] = 0;
    if (folders[i].msgs > 0) commas(folders[i].msgs, cnt, sizeof(cnt));
    foldcnt[i] = folders[i].msgs;
    cw = *cnt? ami_strsiz(foldwf, cnt)+8: 0;
    ami_bold(foldwf, i == foldsel);
    clipstr(foldwf, nm, w-16-cw);
    ami_cursorg(foldwf, 8, y);
    fprintf(foldwf, "%s", nm);
    if (*cnt) {

        ami_cursorg(foldwf, w-8-ami_strsiz(foldwf, cnt), y);
        fprintf(foldwf, "%s", cnt);

    }
    ami_bold(foldwf, FALSE);

}

/* The counts that changed since the pane was drawn: those lines and no
   others. A fetch changes one count at a time, and one line drawn again
   twice a second is a flicker the eye cannot find; the pane drawn again
   for each was one it could not miss. */
static void drawcounts(void)

{

    int i;

    if (!foldwf) return;
    for (i = 0; i < foldct && i < MAXFOLDER; i++)
        if (foldon[i] && folders[i].msgs != foldcnt[i])
            drawfoldline(i, foldy[i], ami_maxxg(foldwf));

}

/* The folder pane, or the band of it between two heights: a rule, a
   heading or a folder line is drawn when it lies in the band and passed
   over when it does not. The whole pane is the band from top to bottom;
   a resize that made the pane taller asks only for the band it exposed,
   and the rest of the pane is not touched. */
static void drawfoldrect(int y1, int y2)

{

    int i;
    int y = 4;
    int sec;
    int w;
    char head[120];

    if (!foldwf) return;
    w = ami_maxxg(foldwf);
    for (i = 0; i < MAXFOLDER; i++) foldon[i] = FALSE;
    /* No clearing of the pane first. Every line paints its own ground
       before its text, so a clear only puts up a blank that the drawing
       covers again -- and a pane that goes blank and comes back is a
       blink, which during a fetch happens over and over. What is
       cleared is the room below the last line, which nothing else
       reaches. */
    ami_fcolor(foldwf, ami_black);
    /* a section for each server, then one for the folders that are ours
       alone, since two servers may each have an INBOX and a local Trash
       is not the server's Trash */
    for (sec = 0; sec <= srvct; sec++) {

        int shown = 0;
        int srv = sec < srvct? sec: -1;

        if (sec) { /* a rule between the sections */

            y += chrh/2;
            if (y+2 >= y1 && y-2 <= y2) {

                ami_fcolor(foldwf, ami_white);
                ami_frect(foldwf, 0, y-2, w, y+2);
                ami_fcolor(foldwf, ami_black);
                divider(foldwf, 6, y, w-8, y);

            }
            y += chrh;

        }
        if (y+chrh+2 >= y1 && y-2 <= y2) {

            if (srv >= 0) snprintf(head, sizeof(head), "%s Server Folders",
                                   servers[srv].name);
            else copystr(head, "Local Folders", sizeof(head));
            clipstr(foldwf, head, w-12);
            ami_fcolor(foldwf, ami_white);
            ami_frect(foldwf, 0, y-2, w, y+chrh+2);
            ami_fcolor(foldwf, ami_black);
            ami_bold(foldwf, TRUE);
            ami_cursorg(foldwf, 6, y);
            fprintf(foldwf, "%s", head);
            ami_bold(foldwf, FALSE);

        }
        y += chrh*2;
        for (i = 0; i < foldct; i++) {

            if (folders[i].srv != srv) continue;
            if (folders[i].local != (srv < 0)) continue;
            shown++;
            foldy[i] = y; /* where it landed, for the click to find */
            foldon[i] = TRUE;
            if (y+chrh >= y1 && y-2 <= y2) drawfoldline(i, y, w);
            y += chrh+4;

        }
        if (!shown) { /* say so rather than leave a gap */

            if (y+chrh >= y1 && y-2 <= y2) {

                ami_fcolor(foldwf, ami_white);
                ami_frect(foldwf, 0, y-2, w, y+chrh);
                ami_fcolorc(foldwf, rgb(130), rgb(130), rgb(130));
                ami_cursorg(foldwf, 8, y);
                fprintf(foldwf, "%s", srv >= 0? "(not fetched)": "(none yet)");
                ami_fcolor(foldwf, ami_black);

            }
            y += chrh+4;

        }

    }
    if (y < y1) y = y1;
    if (y < ami_maxyg(foldwf)) { /* the room the sections did not fill */

        ami_fcolor(foldwf, ami_white);
        ami_frect(foldwf, 0, y, w, ami_maxyg(foldwf));
        ami_fcolor(foldwf, ami_black);

    }

}

static void drawfolders(void)

{

    if (foldwf) drawfoldrect(0, ami_maxyg(foldwf));

}


/* One line of the message list: who it is from, then the subject, then
   as much of the message as is left over, then when it came. This is the
   layout the web readers use and it is the right one: the eye runs down
   the senders, and the subject and the start of the text read as one
   sentence. */
/* The subject and the start of the message as the row shows them: what
   is drawn where, in which weight, worked out from where the date
   column begins. Worked out here for the row drawn whole and for the
   row's tail drawn after a resize, so the two agree to the pixel. */
typedef struct {

    int  x;    /* where the piece starts */
    int  bold;
    char s[MAXSTR+SNIPPET];

} rowpiece;

static int rowpieces(const msgrec* m, int dx, int depth, rowpiece* pc)

{

    int n = 0;
    int x = catx+8+depth*ami_strsiz(listwf, "0")*2; /* a reply stands in */

    ami_bold(listwf, TRUE);
    pc[n].x = x;
    pc[n].bold = TRUE;
    copystr(pc[n].s, m->subject, MAXSTR);
    clipstr(listwf, pc[n].s, dx-x-8);
    x += ami_strsiz(listwf, pc[n].s);
    n++;
    ami_bold(listwf, FALSE);
    if (*m->snip && x < dx-ami_strsiz(listwf, "  ")) {

        pc[n].x = x;
        pc[n].bold = FALSE;
        snprintf(pc[n].s, sizeof(pc[n].s), " - %s", m->snip);
        clipstr(listwf, pc[n].s, dx-x-8);
        n++;

    }

    return (n);

}

static void drawmsg(int i, int y)

{

    msgrec* m = &msgs[i];
    ami_long w = ami_maxxg(listwf)-sbw;
    char    s[MAXSTR+SNIPPET];

    if (i == msgsel) {

        ami_fcolor(listwf, ami_cyan);
        ami_frect(listwf, 0, y-2, w, y+rowh-4);
        ami_fcolor(listwf, ami_black);

    }
    /* The sender. Bold goes on before the fit is measured: bold is
       wider than regular, so a name measured regular and drawn bold
       runs past the divider it was cut to. */
    ami_bold(listwf, TRUE);
    copystr(s, m->from, MAXSTR);
    clipstr(listwf, s, fromx-14);
    ami_cursorg(listwf, 6, y);
    fprintf(listwf, "%s", s);
    /* what kind of mail it is, in its own column and in a quieter grey:
       it is there to be glanced past, not read */
    copystr(s, m->cat, sizeof(s));
    clipstr(listwf, s, catx-fromx-16);
    ami_fcolorc(listwf, rgb(110), rgb(110), rgb(110));
    ami_cursorg(listwf, fromx+8, y);
    fprintf(listwf, "%s", s);
    ami_fcolor(listwf, ami_black);
    /* the subject, then the start of the message after it */
    {

        rowpiece pc[2];
        int      n = rowpieces(m, datex, viewdepth? viewdepth[i]: 0, pc);
        int      k;

        for (k = 0; k < n; k++) {

            ami_bold(listwf, pc[k].bold);
            ami_cursorg(listwf, pc[k].x, y);
            fprintf(listwf, "%s", pc[k].s);

        }
        ami_bold(listwf, FALSE);

    }
    /* the date, against the right, asked again from the date: the index
       keeps the string as it was when the message was indexed */
    if (m->date) whenof(m->date, s, sizeof(s));
    else copystr(s, m->when, sizeof(m->when));
    ami_cursorg(listwf, w-ami_strsiz(listwf, s)-8, y);
    fprintf(listwf, "%s", s);
    /* This row's piece of the column dividers. Drawn with the row so
       that a row drawn on its own -- the selection moving, a scroll
       filling in -- keeps the line whole. A vertical line survives a
       vertical scroll, so the pieces always join. */
    divider(listwf, fromx, y-2, fromx, y+rowh-4);
    divider(listwf, catx, y-2, catx, y+rowh-4);
    divider(listwf, datex-8, y-2, datex-8, y+rowh-4);

}

static void setlistbar(void);   /* forward */

/* A row after the width changed: only what the width moved. The sender
   and the kind stand where they were, and the subject and the snippet
   read the same up to the point where the old cut and the new one part;
   from there to the right edge the row is cleared and drawn again --
   the rest of the text, the date at its new place, the divider before
   it. On a window pulled in a little that is the date and a character
   or two, and the rest of the row is not touched, so it does not
   flash. */
static void drawtail(int i, int y, int olddx)

{

    msgrec*  m = &msgs[i];
    ami_long w = ami_maxxg(listwf)-sbw;
    rowpiece op[2], np[2];
    int      on, nn, k, x0;
    char     s[MAXSTR];

    on = rowpieces(m, olddx, viewdepth? viewdepth[i]: 0, op);
    nn = rowpieces(m, datex, viewdepth? viewdepth[i]: 0, np);
    /* where the two drawings part: a piece one has and the other has
       not, or the first character that differs; alike throughout, the
       divider before the date is the first thing that moved */
    x0 = (datex < olddx? datex: olddx)-8;
    for (k = 0; k < on || k < nn; k++) {

        int c;

        if (k >= on) { x0 = np[k].x; break; }
        if (k >= nn) { x0 = op[k].x; break; }
        if (op[k].x != np[k].x) { x0 = op[k].x < np[k].x? op[k].x: np[k].x; break; }
        for (c = 0; op[k].s[c] && op[k].s[c] == np[k].s[c]; c++);
        if (op[k].s[c] || np[k].s[c]) {

            memcpy(s, np[k].s, c);
            s[c] = 0;
            ami_bold(listwf, np[k].bold);
            x0 = np[k].x+ami_strsiz(listwf, s);
            ami_bold(listwf, FALSE);
            break;

        }

    }
    if (x0 < 0) x0 = 0;
    ami_fcolor(listwf, i == msgsel? ami_cyan: ami_white);
    ami_frect(listwf, x0, y-2, w, y+rowh-4);
    ami_fcolor(listwf, ami_black);
    for (k = 0; k < nn; k++) {

        const char* p = np[k].s;
        int         c = 0;

        ami_bold(listwf, np[k].bold);
        if (np[k].x < x0) { /* the part of it from x0 on */

            while (p[c]) {

                memcpy(s, p, c+1);
                s[c+1] = 0;
                if (np[k].x+ami_strsiz(listwf, s) > x0) break;
                c++;

            }
            if (!p[c]) continue; /* all of it stands to the left */

        }
        ami_cursorg(listwf, x0 > np[k].x? x0: np[k].x, y);
        fprintf(listwf, "%s", p+c);

    }
    ami_bold(listwf, FALSE);
    if (m->date) whenof(m->date, s, sizeof(s));
    else copystr(s, m->when, sizeof(m->when));
    ami_cursorg(listwf, w-ami_strsiz(listwf, s)-8, y);
    fprintf(listwf, "%s", s);
    divider(listwf, datex-8, y-2, datex-8, y+rowh-4);
    ami_fcolor(listwf, ami_white);
    ami_line(listwf, x0, y+rowh-3, w, y+rowh-3);
    ami_fcolor(listwf, ami_black);

}

/* every row on the screen, after the date column moved */
static void listtails(int olddx)

{

    int i;
    int y = 4;
    int x0 = (datex < olddx? datex: olddx)-9;

    if (!listwf) return;
    if (foldsel < 0 || !msgct) { drawlist(); return; }
    for (i = msgtop; i < msgct && y+rowh <= ami_maxyg(listwf); i++) {

        drawtail(i, y, olddx);
        y += rowh;

    }
    if (y < ami_maxyg(listwf)) { /* the divider down the empty part */

        if (x0 < 0) x0 = 0;
        ami_fcolor(listwf, ami_white);
        ami_frect(listwf, x0, y, ami_maxxg(listwf), ami_maxyg(listwf));
        ami_fcolor(listwf, ami_black);
        divider(listwf, datex-8, y, datex-8, ami_maxyg(listwf));

    }
    setlistbar();

}

/* Draw one row where it belongs, background and all. Moving from one
   message to the next changes two rows out of thirty, and drawing a row
   costs a few text writes, so drawing the two beats drawing the lot: a
   redraw of the whole list is over a tenth of a second, which is what
   made stepping down a folder feel slow. */
static void drawrow(int i)

{

    int y;

    if (!listwf || i < msgtop || i >= msgct) return;
    y = 4+(i-msgtop)*rowh;
    if (y+rowh > ami_maxyg(listwf)) return; /* not on the screen */
    ami_fcolor(listwf, ami_white);
    ami_frect(listwf, 0, y-2, ami_maxxg(listwf)-sbw, y+rowh-4);
    ami_fcolor(listwf, ami_black);
    drawmsg(i, y);
    ami_fcolor(listwf, ami_white);
    ami_line(listwf, 0, y+rowh-3, ami_maxxg(listwf)-sbw, y+rowh-3);
    ami_fcolor(listwf, ami_black);

}

/* move the mark from one message to another, without drawing the rest */
static void selectmsg(int i)

{

    int was = msgsel;

    if (i == msgsel) return;
    msgsel = i;
    if (was >= 0) drawrow(was);
    drawrow(i);

}

/* How many rows the list shows. The last one is only drawn if it fits
   whole, which is what the drawing loop does. */
static int listvis(void)

{

    ami_long n = (ami_maxyg(listwf)-4)/rowh;

    return (n < 1? 1: n);

}

/* keep the top of the list inside the list */
static void listclamp(void)

{

    if (msgtop > msgct-listvis()) msgtop = msgct-listvis();
    if (msgtop < 0) msgtop = 0;

}

/* Draw only the rows a redraw rectangle touches, and the dividers down
   it. A resize brings two of these -- the strip down the right and the
   strip along the bottom -- and they are the whole of what needs
   painting, since the buffer keeps what was already there. */
static void listrect(int y1, int y2)

{

    int i;
    int first = msgtop+(y1-4)/rowh;
    int last  = msgtop+(y2-4)/rowh;

    if (first < msgtop) first = msgtop;
    if (last >= msgct) last = msgct-1;
    divider(listwf, fromx, y1, fromx, y2);
    divider(listwf, catx, y1, catx, y2);
    divider(listwf, datex-8, y1, datex-8, y2);
    for (i = first; i <= last; i++) drawrow(i);
    setlistbar();

}

/* Move the list to where it is now supposed to be, by moving what is
   already on the screen. Going down a row brings one row into view;
   drawing thirty of them to show one is what made a wheel notch cost an
   eighth of a second. */
static void showlist(void)

{

    int vis;
    int d;

    if (!listwf || foldsel < 0 || !msgct) { drawlist(); return; }
    listclamp();
    vis = listvis();
    d = msgtop-listshown;
    if (!d) { setlistbar(); return; }
    if (d >= vis || d <= -vis) { drawlist(); return; } /* nothing kept */
    {

        struct timespec t0, t1;

        if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
        ami_scrollg(listwf, 0, d*rowh);
        listshown = msgtop;
        if (diag) {

            clock_gettime(CLOCK_MONOTONIC, &t1);
            fprintf(stderr, "listscroll: %d row%s %.1fms\n", d < 0? -d: d,
                    d == 1 || d == -1? "": "s",
                    (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);

        }

    }
    if (d > 0) { /* the new rows come in at the foot */

        int i;

        for (i = msgtop+vis-d; i < msgtop+vis; i++) drawrow(i);

    } else { /* and at the head */

        int i;

        for (i = msgtop; i < msgtop-d; i++) drawrow(i);

    }
    setlistbar();

}

static void setlistbar(void)

{

    int max = msgct-listrows;

    if (fromdrag) return;
    if (max < 1) max = 1;
    ami_scrollsiz(listwf, SBLIST, fullscale(listrows, msgct));
    ami_scrollpos(listwf, SBLIST, fullscale(msgtop, max));

}

static void drawlist(void)

{

    int i;
    int y = 4;
    struct timespec t0, t1;

    if (!listwf) return;
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
    /* No clearing of the whole pane first. Every row paints its own
       ground before its text, so a clear only puts up a blank that the
       drawing immediately covers -- and a blank surface followed by a
       redraw is a flash the reader can see. What is cleared is what the
       rows will not reach, below the last of them. */
    ami_fcolor(listwf, ami_black);
    if (foldsel < 0) {

        ami_cursorg(listwf, 8, y);
        fprintf(listwf, "Pick a folder.");
    
        return;

    }
    if (!msgct) {

        ami_cursorg(listwf, 8, y);
        /* An empty list means one of two things and they are not alike:
           there is nothing in the folder, or there is and it has not
           been read yet. Four gigabytes takes half a minute to read, and
           for that half minute the reader should not be told the folder
           is empty. */
        if (idxwant == foldsel || idxdoing == foldsel)
            fprintf(listwf, "Reading %s...", folders[foldsel].show);
        else fprintf(listwf, "Nothing in %s yet. Mail/Fetch reads the "
                     "server.", folders[foldsel].show);

        return;

    }
    /* the column dividers first, full height, so the rows overprint
       their own pieces and the empty part of the list is ruled too */
    divider(listwf, fromx, 0, fromx, ami_maxyg(listwf));
    divider(listwf, catx, 0, catx, ami_maxyg(listwf));
    divider(listwf, datex-8, 0, datex-8, ami_maxyg(listwf));
    for (i = msgtop; i < msgct && y+rowh <= ami_maxyg(listwf); i++) {

        drawrow(i);
        y += rowh;

    }
    if (y < ami_maxyg(listwf)) { /* the room the rows did not fill */

        ami_fcolor(listwf, ami_white);
        ami_frect(listwf, 0, y, ami_maxxg(listwf), ami_maxyg(listwf));
        ami_fcolor(listwf, ami_black);
        divider(listwf, fromx, y, fromx, ami_maxyg(listwf));
        divider(listwf, catx, y, catx, ami_maxyg(listwf));
        divider(listwf, datex-8, y, datex-8, ami_maxyg(listwf));

    }
    listshown = msgtop;
    setlistbar();
    if (diag) {

        clock_gettime(CLOCK_MONOTONIC, &t1);
        fprintf(stderr, "list: %d rows %.0fms\n", i-msgtop,
                (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);

    }

}

/*******************************************************************************

Reading one message

*******************************************************************************/

/* wrap the message to the window it is being read in */
/* one more line for the reader to show */
static void readput(const char* s)

{

    if (readlines >= readmax) {

        readmax = readmax? readmax*2: 200;
        readline = realloc(readline, readmax*sizeof(char*));
        if (!readline) { fail("Out of memory"); exit(1); }

    }
    readline[readlines] = getmem(strlen(s)+1);
    strcpy(readline[readlines++], s);

}

static void wrapread(void)

{

    const char* p;
    ami_long    w = ami_maxxg(readwf)-16-sbw;
    int         i;

    for (i = 0; i < readlines; i++) free(readline[i]);
    readlines = 0;
    if (!readtext) return;
    p = readtext;
    while (*p) {

        const char* e = p;
        char        line[MAXLINE];
        int         n;

        while (*e && *e != '\n') e++;
        n = e-p;
        if (n >= (int)sizeof(line)) n = sizeof(line)-1;
        memcpy(line, p, n);
        line[n] = 0;
        while (n && (line[n-1] == '\r' || line[n-1] == ' ')) line[--n] = 0;
        if (!*line) {

            /* A blank line is a line and nothing more. It cannot go
               round the breaking loop below: nothing fits in no width,
               so that loop takes its one character from past the end of
               the string -- and what is past the end is whatever the
               line before left in the buffer. Every blank line in every
               message was followed by a ghost of the line above it. */
            readput("");
            p = *e? e+1: e;

            continue;

        }
        /* a line too long for the window is broken at a space */
        do {

            char  part[MAXLINE];
            int   cut;

            copystr(part, line, sizeof(part));
            cut = fitchars(readwf, part, w);
            if (cut < (int)strlen(part)) {

                int b = cut;

                /* back up to the last space, so words stay whole */
                while (b && part[b-1] != ' ') b--;
                if (b > 1) cut = b;
                part[cut] = 0;

            }
            /* Always take at least one character. A window too narrow
               for a single character would otherwise take none, and a
               loop that takes nothing off the front of the line never
               reaches the end of it. */
            if (cut < 1) { cut = 1; part[1] = 0; }
            readput(part);
            memmove(line, line+cut, strlen(line+cut)+1);
            while (*line == ' ') memmove(line, line+1, strlen(line));

        } while (*line);
        p = *e? e+1: e;

    }

}

/* how many lines of the message the window holds */
static int readpage(void)

{

    ami_long page = (ami_maxyg(readwf)-8)/chrh;

    return (page < 1? 1: page);

}

/* keep the top of the message inside the message */
static void readclamp(void)

{

    int page = readpage();

    if (readtop > readlines-page) readtop = readlines-page;
    if (readtop < 0) readtop = 0;

}

/* put the bar where the text is */
static void readbar(void)

{

    int page = readpage();
    int max = readlines-page;

    if (fromdrag) return;
    if (max < 1) max = 1;
    ami_scrollsiz(readwf, SBREAD, fullscale(page, readlines));
    ami_scrollpos(readwf, SBREAD, fullscale(readtop, max));

}

/* draw the lines from a to b, having cleared the room they go in */
static void readrows(int a, int b)

{

    int i;

    if (a < readtop) a = readtop;
    if (b > readlines-1) b = readlines-1;
    if (a > b) return;
    ami_fcolor(readwf, ami_white);
    ami_frect(readwf, 0, 4+(a-readtop)*chrh, ami_maxxg(readwf),
              4+(b-readtop+1)*chrh);
    ami_fcolor(readwf, ami_black);
    for (i = a; i <= b; i++) if (*readline[i]) {

        ami_cursorg(readwf, 8, 4+(i-readtop)*chrh);
        fprintf(readwf, "%s", readline[i]);

    }

}

static void drawread(void)

{

    if (!readwf) return;
    fprintf(readwf, "\f");
    ami_fcolor(readwf, ami_black);
    readclamp();
    readrows(readtop, readtop+readpage()-1);
    readshown = readtop;
    readbar();

}

/* Move the text to where it is now supposed to be. A step of a line or
   two moves the pixels that are already on the screen and draws only
   the lines that come into view -- a message page is thirty odd lines
   of text and drawing one of them is a thirtieth of the work of drawing
   them all. Anything further than a screen is drawn afresh, since none
   of what is up would be kept anyway. */
static void showread(void)

{

    int page;
    int d;

    if (!readwf) return;
    readclamp();
    page = readpage();
    d = readtop-readshown;
    if (!d) { readbar(); return; } /* it did not move */
    if (d >= page || d <= -page) { drawread(); return; } /* nothing kept */
    /* Positive moves the picture up, which is what going down the
       message does, and leaves the strip at the bottom to be filled. */
    {

        struct timespec t0, t1;

        if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
        ami_scrollg(readwf, 0, d*chrh);
        readshown = readtop;
        if (d > 0) readrows(readtop+page-d, readtop+page-1); /* at the foot */
        else readrows(readtop, readtop-d-1);                 /* at the head */
        readbar();
        if (diag) {

            clock_gettime(CLOCK_MONOTONIC, &t1);
            fprintf(stderr, "scroll: %d line%s %.1fms\n", d < 0? -d: d,
                    d == 1 || d == -1? "": "s",
                    (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);

        }

    }

}

/* open a message to be read: the headers worth showing, then the text */
/* Quote what is being answered, the way mail has quoted since there was
   mail: the sender and the date, then the text with a > before every
   line of it. */
static char* quoted(const char* who, const char* when, const char* text,
                    int mark)

{

    int   n = strlen(text)*2+MAXSTR*2+64;
    char* d = getmem(n);
    int   o = 0;
    const char* p;

    if (mark) o += snprintf(d+o, n-o, "On %s, %s wrote:\n", when, who);
    for (p = text; *p; ) {

        const char* q = p;

        while (*q && *q != '\n') q++;
        if (o+(q-p)+4 < n) {

            if (mark) { d[o++] = '>'; if (q > p) d[o++] = ' '; }
            memcpy(d+o, p, q-p);
            o += q-p;
            d[o++] = '\n';

        }
        p = *q? q+1: q;

    }
    d[o] = 0;

    return (d);

}

/* Answer it, answer everybody on it, or pass it on. */
static void answer(int what)

{

    char  subj[MAXSTR*2];
    char  to[MAXSTR*2];
    char  cc[MAXSTR*2];
    char* body;
    char  who[MAXSTR];

    if (!readwf) return;
    nameof(redfrom, who, sizeof(who));
    if (!*who) copystr(who, redfrom, sizeof(who));
    *cc = 0;
    if (what == MENUFWD) {

        snprintf(subj, sizeof(subj), "%s%s",
                 strncasecmp(redsubj, "Fwd:", 4)? "Fwd: ": "", redsubj);
        *to = 0;
        body = quoted(who, reddate, redtext? redtext: "", FALSE);
        {   /* what is passed on says what it was */

            char* d = getmem(strlen(body)+MAXSTR*4);

            sprintf(d, "\n---------- Forwarded message ----------\n"
                    "From: %s\nDate: %s\nSubject: %s\nTo: %s\n\n%s",
                    redfrom, reddate, redsubj, redto, body);
            free(body);
            body = d;

        }

    } else {

        snprintf(subj, sizeof(subj), "%s%s",
                 strncasecmp(redsubj, "Re:", 3)? "Re: ": "", redsubj);
        /* the answer goes to whoever wrote it */
        addrof(redfrom, to, sizeof(to));
        if (!*to) copystr(to, redfrom, sizeof(to));
        if (what == MENUREPALL) {

            /* and to everybody it was sent to, less ourselves: answering
               everybody does not mean answering yourself */
            char  mine[MAXSTR];
            char  one[MAXSTR];
            const char* p;
            int   o = 0;

            copystr(mine, sendsrv >= 0 && sendsrv < srvct?
                    servers[sendsrv].user: "", sizeof(mine));
            for (p = redto; p || *one; ) {

                char just[MAXSTR];

                p = nextaddr(p, one, sizeof(one));
                if (*one) {

                    addrof(one, just, sizeof(just));
                    if (!*just) copystr(just, one, sizeof(just));
                    if (strcasecmp(just, mine) && strcasecmp(just, to) &&
                        o+strlen(just)+3 < sizeof(cc))
                        o += snprintf(cc+o, sizeof(cc)-o, "%s%s",
                                      o? ", ": "", just);

                }
                if (!p) break;

            }
            for (p = redcc; p || *one; ) {

                char just[MAXSTR];

                p = nextaddr(p, one, sizeof(one));
                if (*one) {

                    addrof(one, just, sizeof(just));
                    if (!*just) copystr(just, one, sizeof(just));
                    if (strcasecmp(just, mine) && strcasecmp(just, to) &&
                        o+strlen(just)+3 < sizeof(cc))
                        o += snprintf(cc+o, sizeof(cc)-o, "%s%s",
                                      o? ", ": "", just);

                }
                if (!p) break;

            }

        }
        body = quoted(who, reddate, redtext? redtext: "", TRUE);
        {   /* a line to write on, above what is being answered */

            char* d = getmem(strlen(body)+4);

            sprintf(d, "\n\n%s", body);
            free(body);
            body = d;

        }

    }
    /* A reply belongs to the conversation it answers and says so; a
       forward is a new message to somebody who was not in it, and
       putting it in the thread would file it under a conversation they
       have never seen. */
    if (what == MENUFWD) cmpopen(to, cc, subj, body, "", "");
    else cmpopen(to, cc, subj, body, redid, redrefs);
    free(body);
    /* the caret where the writing goes, which is the top */
    cmpcl = 0;
    cmpcc = 0;
    cmpdraw();

}

static void openmsgin(ami_long fold, const msgrec* m)

{

    char* raw;
    char* text;
    char  from[MAXSTR], to[MAXSTR], subj[MAXSTR], date[MAXSTR];
    char  title[MAXSTR];
    ami_long wx, wy;
    int   n;
    struct timespec t0, t1, t2, t3, t4;

    if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
    raw = getmsgin(fold, m);
    if (!raw) { fail("The message could not be read back"); return; }
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t1);
    findheader(raw, "From", from, sizeof(from));
    findheader(raw, "To", to, sizeof(to));
    findheader(raw, "Subject", subj, sizeof(subj));
    findheader(raw, "Date", date, sizeof(date));
    text = textof(raw, strlen(raw), 0); /* all of it, to be read */
    /* what a reply to this would be made of */
    copystr(redfrom, from, sizeof(redfrom));
    copystr(redto, to, sizeof(redto));
    copystr(redsubj, subj, sizeof(redsubj));
    copystr(reddate, date, sizeof(reddate));
    if (!findheader(raw, "Cc", redcc, sizeof(redcc))) *redcc = 0;
    if (!findheader(raw, "Message-ID", redid, sizeof(redid))) *redid = 0;
    if (!findheader(raw, "References", redrefs, sizeof(redrefs))) *redrefs = 0;
    free(redtext);
    redtext = getmem(strlen(text)+1);
    strcpy(redtext, text);
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t2);
    free(readtext);
    n = strlen(from)+strlen(to)+strlen(subj)+strlen(date)+strlen(text)+200;
    readtext = getmem(n);
    snprintf(readtext, n,
             "From:    %s\nTo:      %s\nDate:    %s\nSubject: %s\n"
             "\n"
             "%s", from, to, date, subj, text);
    free(text);
    free(raw);
    readtop = 0;
    if (!readwf) {

        ami_openwin(&stdin, &readwf, NULL, READWIN);
        {   /* what can be done with a message that is being read */

            ami_menuptr mp;
            ami_menuptr ml = NULL;

            newmenu(&mp, FALSE, FALSE, OFF, MENUREPLY, "Reply");
            appendmenu(&ml, mp);
            newmenu(&mp, FALSE, FALSE, OFF, MENUREPALL, "Reply All");
            appendmenu(&ml, mp);
            newmenu(&mp, FALSE, FALSE, OFF, MENUFWD, "Forward");
            appendmenu(&ml, mp);
            ami_menu(readwf, ml);

        }
        ami_auto(readwf, FALSE);
        ami_curvis(readwf, FALSE);
        ami_font(readwf, AMI_FONT_TERM);
        ami_setpoints(readwf, pointsz);
        ami_binvis(readwf);
        ami_winclientg(readwf, ami_strsiz(readwf, "0")*84, chrh*40, &wx, &wy,
                       BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
        ami_setsizg(readwf, wx, wy);
        /* Clear of the list it was opened from, which has to stay
           clickable: beside the main window, as far right as the screen
           allows. A fixed position will not do, since it is measured from
           the screen and the list is not -- the reader used to open at
           80,80 and land on top of the very list it came from, where it
           took the clicks meant for the next message, and every one after
           the first appeared to open the same mail. */
        {

            ami_long sx, sy; /* the screen */
            ami_long mx, my; /* and what the main window takes of it */
            int rx;

            ami_scnsizg(stdout, &sx, &sy);
            ami_getsizg(stdout, &mx, &my);
            rx = sx-wx-20;
            if (rx < mx+20) rx = mx+20; /* beside the list at the least */
            if (rx+wx > sx) rx = sx-wx; /* but still on the screen */
            if (rx < 0) rx = 0;
            ami_setposg(readwf, rx, 60);

        }
        ami_scrollvertsizg(readwf, &sbw, &wy);
        ami_scrollvertg(readwf, ami_maxxg(readwf)-sbw, 1, sbw,
                        ami_maxyg(readwf), SBREAD);

    }
    copystr(title, *subj? subj: "(no subject)", MAXSTR);
    ami_title(readwf, title);
    ami_front(readwf);
    wrapread();
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t3);
    drawread();
    if (diag) {

        clock_gettime(CLOCK_MONOTONIC, &t4);
        fprintf(stderr, "open: %d bytes, read %.0fms decode %.0fms "
                        "wrap %.0fms (%d lines) draw %.0fms\n", (int)m->len,
                (t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6,
                (t2.tv_sec-t1.tv_sec)*1e3+(t2.tv_nsec-t1.tv_nsec)/1e6,
                (t3.tv_sec-t2.tv_sec)*1e3+(t3.tv_nsec-t2.tv_nsec)/1e6,
                readlines,
                (t4.tv_sec-t3.tv_sec)*1e3+(t4.tv_nsec-t3.tv_nsec)/1e6);

    }

}

/* open the message at that place in the list being shown */
static void openmsg(int i)

{

    if (i >= 0 && i < msgct) openmsgin(foldsel, &msgs[i]);

}

static void closeread(void)

{

    int i;

    if (!readwf) return;
    /* The widget first. A window closed over a live widget leaves the
       widget's record, and the widget's own window, pointing at a file
       that is gone -- and the record for that file is taken again by
       the next window opened. Resizing the window that got it then
       resized the widget's window too, through the stale record. */
    ami_killwidget(readwf, SBREAD);
    fclose(readwf);
    readwf = NULL;
    for (i = 0; i < readlines; i++) free(readline[i]);
    readlines = 0;
    free(readtext);
    readtext = NULL;

}

/*******************************************************************************

The server form

Everything needed to reach an account, in one window: where the servers
are, who to log in as, and how much of a folder to take at a time. It is
a window of the program's own like the reader, so it is filled in with
the rest of the program still in view, and it is serviced from the same
event loop.

The password is shown as it is typed. There is no widget in the library
that hides what is entered, which is the one thing a form like this
would want.

*******************************************************************************/

/* the fields, in the order they appear */
static const struct {

    int   id;
    char* label;
    char* note;

} srvfld[] = {

    { SRVNAME,  "Name",           "what to call this account here" },
    { SRVIMAP,  "Mail server",    "where the mail is read from" },
    { SRVIPORT, "Port",           "993 for the secure one, which is usual" },
    { SRVSMTP,  "Sending server", "where mail would be sent from" },
    { SRVSPORT, "Port",           "465 for the secure one" },
    { SRVUSER,  "User",           "the whole address, as someone@gmail.com" },
    { SRVPASS,  "Password",       "for Gmail, an application password" },
    { SRVLIMIT, "Messages",       "how many of each folder to fetch" },
    { SRVPOLL,  "Look every",     "seconds between looks at the servers" },

};
#define SRVFLDS ((int)(sizeof(srvfld)/sizeof(srvfld[0])))

static void srvlay(void)

{

    ami_long chrw = ami_strsiz(srvwf, "0");
    ami_long labw = ami_strsiz(srvwf, "Sending server  ");
    ami_long ew, eh, bw, bh;
    int y;
    int i;

    ami_editboxsizg(srvwf, "0", &ew, &eh);
    ami_buttonsizg(srvwf, "Cancel", &bw, &bh);
    fprintf(srvwf, "\f");
    ami_fcolor(srvwf, ami_black);
    y = chrh;
    for (i = 0; i < SRVFLDS; i++) {

        ami_cursorg(srvwf, chrw*2, y+(eh-chrh)/2);
        fprintf(srvwf, "%s", srvfld[i].label);
        ami_poswidgetg(srvwf, srvfld[i].id, chrw*2+labw, y);
        ami_sizwidgetg(srvwf, srvfld[i].id, chrw*30, eh);
        /* what the field is for, beside it, since a form that only says
           "Port" leaves the reader to guess which port */
        ami_cursorg(srvwf, chrw*2+labw+chrw*32, y+(eh-chrh)/2);
        fprintf(srvwf, "%s", srvfld[i].note);
        y += eh+chrh/2;

    }
    y += chrh/2;
    /* One account sends, and only one: a message goes out over one
       connection with one name on it. Checking the box here is choosing
       among the accounts, so checking it for one clears it for the rest
       -- which is what the box does when it is clicked, not something
       the reader has to remember to do. */
    {

        ami_long cw, ch;

        ami_checkboxsizg(srvwf, "Send mail from this account", &cw, &ch);
        ami_poswidgetg(srvwf, SRVSEND, chrw*2+labw, y);
        ami_sizwidgetg(srvwf, SRVSEND, cw, ch);
        ami_selectwidget(srvwf, SRVSEND, srvedit == sendsrv);
        y += ch+chrh/2;

    }
    ami_poswidgetg(srvwf, SRVOK, chrw*2+labw, y);
    ami_sizwidgetg(srvwf, SRVOK, bw, bh);
    ami_poswidgetg(srvwf, SRVCAN, chrw*2+labw+bw+chrw*2, y);
    ami_sizwidgetg(srvwf, SRVCAN, bw, bh);
    ami_poswidgetg(srvwf, SRVNEXT, chrw*2+labw+(bw+chrw*2)*2, y);
    ami_sizwidgetg(srvwf, SRVNEXT, bw, bh);
    ami_poswidgetg(srvwf, SRVNEW, chrw*2+labw+(bw+chrw*2)*3, y);
    ami_sizwidgetg(srvwf, SRVNEW, bw, bh);
    ami_poswidgetg(srvwf, SRVDEL, chrw*2+labw+(bw+chrw*2)*4, y);
    ami_sizwidgetg(srvwf, SRVDEL, bw, bh);
    /* which of them is being shown, since the fields do not say */
    {

        char n[80];

        snprintf(n, sizeof(n), "account %d of %d", srvedit+1,
                 srvct > srvedit? srvct: srvedit+1);
        ami_fcolorc(srvwf, rgb(110), rgb(110), rgb(110));
        ami_cursorg(srvwf, chrw*2, y+bh+chrh/2);
        fprintf(srvwf, "%s", n);
        ami_fcolor(srvwf, ami_black);

    }

}

/* fill the form in from what the program is using now */
static void srvload(void)

{

    char num[40];

    if (srvedit >= srvct) { /* one that does not exist yet */

        if (srvct < MAXSRV) { blankserver(&servers[srvct]); srvedit = srvct; }
        else srvedit = 0;

    }
    ami_putwidgettext(srvwf, SRVNAME, servers[srvedit].name);
    sprintf(num, "%d", pollsec);
    ami_putwidgettext(srvwf, SRVPOLL, num);
    ami_putwidgettext(srvwf, SRVIMAP, servers[srvedit].imap);
    sprintf(num, "%d", servers[srvedit].imapport);
    ami_putwidgettext(srvwf, SRVIPORT, num);
    ami_putwidgettext(srvwf, SRVSMTP, servers[srvedit].smtp);
    sprintf(num, "%d", servers[srvedit].smtpport);
    ami_putwidgettext(srvwf, SRVSPORT, num);
    ami_putwidgettext(srvwf, SRVUSER, servers[srvedit].user);
    ami_putwidgettext(srvwf, SRVPASS, servers[srvedit].pass);
    sprintf(num, "%d", servers[srvedit].limit);
    ami_putwidgettext(srvwf, SRVLIMIT, num);

}

/* take what was typed and keep it */
static void srvsave(void)

{

    char    s[MAXSTR];
    srvrec* r;

    if (srvedit >= MAXSRV) return;
    if (srvedit >= srvct) srvct = srvedit+1; /* a new one is kept */
    r = &servers[srvedit];
    ami_getwidgettext(srvwf, SRVNAME, s, sizeof(s));
    trim(s);
    if (*s && strcmp(s, r->name)) { /* it has been renamed */

        char was[64];

        copystr(was, r->name, sizeof(was));
        copystr(r->name, s, sizeof(r->name));
        /* The mailboxes are named for their account, so they follow it.
           Without this a rename orphans everything the account has
           fetched and it all comes down again under the new name. */
        renamestore(was, r->name);

    }
    ami_getwidgettext(srvwf, SRVIMAP, s, sizeof(s));
    trim(s);
    copystr(r->imap, s, MAXSTR);
    ami_getwidgettext(srvwf, SRVIPORT, s, sizeof(s));
    if (atol(s) > 0) r->imapport = atol(s);
    ami_getwidgettext(srvwf, SRVSMTP, s, sizeof(s));
    trim(s);
    copystr(r->smtp, s, MAXSTR);
    ami_getwidgettext(srvwf, SRVSPORT, s, sizeof(s));
    if (atol(s) > 0) r->smtpport = atol(s);
    ami_getwidgettext(srvwf, SRVUSER, s, sizeof(s));
    trim(s);
    copystr(r->user, s, MAXSTR);
    /* the password is taken as typed, since a space may be part of it */
    ami_getwidgettext(srvwf, SRVPASS, s, sizeof(s));
    copystr(r->pass, s, MAXSTR);
    ami_getwidgettext(srvwf, SRVLIMIT, s, sizeof(s));
    if (atol(s) > 0) r->limit = atol(s);
    ami_getwidgettext(srvwf, SRVPOLL, s, sizeof(s));
    if (atol(s) >= 0) pollsec = atol(s);
    writeaccount();
    useserver(0);
    /* the periodic look follows whatever was just set */
    ami_killtimer(stdout, TIMPOLL);
    if (pollsec > 0) ami_timer(stdout, TIMPOLL, pollsec*10000, TRUE);

}

static void srvclose(void)

{

    int i;

    if (!srvwf) return;
    for (i = SRVIMAP; i <= SRVSEND; i++) ami_killwidget(srvwf, i); /* first: see closeread */
    fclose(srvwf);
    srvwf = NULL;

}

static void srvopen(void)

{

    ami_long wx, wy;
    ami_long ew, eh, bw, bh;
    int i;

    if (srvwf) { ami_front(srvwf); return; }
    ami_openwin(&stdin, &srvwf, NULL, SRVWIN);
    ami_title(srvwf, "Mail server");
    ami_buffer(srvwf, FALSE);
    ami_auto(srvwf, FALSE);
    ami_curvis(srvwf, FALSE);
    ami_font(srvwf, AMI_FONT_SIGN);
    ami_setpoints(srvwf, pointsz);
    ami_binvis(srvwf);
    ami_editboxsizg(srvwf, "0", &ew, &eh);
    ami_buttonsizg(srvwf, "Cancel", &bw, &bh);
    {

        /* Wide enough for whichever is wider: the fields with their
           notes beside them, or the row of buttons under them. The row
           was running off the edge, and the last button with it -- a
           form that will not show what it offers. */
        ami_long chrw = ami_strsiz(srvwf, "0");
        ami_long labw = ami_strsiz(srvwf, "Sending server  ");
        ami_long need = chrw*2+labw+(bw+chrw*2)*4+bw+chrw*2;
        int want = chrw*96;

        if (need > want) want = need;
        ami_winclientg(srvwf, want, (eh+chrh/2)*SRVFLDS+bh*2+chrh*8, &wx, &wy,
                       BIT(ami_wmframe) | BIT(ami_wmsize) |
                       BIT(ami_wmsysbar));

    }
    ami_setsizg(srvwf, wx, wy);
    ami_setposg(srvwf, 120, 120);
    /* made here, placed by the layout, which runs again on a resize */
    for (i = 0; i < SRVFLDS; i++)
        ami_editboxg(srvwf, 1, 1, 2, 2, srvfld[i].id);
    ami_buttong(srvwf, 1, 1, 2, 2, "Save", SRVOK);
    ami_buttong(srvwf, 1, 1, 2, 2, "Cancel", SRVCAN);
    ami_buttong(srvwf, 1, 1, 2, 2, "Next", SRVNEXT);
    ami_buttong(srvwf, 1, 1, 2, 2, "Add", SRVNEW);
    ami_buttong(srvwf, 1, 1, 2, 2, "Remove", SRVDEL);
    ami_checkboxg(srvwf, 1, 1, 2, 2, "Send mail from this account", SRVSEND);
    srvlay();
    srvload();

}

/* an event with the form's window id on it */
static void srvevent(ami_evtrec* er)

{

    switch (er->etype) {

        case ami_etterm: srvclose(); break;

        case ami_etredraw:
        case ami_etresize: srvlay(); break;

        case ami_etchkbox:
            if (er->ckbxid == SRVSEND) {

                /* Ticking it names this account; unticking it would
                   leave nothing able to send, so the box goes back on
                   and says so by staying on. */
                sendsrv = srvedit;
                ami_selectwidget(srvwf, SRVSEND, TRUE);
                writeaccount();

            }
            break;

        case ami_etbutton:
            if (er->butid == SRVNEXT) { /* show the one after this */

                srvsave();
                srvedit = srvct? (srvedit+1)%srvct: 0;
                srvlay();
                srvload();

            } else if (er->butid == SRVNEW) { /* another account */

                srvsave();
                if (srvct < MAXSRV) srvedit = srvct;
                srvlay();
                srvload();

            } else if (er->butid == SRVDEL) { /* take this one away */

                int i;

                if (srvedit < srvct) {

                    for (i = srvedit; i+1 < srvct; i++)
                        servers[i] = servers[i+1];
                    srvct--;

                }
                if (srvedit >= srvct) srvedit = srvct? srvct-1: 0;
                writeaccount();
                srvlay();
                srvload();

            } else if (er->butid == SRVOK) {

                srvsave();
                srvclose();
                status(haveaccount()?
                       "Account saved. Mail/Get Mail reads the server.":
                       "The server, the user and the password are all "
                       "needed before mail can be fetched.");

            } else srvclose();
            break;

        default: break;

    }

}

/*******************************************************************************

Layout

The panes are placed on the main window and moved when it is resized.
The folder pane takes a fixed width, since folder names are short and a
folder list that grows with the window is wasted space.

*******************************************************************************/

/* Lay the window out, whole or after a resize.

   Whole -- at the start, at a change of point size, and when the folder
   pane's width changed, which moves everything -- clears the panes and
   draws them all. After a resize it draws what the resize moved and no
   more: the buffers keep what was drawn, so the folder pane needs
   nothing, the message list needs the tail of each row from where the
   date column was to where it is, the banner needs the picture at its
   new place and the strip needs drawing at its new height. What a
   taller window exposed below arrives as a redraw of the exposed band.
   Drawn whole, every resize flashed the lot. */
static void layout(int whole)

{

    int top = 1+banh; /* under the banner, which is under the menu */
    int h = ami_maxyg(stdout)-top-stath; /* the strip has the foot of it */
    int w = ami_maxxg(stdout);
    int oldfoldw = foldw, olddatex = datex, oldfromx = fromx, oldcatx = catx;
    int oldlistw = listw, oldbanw = banw;
    static int laid;

    if (!laid) { whole = TRUE; laid = TRUE; }
    /* The banner is as wide as the window and stays where it is put. */
    ami_setposg(banwf, 1, 1);
    ami_setsizg(banwf, w, banh);
    ami_sizbufg(banwf, w, banh);

    if (diag) fprintf(stderr, "layout: buf %lldx%lld stath %d progh %d "
                      "panes %d tall\n", AMI_LONG_CAST(ami_maxxg(stdout)), AMI_LONG_CAST(ami_maxyg(stdout)),
                      stath, progh, h);

    foldw = ami_strsiz(stdout, "0")*22;
    if (foldw > w/2) foldw = w/2;
    if (foldw != oldfoldw) whole = TRUE;
    /* the main window shows between and around the panes, so it is
       cleared here rather than left as whatever was under it */
    if (whole) fprintf(stdout, "\f");
    ami_setposg(foldwf, 1, top);
    /* The buffer follows the window. Sizing the window alone leaves the
       buffer the size it was, and a buffered window answers with its
       buffer -- so everything drawn from its measurements would be laid
       out for the pane it used to be. */
    ami_setsizg(foldwf, foldw, h);
    ami_sizbufg(foldwf, foldw, h);
    /* the gap between the panes holds the divider between the sections */
    listx = foldw+8;
    listy = top;
    listw = w-foldw-8;
    ami_setposg(listwf, listx, listy);
    ami_setsizg(listwf, listw, h);
    ami_sizbufg(listwf, listw, h);
    if (!whole) {

        /* The gap between the panes is the main window's, and it keeps
           what was there: the strip's band, where the window was
           shorter. Painted out with a line, as the strip is: a filled
           rectangle does not paint in this window. */
        ami_fcolor(stdout, ami_white);
        ami_linewidth(stdout, 8);
        ami_line(stdout, foldw+4, top, foldw+4, top+h);
        ami_linewidth(stdout, 1);
        ami_fcolor(stdout, ami_black);

    }
    divider(stdout, foldw+4, top, foldw+4, top+h);
    /* The columns of the list, kept here so the rows and their dividers
       agree on where the columns are. They adapt to the width there is,
       as the character front end's do: at full width the sender gets
       eighteen characters' worth and the category a full "promotions",
       and below that the two give up room in proportion and the subject
       keeps what they yield -- fixed columns at a larger point size were
       eating the subject whole. The date is never squeezed: a date that
       does not fit is not a date. */
    datex = ami_maxxg(listwf)-sbw-ami_strsiz(listwf, "Sep 30, 2025 ");
    if (datex < 0) datex = 0; /* a pane too narrow for even the date */
    {

        ami_long unit = ami_strsiz(listwf, "0");
        ami_long catw = ami_strsiz(listwf, "promotions  ");

        fromx = unit*18;
        if (datex-8-(fromx+catw) < unit*12) { /* the subject is starving */

            /* In steps of two characters' width, so that a resize of a
               few pixels leaves the columns where they are: every row
               is drawn again when the columns move, and only the tail
               of each when the date column alone does. */
            ami_long step = unit*2;

            fromx = datex*2/5/step*step;
            if (fromx < unit*8) fromx = unit*8;
            catw = datex/6/step*step;
            if (catw > ami_strsiz(listwf, "promotions  "))
                catw = ami_strsiz(listwf, "promotions  ");
            if (catw < unit*4) catw = unit*4;

        }
        catx = fromx+catw;
        /* The minimums above are what a column would like, not what
           there is: at a large point size in a narrow window they ask
           for more than the whole width. The columns are kept in order
           and inside it, and the subject takes what is left, which may
           be nothing. Left alone, a column past the date column gave the
           drawing a negative width to cut a string to. */
        if (fromx > datex) fromx = datex;
        if (catx > datex) catx = datex;
        if (catx < fromx) catx = fromx;

    }
    /* The bar down the right of the message list is moved and sized, not
       made again: a widget id is taken until the widget is killed, so
       making it a second time is an error, and a resize would raise it. */
    ami_poswidgetg(listwf, SBLIST, ami_maxxg(listwf)-sbw, 1);
    ami_sizwidgetg(listwf, SBLIST, sbw, ami_maxyg(listwf));
    listrows = ami_maxyg(listwf)/rowh;
    if (listrows < 1) listrows = 1;
    if (whole) {

        fprintf(foldwf, "\f");
        fprintf(listwf, "\f");
        drawfolders();
        drawlist();
        drawstatus();
        drawbanner(0);
        banw = w;

        return;

    }
    /* a resize: what it moved, and only that */
    drawstatus(); /* at its new height, whole */
    if (havepic && w != oldbanw) {

        /* the picture at its new place, from the leftmost of the two */
        int oc = oldbanw-picdw-16;
        int nc = w-picdw-16;

        drawbanner(oc < nc? oc: nc);
        /* a wider banner is told of its right strip too; that is drawn */
        if (w > oldbanw) banpart = TRUE;

    }
    banw = w;
    if (fromx != oldfromx || catx != oldcatx) drawlist(); /* the columns moved */
    else if (datex != olddatex) listtails(olddatex); /* the date column alone */
    if (listw > oldlistw) listpart = TRUE;

}

/* Larger and smaller, on control-+ and control--

   The window is left exactly as it is -- that is the user's to set -- and
   what changes is the size of everything in it. Nearly all of this
   program is laid out from the point size: the rows of the list, the
   width of the folder pane, the columns, the wrap of a message. So the
   size moves a point, the measurements are taken again, and the whole
   thing is laid out afresh at the new size.

   A window that is open follows along. A window not yet made takes the
   size when it is made, since it asks for pointsz like everything else. */
static void setzoom(float d)

{

    float was = pointsz;

    pointsz += d;
    if (pointsz < MINPOINT) pointsz = MINPOINT;
    if (pointsz > MAXPOINT) pointsz = MAXPOINT;
    if (pointsz == was) return; /* it is as far over as it goes */
    ami_setpoints(stdout, pointsz);
    ami_setpoints(foldwf, pointsz);
    ami_setpoints(listwf, pointsz);
    ami_setpoints(banwf, pointsz*2);
    /* what everything else is measured from */
    chrh = ami_chrsizy(stdout);
    rowh = chrh+8;
    banmeasure(); /* the banner and its picture follow the size too */
    layout(TRUE);
    /* a message being read is wrapped again at the size it is now */
    if (readwf) {

        ami_setpoints(readwf, pointsz);
        wrapread();
        drawread();

    }

}

/*******************************************************************************

Help topics

A window of the program's own, not a dialog. A dialog stops the program
and runs a loop of its own until it is answered; help is not a question,
so it opens beside the mail and stays up while the mail is worked on.

That takes no machinery. There is one event queue for the program, and
every event names the window it came from, so the main loop tells the
help window's events from the mail's by their window id and hands them
here. Nothing is nested, and no thread is needed -- though one could be
put in charge of this window just as well, since its state is all here.

Widgets are numbered within their window, so the search entry, the topic
list and the close button are 1, 2 and 3 here even though the mail's
scroll bars are 1 and 2 there.

*******************************************************************************/

/* A topic is a title and the text under it, both pointing into the
   block the help file was read into. */
typedef struct { char* title; char* text; } helprec;

/* The wrapped text, one entry per line as it appears on the screen. The
   text is wrapped once, when the topic is picked or the window resized,
   and drawn from there, which is what makes it scrollable. */
typedef struct { char* s; int bold; int ind; } helpline;

static FILE*     helpwf;      /* the help window, NULL when closed */
static char*     helpbuf;     /* the help file, read whole */
static helprec*  helptopics;  /* the topics in it */
static int       helptopicct;
static int*     helpmatch;   /* the topics the search matched */
static int       helpmatches; /* how many of them */
static int       helpsel;     /* the topic shown, -1 for none */
static int       helpx0, helpy0; /* the topic list, in pixels */
static int       helpx1, helpy1;
static int       helplistup;  /* the list box has been made */
static helpline* helplines;   /* the topic, wrapped to the pane */
static int       helplinect;
static int       helplinemax;
static int       helptop;     /* first wrapped line shown */
static int       helppage;    /* wrapped lines the pane holds */

static void helpout(const char* s, int bold, int ind); /* forward */

/*******************************************************************************

Reading the help file

The file is markdown, of the plain kind: a line beginning with a single
# names a topic and everything after it belongs to that topic until the
next one or the end of the file. Within a topic a blank line separates
paragraphs, ## is a heading inside the topic and - is a list item.

Keeping the text in a file rather than in the program means the help can
be rewritten, corrected or translated without a compiler, and it can be
as long as it deserves to be. The file is read whole and the titles and
texts point into it, so a topic costs two pointers.

*******************************************************************************/

/* is this the start of a topic: one #, then a space, then a title? */
static int helphead(const char* p)

{

    return (*p == '#' && p[1] != '#');

}

/* Find and read the help file. It is looked for beside the program
   first, since that is where an installed program's own files belong,
   then in the source directory it is kept in, then where the program
   was run from. The first one found is the one used. */
static int helpread(void)

{

    char  path[600];
    char  dir[500];
    char* e;
    FILE* f = NULL;
    int   i, n;

    /* the directory the program was run from, with its slash */
    dir[0] = 0;
    if (mailprog) {

        snprintf(dir, sizeof(dir), "%s", mailprog);
        e = strrchr(dir, '/');
        if (e) e[1] = 0; else dir[0] = 0;

    }
    for (i = 0; i < 4 && !f; i++) {

        switch (i) {

            /* beside the program, then the source beside that, which is
               where it sits in a build tree, then the two the same way
               from wherever the program was run */
            case 0: snprintf(path, sizeof(path), "%s%s", dir, HELPFILE);
                    break;
            case 1: snprintf(path, sizeof(path), "%s../graph_programs/%s",
                             dir, HELPFILE); break;
            case 2: snprintf(path, sizeof(path), "%s", HELPFILE); break;
            default: snprintf(path, sizeof(path), "graph_programs/%s",
                              HELPFILE); break;

        }
        f = fopen(path, "r");
        if (diag) fprintf(stderr, "help: %s: %s\n", path, f? "found": "no");

    }
    if (!f) return (FALSE);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    helpbuf = malloc(n+1);
    if (!helpbuf) { ami_alert("Mail", "Out of memory"); exit(1); }
    n = fread(helpbuf, 1, n, f);
    helpbuf[n] = 0;
    fclose(f);

    return (TRUE);

}

/* Break the file into topics. The heads are found first and the block
   cut afterwards, in that order: cutting as it went would overwrite the
   newline that the test for the next head stands on, and every other
   topic would be passed over. */
static void helpsplit(void)

{

    char*  p;
    char** head; /* the # of each topic */
    int    n, i;

    /* count the heads, then take them, walking by lines both times */
    n = 0;
    for (p = helpbuf; *p; ) {

        if (helphead(p)) n++;
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }
    head = malloc((n+1)*sizeof(char*));
    helptopics = malloc((n+1)*sizeof(helprec));
    helpmatch = malloc((n+1)*sizeof(int));
    if (!head || !helptopics || !helpmatch)
        { ami_alert("Mail", "Out of memory"); exit(1); }
    n = 0;
    for (p = helpbuf; *p; ) {

        if (helphead(p)) head[n++] = p;
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }
    helptopicct = n;
    /* the title is the rest of the head line, the text is what follows */
    for (i = 0; i < n; i++) {

        char* t = head[i]+1;
        char* e;

        while (*t == ' ') t++;
        helptopics[i].title = t;
        e = t;
        while (*e && *e != '\n') e++;
        if (*e) *e++ = 0; /* end the title */
        while (*e == '\n') e++; /* and the blank line under it */
        helptopics[i].text = e;

    }
    /* Now the cutting: each text ends where the next topic begins. The
       last ends at the end of the file, which is already a zero. */
    for (i = 0; i+1 < n; i++) *head[i+1] = 0;

}

/* load the help, once, and say so if there is none */
static void helpload(void)

{

    static char nofile[400];

    if (helptopics) return; /* already loaded */
    if (helpread()) helpsplit();
    if (!helptopicct) { /* no file, or a file with no topics in it */

        snprintf(nofile, sizeof(nofile),
                 "The help file %s was not found, or holds no topics.\n"
                 "\n"
                 "It is looked for beside the program and in the "
                 "graph_programs directory of the source. Help is kept in "
                 "a file so that it can be changed without rebuilding the "
                 "program; if the file is missing, only the help is.",
                 HELPFILE);
        helptopics = malloc(sizeof(helprec));
        helpmatch = malloc(sizeof(int));
        if (!helptopics || !helpmatch)
            { ami_alert("Mail", "Out of memory"); exit(1); }
        helptopics[0].title = "No help file";
        helptopics[0].text = nofile;
        helptopicct = 1;

    }

}

/*******************************************************************************

The topic list

*******************************************************************************/

/* How many times does the topic hold the text, in either case? A count
   rather than a yes or no, because the list shows it: a topic the word
   is the subject of holds it many times, and one that merely mentions
   it in passing holds it once, and the reader can tell them apart
   without opening either. */
static int helpcount(const helprec* h, const char* what)

{

    const char* p;
    int         n = strlen(what);
    int         c = 0;

    if (!n) return (0); /* an empty search matches everything, uncounted */
    for (p = h->title; *p; p++)
        if (!strncasecmp(p, what, n)) c++;
    for (p = h->text; *p; p++)
        if (!strncasecmp(p, what, n)) c++;

    return (c);

}

/* Build the list of topics matching the search and put it in the list
   box. The list box is made again rather than changed, since a list box
   is given its contents when it is made; it copies them, so the list
   built here is ours to free. */
static void helpfill(const char* what)

{

    ami_strptr sl = NULL, sp, lp = NULL;
    int        i, c;
    char       lab[300];

    /* the strings are ours until the list box has them; it copies */
    helpmatches = 0;
    for (i = 0; i < helptopicct; i++) {

        c = helpcount(&helptopics[i], what);
        if (*what && !c) continue; /* not this one */
        /* the count goes beside the title, so that a topic the word is
           the subject of can be told from one that mentions it once */
        if (*what) snprintf(lab, sizeof(lab), "%s (%d)",
                            helptopics[i].title, c);
        else snprintf(lab, sizeof(lab), "%s", helptopics[i].title);
        sp = malloc(sizeof(ami_strrec));
        if (!sp) { ami_alert("Mail", "Out of memory"); exit(1); }
        sp->str = strdup(lab);
        if (!sp->str) { ami_alert("Mail", "Out of memory"); exit(1); }
        sp->next = NULL;
        if (lp) lp->next = sp; else sl = sp;
        lp = sp;
        helpmatch[helpmatches++] = i;

    }
    /* a search that matches nothing still needs a list, or there would
       be no box to type the next search against */
    if (!sl) {

        sl = malloc(sizeof(ami_strrec));
        if (!sl) { ami_alert("Mail", "Out of memory"); exit(1); }
        sl->str = strdup("(no topic matches)");
        if (!sl->str) { ami_alert("Mail", "Out of memory"); exit(1); }
        sl->next = NULL;

    }
    if (helplistup) ami_killwidget(helpwf, HELPLIST);
    ami_listboxg(helpwf, helpx0, helpy0, helpx1, helpy1, sl, HELPLIST);
    helplistup = TRUE;
    while (sl) { sp = sl->next; free(sl->str); free(sl); sl = sp; }
    /* the topic shown is only still shown if the search kept it */
    if (helpsel >= 0) {

        for (i = 0; i < helpmatches; i++) if (helpmatch[i] == helpsel) break;
        if (i >= helpmatches) helpsel = -1;

    }

}

/*******************************************************************************

Laying the topic out

The text is wrapped to the pane once and kept as lines, so that drawing
it, scrolling it and knowing how much of it there is are all the same
small piece of work. It is wrapped again when the window is resized,
which is the only thing that can change the answer.

*******************************************************************************/

/* keep one finished line */
static void helpout(const char* s, int bold, int ind)

{

    if (helplinect >= helplinemax) {

        helplinemax = helplinemax? helplinemax*2: 100;
        helplines = realloc(helplines, helplinemax*sizeof(helpline));
        if (!helplines) { ami_alert("Mail", "Out of memory"); exit(1); }

    }
    helplines[helplinect].s = strdup(s);
    if (!helplines[helplinect].s)
        { ami_alert("Mail", "Out of memory"); exit(1); }
    helplines[helplinect].bold = bold;
    helplines[helplinect].ind = ind;
    helplinect++;

}

/* Wrap one paragraph, which arrives as a single string with its line
   breaks already turned into spaces. The break goes at the last word
   that still fits, and fitting is measured with the font rather than
   counted in characters, since the font is not fixed pitch. */
static void helpwrap(const char* s, int bold, int ind, int w)

{

    char line[500];
    char try[500];
    int n;

    ami_bold(helpwf, bold);
    while (*s) {

        const char* q = s;

        n = 0;
        line[0] = 0;
        while (*q) { /* as many whole words as fit */

            const char* e = q;
            int         m;

            while (*e && *e != ' ') e++; /* the next word */
            m = e-s;
            if (m >= (int)sizeof(try)) break;
            memcpy(try, s, m);
            try[m] = 0;
            if (n && ami_strsiz(helpwf, try) > w-ind) break;
            strcpy(line, try);
            n = m;
            q = e;
            while (*q == ' ') q++;
            if (!*e) break;

        }
        if (!n) { /* one word wider than the pane: put it down anyway */

            while (*q && *q != ' ') q++;
            n = q-s;
            if (n >= (int)sizeof(line)) n = sizeof(line)-1;
            memcpy(line, s, n);
            line[n] = 0;

        }
        helpout(line, bold, ind);
        s += n;
        while (*s == ' ') s++;

    }
    ami_bold(helpwf, FALSE);

}

/* Wrap the topic being shown to the pane. The markdown is read here: a
   blank line ends a paragraph, ## is a heading within the topic, and -
   is a list item, which is wrapped with its later lines lined up under
   the first word rather than under the dash. */
static void helplay1(int w)

{

    const char* p;
    char        para[4000];
    int         pl = 0;
    int         bold = FALSE;
    int         ind = 0;
    int         i;

    for (i = 0; i < helplinect; i++) free(helplines[i].s);
    helplinect = 0;
    helptop = 0;
    if (helpsel < 0) { helpout("Pick a topic.", FALSE, 0); return; }
    /* the title of the topic, in bold, and a line under it */
    helpout(helptopics[helpsel].title, TRUE, 0);
    helpout("", FALSE, 0);
    p = helptopics[helpsel].text;
    while (1) {

        const char* e = p;
        int         n;

        while (*e && *e != '\n') e++;
        n = e-p;
        while (n && (p[n-1] == ' ' || p[n-1] == '\r')) n--; /* trailing */
        if (!n || *p == '#' || *p == '-' || *p == '*' || !*p) {

            /* the line before is finished, whatever this one is */
            if (pl) { para[pl] = 0; helpwrap(para, bold, ind, w); pl = 0; }
            bold = FALSE;
            ind = 0;

        }
        if (!*p) break;
        if (!n) helpout("", FALSE, 0); /* a blank line stays blank */
        else if (*p == '#') { /* a heading inside the topic */

            const char* t = p;

            while (*t == '#') t++;
            while (*t == ' ') t++;
            bold = TRUE;
            n -= t-p;
            if (n > (int)sizeof(para)-1) n = sizeof(para)-1;
            memcpy(para, t, n);
            pl = n;

        } else if (*p == '-' || *p == '*') { /* a list item */

            ind = ami_strsiz(helpwf, "00");
            if (n > (int)sizeof(para)-1) n = sizeof(para)-1;
            memcpy(para, p, n);
            pl = n;

        } else { /* ordinary text, joined to the line before it */

            if (pl && pl < (int)sizeof(para)-1) para[pl++] = ' ';
            if (pl+n > (int)sizeof(para)-1) n = sizeof(para)-1-pl;
            memcpy(para+pl, p, n);
            pl += n;

        }
        p = *e? e+1: e;

    }

}

/* draw the topic from the line it is scrolled to */
static void helpdraw(void)

{

    ami_long chrh = ami_chrsizy(helpwf);
    ami_long x = helpx1+ami_strsiz(helpwf, "00");
    int y = helpy0;
    int i;

    /* down to and including the line the count is written on, which is
       under the pane: leave it out and each count is written over the
       one before it */
    ami_fcolor(helpwf, ami_white);
    ami_frect(helpwf, x, helpy0, ami_maxxg(helpwf), helpy1+chrh);
    ami_fcolor(helpwf, ami_black);
    helppage = (helpy1-helpy0)/chrh;
    if (helppage < 1) helppage = 1;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    for (i = helptop; i < helplinect && y+chrh <= helpy1; i++) {

        if (*helplines[i].s) {

            ami_bold(helpwf, helplines[i].bold);
            ami_cursorg(helpwf, x+helplines[i].ind, y);
            fprintf(helpwf, "%s", helplines[i].s);
            ami_bold(helpwf, FALSE);

        }
        y += chrh;

    }
    /* say there is more, since a pane with no bar gives no other sign */
    if (helplinect > helppage) {

        char more[80];

        if (helptop+helppage >= helplinect) strcpy(more, "-- end --");
        else sprintf(more, "-- %d more line%s, wheel or page keys --",
                     helplinect-helptop-helppage,
                     helplinect-helptop-helppage == 1? "": "s");
        ami_fcolor(helpwf, ami_blue);
        ami_cursorg(helpwf, x, helpy1);
        fprintf(helpwf, "%s", more);
        ami_fcolor(helpwf, ami_black);

    }

}

/* wrap and draw, which is what everything that changes the topic wants */
static void helptext(void)

{

    helplay1(ami_maxxg(helpwf)-ami_strsiz(helpwf, "00")-
             (helpx1+ami_strsiz(helpwf, "00")));
    helpdraw();

}

/* scroll the topic by so many lines */
static void helpscroll(int by)

{

    int was = helptop;

    helptop += by;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    if (helptop != was) helpdraw();

}

/* place the widgets and work out the panes, on opening and on resize */
static void helplay(void)

{

    ami_long chrh = ami_chrsizy(helpwf);
    ami_long chrw = ami_strsiz(helpwf, "0");
    int lw   = chrw*30;  /* the topic list */
    ami_long bw, bh, ew, eh;

    ami_buttonsizg(helpwf, "Close", &bw, &bh);
    ami_editboxsizg(helpwf, "0", &ew, &eh);
    /* the search entry along the top of the list column */
    helpx0 = chrw*2;
    helpy0 = chrh*2+eh;
    helpx1 = helpx0+lw;
    helpy1 = ami_maxyg(helpwf)-bh-chrh*2;
    if (helpy1 < helpy0+chrh) helpy1 = helpy0+chrh;
    ami_poswidgetg(helpwf, HELPFIND, helpx0+ami_strsiz(helpwf, "Search: "),
                   chrh);
    ami_sizwidgetg(helpwf, HELPFIND,
                   lw-ami_strsiz(helpwf, "Search: "), eh);
    ami_poswidgetg(helpwf, HELPCLOSE, ami_maxxg(helpwf)-bw-chrw*2,
                   ami_maxyg(helpwf)-bh-chrh/2);
    ami_sizwidgetg(helpwf, HELPCLOSE, bw, bh);
    /* The list is moved and sized rather than made again, so that the
       topic picked in it stays picked across a resize. Only a change
       of contents needs it made again, which is what helpfill is for. */
    if (helplistup) {

        ami_poswidgetg(helpwf, HELPLIST, helpx0, helpy0);
        ami_sizwidgetg(helpwf, HELPLIST, helpx1-helpx0, helpy1-helpy0);

    } else helpfill("");
    /* the frame, the label, and the topic */
    fprintf(helpwf, "\f");
    ami_fcolor(helpwf, ami_black);
    ami_cursorg(helpwf, helpx0, chrh);
    fprintf(helpwf, "Search:");
    helptext();

}

/* close the help window, if it is open */
static void helpclose(void)

{

    int i;

    if (!helpwf) return;
    ami_killwidget(helpwf, HELPFIND); /* the widgets first: see closeread */
    if (helplistup) ami_killwidget(helpwf, HELPLIST);
    ami_killwidget(helpwf, HELPCLOSE);
    fclose(helpwf);
    helpwf = NULL;
    helplistup = FALSE;
    for (i = 0; i < helplinect; i++) free(helplines[i].s);
    helplinect = 0;

}

/* Open the help window. A second open just brings the one already up to
   the front, as help does everywhere. */
static void helpopen(void)

{

    ami_long wx, wy;

    if (helpwf) { ami_front(helpwf); return; }
    helpload();
    helpsel = -1;
    helptop = 0;
    ami_openwin(&stdin, &helpwf, NULL, HELPWIN);
    ami_title(helpwf, "Spreadsheet help");
    /* Unbuffered, so the window's measurements are the window's. A
       buffered window answers maxxg with the buffer, which is not what
       the layout wants here: this window has nothing to keep. */
    ami_buffer(helpwf, FALSE);
    ami_auto(helpwf, FALSE);
    ami_curvis(helpwf, FALSE);
    ami_font(helpwf, AMI_FONT_SIGN);
    ami_setpoints(helpwf, pointsz);
    ami_binvis(helpwf);
    ami_winclientg(helpwf, ami_strsiz(helpwf, "0")*86, ami_chrsizy(helpwf)*26,
                   &wx, &wy, BIT(ami_wmframe) | BIT(ami_wmsize) |
                             BIT(ami_wmsysbar));
    ami_setsizg(helpwf, wx, wy);
    /* The entry and the button are made once here and moved by the
       layout after. The list is made by the layout, which is where its
       rectangle is known and where it is made again on every search. */
    ami_editboxg(helpwf, 1, 1, 2, 2, HELPFIND);
    ami_buttong(helpwf, 1, 1, 2, 2, "Close", HELPCLOSE);
    helplay();

}

/* An event with the help window's id on it. The main loop hands them
   here and goes on; nothing about the mail is touched. */
static void helpevent(ami_evtrec* er)

{

    char srch[100];

    switch (er->etype) {

        case ami_etterm:   /* the window was closed, not the program */
        case ami_etbutton: helpclose(); break;

        case ami_etresize:
        case ami_etredraw: helplay(); break;

        case ami_etmouba: /* the wheel, as buttons 4 and 5 */
            if (er->amoubn == 4) helpscroll(-WHEELROWS);
            else if (er->amoubn == 5) helpscroll(WHEELROWS);
            break;

        case ami_etpagu: helpscroll(-(helppage-1)); break;
        case ami_etpagd: helpscroll(helppage-1); break;
        case ami_etup:   helpscroll(-1); break;
        case ami_etdown: helpscroll(1); break;
        case ami_ethome: helpscroll(-helplinect); break;
        case ami_etend:  helpscroll(helplinect); break;

        case ami_etlstbox: /* a topic was picked */
            if (er->lstbsl >= 1 && er->lstbsl <= helpmatches)
                helpsel = helpmatch[er->lstbsl-1];
            helptext();
            break;

        case ami_etedtbox: /* the search was entered */
            ami_getwidgettext(helpwf, HELPFIND, srch, sizeof(srch));
            helpfill(srch);
            helptext();
            break;

        default: break;

    }

}

/*******************************************************************************
/*******************************************************************************

The search form

Modelled on the search form of Gmail: who it is from and to, what the
subject and the message hold and do not hold, how big it is, when it was
sent, and which folder to look in. Search hands the ask to the worker,
which reads the folders and tries every message (see the core), and what
it finds is listed under the form, newest first, each line the date, the
sender and the subject. A line clicked opens the message in the reader,
from whatever folder it lies in. The form is a fixed size: its fields are
made where they stand and only the labels are drawn again.

*******************************************************************************/

static FILE*    srcwf;        /* the search window, NULL when closed */
static int      srclistup;    /* there are results to show */
static ami_long srcx0, srcy0, srcx1, srcy1; /* the list's rectangle */
static ami_long srcsbw;       /* the width of the bar beside it */
static int      srcrowh;      /* the height of a row of it */
static int      srctop;       /* the first row on show */
static int      srcsel;       /* the row picked, or -1 */
static ami_long srcmx, srcmy; /* where the mouse is */
static ami_long srcclickms;   /* when the list was last clicked */

static void srcdraw(void);
static void foldname(ami_long f, char* s, ami_long sl);
static ami_long srcsty;       /* where the status line is written */
static ami_long srcsizesel = 1;   /* the dropboxes' picks, 1 based */
static ami_long srcunitsel = 1;
static ami_long srcwithinsel = 1;
static ami_long srcfoldsel = 1;
static int      srcatt;       /* the attachment box is checked */
static int      srcwild = TRUE; /* and the wildcard box, which starts so */
static char     srcsaid[MAXSTR]; /* what the status line says */

static const char* srcsizeops[] = { "greater than", "less than" };
static const char* srcunits[] = { "MB", "KB", "bytes" };
static const ami_long srcunitval[] = { 1024*1024, 1024, 1 };
static const char* srcwithins[] = { "1 day", "3 days", "1 week", "2 weeks",
                                    "1 month", "2 months", "6 months",
                                    "1 year" };
static const ami_long srcwithinsec[] = { 86400L, 3*86400L, 7*86400L,
                                         14*86400L, 30*86400L, 61*86400L,
                                         183*86400L, 365*86400L };

/* a string list for a widget, from an array of strings; the widget
   copies it, so it is freed after */
static ami_strptr strlist(const char* const* items, int n)

{

    ami_strptr sl = NULL, lp = NULL, sp;
    int i;

    for (i = 0; i < n; i++) {

        sp = malloc(sizeof(ami_strrec));
        if (!sp) { ami_alert("Mail", "Out of memory"); exit(1); }
        sp->str = strdup(items[i]);
        if (!sp->str) { ami_alert("Mail", "Out of memory"); exit(1); }
        sp->next = NULL;
        if (lp) lp->next = sp; else sl = sp;
        lp = sp;

    }

    return (sl);

}

static void freelist(ami_strptr sl)

{

    ami_strptr sp;

    while (sl) { sp = sl->next; free(sl->str); free(sl); sl = sp; }

}

/* the status line under the form: what the search is doing or found */
static void srcstatus(const char* s)

{

    if (!srcwf) return;
    copystr(srcsaid, s, sizeof(srcsaid));
    ami_fcolor(srcwf, ami_white);
    ami_frect(srcwf, 1, srcsty, ami_maxxg(srcwf), srcsty+chrh-1);
    ami_fcolor(srcwf, ami_black);
    ami_cursorg(srcwf, ami_strsiz(srcwf, "0")*2, srcsty);
    fprintf(srcwf, "%s", srcsaid);

}

/* the labels, drawn again on every redraw; the widgets keep themselves */
static void srclay(void)

{

    static const char* labels[] = { "From", "To", "Subject", "Has the words",
                                    "Doesn't have", "Size", "Date within",
                                    "Search" };
    ami_long chrw = ami_strsiz(srcwf, "0");
    ami_long ew, eh, cw, ch, ow, oh, rowh, y;
    ami_strptr sl;
    int i;

    ami_editboxsizg(srcwf, "0", &ew, &eh);
    sl = strlist(srcsizeops, 2);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    freelist(sl);
    rowh = (eh > ch? eh: ch)+chrh/2;
    fprintf(srcwf, "\f");
    ami_fcolor(srcwf, ami_black);
    y = chrh;
    for (i = 0; i < 8; i++) {

        ami_cursorg(srcwf, chrw*2, y+(rowh-chrh/2-chrh)/2);
        fprintf(srcwf, "%s", labels[i]);
        y += rowh;

    }
    srcstatus(srcsaid);
    srcdraw();

}

/* how many rows of what was found fit */
static int srcvis(void)

{

    int n = (srcy1-srcy0-4)/srcrowh;

    return (n < 1? 1: n);

}

/* keep the top of the list inside the list */
static void srcclamp(void)

{

    if (srctop > srcct-srcvis()) srctop = srcct-srcvis();
    if (srctop < 0) srctop = 0;

}

static void srcbar(void)

{

    int max = srcct-srcvis();

    if (max < 1) max = 1;
    if (srcct > srcvis()) ami_scrollsiz(srcwf, SRCSB, fullscale(srcvis(), srcct));
    else ami_scrollsiz(srcwf, SRCSB, INT_MAX);
    ami_scrollpos(srcwf, SRCSB, fullscale(srctop, max));

}

/* What was found, drawn as the message list draws its rows -- when it
   came, who from, what about -- in columns, since the font is not fixed
   pitch and a list box cannot line them up. Newest first, as the message
   list is. */
static void srcdraw(void)

{

    ami_long w = srcx1-srcsbw-srcx0;
    ami_long datew = ami_strsiz(srcwf, "Sep 30, 2026")+16;
    ami_long fromw = ami_strsiz(srcwf, "0")*22;
    ami_long foldw = ami_strsiz(srcwf, "0")*18; /* which folder it is in */
    ami_long x, y;
    char     s[MAXSTR];
    int      i;

    if (!srcwf) return;
    ami_fcolor(srcwf, ami_white);
    ami_frect(srcwf, srcx0, srcy0, srcx1-srcsbw, srcy1);
    ami_fcolor(srcwf, ami_black);
    ami_rect(srcwf, srcx0, srcy0, srcx1-srcsbw, srcy1);
    if (!srclistup) return;
    if (!srcct) {

        ami_cursorg(srcwf, srcx0+8, srcy0+4);
        fprintf(srcwf, "Nothing found");
        return;

    }
    srcclamp();
    y = srcy0+4;
    for (i = srctop; i < srcct && y+srcrowh <= srcy1-2; i++) {

        const msgrec* m = &srcres[i];

        if (i == srcsel) {

            ami_fcolor(srcwf, ami_cyan);
            ami_frect(srcwf, srcx0+1, y-2, srcx1-srcsbw-1, y+srcrowh-4);
            ami_fcolor(srcwf, ami_black);

        }
        x = srcx0+8;
        if (m->date) whenof(m->date, s, sizeof(s));
        else copystr(s, m->when, sizeof(s));
        ami_cursorg(srcwf, x, y);
        fprintf(srcwf, "%s", s);
        x += datew;
        ami_bold(srcwf, TRUE);
        copystr(s, m->from, sizeof(s));
        clipstr(srcwf, s, fromw-8);
        ami_cursorg(srcwf, x, y);
        fprintf(srcwf, "%s", s);
        ami_bold(srcwf, FALSE);
        x += fromw;
        /* the folder it was found in, with its account's name, since two
           accounts each have an INBOX; in the quieter grey of the list's
           kind column, there to be glanced past */
        foldname(srcfold[i], s, sizeof(s));
        clipstr(srcwf, s, foldw-8);
        ami_fcolorc(srcwf, rgb(110), rgb(110), rgb(110));
        ami_cursorg(srcwf, x, y);
        fprintf(srcwf, "%s", s);
        ami_fcolor(srcwf, ami_black);
        x += foldw;
        copystr(s, m->subject, sizeof(s));
        clipstr(srcwf, s, srcx0+w-x-8);
        ami_cursorg(srcwf, x, y);
        fprintf(srcwf, "%s", s);
        y += srcrowh;

    }
    srcbar();

}

/* the search is done: its results on show, and how many */
static void srcfill(void)

{

    char t[MAXSTR];

    srclistup = TRUE;
    srctop = 0;
    srcsel = -1;
    srcdraw();
    if (!srcct) copystr(t, "Nothing found", sizeof(t));
    else if (srcct == 1) copystr(t, "1 message found", sizeof(t));
    else snprintf(t, sizeof(t), "%lld messages found", AMI_LONG_CAST(srcct));
    if (*srcmissed) { /* the folders not yet read, which it could not look in */

        char m[MAXSTR*3];

        snprintf(m, sizeof(m), "%s (not yet read: %s)", t, srcmissed);
        srcstatus(m);

    } else srcstatus(t);

}

/* the list moved: drawn again from its new top */
static void srcscroll(int top)

{

    srctop = top;
    srcclamp();
    srcdraw();

}

/* the name of a folder as the list shows it: the account's, then its own */
static void foldname(ami_long f, char* s, ami_long sl)

{

    if (f < 0 || f >= foldct) { *s = 0; return; }
    if (folders[f].srv >= 0 && folders[f].srv < srvct)
        snprintf(s, sl, "%s %s", servers[folders[f].srv].name, folders[f].show);
    else copystr(s, folders[f].show, sl);

}

/* a row picked: shown as picked, and brought into view */
static void srcpickrow(int i)

{

    if (i < 0 || i >= srcct) return;
    srcsel = i;
    if (i < srctop) srctop = i;
    if (i >= srctop+srcvis()) srctop = i-srcvis()+1;
    srcdraw();

}

/* and read: on a double click, or return */
static void srcopenrow(int i)

{

    if (i < 0 || i >= srcct) return;
    openmsgin(srcfold[i], &srcres[i]);

}

/* Search: the form into the ask, and the worker set going. What it
   finds is picked up by the tick and listed. */
static void srcgo(void)

{

    char s[MAXSTR];
    double v;

    ami_getwidgettext(srcwf, SRCFROM, srcask.from, sizeof(srcask.from));
    trim(srcask.from);
    ami_getwidgettext(srcwf, SRCTO, srcask.to, sizeof(srcask.to));
    trim(srcask.to);
    ami_getwidgettext(srcwf, SRCSUBJ, srcask.subject, sizeof(srcask.subject));
    trim(srcask.subject);
    ami_getwidgettext(srcwf, SRCWORDS, srcask.words, sizeof(srcask.words));
    trim(srcask.words);
    ami_getwidgettext(srcwf, SRCNOT, srcask.nowords, sizeof(srcask.nowords));
    trim(srcask.nowords);
    /* the size, when a number is given */
    ami_getwidgettext(srcwf, SRCSIZE, s, sizeof(s));
    trim(s);
    srcask.sizeop = 0;
    srcask.sizeval = 0;
    if (*s && sscanf(s, "%lf", &v) == 1) {

        srcask.sizeop = srcsizesel;
        srcask.sizeval = (ami_long)(v*srcunitval[srcunitsel-1]);

    }
    /* the date, when a day is given */
    ami_getwidgettext(srcwf, SRCDATE, s, sizeof(s));
    trim(s);
    srcask.within = 0;
    srcask.date = 0;
    if (*s) {

        srcask.date = parseday(s);
        if (!srcask.date) {

            srcstatus("The date is not understood: write it as 2026-09-09");
            return;

        }
        srcask.within = srcwithinsec[srcwithinsel-1];

    }
    srcask.fold = srcfoldsel-2; /* the first entry is all of them */
    srcask.attach = srcatt;
    srcask.wild = srcwild;
    if (srcbusy) srcstatus("Searching again...");
    else srcstatus("Searching...");
    srcwant = TRUE;
    kickworker();

}

/* the form emptied, ready for another search */
static void srcclear(void)

{

    ami_putwidgettext(srcwf, SRCFROM, "");
    ami_putwidgettext(srcwf, SRCTO, "");
    ami_putwidgettext(srcwf, SRCSUBJ, "");
    ami_putwidgettext(srcwf, SRCWORDS, "");
    ami_putwidgettext(srcwf, SRCNOT, "");
    ami_putwidgettext(srcwf, SRCSIZE, "");
    ami_putwidgettext(srcwf, SRCDATE, "");
    srcatt = FALSE;
    ami_selectwidget(srcwf, SRCATT, FALSE);
    srclistup = FALSE;
    srcdraw();
    srcstatus("");

}

/* close the search window, if it is open */
static void srcclose(void)

{

    int i;

    if (!srcwf) return;
    for (i = SRCFROM; i <= SRCWILD; i++) ami_killwidget(srcwf, i); /* first: see closeread */
    fclose(srcwf);
    srcwf = NULL;
    srclistup = FALSE;

}

/* Place the form for the window's size: the fields stretch with the
   width, the buttons and the unit box keep to the right edge, and the
   list of what was found takes the rest of the height. At the open,
   and again when the window is resized. */
static void srcplace(void)

{

    static const int edits[] = { SRCFROM, SRCTO, SRCSUBJ, SRCWORDS, SRCNOT };
    ami_long chrw = ami_strsiz(srcwf, "0");
    ami_long labw = ami_strsiz(srcwf, "Has the words  ");
    ami_long w = ami_maxxg(srcwf);
    ami_long ew, eh, cw, ch, ow, oh, bw, bh, rowh, y, x, fw;
    ami_strptr sl;
    int i;

    ami_editboxsizg(srcwf, "0", &ew, &eh);
    ami_buttonsizg(srcwf, "Search", &bw, &bh);
    sl = strlist(srcsizeops, 2);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    freelist(sl);
    rowh = (eh > ch? eh: ch)+chrh/2;
    x = chrw*2+labw;
    fw = w-x-chrw*2; /* the fields' width: what the window gives */
    if (fw < chrw*24) fw = chrw*24;
    y = chrh;
    for (i = 0; i < 5; i++) { /* the text fields */

        ami_poswidgetg(srcwf, edits[i], x, y);
        ami_sizwidgetg(srcwf, edits[i], fw, eh);
        y += rowh;

    }
    /* the size: greater or less than, how much, of what */
    ami_poswidgetg(srcwf, SRCSIZEOP, x, y);
    ami_poswidgetg(srcwf, SRCSIZE, x+ow+chrw*2, y);
    sl = strlist(srcunits, 3);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    freelist(sl);
    ami_poswidgetg(srcwf, SRCUNIT, x+fw-ow, y);
    y += rowh;
    /* the date: within this of that day */
    sl = strlist(srcwithins, 8);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    freelist(sl);
    ami_poswidgetg(srcwf, SRCWITHIN, x, y);
    ami_poswidgetg(srcwf, SRCDATE, x+ow+chrw*2, y);
    y += rowh;
    ami_poswidgetg(srcwf, SRCFOLD, x, y);
    y += rowh;
    ami_poswidgetg(srcwf, SRCATT, x, y);
    ami_poswidgetg(srcwf, SRCWILD, x+chrw*26, y);
    y += rowh;
    /* the buttons, against the right */
    ami_poswidgetg(srcwf, SRCGO, x+fw-bw, y);
    ami_poswidgetg(srcwf, SRCCLR, x+fw-bw*2-chrw*2, y);
    ami_poswidgetg(srcwf, SRCCLOSE, x+fw-bw*3-chrw*4, y);
    y += bh+chrh;
    srcsty = y;
    y += chrh+chrh/2;
    /* what is found goes below, to the bottom */
    srcx0 = chrw*2;
    srcy0 = y;
    srcx1 = w-chrw*2;
    srcy1 = ami_maxyg(srcwf)-chrh;
    if (srcy1 < srcy0+srcrowh) srcy1 = srcy0+srcrowh;
    ami_poswidgetg(srcwf, SRCSB, srcx1-srcsbw, srcy0);
    ami_sizwidgetg(srcwf, SRCSB, srcsbw, srcy1-srcy0);

}

/* Open the search window. A second open brings the one already up to
   the front. The widgets are made here and placed by srcplace, which
   places them again when the window is resized. */
static void srcopen(void)

{

    static const char* labels[] = { "From", "To", "Subject", "Has the words",
                                    "Doesn't have", "Size", "Date within",
                                    "Search" };
    static const int   edits[] = { SRCFROM, SRCTO, SRCSUBJ, SRCWORDS, SRCNOT };
    ami_long wx, wy;
    ami_long chrw, labw, ew, eh, cw, ch, ow, oh, bw, bh, rowh, y, x, fw;
    ami_strptr sl;
    const char* folds[MAXFOLDER+1];
    int i;

    if (srcwf) { ami_front(srcwf); return; }
    ami_openwin(&stdin, &srcwf, NULL, SRCWIN);
    ami_title(srcwf, "Search mail");
    ami_buffer(srcwf, FALSE);
    ami_auto(srcwf, FALSE);
    ami_curvis(srcwf, FALSE);
    ami_font(srcwf, AMI_FONT_SIGN);
    ami_setpoints(srcwf, pointsz);
    ami_binvis(srcwf);
    chrw = ami_strsiz(srcwf, "0");
    labw = ami_strsiz(srcwf, "Has the words  ");
    ami_editboxsizg(srcwf, "0", &ew, &eh);
    ami_buttonsizg(srcwf, "Search", &bw, &bh);
    sl = strlist(srcsizeops, 2);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    freelist(sl);
    rowh = (eh > ch? eh: ch)+chrh/2;
    fw = chrw*60; /* the fields' width */
    ami_winclientg(srcwf, chrw*2+labw+fw+chrw*2, rowh*9+bh+chrh*2+chrh*16,
                   &wx, &wy, BIT(ami_wmframe) | BIT(ami_wmsysbar) |
                             BIT(ami_wmsize));
    ami_setsizg(srcwf, wx, wy);
    ami_setposg(srcwf, 160, 100);
    x = chrw*2+labw;
    y = chrh;
    for (i = 0; i < 5; i++) { /* the text fields */

        ami_editboxg(srcwf, x, y, x+fw, y+eh, edits[i]);
        y += rowh;

    }
    /* the size: greater or less than, how much, of what */
    sl = strlist(srcsizeops, 2);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    ami_dropboxg(srcwf, x, y, x+ow, y+oh, sl, SRCSIZEOP);
    freelist(sl);
    ami_editboxg(srcwf, x+ow+chrw*2, y, x+ow+chrw*12, y+eh, SRCSIZE);
    sl = strlist(srcunits, 3);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    ami_dropboxg(srcwf, x+fw-ow, y, x+fw, y+oh, sl, SRCUNIT);
    freelist(sl);
    y += rowh;
    /* the date: within this of that day */
    sl = strlist(srcwithins, 8);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    ami_dropboxg(srcwf, x, y, x+ow, y+oh, sl, SRCWITHIN);
    freelist(sl);
    ami_editboxg(srcwf, x+ow+chrw*2, y, x+ow+chrw*16, y+eh, SRCDATE);
    y += rowh;
    /* the folder: all of them, or one */
    folds[0] = "All Mail";
    for (i = 0; i < foldct && i < MAXFOLDER; i++) folds[i+1] = folders[i].show;
    sl = strlist(folds, foldct+1);
    ami_dropboxsizg(srcwf, sl, &cw, &ch, &ow, &oh);
    ami_dropboxg(srcwf, x, y, x+ow, y+oh, sl, SRCFOLD);
    freelist(sl);
    y += rowh;
    ami_checkboxg(srcwf, x, y, x+chrw*24, y+eh, "Has attachment", SRCATT);
    ami_checkboxg(srcwf, x+chrw*26, y, x+chrw*52, y+eh, "Allow wildcards",
                  SRCWILD);
    ami_selectwidget(srcwf, SRCWILD, srcwild);
    y += rowh;
    /* the buttons, and the status line beside them */
    ami_buttong(srcwf, x+fw-bw, y, x+fw, y+bh, "Search", SRCGO);
    ami_buttong(srcwf, x+fw-bw*2-chrw*2, y, x+fw-bw-chrw*2, y+bh, "Clear",
                SRCCLR);
    ami_buttong(srcwf, x+fw-bw*3-chrw*4, y, x+fw-bw*2-chrw*4, y+bh, "Close",
                SRCCLOSE);
    y += bh+chrh;
    srcsty = y;
    y += chrh+chrh/2;
    /* what is found goes below, to the bottom */
    srcx0 = chrw*2;
    srcy0 = y;
    srcx1 = ami_maxxg(srcwf)-chrw*2;
    srcy1 = ami_maxyg(srcwf)-chrh;
    srcrowh = chrh+8;
    ami_scrollvertsizg(srcwf, &srcsbw, &eh);
    ami_scrollvertg(srcwf, srcx1-srcsbw, srcy0, srcx1, srcy1, SRCSB);
    srcplace();
    srctop = 0;
    srcsel = -1;
    srcsizesel = 1;
    srcunitsel = 1;
    srcwithinsel = 1;
    srcfoldsel = 1;
    srcatt = FALSE;
    *srcsaid = 0;
    srclay();
    (void)labels;

}

/* an event with the search window's id on it */
static void srcevent(ami_evtrec* er)

{

    switch (er->etype) {

        case ami_etterm: srcclose(); break; /* the window closed, not the program */
        case ami_etcan: srcclose(); break;  /* Escape: the same */
        case ami_etresize: srcplace(); srclay(); break;
        case ami_etredraw: srclay(); break;
        case ami_etbutton:
            if (er->butid == SRCGO) srcgo();
            else if (er->butid == SRCCLR) srcclear();
            else srcclose();
            break;
        case ami_etedtbox: srcgo(); break; /* return in a field searches */
        case ami_etdrpbox: /* a pick, remembered for the search */
            if (er->drpbid == SRCSIZEOP) srcsizesel = er->drpbsl;
            else if (er->drpbid == SRCUNIT) srcunitsel = er->drpbsl;
            else if (er->drpbid == SRCWITHIN) srcwithinsel = er->drpbsl;
            else if (er->drpbid == SRCFOLD) srcfoldsel = er->drpbsl;
            break;
        case ami_etchkbox:
            if (er->ckbxid == SRCWILD) {

                srcwild = !srcwild;
                ami_selectwidget(srcwf, SRCWILD, srcwild);

            } else {

                srcatt = !srcatt;
                ami_selectwidget(srcwf, SRCATT, srcatt);

            }
            break;
        /* the list of what was found: the bar beside it, the keys, and a
           click on a row */
        case ami_etsclull: srcscroll(srctop-1); break;
        case ami_etscldrl: srcscroll(srctop+1); break;
        case ami_etsclulp: srcscroll(srctop-(srcvis()-1)); break;
        case ami_etscldrp: srcscroll(srctop+(srcvis()-1)); break;
        case ami_etsclpos: srcscroll(scaleback(er->sclpos, srcct-srcvis())); break;
        case ami_etup: if (srcsel > 0) srcpickrow(srcsel-1); break;
        case ami_etdown: if (srcsel+1 < srcct) srcpickrow(srcsel+1); break;
        case ami_etpagu: srcscroll(srctop-(srcvis()-1)); break;
        case ami_etpagd: srcscroll(srctop+(srcvis()-1)); break;
        case ami_etmoumovg: srcmx = er->moupxg; srcmy = er->moupyg; break;
        case ami_etenter: srcopenrow(srcsel); break; /* return reads the pick */
        case ami_etmouba: /* the wheel, as buttons 4 and 5: a row a notch */
            if (er->amoubn == 4) srcscroll(srctop-1);
            else if (er->amoubn == 5) srcscroll(srctop+1);
            else if (er->amoubn == 1 && srclistup && srcmx >= srcx0 &&
                     srcmx < srcx1-srcsbw && srcmy >= srcy0+4 && srcmy < srcy1) {

                /* a click picks the row; a second on the same row within
                   a double click's time reads it */
                int      i = srctop+(srcmy-srcy0-4)/srcrowh;
                ami_long t = msnow();

                if (i == srcsel && t-srcclickms < DBLMS)
                    { srcopenrow(i); srcclickms = 0; }
                else { srcpickrow(i); srcclickms = t; }

            }
            break;
        default: break;

    }

}

/* what the worker found, on the tick that picks up its work */
static void srcpick(void)

{

    if (srcdone) {

        srcdone = FALSE;
        if (srcwf) srcfill();

    } else if (srcbusy && srcwf && *srcwhat) {

        char t[MAXSTR*2];
        char a[40], b[40];

        if (srcmax > 0) {

            commas(srcpos, a, sizeof(a));
            commas(srcmax, b, sizeof(b));
            snprintf(t, sizeof(t), "%s - %s of %s", srcwhat, a, b);

        } else copystr(t, srcwhat, sizeof(t));
        srcstatus(t);

    }

}

/*******************************************************************************

The options

A box of check boxes, opened from Config. What each box says is kept in
the account file with the accounts, so it is there next time.

*******************************************************************************/

static FILE* optwf; /* the window, NULL when not open */

static void optlay(void)

{

    fprintf(optwf, "\f"); /* the boxes say it all; the title says what they are */

}

static void optclose(void)

{

    if (!optwf) return;
    ami_killwidget(optwf, OPTTHREAD); /* the widgets first: see closeread */
    ami_killwidget(optwf, OPTCLOSE);
    fclose(optwf);
    optwf = NULL;

}

static void optopen(void)

{

    ami_long wx, wy, chrw, cw, ch, bw, bh, x, y;

    if (optwf) { ami_front(optwf); return; }
    ami_openwin(&stdin, &optwf, NULL, OPTWIN);
    ami_title(optwf, "Options");
    ami_buffer(optwf, FALSE);
    ami_auto(optwf, FALSE);
    ami_curvis(optwf, FALSE);
    ami_font(optwf, AMI_FONT_SIGN);
    ami_setpoints(optwf, pointsz);
    ami_binvis(optwf);
    chrw = ami_strsiz(optwf, "0");
    ami_checkboxsizg(optwf, "Threaded mode", &cw, &ch);
    ami_buttonsizg(optwf, "Close", &bw, &bh);
    ami_winclientg(optwf, chrw*36, chrh*2+ch+chrh+bh+chrh, &wx, &wy,
                   BIT(ami_wmframe) | BIT(ami_wmsysbar));
    ami_setsizg(optwf, wx, wy);
    ami_setposg(optwf, 200, 140);
    x = chrw*2;
    y = chrh*2;
    ami_checkboxg(optwf, x, y, x+cw, y+ch, "Threaded mode", OPTTHREAD);
    ami_selectwidget(optwf, OPTTHREAD, threaded);
    y += ch+chrh;
    ami_buttong(optwf, ami_maxxg(optwf)-chrw*2-bw, y, ami_maxxg(optwf)-chrw*2,
                y+bh, "Close", OPTCLOSE);
    optlay();

}

/* an event with the options box's id on it */
static void optevent(ami_evtrec* er)

{

    switch (er->etype) {

        case ami_etterm: optclose(); break; /* the box closed, not the program */
        case ami_etcan: optclose(); break;  /* Escape: the same */
        case ami_etredraw:
        case ami_etresize: optlay(); break;
        case ami_etbutton: optclose(); break;
        case ami_etchkbox:
            if (er->ckbxid == OPTTHREAD) {

                threaded = !threaded;
                ami_selectwidget(optwf, OPTTHREAD, threaded);
                writeaccount(); /* kept with the accounts */
                /* the list in its new order, from the top */
                useidx();
                msgtop = 0;
                msgsel = -1;
                listshown = 0;
                drawlist();

            }
            break;
        default: break;

    }

}

/*******************************************************************************

The menu

*******************************************************************************/

static void newmenu(ami_menuptr* mp, int onoff, int bar, int select,
                    int id, char* face)

{

    *mp = malloc(sizeof(ami_menurec));
    if (!*mp) { fail("Out of memory"); exit(1); }
    (*mp)->next = NULL;
    (*mp)->branch = NULL;
    (*mp)->onoff = onoff;
    (*mp)->oneof = FALSE;
    (*mp)->bar = bar;
    (*mp)->id = id;
    (*mp)->face = malloc(strlen(face)+1);
    if (!(*mp)->face) { fail("Out of memory"); exit(1); }
    strcpy((*mp)->face, face);

}

static void appendmenu(ami_menuptr* list, ami_menuptr m)

{

    ami_menuptr lp;

    if (!*list) *list = m;
    else { lp = *list; while (lp->next) lp = lp->next; lp->next = m; }

}

static void setupmenu(void)

{

    ami_menuptr ml = NULL;
    ami_menuptr ma;
    ami_menuptr mp;
    ami_menuptr sm = NULL;

    /* No File menu. All it held was Exit, which the window's own close
       box does, in the corner every window has it in. Get Mail is what
       this program is for, so it stands on the bar rather than inside
       something. */
    newmenu(&mp, FALSE, FALSE, OFF, MENUCOMP, "Compose");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, MENUFETCH, "Get Mail");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, MENUSEARCH, "Search");
    appendmenu(&ml, mp);
    newmenu(&ma, FALSE, FALSE, OFF, MENUMAIL, "Config");
    appendmenu(&ml, ma);
    ami_stdmenu(BIT(AMI_SMHELPTOPIC) | BIT(AMI_SMABOUT), &sm, ml);
    /* as in the spreadsheet, the branch is hung on after the standard
       menu is built, since building it clears the branch link */
    newmenu(&mp, FALSE, FALSE, OFF, MENUSRV, "Servers...");
    appendmenu(&ma->branch, mp);
    newmenu(&mp, FALSE, FALSE, OFF, MENUOPT, "Options...");
    appendmenu(&ma->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, MENUFOLD, "Refresh Folder List");
    appendmenu(&ma->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, MENUCHECK, "Check Sending");
    appendmenu(&ma->branch, mp);
    ami_menu(stdout, sm);

}

static void kickworker(void)

{

    if (!wrkstart) { ami_newthread(mailwork); wrkstart = TRUE; }
    if (!srcstart) { ami_newthread(searchwork); srcstart = TRUE; }
    /* Ten times a second, to pick up what the worker has done and draw
       it. Faster than the eye and slower than the flicker that drawing
       per message would be. */
    ami_timer(stdout, TIMFETCH, 1000, TRUE);
    timerrun = TRUE;

}

/* start one */
static void fetchall(int relist)

{

    if (fetching) return; /* one at a time */
    if (!haveaccount()) return;
    /* A look on the timer does not ask the servers what folders they
       have. Folders do not come and go by the quarter minute, the asking
       costs a connection and a round trip to each of them, and
       rebuilding the list redraws the pane -- which is what made it
       flicker every time the timer came round. Get Mail asks; the timer
       only fetches. */
    if (!foldct) relist = TRUE; /* unless nothing is known yet */
    if (!relist) { /* an account nothing is known of has to be asked */

        int k;

        for (k = 0; k < srvct && !relist; k++) {

            int m;

            if (!*servers[k].imap || !*servers[k].user) continue;
            if (serverquiet(k)) continue; /* not while it is not answering */
            for (m = 0; m < foldct; m++) if (folders[m].srv == k) break;
            /* A newly added account has no folders here, and a look
               that only walks the folders it knows would never touch it
               -- so it would never get any, and never be looked at
               again. One that has none is asked. */
            if (m >= foldct) relist = TRUE;

        }

    }
    fetching = TRUE;
    fetchasked = !quietfail; /* the timer sets that flag; a menu does not */
    failwait = FALSE;
    wrkrelist = relist;
    wrkcount = FALSE;
    wrkdone = FALSE;
    wrkgo = TRUE; /* and it is off */
    kickworker();

}

/* Count the store without fetching anything. The counts are what the
   folder pane shows, and finding them means reading every mailbox
   through -- gigabytes, at startup, in front of somebody waiting for a
   window. It goes to the worker like everything else slow. */
static void countlater(void)

{

    if (fetching) return;
    fetching = TRUE;
    fetchasked = FALSE; /* nobody asked to be told about counting */
    wrkrelist = FALSE;
    wrkcount = TRUE;
    wrkdone = FALSE;
    wrkgo = TRUE;
    kickworker();

}

static void showfolder(int i);

/* What the worker has done, drawn. This is the main thread's whole part
   in a fetch: it looks ten times a second, draws what has changed, and
   tidies up when the worker says it has finished. */
static void fetchpick(void)

{

    int folds = wrkfolds;
    int counts = wrkcounts;
    int list  = wrklist;
    int done  = wrkdone && !wrkgo;

    srcpick(); /* a search finished, or how far one has got */

    wrkfolds = FALSE;
    wrkcounts = FALSE;
    wrklist = FALSE;
    if (folds) drawfolders();
    else if (counts) drawcounts();
    if (*wrkwhat) { /* what it is doing, and how far into it */

        char t[MAXSTR*2];
        char a[40], b[40];

        if (wrkmax > 0) {

            commas(wrkpos, a, sizeof(a));
            commas(wrkmax, b, sizeof(b));
            snprintf(t, sizeof(t), "%s - %s of %s", wrkwhat, a, b);

        } else copystr(t, wrkwhat, sizeof(t));
        if (movwant && *movnote) { /* a move waits its turn behind this */

            strncat(t, " -- then moving ", sizeof(t)-strlen(t)-1);
            strncat(t, movnote, sizeof(t)-strlen(t)-1);

        }
        status(t);
        statprog(wrkpos, wrkmax);

    }
    if (*sentsaid) {

        /* said now, and again when the worker is done: the reading of
           a folder that follows a move writes its progress over it */
        status(sentsaid);
        copystr(keepsaid, sentsaid, sizeof(keepsaid));
        *sentsaid = 0;

    }
    if (failwait) {

        failwait = FALSE;
        /* A look on the timer is nobody waiting, and a server that is
           down fails every one of them: an hour away from it would be
           an hour of boxes to dismiss. Those are left to the diagnostic
           and to the account being set aside. */
        /* A failure to send is always somebody waiting: they pressed
           Send and are owed an answer. */
        if (fetchasked || !fetching || sendfail) ami_alert("Mail", failsaid);
        sendfail = FALSE;

    }
    if (done) {

        char t[MAXSTR];

        fetching = FALSE;
        *wrkwhat = 0;
        statprog(0, 0); /* the bar empties: there is nothing running */
        if (wrkcount) status("");
        else {

            char a[40], b[40];

            commas(fetchgot, a, sizeof(a));
            commas(fetchdup, b, sizeof(b));
            snprintf(t, sizeof(t), "%s new, %s already here", a, b);
            status(t);

        }

    }
    if (list) { drawlist(); drawfolders(); } /* the worker read a folder */
    if (done) {

        /* Reading a folder means reading its whole mailbox, so it is
           asked for only when the mailbox has actually changed under the
           reader: a fetch that put nothing in the folder being read
           leaves what is on the screen right, and finding that out by
           reading four gigabytes is four gigabytes wasted. */
        /* Nothing is read again here. What arrived went into the
           folder's index as it was stored, so the list is already right;
           the worker has put it in order and the drawing above has shown
           it. */
        if (foldsel < 0 && foldct) showfolder(0);
        drawfolders();

    }
    /* The timer runs while there is anything to watch, and stops when
       there is not: a tick ten times a second forever is a program that
       never lets the machine alone. Everything the worker might be in
       the middle of has to be in this test, and something waiting to be
       said counts as well as something being done. Sending was not:
       the tick after Send found no fetch running and stopped the timer
       before the worker had even picked the message up, so what came
       back -- that it had gone, or why it had not -- was never looked
       at, and a message that never left looked exactly like one that
       did. */
    if (timerrun && !fetching && !wrkgo && !wrkbusy && idxwant < 0 &&
        !sendwant && !movwant && !movbusy && !srcwant && !srcbusy && !srcdone &&
        !failwait && !*sentsaid && !*wrkwhat) {

        ami_killtimer(stdout, TIMFETCH);
        timerrun = FALSE;
        if (*keepsaid) { status(keepsaid); *keepsaid = 0; } /* the last word */

    }

}

/* show a folder */
static void showfolder(int i)

{

    if (i < 0 || i >= foldct) return;
    foldsel = i;
    popclose();
    closeread();
    msgtop = 0;
    msgsel = -1;
    /* If the folder has been read, showing it is showing what is already
       here -- the list is its index, not a copy of it, and nothing is
       read at all. Only a folder that has never been read costs
       anything, and that is asked for rather than done: a mailbox of
       four gigabytes takes as long as four gigabytes takes, and doing it
       here is the window going dead on a click. */
    useidx();
    if (!folders[i].idxok) {

        msgct = 0;
        if (idxdoing != i) idxwant = i; /* it is already being read */
        kickworker();

    }
    drawfolders();
    drawlist();

}

/*******************************************************************************

Main

*******************************************************************************/

/* The command line. The account is not here: a password on a command
   line is a password in everybody's process list. */
static ami_long dofetch;   /* fetch on startup */

static ami_optrec opttbl[] = {

    { "fetch", &dofetch, NULL,   NULL, NULL },
    { "f",     &dofetch, NULL,   NULL, NULL },
    { "diag",  &diag,    NULL,   NULL, NULL },
    { "d",     &diag,    NULL,   NULL, NULL },
    { "limit", NULL,     &limit, NULL, NULL },
    { NULL,    NULL,     NULL,   NULL, NULL }

};

int main(int argc, char* argv[])

{

    ami_evtrec er;
    char       msg[MAXSTR];
    int        i;
    ami_long   argi = 1;
    ami_long   argcl = argc;
    ami_long   wx, wy;

    /* the command line, by the option package, which knows what an
       option looks like on the system it is running on */
    mailprog = argv[0]; /* the rules are looked for beside the program */
    ami_options(&argi, &argcl, argv, opttbl, TRUE);
    if (argcl > 1) {

        fprintf(stderr, "Usage: mail [--fetch|-f] [--diag|-d] "
                        "[--limit=<n>]\n");

        return (1);

    }
    ami_title(stdout, "Mail");
    ami_autohold(FALSE);
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_setpoints(stdout, pointsz);
    ami_binvis(stdout);
    chrh = ami_chrsizy(stdout);
    rowh = chrh+8;
    { /* the store, if it is not there yet: makpth is an error if it is */

        char        path[MAXSTR-16];
        struct stat sb;

        ami_getusr(path, sizeof(path));
        snprintf(store, MAXSTR, "%s/.amimail", path);
        if (stat(store, &sb)) ami_makpth(store);

    }
    readaccount(); /* if there is one; the program comes up either way */
    migratestore(); /* mailboxes from before accounts had names */
    {

        ami_long cw = ami_strsiz(stdout, "0")*130; /* the room wanted inside */
        ami_long ch = chrh*46;

        ami_winclientg(stdout, cw, ch, &wx, &wy,
                       BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
        ami_setsizg(stdout, wx, wy);
        /* The buffer is NOT sized here. It follows the window, and the
           only thing that knows what the window actually became is the
           resize the window manager sends back -- which may be nothing
           like what was asked for. Sizing it here to what was asked for
           puts the buffer ahead of the window instead of behind it, and
           then every measurement is of a window that does not exist:
           the status strip is drawn at the foot of a buffer taller than
           the window, where nobody can see it. The resize that arrives
           at startup does the sizing, as every later one does. */

    }
    /* The menu is built once the window is its final size. Built before,
       the menu strip follows the resize but its newly exposed right end
       is never painted, and sits there as a black box until something
       makes the window resize again. */
    setupmenu();
    /* the two panes */
    ami_openwin(&stdin, &foldwf, stdout, FOLDWIN);
    /* A pane is part of the main window, not a window of its own: no
       frame, no title bar, no sizing bars. */
    ami_frame(foldwf, FALSE);

    ami_auto(foldwf, FALSE);
    ami_curvis(foldwf, FALSE);
    ami_font(foldwf, AMI_FONT_SIGN);
    ami_setpoints(foldwf, pointsz);
    ami_binvis(foldwf);
    ami_openwin(&stdin, &listwf, stdout, LISTWIN);
    ami_frame(listwf, FALSE);
    ami_auto(listwf, FALSE);
    ami_curvis(listwf, FALSE);
    ami_font(listwf, AMI_FONT_SIGN);
    ami_setpoints(listwf, pointsz);
    ami_binvis(listwf);
    /* The strip at the foot, and the bar in it. Its natural height is
       what the strip is built around; its width is set here, since a bar
       as wide as a window says less than a short one does. */
    /* the banner across the top: its own window, so that it is placed
       and drawn in one space and answers for its own redrawing */
    ami_openwin(&stdin, &banwf, stdout, BANWIN);
    ami_frame(banwf, FALSE);
    ami_auto(banwf, FALSE);
    ami_curvis(banwf, FALSE);
    ami_font(banwf, AMI_FONT_SIGN);
    ami_setpoints(banwf, pointsz*2);
    {

        char path[MAXSTR];

        if (resfile("mail.bmp", path, sizeof(path))) {

            ami_loadpict(banwf, BANPIC, path);
            picw = ami_pictsizx(banwf, BANPIC);
            pich = ami_pictsizy(banwf, BANPIC);
            havepic = picw > 0 && pich > 0;

        }

    }
    /* as tall as the picture wants, or as the name does */
    banmeasure();
    /* the strip along the foot, and the bar drawn in it */
    progh = chrh-2;
    progw = ami_strsiz(stdout, "0")*20;
    stath = chrh+8;
    ami_scrollvertsizg(listwf, &sbw, &wy);
    ami_scrollvertg(listwf, 1, 1, sbw, chrh*10, SBLIST); /* moved by layout */
    layout(TRUE);
    /* Start from the store, not from the server. Whatever was fetched
       before can be read without a network at all, which is the point of
       keeping it in files, and starting this way means the program comes
       up at once and comes up on a train. The server is asked only when
       the user asks for it. */
    datlock = ami_initlock(); /* before there is a second thread to want it */
    storeall();
    if (foldct) showfolder(0);
    drawfolders();
    drawlist();
    if (!haveaccount())
        status("No account yet. Mail/Server asks for one.");
    else if (!foldct) status("Nothing fetched yet. Mail/Get Mail reads the "
                             "server.");
    else status("");
    /* The counts come from reading every mailbox through, which on a
       store of gigabytes is not something to do in front of somebody
       waiting for a window. The worker does it, and the pane fills in
       as it goes. */
    if (dofetch) fetchall(TRUE); else countlater();
    /* Look again every so often while the program is up. The timer is
       in hundred microsecond counts, so a second is ten thousand. */
    if (pollsec > 0) ami_timer(stdout, TIMPOLL, pollsec*10000, TRUE);
    /* Held for as long as this thread is doing anything, dropped while
       it waits for the next event. Every click, redraw, menu choice and
       tick is covered by it without a lock of its own, and the worker
       gets its turn in the gap -- which is nearly all of the time, since
       waiting for an event is nearly all this thread does. */
    dlock();
    do {

        dunlock();
        ami_event(stdin, &er);
        dlock();
        if (diag) switch (er.etype) {

            case ami_etresize:
                fprintf(stderr, "resize win %lld: %lldx%lld\n", AMI_LONG_CAST(er.winid),
                        AMI_LONG_CAST(er.rszxg), AMI_LONG_CAST(er.rszyg));
                break;
            case ami_etredraw:
                fprintf(stderr, "redraw win %lld: %lld,%lld to %lld,%lld\n",
                        AMI_LONG_CAST(er.winid), AMI_LONG_CAST(er.rsx), AMI_LONG_CAST(er.rsy), AMI_LONG_CAST(er.rex), AMI_LONG_CAST(er.rey));
                break;
            default: break;

        }
        /* Larger and smaller belong to the program and not to any one
           window of it: whichever window the user had in hand when they
           asked, it is all of it that changes size. So these are taken
           before the event is handed to the window it came from. */
        if (er.etype == ami_etusize) { setzoom(1.0); continue; }
        if (er.etype == ami_etdsize) { setzoom(-1.0); continue; }
        /* Every event names the window it came from. The reader is a
           window of its own and the panes are windows of their own, so
           this one loop serves them all. */
        /* The message menu goes away on Escape, wherever the keys are,
           and on a click anywhere but on it: a menu nobody wants is put
           away without choosing from it. The list places a click for
           itself, since the menu may stand over it. */
        if (popwf) {

            if (er.etype == ami_etcan) { popclose(); continue; }
            if (er.etype == ami_etmouba && er.winid != POPWIN &&
                er.winid != LISTWIN) { popclose(); continue; }

        }
        if (er.winid == HELPWIN) { helpevent(&er); continue; }
        if (er.winid == SRCWIN) { srcevent(&er); continue; }
        if (er.winid == CMPWIN) { cmpevent(&er); continue; }
        if (er.winid == SRVWIN) { srvevent(&er); continue; }
        if (er.winid == OPTWIN) { optevent(&er); continue; }
        if (er.winid == READWIN) {

            switch (er.etype) {

                case ami_etmenus:
                    if (er.menuid == MENUREPLY || er.menuid == MENUREPALL ||
                        er.menuid == MENUFWD) answer(er.menuid);
                    break;
                case ami_etterm: closeread(); break;
                case ami_etredraw:
                case ami_etresize: wrapread(); drawread(); break;
                case ami_etmouba:
                    if (er.amoubn == 4) { readtop -= WHEELROWS; showread(); }
                    else if (er.amoubn == 5)
                        { readtop += WHEELROWS; showread(); }
                    break;
                case ami_etsclull: readtop--; showread(); break;
                case ami_etscldrl: readtop++; showread(); break;
                case ami_etsclulp: readtop -= readpage()-1; showread(); break;
                case ami_etscldrp: readtop += readpage()-1; showread(); break;
                case ami_etsclpos:
                    /* echo the bar where the user put it and do not set
                       it again from the view, which would drag the
                       thumb back to the nearest whole line */
                    ami_scrollpos(readwf, SBREAD, er.sclpos);
                    readtop = scaleback(er.sclpos, readlines-1);
                    fromdrag = TRUE;
                    showread();
                    fromdrag = FALSE;
                    break;
                case ami_etup: readtop--; showread(); break;
                case ami_etdown: readtop++; showread(); break;
                case ami_etpagu: readtop -= readpage()-1; showread(); break;
                case ami_etpagd: readtop += readpage()-1; showread(); break;
                case ami_ethome: readtop = 0; showread(); break;
                case ami_etend: readtop = readlines; showread(); break;
                case ami_etcan: closeread(); break;
                default: break;

            }
            continue;

        }
        if (er.winid == BANWIN) {

            /* Sized by the layout, which draws what the sizing moved; a
               redraw is drawn from its left edge, except the right strip
               a wider banner is told of after the layout drew it. */
            if (er.etype == ami_etredraw) {

                if (er.rsx > 0 && banpart) banpart = FALSE;
                else drawbanner(er.rsx);

            }

            continue;

        }
        if (er.winid == FOLDWIN) {

            switch (er.etype) {

                case ami_etmoumovg: mpx = er.moupxg; mpy = er.moupyg; break;
                case ami_etmouba: {

                    int best = -1;

                    if (er.amoubn != 1) break;
                    /* Which folder the click landed on, found from where
                       each was drawn rather than by counting rows: the
                       two headings and the rule between the lists make
                       row arithmetic wrong. */
                    for (i = 0; i < foldct; i++)
                        if (mpy >= foldy[i]-2 && mpy < foldy[i]+chrh+2)
                            best = i;
                    if (best >= 0) showfolder(best);
                    break;

                }
                case ami_etredraw: drawfoldrect(er.rsy, er.rey); break;
                case ami_etresize: /* sized by the layout, which drew it */
                    break;
                default: break;

            }
            continue;

        }
        if (er.winid == POPWIN) {

            switch (er.etype) {

                /* The menu does nothing at all until a button goes
                   down: it notes where the mouse is and no more. It
                   drew itself again on every move to follow the pointer
                   with a highlight, and that redraw was what made it
                   disappear the moment the mouse set off towards an
                   entry. A menu that waits for the click is what every
                   other program does anyway. */
                case ami_etmoumovg: {

                    ami_long r = (er.moupyg-3)/(poprowh? poprowh: 1);

                    poprow = r >= 0 && r < 3? r: -1;
                    break;

                }
                case ami_etmouba: /* a button down on an entry takes it */
                    if (poprow >= 0) popact(poprow);
                    else popclose(); /* down on the edge: put it away */
                    break;
                case ami_etmoubd: break; /* the opening click coming up */
                case ami_etcan:
                case ami_etterm: popclose(); break;
                default: break;

            }
            continue;

        }
        if (er.winid == LISTWIN) {

            /* An open menu is cancelled by the next click, and by
               nothing else, which is what every other program does.
               Closing on anything that was not a mouse move looked
               reasonable and was not: the button that opened it comes
               up a moment later, and the mouse leaving this pane for
               the menu sends this pane a nohover -- so the menu was
               destroyed either as it was born or the moment the mouse
               set off towards it. */
            if (popwf && er.etype == ami_etmouba) {

                /* A click while the menu is open: on an entry it takes
                   the entry, though the click came to the list -- the
                   menu had not been told of the pointer's crossing and
                   knew no row -- and anywhere else it puts the menu
                   away. */
                int px = mpx+listx, py = mpy+listy;

                if (er.amoubn == 1 && px >= popx && px < popx+popw &&
                    py >= popy && py < popy+poph) {

                    int r = (py-popy-3)/(poprowh? poprowh: 1);

                    if (r >= 0 && r < 3) popact(r); else popclose();

                } else popclose();
                continue;

            }
            switch (er.etype) {

                case ami_etmoumovg: mpx = er.moupxg; mpy = er.moupyg; break;
                case ami_etmouba:
                    if (er.amoubn == 4) {

                        msgtop -= WHEELMSGS;
                        showlist();

                    } else if (er.amoubn == 5) {

                        msgtop += WHEELMSGS;
                        showlist();

                    } else if (er.amoubn == 1) {

                        i = msgtop+(mpy-4)/rowh;
                        if (i >= 0 && i < msgct) {

                            /* a click picks the message; a second on the
                               same one, within a double click's time,
                               opens it */
                            ami_long t = msnow();

                            if (i == msgsel && t-clickms < DBLMS)
                                { openmsg(i); clickms = 0; }
                            else { selectmsg(i); clickms = t; }

                        }

                    } else if (er.amoubn == 2 || er.amoubn == 3) {

                        /* the second button: the message menu */
                        i = msgtop+(mpy-4)/rowh;
                        if (i >= 0 && i < msgct) {

                            selectmsg(i);
                            popopen(i, mpx, mpy);

                        }

                    }
                    break;
                case ami_etsclull: case ami_etup:
                    msgtop--;
                    showlist();
                    break;
                case ami_etscldrl: case ami_etdown:
                    msgtop++;
                    showlist();
                    break;
                case ami_etsclulp: case ami_etpagu:
                    msgtop -= listvis()-1;
                    showlist();
                    break;
                case ami_etscldrp: case ami_etpagd:
                    msgtop += listvis()-1;
                    showlist();
                    break;
                case ami_etsclpos:
                    ami_scrollpos(listwf, SBLIST, er.sclpos);
                    msgtop = scaleback(er.sclpos, msgct-listvis());
                    fromdrag = TRUE;
                    showlist();
                    fromdrag = FALSE;
                    break;
                case ami_etenter: /* return opens the message picked */
                    if (msgsel >= 0) openmsg(msgsel);
                    break;

                case ami_etredraw: /* only what was exposed */
                    /* the right strip a wider pane is told of was drawn
                       by the layout, with the rows' tails */
                    if (er.rsx > 0 && listpart) listpart = FALSE;
                    else if (foldsel >= 0 && msgct) listrect(er.rsy, er.rey);
                    else drawlist();
                    break;

                case ami_etresize: break; /* sized by the layout, which
                                             drew what the sizing moved */
                default: break;

            }
            continue;

        }
        switch (er.etype) {

            case ami_etresize:
                /* Buffer follow mode. The buffer is made the size the
                   window now is, which leaves what was already drawn
                   where it was, and the panes are placed again. Nothing
                   is drawn here: the redraws that follow say what the
                   resize exposed, and the panes report their own. */
                ami_sizbufg(stdout, er.rszxg, er.rszyg);
                layout(FALSE);
                break;

            case ami_etredraw:
                /* What this window shows for itself is the divider
                   between the panes and the strip at the foot, so that
                   is all a redraw of it costs. */
                /* The band under the menu is ground the panes used to
                   stand on: when they move down to make room for it,
                   what they leave is the main window's again, and the
                   main window has to paint it. */
                divider(stdout, foldw+4, 1+banh, foldw+4,
                        ami_maxyg(stdout)-stath);
                drawstatus();
                break;

            case ami_etenter: /* return opens the message picked */
                if (msgsel >= 0) openmsg(msgsel);
                break;

            case ami_ettim:
                /* one step of a fetch, if one is running */
                if (er.timnum == TIMFETCH) fetchpick();
                /* and the periodic look at the servers, which does
                   nothing while a fetch is already under way */
                else if (er.timnum == TIMPOLL && !fetching && haveaccount()) {

                    quietfail = TRUE; /* nobody asked for this one */
                    fetchall(FALSE);  /* look, but do not relist */
                    quietfail = FALSE;

                }
                break;

            case ami_etmenus:
                switch (er.menuid) {

                    case MENUSRV: srvopen(); break;
                    case MENUOPT: optopen(); break;

                    case MENUCOMP:
                        if (!haveaccount()) { srvopen(); break; }
                        cmpopen("", "", "", "", "", "");
                        break;
                    case MENUSEARCH: srcopen(); break;

                    case MENUFETCH:
                        if (!haveaccount()) {

                            status("No account yet. Mail/Server asks for "
                                   "one.");
                            srvopen();

                        } else fetchall(TRUE);
                        break;

                    case MENUFOLD: { /* every server's list, again */

                        int k;

                        if (!haveaccount()) { srvopen(); break; }
                        idxsetaside(); /* the indexes outlive the list */
                        foldct = 0;
                        foldsel = -1;
                        for (k = 0; k < srvct; k++) {

                            if (!*servers[k].imap || !*servers[k].user)
                                continue;
                            getfolders(k);
                            imapclose();

                        }
                        storefolders(-1);
                        idxgiveback();
                        countfolders();
                        kickworker();
                        drawfolders();
                        break;

                    }

                    case MENUCHECK:
                        if (!haveaccount()) { srvopen(); break; }
                        /* It sends over the same name and password the
                           fetch is using, and setting them up under a
                           fetch that is halfway through a login is how
                           one account's password goes to another
                           account's server. It waits. */
                        if (fetching) {

                            ami_alert("Mail", "Not while mail is being "
                                      "fetched. Try again when it has "
                                      "finished.");

                            break;

                        }
                        smtpcheck();
                        break;

                    case AMI_SMHELPTOPIC: helpopen(); break;

                    case AMI_SMABOUT:
                        ami_alert("Mail",
                                  "A mail reader on Petit-Ami graphics");
                        break;

                    case AMI_SMEXIT: goto done;

                }
                break;

            default: break;

        }

    /* a terminate for the reader closed the reader, not the program */
    } while (er.etype != ami_etterm || er.winid == READWIN ||
             er.winid == SRVWIN || er.winid == OPTWIN || er.winid == HELPWIN ||
             er.winid == CMPWIN || er.winid == SRCWIN);
    done:
    /* The lock is held here, so the worker is not in the middle of
       writing a message: what is in the store is whole. It is told to
       stop and is not waited for -- it may be in a read that has forty
       five seconds left to run, and nobody should wait that long to
       close a window. Nor is its connection closed from here: it is the
       worker's, and closing it under a thread reading it is how a tidy
       exit turns into a crash. Every message is written and closed as it
       arrives, so there is nothing outstanding to lose. */
    wrkstop = TRUE;
    if (fetching) ami_killtimer(stdout, TIMFETCH);
    popclose();
    helpclose();
    srcclose();
    optclose();
    srvclose();
    closeread();
    cmpclose();
    dunlock();

    return (0);

}
