/*******************************************************************************
*                                                                              *
*                     CURSES COMPATIBILITY LAYER FOR PETIT-AMI                 *
*                                                                              *
* Maps curses API calls to the Petit-Ami terminal interface.                   *
*                                                                              *
* Key mappings:                                                                *
*   curses          Petit-Ami                                                  *
*   ------          ---------                                                  *
*   initscr()       ami_auto(off)  (cursor left visible, as in ncurses)        *
*   endwin()        ami_auto(on), ami_curvis(on)                               *
*   move(y,x)       ami_cursor(stdout, x+1, y+1)   (0-based to 1-based)       *
*   addch(c)        putchar(c)                                                 *
*   clear()         putchar('\f')                                              *
*   getch()         ami_event loop                                             *
*   attron/off      ami_bold, ami_reverse, ami_underline, etc.                 *
*   COLOR_PAIR      ami_fcolor, ami_bcolor                                     *
*                                                                              *
* Coordinate convention: curses uses (y, x) 0-based. Ami uses (x, y) 1-based. *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <terminal.h>
#include <services.h>
#include "curses.h"

#define ADP_MAXSTR    1024  /* path buffer size */
#define ADP_MAXKEYSTR 64    /* max chars per mapped key string */
#define ADP_MAXLABEL  48    /* max chars per hint label */
#define ADP_MAXHINT   4096  /* max chars for the full hint string */
#define ADP_MAXFKEY   24    /* highest supported function key number */
#define ADP_EVT_COUNT (ami_etmenus + 1) /* size of indexed event table */

/* Program invocation name (glibc/MinGW provides this). Used to derive the
   .adp file name — ami_getpgm() returns only the directory component. */
#if defined(__linux) || defined(__MINGW32__)
extern char* program_invocation_name;
#endif

/* ami_menurec / ami_menu are declared in terminal.h. At runtime, ami_menu
   is a no-op on the terminal backend and a real menu bar under graphics.c.
   AMI_CURSES_MENU gates whether curses.c actually builds and binds menus. */

/* internal state */
static int   cur_initialized = 0;
static int   cur_echo = 0;      /* echo mode (default off after initscr) */
static int   cur_nodelay = 0;   /* non-blocking mode */
static int   cur_timeout_ms = -1; /* getch timeout (-1 = blocking) */
static int   cur_keypad = 1;    /* keypad mode (always on) */
static attr_t cur_attrs = A_NORMAL;
static int   cur_ended = 0;
static int   cur_ungetch = -1;  /* ungetch buffer */

/*******************************************************************************

Adapter (.adp) tables and parser

A .adp file named <progname>.adp alongside the executable maps Ami terminal
events to key sequences that the adapted program consumes. When an adapter
mapping exists for an incoming event, getch() returns the mapped string one
character per call instead of the default curses KEY_* code. If the file is
absent, the adapter layer is dormant and default behavior is unchanged.

*******************************************************************************/

typedef struct {
    int  len;                         /* 0 = no mapping */
    unsigned char s[ADP_MAXKEYSTR];
    char label[ADP_MAXLABEL];         /* trailing-comment description */
} adp_entry;

static int       adp_loaded = 0;
static adp_entry adp_evt[ADP_EVT_COUNT];   /* by event code */
static adp_entry adp_fkey[ADP_MAXFKEY + 1];/* by function-key number (1..N) */

/* Emission queue — chars pending delivery to getch() from a mapped string. */
static unsigned char adp_outbuf[ADP_MAXKEYSTR];
static int           adp_outpos = 0;
static int           adp_outlen = 0;

/* Pre-formatted hint line(s) shown at the bottom of the screen. Derived from
   the mapped events + their trailing-comment labels. Each line fits COLS. */
static char adp_hint[ADP_MAXHINT];
static int  adp_hint_rows = 0;        /* number of reserved rows (0 = off) */

/* forward decl — defined further down with the curses attribute code */
static void apply_attrs(void);

#ifdef AMI_CURSES_MENU
/* Menu item key table — indexed by ami_menurec.id. When a menu item is
   selected, we inject its mapped keystring via the emission queue. */
#define ADP_MAX_MENU_ITEMS 128
static adp_entry   adp_menu_keys[ADP_MAX_MENU_ITEMS];
static ami_menuptr adp_menu_root      = NULL;
static int         adp_menu_next_id   = 1;
#endif

/* Name-to-code table for .adp event identifiers. The spec drops the "ami_"
   prefix in the file; "etfunN" (N a decimal integer) is handled separately
   as a per-function-key mapping. */
static const struct { const char* name; int code; } adp_evnames[] = {
    { "etchar",    ami_etchar    }, { "etup",      ami_etup      },
    { "etdown",    ami_etdown    }, { "etleft",    ami_etleft    },
    { "etright",   ami_etright   }, { "etleftw",   ami_etleftw   },
    { "etrightw",  ami_etrightw  }, { "ethome",    ami_ethome    },
    { "ethomes",   ami_ethomes   }, { "ethomel",   ami_ethomel   },
    { "etend",     ami_etend     }, { "etends",    ami_etends    },
    { "etendl",    ami_etendl    }, { "etscrl",    ami_etscrl    },
    { "etscrr",    ami_etscrr    }, { "etscru",    ami_etscru    },
    { "etscrd",    ami_etscrd    }, { "etpagd",    ami_etpagd    },
    { "etpagu",    ami_etpagu    }, { "ettab",     ami_ettab     },
    { "etenter",   ami_etenter   }, { "etinsert",  ami_etinsert  },
    { "etinsertl", ami_etinsertl }, { "etinsertt", ami_etinsertt },
    { "etdel",     ami_etdel     }, { "etdell",    ami_etdell    },
    { "etdelcf",   ami_etdelcf   }, { "etdelcb",   ami_etdelcb   },
    { "etcopy",    ami_etcopy    }, { "etcopyl",   ami_etcopyl   },
    { "etcan",     ami_etcan     }, { "etstop",    ami_etstop    },
    { "etcont",    ami_etcont    }, { "etprint",   ami_etprint   },
    { "etprintb",  ami_etprintb  }, { "etprints",  ami_etprints  },
    { "etfun",     ami_etfun     }, { "etmenu",    ami_etmenu    },
    { "etmouba",   ami_etmouba   }, { "etmoubd",   ami_etmoubd   },
    { "etmoumov",  ami_etmoumov  }, { "ettim",     ami_ettim     },
    { "etjoyba",   ami_etjoyba   }, { "etjoybd",   ami_etjoybd   },
    { "etjoymov",  ami_etjoymov  }, { "etresize",  ami_etresize  },
    { "etfocus",   ami_etfocus   }, { "etnofocus", ami_etnofocus },
    { "ethover",   ami_ethover   }, { "etnohover", ami_etnohover },
    { "etterm",    ami_etterm    }, { "etframe",   ami_etframe   },
    { "etredraw",  ami_etredraw  }, { "etmin",     ami_etmin     },
    { "etmax",     ami_etmax     }, { "etnorm",    ami_etnorm    },
    { "etmenus",   ami_etmenus   },
    { NULL, 0 }
};

