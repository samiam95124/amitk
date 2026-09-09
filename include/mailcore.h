/*******************************************************************************

Mail: what both front ends share

The mail program is in two halves. mailcore.c is the program proper --
the store, the index, the digests, the accounts, the reading of
messages, the talking to servers, and the thread that does all of it --
and it draws nothing at all. A front end puts it on a screen: mail does
that in graphics, mailc in characters, and they differ in nothing else.

A front end must supply three things, since only it knows how to say
anything:

    status()    a line saying what is going on
    fail()      something went wrong and somebody should be told
    statprog()  how far into a piece of work we are

and it must keep one rule: hold the lock, dlock() to dunlock(), for as
long as it is handling an event. The core takes the same lock around
each message it commits and around anything that rebuilds the folder
table, and takes it nowhere else. That is what makes it safe for the
fetch thread to be writing the store while the display is drawing it,
and it is the whole of the arrangement -- there is no other locking to
think about, and nothing in a front end should ever take the lock again.

*******************************************************************************/

#ifndef __MAILCORE_H__
#define __MAILCORE_H__
#include <localdefs.h>

#include <stdio.h>

#define MAXSTR    500   /* the usual string */
#define MAXLINE   4000  /* longest protocol or message line taken whole */
#define MAXFOLDER 200   /* folders on the server */
#define MAXMSG    20000 /* messages indexed in one folder */
#define SNIPPET   400   /* characters of the message kept for the list */
#define DEFLIMIT  200   /* messages fetched from a folder, most recent first */
#define MAXSRV    8     /* accounts */
#define DEFPOLL   15    /* seconds between looks at the servers */
#define NETWAIT   45    /* seconds to wait on a server before giving up */
#define DIGLEN    65    /* a sha-256 as hex, and its terminator */

/* One message, as the list shows it and as the store finds it. The
   digest says what the message is wherever it came from; the offset and
   length say where it is in its mailbox. */
typedef struct {

    ami_long off;             /* where the message starts in the file */
    ami_long len;             /* how long it is */
    char from[MAXSTR];    /* who it is from, shown */
    char subject[MAXSTR]; /* the subject line */
    char addr[100];       /* the sender's address, for matching */
    char dig[DIGLEN];     /* what the message is, wherever it came from */
    char cat[32];         /* what kind of mail it is */
    char snip[SNIPPET];   /* the start of the message */
    char when[40];        /* the date, shown the way mail readers show it */
    ami_long date;            /* the date, for sorting */

} msgrec;

/* One folder: a mailbox in the store, the index beside it, and which
   account it belongs to. A local folder belongs to none. */
typedef struct {

    char name[MAXSTR];   /* the name the server knows it by */
    char show[MAXSTR];   /* the name shown, without the [Gmail]/ part */
    char file[MAXSTR*2]; /* the mbox file it is kept in */
    ami_long msgs;           /* messages in the file */
    ami_long dirty;          /* something has been put in it since it was read */
    msgrec* idx;         /* every message in it, read from the index file */
    ami_long idxct;          /* how many */
    ami_long idxmax;         /* and how many the array has room for */
    ami_long idxok;          /* the index has been read and is good */
    ami_long wantidx;        /* it wants reading, when the worker gets to it */
    ami_long noselect;       /* the server says it holds no messages */
    ami_long local;          /* ours alone: a folder with no server side */
    ami_long srv;            /* which server it belongs to, -1 if local */

} foldrec;

/* One account. */
typedef struct {

    char name[64];      /* what it is called here */
    char imap[MAXSTR];  /* where the mail is read from */
    ami_long imapport;
    char smtp[MAXSTR];  /* and where it would be sent from */
    ami_long smtpport;
    char user[MAXSTR];
    char pass[MAXSTR];
    ami_long limit;         /* messages fetched from each folder */

} srvrec;

/*******************************************************************************

What the front end supplies

*******************************************************************************/

void status(const char* s);
void fail(const char* what);
void statprog(ami_long pos, ami_long max);

/*******************************************************************************

What the core keeps

*******************************************************************************/

extern foldrec folders[MAXFOLDER];
extern ami_long    foldct;
extern ami_long    foldsel;    /* the folder being shown */
extern msgrec* msgs;       /* the messages of that folder */
extern ami_long    msgct;
extern ami_long    msgsel;     /* the message being read */
extern ami_long    msgtop;     /* the first message shown */
extern srvrec  servers[MAXSRV];
extern ami_long    srvct;      /* how many accounts there are */
extern ami_long    pollsec;    /* how often to look, in seconds */
extern ami_long    sendsrv;    /* which account mail is sent from */
extern char    store[MAXSTR];
extern ami_long    limit;
extern ami_long    diag;       /* report the conversation to stderr */
extern char*   mailprog;   /* argv[0], to find what is kept beside it */

/* the message waiting to go out, which the compose window fills in */
extern ami_long    sendwant;
extern char    outto[MAXSTR];
extern char    outcc[MAXSTR];
extern char    outsub[MAXSTR];
extern char*   outbody;
extern char    outinreply[MAXSTR];
extern char    outrefs[MAXSTR*2];

/* the connection the fetch is using */
extern char    imapsrv[MAXSTR];
extern ami_long    imapport;
extern char    smtpsrv[MAXSTR];
extern ami_long    smtpport;
extern char    username[MAXSTR];
extern char    password[MAXSTR];

/*******************************************************************************

What the worker is doing, for the front end to draw

Nothing here is drawn by the worker -- it draws nothing. It is left, and
picked up on the front end's next tick.

*******************************************************************************/

