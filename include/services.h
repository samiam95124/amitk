/*******************************************************************************
*                                                                              *
*                         Services Library Header                              *
*                                                                              *
*                              Created 1996                                    *
*                                                                              *
*                               S. A. MOORE                                    *
*                                                                              *
*******************************************************************************/

#ifndef __SERVICES_H__
#define __SERVICES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define BIT(b) (1<<b) /* set bit from bit number */
#define BITMSK(b) (~BIT(b)) /* mask out bit number */

/*
 * Set manipulation operators for chrset.
 *
 * These are used to change the character set that defines what characters
 * are admissible in filenames.
 */
#define CSETLEN 32 /* length of char set */
#define INCSET(s, b) (!!(s[b>>3] & BIT(b%8))) /* test inclusion */
#define ADDCSET(s, b) (s[b>>3] |= BIT(b%8)) /* add set member */
#define SUBCSET(s, b) (s[b>>3] &= BITMSK(b%8)) /* remove set member */
#define CLRCSET(s) { int i; for (i = 0; i < SETLEN; i++) s[i] = 0; } /* clear set */

/*
 * Set manipulation operators for integer sets.
 *
 * These are used to change the character set that defines what characters
 * are admissible in filenames.
 */
#define INISET(s, b) (!!(s & BIT(b%8))) /* test inclusion */
#define ADDISET(s, b) (s |= BIT(b%8)) /* add set member */
#define SUBISET(s, b) (s &= BITMSK(b%8)) /* remove set member */

/*
 * Coining configuration for external names.
 *
 * SERVICES_NOCOIN  - No prefix (e.g., list, times, time)
 * SERVICES_PACOIN  - pa_ prefix (e.g., pa_list, pa_times, pa_time) [default]
 * SERVICES_MODCOIN - Module prefix (e.g., services_list, services_times)
 *
 * If no coining option is specified, pa_ coining is used as the default.
 */

/* set default coining if none specified */
#if !defined(SERVICES_NOCOIN) && !defined(SERVICES_MODCOIN) && !defined(SERVICES_PACOIN)
#define SERVICES_PACOIN
#endif

/* token pasting helpers */
#define SERVICES_JOIN_(a, b) a##b
#define SERVICES_JOIN(a, b) SERVICES_JOIN_(a, b)

#if defined(SERVICES_NOCOIN)
#define SVCFN(name) name
#elif defined(SERVICES_MODCOIN)
#define SVCFN(name) SERVICES_JOIN(services_, name)
#else /* SERVICES_PACOIN - default */
#define SVCFN(name) SERVICES_JOIN(pa_, name)
#endif

/* coin type names */
#define attribute     SVCFN(attribute)
#define attrset       SVCFN(attrset)
#define permission    SVCFN(permission)
#define permset       SVCFN(permset)
#define filrec        SVCFN(filrec)
#define filptr        SVCFN(filptr)
#define envrec        SVCFN(envrec)
#define envptr        SVCFN(envptr)
#define chrset        SVCFN(chrset)

/* coin enum value names */
#define atexec        SVCFN(atexec)
#define atarc         SVCFN(atarc)
#define atsys         SVCFN(atsys)
#define atdir         SVCFN(atdir)
#define atloop        SVCFN(atloop)
#define pmread        SVCFN(pmread)
#define pmwrite       SVCFN(pmwrite)
#define pmexec        SVCFN(pmexec)
#define pmdel         SVCFN(pmdel)
#define pmvis         SVCFN(pmvis)
#define pmcopy        SVCFN(pmcopy)
#define pmren         SVCFN(pmren)

