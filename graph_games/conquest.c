/*******************************************************************************
*                                                                              *
*                              CONQUEST GAME                                   *
*                                                                              *
*                       COPYRIGHT (C) 2026 S. A. FRANCO                        *
*                                                                              *
* A territory conquest strategy game with real world geography. Players take   *
* turns deploying armies, attacking adjacent territories with dice combat,     *
* and fortifying positions in pursuit of world domination.                     *
*                                                                              *
*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include <localdefs.h>
#include <sound.h>
#include <graphics.h>

/*******************************************************************************

Defines

*******************************************************************************/

#define SECOND       10000                  /* one second in timer units */

/* timer IDs */
#define TIMER_AI     1                      /* AI thinking delay */
#define TIMER_DICE   2                      /* dice display timer */
#define TIMER_MSG    3                      /* message display timer */

/* timer intervals */
#define AI_DELAY     3000                   /* ~300ms AI delay */
#define DICE_DELAY   10000                  /* 1s dice display */
#define MSG_DELAY    15000                  /* 1.5s message display */

/* menu IDs */
#define MENU_NEW     100
#define MENU_EXIT    101
#define MENU_ABOUT   102
#define MENU_2P      110
#define MENU_3P      111
#define MENU_4P      112
#define MENU_5P      113
#define MENU_P1H     120
#define MENU_P1C     121
#define MENU_P2H     122
#define MENU_P2C     123
#define MENU_P3H     124
#define MENU_P3C     125
#define MENU_P4H     126
#define MENU_P4C     127
#define MENU_P5H     128
#define MENU_P5C     129

/* sound defines */
#define DICE_NOTE    (AMI_NOTE_E+AMI_OCTAVE_5)
#define CONQUER_NOTE (AMI_NOTE_C+AMI_OCTAVE_6)
#define DEPLOY_NOTE  (AMI_NOTE_G+AMI_OCTAVE_5)
#define LOSE_NOTE    (AMI_NOTE_C+AMI_OCTAVE_3)
#define WIN_NOTE     (AMI_NOTE_C+AMI_OCTAVE_7)
#define DICE_DUR     150
#define CONQUER_DUR  400
#define DEPLOY_DUR   100
#define LOSE_DUR     800
#define WIN_DUR      1000

/* color helper: scale 0-255 to 0-INT_MAX */
#define CLR(v) ((v) * (INT_MAX / 255))

/* territory count */
#define NUM_TERR     44

/* max adjacencies per territory */
#define MAX_ADJ      8

/* continents */
#define CONT_NAMERICA   0
#define CONT_SAMERICA   1
#define CONT_EUROPE     2
#define CONT_AFRICA     3
#define CONT_MIDEAST    4
#define CONT_ASIA       5
#define CONT_OCEANIA    6
#define NUM_CONTINENTS  7

/* max players */
#define MAX_PLAYERS  5

/* game phases */
#define PHASE_DEPLOY   0
#define PHASE_ATTACK   1
#define PHASE_FORTIFY  2

/* game states */
#define STATE_SETUP    0     /* pre-game */
#define STATE_PLAYING  1
#define STATE_GAMEOVER 2

/* max dice */
#define MAX_ATT_DICE   3
#define MAX_DEF_DICE   2

/*******************************************************************************

Types

*******************************************************************************/

typedef struct {
    const char *name;
    int continent;
    int owner;              /* player index or -1 */
    int armies;
    int nadj;
    int adj[MAX_ADJ];       /* indices of adjacent territories */
    float mx, my;           /* marker position, normalized 0.0-1.0 */
} territory_t;

typedef struct {
    int active;             /* is this player in the game? */
    int human;              /* TRUE = human, FALSE = computer */
    int territories;        /* count of owned territories */
    int cards;              /* simplified: count of cards held */
    int eliminated;         /* TRUE if knocked out */
} player_t;

typedef struct {
    const char *name;
    int bonus;
    int start;              /* first territory index */
    int count;              /* number of territories */
} continent_t;

/*******************************************************************************

Territory data

*******************************************************************************/

/* territory indices */
#define T_ALASKA        0
#define T_WCANADA       1
#define T_ECANADA       2
#define T_PACCOAST      3
#define T_MTNWEST       4
#define T_CENTRALUS     5
#define T_SOUTHERNUS    6
#define T_NORTHEASTUS   7
#define T_MEXICO        8
#define T_CENAMERICA    9
#define T_COLVEN        10
#define T_BRAZILN       11
#define T_BRAZILS       12
#define T_PERUBOL       13
#define T_ARGCHILE      14
#define T_BRITIRE       15
#define T_SCANDINAVIA   16
#define T_WEUROPE       17
#define T_CENEUROPE     18
#define T_SEUROPE       19
#define T_EEUROPE       20
#define T_UKRBALK       21
#define T_NAFRICA       22
#define T_WAFRICA       23
#define T_EAFRICA       24
#define T_CENAFRICA     25
#define T_SAFRICA       26
#define T_MADAGASCAR    27
#define T_TURKEYCAUC    28
#define T_MIDDLEEAST    29
#define T_ARABPEN       30
#define T_CENTRALASIA   31
#define T_RUSSIAW       32
#define T_RUSSIACEN     33
#define T_RUSSIAE       34
#define T_CHINAN        35
#define T_CHINAS        36
#define T_INDIA         37
#define T_SEASIA        38
#define T_KOREAJAPAN    39
#define T_WAUSTRALIA    40
#define T_EAUSTRALIA    41
#define T_NEWZEALAND    42
#define T_INDOPAPUA     43

/*******************************************************************************

Static territory initialization

*******************************************************************************/

