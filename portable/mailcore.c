/*******************************************************************************

Mail: the part that does not draw

The mail program proper -- the store, the index, the digests, the
accounts, the reading of messages, the talking to servers, and the
thread that does all of it. None of it draws anything.

Two front ends put it on a screen: mail, which is graphical, and mailc,
which is characters. They share this file and differ in nothing else.
What each must supply, and the one rule they must both keep, is written
at the top of mailcore.h.

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
#include <network.h>
#include <services.h>

#include <mailcore.h>
ami_long imaptag;   /* the number the next command is tagged with */
FILE* imap;

/* the store's directory names, defined with the store code below */
void srvdir(ami_long srv, char* dn, ami_long dnl);
void safename(const char* nm, char* fn, ami_long fnl);

/* What both front ends read. The types and the declarations are in
   mailcore.h; this is where they live. */
foldrec folders[MAXFOLDER];
ami_long    foldct;
ami_long    foldsel = -1;    /* the folder being shown */
msgrec* msgs;            /* the messages of that folder */
ami_long    msgct;
ami_long    msgsel = -1;     /* the message being read */
ami_long    msgtop;          /* the first message shown */
srvrec servers[MAXSRV];
ami_long   srvct;    /* how many there are */
ami_long   pollsec = DEFPOLL; /* how often to look, in seconds */
ami_long   sendsrv;
ami_long   sendwant;
char   outto[MAXSTR];
char   outcc[MAXSTR];
char   outsub[MAXSTR];
char*  outbody;
char   outinreply[MAXSTR];
char   outrefs[MAXSTR*2];
char imapsrv[MAXSTR] = "imap.gmail.com";
ami_long imapport = 993;
char smtpsrv[MAXSTR] = "smtp.gmail.com";
ami_long smtpport = 465;
char username[MAXSTR];
char password[MAXSTR];
char store[MAXSTR];      /* the directory the mail is kept in */
ami_long limit = DEFLIMIT;   /* messages fetched from a folder */
ami_long diag;               /* report the conversation to stderr */
char* mailprog;          /* argv[0], to find the rules by */
char*     mailprog;    /* argv[0], to find the help file by */

void* getmem(ami_long n)

{

    void* p = malloc(n);

    if (!p) { fail("Out of memory"); exit(1); }

    return (p);

}

/* trim the blanks and the line ending off both ends of a string */
void trim(char* s)

