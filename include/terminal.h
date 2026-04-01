/** ****************************************************************************
 *
 * Terminal library interface header
 *
 * Declares a routines and data for the Petit Ami terminal level
 * interface. The terminal interface describes a 2 demensional, fixed window on
 * which characters are drawn. Each character can have colors or attributes.
 * The size of the window can be determined, and timer, mouse, and joystick
 * services are supported.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <localdefs.h>

#define AMI_MAXTIM 10 /**< maximum number of timers available */

/** colors displayable in text mode */
typedef enum { ami_black, ami_white, ami_red, ami_green, ami_blue, ami_cyan,
               ami_yellow, ami_magenta } ami_color;

/** events */
typedef enum {

    /** ANSI character returned */      ami_etchar,
    /** cursor up one line */           ami_etup,
    /** down one line */                ami_etdown,
    /** left one character */           ami_etleft,
    /** right one character */          ami_etright,
    /** left one word */                ami_etleftw,
    /** right one word */               ami_etrightw,
    /** home of document */             ami_ethome,
    /** home of screen */               ami_ethomes,
    /** home of line */                 ami_ethomel,
    /** end of document */              ami_etend,
    /** end of screen */                ami_etends,
    /** end of line */                  ami_etendl,
    /** scroll left one character */    ami_etscrl,
    /** scroll right one character */   ami_etscrr,
    /** scroll up one line */           ami_etscru,
    /** scroll down one line */         ami_etscrd,
    /** page down */                    ami_etpagd,
    /** page up */                      ami_etpagu,
    /** tab */                          ami_ettab,
    /** enter line */                   ami_etenter,
    /** insert block */                 ami_etinsert,
    /** insert line */                  ami_etinsertl,
    /** insert toggle */                ami_etinsertt,
    /** delete block */                 ami_etdel,
    /** delete line */                  ami_etdell,
    /** delete character forward */     ami_etdelcf,
    /** delete character backward */    ami_etdelcb,
    /** copy block */                   ami_etcopy,
    /** copy line */                    ami_etcopyl,
    /** cancel current operation */     ami_etcan,
    /** stop current operation */       ami_etstop,
    /** continue current operation */   ami_etcont,
    /** print document */               ami_etprint,
    /** print block */                  ami_etprintb,
    /** print screen */                 ami_etprints,
    /** function key */                 ami_etfun,
    /** display menu */                 ami_etmenu,
    /** mouse button assertion */       ami_etmouba,
    /** mouse button deassertion */     ami_etmoubd,
    /** mouse move */                   ami_etmoumov,
    /** timer matures */                ami_ettim,
    /** joystick button assertion */    ami_etjoyba,
    /** joystick button deassertion */  ami_etjoybd,
    /** joystick move */                ami_etjoymov,
    /** window was resized */           ami_etresize,
    /** window has focus */             ami_etfocus,    
    /** window lost focus */            ami_etnofocus,  
    /** window being hovered */         ami_ethover,    
    /** window stopped being hovered */ ami_etnohover,  
    /** terminate program */            ami_etterm,
    /** frame sync */                   ami_etframe,
    /** window redraw */                ami_etredraw,   
    /** window minimized */             ami_etmin,      
    /** window maximized */             ami_etmax,      
    /** window normalized */            ami_etnorm,     
    /** menu item selected */           ami_etmenus,    

    /* Reserved extra code areas, these are module defined. */
    ami_etsys    = 0x1000, /* start of base system reserved codes */
    ami_etman    = 0x2000, /* start of window management reserved codes */
    ami_etwidget = 0x3000, /* start of widget reserved codes */
    ami_etuser   = 0x4000  /* start of user defined codes */

} ami_evtcod;

/** event record */

typedef struct {

    /* identifier of window for event */ int winid;
    /* event type */                     ami_evtcod etype;
    /* event was handled */              int handled;
    union {

        /* these events require parameter data */

        /** etchar: ANSI character returned */  char echar;
        /** ettim: timer handle that matured */ int timnum;
        /** etmoumov: */
        struct {

            /** mouse number */   int mmoun;
            /** mouse movement */ int moupx, moupy;

        };
        /** etmouba */
        struct {

            /** mouse handle */  int amoun;
            /** button number */ int amoubn;

        };
        /** etmoubd */
        struct {

            /** mouse handle */  int dmoun;
            /** button number */ int dmoubn;

        };
        /** etjoyba */
        struct {

            /** joystick number */ int ajoyn;
            /** button number */   int ajoybn;

        };
        /** etjoybd */
        struct {

            /** joystick number */ int djoyn;
            /** button number */   int djoybn;

        };
        /** etjoymov */
        struct {

            /** joystick number */      int mjoyn;
            /** joystick coordinates */ int joypx, joypy, joypz;
                                        int joyp4, joyp5, joyp6;

        };
        /** function key */ int fkey;
        /** etresize */
        struct {

            int rszx, rszy;

        };
        /* ami_etmenus */
        int menuid; /* menu item selected */

     };

} ami_evtrec, *ami_evtptr;