territory_t territories[NUM_TERR] = {
    /* North America (0-9) */
    { "Alaska",       CONT_NAMERICA, -1, 0, 3, { T_WCANADA, T_PACCOAST, T_RUSSIAE, -1,-1,-1,-1,-1 } },
    { "W Canada",     CONT_NAMERICA, -1, 0, 4, { T_ALASKA, T_ECANADA, T_PACCOAST, T_MTNWEST, -1,-1,-1,-1 } },
    { "E Canada",     CONT_NAMERICA, -1, 0, 4, { T_WCANADA, T_MTNWEST, T_CENTRALUS, T_NORTHEASTUS, -1,-1,-1,-1 } },
    { "Pac Coast",    CONT_NAMERICA, -1, 0, 4, { T_ALASKA, T_WCANADA, T_MTNWEST, T_MEXICO, -1,-1,-1,-1 } },
    { "Mtn West",     CONT_NAMERICA, -1, 0, 5, { T_WCANADA, T_ECANADA, T_PACCOAST, T_CENTRALUS, T_MEXICO, -1,-1,-1 } },
    { "Central US",   CONT_NAMERICA, -1, 0, 5, { T_ECANADA, T_MTNWEST, T_NORTHEASTUS, T_SOUTHERNUS, T_MEXICO, -1,-1,-1 } },
    { "Southern US",  CONT_NAMERICA, -1, 0, 4, { T_CENTRALUS, T_NORTHEASTUS, T_MEXICO, T_CENAMERICA, -1,-1,-1,-1 } },
    { "NE US",        CONT_NAMERICA, -1, 0, 3, { T_ECANADA, T_CENTRALUS, T_SOUTHERNUS, -1,-1,-1,-1,-1 } },
    { "Mexico",       CONT_NAMERICA, -1, 0, 5, { T_PACCOAST, T_MTNWEST, T_CENTRALUS, T_SOUTHERNUS, T_CENAMERICA, -1,-1,-1 } },
    { "C America",    CONT_NAMERICA, -1, 0, 3, { T_MEXICO, T_SOUTHERNUS, T_COLVEN, -1,-1,-1,-1,-1 } },

    /* South America (10-14) */
    { "Col/Ven",      CONT_SAMERICA, -1, 0, 3, { T_CENAMERICA, T_BRAZILN, T_PERUBOL, -1,-1,-1,-1,-1 } },
    { "Brazil N",     CONT_SAMERICA, -1, 0, 4, { T_COLVEN, T_BRAZILS, T_PERUBOL, T_NAFRICA, -1,-1,-1,-1 } },
    { "Brazil S",     CONT_SAMERICA, -1, 0, 3, { T_BRAZILN, T_PERUBOL, T_ARGCHILE, -1,-1,-1,-1,-1 } },
    { "Peru/Bol",     CONT_SAMERICA, -1, 0, 4, { T_COLVEN, T_BRAZILN, T_BRAZILS, T_ARGCHILE, -1,-1,-1,-1 } },
    { "Arg/Chile",    CONT_SAMERICA, -1, 0, 2, { T_BRAZILS, T_PERUBOL, -1,-1,-1,-1,-1,-1 } },

    /* Europe (15-21) */
    { "Brit/Ire",     CONT_EUROPE, -1, 0, 3, { T_SCANDINAVIA, T_WEUROPE, T_CENEUROPE, -1,-1,-1,-1,-1 } },
    { "Scandinavia",  CONT_EUROPE, -1, 0, 3, { T_BRITIRE, T_CENEUROPE, T_RUSSIAW, -1,-1,-1,-1,-1 } },
    { "W Europe",     CONT_EUROPE, -1, 0, 4, { T_BRITIRE, T_CENEUROPE, T_SEUROPE, T_NAFRICA, -1,-1,-1,-1 } },
    { "Cen Europe",   CONT_EUROPE, -1, 0, 6, { T_BRITIRE, T_SCANDINAVIA, T_WEUROPE, T_SEUROPE, T_EEUROPE, T_UKRBALK, -1,-1 } },
    { "S Europe",     CONT_EUROPE, -1, 0, 5, { T_WEUROPE, T_CENEUROPE, T_UKRBALK, T_NAFRICA, T_TURKEYCAUC, -1,-1,-1 } },
    { "E Europe",     CONT_EUROPE, -1, 0, 4, { T_CENEUROPE, T_UKRBALK, T_RUSSIAW, T_TURKEYCAUC, -1,-1,-1,-1 } },
    { "Ukr/Balk",     CONT_EUROPE, -1, 0, 5, { T_CENEUROPE, T_SEUROPE, T_EEUROPE, T_TURKEYCAUC, T_MIDDLEEAST, -1,-1,-1 } },

    /* Africa (22-27) */
    { "N Africa",     CONT_AFRICA, -1, 0, 5, { T_BRAZILN, T_WEUROPE, T_SEUROPE, T_WAFRICA, T_EAFRICA, -1,-1,-1 } },
    { "W Africa",     CONT_AFRICA, -1, 0, 4, { T_NAFRICA, T_EAFRICA, T_CENAFRICA, T_SAFRICA, -1,-1,-1,-1 } },
    { "E Africa",     CONT_AFRICA, -1, 0, 5, { T_NAFRICA, T_WAFRICA, T_CENAFRICA, T_ARABPEN, T_MADAGASCAR, -1,-1,-1 } },
    { "Cen Africa",   CONT_AFRICA, -1, 0, 4, { T_WAFRICA, T_EAFRICA, T_SAFRICA, T_MADAGASCAR, -1,-1,-1,-1 } },
    { "S Africa",     CONT_AFRICA, -1, 0, 3, { T_WAFRICA, T_CENAFRICA, T_MADAGASCAR, -1,-1,-1,-1,-1 } },
    { "Madagascar",   CONT_AFRICA, -1, 0, 3, { T_EAFRICA, T_CENAFRICA, T_SAFRICA, -1,-1,-1,-1,-1 } },

    /* Middle East & Central Asia (28-31) */
    { "Turkey",       CONT_MIDEAST, -1, 0, 5, { T_SEUROPE, T_EEUROPE, T_UKRBALK, T_MIDDLEEAST, T_CENTRALASIA, -1,-1,-1 } },
    { "Mid East",     CONT_MIDEAST, -1, 0, 5, { T_UKRBALK, T_TURKEYCAUC, T_ARABPEN, T_CENTRALASIA, T_INDIA, -1,-1,-1 } },
    { "Arab Pen",     CONT_MIDEAST, -1, 0, 3, { T_EAFRICA, T_MIDDLEEAST, T_INDIA, -1,-1,-1,-1,-1 } },
    { "Cen Asia",     CONT_MIDEAST, -1, 0, 5, { T_TURKEYCAUC, T_MIDDLEEAST, T_RUSSIAW, T_RUSSIACEN, T_CHINAN, -1,-1,-1 } },

    /* Asia (32-39) */
    { "Russia W",     CONT_ASIA, -1, 0, 4, { T_SCANDINAVIA, T_EEUROPE, T_RUSSIACEN, T_CENTRALASIA, -1,-1,-1,-1 } },
    { "Russia C",     CONT_ASIA, -1, 0, 4, { T_RUSSIAW, T_RUSSIAE, T_CENTRALASIA, T_CHINAN, -1,-1,-1,-1 } },
    { "Russia E",     CONT_ASIA, -1, 0, 4, { T_ALASKA, T_RUSSIACEN, T_CHINAN, T_KOREAJAPAN, -1,-1,-1,-1 } },
    { "China N",      CONT_ASIA, -1, 0, 6, { T_CENTRALASIA, T_RUSSIACEN, T_RUSSIAE, T_CHINAS, T_KOREAJAPAN, T_INDIA, -1,-1 } },
    { "China S",      CONT_ASIA, -1, 0, 4, { T_CHINAN, T_INDIA, T_SEASIA, T_KOREAJAPAN, -1,-1,-1,-1 } },
    { "India",        CONT_ASIA, -1, 0, 5, { T_MIDDLEEAST, T_ARABPEN, T_CHINAN, T_CHINAS, T_SEASIA, -1,-1,-1 } },
    { "SE Asia",      CONT_ASIA, -1, 0, 4, { T_CHINAS, T_INDIA, T_INDOPAPUA, T_KOREAJAPAN, -1,-1,-1,-1 } },
    { "Korea/Jpn",    CONT_ASIA, -1, 0, 4, { T_RUSSIAE, T_CHINAN, T_CHINAS, T_SEASIA, -1,-1,-1,-1 } },

    /* Oceania (40-43) */
    { "W Australia",  CONT_OCEANIA, -1, 0, 3, { T_EAUSTRALIA, T_INDOPAPUA, T_NEWZEALAND, -1,-1,-1,-1,-1 } },
    { "E Australia",  CONT_OCEANIA, -1, 0, 3, { T_WAUSTRALIA, T_INDOPAPUA, T_NEWZEALAND, -1,-1,-1,-1,-1 } },
    { "New Zealand",  CONT_OCEANIA, -1, 0, 2, { T_WAUSTRALIA, T_EAUSTRALIA, -1,-1,-1,-1,-1,-1 } },
    { "Indo/Papua",   CONT_OCEANIA, -1, 0, 3, { T_SEASIA, T_WAUSTRALIA, T_EAUSTRALIA, -1,-1,-1,-1,-1 } }
};

continent_t continents[NUM_CONTINENTS] = {
    { "N America",  5, 0,  10 },
    { "S America",  2, 10,  5 },
    { "Europe",     5, 15,  7 },
    { "Africa",     3, 22,  6 },
    { "Mid East",   3, 28,  4 },
    { "Asia",       7, 32,  8 },
    { "Oceania",    2, 40,  4 }
};

/*******************************************************************************

Player colors

*******************************************************************************/

int player_colors[MAX_PLAYERS][3] = {
    { 220,  50,  50 },  /* red */
    {  50, 100, 220 },  /* blue */
    {  50, 180,  50 },  /* green */
    { 220, 200,  50 },  /* yellow */
    { 160,  50, 200 }   /* purple */
};

/*******************************************************************************

Global state

*******************************************************************************/

/* screen metrics */
int scr_w, scr_h;
int map_x, map_y, map_w, map_h;   /* map area */
int status_h;                      /* status bar height */

/* game state */
player_t players[MAX_PLAYERS];
int num_players = 3;
int cur_player;
int phase;
int game_state = STATE_SETUP;
int flip;

/* selection state */
int sel_source = -1;       /* selected source territory */
int sel_target = -1;       /* selected target territory */
int hover_terr = -1;       /* territory under mouse */

/* dice results */
int att_dice[MAX_ATT_DICE];
int def_dice[MAX_DEF_DICE];
int num_att_dice, num_def_dice;
int show_dice;             /* TRUE while dice are displayed */
int att_wins, def_wins;    /* results of last battle */