/* -------- tokenizer -------- */

typedef enum { ADP_TK_EOF, ADP_TK_IDENT, ADP_TK_STRING, ADP_TK_ERR } adp_tktype;

typedef struct {
    FILE* f;
    int   pb;          /* pushback char, -1 if empty */
    adp_tktype type;
    char       ident[64];
    unsigned char str[ADP_MAXKEYSTR];
    int           strlen_;
} adp_parser;

static int adp_getc(adp_parser* p) {

    int c;
    if (p->pb != -1) { c = p->pb; p->pb = -1; return c; }
    c = fgetc(p->f);
    return c;

}

static void adp_ungetc(adp_parser* p, int c) { p->pb = c; }

/* Skip whitespace and '!' comments (to end of line). */
static void adp_skip_ws(adp_parser* p) {

    int c;
    for (;;) {
        c = adp_getc(p);
        if (c == EOF) return;
        if (c == '!') {
            while ((c = adp_getc(p)) != EOF && c != '\n') ;
            continue;
        }
        if (!isspace(c)) { adp_ungetc(p, c); return; }
    }

}

/* Hex digit to value, -1 if not hex. */
static int adp_hex(int c) {

    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;

}

/* Parse a single escape after the leading backslash. */
static int adp_escape(adp_parser* p) {

    int c = adp_getc(p);
    int h1, h2, v;

    switch (c) {
        case 'r':  return '\r';
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'b':  return '\b';
        case 'f':  return '\f';
        case 'a':  return '\a';
        case 'v':  return '\v';
        case 'e':  return 0x1B;
        case '\\': return '\\';
        case '\'': return '\'';
        case '"':  return '"';
        case '0':
            /* \0xNN hex form, or plain \0 (NUL) */
            c = adp_getc(p);
            if (c == 'x' || c == 'X') {
                h1 = adp_hex(adp_getc(p));
                if (h1 < 0) return 0;
                h2 = adp_hex(adp_getc(p));
                if (h2 < 0) return h1;
                return (h1 << 4) | h2;
            }
            adp_ungetc(p, c);
            return 0;
        case 'x': case 'X':
            h1 = adp_hex(adp_getc(p));
            if (h1 < 0) return 0;
            h2 = adp_hex(adp_getc(p));
            if (h2 < 0) return h1;
            v = (h1 << 4) | h2;
            return v;
        default:
            return c; /* unknown escape: take literal */
    }

}

/* Read the next token (identifier or quoted string). */
static void adp_next(adp_parser* p) {

    int c, ch;

    adp_skip_ws(p);
    c = adp_getc(p);
    if (c == EOF) { p->type = ADP_TK_EOF; return; }

    if (c == '\'') {
        /* quoted string */
        p->strlen_ = 0;
        for (;;) {
            c = adp_getc(p);
            if (c == EOF)   { p->type = ADP_TK_ERR; return; }
            if (c == '\'')  { p->type = ADP_TK_STRING; return; }
            if (c == '^') {
                ch = adp_getc(p);
                if (ch == EOF) { p->type = ADP_TK_ERR; return; }
                /* ctrl: toupper(ch) & 0x1F; preserves ^\ ^] ^_ etc. */
                c = toupper((unsigned char)ch) & 0x1F;
            } else if (c == '\\') {
                c = adp_escape(p);
            }
            if (p->strlen_ < ADP_MAXKEYSTR)
                p->str[p->strlen_++] = (unsigned char)c;
        }
    }

    if (isalpha(c) || c == '_') {
        int n = 0;
        p->ident[n++] = (char)c;
        while ((c = adp_getc(p)) != EOF
               && (isalnum(c) || c == '_')) {
            if (n < (int)sizeof(p->ident) - 1) p->ident[n++] = (char)c;
        }
        p->ident[n] = 0;
        if (c != EOF) adp_ungetc(p, c);
        p->type = ADP_TK_IDENT;
        return;
    }

    p->type = ADP_TK_ERR;

}

/* -------- adapter lookup / resolution -------- */

/* Match "et<name>" identifier. Handles "etfunN" as a special case, returning
   the function-key index in *fkn (>0) with code=ami_etfun. Returns 1 if
   matched, 0 otherwise. */
static int adp_resolve(const char* id, int* code, int* fkn) {

    int i;

    *fkn = 0;
    /* etfunN (decimal) */
    if (!strncmp(id, "etfun", 5) && id[5]) {
        int n = 0;
        const char* q = id + 5;
        while (*q >= '0' && *q <= '9') { n = n * 10 + (*q - '0'); q++; }
        if (*q == 0 && n >= 1 && n <= ADP_MAXFKEY) {
            *code = ami_etfun;
            *fkn  = n;
            return 1;
        }
    }
    for (i = 0; adp_evnames[i].name; i++) {
        if (!strcmp(adp_evnames[i].name, id)) {
            *code = adp_evnames[i].code;
            return 1;
        }
    }
    return 0;

}