/** Error codes this module */
typedef enum {

    ami_dispeftbful,          /* file table full */
    ami_dispejoyacc,          /* joystick access */
    ami_dispetimacc,          /* timer access */
    ami_dispefilopr,          /* cannot perform operation on special file */
    ami_dispeinvpos,          /* invalid screen position */
    ami_dispefilzer,          /* filename is empty */
    ami_dispeinvscn,          /* invalid screen number */
    ami_dispeinvhan,          /* invalid handle */
    ami_dispeinvthn,          /* invalid timer handle */
    ami_dispemouacc,          /* mouse access */
    ami_dispeoutdev,          /* output device error */
    ami_dispeinpdev,          /* input device error */
    ami_dispeinvtab,          /* invalid tab stop */
    ami_dispeinvjoy,          /* Invalid joystick ID */
    ami_dispecfgval,          /* invalid configuration value */
    ami_dispenomem,           /* out of memory */
    ami_dispesendevent_unimp, /* sendevent unimplemented */
    ami_dispeopenwin_unimp,   /* openwin unimplemented */
    ami_dispebuffer_unimp,    /* buffer unimplemented */
    ami_dispesizbuf_unimp,    /* sizbuf unimplemented */
    ami_dispegetsiz_unimp,    /* getsiz unimplemented */
    ami_dispesetsiz_unimp,    /* setsiz unimplemented */
    ami_dispesetpos_unimp,    /* setpos unimplemented */
    ami_dispescnsiz_unimp,    /* scnsiz unimplemented */
    ami_dispescncen_unimp,    /* scncen unimplemented */
    ami_dispewinclient_unimp, /* winclient unimplemented */
    ami_dispefront_unimp,     /* front unimplemented */
    ami_dispeback_unimp,      /* back unimplemented */
    ami_dispeframe_unimp,     /* frame unimplemented */
    ami_dispesizable_unimp,   /* sizable unimplemented */
    ami_dispesysbar_unimp,    /* sysbar unimplemented */
    ami_dispemenu_unimp,      /* menu unimplemented */
    ami_dispemenuena_unimp,   /* menuena unimplemented */
    ami_dispemenusel_unimp,   /* menusel unimplemented */
    ami_dispestdmenu_unimp,   /* stdmenu unimplemented */
    ami_dispegetwinid_unimp,  /* getwinid unimplemented */
    ami_dispefocus_unimp,     /* focus unimplemented */
    ami_dispesystem           /* system fault */

} ami_errcod;

/** event function pointer */
typedef void (*ami_pevthan)(ami_evtrec*);

/** error function pointer */
typedef void (*ami_errhan)(ami_errcod e);

/* menu */
typedef struct ami_menurec* ami_menuptr;
typedef struct ami_menurec {

        ami_menuptr next;   /* next menu item in list */
        ami_menuptr branch; /* menu branch */
        int        onoff;  /* on/off highlight */
        int        oneof;  /* "one of" highlight */
        int        bar;    /* place bar under */
        int        id;     /* id of menu item */
        char*      face;   /* text to place on button */

} ami_menurec;
/* standard menu selector */
typedef int ami_stdmenusel;
/* windows mode sets */
typedef enum {

    ami_wmframe, /* frame on/off */
    ami_wmsize,  /* size bars on/off */
    ami_wmsysbar /* system bar on/off */

} ami_winmod;
typedef int ami_winmodset;

/* 
 * Routines at this level 
 */
void ami_cursor(FILE* f, int x, int y);
int  ami_maxx(FILE* f);
int  ami_maxy(FILE* f);
void ami_home(FILE* f);
void ami_del(FILE* f);
void ami_up(FILE* f);
void ami_down(FILE* f);
void ami_left(FILE* f);
void ami_right(FILE* f);
void ami_blink(FILE* f, int e);
void ami_reverse(FILE* f, int e);
void ami_underline(FILE* f, int e);
void ami_superscript(FILE* f, int e);
void ami_subscript(FILE* f, int e);
void ami_italic(FILE* f, int e);
void ami_bold(FILE* f, int e);
void ami_strikeout(FILE* f, int e);
void ami_standout(FILE* f, int e);
void ami_fcolor(FILE* f, ami_color c);
void ami_bcolor(FILE* f, ami_color c);
void ami_auto(FILE* f, int e);
void ami_curvis(FILE* f, int e);
void ami_scroll(FILE* f, int x, int y);
int  ami_curx(FILE* f);
int  ami_cury(FILE* f);
int  ami_curbnd(FILE* f);
void ami_select(FILE *f, int u, int d);
void ami_event(FILE* f, ami_evtrec* er);
void ami_timer(FILE* f, int i, long t, int r);
void ami_killtimer(FILE* f, int i);
int  ami_mouse(FILE *f);
int  ami_mousebutton(FILE* f, int m);
int  ami_joystick(FILE* f);
int  ami_joybutton(FILE* f, int j);
int  ami_joyaxis(FILE* f, int j);
void ami_settab(FILE* f, int t);
void ami_restab(FILE* f, int t);
void ami_clrtab(FILE* f);
int  ami_funkey(FILE* f);
void ami_frametimer(FILE* f, int e);
void ami_autohold(int e);
void ami_wrtstr(FILE* f, char *s);
void ami_wrtstrn(FILE* f, char* s, int n);
void ami_sizbuf(FILE* f, int x, int y);
void ami_title(FILE* f, char* ts);
void ami_titlen(FILE* f, char* ts, int n);
void ami_fcolorc(FILE* f, int r, int g, int b);
void ami_bcolorc(FILE* f, int r, int g, int b);
void ami_eventover(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh);
void ami_eventsover(ami_pevthan eh, ami_pevthan* oeh);
void ami_sendevent(FILE* f, ami_evtrec* er);
void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid);
void ami_buffer(FILE* f, int e);
void ami_getsiz(FILE* f, int* x, int* y);
void ami_setsiz(FILE* f, int x, int y);
void ami_setpos(FILE* f, int x, int y);
void ami_scnsiz(FILE* f, int* x, int* y);
void ami_scncen(FILE* f, int* x, int* y);
void ami_winclient(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms);
void ami_front(FILE* f);
void ami_back(FILE* f);
void ami_frame(FILE* f, int e);
void ami_sizable(FILE* f, int e);
void ami_sysbar(FILE* f, int e);
void ami_menu(FILE* f, ami_menuptr m);
void ami_menuena(FILE* f, int id, int onoff);
void ami_menusel(FILE* f, int id, int select);
void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm);
void ami_focus(FILE* f);
int ami_getwinid(void);
void ami_errorover(ami_errhan nfp, ami_errhan* ofp);