{

    char* p = s;
    ami_long  n;

    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p)+1);
    n = strlen(s);
    while (n && (s[n-1] == '\r' || s[n-1] == '\n' ||
                 s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;

}

/* copy with a limit, always terminated */
void copystr(char* d, const char* s, ami_long n)

{

    strncpy(d, s, n-1);
    d[n-1] = 0;

}

/*******************************************************************************

Digests

Every message gets a SHA-256 of the bytes it is stored as. It is what
lets this program know a message it already has, whoever sends it and
whatever folder it arrives in: mail moved between folders keeps its
digest, mail fetched twice has the same digest, and mail from a second
server that is the same mail has the same digest. UIDs cannot do any of
that -- they belong to one folder on one server and mean nothing
anywhere else.

The digest covers the message and not the separator line above it, since
that line is this program's own and carries a time that is not the
message's.

The digests of a folder are kept in a file beside its mailbox, one to a
line, with the size the mailbox had when they were taken on the first
line. A mailbox that has grown since is read again and the file rebuilt,
so the two cannot drift apart without being noticed.

*******************************************************************************/

static void digestbytes(const char* data, ami_long len, char* hex); /* forward */

/* The SHA-256 of a message, taken over one canonical form of it so that
   the same message gives the same digest wherever it is seen. What
   arrives from the server and what is written to the mailbox are not the
   same bytes: storing escapes any line that begins "From " with a >, so
   that it cannot be mistaken for a separator, and drops the newlines
   that trail the message. Undo both before taking the digest, and the
   message as received and the message as stored agree -- which is the
   whole point of having one. */
static void digestof(const char* data, ami_long len, char* hex)

{

    char* norm = malloc(len+1);
    ami_long  i, o = 0;
    int   atbol = TRUE;

    if (!norm) { fail("Out of memory"); exit(1); }
    for (i = 0; i < len; i++) {

        if (atbol && data[i] == '>') { /* a line the storing escaped? */

            ami_long j = i;

            while (j < len && data[j] == '>') j++;
            if (j+5 <= len && !strncmp(data+j, "From ", 5)) i++; /* one off */

        }
        norm[o++] = data[i];
        atbol = data[i] == '\n';

    }
    while (o && (norm[o-1] == '\n' || norm[o-1] == '\r')) o--;
    digestbytes(norm, o, hex);
    free(norm);

}

/* the SHA-256 of a block of bytes, as hex */
static void digestbytes(const char* data, ami_long len, char* hex)

{

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int  n = 0;
    EVP_MD_CTX*   c = EVP_MD_CTX_new();
    ami_long      i;

    if (!c) { fail("Out of memory"); exit(1); }
    EVP_DigestInit_ex(c, EVP_sha256(), NULL);
    EVP_DigestUpdate(c, data, len);
    EVP_DigestFinal_ex(c, md, &n);
    EVP_MD_CTX_free(c);
    for (i = 0; i < (ami_long)n; i++) sprintf(hex+i*2, "%02x", md[i]);
    hex[n*2] = 0;

}

/* The digests this store holds, in a table of their own. Kept as a plain
   open hash: the whole point is that asking whether a message is already
   here costs nothing, so a search of every folder will not do. */
#define DIGBKT 4096

typedef struct digent {

    ami_long       fold; /* the folder holding the message */
    ami_long       rec;  /* and which of its messages it is */
    struct digent* next;

} digent;

static digent* digtab[DIGBKT];
ami_long    digct;

static ami_long dighash(const char* d)

{

    ami_long h = 0;
    ami_long i;

    /* the digest is already spread evenly, so any few of its characters
       make as good a bucket as all of them */
    for (i = 0; i < 8; i++) h = h*31+d[i];

    return ((h & 0x7fffffff) % DIGBKT);

}

/* What a digest names: a message, in a folder, at a place in its file.
   The table holds no digests of its own -- it holds where to find them,
   so that knowing a message is here also says where here is. That is
   what makes it worth anything to an audit: not "something with this
   digest was seen once" but "this message is in that file at that
   offset". */
static const char* digof(const digent* p)

{

    return (folders[p->fold].idx[p->rec].dig);

}

/* the message this digest names, or none */
static int finddigest(const char* d, ami_long* fold, ami_long* rec)

{

    digent* p;

    for (p = digtab[dighash(d)]; p; p = p->next)
        if (!strcmp(digof(p), d)) {

            if (fold) *fold = p->fold;
            if (rec) *rec = p->rec;

            return (TRUE);

        }

    return (FALSE);

}

static int hasdigest(const char* d)

{

    return (finddigest(d, NULL, NULL));

}

static void adddigest(ami_long fold, ami_long rec)

{

    digent* p;
    ami_long    b;
    const char* d = folders[fold].idx[rec].dig;

    if (hasdigest(d)) return;
    b = dighash(d);
    p = getmem(sizeof(digent));
    p->fold = fold;
    p->rec = rec;
    p->next = digtab[b];
    digtab[b] = p;
    digct++;

}

/* Take the table again over every folder's index. Sorting a folder moves
   its messages about and the table names them by where they sit, so it
   is taken again whenever an index is rebuilt rather than added to. */
static void rehashall(void)

{

    ami_long i, j;

    for (i = 0; i < DIGBKT; i++) {

        digent* p = digtab[i];

        while (p) { digent* n = p->next; free(p); p = n; }
        digtab[i] = NULL;

    }
    digct = 0;
    for (i = 0; i < foldct; i++)
        for (j = 0; j < folders[i].idxct; j++) adddigest(i, j);

}

/* the file a folder's digests are kept in */
/*******************************************************************************

The index file

One beside each mailbox, holding a line for every message in it: where
it is in the file and how long it is, what it is (its digest), who it is
from, what it is about, when it came and the start of what it says --
everything the list shows and everything the store is searched by.

It is written as it is filled, a line at a time, and read back at
startup. Reading it costs a few megabytes; working it out again from the
mailbox costs reading and parsing every byte of mail there is, which on
this store is five gigabytes and half a minute of somebody's attention.

The file needs no stamp saying how far it goes. The messages it names
say that themselves: the furthest of them ends where the indexed part of
the mailbox ends, and anything after that in the mailbox is new. A
mailbox that is shorter than that, or that does not have a separator
where the index says a message begins, has been rewritten under the
index, and the index is thrown away and taken again.

Fields are separated by tabs. Anything that could hold a tab or a line
end -- a subject, a snippet -- is written with those escaped, since a
record is a line and has to stay one.

*******************************************************************************/

/* The line every index file begins with. The number is the version of
   what follows, and moving it throws away every index written before:
   version 1 held dates read without the sender's timezone, so mail from
   a server keeping UTC sorted hours into the future. An index that is
   wrong is worse than none, since nothing would ever go back and look
   at the mailbox again. */
#define IDXHEAD "ami-mail-index 2"

static void idxfile(ami_long fold, char* fn, ami_long fnl)

{

    snprintf(fn, fnl, "%s.idx", folders[fold].file);

}

static void digfile(ami_long fold, char* fn, ami_long fnl)

{

    snprintf(fn, fnl, "%s.dig", folders[fold].file);

}

/* a field, with what would break the line taken out of it */
static void idxput(FILE* f, const char* s)

{

    fputc('\t', f);
    while (*s) {

        switch (*s) {

            case '\\': fputs("\\\\", f); break;
            case '\t': fputs("\\t", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': break;
            default:   fputc(*s, f); break;

        }
        s++;

    }

}

/* and back again: the next field of the line, up to the tab */
static char* idxget(char* p, char* d, ami_long dl)

{

    ami_long i = 0;

    if (!p) { if (dl) *d = 0; return (NULL); }
    if (*p == '\t') p++;
    while (*p && *p != '\t') {

        char c = *p++;

        if (c == '\\' && *p) {

            c = *p++;
            if (c == 't') c = '\t';
            else if (c == 'n') c = '\n';

        }
        if (i < dl-1) d[i++] = c;

    }
    if (dl) d[i] = 0;

    return (p);

}

static void idxwrite(FILE* f, const msgrec* m)

{

    fprintf(f, "%lld\t%lld\t%lld", AMI_LONG_CAST(m->off), AMI_LONG_CAST(m->len), AMI_LONG_CAST(m->date));
    idxput(f, m->dig);
    idxput(f, m->cat);
    idxput(f, m->when);
    idxput(f, m->addr);
    idxput(f, m->from);
    idxput(f, m->subject);
    idxput(f, m->snip);
    fputc('\n', f);

}

static int idxread(char* line, msgrec* m)

{

    char* p = line;
    char  num[40];

    p = idxget(p, num, sizeof(num)); m->off = atol(num);
    p = idxget(p, num, sizeof(num)); m->len = atol(num);
    p = idxget(p, num, sizeof(num)); m->date = atol(num);
    p = idxget(p, m->dig, sizeof(m->dig));
    p = idxget(p, m->cat, sizeof(m->cat));
    p = idxget(p, m->when, sizeof(m->when));
    p = idxget(p, m->addr, sizeof(m->addr));
    p = idxget(p, m->from, sizeof(m->from));
    p = idxget(p, m->subject, sizeof(m->subject));
    p = idxget(p, m->snip, sizeof(m->snip));

    return (p && m->len > 0 && strlen(m->dig) == DIGLEN-1);

}

/* Room for one more in the index being built.

   A folder is read into here and put in place in one move, and never
   added to where it stands. It was added to where it stood, and that is
   what crashed the program: growing an array moves it, the display's
   list IS the array, and a display drawing the folder that was being
   read went on drawing the block that had just been freed underneath
   it. Reading a folder while looking at it is the ordinary case, not a
   corner of one.

   The fetch is different and adds in place, but the fetch does it
   holding the lock and points the display at the array again
   afterwards, which is the same protection by another road. */
static msgrec* bidx;
static ami_long    bct;
static ami_long    bmax;

static msgrec* bldroom(void)

{

    if (bct >= bmax) {

        bmax = bmax? bmax*2: 256;
        bidx = realloc(bidx, bmax*sizeof(msgrec));
        if (!bidx) { fail("Out of memory"); exit(1); }

    }

    return (&bidx[bct++]);

}

/* room for one more in a folder's index */
static msgrec* idxroom(ami_long fold)

{

    foldrec* fo = &folders[fold];

    if (fo->idxct >= fo->idxmax) {

        fo->idxmax = fo->idxmax? fo->idxmax*2: 256;
        fo->idx = realloc(fo->idx, fo->idxmax*sizeof(msgrec));
        if (!fo->idx) { fail("Out of memory"); exit(1); }

    }

    return (&fo->idx[fo->idxct++]);

}

/* Where the mailbox has been indexed up to: the end of the message that
   reaches furthest into it. The file is written in the order messages
   arrive but sorted in memory, so it is the highest end and not the
   last line. */
static ami_long idxend(void)

{

    ami_long i;
    ami_long e = 0;

    for (i = 0; i < bct; i++) {

        ami_long x = bidx[i].off+bidx[i].len;

        if (x > e) e = x;

    }

    return (e);

}

/*******************************************************************************

Reading a folder

An index file is read if there is one and it still fits the mailbox, and
whatever the mailbox holds beyond what the index names is read from the
mailbox itself and added. So a folder that has not changed costs reading
a few hundred kilobytes; one that has had a hundred messages put in it
costs those hundred; and only a folder with no index at all, or one
whose mailbox has been rewritten under it, costs reading the whole
thing.

*******************************************************************************/

/* Does the index still describe this mailbox? The messages it names say
   how far it goes; the mailbox must be at least that long, and must
   have a separator line where the index says the last message begins.
   Anything else means the mailbox has been rewritten -- by a move, by a
   hand, by another program -- and the index is not to be trusted. */
static int idxfits(ami_long fold)

{

    struct stat sb;
    ami_long    e;
    ami_long    off = -1;
    ami_long    i;
    FILE*       f;
    char        buf[300];
    ami_long    back, n;
    char*       ln;

    if (stat(folders[fold].file, &sb)) return (FALSE); /* no mailbox */
    e = idxend();
    if (!e) return (bct == 0); /* nothing named, nothing to fit */
    if (e > (ami_long)sb.st_size) return (FALSE);  /* the mailbox lost bytes */
    for (i = 0; i < bct; i++)
        if (bidx[i].off+bidx[i].len == e) { off = bidx[i].off; break; }
    if (off <= 0) return (FALSE);
    f = fopen(folders[fold].file, "r");
    if (!f) return (FALSE);
    back = off > (ami_long)sizeof(buf)-1? (ami_long)sizeof(buf)-1: off;
    fseek(f, off-back, SEEK_SET);
    n = fread(buf, 1, back, f);
    fclose(f);
    if (n <= 0) return (FALSE);
    buf[n] = 0;
    /* the separator is the line that ends where the message begins */
    ln = buf+n;
    if (ln > buf && ln[-1] == '\n') ln--;
    while (ln > buf && ln[-1] != '\n') ln--;

    return (!strncmp(ln, "From ", 5));

}

/* read the index file, if there is one */
static void idxload(ami_long fold)

{

    char  fn[MAXSTR*2+8];
    FILE* f;
    char* line;
    ami_long  lsz = MAXSTR*4+SNIPPET*2+400;

    bct = 0;
    folders[fold].idxok = FALSE;
    idxfile(fold, fn, sizeof(fn));
    f = fopen(fn, "r");
    if (!f) return;
    line = getmem(lsz);
    if (!fgets(line, lsz, f) || strncmp(line, IDXHEAD, strlen(IDXHEAD)))
        { free(line); fclose(f); return; } /* not one of ours */
    while (fgets(line, lsz, f)) {

        msgrec m;

        trim(line);
        if (!*line) continue;
        if (!idxread(line, &m)) continue; /* a line that says nothing */
        *bldroom() = m;

    }
    free(line);
    fclose(f);
    folders[fold].idxok = idxfits(fold);
    if (!folders[fold].idxok) bct = 0; /* it will be taken again */

}

/* write the whole of it */
static void idxsave(ami_long fold)

{

    char  fn[MAXSTR*2+8];
    FILE* f;
    ami_long  i;

    idxfile(fold, fn, sizeof(fn));
    f = fopen(fn, "w");
    if (!f) return; /* an index that cannot be kept is worked out again */
    fprintf(f, "%s\n", IDXHEAD);
    for (i = 0; i < bct; i++) idxwrite(f, &bidx[i]);
    fclose(f);
    /* the digests file it replaces, which held one of these ten fields */
    digfile(fold, fn, sizeof(fn));
    remove(fn);

}

/* Throw an index away, file and all. The mailbox under it has been
   rewritten -- by a move, or by a hand -- so every offset in it is
   wrong. What is in it cannot be patched: a message moved is the same
   message, but it is not in the same place. */
void idxdrop(ami_long fold)

{

    char fn[MAXSTR*2+8];

    idxfile(fold, fn, sizeof(fn));
    remove(fn);
    folders[fold].idxct = 0;
    folders[fold].idxok = FALSE;
    folders[fold].wantidx = TRUE;

}

/* and add to it as messages arrive */
static void idxappend(ami_long fold, const msgrec* m)

{

    char  fn[MAXSTR*2+8];
    FILE* f;
    int   made;

    idxfile(fold, fn, sizeof(fn));
    made = access(fn, F_OK) != 0;
    f = fopen(fn, "a");
    if (!f) return;
    if (made) fprintf(f, "%s\n", IDXHEAD);
    idxwrite(f, m);
    fclose(f);

}

/* Move a flat store into directories. Everything used to live in one
   place, named for its account or with local_ in front of it; each
   account has a directory of its own now, and what is ours has one
   called local. Done once, on startup, so that nothing has to be
   fetched again. */
void migratestore(void)

{

    ami_filptr lp = NULL;
    ami_filptr fp;
    char       what[MAXSTR*2];

    snprintf(what, sizeof(what), "%.400s/*.mbox", store);
    ami_list(what, &lp);
    for (fp = lp; fp; fp = fp->next) {

        char nm[MAXSTR];
        char dn[MAXSTR];
        char old[MAXSTR*2], new[MAXSTR*2];
        ami_long n, i;
        ami_long srv = -2;
        const char* leaf = NULL;
        static const char* ext[] = { "", ".state", ".dig", ".idx" };
        ami_long e;

        copystr(nm, fp->name, sizeof(nm));
        n = strlen(nm);
        if (n > 5 && !strcmp(nm+n-5, ".mbox")) nm[n-5] = 0; else continue;
        if (!strncmp(nm, "local_", 6)) { srv = -1; leaf = nm+6; }
        else for (i = 0; i < srvct; i++) { /* named for an account? */

            ami_long k = strlen(servers[i].name);

            if (!strncmp(nm, servers[i].name, k) && nm[k] == '_')
                { srv = i; leaf = nm+k+1; break; }

        }
        /* and a store older still, from before accounts had names at
           all: it belongs to the first of them */
        if (srv == -2 && srvct) { srv = 0; leaf = nm; }
        if (srv == -2) continue;
        srvdir(srv, dn, sizeof(dn));
        { /* the shown name, if the state file says what it really is */

            char  sn[MAXSTR*2+8];
            char  real[MAXSTR];
            char  lf[MAXSTR];
            ami_long  v, u, u2;
            FILE* sf;

            copystr(lf, leaf, sizeof(lf));
            /* [Gmail]/All Mail was written _Gmail__All_Mail when the
               awkward characters were taken out of it. Inside the
               account's own directory that front half says nothing, and
               a store whose state files have been lost has nothing else
               to go on. */
            if (!strncmp(lf, "_Gmail__", 8))
                memmove(lf, lf+8, strlen(lf+8)+1);
            snprintf(sn, sizeof(sn), "%.400s/%.150s.mbox.state", store, nm);
            sf = fopen(sn, "r");
            if (sf) {

                if (fscanf(sf, "%lld %lld %lld %499[^\n]", (long long*)(&v), (long long*)(&u), (long long*)(&u2),
                           real) == 4 ||
                    (rewind(sf), fscanf(sf, "%lld %lld %499[^\n]",
                                        (long long*)(&v), (long long*)(&u), real) == 3)) {

                    trim(real);
                    if (!strncmp(real, "[Gmail]/", 8))
                        safename(real+8, lf, sizeof(lf));
                    else safename(real, lf, sizeof(lf));

                }
                fclose(sf);

            }
            for (e = 0; e < 4; e++) {

                snprintf(old, sizeof(old), "%.400s/%.150s.mbox%.8s",
                         store, nm, ext[e]);
                snprintf(new, sizeof(new), "%.400s/%.150s.mbox%.8s",
                         dn, lf, ext[e]);
                rename(old, new); /* what is not there does not move */

            }

        }

    }

}

/* Renaming an account renames its directory, so that everything it
   holds goes with it rather than being fetched again. */
void renamestore(const char* was, const char* now)

{

    char        old[MAXSTR*2], new[MAXSTR*2];
    struct stat sb;

    if (!*was || !strcmp(was, now)) return;
    snprintf(old, sizeof(old), "%.400s/%.60s", store, was);
    snprintf(new, sizeof(new), "%.400s/%.60s", store, now);
    if (stat(old, &sb)) return; /* nothing there to move */
    if (!rename(old, new)) return; /* the whole directory went */
    /* It would not: renaming a directory onto one that already exists
       fails, and the new name's directory may well have been made
       already by anything that asked where the account's mail goes. So
       move what is in it, one file at a time, and take the empty
       directory away after. Failing quietly here is what left an
       account renamed with its mail still under the old name and every
       folder reading (not fetched). */
    {

        ami_filptr lp = NULL;
        ami_filptr fp;
        char       what[MAXSTR*2];

        snprintf(what, sizeof(what), "%.400s/*", old);
        ami_list(what, &lp);
        for (fp = lp; fp; fp = fp->next) {

            char a[MAXSTR*2], b[MAXSTR*2];

            snprintf(a, sizeof(a), "%.400s/%.150s", old, fp->name);
            snprintf(b, sizeof(b), "%.400s/%.150s", new, fp->name);
            rename(a, b);

        }
        rmdir(old); /* which does nothing unless it is empty */

    }

}

/* every folder in the store, so that a fetch knows what is already here */

/*******************************************************************************

The account

The account is the program's own business and is kept in the program's
own file, in the mail store, written only by the form under Mail/Server.
It is not on the command line, since a password on a command line is a
password in everybody's process list, and it is not in the Petit-Ami
configuration, which is a general facility shared by everything and no
place for one program's password.

The file is written readable by its owner and nobody else, and it is one
setting to a line, so it can be read and corrected with any editor.

*******************************************************************************/

/* where the account is kept */
static void acctfile(char* fn, ami_long fnl)

{

    snprintf(fn, fnl, "%s/account", store);

}

/* Make the one being talked to be this one. The protocol routines work
   from these, so setting them is how a server is chosen. */
void useserver(ami_long i)

{

    if (i < 0 || i >= srvct) return;
    copystr(imapsrv, servers[i].imap, MAXSTR);
    imapport = servers[i].imapport;
    copystr(smtpsrv, servers[i].smtp, MAXSTR);
    smtpport = servers[i].smtpport;
    copystr(username, servers[i].user, MAXSTR);
    copystr(password, servers[i].pass, MAXSTR);
    limit = servers[i].limit;

}

/* an account with nothing in it, ready to be filled in */
void blankserver(srvrec* r)

{

    memset(r, 0, sizeof(srvrec));
    copystr(r->name, "new", sizeof(r->name));
    copystr(r->imap, "imap.gmail.com", MAXSTR);
    r->imapport = 993;
    copystr(r->smtp, "smtp.gmail.com", MAXSTR);
    r->smtpport = 465;
    r->limit = DEFLIMIT;

}

/* Read the accounts. Each is a block: "server <name>" to "end", and
   anything outside a block is a setting of the program's own. The old
   one account form, which had no blocks at all, is still read: its
   settings arrive before any server line and make up the first one. */
void readaccount(void)

{

    char    fn[MAXSTR*2];
    FILE*   f;
    char    line[MAXSTR*2];
    srvrec* r = NULL;
    int     old = FALSE;

    acctfile(fn, sizeof(fn));
    f = fopen(fn, "r");
    if (!f) return; /* there is none yet, which is not an error */
    while (fgets(line, sizeof(line), f)) {

        char* p = line;
        char* v;

        trim(p);
        if (!*p || *p == '#') continue;
        v = p;
        while (*v && *v != ' ' && *v != '\t') v++;
        if (*v) *v++ = 0;
        while (*v == ' ' || *v == '\t') v++;
        if (!strcasecmp(p, "server")) { /* a new account begins */

            if (srvct >= MAXSRV) break;
            r = &servers[srvct++];
            blankserver(r);
            copystr(r->name, v, sizeof(r->name));
            old = FALSE;

            continue;

        }
        if (!strcasecmp(p, "end")) { r = NULL; continue; }
        if (!strcasecmp(p, "poll")) { pollsec = atol(v); continue; }
        if (!strcasecmp(p, "sendfrom")) { sendsrv = atol(v); continue; }
        if (!strcasecmp(p, "threaded")) { threaded = atol(v); continue; }
        if (!r) { /* the old form: settings before any server line */

            if (srvct >= MAXSRV) break;
            r = &servers[srvct++];
            blankserver(r);
            copystr(r->name, "mail", sizeof(r->name));
            old = TRUE;

        }
        if (!strcasecmp(p, "imap")) copystr(r->imap, v, MAXSTR);
        else if (!strcasecmp(p, "imapport")) r->imapport = atol(v);
        else if (!strcasecmp(p, "smtp")) copystr(r->smtp, v, MAXSTR);
        else if (!strcasecmp(p, "smtpport")) r->smtpport = atol(v);
        else if (!strcasecmp(p, "user")) copystr(r->user, v, MAXSTR);
        else if (!strcasecmp(p, "pass")) copystr(r->pass, v, MAXSTR);
        else if (!strcasecmp(p, "limit")) r->limit = atol(v);
        (void)old;

    }
    fclose(f);
    if (srvct) useserver(0);

}

void writeaccount(void)

{

    char   fn[MAXSTR*2];
    FILE*  f;
    mode_t um;
    ami_long   i;

    acctfile(fn, sizeof(fn));
    /* The mask is set around the open so that the file is never readable
       by anyone else, not even for the moment between being made and
       being given its permissions. */
    um = umask(S_IRWXG | S_IRWXO);
    f = fopen(fn, "w");
    umask(um);
    if (!f) {

        char msg[MAXSTR*4];

        snprintf(msg, sizeof(msg), "%s could not be written:\n%s",
                 fn, strerror(errno));
        fail(msg);

        return;

    }
    fprintf(f, "# Mail accounts. Written by the Config form in mail.\n");
    fprintf(f, "poll %lld\n", AMI_LONG_CAST(pollsec));
    fprintf(f, "sendfrom %lld\n", AMI_LONG_CAST(sendsrv));
    fprintf(f, "threaded %lld\n", AMI_LONG_CAST(threaded));
    for (i = 0; i < srvct; i++) {

        fprintf(f, "\nserver %s\n", servers[i].name);
        fprintf(f, "imap %s\n", servers[i].imap);
        fprintf(f, "imapport %lld\n", AMI_LONG_CAST(servers[i].imapport));
        fprintf(f, "smtp %s\n", servers[i].smtp);
        fprintf(f, "smtpport %lld\n", AMI_LONG_CAST(servers[i].smtpport));
        fprintf(f, "user %s\n", servers[i].user);
        fprintf(f, "pass %s\n", servers[i].pass);
        fprintf(f, "limit %lld\n", AMI_LONG_CAST(servers[i].limit));
        fprintf(f, "end\n");

    }
    fclose(f);
    chmod(fn, S_IRUSR | S_IWUSR);

}

/* is there enough to talk to a server with? */
int haveaccount(void)

{

    ami_long i;

    for (i = 0; i < srvct; i++)
        if (*servers[i].imap && *servers[i].user && *servers[i].pass)
            return (TRUE);

    return (FALSE);

}

/*******************************************************************************

Headers

Mail headers are folded: a header too long for a line is continued on the
next line, which begins with a space. And anything that is not plain
ascii is encoded, by RFC 2047, as =?charset?B?...?= or =?charset?Q?...?=.
Both are undone here, since a subject line shown with its encoding still
on it is not a subject line anybody can read.

*******************************************************************************/

static ami_long b64val(int c)

{

    if (c >= 'A' && c <= 'Z') return (c-'A');
    if (c >= 'a' && c <= 'z') return (c-'a'+26);
    if (c >= '0' && c <= '9') return (c-'0'+52);
    if (c == '+') return (62);
    if (c == '/') return (63);

    return (-1);

}

/* decode base64 into the buffer, giving the length */
static ami_long b64dec(const char* s, ami_long n, char* d, ami_long dn)

{

    ami_long acc = 0, bits = 0, o = 0;
    ami_long i;

    for (i = 0; i < n && o < dn-1; i++) {

        ami_long v = b64val(s[i]);

        if (v < 0) continue; /* whitespace, padding, anything else */
        acc = (acc<<6) | v;
        bits += 6;
        if (bits >= 8) {

            bits -= 8;
            d[o++] = (acc>>bits) & 0xff;

        }

    }
    d[o] = 0;

    return (o);

}

/* decode quoted printable into the buffer, giving the length. In a header
   word an underscore stands for a space; in a body it does not. */
static ami_long qpdec(const char* s, ami_long n, char* d, ami_long dn, int inhdr)

{

    ami_long i, o = 0;

    for (i = 0; i < n && o < dn-1; i++) {

        if (s[i] == '=' && i+2 < n && isxdigit((unsigned char)s[i+1]) &&
                                      isxdigit((unsigned char)s[i+2])) {

            char h[3];

            h[0] = s[i+1]; h[1] = s[i+2]; h[2] = 0;
            d[o++] = (char)strtol(h, NULL, 16);
            i += 2;

        } else if (s[i] == '=' && i+1 < n && s[i+1] == '\n') i++; /* soft */
        else if (s[i] == '=' && i+2 < n && s[i+1] == '\r' && s[i+2] == '\n')
            i += 2; /* a soft break, which joins the lines */
        else if (inhdr && s[i] == '_') d[o++] = ' ';
        else d[o++] = s[i];

    }
    d[o] = 0;

    return (o);

}

/* Undo the RFC 2047 encoding of a header. Only the bytes are recovered:
   a character set that is not ascii or utf8 is left as its bytes, which
   is what the display can show anyway. */
static void hdrdecode(char* s)

{

    char  out[MAXSTR];
    char* p = s;
    ami_long  o = 0;

    while (*p && o < (ami_long)sizeof(out)-1) {

        if (p[0] == '=' && p[1] == '?') {

            char* cs = p+2;
            char* enc;
            char* txt;
            char* end;

            enc = strchr(cs, '?');
            if (!enc) { out[o++] = *p++; continue; }
            txt = strchr(enc+1, '?');
            if (!txt) { out[o++] = *p++; continue; }
            end = strstr(txt+1, "?=");
            if (!end) { out[o++] = *p++; continue; }
            /* the encoding is the one character between the question marks */
            if (enc[1] == 'B' || enc[1] == 'b')
                o += b64dec(txt+1, end-(txt+1), out+o, sizeof(out)-o);
            else if (enc[1] == 'Q' || enc[1] == 'q')
                o += qpdec(txt+1, end-(txt+1), out+o, sizeof(out)-o, TRUE);
            else { out[o++] = *p++; continue; }
            p = end+2;
            /* Space between two encoded words is not part of the text and
               is dropped, which is what puts the words back together. */
            if (*p == ' ' && p[1] == '=' && p[2] == '?') p++;

        } else out[o++] = *p++;

    }
    out[o] = 0;
    copystr(s, out, MAXSTR);

}

/* Find a header in a message and give its value, unfolded and decoded.
   The message is the whole thing, headers then a blank line then the
   body, which is how it is stored. */
int findheader(const char* msg, const char* name, char* val, ami_long vn)

{

    const char* p = msg;
    ami_long    nl = strlen(name);

    *val = 0;
    while (*p && !(p[0] == '\n' && (p[1] == '\n' || (p[1] == '\r' &&
                                                     p[2] == '\n')))) {

        if (!strncasecmp(p, name, nl) && p[nl] == ':') {

            const char* v = p+nl+1;
            ami_long    o = 0;

            while (*v == ' ' || *v == '\t') v++;
            /* take it, and any line after it that begins with a blank,
               which is the same header continued */
            while (*v && o < vn-1) {

                if (*v == '\n') {

                    if (v[1] != ' ' && v[1] != '\t') break; /* it ends */
                    while (*v == '\n' || *v == '\r' ||
                           *v == ' ' || *v == '\t') v++;
                    val[o++] = ' '; /* the fold becomes one space */

                } else if (*v == '\r') v++;
                else val[o++] = *v++;

            }
            val[o] = 0;
            trim(val);
            hdrdecode(val);

            return (TRUE);

        }
        /* on to the next header */
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }

    return (FALSE);

}

/* where the body starts: after the blank line that ends the headers */
static const char* bodyof(const char* msg)

{

    const char* p = msg;

    while (*p) {

        if (p[0] == '\n' && p[1] == '\n') return (p+2);
        if (p[0] == '\n' && p[1] == '\r' && p[2] == '\n') return (p+3);
        p++;

    }

    return (p); /* no body at all */

}

/*******************************************************************************

What kind of mail this is

Every message is given a category when it is indexed. The rules are in a
file, not in this program: mail.cat, read at startup, one rule to a line
-- a category and what it matches -- with the first match naming the
message. Nothing about kinds of mail is written into the code, so the
categories can be changed, added to or thrown away without a compiler.

The rule that earns its keep is the last one. Machine-sent mail says so
in its headers: List-Unsubscribe, List-Id, Precedence: bulk. Mail with
none of those, having matched nothing more particular, was written by a
person -- which is the division that actually matters in a mailbox, and
the one headers get right nearly always.

*******************************************************************************/

typedef struct catrule {

    char            cat[32];   /* what it is called */
    char            kind[12];  /* from, to, subject, header, bulk, always */
    char            match[200]; /* what it looks for */
    struct catrule* next;

} catrule;

static catrule* catrules;
static ami_long catct;

/* read the rules, once */
static void loadcats(void)

{

    static const char* where[] = { "%smail.cat", "%s../graph_programs/mail.cat",
                                   "mail.cat", "graph_programs/mail.cat" };
    char     path[MAXSTR];
    char     dir[MAXSTR];
    char*    e;
    FILE*    f = NULL;
    char     line[MAXSTR];
    catrule* last = NULL;
    ami_long i;

    if (catrules) return;
    dir[0] = 0;
    if (mailprog) {

        snprintf(dir, sizeof(dir), "%s", mailprog);
        e = strrchr(dir, '/');
        if (e) e[1] = 0; else dir[0] = 0;

    }
    for (i = 0; i < 4 && !f; i++) {

        if (i < 2) snprintf(path, sizeof(path), where[i], dir);
        else snprintf(path, sizeof(path), "%s", where[i]);
        f = fopen(path, "r");

    }
    if (!f) return; /* no rules: everything will be "other" */
    while (fgets(line, sizeof(line), f)) {

        char*    p = line;
        char*    k;
        char*    m;
        catrule* r;

        trim(p);
        if (!*p || *p == '#') continue;
        k = p;
        while (*k && *k != ' ' && *k != '\t') k++;
        if (*k) *k++ = 0;
        while (*k == ' ' || *k == '\t') k++;
        m = k;
        while (*m && *m != ' ' && *m != '\t') m++;
        if (*m) *m++ = 0;
        while (*m == ' ' || *m == '\t') m++;
        r = getmem(sizeof(catrule));
        copystr(r->cat, p, sizeof(r->cat));
        copystr(r->kind, k, sizeof(r->kind));
        copystr(r->match, m, sizeof(r->match));
        r->next = NULL;
        if (last) last->next = r; else catrules = r;
        last = r;
        catct++;

    }
    fclose(f);

}

/* does this text hold that, in any case? */
static int holds(const char* hay, const char* needle)

{

    ami_long n = strlen(needle);
    const char* p;

    if (!n) return (FALSE);
    for (p = hay; *p; p++) if (!strncasecmp(p, needle, n)) return (TRUE);

    return (FALSE);

}

/* Does the message carry the marks of having been sent to a list? This
   is the structural test, and the one that tells a person from a
   machine without knowing anything about either. */
static int isbulk(const char* msg)

{

    char v[MAXSTR];

    if (findheader(msg, "List-Unsubscribe", v, sizeof(v))) return (TRUE);
    if (findheader(msg, "List-Id", v, sizeof(v))) return (TRUE);
    if (findheader(msg, "Precedence", v, sizeof(v)) &&
        (holds(v, "bulk") || holds(v, "list"))) return (TRUE);
    if (findheader(msg, "Auto-Submitted", v, sizeof(v)) &&
        !holds(v, "no")) return (TRUE);

    return (FALSE);

}

/* what kind of mail this message is */
void classify(const char* msg, char* cat, ami_long cl)

{

    catrule* r;
    char     from[MAXSTR], to[MAXSTR], subj[MAXSTR], v[MAXSTR];

    copystr(cat, "other", cl);
    loadcats();
    findheader(msg, "From", from, sizeof(from));
    findheader(msg, "Subject", subj, sizeof(subj));
    findheader(msg, "To", to, sizeof(to));
    for (r = catrules; r; r = r->next) {

        int hit = FALSE;

        if (!strcasecmp(r->kind, "from")) hit = holds(from, r->match);
        else if (!strcasecmp(r->kind, "subject")) hit = holds(subj, r->match);
        else if (!strcasecmp(r->kind, "to")) {

            hit = holds(to, r->match);
            if (!hit && findheader(msg, "Cc", v, sizeof(v)))
                hit = holds(v, r->match);

        } else if (!strcasecmp(r->kind, "header"))
            hit = findheader(msg, r->match, v, sizeof(v));
        else if (!strcasecmp(r->kind, "bulk")) hit = isbulk(msg);
        else if (!strcasecmp(r->kind, "always")) hit = TRUE;
        if (hit) { copystr(cat, r->cat, cl); return; }

    }

}

/*******************************************************************************

Making a message readable

Mail is not text any more. It is nearly always MIME, often several
copies of the same message in different forms, and what is wanted is the
plain text one. This finds it, decodes it, and where there is no plain
text one at all it takes the html and strips the tags, which is not
pretty but is better than showing the markup.

*******************************************************************************/

/* the value of a parameter of a header, as charset= or boundary= */
static void hdrparam(const char* hdr, const char* name, char* val, ami_long vn)

{

    const char* p = hdr;
    ami_long    nl = strlen(name);

    *val = 0;
    while ((p = strchr(p, ';'))) {

        p++;
        while (*p == ' ' || *p == '\t') p++;
        if (!strncasecmp(p, name, nl) && p[nl] == '=') {

            const char* v = p+nl+1;
            ami_long    o = 0;
            char        q = 0;

            if (*v == '"') q = *v++;
            while (*v && o < vn-1 && (q? *v != q: (*v != ';' && *v != ' ')))
                val[o++] = *v++;
            val[o] = 0;

            return;

        }

    }

}

/* take the tags out of html, leaving what was between them */
static void detag(const char* s, char* d, ami_long dn)

{

    ami_long o = 0;
    int  sp = FALSE;

    while (*s && o < dn-1) {

        if (*s == '<') { /* a tag, and maybe a whole part */

            if (!strncasecmp(s, "<br", 3) || !strncasecmp(s, "</p", 3) ||
                !strncasecmp(s, "</div", 5) || !strncasecmp(s, "</tr", 4)) {

                if (o < dn-1) d[o++] = '\n';

            }
            if (!strncasecmp(s, "<style", 6) || !strncasecmp(s, "<script", 7)) {

                const char* e = strchr(s+1, '>');

                /* everything in these is for the machine, not the reader */
                while (*s && strncasecmp(s, "</", 2)) s++;
                while (*s && *s != '>') s++;
                if (*s) s++;
                if (!e) break;
                continue;

            }
            while (*s && *s != '>') s++;
            if (*s) s++;
            sp = TRUE;

        } else if (*s == '&') { /* an entity, of the few worth knowing */

            if (!strncasecmp(s, "&nbsp;", 6)) { d[o++] = ' '; s += 6; }
            else if (!strncasecmp(s, "&amp;", 5)) { d[o++] = '&'; s += 5; }
            else if (!strncasecmp(s, "&lt;", 4)) { d[o++] = '<'; s += 4; }
            else if (!strncasecmp(s, "&gt;", 4)) { d[o++] = '>'; s += 4; }
            else if (!strncasecmp(s, "&quot;", 6)) { d[o++] = '"'; s += 6; }
            else if (!strncasecmp(s, "&#39;", 5)) { d[o++] = '\''; s += 5; }
            else d[o++] = *s++;

        } else {

            if (sp && *s != '\n' && o && d[o-1] != '\n' && d[o-1] != ' ')
                d[o++] = ' ';
            sp = FALSE;
            if (o < dn-1) d[o++] = *s++; else s++;

        }

    }
    d[o] = 0;

}

/* decode one part according to what its headers say it is */
static char* decodepart(const char* part, ami_long len, int* ishtml, ami_long want)

{

    char  enc[MAXSTR];
    char  typ[MAXSTR];
    char* raw = getmem(len+1);
    char* out;
    const char* body;
    ami_long  bl;
    ami_long  room;

    memcpy(raw, part, len);
    raw[len] = 0;
    findheader(raw, "Content-Transfer-Encoding", enc, sizeof(enc));
    findheader(raw, "Content-Type", typ, sizeof(typ));
    *ishtml = !strncasecmp(typ, "text/html", 9);
    body = bodyof(raw);
    bl = len-(body-raw);
    /* Only as much as the caller has a use for. The list wants a few
       hundred characters to show beside the subject; decoding a
       megabyte of base64 to throw all but four hundred bytes of it away
       is what made opening a folder of large messages slow. The reader
       asks for the whole thing by wanting nothing in particular. */
    room = bl+2;
    if (want > 0 && want+2 < room) room = want+2;
    out = getmem(room);
    if (!strcasecmp(enc, "base64")) b64dec(body, bl, out, room);
    else if (!strcasecmp(enc, "quoted-printable"))
        qpdec(body, bl, out, room, FALSE);
    else {

        if (bl > room-1) bl = room-1;
        memcpy(out, body, bl);
        out[bl] = 0;

    }
    free(raw);

    return (out);

}

/* What a part says it is. Only the head of it is looked at: a part can
   be an attachment of twenty megabytes, and its headers are in the
   first few lines of it. */
static void parttype(const char* part, ami_long len, char* typ, ami_long tn)

{

    char hdr[4000];
    ami_long n = len;

    if (n > (ami_long)sizeof(hdr)-1) n = sizeof(hdr)-1;
    memcpy(hdr, part, n);
    hdr[n] = 0;
    findheader(hdr, "Content-Type", typ, tn);

}

/* Find the text of a message: the plain text part if there is one, the
   html one with its tags taken out if there is not. Multipart messages
   are walked into, since the plain part is nearly always inside one. */
char* textof(const char* msg, ami_long len, ami_long want)

{

    char  typ[MAXSTR];
    char  bound[MAXSTR];
    char* best = NULL;
    int   besthtml = TRUE;
    char  sep[MAXSTR+8];
    const char* p;
    ami_long  sl;

    findheader(msg, "Content-Type", typ, sizeof(typ));
    if (strncasecmp(typ, "multipart/", 10)) { /* one part, the whole thing */

        int   ishtml;
        char* t = decodepart(msg, len, &ishtml, want);

        if (ishtml) {

            char* d = getmem(strlen(t)+1);

            detag(t, d, strlen(t)+1);
            free(t);
            t = d;

        }

        return (t);

    }
    hdrparam(typ, "boundary", bound, sizeof(bound));
    if (!*bound) { /* multipart with no boundary: take it as it lies */

        ami_long  n = want > 0 && want < len? want: len;
        char* t = getmem(n+1);

        memcpy(t, msg, n);
        t[n] = 0;

        return (t);

    }
    snprintf(sep, sizeof(sep), "--%s", bound);
    sl = strlen(sep);
    /* every part between the separators, keeping the best one found */
    p = msg;
    while ((p = strstr(p, sep))) {

        const char* s = p+sl;
        const char* e;
        int         ishtml;
        char*       t;
        char        ptyp[MAXSTR];

        if (s[0] == '-' && s[1] == '-') break; /* the end separator */
        while (*s == '\r') s++;
        if (*s == '\n') s++;
        e = strstr(s, sep);
        if (!e) e = msg+len;
        parttype(s, e-s, ptyp, sizeof(ptyp));
        if (!strncasecmp(ptyp, "multipart/", 10)) {

            /* A part can be a multipart itself, and usually is: mail
               with an attachment is a multipart/mixed holding a
               multipart/alternative holding the text. Taken as a part
               rather than gone into, the reader is shown the separators
               and the part headers as though they were the message. */
            t = textof(s, e-s, want);
            ishtml = FALSE; /* whatever it found, it found the best of */

        } else if (*ptyp && strncasecmp(ptyp, "text/", 5)) {

            p = e; /* an attachment, a picture, something not to read */
            continue;

        } else t = decodepart(s, e-s, &ishtml, want);
        if (!best || (besthtml && !ishtml)) { /* plain beats html */

            free(best);
            best = t;
            besthtml = ishtml;
            if (!ishtml) break; /* nothing beats plain text */

        } else free(t);
        p = e;

    }
    if (!best) { best = getmem(1); *best = 0; }
    if (besthtml) {

        char* d = getmem(strlen(best)+1);

        detag(best, d, strlen(best)+1);
        free(best);
        best = d;

    }

    return (best);

}

/*******************************************************************************

Dates

The date on a message is RFC 5322: "Fri, 1 Aug 2025 13:06:22 -0700". It is
shown the way mail readers show it, which is the time for something that
came today and the date for anything older, because that is the part the
reader wants.

*******************************************************************************/

static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* When a message came, as the list shows it: the time of day for the
   last twelve hours, the day for the rest of the year, the day and the
   year for older. Shown in our own time, whatever time the sender kept:
   a message is filed by when it arrived here. The index keeps the string
   as it was when the message was indexed, so a list that has been up for
   a day asks again with the date: the "11:15 AM" of this morning is the
   "Sep 9" of tomorrow. */
void whenof(ami_long t, char* show, ami_long sn)

{

    time_t    now = time(NULL);
    time_t    tt = t;
    struct tm lt;

    localtime_r(&tt, &lt);
    if (now-tt < 12*60*60) {

        ami_long h12 = lt.tm_hour%12;

        if (!h12) h12 = 12;
        snprintf(show, sn, "%lld:%02d %s", AMI_LONG_CAST(h12), lt.tm_min,
                 lt.tm_hour < 12? "AM": "PM");

    } else if (now-tt < 300L*24*60*60)
        snprintf(show, sn, "%s %d", months[lt.tm_mon], lt.tm_mday);
    else snprintf(show, sn, "%s %d, %d", months[lt.tm_mon], lt.tm_mday,
                  lt.tm_year+1900);

}

/* pull a date apart, giving the time it stands for, and how to show it */
ami_long parsedate(const char* s, char* show, ami_long sn)

{

    char      mon[10];
    ami_long  day = 0, year = 0, hour = 0, min = 0, sec = 0;
    ami_long  i;
    struct tm tm;
    time_t    t;

    *show = 0;
    while (*s == ' ') s++;
    if (isalpha((unsigned char)*s)) { /* the day name, which tells us nothing */

        while (*s && *s != ',') s++;
        if (*s == ',') s++;

    }
    mon[0] = 0;
    if (sscanf(s, " %lld %9s %lld %lld:%lld:%lld", (long long*)(&day), mon, (long long*)(&year),
               (long long*)(&hour), (long long*)(&min), (long long*)(&sec)) < 3) return (0);
    for (i = 0; i < 12; i++) if (!strncasecmp(mon, months[i], 3)) break;
    if (i >= 12) return (0);
    if (year < 100) year += year < 70? 2000: 1900;
    memset(&tm, 0, sizeof(tm));
    tm.tm_mday = day;
    tm.tm_mon = i;
    tm.tm_year = year-1900;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    /* The zone the sender wrote it in. Without it every message from a
       server keeping UTC -- which is most of them -- was read as though
       its clock were ours, and landed hours in the future: mail sent at
       eight in the evening in London sorted above mail sent here five
       minutes ago, and a message answered to yourself came back ten rows
       down the list instead of at the top of it. */
    {

        const char* z = s;
        ami_long    off = 0;   /* seconds east of UTC */
        int         got = FALSE;

        /* the offset comes after the time, as +hhmm or a name */
        /* four of them: the day, the month, the year and the time, which
           is one word however many parts it has. Stepping over six --
           which the first try of this did -- steps over the zone as
           well, and every message goes back to being read in our own
           time, which is the fault this is here to fix. */
        for (i = 0; i < 4 && *z; i++) {

            while (*z == ' ') z++;
            while (*z && *z != ' ') z++;

        }
        while (*z == ' ') z++;
        if (*z == '+' || *z == '-') {

            ami_long hh = 0, mm = 0;

            if (sscanf(z+1, "%2lld%2lld", (long long*)(&hh), (long long*)(&mm)) == 2) {

                off = (hh*60+mm)*60;
                if (*z == '-') off = -off;
                got = TRUE;

            }

        } else if (isalpha((unsigned char)*z)) {

            static const struct { const char* nm; ami_long off; } zones[] = {

                { "UT", 0 }, { "GMT", 0 }, { "Z", 0 },
                { "EST", -5 }, { "EDT", -4 }, { "CST", -6 }, { "CDT", -5 },
                { "MST", -7 }, { "MDT", -6 }, { "PST", -8 }, { "PDT", -7 },
                { NULL, 0 }

            };
            ami_long k;

            for (k = 0; zones[k].nm; k++)
                if (!strncasecmp(z, zones[k].nm, strlen(zones[k].nm))) {

                    off = zones[k].off*3600;
                    got = TRUE;

                    break;

                }

        }
        /* timegm reads the fields as UTC, which they are once the
           sender's offset is taken off them */
        if (got) {

            tm.tm_isdst = 0;
            t = timegm(&tm)-off;

        } else { /* no zone: the best that can be done is our own */

            tm.tm_isdst = -1;
            t = mktime(&tm);

        }

    }
    if (t == (time_t)-1) return (0);
    whenof((ami_long)t, show, sn);

    return ((ami_long)t);

}

/*******************************************************************************

The mbox file

A message is stored as it arrived, with a line before it saying who it is
from and when it got here. That line is the only thing added, and it is
how every message after the first is found: a line beginning "From " at
the start of a line begins a message. A line in the message that would
look like one has a > put in front of it, which is what mail programs
have always done and what they all undo on the way out.

*******************************************************************************/

/* the address out of a From header: what is inside the angle brackets if
   there are any, the whole thing if not */
void addrof(const char* from, char* addr, ami_long an)

{

    const char* p = strchr(from, '<');
    ami_long    o = 0;

    if (p) {

        p++;
        while (*p && *p != '>' && o < an-1) addr[o++] = *p++;

    } else while (*from && *from != ' ' && o < an-1) addr[o++] = *from++;
    addr[o] = 0;
    if (!o) copystr(addr, "unknown", an);

}

/* The name to show for a sender. "Scott Franco <x@y.com>" shows as the
   name, "x@y.com" shows as the address: what the sender called
   themselves if they said, what they are if they did not. */
void nameof(const char* from, char* name, ami_long nn)

{

    const char* p = strchr(from, '<');
    ami_long    o = 0;

    if (p && p != from) { /* there is a name before the address */

        const char* q = from;

        while (*q == ' ' || *q == '"') q++;
        while (q < p && o < nn-1) name[o++] = *q++;
        while (o && (name[o-1] == ' ' || name[o-1] == '"')) o--;
        name[o] = 0;
        if (o) return;

    }
    addrof(from, name, nn);

}

/* write one message to the folder's mbox file */
/* Store a message, and say where it went: the offset the message itself
   begins at, past the separator line, and the length it came out as --
   which is not the length it went in as, since storing escapes any line
   that could be mistaken for a separator. Those two are what the index
   needs to find it again. */
static ami_long mboxwrite(const char* file, const char* msg, ami_long len,
                      ami_long* stored)

{

    FILE* f = fopen(file, "a");
    ami_long  off;
    char  from[MAXSTR];
    char  addr[MAXSTR];
    char  date[MAXSTR];
    char  show[40];
    ami_long  when;
    const char* p;
    const char* e;

    if (!f) { fail("Cannot write to the mail store"); return (-1); }
    findheader(msg, "From", from, sizeof(from));
    addrof(from, addr, sizeof(addr));
    findheader(msg, "Date", date, sizeof(date));
    when = parsedate(date, show, sizeof(show));
    if (!when) when = (ami_long)time(NULL);
    {

        time_t t = when;

        fprintf(f, "From %s %s", addr, ctime(&t)); /* ctime ends the line */

    }
    off = ftell(f); /* the message itself starts here */
    /* the message, with any line that looks like a separator marked */
    p = msg;
    e = msg+len;
    while (p < e) {

        const char* q = p;

        while (q < e && *q != '\n') q++;
        if (!strncmp(p, "From ", 5) || (*p == '>' && strstr(p, "From ") == p+1))
            fputc('>', f);
        fwrite(p, 1, q-p, f);
        fputc('\n', f);
        p = q < e? q+1: e;

    }
    if (stored) *stored = ftell(f)-off;
    fputc('\n', f); /* a blank line ends a message, always */
    fclose(f);

    return (off);

}

/*******************************************************************************

Indexing a folder

The file is read and every message in it noted: where it is, who it is
from, its subject, when it came, and the start of its text. That is all
the list needs. The message itself is read again from the file when it is
opened, so nothing is held twice.

*******************************************************************************/

/* the start of the message, for the list: the first text, run together */
static void snipof(const char* text, char* snip, ami_long sn)

{

    ami_long o = 0;

    while (*text && o < sn-1) {

        if (*text == '\n' || *text == '\r' || *text == '\t') {

            /* line breaks become one space, since the line is one line */
            if (o && snip[o-1] != ' ') snip[o++] = ' ';
            text++;

        } else if (*text == ' ') {

            if (o && snip[o-1] != ' ') snip[o++] = ' ';
            text++;

        } else snip[o++] = *text++;

    }
    while (o && snip[o-1] == ' ') o--;
    snip[o] = 0;

}

/* note one message found in the file */
/* The index being built. It is built to one side and put in place in one
   move, because it is built on the fetch thread and the display is
   reading the one it already has the whole time. */

/* One message. What is passed is as much of it as was kept -- the
   headers and the beginning of the body, which is all the list shows --
   while len is the length of the whole thing, which is what reading it
   later will need. */
static void fillrec(msgrec* m, const char* msg, ami_long have, ami_long len,
                    ami_long off, const char* dig)

{

    char    from[MAXSTR];
    char    date[MAXSTR];
    char*   text;

    m->off = off;
    m->len = len;
    copystr(m->dig, dig, DIGLEN);
    classify(msg, m->cat, sizeof(m->cat));
    findheader(msg, "From", from, sizeof(from));
    nameof(from, m->from, sizeof(m->from));
    addrof(from, m->addr, sizeof(m->addr));
    if (!findheader(msg, "Subject", m->subject, sizeof(m->subject)))
        copystr(m->subject, "(no subject)", sizeof(m->subject));
    findheader(msg, "Date", date, sizeof(date));
    m->date = parsedate(date, m->when, sizeof(m->when));
    /* only what the list will show: see decodepart */
    text = textof(msg, have, SNIPPET*2);
    snipof(text, m->snip, sizeof(m->snip));
    free(text);

}

/* newest first, which is the order every mail reader shows */
static int bydate(const void* a, const void* b)

{

    const msgrec* x = a;
    const msgrec* y = b;

    if (x->date < y->date) return (1);
    if (x->date > y->date) return (-1);

    return (0);

}

/* Read a folder's file and note every message in it. The separator is a
   line beginning "From " that follows a blank line, or the first line of
   the file; anything else beginning "From " is part of a message. */
/*******************************************************************************

What the worker is doing

None of this is drawn by the worker -- it draws nothing at all. It is
left here and picked up by the display on its next tick. The words
change once a folder and the numbers change once a message, so the two
are kept apart: the display reads a string that is not being rewritten
under it, and puts the numbers -- single words, which cannot be read
half-written -- against it itself.

*******************************************************************************/

void dlock(void);
void dunlock(void);
void serveindex(void);
void servesend(void);
int reachable(const char* host, ami_long port, ami_long secs);

char wrkwhat[MAXSTR]; /* what is being worked on */
ami_long wrkpos;          /* how far into it */
ami_long wrkmax;          /* and how big it is */
ami_long wrkfolds;        /* the folder pane wants redrawing */
ami_long wrkcounts;       /* a folder's count changed: its line wants redrawing */
ami_long wrklist;         /* and so does the message list */
ami_long wrkstop;         /* drop what you are doing */
ami_long wrkbusy;         /* it has something in hand just now */
ami_long idxwant = -1;
ami_long idxfold = -1;
ami_long idxdoing = -1;   /* the folder being read just now */
ami_long wrkgo;      /* a fetch is running on the other thread */
char failsaid[MAXSTR*3];
char sentsaid[MAXSTR]; /* and what went right */
ami_long threaded;     /* the Options box: messages shown by thread */
ami_long sendfail;         /* and whether it was a send that failed */
ami_long failwait;

/* one line of it, or one piece of a line too long to come in one */
/* How much of a message is kept for what the list shows: the headers
   and the start of the body. A mailbox of four gigabytes was being read
   into one allocation of four gigabytes and every message decoded in
   full to find forty characters of snippet. */
#define MSGCAP 65536

/* The digest of a message, taken as the message goes past rather than
   from a copy of it. It has to agree exactly with digestof(), which
   works on a whole message in memory: escapes undone, and every newline
   at the end taken off. The escape is undone at the start of a line
   only, and the trailing newlines are held back and only given to the
   digest if something follows them. */
/* How much of a run of line ends can be held back. The canonical form
   drops every one of them at the end of a message, so none can be given
   to the digest until something with content follows -- and a blank line
   is line ends and nothing else, so a blank line does not settle the
   question, it lengthens it. Half a kilobyte is more consecutive blank
   lines than mail has; a message ending in more would be digested a
   line end too long, and would not match one digested whole. */
#define PENDMAX 512

typedef struct {

    EVP_MD_CTX* c;
    char        pend[PENDMAX]; /* line ends held back */
    ami_long    pendn;

} digrun;

void digstart(digrun* d)

{

    d->c = EVP_MD_CTX_new();
    if (!d->c) { fail("Out of memory"); exit(1); }
    EVP_DigestInit_ex(d->c, EVP_sha256(), NULL);
    d->pendn = 0;

}

/* one line of it, or one piece of a line too long to come in one */void digline(digrun* d, const char* p, ami_long n, int atbol)

{

    ami_long t;

    if (atbol && n && *p == '>') { /* a line the storing escaped? */

        ami_long j = 0;

        while (j < n && p[j] == '>') j++;
        if (j+5 <= n && !strncmp(p+j, "From ", 5)) { p++; n--; } /* one off */

    }
    t = n;
    while (t && (p[t-1] == '\n' || p[t-1] == '\r')) t--;
    if (!t) { /* nothing but line ends: hold them with the rest */

        if (d->pendn+n <= PENDMAX) { memcpy(d->pend+d->pendn, p, n);
                                     d->pendn += n; }
        else { /* further than can be held: give up holding */

            EVP_DigestUpdate(d->c, d->pend, d->pendn);
            EVP_DigestUpdate(d->c, p, n);
            d->pendn = 0;

        }

        return;

    }
    /* something with content in it: whatever was held really was in the
       middle of the message after all */
    if (d->pendn) { EVP_DigestUpdate(d->c, d->pend, d->pendn); d->pendn = 0; }
    EVP_DigestUpdate(d->c, p, t);
    if (n-t > 0 && n-t <= PENDMAX) { memcpy(d->pend, p+t, n-t); d->pendn = n-t; }

}

static void digend(digrun* d, char* hex)

{

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int  n = 0;
    ami_long      i;

    EVP_DigestFinal_ex(d->c, md, &n);
    EVP_MD_CTX_free(d->c);
    d->c = NULL;
    for (i = 0; i < (ami_long)n; i++) sprintf(hex+i*2, "%02x", md[i]);
    hex[n*2] = 0;

}

/*******************************************************************************

Keeping the indexes across a rebuild of the folder list

Asking the servers what folders they have builds the list again from
their answers, which throws away everything the old list held --
including every folder's index. Reading them all back means reading
every mailbox in the store, and losing them means the digests go with
them: a fetch straight after a Get Mail would find nothing already here
and store the whole store a second time. It did, once, and doubled a
test mailbox before the counts gave it away.

So the indexes are set aside before the list is taken again, and each
one is given back to whichever folder ends up with its mailbox. What no
folder claims is let go: its mailbox is not there any more.

*******************************************************************************/

typedef struct {

    char    file[MAXSTR*2];
    msgrec* idx;
    ami_long    ct;
    ami_long    max;
    ami_long    ok;

} idxkeep;

static idxkeep* kept;
static ami_long keptct;

void idxsetaside(void)

{

    ami_long i;

    free(kept);
    kept = getmem(sizeof(idxkeep)*(foldct? foldct: 1));
    keptct = 0;
    for (i = 0; i < foldct; i++) {

        if (!folders[i].idx) continue;
        copystr(kept[keptct].file, folders[i].file, MAXSTR*2);
        kept[keptct].idx = folders[i].idx;
        kept[keptct].ct = folders[i].idxct;
        kept[keptct].max = folders[i].idxmax;
        kept[keptct].ok = folders[i].idxok;
        keptct++;
        folders[i].idx = NULL; /* it belongs to the kept list now */
        folders[i].idxct = 0;
        folders[i].idxmax = 0;
        folders[i].idxok = FALSE;

    }

}

void idxgiveback(void)

{

    ami_long i, j;

    for (i = 0; i < foldct; i++)
        for (j = 0; j < keptct; j++)
            if (kept[j].idx && !strcmp(kept[j].file, folders[i].file)) {

                folders[i].idx = kept[j].idx;
                folders[i].idxct = kept[j].ct;
                folders[i].idxmax = kept[j].max;
                folders[i].idxok = kept[j].ok;
                kept[j].idx = NULL;
                break;

            }
    for (j = 0; j < keptct; j++) free(kept[j].idx); /* nobody wanted it */
    free(kept);
    kept = NULL;
    keptct = 0;
    rehashall(); /* the table names messages by folder, and folders moved */

}

/* A folder has been read, however it was read: from its index file, or
   from the mailbox, or both. It is put in order, the display is pointed
   at it, and the table that finds a message by its digest is taken
   again -- which has to happen on every path, including the one where
   the index was complete and nothing was read at all. It was missing
   from exactly that path once, and the whole store lost its dedup: a
   second fetch stored every message it already had. */
static void idxdone(ami_long fold)

{

    msgrec* was;

    /* Sorted here, where nothing else can see it. Sorting the array the
       display is drawing moves the rows out from under it. */
    qsort(bidx, bct, sizeof(msgrec), bydate);
    dlock();
    was = folders[fold].idx;
    folders[fold].idx = bidx;
    folders[fold].idxct = bct;
    folders[fold].idxmax = bmax;
    folders[fold].msgs = bct;
    folders[fold].idxok = TRUE;
    folders[fold].dirty = FALSE;
    idxfold = fold;
    useidx(); /* the display's list is the new array from here on */
    dunlock();
    /* the array it had is nobody's now */
    free(was);
    bidx = NULL;
    bct = 0;
    bmax = 0;
    rehashall();

}

/* Read a folder: its index file if there is a good one, and whatever
   the mailbox holds past the end of what that names. */
void indexfolder(ami_long fold)

{

    FILE*  f;
    char*  hold;
    char   line[MAXLINE];
    char   hex[DIGLEN];
    digrun dg;
    ami_long   holdn = 0;
    ami_long   start = -1;    /* where the message being read began */
    ami_long   endpos = 0;    /* and where its last line of substance ended */
    ami_long   pos = 0;       /* where in the file the next piece begins */
    ami_long   size = 0;
    ami_long   from = 0;      /* where the reading of the mailbox begins */
    ami_long   had;           /* how many were already known */
    int    blank = TRUE;  /* the line before this one held nothing */
    int    atbol = TRUE;  /* and this piece begins a line */
    int    stopped = FALSE;
    struct timespec t0;

    if (fold < 0) return;
    if (diag) clock_gettime(CLOCK_MONOTONIC, &t0);
    bct = 0;
    idxload(fold);
    had = bct;
    from = folders[fold].idxok? idxend(): 0;
    f = fopen(folders[fold].file, "r");
    if (!f) { /* nothing fetched yet, which is not an error */

        dlock();
        folders[fold].idxct = 0;
        folders[fold].msgs = 0;
        idxfold = fold;
        useidx();
        dunlock();

        return;

    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, from, SEEK_SET);
    pos = from;
    if (size <= from) { /* the index already names all of it */

        fclose(f);
        idxdone(fold);
        if (diag) fprintf(stderr, "index: %s, %lld known, nothing new\n",
                          folders[fold].name, AMI_LONG_CAST(folders[fold].idxct));

        return;

    }
    hold = getmem(MSGCAP+1);
    digstart(&dg);
    /* Read through a piece at a time. A separator is a line beginning
       "From " with nothing on the line before it, which is what the
       storing writes and what it escapes in the messages themselves. A
       line longer than the buffer comes in pieces, and only the first
       piece of one begins a line: everything that asks what a line
       starts with has to ask that first. */
    while (fgets(line, sizeof(line), f)) {

        ami_long ll = strlen(line);
        int  sep = atbol && blank && !strncmp(line, "From ", 5);

        if (sep) {

            if (start >= 0) {

                msgrec m;

                digend(&dg, hex);
                hold[holdn] = 0;
                fillrec(&m, hold, holdn, endpos-start, start, hex);
                *bldroom() = m;
                digstart(&dg);

            }
            start = pos+ll; /* the message begins after the separator */
            endpos = start;
            holdn = 0;
            /* Say how far along, and give up if the reader has asked for
               another folder: there is no sense reading four gigabytes
               nobody is waiting for. */
            if (!(bct%256)) {

                wrkpos = pos-from;
                wrkmax = size-from;
                if ((idxwant >= 0 && idxwant != fold) || wrkstop)
                    { stopped = TRUE; break; }

            }

        } else if (start >= 0) {

            digline(&dg, line, ll, atbol);
            if (holdn < MSGCAP) {

                ami_long take = ll;

                if (take > MSGCAP-holdn) take = MSGCAP-holdn;
                memcpy(hold+holdn, line, take);
                holdn += take;

            }

        }
        atbol = ll && line[ll-1] == '\n';
        if (atbol) blank = ll == 1 || (ll == 2 && line[0] == '\r');
        if (!blank || !atbol) endpos = pos+ll; /* messages do not end blank */
        pos += ll;

    }
    if (start >= 0 && !stopped) { /* the last one */

        msgrec m;

        digend(&dg, hex);
        hold[holdn] = 0;
        fillrec(&m, hold, holdn, endpos-start, start, hex);
        *bldroom() = m;

    } else if (dg.c) { digend(&dg, hex); }
    free(hold);
    fclose(f);
    /* What was read is written down: the whole file if it was worked out
       from nothing, the new lines only if it was added to. */
    if (!from) idxsave(fold);
    else {

        ami_long i;

        for (i = had; i < bct; i++) idxappend(fold, &bidx[i]);

    }
    idxdone(fold);
    if (diag) {

        struct timespec t1;

        clock_gettime(CLOCK_MONOTONIC, &t1);
        fprintf(stderr, "index: %s, %lld new of %lld, read from %lld of %lld, "
                "%.0fms\n", folders[fold].name, AMI_LONG_CAST(folders[fold].idxct-had),
                AMI_LONG_CAST(folders[fold].idxct), AMI_LONG_CAST(from), AMI_LONG_CAST(size),
                (t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1000000.0);

    }

}

/* How many messages a folder holds comes from its index, so asking is
   free and nothing reads a mailbox to answer it. A folder with no index
   yet asks for one. */
void countfolders(void)

{

    ami_long i;

    for (i = 0; i < foldct; i++) {

        folders[i].msgs = folders[i].idxct;
        if (!folders[i].idxok) folders[i].wantidx = TRUE;

    }

}

void srvdir(ami_long srv, char* dn, ami_long dnl)

{

    struct stat sb;

    snprintf(dn, dnl, "%.400s/%.60s", store,
             srv >= 0 && srv < srvct? servers[srv].name: "local");
    if (stat(dn, &sb)) ami_makpth(dn);

}

/* a name with the awkward characters taken out, fit to be a file name */
void safename(const char* nm, char* fn, ami_long fnl)

{

    ami_long i;

    snprintf(fn, fnl, "%s", nm);
    for (i = 0; fn[i]; i++)
        if (fn[i] == '/' || fn[i] == '\\' || fn[i] == ' ' ||
            fn[i] == '[' || fn[i] == ']' || fn[i] == '"') fn[i] = '_';

}

/* the file a local folder of this name lives in */
static void localfile(const char* show, char* fn, ami_long fnl)

{

    char nm[MAXSTR/2];
    ami_long i;

    copystr(nm, show, sizeof(nm));
    for (i = 0; nm[i]; i++)
        if (nm[i] == '/' || nm[i] == '\\' || nm[i] == ' ' ||
            nm[i] == '[' || nm[i] == ']' || nm[i] == '"') nm[i] = '_';
    {

        char dn[MAXSTR];

        srvdir(-1, dn, sizeof(dn));
        snprintf(fn, fnl, "%.400s/%.150s.mbox", dn, nm);

    }

}

/* Find a local folder by its shown name, or make one. Making one is
   writing its name file, so it comes back after a restart, and putting
   it in the list, which keeps locals after the server's folders. */
ami_long localfolder(const char* show)

{

    ami_long i;
    foldrec* f;
    char     fn[MAXSTR*2+8];
    FILE*    sf;

    for (i = 0; i < foldct; i++)
        if (folders[i].local && !strcmp(folders[i].show, show)) return (i);
    if (foldct >= MAXFOLDER) return (-1);
    f = &folders[foldct++];
    copystr(f->name, show, MAXSTR);
    copystr(f->show, show, MAXSTR);
    localfile(show, f->file, sizeof(f->file));
    f->noselect = FALSE;
    f->local = TRUE;
    /* Belonging to no server, which is the whole of what a local folder
       is. Left unsaid it inherits whatever was in the slot, and the
       folder appears under a server's heading -- which is exactly the
       thing a local folder is not. */
    f->srv = -1;
    f->msgs = 0;
    /* the name beside the file, as the server folders keep theirs */
    snprintf(fn, sizeof(fn), "%s.state", f->file);
    sf = fopen(fn, "w");
    if (sf) { fprintf(sf, "0 0 %s\n", show); fclose(sf); }

    return (foldct-1);

}

/* Move messages out of a folder's file and into another's. The set says
   which, by index in the folder's index. The blocks are moved whole and
   verbatim -- separator line to trailing blank -- so nothing is
   reencoded, requoted or otherwise touched on the way.

   The file is streamed, a line at a time, and never held whole: a
   mailbox of four gigabytes was read into memory entire and written
   out again, which is what made a move from the inbox look like
   nothing happening. Which message a block is, is found from where it
   begins, by looking that offset up in the index sorted by offset. */

typedef struct { ami_long off; ami_long i; } offrec;

static int byoff(const void* a, const void* b)

{

    const offrec* x = a;
    const offrec* y = b;

    return (x->off < y->off? -1: x->off > y->off);

}

#define MOVELINE 65536 /* a line is taken in pieces this long */

ami_long movelocal(ami_long fold, const char* dstfile, const char* set)

{

    FILE*    f;
    FILE*    out;
    FILE*    dst;
    FILE*    cur;
    char     tmp[MAXSTR*2+8];
    char*    line;
    offrec*  offs;
    ami_long n, i;
    ami_long pos = 0;
    ami_long moved = 0;
    int      atstart = TRUE;  /* the next read begins a line */
    int      prevblank = TRUE; /* the line before was blank: a separator may follow */

    f = fopen(folders[fold].file, "r");
    if (!f) return (0);
    snprintf(tmp, sizeof(tmp), "%s/movetmp", store);
    out = fopen(tmp, "w");
    dst = fopen(dstfile, "a");
    if (!out || !dst) {

        if (out) fclose(out);
        if (dst) fclose(dst);
        fclose(f);
        fail("The move could not open its files");
        return (0);

    }
    /* the index by offset, for finding a block's message */
    n = folders[fold].idxct;
    offs = getmem((n? n: 1)*sizeof(offrec));
    for (i = 0; i < n; i++) { offs[i].off = folders[fold].idx[i].off; offs[i].i = i; }
    qsort(offs, n, sizeof(offrec), byoff);
    line = getmem(MOVELINE);
    cur = out;
    wrkmax = n;
    wrkpos = 0;
    while (fgets(line, MOVELINE, f)) {

        ami_long len = strlen(line);
        int      whole = len && line[len-1] == '\n';

        /* a separator: "From " at the start of a line, after a blank one
           or at the start of the file, exactly as the indexing takes it */
        if (atstart && prevblank && !strncmp(line, "From ", 5)) {

            /* the message begins past this line; whose is it? */
            ami_long start = pos+len;
            ami_long lo = 0, hi = n-1, m = -1;

            while (lo <= hi) {

                ami_long mid = (lo+hi)/2;

                if (offs[mid].off == start) { m = offs[mid].i; break; }
                if (offs[mid].off < start) lo = mid+1; else hi = mid-1;

            }
            if (m >= 0 && set[m]) { cur = dst; moved++; } else cur = out;
            wrkpos++;

        }
        fwrite(line, 1, len, cur);
        pos += len;
        if (atstart) prevblank = whole && (len == 1 || (len == 2 && line[0] == '\r'));
        else if (whole) prevblank = FALSE;
        atstart = whole;
        if (wrkstop) break;

    }
    free(line);
    free(offs);
    fclose(f);
    fclose(dst);
    fclose(out);
    if (wrkstop) { remove(tmp); return (0); }
    rename(tmp, folders[fold].file);
    /* This mailbox has been written out again without the messages that
       left it, so everything after the first of them sits somewhere
       else. The index is thrown away and taken again from what is
       actually there. */
    idxdrop(fold);

    return (moved);

}

/* The move as the worker does it: the front end asks, with the set of
   messages to go and the folder they go to, and the worker moves them,
   drops the index of the folder they left for reading again, and says
   how many went. The set is the worker's from the ask on. */
static ami_long movfold, movdst, movn;
static char*    movset;
ami_long        movwant;
ami_long        movbusy;

void movask(ami_long fold, ami_long dst, const char* set, ami_long n)

{

    free(movset);
    movset = getmem(n? n: 1);
    memcpy(movset, set, n);
    movn = n;
    movfold = fold;
    movdst = dst;
    movwant = TRUE;

}

void servemove(void)

{

    ami_long fold, dst, moved;
    char*    set;
    char     to[MAXSTR];

    if (!movwant) return;
    dlock(); /* the ask, under the lock the front end wrote it under */
    fold = movfold;
    dst = movdst;
    set = movset;
    movset = NULL;
    movwant = FALSE;
    movbusy = TRUE;
    copystr(to, folders[dst].show, sizeof(to));
    dunlock();
    snprintf(wrkwhat, sizeof(wrkwhat), "Moving to %.400s", to);
    moved = movelocal(fold, folders[dst].file, set);
    free(set);
    wrkwhat[0] = 0;
    wrkpos = 0;
    wrkmax = 0;
    dlock();
    /* The counts are worked out rather than counted again: what left
       this folder arrived in that one. The folder itself is read again,
       since its file has just changed under the list. */
    folders[fold].msgs -= moved;
    if (folders[fold].msgs < 0) folders[fold].msgs = 0;
    folders[dst].msgs += moved;
    folders[fold].dirty = TRUE;
    folders[dst].dirty = TRUE;
    idxwant = fold;
    wrkfolds = TRUE;
    snprintf(sentsaid, sizeof(sentsaid), "%lld message%s moved to %s -- "
             "locally; the server is not touched", AMI_LONG_CAST(moved),
             moved == 1? "": "s", to);
    movbusy = FALSE;
    dunlock();

}

/*******************************************************************************

Talking to the IMAP server

The protocol is lines. A command is given a tag, and everything the
server says back is either untagged, beginning with *, which is data, or
tagged with our tag, which is the answer and ends the command. What makes
it more than that is the literal: a length in braces at the end of a
line, followed by exactly that many bytes, which may be anything at all
including newlines. That is how a message arrives.

*******************************************************************************/

/* send a command, with a tag of its own, and say what the tag was */
static void imsend(char* tag, ami_long tn, const char* fmt, ...)

{

    va_list ap;
    char    cmd[MAXLINE];

    snprintf(tag, tn, "a%03lld", AMI_LONG_CAST(++imaptag));
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    if (diag) fprintf(stderr, "> %s %s\n", tag, cmd);
    fprintf(imap, "%s %s\r\n", tag, cmd);
    fflush(imap);

}

static ami_long netquiet; /* the last read got nothing at all */

/*******************************************************************************

Two threads

The fetching is done on a thread of its own, and the display is left to
the main one. It has to be: a read from a server can take as long as the
server feels like taking, and while it did that on the display thread
the window did not repaint and the mouse did nothing. Timeouts bounded
that wait; they did not remove it.

The division is simple, and it is worth stating plainly because the
whole of the locking depends on it.

    The worker owns the network. It opens connections, reads messages,
    appends them to the mailboxes, writes the digests and the markers,
    and moves the folder counts up. It draws nothing and calls no
    graphics function at all.

    The main thread owns the display. Everything it does, it does while
    handling an event, and it looks at the worker's progress on a timer
    -- ten times a second, which is faster than the eye and slower than
    the flicker that redrawing per message would be.

One lock guards what they share: the folder table and the files under
the store. The main thread holds it for as long as it is handling an
event and drops it while it waits for the next one, so every click,
redraw and menu choice is already covered without a lock of its own.
The worker takes it around the commit of each message -- the append, the
digest, the count -- which is short, and around anything that rebuilds
the folder table.

Because the lock is not recursive, it is taken in exactly those two
places and nowhere else. No helper takes it, so no helper can be called
from a path that already holds it and deadlock.

*******************************************************************************/

ami_long datlock;          /* what the two of them share */

void dlock(void)

{

    if (datlock) ami_lock(datlock);

}

void dunlock(void)

{

    if (datlock) ami_unlock(datlock);

}

ami_long wrkstart;  /* the thread has been made */
ami_long srcstart;  /* and the search thread */
ami_long timerrun;  /* the timer that watches it is going */
ami_long wrkdone;   /* it finished, and nobody has noticed yet */
ami_long wrkrelist; /* this fetch is to ask what folders there are */
ami_long wrkcount;  /* and this one is only to count the store */

/* get one line back, without its line ending */
static int imline(char* buf, ami_long bn)

{

    if (!fgets(buf, bn, imap)) {

        /* Nothing came, and something should have: the server has gone
           quiet or the connection has died under us. Worth telling apart
           from a refusal -- a refused login is a password to go and look
           at, and silence is not. */
        netquiet = TRUE;
        if (diag) fprintf(stderr, "! no answer from %s\n", imapsrv);

        return (FALSE);

    }
    trim(buf);
    if (diag) fprintf(stderr, "< %s\n", buf);

    return (TRUE);

}

/* Is there a literal at the end of this line, and how long? A literal is
   the count in braces, last thing on the line. */
static ami_long literalof(const char* line)

{

    const char* p = strrchr(line, '{');

    if (!p || !strchr(p, '}')) return (-1);
    if (strchr(p, '}')[1]) return (-1); /* the brace is not last */

    return (atol(p+1));

}

/* read exactly n bytes, which is what a literal is */
static char* imliteral(ami_long n)

{

    char* buf = getmem(n+1);
    ami_long  got = 0;

    while (got < n) {

        ami_long r = fread(buf+got, 1, n-got, imap);

        if (r <= 0) break;
        got += r;

    }
    buf[got] = 0;

    return (buf);

}

/* what the server said to the last command, for when it said no */
static char imanswer[MAXLINE];

/* Read to the answer to the command with this tag, handing every
   untagged line to the caller. Gives TRUE if the server said OK. */
static int imwait(const char* tag, void (*line)(const char*))

{

    char line1[MAXLINE];
    ami_long tl = strlen(tag);

    imanswer[0] = 0;
    while (imline(line1, sizeof(line1))) {

        if (!strncmp(line1, tag, tl) && line1[tl] == ' ') {

            const char* r = line1+tl+1;

            /* Keep it. A server that refuses a command usually says why,
               and what it says is worth more to the user than anything
               this program could make up: Gmail answers a login with the
               wrong sort of password by naming the right sort. */
            copystr(imanswer, r, MAXLINE);

            return (!strncasecmp(r, "OK", 2));

        }
        if (line) (*line)(line1);

    }
    copystr(imanswer, "The server stopped answering.", MAXLINE);

    return (FALSE); /* the connection went away */

}

/* the folder list, gathered from the LIST replies */
static ami_long listsrv = -1; /* the server whose folders are arriving */

static void listline(const char* line)

{

    const char* p;
    const char* q;
    char        name[MAXSTR];
    ami_long    o = 0;
    foldrec*    f;

    if (strncmp(line, "* LIST", 6)) return;
    /* A LIST reply is the flags in parentheses, then the delimiter, then
       the name:

           * LIST (\HasNoChildren) "/" "INBOX"      as Gmail writes it
           * LIST (\HasChildren) "." INBOX          as Dovecot writes it

       Either part may be quoted or not, and taking the text between the
       last pair of quotes -- which is what this did -- reads Dovecot's
       delimiter as the name. Every folder then came back called "." and
       not one of them could be opened, which is what an account on
       shared hosting looked like: logged in, folders listed, nothing
       fetched. So it is read in order: past the flags, past the
       delimiter, and what is left is the name. */
    p = line+6;
    while (*p == ' ') p++;
    if (*p == '(') { while (*p && *p != ')') p++; if (*p) p++; } /* flags */
    while (*p == ' ') p++;
    if (*p == '"') { /* the delimiter, quoted */

        p++;
        while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
        if (*p) p++;

    } else while (*p && *p != ' ') p++; /* or NIL, unquoted */
    while (*p == ' ') p++;
    if (*p == '"') { /* the name, quoted */

        p++;
        while (*p && *p != '"' && o < MAXSTR-1) {

            if (*p == '\\' && p[1]) p++; /* what is escaped is literal */
            name[o++] = *p++;

        }

    } else while (*p && o < MAXSTR-1) name[o++] = *p++; /* or bare */
    name[o] = 0;
    trim(name);
    (void)q;
    if (!o || foldct >= MAXFOLDER) return;
    f = &folders[foldct++];
    copystr(f->name, name, MAXSTR);
    /* the name to show: Gmail puts its own folders under [Gmail]/, which
       is a fact about Gmail and not something the reader needs to see */
    if (!strncmp(name, "[Gmail]/", 8)) copystr(f->show, name+8, MAXSTR);
    else copystr(f->show, name, MAXSTR);
    /* A folder marked \\Noselect is a place to hang other folders under
       and holds no messages -- Gmail's [Gmail] is one. It is not a
       folder to a reader, so it is not listed as one. */
    if (strstr(line, "\\Noselect")) { foldct--; return; }
    f->noselect = FALSE;
    f->local = FALSE;
    f->srv = listsrv; /* the server whose list this is */
    /* The file it goes in, named for its server as well as itself: two
       servers may each have an INBOX, and they are not the same mail. */
    {

        char fn[MAXSTR/2];
        ami_long i;

        char  dn[MAXSTR];
        char  sn[MAXSTR*2+8];
        FILE* sf;

        (void)i;
        /* Named for what it is called rather than for the path the
           server files it under: inside the account's own directory the
           [Gmail]/ in front of a Gmail folder says nothing that the
           directory has not said already. */
        safename(f->show, fn, sizeof(fn));
        srvdir(listsrv, dn, sizeof(dn));
        snprintf(f->file, sizeof(f->file), "%.400s/%.150s.mbox", dn, fn);
        /* Unless another folder has that name already. Two folders
           whose shown names come out the same would otherwise share one
           mailbox and merge their mail, which is worse than an ugly
           name, so the second keeps its full one. */
        snprintf(sn, sizeof(sn), "%s.state", f->file);
        sf = fopen(sn, "r");
        if (sf) {

            char real[MAXSTR];
            ami_long v, u;

            if (fscanf(sf, "%lld %lld %499[^\n]", (long long*)(&v), (long long*)(&u), real) == 3) {

                trim(real);
                if (strcmp(real, name)) { /* somebody else's */

                    safename(name, fn, sizeof(fn));
                    snprintf(f->file, sizeof(f->file), "%.400s/%.150s.mbox",
                             dn, fn);

                }

            }
            fclose(sf);

        }

    }

}

/* The folders that are already in the store, for when the server cannot
   be reached. The whole point of keeping the mail in files is that it can
   be read without a server, so a program that shows nothing until it has
   connected has thrown that away. The name is recovered from the file
   name, which is the folder name with the awkward characters replaced,
   so it comes back readable if not always exact. */
/* Read one directory of the store into the folder list: an account's,
   or the one holding what is ours. The folders of a directory are its
   mailboxes, and each keeps its real name in the state file beside it,
   since a file name has had the awkward characters taken out of it. */
void storefolders(ami_long srv)

{

    ami_filptr lp = NULL;
    ami_filptr fp;
    char       dn[MAXSTR];
    char       what[MAXSTR*2];

    srvdir(srv, dn, sizeof(dn));
    snprintf(what, sizeof(what), "%.400s/*.mbox", dn);
    ami_list(what, &lp);
    for (fp = lp; fp && foldct < MAXFOLDER; fp = fp->next) {

        foldrec* f;
        char     nm[MAXSTR/2];
        ami_long n;

        copystr(nm, fp->name, sizeof(nm));
        n = strlen(nm);
        if (n > 5 && !strcmp(nm+n-5, ".mbox")) nm[n-5] = 0; else continue;
        f = &folders[foldct++];
        copystr(f->name, nm, MAXSTR);
        copystr(f->show, nm, MAXSTR);
        { /* the file name had its spaces taken out; put them back for
             the reader, until a fetch writes the real name beside it */

            ami_long k;

            for (k = 0; f->show[k]; k++) if (f->show[k] == '_')
                f->show[k] = ' ';

        }
        snprintf(f->file, sizeof(f->file), "%.400s/%.150s.mbox", dn, nm);
        f->noselect = FALSE;
        f->local = srv < 0;
        f->srv = srv;
        { /* the real name, if the state file beside it kept one */

            char  sn[MAXSTR*2+8];
            char  real[MAXSTR];
            ami_long  v, u, u2;
            FILE* sf;

            snprintf(sn, sizeof(sn), "%s.state", f->file);
            sf = fopen(sn, "r");
            if (sf) {

                if (fscanf(sf, "%lld %lld %lld %499[^\n]", (long long*)(&v), (long long*)(&u), (long long*)(&u2),
                           real) == 4 ||
                    (rewind(sf), fscanf(sf, "%lld %lld %499[^\n]",
                                        (long long*)(&v), (long long*)(&u), real) == 3)) {

                    trim(real);
                    copystr(f->name, real, MAXSTR);
                    if (!strncmp(real, "[Gmail]/", 8))
                        copystr(f->show, real+8, MAXSTR);
                    else copystr(f->show, real, MAXSTR);

                }
                fclose(sf);

            }

        }

    }

}

/* every account's folders, then the local ones, which is the order the
   pane shows them in */
void storeall(void)

{

    ami_long i;

    for (i = 0; i < srvct; i++) storefolders(i);
    storefolders(-1);

}

/* connect and log in */
static int imapopen(void)

{

    ami_ulong addr;
    char          tag[20];
    char          line[MAXLINE];

    if (imap) return (TRUE); /* already there */
    if (!reachable(imapsrv, imapport, 10)) {

        char msg[MAXSTR*2];

        snprintf(msg, sizeof(msg), "Cannot reach %s on port %lld.", imapsrv,
                 AMI_LONG_CAST(imapport));
        fail(msg);

        return (FALSE);

    }
    ami_addrnet(imapsrv, &addr);
    imap = ami_opennet(addr, imapport, TRUE);
    if (!imap) { fail("Cannot reach the mail server"); return (FALSE); }
    { /* a read that never comes back must not stop the program

         A server that stops answering -- and one will, over tens of
         thousands of messages -- leaves a read waiting for data that is
         never sent. Every read here is on the same thread as the
         display, so the whole program stops with it: the window does
         not repaint and the mouse does nothing, which looks like a
         crash and is not. A timeout turns that into an error the
         program can act on. */

        struct timeval tv;
        int            fd = fileno(imap);
        int            on = 1;

        tv.tv_sec = NETWAIT;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        /* and let the system notice a connection that has gone away
           rather than waiting on one that will never speak again */
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    }
    if (!imline(line, sizeof(line))) { /* the greeting */

        fail("The mail server said nothing");
        fclose(imap);
        imap = NULL;

        return (FALSE);

    }
    /* The password is sent in the clear inside the TLS connection, which
       is what LOGIN is and what every mail program does. It is never
       written to the diagnostic. */
    if (diag) fprintf(stderr, "> a%03lld LOGIN %s <password>\n",
                      AMI_LONG_CAST(imaptag+1), username);
    {

        ami_long sav = diag;

        diag = FALSE;
        netquiet = FALSE;
        imsend(tag, sizeof(tag), "LOGIN \"%s\" \"%s\"", username, password);
        diag = sav;

    }
    if (!imwait(tag, NULL)) {

        char msg[MAXSTR*3];
        char said[MAXSTR];

        /* the server's own words, with the NO or BAD and any bracketed
           code taken off the front, since those mean nothing to a reader */
        copystr(said, imanswer, MAXSTR);
        {

            char* p = said;

            if (!strncasecmp(p, "NO ", 3)) p += 3;
            else if (!strncasecmp(p, "BAD ", 4)) p += 4;
            if (*p == '[') { while (*p && *p != ']') p++; if (*p) p++; }
            while (*p == ' ') p++;
            memmove(said, p, strlen(p)+1);

        }
        if (netquiet) /* it did not refuse us, it said nothing at all */
            snprintf(msg, sizeof(msg),
                     "%s stopped answering during the login.\n"
                     "It will be left alone for a while and tried again.",
                     imapsrv);
        else snprintf(msg, sizeof(msg),
                      "%s would not accept the login.\n%s", imapsrv, said);
        fail(msg);
        fclose(imap);
        imap = NULL;

        return (FALSE);

    }

    return (TRUE);

}

void imapclose(void)

{

    char tag[20];

    if (!imap) return;
    imsend(tag, sizeof(tag), "LOGOUT");
    imwait(tag, NULL);
    fclose(imap);
    imap = NULL;

}

/* ask the server what folders there are */
int getfolders(ami_long srv)

{

    char tag[20];

    useserver(srv);
    listsrv = srv;
    if (!imapopen()) return (FALSE);
    imsend(tag, sizeof(tag), "LIST \"\" \"*\"");
    if (!imwait(tag, listline)) { fail("The server would not list the "
                                       "folders"); return (FALSE); }

    return (TRUE);

}

/* what the last fetch of this folder reached */
/* What a folder has been read of: the stretch of uids already here,
   from the oldest taken to the newest. Only the newest was remembered
   before, with everything below it assumed present -- which is true
   while a fetch always takes the newest so many and works upward, and
   false the moment somebody asks for more of the folder than they asked
   for last time. Those older messages are all below the mark, so every
   one of them was skipped and raising the limit did nothing at all. */
static void readstate(ami_long fold, ami_long* validity, ami_long* lowuid,
                      ami_long* lastuid)

{

    char  fn[MAXSTR*2+8];
    FILE* f;
    char  line[MAXSTR];
    ami_long  a, b, c;

    *validity = 0;
    *lowuid = 0;
    *lastuid = 0;
    snprintf(fn, sizeof(fn), "%s.state", folders[fold].file);
    f = fopen(fn, "r");
    if (!f) return;
    if (fgets(line, sizeof(line), f)) {

        if (sscanf(line, "%lld %lld %lld", (long long*)(&a), (long long*)(&b), (long long*)(&c)) == 3 && c >= b) {

            /* validity, the oldest taken, the newest */
            *validity = a;
            *lowuid = b;
            *lastuid = c;

        } else if (sscanf(line, "%lld %lld", (long long*)(&a), (long long*)(&b)) == 2) {

            /* the older form, which knew only the newest. Nothing can be
               said about what is below it, so nothing is assumed: the
               stretch is empty and an older message is fetched again
               rather than passed over. The digests will know it if it
               is already here. */
            *validity = a;
            *lowuid = 0;
            *lastuid = 0;
            (void)b;

        }

    }
    fclose(f);

}

static void writestate(ami_long fold, ami_long validity, ami_long lowuid, ami_long lastuid)

{

    char  fn[MAXSTR*2+8];
    FILE* f;

    snprintf(fn, sizeof(fn), "%s.state", folders[fold].file);
    f = fopen(fn, "w");
    if (!f) return;
    /* The name goes down with it. The file it is kept in has the
       awkward characters taken out of the name, which cannot be undone,
       so without this a folder read back from the store on its own
       shows as _Gmail__Sent_Mail rather than Sent Mail. */
    fprintf(f, "%lld %lld %lld %s\n", AMI_LONG_CAST(validity), AMI_LONG_CAST(lowuid), AMI_LONG_CAST(lastuid),
            folders[fold].name);
    fclose(f);

}

/* what EXAMINE says about the folder, gathered from its untagged lines */
static ami_long exists, uidvalidity;

static void examline(const char* line)

{

    const char* p;
    char        what[40];
    ami_long    n;

    /* The word is checked, not assumed. sscanf gives the number of
       assignments it made before it gave up, so "* 0 RECENT" against
       "* %ld EXISTS" assigns the 0 and answers 1 just as a real EXISTS
       does -- and RECENT comes straight after EXISTS, so the count was
       being thrown away and every folder looked empty. */
    if (sscanf(line, "* %lld %39s", (long long*)(&n), what) == 2 &&
        !strcasecmp(what, "EXISTS")) exists = n;
    p = strstr(line, "[UIDVALIDITY ");
    if (p) uidvalidity = atol(p+13);

}

/* the uids of the messages asked for, gathered from the FETCH replies */
static ami_long* uidlist;
static ami_long  uidct;
static ami_long  uidmax;

static void uidline(const char* line)

{

    const char* p = strstr(line, "UID ");

    if (strncmp(line, "* ", 2) || !strstr(line, "FETCH") || !p) return;
    if (uidct >= uidmax) {

        uidmax = uidmax? uidmax*2: 256;
        uidlist = realloc(uidlist, uidmax*sizeof(ami_long));
        if (!uidlist) { fail("Out of memory"); exit(1); }

    }
    uidlist[uidct++] = atol(p+4);

}

/*******************************************************************************

Talking to the SMTP server

Nothing is sent yet: this program reads mail. What is here is the part
that proves the account could send -- connect, say hello, start TLS if
the port is the one that needs it, log in, and say goodbye -- so that the
sending half has somewhere to stand when it is written.

*******************************************************************************/

/* read a reply, which may be several lines, and give its code */
static ami_long smtpresp(FILE* f)

{

    char line[MAXLINE];
    ami_long code = 0;

    while (fgets(line, sizeof(line), f)) {

        trim(line);
        if (diag) fprintf(stderr, "< %s\n", line);
        code = atol(line);
        if (strlen(line) < 4 || line[3] != '-') break; /* the last line */

    }

    return (code);

}

static void smtpsend(FILE* f, const char* fmt, ...)

{

    va_list ap;
    char    cmd[MAXLINE];

    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    if (diag) fprintf(stderr, "> %s\n", cmd);
    fprintf(f, "%s\r\n", cmd);
    fflush(f);

}

/* base64 encode, which is how a password is given to SMTP */
static void b64enc(const char* s, ami_long n, char* d, ami_long dn)

{

    static const char* tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz0123456789+/";
    ami_long i, o = 0;

    for (i = 0; i < n; i += 3) {

        ami_long v = (unsigned char)s[i];
        ami_long r = n-i;

        v = v<<8 | (r > 1? (unsigned char)s[i+1]: 0);
        v = v<<8 | (r > 2? (unsigned char)s[i+2]: 0);
        if (o < dn-4) {

            d[o++] = tab[(v>>18)&0x3f];
            d[o++] = tab[(v>>12)&0x3f];
            d[o++] = r > 1? tab[(v>>6)&0x3f]: '=';
            d[o++] = r > 2? tab[v&0x3f]: '=';

        }

    }
    d[o] = 0;

}

/* Prove the account can send, without sending anything. */
/*******************************************************************************

Sending

The one thing this program does that reaches out rather than reads. It
speaks just enough SMTP: a connection, a greeting, a login, who it is
from, who it is to, and the message itself.

A copy of everything sent is put in a local folder called Sent. That is
the same rule the rest of the program is built to -- what is here is
here, in a file, not on somebody's server -- and it means a sent message
can be read, searched and counted like any other.

*******************************************************************************/

/* Split a list of addresses on commas, giving each in turn. Quoted
   display names can hold commas, so the split ignores anything inside
   quotes or angle brackets. */
const char* nextaddr(const char* p, char* d, ami_long dl)

{

    ami_long i = 0;
    int  quote = FALSE;
    int  angle = FALSE;

    if (!p || !*p) { if (dl) *d = 0; return (NULL); }
    while (*p == ' ' || *p == ',') p++;
    while (*p && (quote || angle || *p != ',')) {

        if (*p == '"') quote = !quote;
        if (*p == '<') angle = TRUE;
        if (*p == '>') angle = FALSE;
        if (i < dl-1) d[i++] = *p;
        p++;

    }
    if (dl) d[i] = 0;
    trim(d);

    return (*p? p+1: NULL);

}

/* Something that will do as a message id: the time, a count within the
   second, and the sending account's domain. */
static void makemsgid(char* d, ami_long dl)

{

    static ami_long ct;
    const char* at = sendsrv >= 0 && sendsrv < srvct?
                     strchr(servers[sendsrv].user, '@'): NULL;

    snprintf(d, dl, "<%lld.%lld.amimail@%s>", AMI_LONG_CAST((ami_long)time(NULL)), AMI_LONG_CAST(++ct),
             at? at+1: "localhost");

}

/* the date, as a message header wants it */
static void makedate(char* d, ami_long dl)

{

    time_t    t = time(NULL);
    struct tm lt;

    localtime_r(&t, &lt);
    strftime(d, dl, "%a, %d %b %Y %H:%M:%S %z", &lt);

}

/* Send one message. Gives an empty string if it went, and what went
   wrong if it did not. The whole of it happens on the fetch thread, so
   that a server taking its time over a message does not stop the
   display. */
void sendmail(const char* to, const char* cc, const char* subject,
                     const char* body, const char* inreply,
                     const char* refs, char* err, ami_long errl)

{

    ami_ulong addr;
    FILE*  f;
    char   b64[MAXSTR*2];
    char   one[MAXSTR];
    char   ssrv[MAXSTR];  /* the sending account's own details, taken
                             here rather than from whatever the fetch
                             has open: the two run at once */
    char   suser[MAXSTR];
    char   spass[MAXSTR];
    ami_long   sport;
    char   msgid[MAXSTR];
    char   date[MAXSTR];
    char*  msg;
    ami_long   msgl;
    ami_long   o = 0;
    ami_long   code;
    const char* p;
    ami_long   sent = 0;

    *err = 0;
    if (sendsrv < 0 || sendsrv >= srvct) sendsrv = 0;
    dlock();
    copystr(ssrv, servers[sendsrv].smtp, MAXSTR);
    sport = servers[sendsrv].smtpport;
    copystr(suser, servers[sendsrv].user, MAXSTR);
    copystr(spass, servers[sendsrv].pass, MAXSTR);
    dunlock();
    if (!*ssrv || !*suser) {

        copystr(err, "No account is set to send from. Config/Servers has a "
                "box for it, and a sending server to fill in.", errl);

        return;

    }
    if (!reachable(ssrv, sport, 10)) {

        snprintf(err, errl, "Cannot reach %s on port %lld.\n"
                 "Check the sending server and its port in Config/Servers. "
                 "Gmail sends on 465.", ssrv, AMI_LONG_CAST(sport));

        return;

    }
    ami_addrnet(ssrv, &addr);
    /* 465 is TLS from the first byte; 587 begins in the clear and turns
       it on with STARTTLS, which this cannot do through the library's
       socket, so 465 is the one to use */
    f = ami_opennet(addr, sport, TRUE);
    if (!f) { snprintf(err, errl, "Cannot reach %s.", ssrv); return; }
    {

        struct timeval tv;
        int            fd = fileno(f);

        tv.tv_sec = NETWAIT;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    }
    if (smtpresp(f)/100 != 2)
        { copystr(err, "The sending server did not greet us.", errl);
          fclose(f); return; }
    smtpsend(f, "EHLO localhost");
    if (smtpresp(f)/100 != 2)
        { copystr(err, "The sending server refused EHLO.", errl);
          fclose(f); return; }
    smtpsend(f, "AUTH LOGIN");
    if (smtpresp(f) != 334)
        { copystr(err, "The sending server would not start a login.", errl);
          fclose(f); return; }
    b64enc(suser, strlen(suser), b64, sizeof(b64));
    smtpsend(f, "%s", b64);
    if (smtpresp(f) != 334)
        { copystr(err, "The sending server refused the user name.", errl);
          fclose(f); return; }
    b64enc(spass, strlen(spass), b64, sizeof(b64));
    if (diag) fprintf(stderr, "> <password>\n");
    fprintf(f, "%s\r\n", b64);
    fflush(f);
    if (smtpresp(f)/100 != 2) {

        copystr(err, "The sending server would not accept the login.\n"
                "For Gmail this must be an application password.", errl);
        fclose(f);

        return;

    }
    smtpsend(f, "MAIL FROM:<%s>", suser);
    if (smtpresp(f)/100 != 2)
        { copystr(err, "The sending server would not take the sender.", errl);
          fclose(f); return; }
    /* everybody it is going to, whether they are shown or not */
    for (p = to; (p = nextaddr(p, one, sizeof(one))) || *one; ) {

        char just[MAXSTR];

        if (*one) {

            addrof(one, just, sizeof(just));
            smtpsend(f, "RCPT TO:<%s>", *just? just: one);
            if (smtpresp(f)/100 == 2) sent++;

        }
        if (!p) break;

    }
    for (p = cc; p && (p = nextaddr(p, one, sizeof(one))) || (cc && *one); ) {

        char just[MAXSTR];

        if (*one) {

            addrof(one, just, sizeof(just));
            smtpsend(f, "RCPT TO:<%s>", *just? just: one);
            if (smtpresp(f)/100 == 2) sent++;

        }
        if (!p) break;

    }
    if (!sent) {

        copystr(err, "Nobody the message is addressed to was accepted.", errl);
        smtpsend(f, "QUIT");
        smtpresp(f);
        fclose(f);

        return;

    }
    smtpsend(f, "DATA");
    if (smtpresp(f) != 354)
        { copystr(err, "The sending server would not take the message.", errl);
          fclose(f); return; }
    makemsgid(msgid, sizeof(msgid));
    makedate(date, sizeof(date));
    /* The message itself, headers and all, kept so that the same bytes
       can be put in the Sent folder as went down the wire. */
    msgl = strlen(body)+MAXSTR*8;
    msg = getmem(msgl);
    o += snprintf(msg+o, msgl-o, "From: %s\r\n", suser);
    o += snprintf(msg+o, msgl-o, "To: %s\r\n", to);
    if (cc && *cc) o += snprintf(msg+o, msgl-o, "Cc: %s\r\n", cc);
    o += snprintf(msg+o, msgl-o, "Subject: %s\r\n", subject);
    o += snprintf(msg+o, msgl-o, "Date: %s\r\n", date);
    o += snprintf(msg+o, msgl-o, "Message-ID: %s\r\n", msgid);
    if (inreply && *inreply) {

        o += snprintf(msg+o, msgl-o, "In-Reply-To: %s\r\n", inreply);
        o += snprintf(msg+o, msgl-o, "References: %s%s%s\r\n",
                      refs && *refs? refs: "", refs && *refs? " ": "",
                      inreply);

    }
    o += snprintf(msg+o, msgl-o, "MIME-Version: 1.0\r\n");
    o += snprintf(msg+o, msgl-o,
                  "Content-Type: text/plain; charset=UTF-8\r\n");
    o += snprintf(msg+o, msgl-o, "\r\n");
    /* the body, with every line ending as the wire wants it and any
       line of a single dot made safe */
    for (p = body; *p; ) {

        const char* q = p;

        while (*q && *q != '\n') q++;
        if (q-p == 1 && *p == '.') o += snprintf(msg+o, msgl-o, ".");
        if (o+(q-p)+3 < msgl) {

            memcpy(msg+o, p, q-p);
            o += q-p;
            o += snprintf(msg+o, msgl-o, "\r\n");

        }
        p = *q? q+1: q;

    }
    msg[o] = 0;
    fwrite(msg, 1, o, f);
    fprintf(f, "\r\n.\r\n");
    fflush(f);
    code = smtpresp(f);
    smtpsend(f, "QUIT");
    smtpresp(f);
    fclose(f);
    if (code/100 != 2) {

        snprintf(err, errl, "The sending server would not take the message.");
        free(msg);

        return;

    }
    /* And a copy here, which is the point of the whole program: what is
       sent is ours as much as what is received. */
    {

        ami_long fold;
        char hex[DIGLEN];
        ami_long off, stored = 0;

        digestof(msg, o, hex);
        /* The whole of it with the display shut out: making the folder
           adds to the table the display draws from, and the message
           itself grows the array the display's list IS. */
        dlock();
        fold = localfolder("Sent");
        if (fold >= 0) {


            off = mboxwrite(folders[fold].file, msg, o, &stored);
            if (off >= 0) {

                msgrec* m = idxroom(fold);

                fillrec(m, msg, o, stored, off, hex);
                adddigest(fold, folders[fold].idxct-1);
                idxappend(fold, m);
                folders[fold].msgs = folders[fold].idxct;
                folders[fold].dirty = TRUE;
                useidx();

            }

        }
        dunlock();
        wrkfolds = TRUE;
        wrklist = TRUE;

    }
    free(msg);

}

void smtpcheck(void)

{

    ami_ulong addr;
    FILE* f;
    char  b64[MAXSTR*2];
    char  msg[MAXSTR*2];
    ami_long  code;

    if (!reachable(smtpsrv, smtpport, 10)) {

        char msg[MAXSTR*2];

        snprintf(msg, sizeof(msg), "Cannot reach %s on port %lld.\n"
                 "Gmail sends on 465.", smtpsrv, AMI_LONG_CAST(smtpport));
        fail(msg);

        return;

    }
    ami_addrnet(smtpsrv, &addr);
    /* 465 is TLS from the first byte; 587 begins in the clear and turns
       it on with STARTTLS, which this cannot do through the library's
       socket, so 465 is the one to use */
    f = ami_opennet(addr, smtpport, TRUE);
    if (!f) { fail("Cannot reach the sending server"); return; }
    if (smtpresp(f)/100 != 2)
        { fail("The sending server did not greet us"); fclose(f); return; }
    smtpsend(f, "EHLO localhost");
    if (smtpresp(f)/100 != 2)
        { fail("The sending server refused EHLO"); fclose(f); return; }
    smtpsend(f, "AUTH LOGIN");
    if (smtpresp(f) != 334) {

        fail("The sending server would not start a login.\n"
             "On port 587 the login begins in the clear and needs "
             "STARTTLS; try port 465 instead.");
        fclose(f);

        return;

    }
    b64enc(username, strlen(username), b64, sizeof(b64));
    smtpsend(f, "%s", b64);
    if (smtpresp(f) != 334)
        { fail("The sending server refused the user name"); fclose(f);
          return; }
    b64enc(password, strlen(password), b64, sizeof(b64));
    if (diag) fprintf(stderr, "> <password>\n");
    fprintf(f, "%s\r\n", b64);
    fflush(f);
    code = smtpresp(f);
    smtpsend(f, "QUIT");
    smtpresp(f);
    fclose(f);
    if (code/100 == 2)
        snprintf(msg, sizeof(msg),
                 "Mail could be sent from this account.\n"
                 "%s accepted the login on port %lld. Nothing was sent.",
                 smtpsrv, AMI_LONG_CAST(smtpport));
    else
        snprintf(msg, sizeof(msg),
                 "%s would not accept the login on port %lld.\n"
                 "For Gmail this must be an application password.",
                 smtpsrv, AMI_LONG_CAST(smtpport));
    fail(msg);

}

/*******************************************************************************

/*******************************************************************************

Fetching

*******************************************************************************/

/*******************************************************************************

Fetching, a message at a time

A fetch of a real mailbox is thousands of messages and takes as long as
it takes. Done in one loop it would hold the event loop shut for all of
that time, and a window that is not in its event loop is a window that
shows nothing: the library puts its drawing on the wire in ami_event and
nowhere else, so a progress line written from inside such a loop is
written to a screen that never repaints. The window would sit there,
frozen and blank, for hours.

So the fetch is a state machine, stepped by a timer. Each tick takes one
message and returns, which keeps the event loop turning: the count
appears as it climbs, the window redraws, the folders can be clicked
through while it runs, and it can be stopped.

*******************************************************************************/

ami_long fetching;    /* a fetch is under way */
static ami_long fetchsrv;    /* the server being read */
static ami_long fetchfold;   /* where the stepping has got to in the order */
static ami_long fetchcur = -1; /* and the folder that place names */
static ami_long fetchi;      /* which of that folder's uids is next */
ami_long fetchgot;    /* messages taken, this fetch */
ami_long fetchdup;    /* and passed over as already here */
static ami_long fetchlast;   /* the highest uid taken from this folder */
static ami_long fetchseen;   /* the highest taken before this fetch */
static ami_long fetchlow;    /* and the lowest, which together say what is
                            already here rather than only how far up */
static ami_long fetchnewlow; /* the lowest this fetch has reached */

/*******************************************************************************

Leaving a server alone after it stops answering

A server that has gone quiet is not helped by being asked again a
quarter of a minute later, and a server that has gone quiet because it
has had enough of us -- which is what tens of thousands of messages
earns from a large provider -- is made worse by it. So a failure puts
that account aside for a while, and the while doubles each time: a
quarter minute, then half, then a minute, up to a quarter of an hour.
Anything that arrives puts it back to nothing.

This is the whole of the retry policy, and it is deliberately not a loop
around the read. The fetch is already resumable -- every folder
remembers the stretch it has taken, and every message is known by its
digest -- so the next look IS the retry, and it costs nothing to let it
be the next scheduled one. A retry inside the fetch would only be the
same request sent sooner, to a server that has just declined to answer.

*******************************************************************************/

#define BACKOFF   15  /* seconds to wait after the first failure */
#define BACKMAX   900 /* and never more than a quarter of an hour */

static time_t srvquiet[MAXSRV]; /* not to be asked before this time */
static ami_long   srvwait[MAXSRV];  /* how long it was left alone last time */

/* that account has stopped answering */
static void serverfailed(ami_long srv)

{

    if (srv < 0 || srv >= MAXSRV) return;
    srvwait[srv] = srvwait[srv]? srvwait[srv]*2: BACKOFF;
    if (srvwait[srv] > BACKMAX) srvwait[srv] = BACKMAX;
    srvquiet[srv] = time(NULL)+srvwait[srv];
    if (diag) fprintf(stderr, "! %s left alone for %llds\n",
                      servers[srv].name, AMI_LONG_CAST(srvwait[srv]));

}

/* and that one is talking again */
static void serverspoke(ami_long srv)

{

    if (srv < 0 || srv >= MAXSRV) return;
    srvwait[srv] = 0;
    srvquiet[srv] = 0;

}

/* is this account being left alone just now? */
int serverquiet(ami_long srv)

{

    if (srv < 0 || srv >= MAXSRV) return (FALSE);

    return (srvquiet[srv] && time(NULL) < srvquiet[srv]);

}

/* Move to the next folder worth reading, and ask it what it holds.
   Gives FALSE when there are no folders left. */
/* The order the folders are fetched in: the accounts take turns, one
   folder each, round and round, rather than one account being finished
   before the next is begun. A big account and a small one arrive
   together that way; done in order, a mailbox of tens of thousands
   starves everything configured after it, and the second account sits
   at (not fetched) through cycle after cycle of the first.

   Worked out once at the start of a fetch, into an order the stepping
   walks straight down. */
static ami_long fetchord[MAXFOLDER];
static ami_long fetchordct;

void fetchorder(void)

{

    ami_long round;
    ami_long i;

    fetchordct = 0;
    for (round = 0; round < MAXFOLDER; round++) {

        ami_long took = 0;

        for (i = 0; i < srvct; i++) { /* one folder from each, in turn */

            ami_long seen = 0;
            ami_long k;

            if (serverquiet(i)) continue; /* it is not answering yet */

            for (k = 0; k < foldct; k++) {

                if (folders[k].srv != i) continue;
                if (folders[k].local || folders[k].noselect) continue;
                if (seen++ != round) continue;
                fetchord[fetchordct++] = k;
                took++;
                break;

            }

        }
        if (!took) break; /* every account is out of folders */

    }

}

static int fetchnext(void)

{

    char tag[20];
    ami_long validity;
    ami_long lo, hi;

    while (++fetchfold < fetchordct) {

        ami_long fold = fetchord[fetchfold];

        if (folders[fold].srv != fetchsrv) { /* a different account */

            imapclose();
            fetchsrv = folders[fold].srv;
            if (fetchsrv < 0) continue;
            snprintf(wrkwhat, sizeof(wrkwhat), "Connecting to %s",
                     servers[folders[fold].srv].name);
            wrkpos = 0;
            wrkmax = 0;
            /* The server's name and password are copied out of the
               table the config window writes, so the copy is made with
               that window shut out of it. */
            dlock();
            useserver(fetchsrv);
            dunlock();
            if (!imapopen()) { serverfailed(fetchsrv); fetchsrv = -2; continue; }
            serverspoke(fetchsrv);

        }
        fetchi = 0;
        uidct = 0;
        snprintf(wrkwhat, sizeof(wrkwhat), "%.60s: %.400s",
                 fetchsrv >= 0? servers[fetchsrv].name: "", folders[fold].show);
        wrkpos = 0;
        wrkmax = 0;
        exists = 0;
        uidvalidity = 0;
        fetchcur = fold; /* the folder this step is working on */
        imsend(tag, sizeof(tag), "EXAMINE \"%s\"", folders[fold].name);
        if (!imwait(tag, examline)) continue; /* gone, or not a folder */
        if (!exists) continue; /* nothing in it */
        readstate(fold, &validity, &fetchlow, &fetchseen);
        /* a folder that was rebuilt on the server has new uids for the
           same messages, so what was taken before means nothing */
        if (validity != uidvalidity) { fetchlow = 0; fetchseen = 0; }
        fetchlast = fetchseen;
        fetchnewlow = 0;
        /* the most recent messages, as many as were asked for */
        hi = exists;
        lo = hi-limit+1;
        if (lo < 1) lo = 1;
        imsend(tag, sizeof(tag), "FETCH %lld:%lld (UID)", AMI_LONG_CAST(lo), AMI_LONG_CAST(hi));
        if (!imwait(tag, uidline)) continue;
        wrkmax = uidct; /* what this folder is offering */
        if (uidct) return (TRUE);

    }

    return (FALSE);

}

/* The fetch is over, however it ended. This runs on the worker, so it
   puts the connection down and says so; what the reader sees of the end
   is drawn by the main thread when it notices. */
void fetchend(void)

{

    ami_long i;

    imapclose();
    /* What arrived was put at the end of each folder's index as it came.
       The list is shown newest first, so the folders that took anything
       are put in order -- and the table that finds a message by its
       digest is taken again, since it names them by where they sit and
       sorting has moved them. */
    for (i = 0; i < foldct; i++) if (folders[i].dirty) {

        dlock();
        qsort(folders[i].idx, folders[i].idxct, sizeof(msgrec), bydate);
        folders[i].dirty = FALSE;
        useidx();
        dunlock();
        wrklist = TRUE;

    }
    rehashall();
    if (diag) fprintf(stderr, "fetch: %lld new, %lld already here\n",
                      AMI_LONG_CAST(fetchgot), AMI_LONG_CAST(fetchdup));
    wrkfolds = TRUE;
    wrklist = TRUE;
    wrkdone = TRUE;

}

/* One step: one message, or the move to the next folder. Called from
   the event loop on every tick of the timer, so that everything between
   the steps -- drawing, clicking, closing -- still works. */
void fetchstep(void)

{

    char  tag[20];
    char  line[MAXLINE];
    ami_long  uid;
    ami_long  n;
    char* msg;

    if (wrkdone) return;
    while (fetchi >= uidct) { /* this folder is done */

        if (fetchcur >= 0 && fetchcur < foldct && uidct)
            writestate(fetchcur, uidvalidity,
                       fetchlow? fetchlow: fetchnewlow, fetchlast);
        if (!fetchnext()) { fetchend(); return; }

    }
    uid = uidlist[fetchi++];
    wrkpos = fetchi;
    /* Already here if it falls inside the stretch this folder has been
       read of. Below that stretch is mail older than anything taken so
       far, which is exactly what asking for more of a folder is for. */
    if (fetchlow && uid >= fetchlow && uid <= fetchseen)
        { if (!fetchnewlow || uid < fetchnewlow) fetchnewlow = uid;
          return; }
    imsend(tag, sizeof(tag), "UID FETCH %lld (BODY.PEEK[])", AMI_LONG_CAST(uid));
    /* the reply is a line ending in a literal, then the message itself,
       then the rest of the reply and the tagged answer */
    if (!imline(line, sizeof(line))) {

        /* The server stopped answering. What has been taken is written
           and kept -- the marker for this folder goes down with the
           stretch reached -- and the fetch ends rather than reading a
           socket that is not going to speak. The next look picks it up
           from there. */
        if (fetchcur >= 0 && fetchcur < foldct)
            writestate(fetchcur, uidvalidity,
                       fetchlow? fetchlow: fetchnewlow, fetchlast);
        serverfailed(fetchsrv);
        status("The server stopped answering. What arrived is kept.");
        fetchend();

        return;

    }
    n = literalof(line);
    if (n < 0) { imwait(tag, NULL); return; } /* no message came */
    msg = imliteral(n);
    {

        char hex[DIGLEN];

        /* What the message is, rather than where the server filed it.
           A message already in this store is not written again, whatever
           folder it is in and whichever server it came from -- which is
           what happens when mail is moved here, or fetched twice, or
           arrives in two folders that are views of the same mail, as
           All Mail is of everything. */
        digestof(msg, n, hex);
        if (hasdigest(hex)) {

            free(msg);
            imwait(tag, NULL);
            if (uid > fetchlast) fetchlast = uid;
            if (!fetchnewlow || uid < fetchnewlow) fetchnewlow = uid;
            fetchdup++;

            return;

        }
        /* The message goes into the store, into the folder's index and
           into the table that finds it, with the display shut out of all
           three. This is the one place the worker writes what the main
           thread reads: half a message in a mailbox is what a reader
           would find if it read a folder in the middle of an append. */
        dlock();
        {

            ami_long    off, stored = 0;
            msgrec* m;

            off = mboxwrite(folders[fetchcur].file, msg, n, &stored);
            if (off >= 0) {

                m = idxroom(fetchcur);
                fillrec(m, msg, n, stored, off, hex);
                adddigest(fetchcur, folders[fetchcur].idxct-1);
                /* written down as it arrives, so that a program stopped
                   halfway keeps what it had */
                idxappend(fetchcur, m);
                folders[fetchcur].msgs = folders[fetchcur].idxct;
                folders[fetchcur].dirty = TRUE;
                /* The folder pane is the progress display, its counts
                   climbing as the messages land. The worker does not
                   draw it: it says that a count changed, and the main
                   thread draws that line on the next tick -- that line,
                   not the pane. The pane was drawn
                   whole for every message looked at, stored or not, and
                   a frame shown between a line's white ground and its
                   text made the whole pane blink through every fetch. */
                wrkcounts = TRUE;
                useidx(); /* the array may have moved as it grew */

            }

        }
        dunlock();

    }
    free(msg);
    fetchgot++;
    imwait(tag, NULL); /* the ) and the answer */
    if (uid > fetchlast) fetchlast = uid;
    if (!fetchnewlow || uid < fetchnewlow) fetchnewlow = uid;

}

/* start one */
/* The worker's half of a fetch: everything with a network in it, and
   everything slow enough to be worth keeping off the display. It runs
   on the fetch thread from beginning to end and draws nothing. */
void fetchrun(void)

{

    ami_long i;

    if (wrkrelist) {

        char wasname[MAXSTR];
        ami_long wassrv = -1;

        /* Asking every server what folders it has, and rebuilding the
           table from the answers. The table is what the display reads,
           so the rebuild is done with the display shut out: a pane drawn
           halfway through would be drawn off a list with some servers in
           it and some not.

           This is the one place the display can be held up for as long
           as a server takes to answer, and it is why the timer never
           asks: Get Mail asks, and Get Mail is somebody waiting. */
        dlock();
        wasname[0] = 0;
        /* Keep which folder is being read: rebuilding the list throws
           the selection away, and a fetch that moved the reader back to
           the first folder every quarter minute would be unusable. */
        if (foldsel >= 0 && foldsel < foldct) {

            copystr(wasname, folders[foldsel].name, MAXSTR);
            wassrv = folders[foldsel].srv;

        }
        idxsetaside();
        foldct = 0;
        foldsel = -1;
        for (i = 0; i < srvct; i++) {

            if (!*servers[i].imap || !*servers[i].user) continue;
            /* If the server cannot be reached -- or is one being left
               alone after it stopped answering -- keep what the store
               knows of its folders rather than leaving the section
               empty: an account that is merely unreachable has not
               stopped having folders. */
            if (serverquiet(i)) { storefolders(i); continue; }
            snprintf(wrkwhat, sizeof(wrkwhat), "Asking %s what folders it has",
                     servers[i].name);
            wrkpos = 0;
            wrkmax = 0;
            if (!getfolders(i))
                { imapclose(); serverfailed(i); storefolders(i); continue; }
            imapclose(); /* one at a time: the connection is per server */

        }
        storefolders(-1); /* the locals rejoin, after the servers' */
        idxgiveback();    /* and every folder gets its index back */
        if (*wasname) /* put the reader back on the folder it was reading */
            for (i = 0; i < foldct; i++)
                if (folders[i].srv == wassrv &&
                    !strcmp(folders[i].name, wasname)) { foldsel = i; break; }
        dunlock();
        wrkfolds = TRUE;
        wrklist = TRUE;

    }
    /* Every folder that has not been read is read, and what they hold is
       what the store holds: the counts, and the digests that say whether
       an arriving message is already here. Reading is an index file
       each; only a folder without one costs anything, and only once. */
    countfolders();
    serveindex();
    wrkfolds = TRUE;
    if (wrkcount) return; /* reading the store was the whole job */
    if (diag) fprintf(stderr, "digests known: %lld over %lld folders\n",
                      AMI_LONG_CAST(digct), AMI_LONG_CAST(foldct));
    fetchsrv = -2; /* no server open yet */
    fetchorder();
    fetchfold = -1;
    fetchcur = -1;
    fetchi = 0;
    uidct = 0;
    fetchgot = 0;
    fetchdup = 0;
    /* Straight through, one message after another, with the reads
       blocking as reads do. There is no state machine any more and no
       timer setting the pace: that apparatus existed only to give the
       display a turn between messages, and the display has a thread of
       its own now. */
    while (!wrkdone && !wrkstop) { servesend(); servemove(); serveindex(); fetchstep(); }

}

/* Send whatever the compose window has left to be sent. Between
   messages of a fetch as well as when nothing else is happening, so a
   message written during a long fetch goes now rather than after it. */
void servesend(void)

{

    char to[MAXSTR], cc[MAXSTR], sub[MAXSTR], inreply[MAXSTR];
    char refs[MAXSTR*2];
    char err[MAXSTR*3];
    char* body;

    if (!sendwant) return;
    dlock();
    sendwant = FALSE;
    copystr(to, outto, sizeof(to));
    copystr(cc, outcc, sizeof(cc));
    copystr(sub, outsub, sizeof(sub));
    copystr(inreply, outinreply, sizeof(inreply));
    copystr(refs, outrefs, sizeof(refs));
    body = outbody;
    outbody = NULL;
    dunlock();
    snprintf(wrkwhat, sizeof(wrkwhat), "Sending to %.400s", to);
    wrkpos = 0;
    wrkmax = 0;
    sendmail(to, cc, sub, body? body: "", inreply, refs, err, sizeof(err));
    free(body);
    wrkwhat[0] = 0;
    if (*err) {

        copystr(failsaid, err, sizeof(failsaid));
        failwait = TRUE;
        sendfail = TRUE;

    }
    else copystr(sentsaid, "Message sent, and a copy put in Sent.",
                 sizeof(sentsaid));
    wrkfolds = TRUE;

}

/* Read whatever folder the display has asked for. Called from the idle
   loop and from between messages of a fetch, so that opening a folder
   answers while mail is arriving. */
void serveindex(void)

{

    ami_long i;

    while (!wrkstop) {

        ami_long f = idxwant;

        if (f < 0) { /* nobody is waiting; read whatever wants reading */

            for (i = 0; i < foldct; i++) if (folders[i].wantidx) break;
            if (i >= foldct) break;
            f = i;

        }
        idxwant = -1;
        folders[f].wantidx = FALSE;
        idxdoing = f;
        snprintf(wrkwhat, sizeof(wrkwhat), "Reading %.400s",
                 folders[f].show);
        wrkpos = 0;
        wrkmax = 0;
        indexfolder(f);
        idxdoing = -1;
        wrkwhat[0] = 0;
        wrkpos = 0;
        wrkmax = 0;
        wrklist = TRUE;

    }

}

/* The fetch thread. It is made once and lives as long as the program:
   the services thread table has no way to give an entry back, so a
   thread per fetch would run it out inside an hour of quarter-minute
   looks. It waits for work rather than being made for it. */
void mailwork(void)

{

    while (TRUE) {

        wrkbusy = TRUE;
        servesend();
        servemove(); /* before the index: the move drops it, the reading remakes it */
        serveindex();
        if (wrkgo) {

            wrkdone = FALSE;
            fetchrun();
            wrkdone = TRUE; /* however it ended */
            wrkgo = FALSE;

        }
        wrkbusy = FALSE;
        usleep(50000); /* a twentieth of a second, unnoticeable at a start */

    }

}

/* Set the worker going, and the timer that watches it. Everything that
   wants work done goes through here. */
/* read one message back out of the file, whole */
char* getmsg(ami_long fold, ami_long i)

{

    FILE* f;
    char* buf;

    if (fold < 0 || i < 0 || i >= msgct) return (NULL);
    f = fopen(folders[fold].file, "r");
    if (!f) return (NULL);
    buf = getmem(msgs[i].len+1);
    fseek(f, msgs[i].off, SEEK_SET);
    msgs[i].len = fread(buf, 1, msgs[i].len, f);
    buf[msgs[i].len] = 0;
    fclose(f);

    return (buf);

}

/*******************************************************************************

The search

The form's ask and the worker's answer, as the header says. The worker
takes the ask between its other work, reads any folder it is to search
that has no index yet, and tries every message: the tests that the index
answers first, and the message itself read back only for a survivor that
still has a test to pass. What it finds is a copy of the record and the
folder it was in, published under the lock when the whole search is
done, so the front end never sees a list half made.

*******************************************************************************/

srcrec    srcask;
ami_long  srcwant;
ami_long  srcbusy;
ami_long  srcdone;
msgrec*   srcres;
ami_long* srcfold;
ami_long  srcct;
char      srcwhat[MAXSTR];   /* what the search is doing */
ami_long  srcpos;            /* how far into it */
ami_long  srcmax;
char      srcmissed[MAXSTR*2]; /* the folders it could not look in */

/* read one message back out of a folder, whole, wherever the folder is:
   the record says where in the file it lies */
char* getmsgin(ami_long fold, const msgrec* m)

{

    FILE* f;
    char* buf;
    ami_long n;

    if (fold < 0 || fold >= foldct || !m) return (NULL);
    f = fopen(folders[fold].file, "r");
    if (!f) return (NULL);
    buf = getmem(m->len+1);
    fseek(f, m->off, SEEK_SET);
    n = fread(buf, 1, m->len, f);
    buf[n] = 0;
    fclose(f);

    return (buf);

}

/* Does the message carry an attachment? A part that says so in its
   disposition, or one that is neither text nor a multipart of its own,
   which is a picture or a file going along. The walk is textof's. */
int hasattach(const char* msg, ami_long len)

{

    char  typ[MAXSTR];
    char  bound[MAXSTR];
    char  sep[MAXSTR+8];
    char  disp[MAXSTR];
    const char* p;
    ami_long sl;

    findheader(msg, "Content-Type", typ, sizeof(typ));
    if (strncasecmp(typ, "multipart/", 10)) { /* one part: its own say */

        if (findheader(msg, "Content-Disposition", disp, sizeof(disp)) &&
            !strncasecmp(disp, "attachment", 10)) return (TRUE);
        return (FALSE);

    }
    hdrparam(typ, "boundary", bound, sizeof(bound));
    if (!*bound) return (FALSE);
    snprintf(sep, sizeof(sep), "--%s", bound);
    sl = strlen(sep);
    p = msg;
    while ((p = strstr(p, sep))) {

        const char* s = p+sl;
        const char* e;
        char        ptyp[MAXSTR];
        char        hdr[4000];
        ami_long    n;

        if (s[0] == '-' && s[1] == '-') break; /* the end separator */
        while (*s == '\r') s++;
        if (*s == '\n') s++;
        e = strstr(s, sep);
        if (!e) e = msg+len;
        parttype(s, e-s, ptyp, sizeof(ptyp));
        if (!strncasecmp(ptyp, "multipart/", 10)) {

            if (hasattach(s, e-s)) return (TRUE);

        } else {

            /* the part's own headers, as parttype reads them */
            n = e-s;
            if (n > (ami_long)sizeof(hdr)-1) n = sizeof(hdr)-1;
            memcpy(hdr, s, n);
            hdr[n] = 0;
            if (findheader(hdr, "Content-Disposition", disp, sizeof(disp)) &&
                !strncasecmp(disp, "attachment", 10)) return (TRUE);
            if (*ptyp && strncasecmp(ptyp, "text/", 5)) return (TRUE);

        }
        p = e;

    }

    return (FALSE);

}

/* A day as the form writes it, 2026-09-09 or 9/9/2026, as the time at
   its noon; 0 for none. Noon, so that "within a day" of it takes in the
   whole of the day either side. */
ami_long parseday(const char* s)

{

    int y, m, d;
    struct tm t;

    while (*s == ' ') s++;
    if (sscanf(s, "%d-%d-%d", &y, &m, &d) == 3) ;
    else if (sscanf(s, "%d/%d/%d", &m, &d, &y) == 3) ;
    else return (0);
    if (y < 100) y += 2000;
    if (m < 1 || m > 12 || d < 1 || d > 31) return (0);
    memset(&t, 0, sizeof(t));
    t.tm_year = y-1900;
    t.tm_mon = m-1;
    t.tm_mday = d;
    t.tm_hour = 12;
    t.tm_isdst = -1;

    return ((ami_long)mktime(&t));

}

/* Does the text match the term? Whole, in any case: "ali" is Ali and
   not AliExpress, and not the personalized in a Quora digest's address.
   With the wildcards on, * in the term stands for any run of characters
   and ? for any one, so "ali*" is both and "*@quora.com" is every
   digest. Off, the two are characters like any other. */
static int termeq(const char* term, const char* text, int wild)

{

    if (!wild) return (!strcasecmp(term, text));
    while (*term) {

        if (*term == '*') {

            while (*term == '*') term++;
            if (!*term) return (TRUE); /* a star at the end takes the rest */
            for (; *text; text++) if (termeq(term, text, wild)) return (TRUE);

            return (FALSE);

        }
        if (!*text) return (FALSE);
        if (*term != '?' && tolower((unsigned char)*term) !=
                            tolower((unsigned char)*text)) return (FALSE);
        term++;
        text++;

    }

    return (!*text);

}

/* Does any word of the text match the term? The words are what spaces
   set apart: a comma or a stop stays on its word, and a term that wants
   the word with or without it says so with a star. */
static int wordeq(const char* term, const char* text, int wild)

{

    char w[MAXSTR];
    const char* p = text;
    int n;

    while (*p) {

        while (*p && isspace((unsigned char)*p)) p++;
        n = 0;
        while (*p && !isspace((unsigned char)*p)) {

            if (n < (int)sizeof(w)-1) w[n++] = *p;
            p++;

        }
        w[n] = 0;
        if (n && termeq(term, w, wild)) return (TRUE);

    }

    return (FALSE);

}

/* Does the term match the name or the address of any of the people on
   the line? A To line names several; each is tried whole, then as its
   address and its name alone. */
static int personeq(const char* term, const char* line, int wild)

{

    char one[MAXSTR], part[MAXSTR];
    const char* p = line;
    int n;

    while (*p) {

        while (*p == ' ' || *p == ',') p++;
        n = 0;
        while (*p && *p != ',') { if (n < (int)sizeof(one)-1) one[n++] = *p; p++; }
        one[n] = 0;
        trim(one);
        if (!*one) continue;
        if (termeq(term, one, wild)) return (TRUE);
        addrof(one, part, sizeof(part));
        if (*part && termeq(term, part, wild)) return (TRUE);
        nameof(one, part, sizeof(part));
        if (*part && termeq(term, part, wild)) return (TRUE);

    }

    return (FALSE);

}

/* every word of the list is a word of the text (all) or none is (any) */
static int wordsin(const char* words, const char* text, int wild, int all)

{

    char w[MAXSTR];
    const char* p = words;
    int n, found = FALSE;

    while (*p) {

        while (*p == ' ') p++;
        n = 0;
        while (*p && *p != ' ' && n < (int)sizeof(w)-1) w[n++] = *p++;
        w[n] = 0;
        if (!n) continue;
        found = wordeq(w, text, wild);
        if (all && !found) return (FALSE);
        if (!all && found) return (TRUE);

    }

    return (all);

}

/* one message against the ask; the folder is for reading it back */
static int srcmatch(ami_long fold, const msgrec* m, const srcrec* a)

{

    char* raw = NULL;
    char* text = NULL;
    char  to[MAXSTR];
    char  all[MAXSTR*4+SNIPPET];
    int   ok = TRUE;

    /* what the index answers */
    if (*a->from && !termeq(a->from, m->from, a->wild) &&
        !termeq(a->from, m->addr, a->wild)) return (FALSE);
    if (*a->subject && !termeq(a->subject, m->subject, a->wild)) return (FALSE);
    if (a->sizeop == 1 && m->len <= a->sizeval) return (FALSE);
    if (a->sizeop == 2 && m->len >= a->sizeval) return (FALSE);
    if (a->within && (m->date < a->date-a->within ||
                      m->date > a->date+a->within)) return (FALSE);
    if (!*a->to && !*a->words && !*a->nowords && !a->attach) return (TRUE);
    /* what only the message answers */
    raw = getmsgin(fold, m);
    if (!raw) return (FALSE);
    if (!findheader(raw, "To", to, sizeof(to))) *to = 0;
    if (*a->to && !personeq(a->to, to, a->wild)) ok = FALSE;
    if (ok && a->attach && !hasattach(raw, strlen(raw))) ok = FALSE;
    if (ok && (*a->words || *a->nowords)) {

        /* the words are looked for in the heads and the text alike:
           every word of the one list somewhere, no word of the other */
        text = textof(raw, strlen(raw), 0);
        snprintf(all, sizeof(all), "%s\n%s\n%s\n%s\n", m->from, m->addr, to,
                 m->subject);
        if (*a->words) {

            char w[MAXSTR];
            const char* p = a->words;
            int n;

            while (*p && ok) {

                while (*p == ' ') p++;
                n = 0;
                while (*p && *p != ' ' && n < (int)sizeof(w)-1) w[n++] = *p++;
                w[n] = 0;
                if (n && !wordeq(w, all, a->wild) && !wordeq(w, text, a->wild))
                    ok = FALSE;

            }

        }
        if (ok && *a->nowords && (wordsin(a->nowords, all, a->wild, FALSE) ||
                                   wordsin(a->nowords, text, a->wild, FALSE)))
            ok = FALSE;
        free(text);

    }
    free(raw);

    return (ok);

}

/* The search, on its own thread: the folders asked and every message of
   them tried, and the result published whole at the end.

   The index is the one thing it shares with the fetch, which grows a
   folder's index while this reads it -- and growing can move the array.
   So a folder's entries are copied under the lock and the copy is
   searched. INBOX is the biggest at some tens of megabytes of entries,
   a few milliseconds against a scan that reads messages from the disc.
   A message is read from its mailbox by offset, and a mailbox only ever
   grows, so an entry copied before the fetch appends more is still
   right.

   A folder with no index yet is not read here: indexing is the worker's,
   and a search that took to reading a four gigabyte mailbox would be the
   wait it was made to avoid. Its name is kept for the front end to say. */
void servesearch(void)

{

    srcrec   a;
    msgrec*  res = NULL;
    ami_long* rfold = NULL;
    ami_long ct = 0, max = 0;
    ami_long f, i;
    ami_long f0, f1;
    char     show[MAXSTR];

    if (!srcwant) return;
    dlock(); /* the ask is copied under the lock the form wrote it under */
    a = srcask;
    srcwant = FALSE;
    srcbusy = TRUE;
    srcmissed[0] = 0;
    if (a.fold >= 0 && a.fold < foldct) { f0 = a.fold; f1 = a.fold+1; }
    else { f0 = 0; f1 = foldct; }
    dunlock();
    for (f = f0; f < f1 && !wrkstop && !srcwant; f++) {

        msgrec*  snap;
        ami_long n;

        dlock();
        if (f >= foldct) { dunlock(); break; } /* the table was rebuilt under us */
        copystr(show, folders[f].show, sizeof(show));
        if (!folders[f].idxok) {

            dunlock();
            if (*srcmissed) strncat(srcmissed, ", ", sizeof(srcmissed)-strlen(srcmissed)-1);
            strncat(srcmissed, show, sizeof(srcmissed)-strlen(srcmissed)-1);
            continue;

        }
        n = folders[f].idxct;
        snap = malloc((n? n: 1)*sizeof(msgrec));
        if (!snap) { fprintf(stderr, "Out of memory\n"); exit(1); }
        if (n) memcpy(snap, folders[f].idx, n*sizeof(msgrec));
        dunlock();
        snprintf(srcwhat, sizeof(srcwhat), "Searching %.400s", show);
        srcmax = n;
        for (i = 0; i < n && !wrkstop && !srcwant; i++) {

            srcpos = i+1;
            if (srcmatch(f, &snap[i], &a)) {

                if (ct >= max) {

                    max = max? max*2: 100;
                    res = realloc(res, max*sizeof(msgrec));
                    rfold = realloc(rfold, max*sizeof(ami_long));
                    if (!res || !rfold) { fprintf(stderr, "Out of memory\n"); exit(1); }

                }
                res[ct] = snap[i];
                rfold[ct] = f;
                ct++;

            }

        }
        free(snap);

    }
    srcwhat[0] = 0;
    srcpos = 0;
    srcmax = 0;
    if (srcwant || wrkstop) { /* asked again before this was done: this one is dropped */

        free(res);
        free(rfold);
        srcbusy = FALSE;
        return;

    }
    /* newest first, as the list is */
    qsort(res, ct, sizeof(msgrec), bydate);
    dlock();
    free(srcres);
    free(srcfold);
    srcres = res;
    srcfold = rfold;
    srcct = ct;
    srcbusy = FALSE;
    srcdone = TRUE;
    dunlock();

}

/* The search thread's whole life: a search when one is asked, and a
   nap between looks. It is not the worker's thread, so a search asked
   in the middle of an hour's fetch answers in a second or two. */
void searchwork(void)

{

    while (!wrkstop) {

        if (srcwant) servesearch();
        usleep(50000); /* a twentieth of a second, unnoticeable at a start */

    }

}

/* Can this server be reached at all?

   The library treats a connection it cannot make as an error, and an
   error stops the program: ami_opennet does not come back to say no. A
   mail program meets servers that are down -- a wrong port in a form, a
   host that has moved, a network that is not there -- and it must go on
   meeting them, so the connection is tried here first with a plain
   socket, and the library is only asked for one that is going to work.

   The try is made without blocking and given a few seconds, since the
   whole point is not to hang on a host that is not answering. */
int reachable(const char* host, ami_long port, ami_long secs)

{

    struct addrinfo  hints;
    struct addrinfo* res = NULL;
    struct addrinfo* p;
    char             portstr[16];
    int              ok = FALSE;

    if (!host || !*host) return (FALSE);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* which is what the library speaks */
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%lld", AMI_LONG_CAST(port));
    if (getaddrinfo(host, portstr, &hints, &res)) return (FALSE);
    for (p = res; p && !ok; p = p->ai_next) {

        int            sk = socket(p->ai_family, SOCK_STREAM, 0);
        int            fl;
        struct timeval tv;
        fd_set         wr;

        if (sk < 0) continue;
        fl = fcntl(sk, F_GETFL, 0);
        fcntl(sk, F_SETFL, fl|O_NONBLOCK);
        if (!connect(sk, p->ai_addr, p->ai_addrlen)) ok = TRUE;
        else if (errno == EINPROGRESS) {

            FD_ZERO(&wr);
            FD_SET(sk, &wr);
            tv.tv_sec = secs;
            tv.tv_usec = 0;
            if (select(sk+1, NULL, &wr, NULL, &tv) > 0) {

                int       e = 0;
                socklen_t el = sizeof(e);

                if (!getsockopt(sk, SOL_SOCKET, SO_ERROR, &e, &el) && !e)
                    ok = TRUE;

            }

        }
        close(sk);

    }
    freeaddrinfo(res);
    if (!ok && diag) fprintf(stderr, "! cannot reach %s port %lld\n", host, AMI_LONG_CAST(port));

    return (ok);

}

/* Something kept beside the program: the categories, the help, the
   picture. Looked for where the program is, then where the source is,
   so that it works run from anywhere and from the build directory. */
int resfile(const char* leaf, char* path, ami_long pl)

{

    char  dir[MAXSTR];
    char* e;
    FILE* f;
    ami_long  i;

    dir[0] = 0;
    if (mailprog) {

        snprintf(dir, sizeof(dir), "%s", mailprog);
        e = strrchr(dir, '/');
        if (e) e[1] = 0; else dir[0] = 0;

    }
    for (i = 0; i < 4; i++) {

        if (i == 0) snprintf(path, pl, "%s%s", dir, leaf);
        else if (i == 1) snprintf(path, pl, "%s../graph_programs/%s", dir, leaf);
        else if (i == 2) snprintf(path, pl, "%s", leaf);
        else snprintf(path, pl, "graph_programs/%s", leaf);
        f = fopen(path, "r");
        if (f) { fclose(f); return (TRUE); }

    }

    return (FALSE);

}

/* The list the display draws is the selected folder's index -- not a
   copy of it, and not one built for the purpose. This points the two at
   each other, and is called wherever either can move: the array is
   grown as mail arrives, and growing it can move it. Every caller holds
   the lock, which is what makes that safe. */
void useidx(void)

{

    if (foldsel >= 0 && foldsel < foldct) {

        msgs = folders[foldsel].idx;
        msgct = folders[foldsel].idxct;

    } else { msgs = NULL; msgct = 0; }

}