/* card trade tracking */
int card_sets_traded = 0;  /* global count of sets traded */
int conquered_this_turn;   /* did current player conquer a territory? */

/* message display */
char msg_text[256];
int show_msg;

/* mouse tracking */
int mouse_x, mouse_y;

/* "Done" button area */
int done_bx, done_by, done_bw, done_bh;

/* AI timer pending */
int ai_pending;

/*******************************************************************************

Menu setup

*******************************************************************************/

ami_menuptr newmenuitem(int onoff, int oneof, int bar, int id,
                        const char *face)
{
    ami_menuptr mp;

    mp = malloc(sizeof(ami_menurec));
    mp->next = NULL;
    mp->branch = NULL;
    mp->onoff = onoff;
    mp->oneof = oneof;
    mp->bar = bar;
    mp->id = id;
    mp->face = malloc(strlen(face) + 1);
    strcpy(mp->face, face);
    return mp;
}

void appendmenu(ami_menuptr *list, ami_menuptr item)
{
    ami_menuptr p;

    if (*list == NULL) {
        *list = item;
    } else {
        p = *list;
        while (p->next) p = p->next;
        p->next = item;
    }
}

void setup_menu(void)
{
    ami_menuptr menu = NULL;
    ami_menuptr game_menu, game_items;
    ami_menuptr plyr_menu, plyr_items;
    ami_menuptr help_menu, help_items;

    /* Game menu */
    game_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Game");
    appendmenu(&menu, game_menu);
    game_items = NULL;
    appendmenu(&game_items,
        newmenuitem(FALSE, FALSE, TRUE, MENU_NEW, "New Game"));
    appendmenu(&game_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_EXIT, "Exit"));
    game_menu->branch = game_items;

    /* Players menu */
    plyr_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Players");
    appendmenu(&menu, plyr_menu);
    plyr_items = NULL;
    appendmenu(&plyr_items,
        newmenuitem(FALSE, TRUE, TRUE, MENU_2P, "2 Players"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_3P, "3 Players"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_4P, "4 Players"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_5P, "5 Players"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, TRUE, MENU_P1H, "P1: Human"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P1C, "P1: Computer"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P2H, "P2: Human"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P2C, "P2: Computer"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P3H, "P3: Human"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P3C, "P3: Computer"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P4H, "P4: Human"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P4C, "P4: Computer"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P5H, "P5: Human"));
    appendmenu(&plyr_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_P5C, "P5: Computer"));
    plyr_menu->branch = plyr_items;

    /* Help menu */
    help_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Help");
    appendmenu(&menu, help_menu);
    help_items = NULL;
    appendmenu(&help_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_ABOUT, "About Conquest"));
    help_menu->branch = help_items;

    ami_menu(stdout, menu);
}

/*******************************************************************************

Calculate screen metrics

*******************************************************************************/

void calc_metrics(void)
{

    scr_w = ami_maxxg(stdout);
    scr_h = ami_maxyg(stdout);
    status_h = scr_h / 12;
    map_x = scr_w / 40;
    map_y = scr_h / 40;
    map_w = scr_w - map_x * 2;
    map_h = scr_h - status_h - map_y * 2;

    /* Done button in status bar */
    done_bw = scr_w / 8;
    done_bh = status_h * 2 / 3;
    done_bx = scr_w - done_bw - scr_w / 40;
    done_by = scr_h - status_h + (status_h - done_bh) / 2;

}

/*******************************************************************************

Initialize territory marker positions

*******************************************************************************/

void init_markers(void)
{
    /* North America */
    territories[T_ALASKA].mx      = 0.055f; territories[T_ALASKA].my      = 0.11f;
    territories[T_WCANADA].mx     = 0.12f;  territories[T_WCANADA].my     = 0.16f;
    territories[T_ECANADA].mx     = 0.22f;  territories[T_ECANADA].my     = 0.13f;
    territories[T_PACCOAST].mx    = 0.09f;  territories[T_PACCOAST].my    = 0.27f;
    territories[T_MTNWEST].mx     = 0.13f;  territories[T_MTNWEST].my     = 0.27f;
    territories[T_CENTRALUS].mx   = 0.17f;  territories[T_CENTRALUS].my   = 0.27f;
    territories[T_SOUTHERNUS].mx  = 0.17f;  territories[T_SOUTHERNUS].my  = 0.35f;
    territories[T_NORTHEASTUS].mx = 0.21f;  territories[T_NORTHEASTUS].my = 0.27f;
    territories[T_MEXICO].mx      = 0.11f;  territories[T_MEXICO].my      = 0.42f;
    territories[T_CENAMERICA].mx  = 0.13f;  territories[T_CENAMERICA].my  = 0.49f;

    /* South America */
    territories[T_COLVEN].mx      = 0.22f;  territories[T_COLVEN].my      = 0.55f;
    territories[T_BRAZILN].mx     = 0.27f;  territories[T_BRAZILN].my     = 0.58f;
    territories[T_BRAZILS].mx     = 0.26f;  territories[T_BRAZILS].my     = 0.68f;
    territories[T_PERUBOL].mx     = 0.21f;  territories[T_PERUBOL].my     = 0.68f;
    territories[T_ARGCHILE].mx    = 0.22f;  territories[T_ARGCHILE].my    = 0.78f;

    /* Europe */
    territories[T_BRITIRE].mx     = 0.35f;  territories[T_BRITIRE].my     = 0.22f;
    territories[T_SCANDINAVIA].mx = 0.39f;  territories[T_SCANDINAVIA].my = 0.12f;
    territories[T_WEUROPE].mx     = 0.37f;  territories[T_WEUROPE].my     = 0.32f;
    territories[T_CENEUROPE].mx   = 0.39f;  territories[T_CENEUROPE].my   = 0.27f;
    territories[T_SEUROPE].mx     = 0.39f;  territories[T_SEUROPE].my     = 0.35f;
    territories[T_EEUROPE].mx     = 0.42f;  territories[T_EEUROPE].my     = 0.24f;
    territories[T_UKRBALK].mx     = 0.44f;  territories[T_UKRBALK].my     = 0.29f;

    /* Africa */
    territories[T_NAFRICA].mx     = 0.43f;  territories[T_NAFRICA].my     = 0.47f;
    territories[T_WAFRICA].mx     = 0.42f;  territories[T_WAFRICA].my     = 0.55f;
    territories[T_EAFRICA].mx     = 0.49f;  territories[T_EAFRICA].my     = 0.55f;
    territories[T_CENAFRICA].mx   = 0.47f;  territories[T_CENAFRICA].my   = 0.62f;
    territories[T_SAFRICA].mx     = 0.47f;  territories[T_SAFRICA].my     = 0.72f;
    territories[T_MADAGASCAR].mx  = 0.52f;  territories[T_MADAGASCAR].my  = 0.68f;

    /* Middle East & Central Asia */
    territories[T_TURKEYCAUC].mx  = 0.45f;  territories[T_TURKEYCAUC].my  = 0.37f;
    territories[T_MIDDLEEAST].mx  = 0.48f;  territories[T_MIDDLEEAST].my  = 0.42f;
    territories[T_ARABPEN].mx     = 0.49f;  territories[T_ARABPEN].my     = 0.48f;
    territories[T_CENTRALASIA].mx = 0.52f;  territories[T_CENTRALASIA].my = 0.32f;

    /* Asia */
    territories[T_RUSSIAW].mx     = 0.48f;  territories[T_RUSSIAW].my     = 0.16f;
    territories[T_RUSSIACEN].mx   = 0.57f;  territories[T_RUSSIACEN].my   = 0.12f;
    territories[T_RUSSIAE].mx     = 0.69f;  territories[T_RUSSIAE].my     = 0.11f;
    territories[T_CHINAN].mx      = 0.66f;  territories[T_CHINAN].my      = 0.32f;
    territories[T_CHINAS].mx      = 0.67f;  territories[T_CHINAS].my      = 0.42f;
    territories[T_INDIA].mx       = 0.58f;  territories[T_INDIA].my       = 0.45f;
    territories[T_SEASIA].mx      = 0.65f;  territories[T_SEASIA].my      = 0.48f;
    territories[T_KOREAJAPAN].mx  = 0.72f;  territories[T_KOREAJAPAN].my  = 0.28f;

    /* Oceania */
    territories[T_WAUSTRALIA].mx  = 0.78f;  territories[T_WAUSTRALIA].my  = 0.78f;
    territories[T_EAUSTRALIA].mx  = 0.82f;  territories[T_EAUSTRALIA].my  = 0.78f;
    territories[T_NEWZEALAND].mx  = 0.88f;  territories[T_NEWZEALAND].my  = 0.82f;
    territories[T_INDOPAPUA].mx   = 0.74f;  territories[T_INDOPAPUA].my   = 0.58f;
}

