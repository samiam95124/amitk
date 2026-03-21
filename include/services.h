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

/* attributes */
typedef enum {

    services_atexec, /* is an executable file type */
    services_atarc,  /* has been archived since last modification */
    services_atsys,  /* is a system special file */
    services_atdir,  /* is a directory special file */
    services_atloop  /* contains heriarchy loop */

} services_attribute;
typedef long services_attrset; /* attributes in a set */

/* permissions */
typedef enum {

    services_pmread,  /* may be read */
    services_pmwrite, /* may be written */
    services_pmexec,  /* may be executed */
    services_pmdel,   /* may be deleted */
    services_pmvis,   /* may be seen in directory listings */
    services_pmcopy,  /* may be copied */
    services_pmren    /* may be renamed/moved */

} services_permission;
typedef long services_permset; /* permissions in a set */

/* standard directory format */
typedef struct services_filrec {

    char*                    name;    /* name of file (zero terminated) */
    long                     namel;   /* length of filename */
    long long                size;    /* size of file */
    long long                alloc;   /* allocation of file */
    services_attrset         attr;    /* attributes */
    long                     create;  /* time of creation */
    long                     modify;  /* time of last modification */
    long                     access;  /* time of last access */
    long                     backup;  /* time of last backup */
    services_permset         user;    /* user permissions */
    services_permset         group;   /* group permissions */
    services_permset         other;   /* other permissions */
    struct services_filrec*  next;    /* next entry in list */

} services_filrec;
typedef services_filrec* services_filptr; /* pointer to file records */

/* environment strings */
typedef struct services_envrec {

    char* name;    /* name of string (zero terminated) */
    char* data;    /* data in string (zero terminated) */
    struct services_envrec *next; /* next entry in list */

} services_envrec;
typedef services_envrec* services_envptr; /* pointer to environment record */

/* character set */
typedef unsigned char services_chrset[CSETLEN];

/*
 * Functions exposed in the services module
 */
extern void services_list(char* f, services_filrec **lp);
extern void services_listl(char* f, int l, services_filrec **lp);
extern void services_times(char* s, int sl, int t);
extern void services_dates(char* s, int sl, int t);
extern void services_writetime(FILE *f, int t);
extern void services_writedate(FILE *f, int t);
extern long services_time(void);
extern long services_local(long t);
extern long services_clock(void);
extern long services_elapsed(long r);
extern int  services_validfile(char* s);
extern int  services_validfilel(char* s, int l);
extern int  services_validpath(char* s);
extern int  services_validpathl(char* s, int l);
extern int  services_wild(char* s);
extern int  services_wildl(char* s, int l);
extern void services_getenv(char* ls, char* ds, int dsl);
extern void services_getenvl(char* ls, int lsl, char* ds, int dsl);
extern void services_setenv(char* sn, char* sd);
extern void services_setenvl(char* sn, int snl, char* sd, int sdl);
extern void services_allenv(services_envrec **el);
extern void services_remenv(char* sn);
extern void services_remenvl(char* sn, int snl);
extern void services_exec(char* cmd);
extern void services_execl(char* cmd, int cmdl);
extern void services_exece(char* cmd, services_envrec *el);
extern void services_execel(char* cmd, int cmdl, services_envrec *el);
extern void services_execw(char* cmd, int *e);
extern void services_execwl(char* cmd, int cmdl, int *e);
extern void services_execew(char* cmd, services_envrec *el, int *e);
extern void services_execewl(char* cmd, int cmdl, services_envrec *el, int *e);
extern void services_getcur(char* fn, int l);
extern void services_setcur(char* fn);
extern void services_setcurl(char* fn, int fnl);
extern void services_brknam(char* fn, char* p, int pl, char* n, int nl, char* e, int el);
extern void services_brknaml(char* fn, int fnl, char* p, int pl, char* n, int nl, char* e, int el);
extern void services_maknam(char* fn, int fnl, char* p, char* n, char* e);
extern void services_maknaml(char* fn, int fnl, char* p, int pl, char* n, int nl, char* e, int el);
extern void services_fulnam(char* fn, int fnl);
extern void services_getpgm(char* p, int pl);
extern void services_getusr(char* fn, int fnl);
extern void services_setatr(char* fn, services_attrset a);
extern void services_setatrl(char* fn, int fnl, services_attrset a);
extern void services_resatr(char* fn, services_attrset a);
extern void services_resatrl(char* fn, int fnl, services_attrset a);
extern void services_bakupd(char* fn);
extern void services_bakupdl(char* fn, int fnl);
extern void services_setuper(char* fn, services_permset p);
extern void services_setuperl(char* fn, int fnl, services_permset p);
extern void services_resuper(char* fn, services_permset p);
extern void services_resuperl(char* fn, int fnl, services_permset p);
extern void services_setgper(char* fn, services_permset p);
extern void services_setgperl(char* fn, int fnl, services_permset p);
extern void services_resgper(char* fn, services_permset p);
extern void services_resgperl(char* fn, int fnl, services_permset p);
extern void services_setoper(char* fn, services_permset p);
extern void services_setoperl(char* fn, int fnl, services_permset p);
extern void services_resoper(char* fn, services_permset p);
extern void services_resoperl(char* fn, int fnl, services_permset p);
extern void services_makpth(char* fn);
extern void services_makpthl(char* fn, int fnl);
extern void services_rempth(char* fn);
extern void services_rempthl(char* fn, int fnl);
extern void services_filchr(services_chrset fc);
extern char services_optchr(void);
extern char services_pthchr(void);
extern int  services_latitude(void);
extern int  services_longitude(void);
extern int  services_altitude(void);
extern int  services_country(void);
extern void services_countrys(char* s, int sl, int c);
extern int  services_timezone(void);
extern int  services_daysave(void);
extern int  services_time24hour(void);
extern int  services_language(void);
extern void services_languages(char* s, int sl, int l);
extern char services_decimal(void);
extern char services_numbersep(void);
extern int  services_timeorder(void);
extern int  services_dateorder(void);
extern char services_datesep(void);
extern char services_timesep(void);
extern char services_currchr(void);
extern int services_newthread(void (*threadmain)(void));
extern int services_initlock(void);
extern void services_deinitlock(int ln);
extern void services_lock(int ln);
extern void services_unlock(int ln);
extern int services_initsig(void);
extern void services_deinitsig(int sn);
extern void services_sendsig(int sn);
extern void services_sendsigone(int sn);
extern void services_waitsig(int ln, int sn);

#ifdef __cplusplus
}
#endif

#endif