/* Store a parsed mapping. label may be NULL. */
static void adp_store(int code, int fkn,
                      const unsigned char* s, int len,
                      const char* label) {

    adp_entry* e;
    if (len <= 0) return;
    if (len > ADP_MAXKEYSTR) len = ADP_MAXKEYSTR;
    if (fkn > 0 && code == ami_etfun) {
        if (fkn <= 0 || fkn > ADP_MAXFKEY) return;
        e = &adp_fkey[fkn];
    } else {
        if (code < 0 || code >= ADP_EVT_COUNT) return;
        e = &adp_evt[code];
    }
    memcpy(e->s, s, len);
    e->len = len;
    if (label && *label) {
        strncpy(e->label, label, ADP_MAXLABEL - 1);
        e->label[ADP_MAXLABEL - 1] = 0;
    } else {
        e->label[0] = 0;
    }

}

/* Read a trailing "! ... <newline>" comment on the current line, if any.
   Writes the trimmed comment into out[0..maxlen-1]. Leaves the stream
   positioned just before the terminating newline (or EOF). Non-comment
   non-whitespace characters are pushed back so the main parser sees them. */
static void adp_trailing_comment(adp_parser* p, char* out, int maxlen) {

    int c, n;

    out[0] = 0;
    for (;;) {
        c = adp_getc(p);
        if (c == EOF) return;
        if (c == '\n') { adp_ungetc(p, c); return; }
        if (c == ' ' || c == '\t') continue;
        if (c != '!') { adp_ungetc(p, c); return; }
        /* consume comment text */
        while ((c = adp_getc(p)) == ' ' || c == '\t') ;
        n = 0;
        while (c != EOF && c != '\n') {
            if (n < maxlen - 1) out[n++] = (char)c;
            c = adp_getc(p);
        }
        while (n > 0 && (out[n-1] == ' ' || out[n-1] == '\t')) n--;
        out[n] = 0;
        if (c == '\n') adp_ungetc(p, c);
        return;
    }

}

/* Parse a keyequ block: pairs of (identifier, string [, ! comment]) until
   "keyend". */
static void adp_parse_keyequ(adp_parser* p) {

    int  code, fkn;
    char label[ADP_MAXLABEL];

    for (;;) {
        adp_next(p);
        if (p->type == ADP_TK_EOF) return;
        if (p->type == ADP_TK_ERR) return;
        if (p->type == ADP_TK_IDENT && !strcmp(p->ident, "keyend")) return;
        if (p->type != ADP_TK_IDENT) continue;
        if (!adp_resolve(p->ident, &code, &fkn)) {
            /* unknown event name: consume its value and ignore */
            adp_next(p);
            continue;
        }
        adp_next(p);
        if (p->type != ADP_TK_STRING) continue;
        adp_trailing_comment(p, label, sizeof(label));
        adp_store(code, fkn, p->str, p->strlen_, label);
    }

}

#ifdef AMI_CURSES_MENU

/* Build a menu (or submenu) linked list from the .adp stream. endkw selects
   whether this is the top-level "menuend" or a nested "submenuend". Each
   leaf gets a unique id; its keystring is stored in adp_menu_keys[id] for
   later retrieval when ami_etmenus fires. Nested submenus recurse. */
static ami_menuptr adp_build_menu_body(adp_parser* p, const char* endkw) {

    ami_menuptr head = NULL, tail = NULL;
    int bar_pending = 0;
    char face[ADP_MAXKEYSTR + 1];
    int  facelen;
    ami_menurec* item;

    for (;;) {
        adp_next(p);
        if (p->type == ADP_TK_EOF || p->type == ADP_TK_ERR) break;
        if (p->type == ADP_TK_IDENT && !strcmp(p->ident, endkw)) break;
        if (p->type == ADP_TK_IDENT && !strcmp(p->ident, "bar")) {
            bar_pending = 1;
            continue;
        }
        if (p->type != ADP_TK_STRING) continue;

        /* capture the face string for this item */
        facelen = p->strlen_;
        if (facelen >= (int)sizeof(face)) facelen = sizeof(face) - 1;
        memcpy(face, p->str, facelen);
        face[facelen] = 0;

        item = (ami_menurec*)calloc(1, sizeof(ami_menurec));
        if (!item) continue;
        item->face = strdup(face);
        item->bar  = bar_pending;
        bar_pending = 0;

        adp_next(p);
        if (p->type == ADP_TK_IDENT && !strcmp(p->ident, "submenu")) {
            item->branch = adp_build_menu_body(p, "submenuend");
        } else if (p->type == ADP_TK_STRING
                   && adp_menu_next_id < ADP_MAX_MENU_ITEMS) {
            adp_entry* k = &adp_menu_keys[adp_menu_next_id];
            int klen = p->strlen_;
            if (klen > ADP_MAXKEYSTR) klen = ADP_MAXKEYSTR;
            memcpy(k->s, p->str, klen);
            k->len   = klen;
            item->id = adp_menu_next_id;
            adp_menu_next_id++;
        }

        if (tail) { tail->next = item; tail = item; }
        else      { head = tail = item; }
    }
    return head;

}

static void adp_apply_menu(void) {

    if (adp_menu_root) ami_menu(stdout, adp_menu_root);

}

#else /* !AMI_CURSES_MENU — tokenize through the menu block and discard */