/*******************************************************************************

Territory marker screen position and hit testing

*******************************************************************************/

/* get marker screen position */
void terr_marker_pos(int ti, int *px, int *py)
{
    *px = map_x + (int)(territories[ti].mx * map_w);
    *py = map_y + (int)(territories[ti].my * map_h);
}

/* marker hit radius in pixels */
int marker_radius(void)
{
    return map_w / 60;
}

/* check if point is within marker radius of territory */
int point_in_terr(int px, int py, int ti)
{
    int cx, cy, dx, dy, r;

    terr_marker_pos(ti, &cx, &cy);
    r = marker_radius();
    dx = px - cx;
    dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

int find_terr_at(int px, int py)
{
    int i, cx, cy, dx, dy, r;
    int best = -1;
    int best_dist = INT_MAX;

    r = marker_radius();
    for (i = 0; i < NUM_TERR; i++) {
        terr_marker_pos(i, &cx, &cy);
        dx = px - cx;
        dy = py - cy;
        if (dx * dx + dy * dy <= r * r) {
            if (dx * dx + dy * dy < best_dist) {
                best_dist = dx * dx + dy * dy;
                best = i;
            }
        }
    }
    return best;
}

/*******************************************************************************

Adjacency check

*******************************************************************************/

int are_adjacent(int a, int b)
{
    int i;

    for (i = 0; i < territories[a].nadj; i++) {
        if (territories[a].adj[i] == b) return TRUE;
    }
    return FALSE;
}

/*******************************************************************************

Connected territory check (for fortify - BFS)

*******************************************************************************/

int are_connected(int a, int b, int owner)
{
    int visited[NUM_TERR];
    int queue[NUM_TERR];
    int head, tail, cur, i, nb;

    if (a == b) return TRUE;
    memset(visited, 0, sizeof(visited));
    head = 0; tail = 0;
    queue[tail++] = a;
    visited[a] = TRUE;

    while (head < tail) {
        cur = queue[head++];
        for (i = 0; i < territories[cur].nadj; i++) {
            nb = territories[cur].adj[i];
            if (nb < 0) break;
            if (!visited[nb] && territories[nb].owner == owner) {
                if (nb == b) return TRUE;
                visited[nb] = TRUE;
                queue[tail++] = nb;
            }
        }
    }
    return FALSE;
}

/*******************************************************************************

Player territory/army counting

*******************************************************************************/

int count_territories(int p)
{
    int i, n = 0;

    for (i = 0; i < NUM_TERR; i++) {
        if (territories[i].owner == p) n++;
    }
    return n;
}

int count_total_armies(int p)
{
    int i, n = 0;

    for (i = 0; i < NUM_TERR; i++) {
        if (territories[i].owner == p) n += territories[i].armies;
    }
    return n;
}

int owns_continent(int p, int c)
{
    int i;

    for (i = continents[c].start;
         i < continents[c].start + continents[c].count; i++) {
        if (territories[i].owner != p) return FALSE;
    }
    return TRUE;
}

int calc_reinforcements(int p)
{
    int base, bonus, c;

    base = count_territories(p) / 3;
    if (base < 3) base = 3;

    bonus = 0;
    for (c = 0; c < NUM_CONTINENTS; c++) {
        if (owns_continent(p, c)) bonus += continents[c].bonus;
    }

    return base + bonus;
}

/*******************************************************************************

Card trading

*******************************************************************************/

int card_trade_value(void)
{
    /* increasing value: 4, 6, 8, 10, 12, 15, 20, 25, ... */
    if (card_sets_traded < 5) return 4 + card_sets_traded * 2;
    return 5 + card_sets_traded * 5;
}

/*******************************************************************************

Dice combat

*******************************************************************************/

int roll_die(void)
{
    return (rand() % 6) + 1;
}

void sort_dice_desc(int *dice, int n)
{
    int i, j, tmp;

    for (i = 0; i < n - 1; i++) {
        for (j = i + 1; j < n; j++) {
            if (dice[j] > dice[i]) {
                tmp = dice[i]; dice[i] = dice[j]; dice[j] = tmp;
            }
        }
    }
}

void resolve_combat(int att_t, int def_t)
{
    int att_army, def_army, compare, i;

    att_army = territories[att_t].armies - 1;
    def_army = territories[def_t].armies;

    /* determine dice count */
    num_att_dice = att_army;
    if (num_att_dice > MAX_ATT_DICE) num_att_dice = MAX_ATT_DICE;
    num_def_dice = def_army;
    if (num_def_dice > MAX_DEF_DICE) num_def_dice = MAX_DEF_DICE;

    /* roll */
    for (i = 0; i < num_att_dice; i++) att_dice[i] = roll_die();
    for (i = 0; i < num_def_dice; i++) def_dice[i] = roll_die();

    sort_dice_desc(att_dice, num_att_dice);
    sort_dice_desc(def_dice, num_def_dice);

    /* compare */
    compare = num_att_dice < num_def_dice ? num_att_dice : num_def_dice;
    att_wins = 0;
    def_wins = 0;
    for (i = 0; i < compare; i++) {
        if (att_dice[i] > def_dice[i]) att_wins++;
        else def_wins++;
    }

    territories[att_t].armies -= def_wins;
    territories[def_t].armies -= att_wins;
}

/*******************************************************************************

Check for player elimination and game over

*******************************************************************************/

void check_eliminations(void)
{
    int i, alive_count = 0, winner = -1;

    for (i = 0; i < num_players; i++) {
        if (!players[i].active) continue;
        players[i].territories = count_territories(i);
        if (players[i].territories == 0 && !players[i].eliminated) {
            players[i].eliminated = TRUE;
        }
        if (!players[i].eliminated) {
            alive_count++;
            winner = i;
        }
    }

    if (alive_count == 1 && game_state == STATE_PLAYING) {
        game_state = STATE_GAMEOVER;
        sprintf(msg_text, "Player %d wins!", winner + 1);
        show_msg = TRUE;
    }
}

/*******************************************************************************

Initialize new game - random territory distribution

*******************************************************************************/

void init_game(void)
{
    int i, order[NUM_TERR], tmp, j, p;
    int init_armies;

    game_state = STATE_PLAYING;
    card_sets_traded = 0;
    show_dice = FALSE;
    show_msg = FALSE;
    sel_source = -1;
    sel_target = -1;
    ai_pending = FALSE;

    /* reset players */
    for (i = 0; i < MAX_PLAYERS; i++) {
        players[i].active = (i < num_players);
        players[i].territories = 0;
        players[i].cards = 0;
        players[i].eliminated = FALSE;
    }

    /* create random order of territories */
    for (i = 0; i < NUM_TERR; i++) order[i] = i;
    for (i = NUM_TERR - 1; i > 0; i--) {
        j = rand() % (i + 1);
        tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }

    /* distribute territories round-robin */
    for (i = 0; i < NUM_TERR; i++) {
        p = i % num_players;
        territories[order[i]].owner = p;
        territories[order[i]].armies = 1;
    }

    /* distribute remaining initial armies (30 per player for 3p, scaled) */
    switch (num_players) {
        case 2: init_armies = 40; break;
        case 3: init_armies = 35; break;
        case 4: init_armies = 30; break;
        default: init_armies = 25; break;
    }

    for (p = 0; p < num_players; p++) {
        int placed = count_territories(p);
        int remaining = init_armies - placed;
        for (i = 0; i < remaining; i++) {
            /* place on random owned territory */
            int choices[NUM_TERR], nc = 0;
            for (j = 0; j < NUM_TERR; j++) {
                if (territories[j].owner == p)
                    choices[nc++] = j;
            }
            if (nc > 0) {
                territories[choices[rand() % nc]].armies++;
            }
        }
        players[p].territories = count_territories(p);
    }

    cur_player = 0;
    phase = PHASE_DEPLOY;
    conquered_this_turn = FALSE;

    /* calculate initial reinforcements - store in msg */
    sprintf(msg_text, "Player %d: Deploy %d armies",
            cur_player + 1, calc_reinforcements(cur_player));
    show_msg = TRUE;
}

/*******************************************************************************

Reinforcements tracking for deploy phase

*******************************************************************************/

int deploy_remaining;

void start_deploy_phase(void)
{
    int bonus;

    phase = PHASE_DEPLOY;
    deploy_remaining = calc_reinforcements(cur_player);

    /* auto-trade cards if player has 5+ */
    while (players[cur_player].cards >= 3 && deploy_remaining < 50) {
        bonus = card_trade_value();
        players[cur_player].cards -= 3;
        deploy_remaining += bonus;
        card_sets_traded++;
    }

    sprintf(msg_text, "Player %d: Deploy %d armies",
            cur_player + 1, deploy_remaining);
    show_msg = TRUE;
}

/*******************************************************************************

Advance to next player

*******************************************************************************/

void next_player(void)
{
    int start;

    /* award card if conquered territory this turn */
    if (conquered_this_turn) {
        players[cur_player].cards++;
    }

    start = cur_player;
    do {
        cur_player = (cur_player + 1) % num_players;
    } while (players[cur_player].eliminated && cur_player != start);

    conquered_this_turn = FALSE;
    sel_source = -1;
    sel_target = -1;

    if (game_state == STATE_PLAYING) {
        start_deploy_phase();
    }
}

/*******************************************************************************

Advance phase

*******************************************************************************/

void advance_phase(void)
{
    switch (phase) {
        case PHASE_DEPLOY:
            if (deploy_remaining > 0) return;  /* must place all armies */
            phase = PHASE_ATTACK;
            sel_source = -1;
            sel_target = -1;
            sprintf(msg_text, "Player %d: Attack or Done",
                    cur_player + 1);
            show_msg = TRUE;
            break;
        case PHASE_ATTACK:
            phase = PHASE_FORTIFY;
            sel_source = -1;
            sel_target = -1;
            sprintf(msg_text, "Player %d: Fortify or Done",
                    cur_player + 1);
            show_msg = TRUE;
            break;
        case PHASE_FORTIFY:
            next_player();
            break;
    }
}

/*******************************************************************************

Handle territory click during deploy phase

*******************************************************************************/

void handle_deploy_click(int ti)
{
    if (ti < 0 || territories[ti].owner != cur_player) return;
    if (deploy_remaining <= 0) return;

    territories[ti].armies++;
    deploy_remaining--;

    ami_noteon(AMI_SYNTH_OUT, 0, 1, DEPLOY_NOTE, INT_MAX);
    ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout() + DEPLOY_DUR,
                1, DEPLOY_NOTE, INT_MAX);

    if (deploy_remaining <= 0) {
        sprintf(msg_text, "Player %d: Deploy done. Press Done to attack.",
                cur_player + 1);
        show_msg = TRUE;
    } else {
        sprintf(msg_text, "Player %d: Deploy %d remaining",
                cur_player + 1, deploy_remaining);
        show_msg = TRUE;
    }
}