/*
 * Event function override types
 */
typedef int (*ami_evchar_t)(char c);
typedef int (*ami_evup_t)(void);
typedef int (*ami_evdown_t)(void);
typedef int (*ami_evleft_t)(void);
typedef int (*ami_evright_t)(void);
typedef int (*ami_evleftw_t)(void);
typedef int (*ami_evrightw_t)(void);
typedef int (*ami_evhome_t)(void);
typedef int (*ami_evhomes_t)(void);
typedef int (*ami_evhomel_t)(void);
typedef int (*ami_evend_t)(void);
typedef int (*ami_evends_t)(void);
typedef int (*ami_evendl_t)(void);
typedef int (*ami_evscrl_t)(void);
typedef int (*ami_evscrr_t)(void);
typedef int (*ami_evscru_t)(void);
typedef int (*ami_evscrd_t)(void);
typedef int (*ami_evpagd_t)(void);
typedef int (*ami_evpagu_t)(void);
typedef int (*ami_evtab_t)(void);
typedef int (*ami_eventer_t)(void);
typedef int (*ami_evinsert_t)(void);
typedef int (*ami_evinsertl_t)(void);
typedef int (*ami_evinsertt_t)(void);
typedef int (*ami_evdel_t)(void);
typedef int (*ami_evdell_t)(void);
typedef int (*ami_evdelcf_t)(void);
typedef int (*ami_evdelcb_t)(void);
typedef int (*ami_evcopy_t)(void);
typedef int (*ami_evcopyl_t)(void);
typedef int (*ami_evcan_t)(void);
typedef int (*ami_evstop_t)(void);
typedef int (*ami_evcont_t)(void);
typedef int (*ami_evprint_t)(void);
typedef int (*ami_evprintb_t)(void);
typedef int (*ami_evprints_t)(void);
typedef int (*ami_evfun_t)(int k);
typedef int (*ami_evmenu_t)(void);
typedef int (*ami_evmouba_t)(int m, int b);
typedef int (*ami_evmoubd_t)(int m, int b);
typedef int (*ami_evmoumov_t)(int m, int x, int y);
typedef int (*ami_evtim_t)(int t);
typedef int (*ami_evjoyba_t)(int j, int b);
typedef int (*ami_evjoybd_t)(int j, int b);
typedef int (*ami_evjoymov_t)(int j, int x, int y, int z);
typedef int (*ami_evresize_t)(int rszx, int rszy);
typedef int (*ami_evfocus_t)(void);
typedef int (*ami_evnofocus_t)(void);
typedef int (*ami_evhover_t)(void);
typedef int (*ami_evnohover_t)(void);
typedef int (*ami_evterm_t)(void);
typedef int (*ami_evframe_t)(void);

/*
 * Event function overrides
 */