static void adp_parse_menu_body(adp_parser* p, const char* endkw) {

    for (;;) {
        adp_next(p);
        if (p->type == ADP_TK_EOF || p->type == ADP_TK_ERR) return;
        if (p->type == ADP_TK_IDENT && !strcmp(p->ident, endkw)) return;
        if (p->type == ADP_TK_IDENT && !strcmp(p->ident, "bar")) continue;
        if (p->type == ADP_TK_IDENT && !strcmp(p->ident, "submenu")) {
            adp_parse_menu_body(p, "submenuend");
            continue;
        }
        if (p->type == ADP_TK_STRING) {
            adp_next(p);
            if (p->type == ADP_TK_IDENT && !strcmp(p->ident, "submenu")) {
                adp_parse_menu_body(p, "submenuend");
                continue;
            }
        }
    }

}

#endif /* AMI_CURSES_MENU */

/* -------- hint-line construction -------- */

/* Short human-readable name for each event that makes sense as a key. */
static const struct { int code; const char* name; } adp_keynames[] = {
    { ami_etup,      "Up"       }, { ami_etdown,    "Down"     },
    { ami_etleft,    "Left"     }, { ami_etright,   "Right"    },
    { ami_etleftw,   "WordLeft" }, { ami_etrightw,  "WordRight"},
    { ami_ethome,    "Home"     }, { ami_ethomel,   "Home"     },
    { ami_ethomes,   "Home"     }, { ami_etend,     "End"      },
    { ami_etendl,    "End"      }, { ami_etends,    "End"      },
    { ami_etpagu,    "PgUp"     }, { ami_etpagd,    "PgDn"     },
    { ami_ettab,     "Tab"      }, { ami_etenter,   "Enter"    },
    { ami_etinsert,  "Ins"      }, { ami_etinsertl, "InsLine"  },
    { ami_etinsertt, "InsTog"   }, { ami_etdel,     "Del"      },
    { ami_etdell,    "DelLine"  }, { ami_etdelcf,   "Del>"     },
    { ami_etdelcb,   "BkSp"     }, { ami_etcopy,    "Copy"     },
    { ami_etcopyl,   "CopyLn"   }, { ami_etcan,     "Esc"      },
    { ami_etstop,    "Stop"     }, { ami_etcont,    "Cont"     },
    { ami_etprint,   "Print"    }, { ami_etprintb,  "PrintBlk" },
    { ami_etprints,  "PrintScr" }, { ami_etmenu,    "Menu"     },
    { ami_etterm,    "Quit"     },
    { 0, NULL }
};

/* Append "piece " to adp_hint if it fits. */
static void adp_hint_append(const char* piece) {

    size_t n = strlen(adp_hint);
    size_t plen = strlen(piece);
    if (n + plen + 2 >= sizeof(adp_hint)) return;
    memcpy(adp_hint + n, piece, plen);
    adp_hint[n + plen]     = ' ';
    adp_hint[n + plen + 1] = ' ';
    adp_hint[n + plen + 2] = 0;

}

/* Format "<key>" or "<key>=<label>" into piece[]. */
static void adp_fmt_piece(char* piece, int psize,
                          const char* key, const char* label) {

    int n = 0;
    int klen = (int)strlen(key);
    int llen = label ? (int)strlen(label) : 0;
    if (klen > psize - 1) klen = psize - 1;
    memcpy(piece, key, klen);
    n = klen;
    if (llen > 0 && n < psize - 1) {
        piece[n++] = '=';
        if (llen > psize - 1 - n) llen = psize - 1 - n;
        memcpy(piece + n, label, llen);
        n += llen;
    }
    piece[n] = 0;

}

/* Format "F<n>" or "F<n>=<label>" into piece[]. */
static void adp_fmt_fkey(char* piece, int psize, int k, const char* label) {

    char numbuf[8];
    int  nn = 0, n = 0;
    int  llen = label ? (int)strlen(label) : 0;
    int  kk = k;
    /* integer to decimal, reversed */
    if (kk == 0) numbuf[nn++] = '0';
    while (kk > 0) { numbuf[nn++] = (char)('0' + kk % 10); kk /= 10; }
    if (n < psize - 1) piece[n++] = 'F';
    while (nn > 0 && n < psize - 1) piece[n++] = numbuf[--nn];
    if (llen > 0 && n < psize - 1) {
        piece[n++] = '=';
        if (llen > psize - 1 - n) llen = psize - 1 - n;
        memcpy(piece + n, label, (size_t)llen);
        n += llen;
    }
    piece[n] = 0;

}

/* Build the hint string from loaded mappings. Each mapped event becomes
   "<KeyName>" (if label is empty or just a key echo) or "<KeyName>=<label>"
   (short description of the action). Result goes into adp_hint. */
static void adp_build_hint(void) {

    char   piece[ADP_MAXLABEL + 32];
    int    i;
    const adp_entry* e;

    adp_hint[0] = 0;
    for (i = 0; adp_keynames[i].name; i++) {
        int c = adp_keynames[i].code;
        if (c < 0 || c >= ADP_EVT_COUNT) continue;
        e = &adp_evt[c];
        if (e->len == 0) continue;
        /* avoid duplicate entries for aliased names (e.g., ethome/ethomel
           both print "Home"): skip if an earlier synonym already appeared */
        {
            int j, seen = 0;
            for (j = 0; j < i; j++) {
                int cc = adp_keynames[j].code;
                if (cc >= 0 && cc < ADP_EVT_COUNT
                    && adp_evt[cc].len > 0
                    && !strcmp(adp_keynames[j].name, adp_keynames[i].name)) {
                    seen = 1; break;
                }
            }
            if (seen) continue;
        }
        adp_fmt_piece(piece, (int)sizeof(piece),
                      adp_keynames[i].name, e->label);
        adp_hint_append(piece);
    }
    for (i = 1; i <= ADP_MAXFKEY; i++) {
        if (adp_fkey[i].len == 0) continue;
        adp_fmt_fkey(piece, (int)sizeof(piece), i, adp_fkey[i].label);
        adp_hint_append(piece);
    }

}

/* Count how many rows are needed to display the hint at width cols.
   Uses simple word-wrap (break at spaces). Caps at 4 rows. */
