/** ****************************************************************************
 *
 * Services library interface C++ wrapper declarations
 *
 * See cpp/services.cpp for the rationale. The types of the C header
 * appear here with the ami_ prefixes stripped, laid out identically to
 * their C forms: the wrapper casts between them.
 *
 ******************************************************************************/

#ifndef __SERVICES_HPP__
#define __SERVICES_HPP__

extern "C" {

#include <stdio.h>
#include <localdefs.h>

}

namespace services {

/* length of character set */
const ami_long csetlen = 32;

/* attributes */
typedef enum {

    atexec, /* is an executable file type */
    atarc,  /* has been archived since last modification */
    atsys,  /* is a system special file */
    atdir,  /* is a directory special file */
    atloop  /* contains heriarchy loop */

} attribute;
typedef ami_long attrset; /* attributes in a set */

/* permissions */
typedef enum {

    pmread,  /* may be read */
    pmwrite, /* may be written */
    pmexec,  /* may be executed */
    pmdel,   /* may be deleted */
    pmvis,   /* may be seen in directory listings */
    pmcopy,  /* may be copied */
    pmren    /* may be renamed/moved */

} permission;
typedef ami_long permset; /* permissions in a set */

/* standard directory format */
typedef struct filrec {

    char*          name;    /* name of file (zero terminated) */
    ami_long       namel;   /* length of filename */
    long long      size;    /* size of file */
    long long      alloc;   /* allocation of file */
    attrset        attr;    /* attributes */
    ami_long       create;  /* time of creation */
    ami_long       modify;  /* time of last modification */
    ami_long       access;  /* time of last access */
    ami_long       backup;  /* time of last backup */
    permset        user;    /* user permissions */
    permset        group;   /* group permissions */
    permset        other;   /* other permissions */
    struct filrec* next;    /* next entry in list */

} filrec;
typedef filrec* filptr; /* pointer to file records */

/* environment strings */
typedef struct envrec {

    char* name;          /* name of string (zero terminated) */
    char* data;          /* data in string (zero terminated) */
    struct envrec *next; /* next entry in list */

} envrec;
typedef envrec* envptr; /* pointer to environment record */

/* character set */
typedef unsigned char chrset[csetlen];

/* the set operators of the C header, as inline functions */
inline ami_long bit(ami_long b) { return 1l<<b; }
inline ami_long incset(const unsigned char* s, ami_long b)
    { return !!(s[b>>3] & bit(b%8)); }
inline void addcset(unsigned char* s, ami_long b) { s[b>>3] |= bit(b%8); }
inline void subcset(unsigned char* s, ami_long b) { s[b>>3] &= ~bit(b%8); }
inline void clrcset(unsigned char* s)
    { for (ami_long i = 0; i < csetlen; i++) s[i] = 0; }
inline ami_long iniset(ami_long s, ami_long b) { return !!(s & bit(b)); }
inline ami_long addiset(ami_long s, ami_long b) { return s | bit(b); }
inline ami_long subiset(ami_long s, ami_long b) { return s & ~bit(b); }

/* procedures and functions */
void list(const char* f, filptr* lp);
void times(char* s, ami_long sl, ami_long t);
void dates(char* s, ami_long sl, ami_long t);
void writetime(FILE* f, ami_long t);
void writedate(FILE* f, ami_long t);
ami_long time(void);
ami_long local(ami_long t);
ami_long clock(void);
ami_long elapsed(ami_long r);
ami_long validfile(const char* s);
ami_long validpath(const char* s);
ami_long wild(const char* s);
void getenv(const char* ls, char* ds, ami_long dsl);
void setenv(const char* sn, const char* sd);
void allenv(envptr* el);
void remenv(const char* sn);
void exec(const char* cmd);
void exece(const char* cmd, envptr el);
void execw(const char* cmd, ami_long* e);
void execew(const char* cmd, envptr el, ami_long* e);
void getcur(char* fn, ami_long l);
void setcur(const char* fn);
void brknam(const char* fn, char* p, ami_long pl, char* n, ami_long nl, char* e, ami_long el);
void maknam(char* fn, ami_long fnl, const char* p, const char* n, const char* e);
void fulnam(char* fn, ami_long fnl);
void getpgm(char* p, ami_long pl);
void getusr(char* fn, ami_long fnl);
void setatr(const char* fn, attrset at);
void resatr(const char* fn, attrset at);
void bakupd(const char* fn);
void setuper(const char* fn, permset p);
void resuper(const char* fn, permset p);
void setgper(const char* fn, permset p);
void resgper(const char* fn, permset p);
void setoper(const char* fn, permset p);
void resoper(const char* fn, permset p);
void makpth(const char* fn);
void rempth(const char* fn);
void filchr(chrset fc);
char optchr(void);
char pthchr(void);
ami_long latitude(void);
ami_long longitude(void);
ami_long altitude(void);
ami_long country(void);
void countrys(char* s, ami_long sl, ami_long c);
ami_long timezone(void);
ami_long daysave(void);
ami_long time24hour(void);
ami_long language(void);
void languages(char* s, ami_long sl, ami_long l);
char decimal(void);
char numbersep(void);
ami_long timeorder(void);
ami_long dateorder(void);
char datesep(void);
char timesep(void);
char currchr(void);
ami_long newthread(void (*threadmain)(void));
ami_long initlock(void);
void deinitlock(ami_long ln);
void lock(ami_long ln);
void unlock(ami_long ln);
ami_long initsig(void);
void deinitsig(ami_long sn);
void sendsig(ami_long sn);
void sendsigone(ami_long sn);
void waitsig(ami_long ln, ami_long sn);

/*******************************************************************************

The thread, mutex and signal objects

A thread starts on construction and runs the function it was given; the
services interface has no join or kill, so the object is the start and
the identifier. A mutex is made by its constructor and freed by its
destructor, and locks and unlocks by the C verbs: m.lock(), m.unlock().
The class is called mutex rather than lock only because C++ will not
let a class and its method share a name. A signal likewise lives from
constructor to destructor, sends with sendsig() or sendsigone(), and
waits on a mutex with waitsig(m).

These hold identifiers, not hooks: any number of each may exist.

*******************************************************************************/

class thread {

ami_long tid; /* the thread identifier */

public:

/* constructor: the thread starts here */
thread(void (*threadmain)(void));

/* copying is refused: a thread is one act, not two */
thread(const thread&) = delete;
thread& operator=(const thread&) = delete;

}; /* class thread */

class mutex {

friend class signal;

ami_long ln; /* the lock identifier */

public:

/* constructor */
mutex();

/* destructor */
~mutex();

/* copying is refused: two objects would free one lock */
mutex(const mutex&) = delete;
mutex& operator=(const mutex&) = delete;

/* methods */
void lock(void);
void unlock(void);

}; /* class mutex */

class signal {

ami_long sn; /* the signal identifier */

public:

/* constructor */
signal();

/* destructor */
~signal();

/* copying is refused: two objects would free one signal */
signal(const signal&) = delete;
signal& operator=(const signal&) = delete;

/* methods */
void sendsig(void);
void sendsigone(void);
void waitsig(mutex& m);

}; /* class signal */

} /* namespace services */

#endif /* __SERVICES_HPP__ */
