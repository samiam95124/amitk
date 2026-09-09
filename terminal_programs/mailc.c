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
#include <terminalw.h>
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
#define SRCWIN   10 /* the search form */

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
static int   foldw;             /* the width of the folder pane */
static ami_long sbw;               /* scroll bar thickness */
static int   listrows;          /* message lines the list holds */
static int   foldy[MAXFOLDER];  /* where each folder was drawn, for clicks */
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
static void layout(void);
static void drawfolders(void);
static void drawlist(void);

/*******************************************************************************

Odds and ends

*******************************************************************************/

/* A colour named the way everybody names one, in parts of 255.

   The library takes each part as a fraction of the whole range of a
   long, not as a byte: 245 out of LONG_MAX is not a light grey, it is
   black, and every "quiet grey" in this program was black until this
   was noticed. Written once, here, so it cannot be got wrong twice. */
static int rgb(int c)

{

    if (c < 0) c = 0;
    if (c > 255) c = 255;

    return (c*(LONG_MAX/255));

}

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
    copystr(key, at+1, kl);

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
    ami_long w = ami_maxx(popwf);
    ami_long h = ami_maxy(popwf);

    fprintf(popwf, "\f");
    /* the edge, drawn in characters, so it reads as a card over the
       list */
    ami_cursor(popwf, 1, 1);
    fputc('+', popwf);
    for (i = 2; i < w; i++) fputc('-', popwf);
    fputc('+', popwf);
    ami_cursor(popwf, 1, h);
    fputc('+', popwf);
    for (i = 2; i < w; i++) fputc('-', popwf);
    fputc('+', popwf);
    for (i = 2; i < h; i++) {

        ami_cursor(popwf, 1, i);
        fputc('|', popwf);
        ami_cursor(popwf, w, i);
        fputc('|', popwf);

    }
    for (i = 0; i < 3; i++) {

        ami_cursor(popwf, 3, 2+i);
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
        snprintf(poplab[1], sizeof(poplab[1]),
                 "Local folder for %s (%d here)", key, n);
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
    poprowh = 1; /* an entry is a row */
    w = (int)strlen(poplab[0]);
    if ((int)strlen(poplab[1]) > w) w = (int)strlen(poplab[1]);
    if ((int)strlen(poplab[2]) > w) w = (int)strlen(poplab[2]);
    w += 4;      /* the frame and a space each side */
    h = 3+2;     /* three entries inside the frame */
    /* The menu is a child of the main window, not of the list: a child
       is clipped by its parent, and a menu opened near the bottom of
       the list would be cut off by it. The mouse position arrives in
       the list's coordinates, so it is shifted by where the list sits. */
    x += listx;
    y += listy;
    if (x+w > ami_maxx(stdout)) x = ami_maxx(stdout)-w;
    if (y+h > ami_maxy(stdout)) y = ami_maxy(stdout)-h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    ami_openwin(&stdin, &popwf, stdout, POPWIN);
    ami_frame(popwf, FALSE);
    ami_auto(popwf, FALSE);
    ami_curvis(popwf, FALSE);
    ami_setsiz(popwf, w, h);
    ami_setpos(popwf, x, y);
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
    int   moved;
    int   i = popmsg;
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

        int m;
        char key[100];

        senderkey(msgs[i].addr, key, sizeof(key));
        for (m = 0; m < msgct; m++)
            if (fromsender(&msgs[m], key)) set[m] = TRUE;
        copystr(who, msgs[i].from, sizeof(who));
        dst = localfolder(who);

    } else { /* everything from that name */

        int m;

        for (m = 0; m < msgct; m++)
            if (!strcmp(msgs[m].from, msgs[i].from)) set[m] = TRUE;
        copystr(who, msgs[i].from, sizeof(who));
        dst = localfolder(who);

    }
    if (dst < 0) { free(set); fail("No room for another folder"); return; }
    moved = movelocal(foldsel, folders[dst].file, set);
    free(set);
    msgsel = -1;
    /* The counts are worked out rather than counted again: what left
       this folder arrived in that one, and counting means reading every
       mailbox in the store through -- gigabytes, to learn a number
       already known. The folder itself is read again by the worker,
       since its file has just changed under the list. */
    folders[foldsel].msgs -= moved;
    if (folders[foldsel].msgs < 0) folders[foldsel].msgs = 0;
    folders[dst].msgs += moved;
    folders[foldsel].dirty = TRUE;
    folders[dst].dirty = TRUE;
    msgct = 0;
    idxfold = -1;
    idxwant = foldsel;
    kickworker();
    drawlist();
    drawfolders();
    snprintf(msg, sizeof(msg), "%d message%s moved to %s -- locally; the "
             "server is not touched", moved, moved == 1? "": "s", who);
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

/* What the strip says when it has nothing else to say. A terminal shows
   no menu until it is asked, so the keys live here where they can be
   read. */
#define KEYHELP "Tab/f folder  arrows list  Enter read  c compose  " \
                "g get mail  / search  s servers  h help  q quit"

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

    ami_long n = (ami_maxy(cmpwf)-cmpy0-8)/chrh;

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
    int k;
    ami_long w = ami_maxx(cmpwf)-cmpsbw;

    if (!cmpwf) return;
    cmprows = cmpvis();
    if (cmpcl < cmptop) cmptop = cmpcl;
    if (cmpcl >= cmptop+cmprows) cmptop = cmpcl-cmprows+1;
    if (cmptop < 0) cmptop = 0;
    y = cmpy0;
    for (i = cmptop; i < cmptop+cmprows; i++) {

        ami_cursor(cmpwf, 1, y);
        for (k = 0; k < w; k++) fputc(' ', cmpwf);
        if (i < cmpct) {

            char line[CMPMAX];

            copystr(line, cmpline[i], sizeof(line));
            fitchars(cmpwf, line, w-2);
            ami_cursor(cmpwf, 2, y);
            fprintf(cmpwf, "%s", line);
            if (i == cmpcl && cmpfocus) {

                /* The caret is the cell the next character would land
                   in, shown in reverse video: a terminal's caret. */
                char c = cmpcc < (int)strlen(cmpline[i])?
                         cmpline[i][cmpcc]: ' ';

                ami_reverse(cmpwf, TRUE);
                ami_cursor(cmpwf, 2+cmpcc, y);
                fputc(c, cmpwf);
                ami_reverse(cmpwf, FALSE);

            }

        }
        y++;

    }
    cmpbar();

}

static void cmplay(void)