static int adp_hint_rows_for_width(int cols) {

    int rows = 0;
    int col  = 0;
    const char* s = adp_hint;

    if (!*s || cols <= 0) return 0;
    rows = 1;
    while (*s) {
        const char* word = s;
        int wlen;
        while (*s && *s != ' ') s++;
        wlen = (int)(s - word);
        if (col > 0 && col + 1 + wlen > cols) {
            if (rows >= 4) break;
            rows++;
            col = 0;
        }
        if (col > 0) col++;     /* space separator */
        col += wlen;
        if (col >= cols) { if (rows >= 4) break; rows++; col = 0; }
        while (*s == ' ') s++;
    }
    return rows;

}

/* Draw the hint at the reserved bottom rows. Uses Ami direct calls so it
   can run under both terminal.c and graphics.c linkage. Preserves the
   program's cursor position and attribute state. */
static void adp_draw_hint(void) {

    int cx, cy, mx, my;
    int row, col;
    const char* s;

    if (adp_hint_rows == 0 || !adp_hint[0]) return;

    cx = ami_curx(stdout);
    cy = ami_cury(stdout);
    mx = ami_maxx(stdout);
    my = ami_maxy(stdout);

    ami_bcolor(stdout, ami_cyan);
    ami_fcolor(stdout, ami_black);
    s = adp_hint;
    for (row = 0; row < adp_hint_rows; row++) {
        ami_cursor(stdout, 1, my - adp_hint_rows + 1 + row);
        col = 0;
        while (*s) {
            const char* word = s;
            int wlen;
            while (*s && *s != ' ') s++;
            wlen = (int)(s - word);
            if (col > 0 && col + 1 + wlen > mx) break;
            if (col > 0) { putchar(' '); col++; }
            {
                int i;
                for (i = 0; i < wlen && col < mx; i++, col++) putchar(word[i]);
            }
            while (*s == ' ') s++;
        }
        while (col < mx) { putchar(' '); col++; }
    }
    /* reset to curses defaults then re-apply any program-set attrs/pair */
    ami_fcolor(stdout, ami_white);
    ami_bcolor(stdout, ami_black);
    apply_attrs();
    ami_cursor(stdout, cx, cy);

}

/* -------- top-level loader -------- */

static void adp_load(void) {

    char pth[ADP_MAXSTR];
    char nam[ADP_MAXSTR];
    char ext[ADP_MAXSTR];
    char fil[ADP_MAXSTR];
    adp_parser p;
    FILE* f;

    if (adp_loaded) return;
    adp_loaded = 1;

    memset(adp_evt,  0, sizeof(adp_evt));
    memset(adp_fkey, 0, sizeof(adp_fkey));

    /* directory of the running program */
    pth[0] = 0;
    ami_getpgm(pth, ADP_MAXSTR);

    /* basename of the running program */
    nam[0] = 0;
#if defined(__linux) || defined(__MINGW32__)
    if (program_invocation_name && program_invocation_name[0]) {
        ami_brknam(program_invocation_name,
                   ext, ADP_MAXSTR,          /* path (discarded) */
                   nam, ADP_MAXSTR,          /* name */
                   ext, ADP_MAXSTR);         /* ext (discarded) */
    }
#endif
    if (!nam[0]) return;  /* can't determine program name → dormant */

    ami_maknam(fil, ADP_MAXSTR, pth, nam, "adp");
    f = fopen(fil, "r");
    if (!f) return;  /* file absent → dormant */

    p.f = f;
    p.pb = -1;
    for (;;) {
        adp_next(&p);
        if (p.type == ADP_TK_EOF || p.type == ADP_TK_ERR) break;
        if (p.type != ADP_TK_IDENT) continue;
        if (!strcmp(p.ident, "keyequ")) {
            adp_parse_keyequ(&p);
        } else if (!strcmp(p.ident, "menu")) {
#ifdef AMI_CURSES_MENU
            adp_menu_root = adp_build_menu_body(&p, "menuend");
            adp_apply_menu();
#else
            adp_parse_menu_body(&p, "menuend");
#endif
        }
    }
    fclose(f);

    adp_build_hint();

}

/* Look up a mapping for an incoming event; return pointer/length or NULL. */
static const unsigned char* adp_lookup(const ami_evtrec* er, int* out_len) {

    if (!adp_loaded) return NULL;
#ifdef AMI_CURSES_MENU
    if (er->etype == ami_etmenus) {
        int id = er->menuid;
        if (id > 0 && id < ADP_MAX_MENU_ITEMS
            && adp_menu_keys[id].len > 0) {
            *out_len = adp_menu_keys[id].len;
            return adp_menu_keys[id].s;
        }
        return NULL;
    }
#endif
    if (er->etype == ami_etfun) {
        int k = er->fkey;
        if (k >= 1 && k <= ADP_MAXFKEY && adp_fkey[k].len > 0) {
            *out_len = adp_fkey[k].len;
            return adp_fkey[k].s;
        }
    }
    if ((int)er->etype >= 0 && (int)er->etype < ADP_EVT_COUNT
        && adp_evt[er->etype].len > 0) {
        *out_len = adp_evt[er->etype].len;
        return adp_evt[er->etype].s;
    }
    return NULL;

}

/* color pair table: fg/bg for each pair */
static struct { short fg; short bg; } color_pairs[COLOR_PAIRS];

/* screen content buffer for mvinch/winch readback */
static int  scrbuf_inited = 0;
static char scrbuf[256][256];

static void scrbuf_init(void) {

    memset(scrbuf, ' ', sizeof(scrbuf));
    scrbuf_inited = 1;

}

/* the single window */
static WINDOW stdscr_data;
WINDOW* stdscr = &stdscr_data;
int LINES = 25;
int COLS = 80;

/* map curses color to Ami color */
static ami_color color_to_ami(short c) {

    switch (c) {
        case COLOR_BLACK:   return ami_black;
        case COLOR_RED:     return ami_red;
        case COLOR_GREEN:   return ami_green;
        case COLOR_YELLOW:  return ami_yellow;
        case COLOR_BLUE:    return ami_blue;
        case COLOR_MAGENTA: return ami_magenta;
        case COLOR_CYAN:    return ami_cyan;
        case COLOR_WHITE:   return ami_white;
        default:            return ami_white;
    }

}