extern char wrkwhat[MAXSTR]; /* what is being worked on */
extern ami_long wrkpos;          /* how far into it */
extern ami_long wrkmax;          /* and how big it is */
extern ami_long wrkfolds;        /* the folder pane wants redrawing */
extern ami_long wrkcounts;       /* a folder's count changed: its line wants redrawing */
extern ami_long wrklist;         /* and so does the message list */
extern ami_long wrkstop;         /* drop what you are doing */
extern ami_long wrkbusy;         /* it has something in hand just now */
extern ami_long wrkgo;           /* a fetch is running */
extern ami_long wrkdone;         /* it finished, and nobody has noticed yet */
extern ami_long wrkrelist;       /* this fetch is to ask what folders there are */
extern ami_long wrkcount;        /* and this one is only to read the store */
extern ami_long wrkstart;        /* the thread has been made */
extern ami_long srcstart;        /* and the search thread */
extern ami_long fetching;        /* a fetch is under way */
extern ami_long timerrun;        /* the timer that watches it is going */
extern ami_long idxwant;         /* the folder the display wants read */
extern ami_long idxfold;         /* the folder its list was read from */
extern ami_long idxdoing;        /* and the one being read just now */
extern ami_long fetchgot;        /* messages taken, this fetch */
extern ami_long fetchdup;        /* and passed over as already here */
extern ami_long digct;           /* digests known over the whole store */
extern char failsaid[MAXSTR*3]; /* what went wrong, for the front end to show */
extern ami_long failwait;
extern ami_long sendfail;        /* and whether it was a send that failed */
extern char sentsaid[MAXSTR]; /* and what went right */

/*******************************************************************************

The search

What the search form asks for, and what the worker found. The front end
fills the ask and sets srcwant; the search thread takes it, reads whatever
folders it has to, and leaves the messages found, copies of their
records with the folder each is in, under srcdone. The cheap tests, the
sender, the subject, the size and the date, are made on the index; the
others, the To line, the words and the attachment, read the message.

*******************************************************************************/

typedef struct {

    char     from[MAXSTR];    /* the sender holds this */
    char     to[MAXSTR];      /* the To line holds this */
    char     subject[MAXSTR]; /* the subject holds this */
    char     words[MAXSTR];   /* every one of these words is in it */
    char     nowords[MAXSTR]; /* and none of these */
    ami_long sizeop;          /* 0 any size, 1 greater than, 2 less than */
    ami_long sizeval;         /* that many bytes */
    ami_long within;          /* dated within this many seconds, 0 any date */
    ami_long date;            /* of this time */
    ami_long fold;            /* the folder to search, -1 for all of them */
    ami_long attach;          /* it has an attachment */

} srcrec;

extern srcrec    srcask;     /* what is being searched for */
extern ami_long  srcwant;    /* a search is asked for */
extern ami_long  srcbusy;    /* and being made */
extern ami_long  srcdone;    /* and finished, for the front end to show */
extern msgrec*   srcres;     /* the messages found, copies of their records */
extern ami_long* srcfold;    /* and the folder each is in */
extern ami_long  srcct;      /* how many were found */
extern char      srcwhat[];  /* what the search is doing, for the strip */
extern ami_long  srcpos;     /* how far into it */
extern ami_long  srcmax;
extern char      srcmissed[]; /* the folders it could not look in, not yet
                                 indexed; empty when it looked in them all */

/*******************************************************************************

What the front end can ask for

*******************************************************************************/

extern ami_long datlock;         /* made by the front end at startup, with
                                ami_initlock(), before the thread exists */
void  dlock(void);           /* held for the whole of an event */
void  dunlock(void);
void  mailwork(void);        /* the fetch thread's whole life */
void  storeall(void);        /* the folders the store already knows of */
void  countfolders(void);    /* their counts, from their indexes */
void  readaccount(void);
void  writeaccount(void);
int   haveaccount(void);
void  useserver(ami_long i);
void  blankserver(srvrec* r);
void  migratestore(void);
void  renamestore(const char* was, const char* now);
void  indexfolder(ami_long fold);
void  idxdrop(ami_long fold);
void  idxsetaside(void);
void  idxgiveback(void);
void  useidx(void);          /* point the list at the selected folder */
ami_long  localfolder(const char* who);
ami_long  movelocal(ami_long fold, const char* dst, const char* set);
char* getmsg(ami_long fold, ami_long i);
void  smtpcheck(void);
void  sendmail(const char* to, const char* cc, const char* subject,
               const char* body, const char* inreply, const char* refs,
               char* err, ami_long errl);
void  servesend(void);
void  serveindex(void);
void  servesearch(void);
void  searchwork(void);       /* the search thread's whole life */
char* getmsgin(ami_long fold, const msgrec* m);
int   hasattach(const char* msg, ami_long len);
ami_long parseday(const char* s);
void whenof(ami_long t, char* show, ami_long sn);
void  storefolders(ami_long srv);
int   getfolders(ami_long srv);
void  imapclose(void);
void  fetchorder(void);
void  fetchend(void);
void  fetchstep(void);
void  fetchrun(void);
int   serverquiet(ami_long srv);

/* the pieces a message is made of */
int   findheader(const char* msg, const char* name, char* d, ami_long dl);
char* textof(const char* msg, ami_long len, ami_long want);
void  classify(const char* msg, char* cat, ami_long cl);
void  addrof(const char* s, char* d, ami_long dl);
void  nameof(const char* s, char* d, ami_long dl);
ami_long  parsedate(const char* s, char* show, ami_long sn);
void  trim(char* s);
void  copystr(char* d, const char* s, ami_long dl);
void* getmem(ami_long n);
int   reachable(const char* host, ami_long port, ami_long secs);
int   resfile(const char* leaf, char* path, ami_long pl);
const char* nextaddr(const char* p, char* d, ami_long dl);

#endif
