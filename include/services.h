/*******************************************************************************
*                                                                              *
*                         Services Library Header                              *
*                                                                              *
*                              Created 1996                                    *
*                                                                              *
*                               S. A. MOORE                                    *
*                                                                              *
* String output buffers are "critical": a result may occupy the entire        *
* buffer, in which case the terminating zero is left off. Results shorter     *
* than the buffer are zero terminated. C callers should use length limited    *
* reads of such buffers (see strnlen and similar).                            *
*                                                                              *
*******************************************************************************/

#ifndef __SERVICES_H__
#define __SERVICES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <localdefs.h>

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
#define CLRCSET(s) { ami_long i; for (i = 0; i < SETLEN; i++) s[i] = 0; } /* clear set */

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
typedef ami_long ami_attrset; /* attributes in a set */

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
typedef ami_long ami_permset; /* permissions in a set */

/* standard directory format */
typedef struct ami_filrec {

    char*              name;    /* name of file (zero terminated) */
    ami_long           namel;   /* length of filename */
    long long          size;    /* size of file */
    long long          alloc;   /* allocation of file */
    ami_attrset         attr;    /* attributes */
    ami_long           create;  /* time of creation */
    ami_long           modify;  /* time of last modification */
    ami_long           access;  /* time of last access */
    ami_long           backup;  /* time of last backup */
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
extern void ami_times(char* s, ami_long sl, ami_long t);
extern void ami_dates(char* s, ami_long sl, ami_long t);
extern void ami_writetime(FILE *f, ami_long t);
extern void ami_writedate(FILE *f, ami_long t);
extern ami_long ami_time(void);
extern ami_long ami_local(ami_long t);
extern ami_long ami_clock(void);
extern ami_long ami_elapsed(ami_long r);
extern ami_long  ami_validfile(char* s);
extern ami_long  ami_validpath(char* s);
extern ami_long  ami_wild(char* s);
extern void ami_getenv(char* ls, char* ds, ami_long dsl);
extern void ami_setenv(char* sn, char* sd);
extern void ami_allenv(ami_envrec **el);
extern void ami_remenv(char* sn);
extern void ami_exec(char* cmd);
extern void ami_exece(char* cmd, ami_envrec *el);
extern void ami_execw(char* cmd, ami_long *e);
extern void ami_execew(char* cmd, ami_envrec *el, ami_long *e);
extern void ami_getcur(char* fn, ami_long l);
extern void ami_setcur(char* fn);
extern void ami_brknam(char* fn, char* p, ami_long pl, char* n, ami_long nl, char* e, ami_long el);
extern void ami_maknam(char* fn, ami_long fnl, char* p, char* n, char* e);
extern void ami_fulnam(char* fn, ami_long fnl);
extern void ami_getpgm(char* p, ami_long pl);
extern void ami_getusr(char* fn, ami_long fnl);
extern void ami_setatr(char* fn, ami_attrset a);
extern void ami_resatr(char* fn, ami_attrset a);
extern void ami_bakupd(char* fn);
extern void ami_setuper(char* fn, ami_permset p);
extern void ami_resuper(char* fn, ami_permset p);
extern void ami_setgper(char* fn, ami_permset p);
extern void ami_resgper(char* fn, ami_permset p);
extern void ami_setoper(char* fn, ami_permset p);
extern void ami_resoper(char* fn, ami_permset p);
extern void ami_makpth(char* fn);
extern void ami_rempth(char* fn);
extern void ami_filchr(ami_chrset fc);
extern char ami_optchr(void);
extern char ami_pthchr(void);
extern ami_long  ami_latitude(void);
extern ami_long  ami_longitude(void);
extern ami_long  ami_altitude(void);
extern ami_long  ami_country(void);
extern void ami_countrys(char* s, ami_long sl, ami_long c);
extern ami_long  ami_timezone(void);
extern ami_long  ami_daysave(void);
extern ami_long  ami_time24hour(void);
extern ami_long  ami_language(void);
extern void ami_languages(char* s, ami_long sl, ami_long l);
extern char ami_decimal(void);
extern char ami_numbersep(void);
extern ami_long  ami_timeorder(void);
extern ami_long  ami_dateorder(void);
extern char ami_datesep(void);
extern char ami_timesep(void);
extern char ami_currchr(void);
extern ami_long ami_newthread(void (*threadmain)(void));
extern ami_long ami_initlock(void);
extern void ami_deinitlock(ami_long ln);
extern void ami_lock(ami_long ln);
extern void ami_unlock(ami_long ln);
extern ami_long ami_initsig(void);
extern void ami_deinitsig(ami_long sn);
extern void ami_sendsig(ami_long sn);
extern void ami_sendsigone(ami_long sn);
extern void ami_waitsig(ami_long ln, ami_long sn);

#ifdef __cplusplus
}
#endif

#endif