/* apply the current attribute set to Ami */
static void apply_attrs(void) {

    ami_bold(stdout, (cur_attrs & A_BOLD) ? 1 : 0);
    ami_reverse(stdout, (cur_attrs & A_REVERSE) ? 1 : 0);
    ami_underline(stdout, (cur_attrs & A_UNDERLINE) ? 1 : 0);
    ami_blink(stdout, (cur_attrs & A_BLINK) ? 1 : 0);
    ami_italic(stdout, (cur_attrs & A_ITALIC) ? 1 : 0);
    ami_standout(stdout, (cur_attrs & A_STANDOUT) ? 1 : 0);

    /* apply color pair */
    {
        int pair = PAIR_NUMBER(cur_attrs);
        if (pair > 0 && pair < COLOR_PAIRS) {

            ami_fcolor(stdout, color_to_ami(color_pairs[pair].fg));
            ami_bcolor(stdout, color_to_ami(color_pairs[pair].bg));

        }
    }

}

/* encode a Unicode code point as UTF-8 and write to stdout */
static void put_utf8(int cp) {

    if (cp < 0x80) {

        putchar(cp);

    } else if (cp < 0x800) {

        putchar(0xC0 | (cp >> 6));
        putchar(0x80 | (cp & 0x3F));

    } else {

        putchar(0xE0 | (cp >> 12));
        putchar(0x80 | ((cp >> 6) & 0x3F));
        putchar(0x80 | (cp & 0x3F));

    }

}

/*******************************************************************************

Initialization

*******************************************************************************/

WINDOW* initscr(void) {

    if (cur_initialized) return stdscr;
    cur_initialized = 1;
    cur_ended = 0;
    cur_echo = 0;

    /* Ami terminal auto mode handles scrolling and cursor movement.
       For curses compatibility we turn it off so we have full control.
       Cursor visibility is left at the terminal default — real ncurses
       does not hide it in initscr(); programs call curs_set(0) to hide. */
    ami_auto(stdout, 0);

    /* query screen size */
    LINES = ami_maxy(stdout);
    COLS  = ami_maxx(stdout);

    /* init color pairs to default (pair 0 = terminal's current colors) */
    memset(color_pairs, 0, sizeof(color_pairs));
    color_pairs[0].fg = COLOR_WHITE;
    color_pairs[0].bg = COLOR_BLACK;

    /* load <progname>.adp alongside the executable, if present */
    adp_load();

    /* reserve bottom rows for the adapter hint line, if any */
    adp_hint_rows = adp_hint_rows_for_width(COLS);
    if (adp_hint_rows > 0) {
        LINES -= adp_hint_rows;
        if (LINES < 1) { LINES = 1; adp_hint_rows = 0; }
    }
    adp_draw_hint();

    return stdscr;

}

int endwin(void) {

    if (!cur_initialized) return ERR;
    cur_ended = 1;
    ami_auto(stdout, 1);
    ami_curvis(stdout, 1);
    /* reset attributes */
    ami_bold(stdout, 0);
    ami_reverse(stdout, 0);
    ami_underline(stdout, 0);
    ami_blink(stdout, 0);
    ami_italic(stdout, 0);
    ami_fcolor(stdout, ami_white);
    ami_bcolor(stdout, ami_black);
    return OK;

}

int isendwin(void) { return cur_ended; }

/*******************************************************************************

Output

*******************************************************************************/

int refresh(void) {

    adp_draw_hint();
    fflush(stdout);
    return OK;

}

int clear(void) {

    if (!scrbuf_inited) scrbuf_init();
    else memset(scrbuf, ' ', sizeof(scrbuf));
    putchar('\f');
    return OK;

}

int erase(void) { return clear(); }

int clrtoeol(void) {

    int cx = ami_curx(stdout);
    int cy = ami_cury(stdout);
    int mx = ami_maxx(stdout);
    int i;

    for (i = cx; i <= mx; i++) putchar(' ');
    ami_cursor(stdout, cx, cy);
    return OK;

}

int clrtobot(void) {

    int cx = ami_curx(stdout);
    int cy = ami_cury(stdout);
    int mx = ami_maxx(stdout);
    int my = ami_maxy(stdout);
    int x, y;

    /* clear rest of current line */
    for (x = cx; x <= mx; x++) putchar(' ');
    /* clear remaining lines */
    for (y = cy + 1; y <= my; y++) {

        ami_cursor(stdout, 1, y);
        for (x = 1; x <= mx; x++) putchar(' ');

    }
    ami_cursor(stdout, cx, cy);
    return OK;

}

int move(int y, int x) {

    /* curses: 0-based (y, x). Ami: 1-based (x, y) */
    ami_cursor(stdout, x + 1, y + 1);
    return OK;

}

int addch(int ch) {

    if (!scrbuf_inited) scrbuf_init();
    /* track screen contents for mvinch/winch readback */
    {
        int cx = ami_curx(stdout) - 1;
        int cy = ami_cury(stdout) - 1;
        if (cy >= 0 && cy < 256 && cx >= 0 && cx < 256)
            scrbuf[cy][cx] = (char)(ch & 0x7F);
    }
    if (ch > 0x7F) put_utf8(ch); /* ACS/Unicode character */
    else putchar(ch);
    return OK;

}

int addstr(const char* str) {

    while (*str) addch((unsigned char)*str++);
    return OK;

}

int addnstr(const char* str, int n) {

    int i;
    for (i = 0; i < n && str[i]; i++) addch((unsigned char)str[i]);
    return OK;

}

int mvaddch(int y, int x, int ch) {

    move(y, x);
    return addch(ch);

}

int mvaddstr(int y, int x, const char* str) {

    move(y, x);
    return addstr(str);

}