void ami_charover(ami_evchar_t eh, ami_evchar_t* oeh);
void ami_upover(ami_evup_t eh, ami_evup_t* oeh);
void ami_downover(ami_evdown_t eh, ami_evdown_t* oeh);
void ami_leftover(ami_evleft_t eh, ami_evleft_t* oeh);
void ami_rightover(ami_evright_t eh, ami_evright_t* oeh);
void ami_leftwover(ami_evleftw_t eh, ami_evleftw_t* oeh);
void ami_rightwover(ami_evrightw_t eh, ami_evrightw_t* oeh);
void ami_homeover(ami_evhome_t eh, ami_evhome_t* oeh);
void ami_homesover(ami_evhomes_t eh, ami_evhomes_t* oeh);
void ami_homelover(ami_evhomel_t eh, ami_evhomel_t* oeh);
void ami_endover(ami_evend_t eh, ami_evend_t* oeh);
void ami_endsover(ami_evends_t eh, ami_evends_t* oeh);
void ami_endlover(ami_evendl_t eh, ami_evendl_t* oeh);
void ami_scrlover(ami_evscrl_t eh, ami_evscrl_t* oeh);
void ami_scrrover(ami_evscrr_t eh, ami_evscrr_t* oeh);
void ami_scruover(ami_evscru_t eh, ami_evscru_t* oeh);
void ami_scrdover(ami_evscrd_t eh, ami_evscrd_t* oeh);
void ami_pagdover(ami_evpagd_t eh, ami_evpagd_t* oeh);
void ami_paguover(ami_evpagu_t eh, ami_evpagu_t* oeh);
void ami_tabover(ami_evtab_t eh, ami_evtab_t* oeh);
void ami_enterover(ami_eventer_t eh, ami_eventer_t* oeh);
void ami_insertover(ami_evinsert_t eh, ami_evinsert_t* oeh);
void ami_insertlover(ami_evinsertl_t eh, ami_evinsertl_t* oeh);
void ami_inserttover(ami_evinsertt_t eh, ami_evinsertt_t* oeh);
void ami_delover(ami_evdel_t eh, ami_evdel_t* oeh);
void ami_dellover(ami_evdell_t eh, ami_evdell_t* oeh);
void ami_delcfover(ami_evdelcf_t eh, ami_evdelcf_t* oeh);
void ami_delcbover(ami_evdelcb_t eh, ami_evdelcb_t* oeh);
void ami_copyover(ami_evcopy_t eh, ami_evcopy_t* oeh);
void ami_copylover(ami_evcopyl_t eh, ami_evcopyl_t* oeh);
void ami_canover(ami_evcan_t eh, ami_evcan_t* oeh);
void ami_stopover(ami_evstop_t eh, ami_evstop_t* oeh);
void ami_contover(ami_evcont_t eh, ami_evcont_t* oeh);
void ami_printover(ami_evprint_t eh, ami_evprint_t* oeh);
void ami_printbover(ami_evprintb_t eh, ami_evprintb_t* oeh);
void ami_printsover(ami_evprints_t eh, ami_evprints_t* oeh);
void ami_funover(ami_evfun_t eh, ami_evfun_t* oeh);
void ami_menuover(ami_evmenu_t eh, ami_evmenu_t* oeh);
void ami_moubaover(ami_evmouba_t eh, ami_evmouba_t* oeh);
void ami_moubdover(ami_evmoubd_t eh, ami_evmoubd_t* oeh);
void ami_moumovover(ami_evmoumov_t eh, ami_evmoumov_t* oeh);
void ami_timover(ami_evtim_t eh, ami_evtim_t* oeh);
void ami_joybaover(ami_evjoyba_t eh, ami_evjoyba_t* oeh);
void ami_joybdover(ami_evjoybd_t eh, ami_evjoybd_t* oeh);
void ami_joymovover(ami_evjoymov_t eh, ami_evjoymov_t* oeh);
void ami_resizeover(ami_evresize_t eh, ami_evresize_t* oeh);
void ami_focusover(ami_evfocus_t eh, ami_evfocus_t* oeh);
void ami_nofocusover(ami_evnofocus_t eh, ami_evnofocus_t* oeh);
void ami_hoverover(ami_evhover_t eh, ami_evhover_t* oeh);
void ami_nohoverover(ami_evnohover_t eh, ami_evnohover_t* oeh);
void ami_termover(ami_evterm_t eh, ami_evterm_t* oeh);
void ami_frameover(ami_evframe_t eh, ami_evframe_t* oeh);

/** linux system error function pointer */
typedef void (*_pa_linuxerrhan)(int e);

/** linux system error function override */
void _pa_linuxerrorover(_pa_linuxerrhan nfp, _pa_linuxerrhan* ofp);

/*
 * Override vector types
 *
 */