{

    int chrw = (int)strlen("0");
    int labw = (int)strlen("Subject  ");
    ami_long ew, eh, bw, bh;
    int y;
    static const char* lab[] = { "To", "Cc", "Subject" };
    static const int   wid[] = { CMPTO, CMPCC, CMPSUB };
    int i;

    ami_editboxsiz(cmpwf, "0", &ew, &eh);
    ami_buttonsiz(cmpwf, "Cancel", &bw, &bh);
    fprintf(cmpwf, "\f");
    ami_fcolor(cmpwf, ami_black);
    /* In cells nothing is half a character tall: a field is a row and
       the rows go down one at a time. The pixel version's fractional
       paddings all come to zero here, and a widget at row zero is a
       widget off the window. */
    y = 1;
    for (i = 0; i < 3; i++) {

        ami_cursor(cmpwf, 2, y);
        fprintf(cmpwf, "%s", lab[i]);
        ami_poswidget(cmpwf, wid[i], 2+labw, y);
        ami_sizwidget(cmpwf, wid[i], ami_maxx(cmpwf)-labw-4, eh);
        y += eh;

    }
    ami_poswidget(cmpwf, CMPSEND, 2+labw, y);
    ami_sizwidget(cmpwf, CMPSEND, bw, bh);
    ami_poswidget(cmpwf, CMPCAN, 2+labw+bw+2, y);
    ami_sizwidget(cmpwf, CMPCAN, bw, bh);
    y += bh;
    /* the line that says the message begins here */
    divider(cmpwf, 1, y, ami_maxx(cmpwf), y);
    cmpy0 = y+1;
    ami_poswidget(cmpwf, CMPSB, ami_maxx(cmpwf)-cmpsbw, cmpy0);
    ami_sizwidget(cmpwf, CMPSB, cmpsbw, ami_maxy(cmpwf)-cmpy0);
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
    /* most of the terminal, sized as a window for the same reason the
       reader is: the decorations are the manager's business, not a sum
       to be guessed at */
    ami_scnsiz(cmpwf, &wx, &wy);
    ami_setsiz(cmpwf, wx-4, wy-2);
    ami_setpos(cmpwf, 3, 2);
    {

        ami_long ew, eh, bw, bh;

        ami_editboxsiz(cmpwf, "0", &ew, &eh);
        ami_buttonsiz(cmpwf, "Cancel", &bw, &bh);
        ami_editbox(cmpwf, 1, 1, 100, 1+eh, CMPTO);
        ami_editbox(cmpwf, 1, 1, 100, 1+eh, CMPCC);
        ami_editbox(cmpwf, 1, 1, 100, 1+eh, CMPSUB);
        ami_button(cmpwf, 1, 1, 1+bw, 1+bh, "Send", CMPSEND);
        ami_button(cmpwf, 1, 1, 1+bw, 1+bh, "Cancel", CMPCAN);
        ami_scrollvertsiz(cmpwf, &cmpsbw, &eh);
        ami_scrollvert(cmpwf, 1, 1, cmpsbw, chrh*10, CMPSB);

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
            ami_sizbuf(cmpwf, er->rszx, er->rszy);
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
                        if (8+(int)strlen(upto) > cmpmx) break;
                        upto[i+1] = c;

                    }
                    cmpcc = i;

                }
                cmpdraw();

            } else if (er->amoubn == 1) cmpfocus = FALSE;
            break;
        case ami_etmoumov: cmpmx = er->moupx; cmpmy = er->moupy; break;
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



static void drawbanner(void)