int printw(const char* fmt, ...) {

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return OK;

}

int mvprintw(int y, int x, const char* fmt, ...) {

    va_list ap;
    move(y, x);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return OK;

}

/*******************************************************************************

Input

*******************************************************************************/

int getch(void) {

    ami_evtrec er;

    /* emit queued chars from a previous adapter mapping */
    if (adp_outpos < adp_outlen) {
        return (int)adp_outbuf[adp_outpos++];
    }

    /* check ungetch buffer first */
    if (cur_ungetch >= 0) {

        int ch = cur_ungetch;
        cur_ungetch = -1;
        return ch;

    }

    /* if timeout is set, use timer-based approach */
    if (cur_nodelay || cur_timeout_ms == 0) {

        /* non-blocking: check for event without waiting */
        /* Ami doesn't have a non-blocking event check, so we use a
           short timer. For simplicity, return ERR immediately. */
        return ERR;

    }

    /* blocking wait for event */
    while (1) {

        const unsigned char* mapped;
        int mlen = 0;

        ami_event(stdin, &er);

        /* adapter override: if the .adp file mapped this event, emit
           the mapped string one character per getch() call. */
        mapped = adp_lookup(&er, &mlen);
        if (mapped && mlen > 0) {
            memcpy(adp_outbuf, mapped, (size_t)mlen);
            adp_outlen = mlen;
            adp_outpos = 1;
            return (int)adp_outbuf[0];
        }

        switch (er.etype) {

            case ami_etchar:   return er.echar;
            case ami_etup:     return KEY_UP;
            case ami_etdown:   return KEY_DOWN;
            case ami_etleft:   return KEY_LEFT;
            case ami_etright:  return KEY_RIGHT;
            case ami_ethome:   return KEY_HOME;
            case ami_ethomel:  return KEY_HOME;
            case ami_etend:    return KEY_END;
            case ami_etendl:   return KEY_END;
            case ami_etpagu:   return KEY_PPAGE;
            case ami_etpagd:   return KEY_NPAGE;
            case ami_etenter:  return '\n';
            case ami_etdel:    return KEY_BACKSPACE;
            case ami_etdelcf:  return KEY_DC;
            case ami_etdelcb:  return KEY_BACKSPACE;
            case ami_etinsertt: return KEY_IC;
            case ami_etterm:   return ERR;
            case ami_etfun:    return KEY_F(er.fkey);
            case ami_etresize:
                LINES = ami_maxy(stdout);
                COLS  = ami_maxx(stdout);
                return KEY_RESIZE;
            default:           break; /* ignore other events, loop */

        }

    }

}

int nodelay(WINDOW* win, int bf) {

    (void)win;
    cur_nodelay = bf;
    return OK;

}

int timeout(int delay) {

    cur_timeout_ms = delay;
    if (delay == 0) cur_nodelay = 1;
    else cur_nodelay = 0;
    return OK;

}

int keypad(WINDOW* win, int bf) {

    (void)win;
    cur_keypad = bf;
    return OK;

}

int cbreak(void)   { return OK; } /* Ami is always in cbreak mode */
int nocbreak(void)  { return OK; }
int raw(void)       { return OK; }
int noraw(void)     { return OK; }
int echo(void)      { cur_echo = 1; return OK; }
int noecho(void)    { cur_echo = 0; return OK; }

int halfdelay(int tenths) {

    cur_timeout_ms = tenths * 100;
    cur_nodelay = 0;
    return OK;

}

int ungetch(int ch) {

    cur_ungetch = ch;
    return OK;

}

/*******************************************************************************

Attributes

*******************************************************************************/

int attron(int attrs) {

    cur_attrs |= (attr_t)attrs;
    apply_attrs();
    return OK;

}

int attroff(int attrs) {

    cur_attrs &= ~(attr_t)attrs;
    apply_attrs();
    return OK;

}

int attrset(int attrs) {

    cur_attrs = (attr_t)attrs;
    apply_attrs();
    return OK;

}

/*******************************************************************************

Color

*******************************************************************************/

int has_colors(void) { return TRUE; }

int start_color(void) {

    /* Ami always has color support */
    return OK;

}

int init_pair(short pair, short fg, short bg) {

    if (pair < 0 || pair >= COLOR_PAIRS) return ERR;
    color_pairs[pair].fg = fg;
    color_pairs[pair].bg = bg;
    return OK;

}

/*******************************************************************************

Cursor

*******************************************************************************/

int curs_set(int visibility) {

    ami_curvis(stdout, visibility > 0 ? 1 : 0);
    return OK;

}

/*******************************************************************************

Screen size

*******************************************************************************/

int getmaxx(WINDOW* win) { (void)win; return COLS; }
int getmaxy(WINDOW* win) { (void)win; return LINES; }

/* mvinch: move to position and return the character there.
   Ami doesn't have a "read character at position" function, so we
   return ' ' as a fallback. The snake game uses this to check if a
   position is empty — we track the screen content in a simple buffer. */

int mvinch(int y, int x) {

    if (!scrbuf_inited) scrbuf_init();
    if (y < 0 || y >= 256 || x < 0 || x >= 256) return ' ';
    return (unsigned char)scrbuf[y][x];

}

/*******************************************************************************

Miscellaneous

*******************************************************************************/

int napms(int ms) {

    usleep(ms * 1000);
    return OK;

}

int beep(void) { return OK; }
int baudrate(void) { return 38400; }
int scrollok(WINDOW* win, int bf) { (void)win; (void)bf; return OK; }
int touchwin(WINDOW* win) { (void)win; return OK; }
int leaveok(WINDOW* win, int bf) { (void)win; (void)bf; return OK; }
int standout(void) { return attron(A_STANDOUT); }
int standend(void) { return attrset(A_NORMAL); }
int nl(void) { return OK; }
int nonl(void) { return OK; }
char erasechar(void) { return '\b'; }
char killchar(void) { return 0x15; /* Ctrl-U */ }
int delwin(WINDOW* win) { (void)win; return OK; }
int doupdate(void) { fflush(stdout); return OK; }
int wnoutrefresh(WINDOW* win) { (void)win; return OK; }
int clearok(WINDOW* win, int bf) { (void)win; (void)bf; return OK; }
int use_default_colors(void) { return OK; }