/* coin function names */
#define list          SVCFN(list)
#define listl         SVCFN(listl)
#define times         SVCFN(times)
#define dates         SVCFN(dates)
#define writetime     SVCFN(writetime)
#define writedate     SVCFN(writedate)
#define time          SVCFN(time)
#define local         SVCFN(local)
#define clock         SVCFN(clock)
#define elapsed       SVCFN(elapsed)
#define validfile     SVCFN(validfile)
#define validfilel    SVCFN(validfilel)
#define validpath     SVCFN(validpath)
#define validpathl    SVCFN(validpathl)
#define wild          SVCFN(wild)
#define wildl         SVCFN(wildl)
#define getenv        SVCFN(getenv)
#define getenvl       SVCFN(getenvl)
#define setenv        SVCFN(setenv)
#define setenvl       SVCFN(setenvl)
#define remenv        SVCFN(remenv)
#define remenvl       SVCFN(remenvl)
#define allenv        SVCFN(allenv)
#define exec          SVCFN(exec)
#define execl         SVCFN(execl)
#define execw         SVCFN(execw)
#define execwl        SVCFN(execwl)
#define exece         SVCFN(exece)
#define execel        SVCFN(execel)
#define execew        SVCFN(execew)
#define execewl       SVCFN(execewl)
#define getcur        SVCFN(getcur)
#define setcur        SVCFN(setcur)
#define setcurl       SVCFN(setcurl)
#define brknam        SVCFN(brknam)
#define brknaml       SVCFN(brknaml)
#define maknam        SVCFN(maknam)
#define maknaml       SVCFN(maknaml)
#define fulnam        SVCFN(fulnam)
#define getpgm        SVCFN(getpgm)
#define getusr        SVCFN(getusr)
#define setatr        SVCFN(setatr)
#define setatrl       SVCFN(setatrl)
#define resatr        SVCFN(resatr)
#define resatrl       SVCFN(resatrl)
#define bakupd        SVCFN(bakupd)
#define bakupdl       SVCFN(bakupdl)
#define setuper       SVCFN(setuper)
#define setuperl      SVCFN(setuperl)
#define resuper       SVCFN(resuper)
#define resuperl      SVCFN(resuperl)
#define setgper       SVCFN(setgper)
#define setgperl      SVCFN(setgperl)
#define resgper       SVCFN(resgper)
#define resgperl      SVCFN(resgperl)
#define setoper       SVCFN(setoper)
#define setoperl      SVCFN(setoperl)
#define resoper       SVCFN(resoper)
#define resoperl      SVCFN(resoperl)
#define makpth        SVCFN(makpth)
#define makpthl       SVCFN(makpthl)
#define rempth        SVCFN(rempth)
#define rempthl       SVCFN(rempthl)
#define filchr        SVCFN(filchr)
#define optchr        SVCFN(optchr)
#define pthchr        SVCFN(pthchr)
#define latitude      SVCFN(latitude)
#define longitude     SVCFN(longitude)
#define altitude      SVCFN(altitude)
#define country       SVCFN(country)
#define countrys      SVCFN(countrys)
#define timezone      SVCFN(timezone)
#define daysave       SVCFN(daysave)
#define time24hour    SVCFN(time24hour)
#define language      SVCFN(language)
#define languages     SVCFN(languages)
#define decimal       SVCFN(decimal)
#define numbersep     SVCFN(numbersep)
#define timeorder     SVCFN(timeorder)
#define dateorder     SVCFN(dateorder)
#define datesep       SVCFN(datesep)
#define timesep       SVCFN(timesep)
#define currchr       SVCFN(currchr)
#define newthread     SVCFN(newthread)
#define initlock      SVCFN(initlock)
#define deinitlock    SVCFN(deinitlock)
#define lock          SVCFN(lock)
#define unlock        SVCFN(unlock)
#define initsig       SVCFN(initsig)
#define deinitsig     SVCFN(deinitsig)
#define sendsig       SVCFN(sendsig)
#define sendsigone    SVCFN(sendsigone)
#define waitsig       SVCFN(waitsig)

/* attributes */
typedef enum {

    atexec, /* is an executable file type */
    atarc,  /* has been archived since last modification */
    atsys,  /* is a system special file */
    atdir,  /* is a directory special file */
    atloop  /* contains heriarchy loop */

} attribute;
typedef long attrset; /* attributes in a set */

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
typedef long permset; /* permissions in a set */

/* standard directory format */
typedef struct filrec {

    char*           name;    /* name of file (zero terminated) */
    long            namel;   /* length of filename */
    long long       size;    /* size of file */
    long long       alloc;   /* allocation of file */
    attrset         attr;    /* attributes */
    long            create;  /* time of creation */
    long            modify;  /* time of last modification */
    long            access;  /* time of last access */
    long            backup;  /* time of last backup */
    permset         user;    /* user permissions */
    permset         group;   /* group permissions */
    permset         other;   /* other permissions */
    struct filrec*  next;    /* next entry in list */

} filrec;
typedef filrec* filptr; /* pointer to file records */