/*******************************************************************************

Handle territory click during attack phase

*******************************************************************************/

void handle_attack_click(int ti)
{
    int old_owner;

    if (ti < 0) {
        sel_source = -1;
        sel_target = -1;
        return;
    }

    /* clicking own territory: select as source */
    if (territories[ti].owner == cur_player) {
        if (territories[ti].armies > 1) {
            sel_source = ti;
            sel_target = -1;
        }
        return;
    }

    /* clicking enemy territory: attack if source selected and adjacent */
    if (sel_source >= 0 && are_adjacent(sel_source, ti)) {
        sel_target = ti;

        /* play dice sound */
        ami_noteon(AMI_SYNTH_OUT, 0, 1, DICE_NOTE, INT_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout() + DICE_DUR,
                    1, DICE_NOTE, INT_MAX);

        /* resolve combat */
        old_owner = territories[ti].owner;
        resolve_combat(sel_source, ti);
        show_dice = TRUE;

        /* check if territory conquered */
        if (territories[ti].armies <= 0) {
            /* conquer! */
            territories[ti].owner = cur_player;
            /* move armies in */
            territories[ti].armies = territories[sel_source].armies - 1;
            if (territories[ti].armies < 1) territories[ti].armies = 1;
            territories[sel_source].armies = 1;
            conquered_this_turn = TRUE;

            ami_noteon(AMI_SYNTH_OUT, 0, 1, CONQUER_NOTE, INT_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout() + CONQUER_DUR,
                        1, CONQUER_NOTE, INT_MAX);

            sprintf(msg_text, "Player %d conquered %s!",
                    cur_player + 1, territories[ti].name);
            show_msg = TRUE;

            check_eliminations();
        } else {
            sprintf(msg_text, "Battle: lost %d, killed %d",
                    def_wins, att_wins);
            show_msg = TRUE;
        }

        /* deselect if source can't attack anymore */
        if (territories[sel_source].armies <= 1) {
            sel_source = -1;
        }
        sel_target = -1;
    }
}

/*******************************************************************************

Handle territory click during fortify phase

*******************************************************************************/

void handle_fortify_click(int ti)
{
    if (ti < 0) {
        sel_source = -1;
        return;
    }

    if (territories[ti].owner != cur_player) return;

    /* select source */
    if (sel_source < 0) {
        if (territories[ti].armies > 1) {
            sel_source = ti;
        }
        return;
    }

    /* select target - must be connected through own territories */
    if (ti != sel_source && are_connected(sel_source, ti, cur_player)) {
        /* move one army per click */
        if (territories[sel_source].armies > 1) {
            territories[sel_source].armies--;
            territories[ti].armies++;
            sprintf(msg_text, "Fortified %s (+1)", territories[ti].name);
            show_msg = TRUE;
            if (territories[sel_source].armies <= 1) {
                sel_source = -1;
            }
        }
    } else {
        /* clicked different own territory, reselect */
        if (territories[ti].armies > 1) {
            sel_source = ti;
        } else {
            sel_source = -1;
        }
    }
}

/*******************************************************************************

Handle territory click (dispatch by phase)

*******************************************************************************/

void handle_click(int px, int py)
{
    int ti;

    if (game_state != STATE_PLAYING) return;
    if (!players[cur_player].human) return;

    /* check Done button */
    if (px >= done_bx && px <= done_bx + done_bw &&
        py >= done_by && py <= done_by + done_bh) {
        advance_phase();
        return;
    }

    ti = find_terr_at(px, py);

    switch (phase) {
        case PHASE_DEPLOY:  handle_deploy_click(ti); break;
        case PHASE_ATTACK:  handle_attack_click(ti); break;
        case PHASE_FORTIFY: handle_fortify_click(ti); break;
    }
}

/*******************************************************************************

AI: Simple greedy computer player

*******************************************************************************/

/* find border territories (own territory adjacent to enemy) */
int ai_find_border(int p, int *borders, int max)
{
    int i, j, nb, n = 0;

    for (i = 0; i < NUM_TERR && n < max; i++) {
        if (territories[i].owner != p) continue;
        for (j = 0; j < territories[i].nadj; j++) {
            nb = territories[i].adj[j];
            if (nb < 0) break;
            if (territories[nb].owner != p) {
                borders[n++] = i;
                break;
            }
        }
    }
    return n;
}

void ai_deploy(int p)
{
    int borders[NUM_TERR], nb;
    int i, target;

    nb = ai_find_border(p, borders, NUM_TERR);
    if (nb == 0) {
        /* no borders? just pick first owned */
        for (i = 0; i < NUM_TERR; i++) {
            if (territories[i].owner == p) { nb = 1; borders[0] = i; break; }
        }
    }

    while (deploy_remaining > 0) {
        /* deploy to random border territory, with bias toward weaker ones */
        target = borders[rand() % nb];
        territories[target].armies++;
        deploy_remaining--;
    }
}