int waddnstr(WINDOW* win, const char* str, int n) {

    int i;
    (void)win;
    for (i = 0; i < n && str[i]; i++) addch((unsigned char)str[i]);
    return OK;

}

int wredrawln(WINDOW* win, int beg, int num) {

    (void)win; (void)beg; (void)num;
    return OK;

}

int savetty(void) { return OK; }
int resetty(void) { return OK; }
int typeahead(int fd) { (void)fd; return OK; }
int meta(WINDOW* win, int bf) { (void)win; (void)bf; return OK; }
int idlok(WINDOW* win, int bf) { (void)win; (void)bf; return OK; }

mmask_t mousemask(mmask_t newmask, mmask_t* oldmask) {

    if (oldmask) *oldmask = 0;
    (void)newmask;
    return 0;

}

int getmouse(MEVENT* event) {

    (void)event;
    return ERR;

}

int ungetmouse(MEVENT* event) {

    (void)event;
    return ERR;

}

int winch(WINDOW* win) {

    int cx, cy;
    (void)win;
    if (!scrbuf_inited) scrbuf_init();
    cx = ami_curx(stdout) - 1;
    cy = ami_cury(stdout) - 1;
    if (cy < 0 || cy >= 256 || cx < 0 || cx >= 256) return ' ';
    return (unsigned char)scrbuf[cy][cx];

}

/*******************************************************************************

Box drawing

*******************************************************************************/

int box(WINDOW* win, int verch, int horch) {

    int x, y;

    (void)win;
    if (!verch) verch = ACS_VLINE;
    if (!horch) horch = ACS_HLINE;

    /* top row */
    move(0, 0);
    addch(ACS_ULCORNER);
    for (x = 1; x < COLS - 1; x++) addch(horch);
    addch(ACS_URCORNER);

    /* side lines */
    for (y = 1; y < LINES - 1; y++) {

        mvaddch(y, 0, verch);
        mvaddch(y, COLS - 1, verch);

    }

    /* bottom row */
    move(LINES - 1, 0);
    addch(ACS_LLCORNER);
    for (x = 1; x < COLS - 1; x++) addch(horch);
    addch(ACS_LRCORNER);

    return OK;

}

int hline(int ch, int n) {

    int i;
    if (!ch) ch = ACS_HLINE;
    for (i = 0; i < n; i++) addch(ch);
    return OK;

}

int vline(int ch, int n) {

    int cx = ami_curx(stdout) - 1; /* back to 0-based */
    int cy = ami_cury(stdout) - 1;
    int i;

    if (!ch) ch = ACS_VLINE;
    for (i = 0; i < n; i++) {

        mvaddch(cy + i, cx, ch);

    }
    return OK;

}

int mvhline(int y, int x, int ch, int n) { move(y, x); return hline(ch, n); }
int mvvline(int y, int x, int ch, int n) { move(y, x); return vline(ch, n); }

/*******************************************************************************

Window functions (minimal — all map to stdscr with an origin offset)

Curses windows are mapped to stdscr with a stored origin. Drawing calls
offset by the window's begin_y/begin_x. This is sufficient for programs
that use a small number of non-overlapping windows (like worm's status
bar + game area).

*******************************************************************************/

#define MAX_WINS 16
static struct { int used; int by; int bx; int ny; int nx; } wins[MAX_WINS];

WINDOW* newwin(int nlines, int ncols, int begin_y, int begin_x) {

    int i;
    for (i = 1; i < MAX_WINS; i++) {

        if (!wins[i].used) {

            wins[i].used = 1;
            wins[i].by = begin_y;
            wins[i].bx = begin_x;
            wins[i].ny = nlines;
            wins[i].nx = ncols;
            return (WINDOW*)((long)i);

        }

    }
    return stdscr;

}

static void win_origin(WINDOW* win, int* oy, int* ox) {

    long idx = (long)win;
    if (idx > 0 && idx < MAX_WINS && wins[idx].used) {

        *oy = wins[idx].by;
        *ox = wins[idx].bx;

    } else {

        *oy = 0;
        *ox = 0;

    }

}

int wrefresh(WINDOW* win) { (void)win; return refresh(); }

int wmove(WINDOW* win, int y, int x) {

    int oy, ox;
    win_origin(win, &oy, &ox);
    return move(oy + y, ox + x);

}

int waddch(WINDOW* win, int ch) { (void)win; return addch(ch); }
int waddstr(WINDOW* win, const char* str) { (void)win; return addstr(str); }

int mvwaddch(WINDOW* win, int y, int x, int ch) {

    wmove(win, y, x);
    return addch(ch);

}

int mvwaddstr(WINDOW* win, int y, int x, const char* str) {

    wmove(win, y, x);
    return addstr(str);

}

int wprintw(WINDOW* win, const char* fmt, ...) {

    va_list ap;
    (void)win;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return OK;

}

int mvwprintw(WINDOW* win, int y, int x, const char* fmt, ...) {

    va_list ap;
    wmove(win, y, x);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return OK;

}

int mvwaddnstr(WINDOW* win, int y, int x, const char* str, int n) {

    wmove(win, y, x);
    return waddnstr(win, str, n);

}

int wclear(WINDOW* win) { (void)win; return clear(); }
int wclrtoeol(WINDOW* win) { (void)win; return clrtoeol(); }
int wgetch(WINDOW* win) { (void)win; return getch(); }
int wattron(WINDOW* win, int attrs) { (void)win; return attron(attrs); }
int wattroff(WINDOW* win, int attrs) { (void)win; return attroff(attrs); }
