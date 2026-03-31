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

    ami_atexec, /* is an executable file type */
    ami_atarc,  /* has been archived since last modification */
    ami_atsys,  /* is a system special file */
    ami_atdir,  /* is a directory special file */
    ami_atloop  /* contains heriarchy loop */

} ami_attribute;
typedef long ami_attrset; /* attributes in a set */

/* permissions */
typedef enum {

    ami_pmread,  /* may be read */
    ami_pmwrite, /* may be written */
    ami_pmexec,  /* may be executed */
    ami_pmdel,   /* may be deleted */
    ami_pmvis,   /* may be seen in directory listings */
    ami_pmcopy,  /* may be copied */
    ami_pmren    /* may be renamed/moved */

} ami_permission;
typedef long ami_permset; /* permissions in a set */

/* standard directory format */
typedef struct ami_filrec {

    char*              name;    /* name of file (zero terminated) */
    long               namel;   /* length of filename */
    long long          size;    /* size of file */
    long long          alloc;   /* allocation of file */
    ami_attrset         attr;    /* attributes */
    long               create;  /* time of creation */
    long               modify;  /* time of last modification */
    long               access;  /* time of last access */
    long               backup;  /* time of last backup */
    ami_permset         user;    /* user permissions */
    ami_permset         group;   /* group permissions */
    ami_permset         other;   /* other permissions */
    struct ami_filrec*  next;    /* next entry in list */

} ami_filrec;
typedef ami_filrec* ami_filptr; /* pointer to file records */

/* environment strings */
typedef struct ami_envrec {

    char* name;    /* name of string (zero terminated) */
    char* data;    /* data in string (zero terminated) */
    struct ami_envrec *next; /* next entry in list */

} ami_envrec;
typedef ami_envrec* ami_envptr; /* pointer to environment record */

/* character set */
typedef unsigned char ami_chrset[CSETLEN];

/*
 * Functions exposed in the services module
 */
extern void ami_list(char* f, ami_filrec **lp);
extern void ami_listl(char* f, int l, ami_filrec **lp);
extern void ami_times(char* s, int sl, int t);
extern void ami_dates(char* s, int sl, int t);
extern void ami_writetime(FILE *f, int t);
extern void ami_writedate(FILE *f, int t);
extern long ami_time(void);
extern long ami_local(long t);
extern long ami_clock(void);
extern long ami_elapsed(long r);
extern int  ami_validfile(char* s);
extern int  ami_validfilel(char* s, int l);
extern int  ami_validpath(char* s);
extern int  ami_validpathl(char* s, int l);
extern int  ami_wild(char* s);
extern int  ami_wildl(char* s, int l);
extern void ami_getenv(char* ls, char* ds, int dsl);
extern void ami_getenvl(char* ls, int lsl, char* ds, int dsl);
extern void ami_setenv(char* sn, char* sd);
extern void ami_setenvl(char* sn, int snl, char* sd, int sdl);
extern void ami_allenv(ami_envrec **el);
extern void ami_remenv(char* sn);
extern void ami_remenvl(char* sn, int snl);
extern void ami_exec(char* cmd);
extern void ami_execl(char* cmd, int cmdl);
extern void ami_exece(char* cmd, ami_envrec *el);
extern void ami_execel(char* cmd, int cmdl, ami_envrec *el);
extern void ami_execw(char* cmd, int *e);
extern void ami_execwl(char* cmd, int cmdl, int *e);
extern void ami_execew(char* cmd, ami_envrec *el, int *e);
extern void ami_execewl(char* cmd, int cmdl, ami_envrec *el, int *e);
extern void ami_getcur(char* fn, int l);
extern void ami_setcur(char* fn);
extern void ami_setcurl(char* fn, int fnl);
extern void ami_brknam(char* fn, char* p, int pl, char* n, int nl, char* e, int el);
extern void ami_brknaml(char* fn, int fnl, char* p, int pl, char* n, int nl, char* e, int el);
extern void ami_maknam(char* fn, int fnl, char* p, char* n, char* e);
extern void ami_maknaml(char* fn, int fnl, char* p, int pl, char* n, int nl, char* e, int el);
extern void ami_fulnam(char* fn, int fnl);
extern void ami_getpgm(char* p, int pl);
extern void ami_getusr(char* fn, int fnl);
extern void ami_setatr(char* fn, ami_attrset a);
extern void ami_setatrl(char* fn, int fnl, ami_attrset a);
extern void ami_resatr(char* fn, ami_attrset a);
extern void ami_resatrl(char* fn, int fnl, ami_attrset a);
extern void ami_bakupd(char* fn);
extern void ami_bakupdl(char* fn, int fnl);
extern void ami_setuper(char* fn, ami_permset p);
extern void ami_setuperl(char* fn, int fnl, ami_permset p);
extern void ami_resuper(char* fn, ami_permset p);
extern void ami_resuperl(char* fn, int fnl, ami_permset p);
extern void ami_setgper(char* fn, ami_permset p);
extern void ami_setgperl(char* fn, int fnl, ami_permset p);
extern void ami_resgper(char* fn, ami_permset p);
extern void ami_resgperl(char* fn, int fnl, ami_permset p);
extern void ami_setoper(char* fn, ami_permset p);
extern void ami_setoperl(char* fn, int fnl, ami_permset p);
extern void ami_resoper(char* fn, ami_permset p);
extern void ami_resoperl(char* fn, int fnl, ami_permset p);
extern void ami_makpth(char* fn);
extern void ami_makpthl(char* fn, int fnl);
extern void ami_rempth(char* fn);
extern void ami_rempthl(char* fn, int fnl);
extern void ami_filchr(ami_chrset fc);
extern char ami_optchr(void);
extern char ami_pthchr(void);
extern int  ami_latitude(void);
extern int  ami_longitude(void);
extern int  ami_altitude(void);
extern int  ami_country(void);
extern void ami_countrys(char* s, int sl, int c);
extern int  ami_timezone(void);
extern int  ami_daysave(void);
extern int  ami_time24hour(void);
extern int  ami_language(void);
extern void ami_languages(char* s, int sl, int l);
extern char ami_decimal(void);
extern char ami_numbersep(void);
extern int  ami_timeorder(void);
extern int  ami_dateorder(void);
extern char ami_datesep(void);
extern char ami_timesep(void);
extern char ami_currchr(void);
extern int ami_newthread(void (*threadmain)(void));
extern int ami_initlock(void);
extern void ami_deinitlock(int ln);
extern void ami_lock(int ln);
extern void ami_unlock(int ln);
extern int ami_initsig(void);
extern void ami_deinitsig(int sn);
extern void ami_sendsig(int sn);
extern void ami_sendsigone(int sn);
extern void ami_waitsig(int ln, int sn);

#ifdef __cplusplus
}
#endif

#endif