void ai_attack(int p)
{
    int attacked, attempts;
    int i, j, nb;
    int best_src, best_tgt, best_ratio;
    int ratio;

    attempts = 0;
    do {
        attacked = FALSE;
        best_src = -1;
        best_tgt = -1;
        best_ratio = 0;

        for (i = 0; i < NUM_TERR; i++) {
            if (territories[i].owner != p) continue;
            if (territories[i].armies <= 1) continue;

            for (j = 0; j < territories[i].nadj; j++) {
                nb = territories[i].adj[j];
                if (nb < 0) break;
                if (territories[nb].owner == p) continue;

                ratio = territories[i].armies * 100 /
                        (territories[nb].armies + 1);
                /* add randomness */
                ratio += rand() % 40 - 20;

                if (ratio > best_ratio && territories[i].armies > 1) {
                    best_ratio = ratio;
                    best_src = i;
                    best_tgt = nb;
                }
            }
        }

        /* only attack if we have good odds (at least 1.5:1 ratio) */
        if (best_src >= 0 && best_ratio >= 140) {
            sel_source = best_src;
            resolve_combat(best_src, best_tgt);

            if (territories[best_tgt].armies <= 0) {
                territories[best_tgt].owner = p;
                territories[best_tgt].armies =
                    territories[best_src].armies - 1;
                if (territories[best_tgt].armies < 1)
                    territories[best_tgt].armies = 1;
                territories[best_src].armies = 1;
                conquered_this_turn = TRUE;
                check_eliminations();
                if (game_state == STATE_GAMEOVER) return;
            }
            attacked = TRUE;
        }
        attempts++;
    } while (attacked && attempts < 20);

    sel_source = -1;
}

void ai_fortify(int p)
{
    int borders[NUM_TERR], nb;
    int i, j, adj_idx, interior;

    nb = ai_find_border(p, borders, NUM_TERR);
    if (nb == 0) return;

    /* find interior territories with armies and move to border */
    for (i = 0; i < NUM_TERR; i++) {
        if (territories[i].owner != p) continue;
        if (territories[i].armies <= 1) continue;

        /* check if this is an interior territory */
        interior = TRUE;
        for (j = 0; j < territories[i].nadj; j++) {
            adj_idx = territories[i].adj[j];
            if (adj_idx < 0) break;
            if (territories[adj_idx].owner != p) {
                interior = FALSE;
                break;
            }
        }

        if (interior) {
            /* move armies to a random connected border territory */
            int target = borders[rand() % nb];
            if (are_connected(i, target, p)) {
                int move = territories[i].armies - 1;
                territories[i].armies = 1;
                territories[target].armies += move;
                return;  /* only one fortify move */
            }
        }
    }
}

void ai_do_turn(int p)
{
    start_deploy_phase();
    ai_deploy(p);
    ai_attack(p);
    if (game_state == STATE_GAMEOVER) return;
    ai_fortify(p);
    /* conquered_this_turn flag is checked by next_player() for card award */
}

/*******************************************************************************

Drawing functions

*******************************************************************************/

void set_player_color(int p)
{
    if (p < 0 || p >= MAX_PLAYERS)
        ami_fcolorg(stdout, CLR(150), CLR(150), CLR(150));  /* gray */
    else
        ami_fcolorg(stdout, CLR(player_colors[p][0]),
                    CLR(player_colors[p][1]),
                    CLR(player_colors[p][2]));
}

void clear_screen(void)
{
    ami_fcolorg(stdout, CLR(20), CLR(30), CLR(50));
    ami_frect(stdout, 1, 1, scr_w, scr_h);
}

#define PIC_MAP 1

void load_map(void)
{
    ami_loadpict(stdout, PIC_MAP, "graph_games/conquest_map.bmp");
}

void draw_ocean(void)
{
    /* draw world map background image scaled to map area */
    ami_picture(stdout, PIC_MAP, map_x, map_y, map_x + map_w, map_y + map_h);
}

void draw_territory(int ti)
{
    int cx, cy, r;
    char buf[8];
    int fsz, sw;
    int is_valid_target;

    terr_marker_pos(ti, &cx, &cy);
    r = marker_radius();

    /* check if this territory is a valid attack/fortify target */
    is_valid_target = FALSE;
    if (sel_source >= 0 && ti != sel_source) {
        if (phase == PHASE_ATTACK &&
            territories[ti].owner != cur_player &&
            are_adjacent(sel_source, ti)) {
            is_valid_target = TRUE;
        } else if (phase == PHASE_FORTIFY &&
                   territories[ti].owner == cur_player &&
                   are_connected(sel_source, ti, cur_player)) {
            is_valid_target = TRUE;
        }
    }

    /* filled circle in player color */
    set_player_color(territories[ti].owner);
    ami_fellipse(stdout, cx - r, cy - r, cx + r, cy + r);

    /* outline */
    if (ti == sel_source) {
        /* selected source: thick white outline */
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
        ami_linewidth(stdout, 3);
        ami_ellipse(stdout, cx - r, cy - r, cx + r, cy + r);
        ami_linewidth(stdout, 1);
    } else if (is_valid_target) {
        /* valid target: yellow outline */
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(0));
        ami_linewidth(stdout, 2);
        ami_ellipse(stdout, cx - r, cy - r, cx + r, cy + r);
        ami_linewidth(stdout, 1);
    } else {
        /* default: thin black outline */
        ami_fcolorg(stdout, CLR(0), CLR(0), CLR(0));
        ami_ellipse(stdout, cx - r, cy - r, cx + r, cy + r);
    }

    /* army count centered in circle */
    fsz = r;
    if (fsz < 6) fsz = 6;
    if (fsz > 20) fsz = 20;
    ami_fontsiz(stdout, fsz);

    /* black text for light player colors (yellow), white for dark */
    if (territories[ti].owner == 3)
        ami_fcolorg(stdout, CLR(0), CLR(0), CLR(0));
    else
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));

    sprintf(buf, "%d", territories[ti].armies);
    sw = ami_strsiz(stdout, buf);
    ami_cursorg(stdout, cx - sw / 2, cy + fsz / 3);
    printf("%s", buf);
}

void draw_adjacency_lines(void)
{
    int i, j, nb;
    int cxa, cya, cxb, cyb;

    ami_fcolorg(stdout, CLR(80), CLR(80), CLR(100));
    ami_linewidth(stdout, 1);

    for (i = 0; i < NUM_TERR; i++) {
        terr_marker_pos(i, &cxa, &cya);

        for (j = 0; j < territories[i].nadj; j++) {
            nb = territories[i].adj[j];
            if (nb < 0) break;
            if (nb <= i) continue;  /* draw each line only once */

            /* skip Bering Strait line (wraps around) */
            if ((i == T_ALASKA && nb == T_RUSSIAE) ||
                (i == T_RUSSIAE && nb == T_ALASKA)) continue;
            /* skip cross-Atlantic (Brazil N <-> N Africa) */
            if ((i == T_BRAZILN && nb == T_NAFRICA) ||
                (i == T_NAFRICA && nb == T_BRAZILN)) continue;

            terr_marker_pos(nb, &cxb, &cyb);

            ami_line(stdout, cxa, cya, cxb, cyb);
        }
    }
}