/* environment strings */
typedef struct envrec {

    char* name;    /* name of string (zero terminated) */
    char* data;    /* data in string (zero terminated) */
    struct envrec *next; /* next entry in list */

} envrec;
typedef envrec* envptr; /* pointer to environment record */

/* character set */
typedef unsigned char chrset[CSETLEN];

/*
 * Functions exposed in the services module
 */
extern void list(char* f, filrec **lp);
extern void listl(char* f, int l, filrec **lp);
extern void times(char* s, int sl, int t);
extern void dates(char* s, int sl, int t);
extern void writetime(FILE *f, int t);
extern void writedate(FILE *f, int t);
extern long time(void);
extern long local(long t);
extern long clock(void);
extern long elapsed(long r);
extern int  validfile(char* s);
extern int  validfilel(char* s, int l);
extern int  validpath(char* s);
extern int  validpathl(char* s, int l);
extern int  wild(char* s);
extern int  wildl(char* s, int l);
extern void getenv(char* ls, char* ds, int dsl);
extern void getenvl(char* ls, int lsl, char* ds, int dsl);
extern void setenv(char* sn, char* sd);
extern void setenvl(char* sn, int snl, char* sd, int sdl);
extern void allenv(envrec **el);
extern void remenv(char* sn);
extern void remenvl(char* sn, int snl);
extern void exec(char* cmd);
extern void execl(char* cmd, int cmdl);
extern void exece(char* cmd, envrec *el);
extern void execel(char* cmd, int cmdl, envrec *el);
extern void execw(char* cmd, int *e);
extern void execwl(char* cmd, int cmdl, int *e);
extern void execew(char* cmd, envrec *el, int *e);
extern void execewl(char* cmd, int cmdl, envrec *el, int *e);
extern void getcur(char* fn, int l);
extern void setcur(char* fn);
extern void setcurl(char* fn, int fnl);
extern void brknam(char* fn, char* p, int pl, char* n, int nl, char* e, int el);
extern void brknaml(char* fn, int fnl, char* p, int pl, char* n, int nl, char* e, int el);
extern void maknam(char* fn, int fnl, char* p, char* n, char* e);
extern void maknaml(char* fn, int fnl, char* p, int pl, char* n, int nl, char* e, int el);
extern void fulnam(char* fn, int fnl);
extern void getpgm(char* p, int pl);
extern void getusr(char* fn, int fnl);
extern void setatr(char* fn, attrset a);
extern void setatrl(char* fn, int fnl, attrset a);
extern void resatr(char* fn, attrset a);
extern void resatrl(char* fn, int fnl, attrset a);
extern void bakupd(char* fn);
extern void bakupdl(char* fn, int fnl);
extern void setuper(char* fn, permset p);
extern void setuperl(char* fn, int fnl, permset p);
extern void resuper(char* fn, permset p);
extern void resuperl(char* fn, int fnl, permset p);
extern void setgper(char* fn, permset p);
extern void setgperl(char* fn, int fnl, permset p);
extern void resgper(char* fn, permset p);
extern void resgperl(char* fn, int fnl, permset p);
extern void setoper(char* fn, permset p);
extern void setoperl(char* fn, int fnl, permset p);
extern void resoper(char* fn, permset p);
extern void resoperl(char* fn, int fnl, permset p);
extern void makpth(char* fn);
extern void makpthl(char* fn, int fnl);
extern void rempth(char* fn);
extern void rempthl(char* fn, int fnl);
extern void filchr(chrset fc);
extern char optchr(void);
extern char pthchr(void);
extern int  latitude(void);
extern int  longitude(void);
extern int  altitude(void);
extern int  country(void);
extern void countrys(char* s, int sl, int c);
extern int  timezone(void);
extern int  daysave(void);
extern int  time24hour(void);
extern int  language(void);
extern void languages(char* s, int sl, int l);
extern char decimal(void);
extern char numbersep(void);
extern int  timeorder(void);
extern int  dateorder(void);
extern char datesep(void);
extern char timesep(void);
extern char currchr(void);
extern int newthread(void (*threadmain)(void));
extern int initlock(void);
extern void deinitlock(int ln);
extern void lock(int ln);
extern void unlock(int ln);
extern int initsig(void);
extern void deinitsig(int sn);
extern void sendsig(int sn);
extern void sendsigone(int sn);
extern void waitsig(int ln, int sn);

#ifdef __cplusplus
}
#endif

#endif