{

    int i;
    const char* cat = "=^.^=  ~mail~";

    if (!banwf) return;
    fprintf(banwf, "\f");
    /* the name at the left; the terminal has no point sizes, so what
       says "title" here is boldness and room of its own */
    ami_bold(banwf, TRUE);
    ami_cursor(banwf, 2, 1);
    fprintf(banwf, "Ami Mail");
    ami_bold(banwf, FALSE);
    /* the cat, reduced to what characters can say of one */
    ami_cursor(banwf, ami_maxx(banwf)-(int)strlen(cat)-1, 1);
    fprintf(banwf, "%s", cat);
    /* the double line under it */
    ami_cursor(banwf, 1, 2);
    for (i = 0; i < ami_maxx(banwf); i++) fputc('=', banwf);

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

    int y = ami_maxy(stdout);   /* the bottom row is the strip */
    int bx = ami_maxx(stdout)-progw-2;
    int i;
    char t[MAXSTR];


    /* the row, cleared in reverse video, which is what a status line
       looks like on a terminal */
    ami_reverse(stdout, TRUE);
    ami_cursor(stdout, 1, y);
    /* One short of the width. The strip is the last row, and writing
       the last column of the last row is what a terminal answers by
       scrolling -- the row went by before it could be seen. */
    for (i = 0; i < ami_maxx(stdout)-1; i++) fputc(' ', stdout);
    if (*statsaid) {

        copystr(t, statsaid, sizeof(t));
        /* it shares the strip with the bar, and never writes over it */
        fitchars(stdout, t, bx-3);
        ami_cursor(stdout, 2, y);
        fprintf(stdout, "%s", t);

    }
    /* the bar: what is done in blocks, what is left in dots */
    if (statmax > 0) {

        int w = progw*statpos/statmax;

        ami_cursor(stdout, bx, y);
        fputc('[', stdout);
        for (i = 0; i < progw-2; i++)
            fputc(i < w*(progw-2)/progw? '#': '.', stdout);
        fputc(']', stdout);

    }
    ami_reverse(stdout, FALSE);

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

/* How many characters of the string fit in the width. In cells that is
   no measurement at all: a string fits w columns when it is w long, so
   the halving search of the graphical version collapses to a cut. */
static int fitchars(FILE* f, char* s, int w)

{

    int n = (int)strlen(s);

    (void)f;
    if (w < 0) w = 0;
    if (n > w) { s[w] = 0; n = w; }

    return (n);

}

/* The dividers between the parts of the display, all drawn alike: a
   quiet grey, so they mark the columns off without shouting over the
   text the way black rules would. */
/* In characters a line is a run of characters: a bar for the vertical,
   a dash for the horizontal. The grey of the graphical version is not
   asked for -- a terminal that can do colour draws these well enough in
   its own, and one that cannot still draws them. */
static void divider(FILE* f, int x1, int y1, int x2, int y2)

{

    int i;

    if (x1 == x2) for (i = y1; i <= y2; i++) {

        ami_cursor(f, x1, i);
        fputc('|', f);

    } else {

        ami_cursor(f, x1, y1);
        for (i = x1; i <= x2; i++) fputc('-', f);

    }

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

static void drawfolders(void)

{

    int i;
    int y = 1;
    ami_long cw;
    int sec;
    int w;
    char cnt[40];
    char head[120];

    if (!foldwf) return;
    w = ami_maxx(foldwf);
    /* Every row prints its own whole line, spaces and all, so nothing
       has to be cleared first and nothing blinks: what was there is
       simply written over. */
    /* a section for each server, then one for the folders that are ours
       alone, since two servers may each have an INBOX and a local Trash
       is not the server's Trash */
    for (sec = 0; sec <= srvct; sec++) {

        int shown = 0;
        int srv = sec < srvct? sec: -1;

        if (sec) { /* a rule between the sections */

            ami_cursor(foldwf, 1, y);
            for (i = 0; i < w; i++) fputc('-', foldwf);
            y++;

        }
        if (srv >= 0) snprintf(head, sizeof(head), "%s Server Folders",
                               servers[srv].name);
        else copystr(head, "Local Folders", sizeof(head));
        clipstr(foldwf, head, w-2);
        ami_cursor(foldwf, 1, y);
        for (i = 0; i < w; i++) fputc(' ', foldwf);
        ami_bold(foldwf, TRUE);
        ami_cursor(foldwf, 1, y);
        fprintf(foldwf, "%s", head);
        ami_bold(foldwf, FALSE);
        y++;
        for (i = 0; i < foldct; i++) {

            char nm[MAXSTR];

            if (folders[i].srv != srv) continue;
            if (folders[i].local != (srv < 0)) continue;
            shown++;
            foldy[i] = y; /* where it landed, for the click to find */
            copystr(nm, folders[i].show, MAXSTR);
            cnt[0] = 0;
            if (folders[i].msgs > 0) commas(folders[i].msgs, cnt,
                                            sizeof(cnt));
            cw = *cnt? (int)strlen(cnt)+2: 0;
            /* the one being read stands in reverse video, which is what
               a terminal has instead of a coloured bar */
            ami_reverse(foldwf, i == foldsel);
            ami_cursor(foldwf, 1, y);
            {

                int k;

                for (k = 0; k < w; k++) fputc(' ', foldwf);

            }
            clipstr(foldwf, nm, w-2-cw);
            ami_cursor(foldwf, 2, y);
            fprintf(foldwf, "%s", nm);
            if (*cnt) {

                ami_cursor(foldwf, w-(int)strlen(cnt), y);
                fprintf(foldwf, "%s", cnt);

            }
            ami_reverse(foldwf, FALSE);
            y++;

        }
        if (!shown) { /* say so rather than leave a gap */

            ami_cursor(foldwf, 1, y);
            {

                int k;

                for (k = 0; k < w; k++) fputc(' ', foldwf);

            }
            ami_cursor(foldwf, 2, y);
            fprintf(foldwf, "%s", srv >= 0? "(not fetched)": "(none yet)");
            y++;

        }

    }
    while (y <= ami_maxy(foldwf)) { /* the room the sections did not fill */

        ami_cursor(foldwf, 1, y);
        for (i = 0; i < ami_maxx(foldwf); i++) fputc(' ', foldwf);
        y++;

    }

}


/* One line of the message list: who it is from, then the subject, then
   as much of the message as is left over, then when it came. This is the
   layout the web readers use and it is the right one: the eye runs down
   the senders, and the subject and the start of the text read as one
   sentence. */
static void drawmsg(int i, int y)

{

    msgrec* m = &msgs[i];
    ami_long w = ami_maxx(listwf)-sbw;
    int     x;
    int     k;
    char    s[MAXSTR+SNIPPET];
    int     subw;

    /* the whole row printed over, its own ground included; the one that
       is selected stands in reverse video */
    ami_reverse(listwf, i == msgsel);
    ami_cursor(listwf, 1, y);
    for (k = 0; k < w; k++) fputc(' ', listwf);
    /* the sender, bold, in its column */
    ami_bold(listwf, TRUE);
    copystr(s, m->from, MAXSTR);
    clipstr(listwf, s, fromx-3);
    ami_cursor(listwf, 2, y);
    fprintf(listwf, "%s", s);
    ami_bold(listwf, FALSE);
    /* what kind of mail it is, in its own column */
    copystr(s, m->cat, sizeof(s));
    clipstr(listwf, s, catx-fromx-3);
    ami_cursor(listwf, fromx+2, y);
    fprintf(listwf, "%s", s);
    /* the subject, then the start of the message after it */
    x = catx+2;
    subw = datex-x-2;
    copystr(s, m->subject, MAXSTR);
    clipstr(listwf, s, subw);
    ami_cursor(listwf, x, y);
    fprintf(listwf, "%s", s);
    x += (int)strlen(s);
    if (x < datex-8 && *m->snip) {

        snprintf(s, sizeof(s), " - %s", m->snip);
        clipstr(listwf, s, datex-x-2);
        ami_cursor(listwf, x, y);
        fprintf(listwf, "%s", s);

    }
    /* the date, against the right */
    copystr(s, m->when, sizeof(m->when));
    ami_cursor(listwf, w-(int)strlen(s), y);
    fprintf(listwf, "%s", s);
    /* this row's piece of the column dividers */
    ami_cursor(listwf, fromx, y);
    fputc('|', listwf);
    ami_cursor(listwf, catx, y);
    fputc('|', listwf);
    ami_cursor(listwf, datex-1, y);
    fputc('|', listwf);
    ami_reverse(listwf, FALSE);

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
    y = 1+(i-msgtop);
    if (y > ami_maxy(listwf)-1) return; /* not on the screen */
    drawmsg(i, y);

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

    ami_long n = (ami_maxy(listwf)-4)/rowh;

    return (n < 1? 1: n);

}

/* keep the top of the list inside the list */
static void listclamp(void)

{

    if (msgtop > msgct-listvis()) msgtop = msgct-listvis();
    if (msgtop < 0) msgtop = 0;

}

static void setlistbar(void);   /* forward */

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
        ami_scroll(listwf, 0, d);
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
    int y = 1;
    int k;

    if (!listwf) return;
    if (foldsel < 0) {

        fprintf(listwf, "\f");
        ami_cursor(listwf, 2, 1);
        fprintf(listwf, "Pick a folder.");

        return;

    }
    if (!msgct) {

        fprintf(listwf, "\f");
        ami_cursor(listwf, 2, 1);
        /* An empty list means one of two things and they are not alike:
           there is nothing in the folder, or there is and it has not
           been read yet. */
        if (idxwant == foldsel || idxdoing == foldsel)
            fprintf(listwf, "Reading %s...", folders[foldsel].show);
        else fprintf(listwf, "Nothing in %s yet. Mail/Fetch reads the "
                     "server.", folders[foldsel].show);

        return;

    }
    for (i = msgtop; i < msgct && y <= ami_maxy(listwf); i++) {

        drawrow(i);
        y++;

    }
    while (y <= ami_maxy(listwf)) { /* the room the rows did not fill,
                                       with the dividers carried down */

        ami_cursor(listwf, 1, y);
        for (k = 1; k <= ami_maxx(listwf)-sbw; k++)
            fputc(k == fromx || k == catx || k == datex-1? '|': ' ', listwf);
        y++;

    }
    listshown = msgtop;
    setlistbar();

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
    ami_long    w = ami_maxx(readwf)-sbw-2;
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

    ami_long page = ami_maxy(readwf); /* every row shows a line */

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

    int k;

    if (a < readtop) a = readtop;
    if (b > readlines-1) b = readlines-1;
    if (a > b) return;
    for (i = a; i <= b; i++) {

        ami_cursor(readwf, 1, 1+(i-readtop));
        for (k = 1; k <= ami_maxx(readwf)-sbw; k++) fputc(' ', readwf);
        if (*readline[i]) {

            ami_cursor(readwf, 2, 1+(i-readtop));
            fprintf(readwf, "%s", readline[i]);

        }

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
        ami_scroll(readwf, 0, d);
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

/* Open a message to be read: this one, of this folder. The message list
   opens its own by number; the search opens what it found, which may be
   in any folder. */
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
        /* A terminal is small: the reader takes most of it. The WINDOW
           is sized, not the client, and from the screen itself: asking
           winclient to work back from a wished-for client means
           guessing the decorations -- frame, title, underbar, and the
           menu this window carries -- and every wrong guess is a row
           hanging off the screen. Sized as a window, the bottom border
           lands where it is put and the client is whatever remains,
           which is what the drawing adapts to anyway. */
        ami_scnsiz(readwf, &wx, &wy);
        ami_setsiz(readwf, wx-4, wy-2);
        ami_setpos(readwf, 3, 2);
        /* The bar down the right. Its thickness is asked for into a
           variable of its own: asking into wy, which is where the
           window height was, left the window sized from a scroll bar's
           nominal height -- the bar came out a full width wide, placed
           at the right edge, and stood entirely off the window. */
        {

            ami_long bh;

            ami_scrollvertsiz(readwf, &sbw, &bh);

        }
        ami_scrollvert(readwf, ami_maxx(readwf)-sbw+1, 1, ami_maxx(readwf),
                       ami_maxy(readwf), SBREAD);
        ami_frontwidget(readwf, SBREAD); /* the window fronts over it */

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

static void closeread(void)

{

    int i;

    if (!readwf) return;
    ami_killwidget(readwf, SBREAD); /* the manager's record of it first */
    fclose(readwf);
    readwf = NULL;
    for (i = 0; i < readlines; i++) free(readline[i]);
    readlines = 0;
    free(readtext);
    readtext = NULL;

}

static void openmsg(int i)

{

    if (i >= 0 && i < msgct) openmsgin(foldsel, &msgs[i]);

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

    int chrw = (int)strlen("0");
    int labw = (int)strlen("Sending server  ");
    ami_long ew, eh, bw, bh;
    int y;
    int i;

    ami_editboxsiz(srvwf, "0", &ew, &eh);
    ami_buttonsiz(srvwf, "Cancel", &bw, &bh);
    fprintf(srvwf, "\f");
    ami_fcolor(srvwf, ami_black);
    y = chrh;
    for (i = 0; i < SRVFLDS; i++) {

        ami_cursor(srvwf, chrw*2, y+(eh-chrh)/2);
        fprintf(srvwf, "%s", srvfld[i].label);
        ami_poswidget(srvwf, srvfld[i].id, chrw*2+labw, y);
        ami_sizwidget(srvwf, srvfld[i].id, chrw*30, eh);
        /* what the field is for, beside it, since a form that only says
           "Port" leaves the reader to guess which port */
        ami_cursor(srvwf, chrw*2+labw+chrw*32, y+(eh-chrh)/2);
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

        ami_checkboxsiz(srvwf, "Send mail from this account", &cw, &ch);
        ami_poswidget(srvwf, SRVSEND, chrw*2+labw, y);
        ami_sizwidget(srvwf, SRVSEND, cw, ch);
        ami_selectwidget(srvwf, SRVSEND, srvedit == sendsrv);
        y += ch+chrh/2;

    }
    ami_poswidget(srvwf, SRVOK, chrw*2+labw, y);
    ami_sizwidget(srvwf, SRVOK, bw, bh);
    ami_poswidget(srvwf, SRVCAN, chrw*2+labw+bw+chrw*2, y);
    ami_sizwidget(srvwf, SRVCAN, bw, bh);
    ami_poswidget(srvwf, SRVNEXT, chrw*2+labw+(bw+chrw*2)*2, y);
    ami_sizwidget(srvwf, SRVNEXT, bw, bh);
    ami_poswidget(srvwf, SRVNEW, chrw*2+labw+(bw+chrw*2)*3, y);
    ami_sizwidget(srvwf, SRVNEW, bw, bh);
    ami_poswidget(srvwf, SRVDEL, chrw*2+labw+(bw+chrw*2)*4, y);
    ami_sizwidget(srvwf, SRVDEL, bw, bh);
    /* which of them is being shown, since the fields do not say */
    {

        char n[80];

        snprintf(n, sizeof(n), "account %d of %d", srvedit+1,
                 srvct > srvedit? srvct: srvedit+1);
        ami_fcolorc(srvwf, rgb(110), rgb(110), rgb(110));
        ami_cursor(srvwf, chrw*2, y+bh+chrh/2);
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
    /* The widgets first. The manager keeps a record for each, and a
       window closed over live widgets leaves those records pointing at
       a file that is gone -- which is the "Invalid file" that killed
       the program when Cancel was pressed. */
    for (i = SRVIMAP; i <= SRVSEND; i++) ami_killwidget(srvwf, i);
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
    ami_editboxsiz(srvwf, "0", &ew, &eh);
    ami_buttonsiz(srvwf, "Cancel", &bw, &bh);
    {

        /* Wide enough for whichever is wider: the fields with their
           notes beside them, or the row of buttons under them. The row
           was running off the edge, and the last button with it -- a
           form that will not show what it offers. */
        int chrw = (int)strlen("0");
        int labw = (int)strlen("Sending server  ");
        ami_long need = chrw*2+labw+(bw+chrw*2)*4+bw+chrw*2;
        int want = chrw*96;

        if (need > want) want = need;
        if (want > ami_maxx(stdout)-4) want = ami_maxx(stdout)-4;
        ami_winclient(srvwf, want, (eh+chrh/2)*SRVFLDS+bh*2+chrh*8, &wx, &wy,
                       BIT(ami_wmframe) | BIT(ami_wmsize) |
                       BIT(ami_wmsysbar));

    }
    ami_setsiz(srvwf, wx, wy);
    ami_setpos(srvwf, 3, 3);
    /* made here, placed by the layout, which runs again on a resize */
    for (i = 0; i < SRVFLDS; i++)
        ami_editbox(srvwf, 1, 1, 2, 2, srvfld[i].id);
    ami_button(srvwf, 1, 1, 2, 2, "Save", SRVOK);
    ami_button(srvwf, 1, 1, 2, 2, "Cancel", SRVCAN);
    ami_button(srvwf, 1, 1, 2, 2, "Next", SRVNEXT);
    ami_button(srvwf, 1, 1, 2, 2, "Add", SRVNEW);
    ami_button(srvwf, 1, 1, 2, 2, "Remove", SRVDEL);
    ami_checkbox(srvwf, 1, 1, 2, 2, "Send mail from this account", SRVSEND);
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

static void layout(void)

{

    /* The client is the inside: no frame, no system bar, no menu. The
       banner starts at its first row. */
    int top = 1+banh;
    int h = ami_maxy(stdout)-top-stath; /* the strip has the foot of it */

    /* The banner is as wide as the window and stays where it is put. */
    ami_setpos(banwf, 1, 1);
    ami_setsiz(banwf, ami_maxx(stdout), banh);
    ami_sizbuf(banwf, ami_maxx(stdout), banh);

    if (diag) fprintf(stderr, "layout: buf %lldx%lld stath %d progh %d "
                      "panes %d tall\n", AMI_LONG_CAST(ami_maxx(stdout)), AMI_LONG_CAST(ami_maxy(stdout)),
                      stath, progh, h);

    /* the main window shows between and around the panes, so it is
       cleared here rather than left as whatever was under it */
    fprintf(stdout, "\f");

    foldw = (int)strlen("0")*22;
    if (foldw > ami_maxx(stdout)/2) foldw = ami_maxx(stdout)/2;
    ami_setpos(foldwf, 1, top);
    /* The buffer follows the window. Sizing the window alone leaves the
       buffer the size it was, and a buffered window answers with its
       buffer -- so everything drawn from its measurements would be laid
       out for the pane it used to be. */
    ami_setsiz(foldwf, foldw, h);
    ami_sizbuf(foldwf, foldw, h);
    /* the gap between the panes holds the divider between the sections */
    listx = foldw+8;
    listy = top;
    ami_setpos(listwf, listx, listy);
    ami_setsiz(listwf, ami_maxx(stdout)-foldw-8, h);
    ami_sizbuf(listwf, ami_maxx(stdout)-foldw-8, h);
    divider(stdout, foldw+4, top, foldw+4, top+h);
    /* the columns of the list, kept here so the rows and their dividers
       agree on where the columns are */
    /* The columns adapt to the width there is. At full width the sender
       gets eighteen cells and the category twelve; in a narrow window
       fixed columns leave the subject nothing at all -- an eighty
       column window gave it zero -- so below that the sender and the
       category give up room in proportion and the subject keeps what
       they yield. The date column is never squeezed: a date that does
       not fit is not a date. */
    datex = ami_maxx(listwf)-sbw-(int)strlen("Sep 30, 2025 ");
    fromx = 18;
    {

        int catw = 12;

        if (datex-1-(fromx+catw) < 12) { /* the subject is starving */

            fromx = (datex-1)*2/5;
            if (fromx < 8) fromx = 8;
            catw = (datex-1)/6;
            if (catw > 12) catw = 12;
            if (catw < 4) catw = 4;

        }
        catx = fromx+catw;

    }
    /* The bar down the right of the message list is moved and sized, not
       made again: a widget id is taken until the widget is killed, so
       making it a second time is an error, and a resize would raise it. */
    ami_poswidget(listwf, SBLIST, ami_maxx(listwf)-sbw, 1);
    ami_sizwidget(listwf, SBLIST, sbw, ami_maxy(listwf));
    listrows = ami_maxy(listwf)/rowh;
    if (listrows < 1) listrows = 1;
    /* The panes are cleared and drawn again, since this is the one thing
       that moves what is in them: a pane that has changed height has the
       old rows where the new ones are not, and a buffer keeps whatever
       nothing has drawn over. Once per layout, which is once per resize
       -- not once per redraw, which is what made a resize flash. */
    fprintf(foldwf, "\f");
    fprintf(listwf, "\f");
    drawfolders();
    drawlist();
    drawstatus();
    drawbanner();

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
    ami_listbox(helpwf, helpx0, helpy0, helpx1, helpy1, sl, HELPLIST);
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
            if (n && (int)strlen(try) > w-ind) break;
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

            ind = (int)strlen("00");
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

    int chrh = 1;
    int x = helpx1+(int)strlen("00");
    int y = helpy0;
    int i;

    /* the pane, cleared a row at a time down to and including the line
       the count is written on */
    {

        int r, k;

        for (r = helpy0; r <= helpy1; r++) {

            ami_cursor(helpwf, x, r);
            for (k = x; k <= ami_maxx(helpwf); k++) fputc(' ', helpwf);

        }

    }
    helppage = (helpy1-helpy0)/chrh;
    if (helppage < 1) helppage = 1;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    for (i = helptop; i < helplinect && y+chrh <= helpy1; i++) {

        if (*helplines[i].s) {

            ami_bold(helpwf, helplines[i].bold);
            ami_cursor(helpwf, x+helplines[i].ind, y);
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
        ami_cursor(helpwf, x, helpy1);
        fprintf(helpwf, "%s", more);
        ami_fcolor(helpwf, ami_black);

    }

}

/* wrap and draw, which is what everything that changes the topic wants */
static void helptext(void)

{

    helplay1(ami_maxx(helpwf)-(int)strlen("00")-
             (helpx1+(int)strlen("00")));
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

    int chrh = 1;
    int chrw = (int)strlen("0");
    int lw   = chrw*30;  /* the topic list */
    ami_long bw, bh, ew, eh;

    ami_buttonsiz(helpwf, "Close", &bw, &bh);
    ami_editboxsiz(helpwf, "0", &ew, &eh);
    /* the search entry along the top of the list column */
    helpx0 = chrw*2;
    helpy0 = chrh*2+eh;
    helpx1 = helpx0+lw;
    helpy1 = ami_maxy(helpwf)-bh-chrh*2;
    if (helpy1 < helpy0+chrh) helpy1 = helpy0+chrh;
    ami_poswidget(helpwf, HELPFIND, helpx0+(int)strlen("Search: "),
                   chrh);
    ami_sizwidget(helpwf, HELPFIND,
                   lw-(int)strlen("Search: "), eh);
    ami_poswidget(helpwf, HELPCLOSE, ami_maxx(helpwf)-bw-chrw*2,
                   ami_maxy(helpwf)-bh-chrh/2);
    ami_sizwidget(helpwf, HELPCLOSE, bw, bh);
    /* The list is moved and sized rather than made again, so that the
       topic picked in it stays picked across a resize. Only a change
       of contents needs it made again, which is what helpfill is for. */
    if (helplistup) {

        ami_poswidget(helpwf, HELPLIST, helpx0, helpy0);
        ami_sizwidget(helpwf, HELPLIST, helpx1-helpx0, helpy1-helpy0);

    } else helpfill("");
    /* the frame, the label, and the topic */
    fprintf(helpwf, "\f");
    ami_fcolor(helpwf, ami_black);
    ami_cursor(helpwf, helpx0, chrh);
    fprintf(helpwf, "Search:");
    helptext();

}

/* close the help window, if it is open */
static void helpclose(void)

{

    int i;

    if (!helpwf) return;
    ami_killwidget(helpwf, HELPFIND);
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
    ami_scnsiz(helpwf, &wx, &wy);
    wx = wx-6 < 88? wx-6: 88;
    wy = wy-2 < 28? wy-2: 28;
    ami_setsiz(helpwf, wx, wy);
    /* The entry and the button are made once here and moved by the
       layout after. The list is made by the layout, which is where its
       rectangle is known and where it is made again on every search. */
    ami_editbox(helpwf, 1, 1, 2, 2, HELPFIND);
    ami_button(helpwf, 1, 1, 2, 2, "Close", HELPCLOSE);
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

Modelled on the one Gmail keeps behind the search box: who it is from
and to, what the subject holds, words it has and words it must not, how
big it is, when it came and where to look. The worker does the looking
(servesearch), since a store of tens of thousands of messages is read
from the disc, and the results come back on the tick as fetched mail
does. The rows found are drawn in the window itself, under the form,
and a message picked from them is opened to be read as one picked from
the list is.

*******************************************************************************/

static FILE*    srcwf;        /* the search window, NULL when not open */
static int      srclistup;    /* there are results to show */
static ami_long srcx0, srcy0, srcx1, srcy1; /* the list's rectangle */
static ami_long srcsbw;       /* the width of the bar beside it */
static int      srctop;       /* the first row on show */
static int      srcsel;       /* the row picked, or -1 */
static ami_long srcmx, srcmy; /* where the mouse is */
static ami_long srcsty;       /* where the status line is written */
static int      srcsizesel;   /* which size test, 1 based as the box is */
static int      srcunitsel;   /* which unit */
static int      srcwithinsel; /* which period */
static int      srcfoldsel;   /* which folder, the first being all */
static int      srcatt;       /* the attachment box is ticked */
static char     srcsaid[MAXSTR*2]; /* the status line, kept for redraws */

static void srcdraw(void);

static const char* srcsizeops[] = { "greater than", "less than" };
static const char* srcunits[] = { "MB", "KB", "bytes" };
static const ami_long srcunitval[] = { 1024L*1024, 1024L, 1L };
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
        if (!sp) { fail("Out of memory"); exit(1); }
        sp->str = strdup(items[i]);
        if (!sp->str) { fail("Out of memory"); exit(1); }
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

    ami_long w, i;

    if (!srcwf) return;
    copystr(srcsaid, s, sizeof(srcsaid));
    w = ami_maxx(srcwf);
    ami_cursor(srcwf, 1, srcsty);
    for (i = 1; i <= w; i++) fputc(' ', srcwf);
    ami_cursor(srcwf, 3, srcsty);
    fprintf(srcwf, "%.*s", (int)(w-3), srcsaid);

}

/* the labels, drawn again on every redraw; the widgets keep themselves */
static void srclay(void)

{

    static const char* labels[] = { "From", "To", "Subject", "Has the words",
                                    "Doesn't have", "Size", "Date within",
                                    "Search" };
    ami_long ew, eh, cw, ch, ow, oh, rowh, y;
    ami_strptr sl;
    int i;

    ami_editboxsiz(srcwf, "0", &ew, &eh);
    sl = strlist(srcsizeops, 2);
    ami_dropboxsiz(srcwf, sl, &cw, &ch, &ow, &oh);
    freelist(sl);
    rowh = eh > ch? eh: ch;
    fprintf(srcwf, "\f");
    ami_fcolor(srcwf, ami_black);
    y = 2;
    for (i = 0; i < 8; i++) {

        ami_cursor(srcwf, 3, y+(rowh-1)/2);
        fprintf(srcwf, "%s", labels[i]);
        y += rowh;

    }
    srcstatus(srcsaid);
    srcdraw();

}

/* how many rows of what was found fit */
static int srcvis(void)

{

    int n = srcy1-srcy0+1;

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

/* What was found, a row each: when it came, who from, what about.
   Newest first, as the message list is. */
static void srcdraw(void)

{

    ami_long w = srcx1-srcsbw-srcx0+1;
    ami_long x, y;
    char     s[MAXSTR];
    char     when[40];
    int      i, n;

    if (!srcwf) return;
    ami_fcolor(srcwf, ami_black);
    ami_reverse(srcwf, FALSE);
    for (y = srcy0; y <= srcy1; y++) {

        ami_cursor(srcwf, srcx0, y);
        for (x = 0; x < w; x++) fputc(' ', srcwf);

    }
    if (!srclistup) return;
    if (!srcct) {

        ami_cursor(srcwf, srcx0, srcy0);
        fprintf(srcwf, "Nothing found");
        return;

    }
    srcclamp();
    y = srcy0;
    for (i = srctop; i < srcct && y <= srcy1; i++) {

        const msgrec* m = &srcres[i];

        if (m->date) whenof(m->date, when, sizeof(when));
        else copystr(when, m->when, sizeof(when));
        ami_reverse(srcwf, i == srcsel);
        snprintf(s, sizeof(s), "%-12.12s %-20.20s %s", when, m->from,
                 m->subject);
        n = (int)strlen(s);
        if (n > w) n = w;
        ami_cursor(srcwf, srcx0, y);
        fprintf(srcwf, "%.*s", n, s);
        if (i == srcsel) for (x = n; x < w; x++) fputc(' ', srcwf);
        y++;

    }
    ami_reverse(srcwf, FALSE);
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
    srcstatus(t);

}

/* the list moved: drawn again from its new top */
static void srcscroll(int top)

{

    srctop = top;
    srcclamp();
    srcdraw();

}

/* a row picked: shown as picked, and the message read */
static void srcpickrow(int i)

{

    if (i < 0 || i >= srcct) return;
    srcsel = i;
    if (i < srctop) srctop = i;
    if (i >= srctop+srcvis()) srctop = i-srcvis()+1;
    srcdraw();
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

/* close the search window, if it is open. The widgets go first, as the
   server form's do: a window closed over live widgets leaves the
   manager's records pointing at a file that is gone. */
static void srcclose(void)

{

    int i;

    if (!srcwf) return;
    for (i = SRCFROM; i <= SRCSB; i++) ami_killwidget(srcwf, i);
    fclose(srcwf);
    srcwf = NULL;
    srclistup = FALSE;

}

/* Open the search window. A second open brings the one already up to
   the front. The fields are made where they stand, the form being of a
   fixed size; what is found is drawn below them. */
static void srcopen(void)

{

    static const int edits[] = { SRCFROM, SRCTO, SRCSUBJ, SRCWORDS, SRCNOT };
    ami_long wx, wy, sx, sy;
    ami_long labw, ew, eh, cw, ch, ow, oh, bw, bh, rowh, y, x, fw;
    ami_strptr sl;
    const char* folds[MAXFOLDER+1];
    int i;

    if (srcwf) { ami_front(srcwf); return; }
    ami_openwin(&stdin, &srcwf, NULL, SRCWIN);
    ami_title(srcwf, "Search mail");
    ami_buffer(srcwf, FALSE);
    ami_auto(srcwf, FALSE);
    ami_curvis(srcwf, FALSE);
    labw = (ami_long)strlen("Has the words  ");
    ami_editboxsiz(srcwf, "0", &ew, &eh);
    ami_buttonsiz(srcwf, "Search", &bw, &bh);
    sl = strlist(srcsizeops, 2);
    ami_dropboxsiz(srcwf, sl, &cw, &ch, &ow, &oh);
    freelist(sl);
    rowh = eh > ch? eh: ch;
    fw = 50; /* the fields' width */
    /* the form, its buttons and status line, and a dozen rows found;
       cut to the screen when the screen is smaller than that */
    ami_scnsiz(srcwf, &sx, &sy);
    ami_winclient(srcwf, 2+labw+fw+2, rowh*8+bh+2+14, &wx, &wy,
                  BIT(ami_wmframe) | BIT(ami_wmsysbar));
    if (wx > sx-4) wx = sx-4;
    if (wy > sy-2) wy = sy-2;
    ami_setsiz(srcwf, wx, wy);
    ami_setpos(srcwf, 3, 2);
    x = 3+labw;
    y = 2;
    for (i = 0; i < 5; i++) { /* the text fields */

        ami_editbox(srcwf, x, y, x+fw-1, y+eh-1, edits[i]);
        y += rowh;

    }
    /* the size: greater or less than, how much, of what */
    sl = strlist(srcsizeops, 2);
    ami_dropboxsiz(srcwf, sl, &cw, &ch, &ow, &oh);
    ami_dropbox(srcwf, x, y, x+ow-1, y+oh-1, sl, SRCSIZEOP);
    freelist(sl);
    ami_editbox(srcwf, x+ow+2, y, x+ow+11, y+eh-1, SRCSIZE);
    sl = strlist(srcunits, 3);
    ami_dropboxsiz(srcwf, sl, &cw, &ch, &ow, &oh);
    ami_dropbox(srcwf, x+fw-ow, y, x+fw-1, y+oh-1, sl, SRCUNIT);
    freelist(sl);
    y += rowh;
    /* the date: within this of that day */
    sl = strlist(srcwithins, 8);
    ami_dropboxsiz(srcwf, sl, &cw, &ch, &ow, &oh);
    ami_dropbox(srcwf, x, y, x+ow-1, y+oh-1, sl, SRCWITHIN);
    freelist(sl);
    ami_editbox(srcwf, x+ow+2, y, x+ow+15, y+eh-1, SRCDATE);
    y += rowh;
    /* the folder: all of them, or one */
    folds[0] = "All Mail";
    for (i = 0; i < foldct && i < MAXFOLDER; i++) folds[i+1] = folders[i].show;
    sl = strlist(folds, foldct+1);
    ami_dropboxsiz(srcwf, sl, &cw, &ch, &ow, &oh);
    ami_dropbox(srcwf, x, y, x+ow-1, y+oh-1, sl, SRCFOLD);
    freelist(sl);
    y += rowh;
    ami_checkbox(srcwf, x, y, x+22, y+eh-1, "Has attachment", SRCATT);
    y += rowh;
    /* the buttons, and the status line under them */
    ami_button(srcwf, x+fw-bw, y, x+fw-1, y+bh-1, "Search", SRCGO);
    ami_button(srcwf, x+fw-bw*2-2, y, x+fw-bw-3, y+bh-1, "Clear", SRCCLR);
    ami_button(srcwf, x+fw-bw*3-4, y, x+fw-bw*2-5, y+bh-1, "Close", SRCCLOSE);
    y += bh;
    srcsty = y;
    y += 1;
    /* what is found goes below, to the bottom */
    srcx0 = 3;
    srcy0 = y;
    srcx1 = ami_maxx(srcwf);
    srcy1 = ami_maxy(srcwf);
    if (srcy1 < srcy0) srcy1 = srcy0;
    ami_scrollvertsiz(srcwf, &srcsbw, &eh);
    ami_scrollvert(srcwf, srcx1-srcsbw+1, srcy0, srcx1, srcy1, SRCSB);
    srctop = 0;
    srcsel = -1;
    srcsizesel = 1;
    srcunitsel = 1;
    srcwithinsel = 1;
    srcfoldsel = 1;
    srcatt = FALSE;
    *srcsaid = 0;
    srclay();

}

/* an event with the search window's id on it */
static void srcevent(ami_evtrec* er)

{

    switch (er->etype) {

        case ami_etterm: srcclose(); break; /* the window closed, not the program */
        case ami_etresize:
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
            srcatt = !srcatt;
            ami_selectwidget(srcwf, SRCATT, srcatt);
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
        case ami_etmoumov: srcmx = er->moupx; srcmy = er->moupy; break;
        case ami_etmouba:
            if (er->amoubn == 4) srcscroll(srctop-WHEELROWS);
            else if (er->amoubn == 5) srcscroll(srctop+WHEELROWS);
            else if (er->amoubn == 1 && srclistup && srcmx >= srcx0 &&
                     srcmx <= srcx1-srcsbw && srcmy >= srcy0 && srcmy <= srcy1)
                srcpickrow(srctop+(srcmy-srcy0));
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

    } else if (srcbusy && srcwf && *wrkwhat) {

        char t[MAXSTR*2];
        char a[40], b[40];

        if (wrkmax > 0) {

            commas(wrkpos, a, sizeof(a));
            commas(wrkmax, b, sizeof(b));
            snprintf(t, sizeof(t), "%s - %s of %s", wrkwhat, a, b);

        } else copystr(t, wrkwhat, sizeof(t));
        srcstatus(t);

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
    newmenu(&mp, FALSE, FALSE, ON, MENUFOLD, "Refresh Folder List");
    appendmenu(&ma->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, MENUCHECK, "Check Sending");
    appendmenu(&ma->branch, mp);
    ami_menu(stdout, sm);

}

static void kickworker(void)

{

    if (!wrkstart) { ami_newthread(mailwork); wrkstart = TRUE; }
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
    int list  = wrklist;
    int done  = wrkdone && !wrkgo;

    srcpick(); /* a search finished, or how far one has got */
    wrkfolds = FALSE;
    wrklist = FALSE;
    if (folds) drawfolders();
    if (*wrkwhat) { /* what it is doing, and how far into it */

        char t[MAXSTR*2];
        char a[40], b[40];

        if (wrkmax > 0) {

            commas(wrkpos, a, sizeof(a));
            commas(wrkmax, b, sizeof(b));
            snprintf(t, sizeof(t), "%s - %s of %s", wrkwhat, a, b);

        } else copystr(t, wrkwhat, sizeof(t));
        status(t);
        statprog(wrkpos, wrkmax);

    }
    if (*sentsaid) { status(sentsaid); *sentsaid = 0; }
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
        if (wrkcount) status(KEYHELP); /* not blank: the keys come back */
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
        !sendwant && !srcwant && !srcbusy && !srcdone && !failwait &&
        !*sentsaid && !*wrkwhat) {

        ami_killtimer(stdout, TIMFETCH);
        timerrun = FALSE;

    }

}

static int mailquit;   /* somebody asked to leave */

static void openmsg(int i);
static void selectmsg(int i);
static void srvopen(void);
static void helpopen(void);
static void cmpopen(const char* to, const char* cc, const char* subject,
                    const char* body, const char* inreply, const char* refs);
static void fetchall(int relist);
static void showlist(void);
static int listvis(void);

/* The keys, one stream for the whole main window. A terminal has one
   keyboard and this program points it at the message list: the arrows
   move the selection, the page keys page it, Enter opens what is
   selected. It is called from every window of the main family, so it
   does not matter which of them the manager thinks has focus. */
static void listkeys(ami_evtrec* er)

{

    int vis = listvis();
    int sel = msgsel;

    /* The folder keys come first: they have to work when the list is
       empty, which is exactly when somebody wants to change folder. */
    if (er->etype == ami_ettab ||
        (er->etype == ami_etchar && (er->echar == 'f' || er->echar == 'F'))) {

        int i = foldsel;
        int n = 0;
        ami_long back = er->etype == ami_etchar && er->echar == 'F';

        /* the next folder that can be shown, wrapping, skipping the
           headings' worth of nothing -- a server folder that holds no
           messages is still a folder and is not skipped */
        while (n++ < foldct) {

            i = back? i-1: i+1;
            if (i >= foldct) i = 0;
            if (i < 0) i = foldct-1;
            if (!folders[i].noselect) { showfolder(i); break; }

        }

        return;

    }
    if (er->etype == ami_etchar) {

        /* One-key commands, which is how a terminal likes to be driven:
           everything the menu offers, a letter offers too. They are
           answered before the guard below -- composing a message,
           getting mail, asking for help and leaving all make sense with
           an empty list, and it is precisely when the list is empty
           that somebody wants them. */
        switch (er->echar) {

            case 'c': case 'C':
                if (!haveaccount()) srvopen();
                else cmpopen("", "", "", "", "", "");
                break;

            case 'g': case 'G':
                if (!haveaccount()) srvopen();
                else if (!fetching) fetchall(TRUE);
                break;

            case 's': case 'S': srvopen(); break;
            case '/': srcopen(); break;
            case 'h': case 'H': helpopen(); break;
            case 'q': case 'Q': mailquit = TRUE; break;
            default: break;

        }

        return;

    }
    /* what is left moves about in the list, and needs one to move in */
    if (foldsel < 0 || !msgct) return;
    switch (er->etype) {

        case ami_etup:   sel = sel < 0? msgtop: sel-1; break;
        case ami_etdown: sel = sel < 0? msgtop: sel+1; break;
        case ami_etpagu: sel = (sel < 0? msgtop: sel)-(vis-1); break;
        case ami_etpagd: sel = (sel < 0? msgtop: sel)+(vis-1); break;
        case ami_ethome: sel = 0; break;
        case ami_etend:  sel = msgct-1; break;
        case ami_etenter:
            if (msgsel >= 0) openmsg(msgsel);
            return;

        default: return;

    }
    if (sel < 0) sel = 0;
    if (sel > msgct-1) sel = msgct-1;
    /* scrolled into view, then marked */
    if (sel < msgtop) { msgtop = sel; showlist(); }
    else if (sel >= msgtop+vis) { msgtop = sel-vis+1; showlist(); }
    selectmsg(sel);

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
    chrh = 1; /* a row of text is a row */
    rowh = 1; /* and a message line is a row */
    { /* the store, if it is not there yet: makpth is an error if it is */

        char        path[MAXSTR-16];
        struct stat sb;

        ami_getusr(path, sizeof(path));
        snprintf(store, MAXSTR, "%s/.amimail", path);
        if (stat(store, &sb)) ami_makpth(store);

    }
    readaccount(); /* if there is one; the program comes up either way */
    migratestore(); /* mailboxes from before accounts had names */
    /* The root of a terminal is the terminal, but not every build of
       this program runs on one: linked against the graphical library
       the same character program gets a window, and a window has a
       size worth asking for. The request is clamped to what the screen
       says it has, which makes it safe under the manager too -- there
       the screen IS the terminal, and a request for no more than that
       is a request for what already is. */
    {

        ami_long sx, sy;

        ami_scnsiz(stdout, &sx, &sy);
        if (sx > 4 && sy > 4) {

            ami_long wantx = sx-4 < 120? sx-4: 120;
            ami_long wanty = sy-4 < 42? sy-4: 42;

            if (wantx > ami_maxx(stdout) || wanty > ami_maxy(stdout))
                ami_setsiz(stdout, wantx, wanty);

        }

    }    /* The menu is built once the window is its final size. Built before,
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
    ami_openwin(&stdin, &listwf, stdout, LISTWIN);
    ami_frame(listwf, FALSE);
    ami_auto(listwf, FALSE);
    ami_curvis(listwf, FALSE);
    /* The strip at the foot, and the bar in it. Its natural height is
       what the strip is built around; its width is set here, since a bar
       as wide as a window says less than a short one does. */
    /* the banner across the top: its own window, so that it is placed
       and drawn in one space and answers for its own redrawing */
    ami_openwin(&stdin, &banwf, stdout, BANWIN);
    ami_frame(banwf, FALSE);
    ami_auto(banwf, FALSE);
    ami_curvis(banwf, FALSE);
    /* The banner is a row for the name and a row of double line: there
       are no pictures in a terminal, and rows are too dear here for a
       band of them. What the drawing of a cat said in the graphical
       version, five characters of one say here. */
    banh = 2;
    /* the strip along the foot: one row, with the bar drawn in it as a
       run of blocks */
    progh = 1;
    progw = 20;
    stath = 1;
    ami_scrollvertsiz(listwf, &sbw, &wy);
    ami_scrollvert(listwf, 1, 1, sbw, 10, SBLIST); /* moved by layout */
    layout();
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
    else status(KEYHELP);
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
        if (diag && er.etype != ami_etmoumov && er.etype != ami_ettim &&
            er.etype != ami_etframe)
            fprintf(stderr, "event %d win %lld\n", er.etype, AMI_LONG_CAST(er.winid));
        if (diag) switch (er.etype) {

            case ami_etresize:
                fprintf(stderr, "resize win %lld: %lldx%lld\n", AMI_LONG_CAST(er.winid),
                        AMI_LONG_CAST(er.rszx), AMI_LONG_CAST(er.rszy));
                break;
            case ami_etredraw:
                fprintf(stderr, "redraw win %lld\n", AMI_LONG_CAST(er.winid));
                break;
            default: break;

        }
        /* Every event names the window it came from. The reader is a
           window of its own and the panes are windows of their own, so
           this one loop serves them all. */
        if (er.winid == HELPWIN) { helpevent(&er); continue; }
        if (er.winid == CMPWIN) { cmpevent(&er); continue; }
        if (er.winid == SRVWIN) { srvevent(&er); continue; }
        if (er.winid == SRCWIN) { srcevent(&er); continue; }
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
                case ami_etchar:
                    /* q and Escape both close it, which is what a
                       terminal reader is expected to answer to */
                    if (er.echar == 'q' || er.echar == 'Q') closeread();
                    else if (er.echar == 'r' || er.echar == 'R')
                        answer(MENUREPLY);
                    else if (er.echar == 'a' || er.echar == 'A')
                        answer(MENUREPALL);
                    else if (er.echar == 'w' || er.echar == 'W')
                        answer(MENUFWD);
                    break;
                default: break;

            }
            continue;

        }
        if (er.winid == BANWIN) {

            if (er.etype == ami_etredraw) drawbanner();
            else if (er.etype == ami_etresize)
                { ami_sizbuf(banwf, er.rszx, er.rszy); drawbanner(); }
            else listkeys(&er); /* focus may sit anywhere; keys are one */

            continue;

        }
        if (er.winid == FOLDWIN) {

            switch (er.etype) {

                case ami_etmoumov: mpx = er.moupx; mpy = er.moupy; break;
                case ami_etmouba: {

                    int best = -1;

                    if (er.amoubn != 1) break;
                    /* Which folder the click landed on, found from where
                       each was drawn rather than by counting rows. In
                       cells the row IS the folder: the pixel version's
                       two pixels of slop each way became two rows of
                       slop, and the last folder in a five-row band is
                       the one below the pointer -- the click that hit
                       Sent and selected INBOX. */
                    for (i = 0; i < foldct; i++)
                        if (mpy == foldy[i]) best = i;
                    if (diag) fprintf(stderr, "foldclick: mpx %d mpy %d "
                                      "best %d foldy0..2 %d %d %d\n",
                                      mpx, mpy, best, foldy[0], foldy[1],
                                      foldy[2]);
                    if (best >= 0) showfolder(best);
                    break;

                }
                case ami_etredraw: drawfolders(); break;
                case ami_etresize:
                    ami_sizbuf(foldwf, er.rszx, er.rszy);
                    drawfolders();
                    break;
                default: listkeys(&er); break;

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
                case ami_etmoumov: {

                    ami_long r = er.moupy-2; /* the frame, then a row each */

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
            if (popwf && er.etype == ami_etmouba) { popclose(); continue; }
            switch (er.etype) {

                case ami_etmoumov: mpx = er.moupx; mpy = er.moupy; break;
                case ami_etmouba:
                    if (er.amoubn == 4) {

                        msgtop -= WHEELMSGS;
                        showlist();

                    } else if (er.amoubn == 5) {

                        msgtop += WHEELMSGS;
                        showlist();

                    } else if (er.amoubn == 1) {

                        i = msgtop+(mpy-1); /* row 1 is message msgtop */
                        if (i >= 0 && i < msgct) { selectmsg(i); openmsg(i); }

                    } else if (er.amoubn == 2 || er.amoubn == 3) {

                        /* the second button: the message menu */
                        i = msgtop+(mpy-1); /* row 1 is message msgtop */
                        if (i >= 0 && i < msgct) {

                            selectmsg(i);
                            popopen(i, mpx, mpy);

                        }

                    }
                    break;
                /* the bar scrolls the view; the keys move the selection */
                case ami_etsclull: msgtop--; showlist(); break;
                case ami_etscldrl: msgtop++; showlist(); break;
                case ami_etsclulp: msgtop -= listvis()-1; showlist(); break;
                case ami_etscldrp: msgtop += listvis()-1; showlist(); break;
                case ami_etsclpos:
                    ami_scrollpos(listwf, SBLIST, er.sclpos);
                    msgtop = scaleback(er.sclpos, msgct-listvis());
                    fromdrag = TRUE;
                    showlist();
                    fromdrag = FALSE;
                    break;
                case ami_etredraw:
                    /* a character redraw names no rectangle: it is the
                       whole window or nothing */
                    drawlist();
                    break;

                case ami_etresize: {

                    /* The columns are measured from the right edge, so a
                       change of width moves all of them and the rows
                       have to be laid again. A change of height moves
                       nothing: the rows that come into view arrive as a
                       redraw of their own. */
                    static int prevw;

                    ami_sizbuf(listwf, er.rszx, er.rszy);
                    datex = er.rszx-sbw-(int)strlen("Sep 30, 2025 ");
                    listrows = er.rszy/rowh;
                    if (listrows < 1) listrows = 1;
                    if (er.rszx != prevw) drawlist();
                    prevw = er.rszx;
                    break;

                }
                default: listkeys(&er); break;

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
                ami_sizbuf(stdout, er.rszx, er.rszy);
                layout();
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
                        ami_maxy(stdout)-stath);
                drawstatus();
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
                    case MENUSEARCH: srcopen(); break;

                    case MENUCOMP:
                        if (!haveaccount()) { srvopen(); break; }
                        cmpopen("", "", "", "", "", "");
                        break;

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

            /* Keys arrive on the main window under the graphical build
               -- there is no manager handing focus among the panes --
               and on whichever pane the manager has focused under the
               character one. One stream either way: what the main
               window does not use, the list gets. */
            default: listkeys(&er); break;

        }

    /* Asked to leave is tested here and not in the body: every pane
       hands its events back with a continue, which jumps to this
       condition and past anything written after the switch.

       A terminate for the reader closed the reader, not the program. */
    } while (!mailquit &&
             (er.etype != ami_etterm || er.winid == READWIN ||
              er.winid == SRVWIN || er.winid == HELPWIN ||
              er.winid == CMPWIN || er.winid == SRCWIN));
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
    srvclose();
    srcclose();
    closeread();
    cmpclose();
    dunlock();

    return (0);

}