void draw_status_bar(void)
{
    int bar_y, fsz, sw;
    char status[256];
    char phase_str[32];
    int i, bx;

    bar_y = scr_h - status_h;

    /* background */
    ami_fcolorg(stdout, CLR(30), CLR(30), CLR(40));
    ami_frect(stdout, 1, bar_y, scr_w, scr_h);

    /* separator line */
    ami_fcolorg(stdout, CLR(100), CLR(100), CLR(120));
    ami_line(stdout, 1, bar_y, scr_w, bar_y);

    if (game_state == STATE_SETUP) {
        fsz = status_h / 3;
        if (fsz < 8) fsz = 8;
        ami_fontsiz(stdout, fsz);
        ami_fcolorg(stdout, CLR(200), CLR(200), CLR(200));
        ami_cursorg(stdout, scr_w / 20, bar_y + status_h / 2 + fsz / 4);
        printf("Select Game -> New Game to start");
        return;
    }

    if (game_state == STATE_GAMEOVER) {
        fsz = status_h / 3;
        if (fsz < 8) fsz = 8;
        ami_fontsiz(stdout, fsz);
        set_player_color(cur_player);
        ami_cursorg(stdout, scr_w / 20, bar_y + status_h / 2 + fsz / 4);
        printf("%s", msg_text);
        return;
    }

    /* phase name */
    switch (phase) {
        case PHASE_DEPLOY:  strcpy(phase_str, "DEPLOY"); break;
        case PHASE_ATTACK:  strcpy(phase_str, "ATTACK"); break;
        case PHASE_FORTIFY: strcpy(phase_str, "FORTIFY"); break;
        default: strcpy(phase_str, ""); break;
    }

    /* player indicator blocks */
    fsz = status_h / 4;
    if (fsz < 6) fsz = 6;
    ami_fontsiz(stdout, fsz);
    bx = scr_w / 60;
    for (i = 0; i < num_players; i++) {
        if (players[i].eliminated) continue;
        set_player_color(i);
        ami_frect(stdout, bx, bar_y + status_h / 6,
                  bx + status_h / 3, bar_y + status_h / 2);
        if (i == cur_player) {
            ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
            ami_rect(stdout, bx, bar_y + status_h / 6,
                     bx + status_h / 3, bar_y + status_h / 2);
        }
        ami_fcolorg(stdout, CLR(200), CLR(200), CLR(200));
        ami_cursorg(stdout, bx + status_h / 3 + 4,
                    bar_y + status_h / 2 - 2);
        printf("%d", count_territories(i));
        bx += scr_w / 12;
    }

    /* current info */
    fsz = status_h / 3;
    if (fsz < 8) fsz = 8;
    ami_fontsiz(stdout, fsz);

    if (show_msg) {
        sprintf(status, "P%d [%s] %s",
                cur_player + 1, phase_str, msg_text);
    } else {
        sprintf(status, "P%d [%s]", cur_player + 1, phase_str);
    }

    set_player_color(cur_player);
    sw = ami_strsiz(stdout, status);
    ami_cursorg(stdout, scr_w / 2 - sw / 2,
                bar_y + status_h * 4 / 5);
    printf("%s", status);

    /* Draw "Done" button */
    if (game_state == STATE_PLAYING && players[cur_player].human) {
        int can_done = (phase != PHASE_DEPLOY || deploy_remaining <= 0);
        if (can_done) {
            ami_fcolorg(stdout, CLR(60), CLR(120), CLR(60));
        } else {
            ami_fcolorg(stdout, CLR(60), CLR(60), CLR(60));
        }
        ami_frect(stdout, done_bx, done_by,
                  done_bx + done_bw, done_by + done_bh);
        ami_fcolorg(stdout, CLR(200), CLR(200), CLR(200));
        ami_rect(stdout, done_bx, done_by,
                 done_bx + done_bw, done_by + done_bh);

        fsz = done_bh / 2;
        if (fsz < 6) fsz = 6;
        ami_fontsiz(stdout, fsz);
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
        sw = ami_strsiz(stdout, "Done");
        ami_cursorg(stdout, done_bx + done_bw / 2 - sw / 2,
                    done_by + done_bh / 2 + fsz / 4);
        printf("Done");
    }
}

void draw_dice_display(void)
{
    int cx, cy, bw, bh, dsz, i, dx;
    int fsz, sw;
    char buf[16];

    if (!show_dice) return;

    cx = scr_w / 2;
    cy = scr_h / 2 - status_h;
    bw = scr_w / 5;
    bh = scr_h / 8;
    dsz = bh / 2;

    /* background panel */
    ami_fcolorg(stdout, CLR(0), CLR(0), CLR(0));
    ami_frect(stdout, cx - bw, cy - bh, cx + bw, cy + bh);
    ami_fcolorg(stdout, CLR(180), CLR(180), CLR(180));
    ami_rect(stdout, cx - bw, cy - bh, cx + bw, cy + bh);

    fsz = dsz * 2 / 3;
    if (fsz < 8) fsz = 8;
    ami_fontsiz(stdout, fsz);

    /* attacker dice (red) */
    ami_fcolorg(stdout, CLR(200), CLR(50), CLR(50));
    dx = cx - bw + dsz;
    for (i = 0; i < num_att_dice; i++) {
        ami_frect(stdout, dx, cy - dsz / 2, dx + dsz, cy + dsz / 2);
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
        sprintf(buf, "%d", att_dice[i]);
        sw = ami_strsiz(stdout, buf);
        ami_cursorg(stdout, dx + dsz / 2 - sw / 2, cy + fsz / 4);
        printf("%s", buf);
        ami_fcolorg(stdout, CLR(200), CLR(50), CLR(50));
        dx += dsz + dsz / 3;
    }

    /* defender dice (blue) */
    ami_fcolorg(stdout, CLR(50), CLR(100), CLR(200));
    dx = cx + dsz / 2;
    for (i = 0; i < num_def_dice; i++) {
        ami_frect(stdout, dx, cy - dsz / 2, dx + dsz, cy + dsz / 2);
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
        sprintf(buf, "%d", def_dice[i]);
        sw = ami_strsiz(stdout, buf);
        ami_cursorg(stdout, dx + dsz / 2 - sw / 2, cy + fsz / 4);
        printf("%s", buf);
        ami_fcolorg(stdout, CLR(50), CLR(100), CLR(200));
        dx += dsz + dsz / 3;
    }

    /* "vs" text */
    ami_fcolorg(stdout, CLR(200), CLR(200), CLR(200));
    sw = ami_strsiz(stdout, "vs");
    ami_cursorg(stdout, cx - sw / 2, cy - bh + fsz + 4);
    printf("vs");

    /* result text */
    sprintf(buf, "-%d / -%d", def_wins, att_wins);
    sw = ami_strsiz(stdout, buf);
    ami_cursorg(stdout, cx - sw / 2, cy + bh - 4);
    printf("%s", buf);
}

void draw_continent_labels(void)
{
    int c, fsz, sw;
    int cx, cy;
    /* approximate center positions for continent labels (normalized coords) */
    float clabel_x[NUM_CONTINENTS] = { 0.15, 0.24, 0.40, 0.46, 0.50, 0.65, 0.82 };
    float clabel_y[NUM_CONTINENTS] = { 0.08, 0.85, 0.08, 0.75, 0.38, 0.05, 0.90 };

    fsz = map_h / 50;
    if (fsz < 4) fsz = 4;
    ami_fontsiz(stdout, fsz);
    ami_fcolorg(stdout, CLR(200), CLR(200), CLR(130));

    for (c = 0; c < NUM_CONTINENTS; c++) {
        cx = map_x + (int)(clabel_x[c] * map_w);
        cy = map_y + (int)(clabel_y[c] * map_h);
        sw = ami_strsiz(stdout, continents[c].name);
        ami_cursorg(stdout, cx - sw / 2, cy);
        printf("%s(+%d)", continents[c].name, continents[c].bonus);
    }
}

/* draw a stick figure (human) icon centered at cx, cy, size s */
void draw_human_icon(int cx, int cy, int s)
{
    int hr = s / 5; /* head radius */

    /* head */
    ami_fellipse(stdout, cx - hr, cy - s/2, cx + hr, cy - s/2 + 2*hr);
    /* body */
    ami_line(stdout, cx, cy - s/2 + 2*hr, cx, cy + s/6);
    /* arms */
    ami_line(stdout, cx - s/3, cy - s/6, cx + s/3, cy - s/6);
    /* legs */
    ami_line(stdout, cx, cy + s/6, cx - s/4, cy + s/2);
    ami_line(stdout, cx, cy + s/6, cx + s/4, cy + s/2);
}