typedef void (*_pa_cursor_t)(FILE* f, int x, int y);
typedef int (*_pa_maxx_t)(FILE* f);
typedef int (*_pa_maxy_t)(FILE* f);
typedef void (*_pa_home_t)(FILE* f);
typedef void (*_pa_del_t)(FILE* f);
typedef void (*_pa_up_t)(FILE* f);
typedef void (*_pa_down_t)(FILE* f);
typedef void (*_pa_left_t)(FILE* f);
typedef void (*_pa_right_t)(FILE* f);
typedef void (*_pa_blink_t)(FILE* f, int e);
typedef void (*_pa_reverse_t)(FILE* f, int e);
typedef void (*_pa_underline_t)(FILE* f, int e);
typedef void (*_pa_superscript_t)(FILE* f, int e);
typedef void (*_pa_subscript_t)(FILE* f, int e);
typedef void (*_pa_italic_t)(FILE* f, int e);
typedef void (*_pa_bold_t)(FILE* f, int e);
typedef void (*_pa_strikeout_t)(FILE* f, int e);
typedef void (*_pa_standout_t)(FILE* f, int e);
typedef void (*_pa_fcolor_t)(FILE* f, ami_color c);
typedef void (*_pa_bcolor_t)(FILE* f, ami_color c);
typedef int (*_pa_curbnd_t)(FILE* f);
typedef void (*_pa_auto_t)(FILE* f, int e);
typedef void (*_pa_curvis_t)(FILE* f, int e);
typedef void (*_pa_scroll_t)(FILE* f, int x, int y);
typedef int (*_pa_curx_t)(FILE* f);
typedef int (*_pa_cury_t)(FILE* f);
typedef void (*_pa_select_t)(FILE* f, int u, int d);
typedef void (*_pa_event_t)(FILE* f, ami_evtrec* er);
typedef void (*_pa_timer_t)(FILE* f, int i, long t, int r);
typedef void (*_pa_killtimer_t)(FILE* f, int i);
typedef int (*_pa_mouse_t)(FILE* f);
typedef int (*_pa_mousebutton_t)(FILE* f, int m);
typedef int (*_pa_joystick_t)(FILE* f);
typedef int (*_pa_joybutton_t)(FILE* f, int j);
typedef int (*_pa_joyaxis_t)(FILE* f, int j);
typedef void (*_pa_settab_t)(FILE* f, int t);
typedef void (*_pa_restab_t)(FILE* f, int t);
typedef void (*_pa_clrtab_t)(FILE* f);
typedef int (*_pa_funkey_t)(FILE* f);
typedef void (*_pa_frametimer_t)(FILE* f, int e);
typedef void (*_pa_autohold_t)(int e);
typedef void (*_pa_wrtstr_t)(FILE* f, char* s);
typedef void (*_pa_wrtstrn_t)(FILE* f, char* s, int n);
typedef void (*_pa_sizbuf_t)(FILE* f, int x, int y);
typedef void (*_pa_title_t)(FILE* f, char* ts);
typedef void (*_pa_titlen_t)(FILE* f, char* ts, int l);
typedef void (*_pa_fcolorc_t)(FILE* f, int r, int g, int b);
typedef void (*_pa_bcolorc_t)(FILE* f, int r, int g, int b);
typedef void (*_pa_eventover_t)(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh);
typedef void (*_pa_eventsover_t)(ami_pevthan eh,  ami_pevthan* oeh);
typedef void (*_pa_sendevent_t)(FILE* f, ami_evtrec* er);
typedef void (*_pa_openwin_t)(FILE** infile, FILE** outfile, FILE* parent, int wid);
typedef void (*_pa_buffer_t)(FILE* f, int e);
typedef void (*_pa_getsiz_t)(FILE* f, int* x, int* y);
typedef void (*_pa_setsiz_t)(FILE* f, int x, int y);
typedef void (*_pa_setpos_t)(FILE* f, int x, int y);
typedef void (*_pa_scnsiz_t)(FILE* f, int* x, int* y);
typedef void (*_pa_scncen_t)(FILE* f, int* x, int* y);
typedef void (*_pa_winclient_t)(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms);
typedef void (*_pa_front_t)(FILE* f);
typedef void (*_pa_back_t)(FILE* f);
typedef void (*_pa_frame_t)(FILE* f, int e);
typedef void (*_pa_sizable_t)(FILE* f, int e);
typedef void (*_pa_sysbar_t)(FILE* f, int e);
typedef void (*_pa_menu_t)(FILE* f, ami_menuptr m);
typedef void (*_pa_menuena_t)(FILE* f, int id, int onoff);
typedef void (*_pa_menusel_t)(FILE* f, int id, int select);
typedef void (*_pa_stdmenu_t)(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm);
typedef void (*_pa_focus_t)(FILE* f);
typedef int (*_pa_getwinid_t)(void);

/*
 * Overrider routines
 */
void _pa_cursor_ovr(_pa_cursor_t nfp, _pa_cursor_t* ofp);
void _pa_maxx_ovr(_pa_maxx_t nfp, _pa_maxx_t* ofp);
void _pa_maxy_ovr(_pa_maxy_t nfp, _pa_maxy_t* ofp);
void _pa_home_ovr(_pa_home_t nfp, _pa_home_t* ofp);
void _pa_del_ovr(_pa_del_t nfp, _pa_del_t* ofp);
void _pa_up_ovr(_pa_up_t nfp, _pa_up_t* ofp);
void _pa_down_ovr(_pa_down_t nfp, _pa_down_t* ofp);
void _pa_left_ovr(_pa_left_t nfp, _pa_left_t* ofp);
void _pa_right_ovr(_pa_right_t nfp, _pa_right_t* ofp);
void _pa_blink_ovr(_pa_blink_t nfp, _pa_blink_t* ofp);
void _pa_reverse_ovr(_pa_reverse_t nfp, _pa_reverse_t* ofp);
void _pa_underline_ovr(_pa_underline_t nfp, _pa_underline_t* ofp);
void _pa_superscript_ovr(_pa_superscript_t nfp, _pa_superscript_t* ofp);
void _pa_subscript_ovr(_pa_subscript_t nfp, _pa_subscript_t* ofp);
void _pa_italic_ovr(_pa_italic_t nfp, _pa_italic_t* ofp);
void _pa_bold_ovr(_pa_bold_t nfp, _pa_bold_t* ofp);
void _pa_strikeout_ovr(_pa_strikeout_t nfp, _pa_strikeout_t* ofp);
void _pa_standout_ovr(_pa_standout_t nfp, _pa_standout_t* ofp);
void _pa_fcolor_ovr(_pa_fcolor_t nfp, _pa_fcolor_t* ofp);
void _pa_bcolor_ovr(_pa_bcolor_t nfp, _pa_bcolor_t* ofp);
void _pa_curbnd_ovr(_pa_curbnd_t nfp, _pa_curbnd_t* ofp);
void _pa_auto_ovr(_pa_auto_t nfp, _pa_auto_t* ofp);
void _pa_curvis_ovr(_pa_curvis_t nfp, _pa_curvis_t* ofp);
void _pa_scroll_ovr(_pa_scroll_t nfp, _pa_scroll_t* ofp);
void _pa_curx_ovr(_pa_curx_t nfp, _pa_curx_t* ofp);
void _pa_cury_ovr(_pa_cury_t nfp, _pa_cury_t* ofp);
void _pa_select_ovr(_pa_select_t nfp, _pa_select_t* ofp);
void _pa_event_ovr(_pa_event_t nfp, _pa_event_t* ofp);
void _pa_timer_ovr(_pa_timer_t nfp, _pa_timer_t* ofp);
void _pa_killtimer_ovr(_pa_killtimer_t nfp, _pa_killtimer_t* ofp);
void _pa_mouse_ovr(_pa_mouse_t nfp, _pa_mouse_t* ofp);
void _pa_mousebutton_ovr(_pa_mousebutton_t nfp, _pa_mousebutton_t* ofp);
void _pa_joystick_ovr(_pa_joystick_t nfp, _pa_joystick_t* ofp);
void _pa_joybutton_ovr(_pa_joybutton_t nfp, _pa_joybutton_t* ofp);
void _pa_joyaxis_ovr(_pa_joyaxis_t nfp, _pa_joyaxis_t* ofp);
void _pa_settab_ovr(_pa_settab_t nfp, _pa_settab_t* ofp);
void _pa_restab_ovr(_pa_restab_t nfp, _pa_restab_t* ofp);
void _pa_clrtab_ovr(_pa_clrtab_t nfp, _pa_clrtab_t* ofp);
void _pa_funkey_ovr(_pa_funkey_t nfp, _pa_funkey_t* ofp);
void _pa_frametimer_ovr(_pa_frametimer_t nfp, _pa_frametimer_t* ofp);
void _pa_autohold_ovr(_pa_autohold_t nfp, _pa_autohold_t* ofp);
void _pa_wrtstr_ovr(_pa_wrtstr_t nfp, _pa_wrtstr_t* ofp);
void _pa_wrtstrn_ovr(_pa_wrtstrn_t nfp, _pa_wrtstrn_t* ofp);
void _pa_sizbuf_ovr(_pa_sizbuf_t nfp, _pa_sizbuf_t* ofp);
void _pa_title_ovr(_pa_title_t nfp, _pa_title_t* ofp);
void _pa_titlen_ovr(_pa_titlen_t nfp, _pa_titlen_t* ofp);
void _pa_fcolorc_ovr(_pa_fcolorc_t nfp, _pa_fcolorc_t* ofp);
void _pa_bcolorc_ovr(_pa_bcolorc_t nfp, _pa_bcolorc_t* ofp);
void _pa_eventover_ovr(_pa_eventover_t nfp, _pa_eventover_t* ofp);
void _pa_eventsover_ovr(_pa_eventsover_t nfp, _pa_eventsover_t* ofp);
void _pa_sendevent_ovr(_pa_sendevent_t nfp, _pa_sendevent_t* ofp);
void _pa_openwin_ovr(_pa_openwin_t nfp, _pa_openwin_t* ofp);
void _pa_buffer_ovr(_pa_buffer_t nfp, _pa_buffer_t* ofp);
void _pa_getsiz_ovr(_pa_getsiz_t nfp, _pa_getsiz_t* ofp);
void _pa_setsiz_ovr(_pa_setsiz_t nfp, _pa_setsiz_t* ofp);
void _pa_setpos_ovr(_pa_setpos_t nfp, _pa_setpos_t* ofp);
void _pa_scnsiz_ovr(_pa_scnsiz_t nfp, _pa_scnsiz_t* ofp);
void _pa_scncen_ovr(_pa_scncen_t nfp, _pa_scncen_t* ofp);
void _pa_winclient_ovr(_pa_winclient_t nfp, _pa_winclient_t* ofp);
void _pa_front_ovr(_pa_front_t nfp, _pa_front_t* ofp);
void _pa_back_ovr(_pa_back_t nfp, _pa_back_t* ofp);
void _pa_frame_ovr(_pa_frame_t nfp, _pa_frame_t* ofp);
void _pa_sizable_ovr(_pa_sizable_t nfp, _pa_sizable_t* ofp);
void _pa_sysbar_ovr(_pa_sysbar_t nfp, _pa_sysbar_t* ofp);
void _pa_menu_ovr(_pa_menu_t nfp, _pa_menu_t* ofp);
void _pa_menuena_ovr(_pa_menuena_t nfp, _pa_menuena_t* ofp);
void _pa_menusel_ovr(_pa_menusel_t nfp, _pa_menusel_t* ofp);
void _pa_stdmenu_ovr(_pa_stdmenu_t nfp, _pa_stdmenu_t* ofp);
void _pa_focus_ovr(_pa_focus_t nfp, _pa_focus_t* ofp);
void _pa_getwinid_ovr(_pa_getwinid_t nfp, _pa_getwinid_t* ofp);

/*
 * Event function override override types
 */