/* draw a robot icon centered at cx, cy, size s, player color p */
void draw_robot_icon(int cx, int cy, int s, int p)
{
    int bw = s / 2;  /* body width */
    int bh = s / 3;  /* body height */
    int hw = s / 3;  /* head width */
    int hh = s / 4;  /* head height */

    /* antenna */
    ami_line(stdout, cx, cy - s/2, cx, cy - s/2 + s/8);
    ami_fellipse(stdout, cx - s/12, cy - s/2 - s/12, cx + s/12, cy - s/2 + s/12);
    /* head */
    ami_frect(stdout, cx - hw/2, cy - s/2 + s/8, cx + hw/2, cy - s/2 + s/8 + hh);
    /* eyes */
    ami_fcolorg(stdout, CLR(255), CLR(50), CLR(50));
    ami_fellipse(stdout, cx - hw/4 - s/16, cy - s/2 + s/8 + hh/4,
                         cx - hw/4 + s/16, cy - s/2 + s/8 + hh*3/4);
    ami_fellipse(stdout, cx + hw/4 - s/16, cy - s/2 + s/8 + hh/4,
                         cx + hw/4 + s/16, cy - s/2 + s/8 + hh*3/4);
    /* body */
    set_player_color(p);
    ami_frect(stdout, cx - bw/2, cy - s/8, cx + bw/2, cy - s/8 + bh);
    /* legs */
    ami_frect(stdout, cx - bw/3, cy - s/8 + bh, cx - bw/3 + s/8, cy + s/2);
    ami_frect(stdout, cx + bw/3 - s/8, cy - s/8 + bh, cx + bw/3, cy + s/2);
}

/* draw player setup icons showing human/computer status */
void draw_player_icons(void)
{
    int i, cx, cy, icon_sz, gap, total_w, start_x;
    int fsz, sw;
    char label[32];
    int base_y;

    icon_sz = scr_h / 8;
    if (icon_sz < 30) icon_sz = 30;
    gap = scr_w / (MAX_PLAYERS + 1);
    total_w = num_players * gap;
    start_x = (scr_w - total_w) / 2 + gap / 2;

    /* position below the subtitle */
    base_y = scr_h / 2 + scr_h / 12;

    fsz = scr_h / 40;
    if (fsz < 8) fsz = 8;
    ami_fontsiz(stdout, fsz);

    for (i = 0; i < num_players; i++) {
        cx = start_x + i * gap;
        cy = base_y;

        /* draw player color background circle */
        set_player_color(i);
        ami_linewidth(stdout, 2);

        if (players[i].human) {
            draw_human_icon(cx, cy, icon_sz);
        } else {
            draw_robot_icon(cx, cy, icon_sz, i);
        }
        ami_linewidth(stdout, 1);

        /* label */
        sprintf(label, "P%d %s", i + 1,
                players[i].human ? "Human" : "CPU");
        set_player_color(i);
        sw = ami_strsiz(stdout, label);
        ami_cursorg(stdout, cx - sw / 2, cy + icon_sz / 2 + fsz);
        printf("%s", label);
    }
}

void draw_title_screen(void)
{
    int fsz, sw;

    fsz = scr_h / 12;
    if (fsz < 16) fsz = 16;
    ami_fontsiz(stdout, fsz);
    ami_fcolorg(stdout, CLR(220), CLR(180), CLR(50));
    sw = ami_strsiz(stdout, "CONQUEST");
    ami_cursorg(stdout, scr_w / 2 - sw / 2, scr_h / 4);
    printf("CONQUEST");

    fsz = scr_h / 30;
    if (fsz < 8) fsz = 8;
    ami_fontsiz(stdout, fsz);
    ami_fcolorg(stdout, CLR(180), CLR(180), CLR(180));
    sw = ami_strsiz(stdout, "A game of world domination");
    ami_cursorg(stdout, scr_w / 2 - sw / 2, scr_h / 4 + scr_h / 10);
    printf("A game of world domination");

    /* draw player setup icons */
    draw_player_icons();

    fsz = scr_h / 35;
    if (fsz < 8) fsz = 8;
    ami_fontsiz(stdout, fsz);
    ami_fcolorg(stdout, CLR(140), CLR(140), CLR(140));
    sw = ami_strsiz(stdout, "Use Players menu to configure, then Game -> New Game");
    ami_cursorg(stdout, scr_w / 2 - sw / 2, scr_h * 3 / 4);
    printf("Use Players menu to configure, then Game -> New Game");
}

void draw_all(void)
{
    int i;

    calc_metrics();
    clear_screen();

    if (game_state == STATE_SETUP) {
        draw_title_screen();
        draw_status_bar();
        return;
    }

    draw_ocean();
    draw_adjacency_lines();
    draw_continent_labels();

    for (i = 0; i < NUM_TERR; i++) {
        draw_territory(i);
    }

    draw_dice_display();
    draw_status_bar();
}

/*******************************************************************************

Main program

*******************************************************************************/

int main(void)
{
    ami_evtrec er;

    srand(time(NULL));

    ami_title(stdout, "Conquest");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    /* stay in buffered mode for ami_select double buffering */
    ami_font(stdout, AMI_FONT_SIGN);
    ami_bold(stdout, TRUE);
    ami_binvis(stdout);
    ami_bcolorg(stdout, 0, 0, 0);
    ami_frametimer(stdout, TRUE);
    flip = FALSE;

    ami_opensynthout(AMI_SYNTH_OUT);
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    ami_starttimeout();

    setup_menu();
    load_map();
    init_markers();

    /* default: 3 players, P1 human, rest computer */
    num_players = 3;
    players[0].human = TRUE;
    players[1].human = FALSE;
    players[2].human = FALSE;
    players[3].human = FALSE;
    players[4].human = FALSE;

    calc_metrics();
    mouse_x = scr_w / 2;
    mouse_y = scr_h / 2;
    draw_all();

    do {
        ami_event(stdin, &er);

        if (er.etype == ami_etresize) {
            ami_sizbufg(stdout, er.rszxg, er.rszyg);
            calc_metrics();
            draw_all();
            ami_select(stdout, !flip+1, flip+1);
            flip = !flip;
        }

        else if (er.etype == ami_etredraw) {
            draw_all();
        }

        else if (er.etype == ami_etmoumovg) {
            mouse_x = er.moupxg;
            mouse_y = er.moupyg;
            hover_terr = find_terr_at(mouse_x, mouse_y);
        }

        else if (er.etype == ami_etmouba && er.amoubn == 1) {
            handle_click(mouse_x, mouse_y);
        }

        else if (er.etype == ami_etenter) {
            /* Enter key = Done */
            if (game_state == STATE_PLAYING && players[cur_player].human) {
                advance_phase();
            }
        }

        else if (er.etype == ami_etframe) {
            /* handle AI turns during frame events - one per frame */
            if (game_state == STATE_PLAYING &&
                !players[cur_player].human &&
                !players[cur_player].eliminated) {
                ai_do_turn(cur_player);
                if (game_state != STATE_GAMEOVER) {
                    next_player();
                }
            }

            /* double buffer: select offscreen, draw, flip */
            ami_select(stdout, !flip+1, flip+1);
            draw_all();
            flip = !flip;
        }

        else if (er.etype == ami_etmenus) {
            switch (er.menuid) {
                case MENU_NEW:
                    init_game();
                    draw_all();
                    break;
                case MENU_EXIT:
                    er.etype = ami_etterm;
                    break;
                case MENU_2P: num_players = 2; break;
                case MENU_3P: num_players = 3; break;
                case MENU_4P: num_players = 4; break;
                case MENU_5P: num_players = 5; break;
                case MENU_P1H: players[0].human = TRUE; break;
                case MENU_P1C: players[0].human = FALSE; break;
                case MENU_P2H: players[1].human = TRUE; break;
                case MENU_P2C: players[1].human = FALSE; break;
                case MENU_P3H: players[2].human = TRUE; break;
                case MENU_P3C: players[2].human = FALSE; break;
                case MENU_P4H: players[3].human = TRUE; break;
                case MENU_P4C: players[3].human = FALSE; break;
                case MENU_P5H: players[4].human = TRUE; break;
                case MENU_P5C: players[4].human = FALSE; break;
                case MENU_ABOUT:
                    ami_alert("About Conquest",
                              "Conquest - A territory strategy game\n"
                              "Click territories to deploy, attack, fortify\n"
                              "Copyright (C) 2026 S. A. Franco");
                    draw_all();
                    break;
            }
        }

    } while (er.etype != ami_etterm);

    ami_closesynthout(AMI_SYNTH_OUT);

}