typedef void (*_pa_charover_t)(ami_evchar_t eh, ami_evchar_t* oeh);
typedef void (*_pa_upover_t)(ami_evup_t eh, ami_evup_t* oeh);
typedef void (*_pa_downover_t)(ami_evdown_t eh, ami_evdown_t* oeh);
typedef void (*_pa_leftover_t)(ami_evleft_t eh, ami_evleft_t* oeh);
typedef void (*_pa_rightover_t)(ami_evright_t eh, ami_evright_t* oeh);
typedef void (*_pa_leftwover_t)(ami_evleftw_t eh, ami_evleftw_t* oeh);
typedef void (*_pa_rightwover_t)(ami_evrightw_t eh, ami_evrightw_t* oeh);
typedef void (*_pa_homeover_t)(ami_evhome_t eh, ami_evhome_t* oeh);
typedef void (*_pa_homesover_t)(ami_evhomes_t eh, ami_evhomes_t* oeh);
typedef void (*_pa_homelover_t)(ami_evhomel_t eh, ami_evhomel_t* oeh);
typedef void (*_pa_endover_t)(ami_evend_t eh, ami_evend_t* oeh);
typedef void (*_pa_endsover_t)(ami_evends_t eh, ami_evends_t* oeh);
typedef void (*_pa_endlover_t)(ami_evendl_t eh, ami_evendl_t* oeh);
typedef void (*_pa_scrlover_t)(ami_evscrl_t eh, ami_evscrl_t* oeh);
typedef void (*_pa_scrrover_t)(ami_evscrr_t eh, ami_evscrr_t* oeh);
typedef void (*_pa_scruover_t)(ami_evscru_t eh, ami_evscru_t* oeh);
typedef void (*_pa_scrdover_t)(ami_evscrd_t eh, ami_evscrd_t* oeh);
typedef void (*_pa_pagdover_t)(ami_evpagd_t eh, ami_evpagd_t* oeh);
typedef void (*_pa_paguover_t)(ami_evpagu_t eh, ami_evpagu_t* oeh);
typedef void (*_pa_tabover_t)(ami_evtab_t eh, ami_evtab_t* oeh);
typedef void (*_pa_enterover_t)(ami_eventer_t eh, ami_eventer_t* oeh);
typedef void (*_pa_insertover_t)(ami_evinsert_t eh, ami_evinsert_t* oeh);
typedef void (*_pa_insertlover_t)(ami_evinsertl_t eh, ami_evinsertl_t* oeh);
typedef void (*_pa_inserttover_t)(ami_evinsertt_t eh, ami_evinsertt_t* oeh);
typedef void (*_pa_delover_t)(ami_evdel_t eh, ami_evdel_t* oeh);
typedef void (*_pa_dellover_t)(ami_evdell_t eh, ami_evdell_t* oeh);
typedef void (*_pa_delcfover_t)(ami_evdelcf_t eh, ami_evdelcf_t* oeh);
typedef void (*_pa_delcbover_t)(ami_evdelcb_t eh, ami_evdelcb_t* oeh);
typedef void (*_pa_copyover_t)(ami_evcopy_t eh, ami_evcopy_t* oeh);
typedef void (*_pa_copylover_t)(ami_evcopyl_t eh, ami_evcopyl_t* oeh);
typedef void (*_pa_canover_t)(ami_evcan_t eh, ami_evcan_t* oeh);
typedef void (*_pa_stopover_t)(ami_evstop_t eh, ami_evstop_t* oeh);
typedef void (*_pa_contover_t)(ami_evcont_t eh, ami_evcont_t* oeh);
typedef void (*_pa_printover_t)(ami_evprint_t eh, ami_evprint_t* oeh);
typedef void (*_pa_printbover_t)(ami_evprintb_t eh, ami_evprintb_t* oeh);
typedef void (*_pa_printsover_t)(ami_evprints_t eh, ami_evprints_t* oeh);
typedef void (*_pa_funover_t)(ami_evfun_t eh, ami_evfun_t* oeh);
typedef void (*_pa_menuover_t)(ami_evmenu_t eh, ami_evmenu_t* oeh);
typedef void (*_pa_moubaover_t)(ami_evmouba_t eh, ami_evmouba_t* oeh);
typedef void (*_pa_moubdover_t)(ami_evmoubd_t eh, ami_evmoubd_t* oeh);
typedef void (*_pa_moumovover_t)(ami_evmoumov_t eh, ami_evmoumov_t* oeh);
typedef void (*_pa_timover_t)(ami_evtim_t eh, ami_evtim_t* oeh);
typedef void (*_pa_joybaover_t)(ami_evjoyba_t eh, ami_evjoyba_t* oeh);
typedef void (*_pa_joybdover_t)(ami_evjoybd_t eh, ami_evjoybd_t* oeh);
typedef void (*_pa_joymovover_t)(ami_evjoymov_t eh, ami_evjoymov_t* oeh);
typedef void (*_pa_resizeover_t)(ami_evresize_t eh, ami_evresize_t* oeh);
typedef void (*_pa_focusover_t)(ami_evfocus_t eh, ami_evfocus_t* oeh);
typedef void (*_pa_nofocusover_t)(ami_evnofocus_t eh, ami_evnofocus_t* oeh);
typedef void (*_pa_hoverover_t)(ami_evhover_t eh, ami_evhover_t* oeh);
typedef void (*_pa_nohoverover_t)(ami_evnohover_t eh, ami_evnohover_t* oeh);
typedef void (*_pa_termover_t)(ami_evterm_t eh, ami_evterm_t* oeh);
typedef void (*_pa_frameover_t)(ami_evframe_t eh, ami_evframe_t* oeh);

/*
 * Event function override overrides
 */
void _pa_charover_ovr(_pa_charover_t eh, _pa_charover_t* oeh);
void _pa_upover_ovr(_pa_upover_t eh, _pa_upover_t* oeh);
void _pa_downover_ovr(_pa_downover_t eh, _pa_downover_t* oeh);
void _pa_leftover_ovr(_pa_leftover_t eh, _pa_leftover_t* oeh);
void _pa_rightover_ovr(_pa_rightover_t eh, _pa_rightover_t* oeh);
void _pa_leftwover_ovr(_pa_leftwover_t eh, _pa_leftwover_t* oeh);
void _pa_rightwover_ovr(_pa_rightwover_t eh, _pa_rightwover_t* oeh);
void _pa_homeover_ovr(_pa_homeover_t eh, _pa_homeover_t* oeh);
void _pa_homesover_ovr(_pa_homesover_t eh, _pa_homesover_t* oeh);
void _pa_homelover_ovr(_pa_homelover_t eh, _pa_homelover_t* oeh);
void _pa_endover_ovr(_pa_endover_t eh, _pa_endover_t* oeh);
void _pa_endsover_ovr(_pa_endsover_t eh, _pa_endsover_t* oeh);
void _pa_endlover_ovr(_pa_endlover_t eh, _pa_endlover_t* oeh);
void _pa_scrlover_ovr(_pa_scrlover_t eh, _pa_scrlover_t* oeh);
void _pa_scrrover_ovr(_pa_scrrover_t eh, _pa_scrrover_t* oeh);
void _pa_scruover_ovr(_pa_scruover_t eh, _pa_scruover_t* oeh);
void _pa_scrdover_ovr(_pa_scrdover_t eh, _pa_scrdover_t* oeh);
void _pa_pagdover_ovr(_pa_pagdover_t eh, _pa_pagdover_t* oeh);
void _pa_paguover_ovr(_pa_paguover_t eh, _pa_paguover_t* oeh);
void _pa_tabover_ovr(_pa_tabover_t eh, _pa_tabover_t* oeh);
void _pa_enterover_ovr(_pa_enterover_t eh, _pa_enterover_t* oeh);
void _pa_insertover_ovr(_pa_insertover_t eh, _pa_insertover_t* oeh);
void _pa_insertlover_ovr(_pa_insertlover_t eh, _pa_insertlover_t* oeh);
void _pa_inserttover_ovr(_pa_inserttover_t eh, _pa_inserttover_t* oeh);
void _pa_delover_ovr(_pa_delover_t eh, _pa_delover_t* oeh);
void _pa_dellover_ovr(_pa_dellover_t eh, _pa_dellover_t* oeh);
void _pa_delcfover_ovr(_pa_delcfover_t eh, _pa_delcfover_t* oeh);
void _pa_delcbover_ovr(_pa_delcbover_t eh, _pa_delcbover_t* oeh);
void _pa_copyover_ovr(_pa_copyover_t eh, _pa_copyover_t* oeh);
void _pa_copylover_ovr(_pa_copylover_t eh, _pa_copylover_t* oeh);
void _pa_canover_ovr(_pa_canover_t eh, _pa_canover_t* oeh);
void _pa_stopover_ovr(_pa_stopover_t eh, _pa_stopover_t* oeh);
void _pa_contover_ovr(_pa_contover_t eh, _pa_contover_t* oeh);
void _pa_printover_ovr(_pa_printover_t eh, _pa_printover_t* oeh);
void _pa_printbover_ovr(_pa_printbover_t eh, _pa_printbover_t* oeh);
void _pa_printsover_ovr(_pa_printsover_t eh, _pa_printsover_t* oeh);
void _pa_funover_ovr(_pa_funover_t eh, _pa_funover_t* oeh);
void _pa_menuover_ovr(_pa_menuover_t eh, _pa_menuover_t* oeh);
void _pa_moubaover_ovr(_pa_moubaover_t eh, _pa_moubaover_t* oeh);
void _pa_moubdover_ovr(_pa_moubdover_t eh, _pa_moubdover_t* oeh);
void _pa_moumovover_ovr(_pa_moumovover_t eh, _pa_moumovover_t* oeh);
void _pa_timover_ovr(_pa_timover_t eh, _pa_timover_t* oeh);
void _pa_joybaover_ovr(_pa_joybaover_t eh, _pa_joybaover_t* oeh);
void _pa_joybdover_ovr(_pa_joybdover_t eh, _pa_joybdover_t* oeh);
void _pa_joymovover_ovr(_pa_joymovover_t eh, _pa_joymovover_t* oeh);
void _pa_resizeover_ovr(_pa_resizeover_t eh, _pa_resizeover_t* oeh);
void _pa_focusover_ovr(_pa_focusover_t eh, _pa_focusover_t* oeh);
void _pa_nofocusover_ovr(_pa_nofocusover_t eh, _pa_nofocusover_t* oeh);
void _pa_hoverover_ovr(_pa_hoverover_t eh, _pa_hoverover_t* oeh);
void _pa_nohoverover_ovr(_pa_nohoverover_t eh, _pa_nohoverover_t* oeh);
void _pa_termover_ovr(_pa_termover_t eh, _pa_termover_t* oeh);
void _pa_frameover_ovr(_pa_frameover_t eh, _pa_frameover_t* oeh);

#ifdef __cplusplus
}
#endif

#endif /* __TERMINAL_H__ */
