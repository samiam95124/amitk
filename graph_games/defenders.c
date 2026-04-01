/*******************************************************************************
*                                                                              *
*                              DEFENDERS GAME                                  *
*                                                                              *
*                       COPYRIGHT (C) 2026 S. A. FRANCO                        *
*                                                                              *
* A Space Invaders style arcade game with original artwork. Player defends     *
* against waves of descending alien invaders using a mobile cannon.            *
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
#define TIMER_GAME   1                      /* main game tick */
#define TIMER_AFIRE  2                      /* alien fire timer */
#define TIMER_UFO    3                      /* UFO spawn timer */

/* timer intervals */
#define GAME_TICK    500                    /* ~50ms game tick */
#define AFIRE_MIN    5000                   /* min alien fire interval */
#define AFIRE_MAX    15000                  /* max alien fire interval */
#define UFO_INTERVAL 150000                 /* UFO appears ~every 15s */

/* grid dimensions */
#define ALIEN_ROWS   5
#define ALIEN_COLS   8
#define NUM_SHIELDS  4
#define SHIELD_SEGS  5                      /* segments per shield axis */

/* menu IDs */
#define MENU_NEW     100
#define MENU_EXIT    101
#define MENU_ABOUT   102

/* sound defines */
#define SHOOT_NOTE   (AMI_NOTE_C+AMI_OCTAVE_6)
#define HIT_NOTE     (AMI_NOTE_E+AMI_OCTAVE_7)
#define EXPLODE_NOTE (AMI_NOTE_C+AMI_OCTAVE_4)
#define UFO_NOTE     (AMI_NOTE_A+AMI_OCTAVE_5)
#define SHOOT_DUR    100
#define HIT_DUR      150
#define EXPLODE_DUR  500
#define UFO_DUR      200

/* color helpers: scale 0-255 to 0-INT_MAX */
#define CLR(v) ((v) * (INT_MAX / 255))

/* directions */
#define DIR_RIGHT  1
#define DIR_LEFT  -1

/* max bullets */
#define MAX_PBULLETS  3
#define MAX_ABULLETS  8

/*******************************************************************************

Types

*******************************************************************************/

typedef struct {
    int x, y;       /* center position */
    int active;
} bullet_t;

typedef struct {
    int alive;
    int row, col;
    int points;     /* score value */
} alien_t;

typedef struct {
    int active;
    int x, y;
    int dx;         /* direction: positive = right */
} ufo_t;

/* shield segment */
typedef struct {
    int intact;     /* still there? */
} shield_seg_t;

typedef struct {
    int x, y;       /* top-left of shield block */
    int w, h;       /* total size */
    shield_seg_t segs[SHIELD_SEGS][SHIELD_SEGS];
} shield_t;

/*******************************************************************************

Global state

*******************************************************************************/

/* screen metrics */
int scr_w, scr_h;
int alien_w, alien_h;      /* size of one alien cell */
int player_w, player_h;    /* player ship size */
int bullet_w, bullet_h;    /* bullet dimensions */
int ufo_w, ufo_h;          /* UFO size */

/* game objects */
alien_t aliens[ALIEN_ROWS][ALIEN_COLS];
int alien_base_x, alien_base_y;    /* top-left of alien grid */
int alien_dir;                     /* DIR_RIGHT or DIR_LEFT */
int alien_move_counter;            /* ticks until next alien move */
int alien_move_rate;               /* ticks between alien moves */
int aliens_alive;                  /* count of living aliens */

int player_x;                      /* player center x */
int player_y;                      /* player top y */
int mouse_x;                       /* current mouse x */

bullet_t pbullets[MAX_PBULLETS];   /* player bullets */
bullet_t abullets[MAX_ABULLETS];   /* alien bullets */

shield_t shields[NUM_SHIELDS];

ufo_t ufo;

int score;
int lives;
int wave;
int game_over;
int game_started;
int flip;          /* double buffer flip state */

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

    /* Help menu */
    help_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Help");
    appendmenu(&menu, help_menu);
    help_items = NULL;
    appendmenu(&help_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_ABOUT, "About Defenders"));
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
    alien_w = scr_w / 12;
    alien_h = scr_h / 18;
    player_w = scr_w / 14;
    player_h = scr_h / 22;
    bullet_w = scr_w / 200;
    if (bullet_w < 2) bullet_w = 2;
    bullet_h = scr_h / 40;
    if (bullet_h < 4) bullet_h = 4;
    ufo_w = alien_w * 3 / 2;
    ufo_h = alien_h * 2 / 3;

}

/*******************************************************************************

Initialize shields

*******************************************************************************/

void init_shields(void)
{
    int i, r, c;
    int shld_w, shld_h, gap, start_x, shld_y;

    shld_w = scr_w / 10;
    shld_h = scr_h / 16;
    gap = scr_w / (NUM_SHIELDS + 1);
    start_x = gap - shld_w / 2;
    shld_y = scr_h - scr_h / 6;

    for (i = 0; i < NUM_SHIELDS; i++) {
        shields[i].x = start_x + i * gap;
        shields[i].y = shld_y;
        shields[i].w = shld_w;
        shields[i].h = shld_h;
        for (r = 0; r < SHIELD_SEGS; r++)
            for (c = 0; c < SHIELD_SEGS; c++)
                shields[i].segs[r][c].intact = TRUE;
    }
}

/*******************************************************************************

Initialize aliens for a new wave

*******************************************************************************/

void init_aliens(void)
{
    int r, c;

    alien_base_x = scr_w / 8;
    alien_base_y = scr_h / 8;
    alien_dir = DIR_RIGHT;
    alien_move_counter = 0;
    /* speed increases with wave number */
    alien_move_rate = 12 - wave;
    if (alien_move_rate < 2) alien_move_rate = 2;
    aliens_alive = ALIEN_ROWS * ALIEN_COLS;

    for (r = 0; r < ALIEN_ROWS; r++) {
        for (c = 0; c < ALIEN_COLS; c++) {
            aliens[r][c].alive = TRUE;
            aliens[r][c].row = r;
            aliens[r][c].col = c;
            if (r == 0) aliens[r][c].points = 30;
            else if (r <= 2) aliens[r][c].points = 20;
            else aliens[r][c].points = 10;
        }
    }
}

/*******************************************************************************

Initialize a new game

*******************************************************************************/

void init_game(void)
{

    int i;

    calc_metrics();
    score = 0;
    lives = 3;
    wave = 1;
    game_over = FALSE;
    game_started = TRUE;
    player_x = scr_w / 2;
    player_y = scr_h - scr_h / 10;

    for (i = 0; i < MAX_PBULLETS; i++) pbullets[i].active = FALSE;
    for (i = 0; i < MAX_ABULLETS; i++) abullets[i].active = FALSE;

    ufo.active = FALSE;
    init_aliens();
    init_shields();

}

/*******************************************************************************

Drawing helpers

*******************************************************************************/

void set_color_black(void)
{
    ami_fcolorg(stdout, 0, 0, 0);
}

void clear_screen(void)
{
    set_color_black();
    ami_frect(stdout, 1, 1, scr_w, scr_h);
}

/*******************************************************************************

Draw player ship - trapezoid/wedge shape

*******************************************************************************/

void draw_player(void)
{
    int cx, cy, hw, hh, i, w;

    cx = player_x;
    cy = player_y;
    hw = player_w / 2;
    hh = player_h;

    /* bright green ship */
    ami_fcolorg(stdout, CLR(50), CLR(255), CLR(50));

    /* draw wedge shape: stacked rectangles getting narrower toward top */
    for (i = 0; i < hh; i++) {
        w = hw * (hh - i) / hh;
        if (w < 2) w = 2;
        ami_frect(stdout, cx - w, cy + i, cx + w, cy + i);
    }

    /* cannon turret on top */
    ami_fcolorg(stdout, CLR(100), CLR(255), CLR(100));
    ami_frect(stdout, cx - 2, cy - hh / 3, cx + 2, cy);

    /* cockpit highlight */
    ami_fcolorg(stdout, CLR(200), CLR(255), CLR(200));
    ami_frect(stdout, cx - hw / 4, cy + hh / 3, cx + hw / 4, cy + hh / 2);
}

/*******************************************************************************

Draw an alien based on its row type

*******************************************************************************/

void draw_alien_at(int cx, int cy, int row)
{
    int aw, ah, i, w;

    aw = alien_w / 2 - 2;
    ah = alien_h / 2 - 2;

    if (row == 0) {
        /* Diamond shape (row 1, top) - magenta/pink */
        ami_fcolorg(stdout, CLR(255), CLR(80), CLR(220));
        /* draw diamond using lines */
        for (i = 0; i <= ah; i++) {
            w = aw * i / ah;
            ami_frect(stdout, cx - w, cy - i, cx + w, cy - i);
            ami_frect(stdout, cx - w, cy + i, cx + w, cy + i);
        }
        /* inner eye dot */
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
        ami_fellipse(stdout, cx - aw / 4, cy - ah / 4,
                             cx + aw / 4, cy + ah / 4);
    } else if (row <= 2) {
        /* Triangle shapes (rows 2-3) - cyan */
        ami_fcolorg(stdout, CLR(0), CLR(255), CLR(255));
        /* inverted triangle: wide at top, narrow at bottom */
        for (i = 0; i < ah * 2; i++) {
            w = aw * (ah * 2 - i) / (ah * 2);
            ami_frect(stdout, cx - w, cy - ah + i, cx + w, cy - ah + i);
        }
        /* two "eye" dots */
        ami_fcolorg(stdout, CLR(255), CLR(0), CLR(0));
        ami_fellipse(stdout, cx - aw / 3 - 2, cy - ah / 3 - 2,
                             cx - aw / 3 + 2, cy - ah / 3 + 2);
        ami_fellipse(stdout, cx + aw / 3 - 2, cy - ah / 3 - 2,
                             cx + aw / 3 + 2, cy - ah / 3 + 2);
    } else {
        /* Circle shapes (rows 4-5) - yellow-green */
        ami_fcolorg(stdout, CLR(180), CLR(255), CLR(50));
        ami_fellipse(stdout, cx - aw, cy - ah, cx + aw, cy + ah);
        /* inner face: two eyes and mouth */
        ami_fcolorg(stdout, CLR(40), CLR(40), CLR(0));
        ami_fellipse(stdout, cx - aw / 3 - 2, cy - ah / 4 - 2,
                             cx - aw / 3 + 2, cy - ah / 4 + 2);
        ami_fellipse(stdout, cx + aw / 3 - 2, cy - ah / 4 - 2,
                             cx + aw / 3 + 2, cy - ah / 4 + 2);
        ami_frect(stdout, cx - aw / 3, cy + ah / 4,
                          cx + aw / 3, cy + ah / 4 + 1);
    }
}

/*******************************************************************************

Get alien center position from grid coordinates

*******************************************************************************/

void alien_pos(int row, int col, int *cx, int *cy)
{

    *cx = alien_base_x + col * (alien_w + alien_w / 4) + alien_w / 2;
    *cy = alien_base_y + row * (alien_h + alien_h / 4) + alien_h / 2;

}

/*******************************************************************************

Draw all aliens

*******************************************************************************/

void draw_aliens(void)
{
    int r, c, cx, cy;

    for (r = 0; r < ALIEN_ROWS; r++) {
        for (c = 0; c < ALIEN_COLS; c++) {
            if (aliens[r][c].alive) {
                alien_pos(r, c, &cx, &cy);
                draw_alien_at(cx, cy, r);
            }
        }
    }
}

/*******************************************************************************

Draw shields

*******************************************************************************/

void draw_shields(void)
{
    int i, r, c;
    int sw, sh, sx, sy;

    ami_fcolorg(stdout, CLR(60), CLR(60), CLR(255));

    for (i = 0; i < NUM_SHIELDS; i++) {
        sw = shields[i].w / SHIELD_SEGS;
        sh = shields[i].h / SHIELD_SEGS;
        for (r = 0; r < SHIELD_SEGS; r++) {
            for (c = 0; c < SHIELD_SEGS; c++) {
                if (shields[i].segs[r][c].intact) {
                    sx = shields[i].x + c * sw;
                    sy = shields[i].y + r * sh;
                    ami_frect(stdout, sx, sy, sx + sw - 1, sy + sh - 1);
                }
            }
        }
    }
}

/*******************************************************************************

Draw bullets

*******************************************************************************/

void draw_bullets(void)
{
    int i;

    /* player bullets - white */
    ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
    for (i = 0; i < MAX_PBULLETS; i++) {
        if (pbullets[i].active) {
            ami_frect(stdout, pbullets[i].x - bullet_w / 2,
                              pbullets[i].y - bullet_h / 2,
                              pbullets[i].x + bullet_w / 2,
                              pbullets[i].y + bullet_h / 2);
        }
    }

    /* alien bullets - red/orange */
    ami_fcolorg(stdout, CLR(255), CLR(120), CLR(0));
    for (i = 0; i < MAX_ABULLETS; i++) {
        if (abullets[i].active) {
            ami_frect(stdout, abullets[i].x - bullet_w / 2,
                              abullets[i].y - bullet_h / 2,
                              abullets[i].x + bullet_w / 2,
                              abullets[i].y + bullet_h / 2);
        }
    }
}

/*******************************************************************************

Draw UFO

*******************************************************************************/

void draw_ufo(void)
{

    int cx, cy, hw, hh;

    if (!ufo.active) return;

    cx = ufo.x;
    cy = ufo.y;
    hw = ufo_w / 2;
    hh = ufo_h / 2;

    /* red saucer body */
    ami_fcolorg(stdout, CLR(255), CLR(30), CLR(30));
    ami_fellipse(stdout, cx - hw, cy - hh / 2, cx + hw, cy + hh / 2);

    /* dome on top */
    ami_fcolorg(stdout, CLR(255), CLR(100), CLR(100));
    ami_fellipse(stdout, cx - hw / 3, cy - hh, cx + hw / 3, cy - hh / 3);

    /* lights */
    ami_fcolorg(stdout, CLR(255), CLR(255), CLR(0));
    ami_fellipse(stdout, cx - hw / 2 - 2, cy - 2,
                         cx - hw / 2 + 2, cy + 2);
    ami_fellipse(stdout, cx - 2, cy - 2, cx + 2, cy + 2);
    ami_fellipse(stdout, cx + hw / 2 - 2, cy - 2,
                         cx + hw / 2 + 2, cy + 2);

}

/*******************************************************************************

Draw HUD - score, lives, wave

*******************************************************************************/

void draw_hud(void)
{
    int fsz, lx, i;
    char buf[80];

    fsz = scr_h / 30;
    if (fsz < 10) fsz = 10;
    ami_fontsiz(stdout, fsz);

    /* score on left */
    ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
    ami_cursorg(stdout, scr_w / 40, scr_h / 60);
    sprintf(buf, "SCORE %06d", score);
    printf("%s\n", buf);

    /* wave in center */
    ami_cursorg(stdout, scr_w / 2 - scr_w / 20, scr_h / 60);
    sprintf(buf, "WAVE %d", wave);
    printf("%s\n", buf);

    /* lives on right - draw small ship icons */
    ami_cursorg(stdout, scr_w - scr_w / 5, scr_h / 60);
    printf("LIVES\n");
    lx = scr_w - scr_w / 5 + scr_w / 12;
    ami_fcolorg(stdout, CLR(50), CLR(255), CLR(50));
    for (i = 0; i < lives; i++) {
        ami_fellipse(stdout, lx + i * (player_w / 3 + 4), scr_h / 60 + 2,
                             lx + i * (player_w / 3 + 4) + player_w / 4,
                             scr_h / 60 + fsz - 2);
    }
}

/*******************************************************************************

Draw game over overlay

*******************************************************************************/

void draw_gameover(void)
{
    int fsz;

    fsz = scr_h / 10;
    if (fsz < 20) fsz = 20;
    ami_fontsiz(stdout, fsz);
    ami_fcolorg(stdout, CLR(255), CLR(0), CLR(0));
    ami_cursorg(stdout, scr_w / 2 - scr_w / 5, scr_h / 2 - fsz / 2);
    printf("GAME OVER\n");

    fsz = scr_h / 20;
    ami_fontsiz(stdout, fsz);
    ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
    ami_cursorg(stdout, scr_w / 2 - scr_w / 5, scr_h / 2 + scr_h / 10);
    printf("Select Game > New Game to play again\n");
}

/*******************************************************************************

Draw entire screen

*******************************************************************************/

void draw_all(void)
{

    calc_metrics();
    clear_screen();
    if (!game_started) {
        int fsz;

        fsz = scr_h / 10;
        if (fsz < 20) fsz = 20;
        ami_fontsiz(stdout, fsz);
        ami_fcolorg(stdout, CLR(50), CLR(255), CLR(50));
        ami_cursorg(stdout, scr_w / 2 - scr_w / 4, scr_h / 3);
        printf("DEFENDERS\n");
        fsz = scr_h / 20;
        ami_fontsiz(stdout, fsz);
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(255));
        ami_cursorg(stdout, scr_w / 2 - scr_w / 4, scr_h / 2);
        printf("Select Game > New Game to start\n");
        return;
    }
    draw_hud();
    draw_aliens();
    draw_shields();
    draw_player();
    draw_bullets();
    draw_ufo();
    if (game_over) draw_gameover();

}

/*******************************************************************************

Fire a player bullet

*******************************************************************************/

void player_fire(void)
{
    int i;

    if (game_over) return;

    for (i = 0; i < MAX_PBULLETS; i++) {
        if (!pbullets[i].active) {
            pbullets[i].active = TRUE;
            pbullets[i].x = player_x;
            pbullets[i].y = player_y - player_h / 3;
            /* shoot sound */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, SHOOT_NOTE, INT_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout() + SHOOT_DUR,
                        1, SHOOT_NOTE, INT_MAX);
            return;
        }
    }
}

/*******************************************************************************

Alien fires a bullet downward

*******************************************************************************/

void alien_fire(void)
{
    int i, r, c, cx, cy;
    int candidates[ALIEN_COLS];
    int ncand;

    if (game_over || aliens_alive == 0) return;

    /* find bottom-most alive alien in each column */
    ncand = 0;
    for (c = 0; c < ALIEN_COLS; c++) {
        for (r = ALIEN_ROWS - 1; r >= 0; r--) {
            if (aliens[r][c].alive) {
                candidates[ncand++] = r * ALIEN_COLS + c;
                break;
            }
        }
    }

    if (ncand == 0) return;

    /* pick random bottom alien */
    i = candidates[rand() % ncand];
    r = i / ALIEN_COLS;
    c = i % ALIEN_COLS;

    /* find free bullet slot */
    for (i = 0; i < MAX_ABULLETS; i++) {
        if (!abullets[i].active) {
            alien_pos(r, c, &cx, &cy);
            abullets[i].active = TRUE;
            abullets[i].x = cx;
            abullets[i].y = cy + alien_h / 2;
            return;
        }
    }
}

/*******************************************************************************

Check if a point hits a shield segment, and destroy it if so.
Returns TRUE if hit.

*******************************************************************************/

int hit_shield(int bx, int by)
{
    int i, r, c;
    int sw, sh, sx, sy;

    for (i = 0; i < NUM_SHIELDS; i++) {
        /* quick bounds check */
        if (bx < shields[i].x || bx > shields[i].x + shields[i].w) continue;
        if (by < shields[i].y || by > shields[i].y + shields[i].h) continue;

        sw = shields[i].w / SHIELD_SEGS;
        sh = shields[i].h / SHIELD_SEGS;
        for (r = 0; r < SHIELD_SEGS; r++) {
            for (c = 0; c < SHIELD_SEGS; c++) {
                if (!shields[i].segs[r][c].intact) continue;
                sx = shields[i].x + c * sw;
                sy = shields[i].y + r * sh;
                if (bx >= sx && bx <= sx + sw &&
                    by >= sy && by <= sy + sh) {
                    shields[i].segs[r][c].intact = FALSE;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

/*******************************************************************************

Move player bullets and check collisions

*******************************************************************************/

void update_pbullets(void)
{
    int i, r, c, cx, cy;
    int speed;

    speed = scr_h / 80;
    if (speed < 3) speed = 3;

    for (i = 0; i < MAX_PBULLETS; i++) {
        if (!pbullets[i].active) continue;

        pbullets[i].y -= speed;

        /* off screen? */
        if (pbullets[i].y < 0) {
            pbullets[i].active = FALSE;
            continue;
        }

        /* hit shield? */
        if (hit_shield(pbullets[i].x, pbullets[i].y)) {
            pbullets[i].active = FALSE;
            continue;
        }

        /* hit UFO? */
        if (ufo.active) {
            if (pbullets[i].x >= ufo.x - ufo_w / 2 &&
                pbullets[i].x <= ufo.x + ufo_w / 2 &&
                pbullets[i].y >= ufo.y - ufo_h / 2 &&
                pbullets[i].y <= ufo.y + ufo_h / 2) {
                ufo.active = FALSE;
                pbullets[i].active = FALSE;
                score += 100;
                ami_noteon(AMI_SYNTH_OUT, 0, 1, UFO_NOTE, INT_MAX);
                ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout() + UFO_DUR,
                            1, UFO_NOTE, INT_MAX);
                continue;
            }
        }

        /* hit alien? */
        for (r = 0; r < ALIEN_ROWS; r++) {
            for (c = 0; c < ALIEN_COLS; c++) {
                if (!aliens[r][c].alive) continue;
                alien_pos(r, c, &cx, &cy);
                if (pbullets[i].x >= cx - alien_w / 2 &&
                    pbullets[i].x <= cx + alien_w / 2 &&
                    pbullets[i].y >= cy - alien_h / 2 &&
                    pbullets[i].y <= cy + alien_h / 2) {
                    /* hit! */
                    aliens[r][c].alive = FALSE;
                    pbullets[i].active = FALSE;
                    score += aliens[r][c].points;
                    aliens_alive--;

                    /* speed up remaining aliens */
                    if (aliens_alive > 0) {
                        alien_move_rate = 12 - wave -
                            (ALIEN_ROWS * ALIEN_COLS - aliens_alive) / 5;
                        if (alien_move_rate < 1) alien_move_rate = 1;
                    }

                    ami_noteon(AMI_SYNTH_OUT, 0, 1, HIT_NOTE, INT_MAX);
                    ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout() + HIT_DUR,
                                1, HIT_NOTE, INT_MAX);
                    goto next_pbullet;
                }
            }
        }
        next_pbullet:;
    }
}

/*******************************************************************************

Move alien bullets and check collisions

*******************************************************************************/

void update_abullets(void)
{
    int i;
    int speed;

    speed = scr_h / 120;
    if (speed < 2) speed = 2;

    for (i = 0; i < MAX_ABULLETS; i++) {
        if (!abullets[i].active) continue;

        abullets[i].y += speed;

        /* off screen? */
        if (abullets[i].y > scr_h) {
            abullets[i].active = FALSE;
            continue;
        }

        /* hit shield? */
        if (hit_shield(abullets[i].x, abullets[i].y)) {
            abullets[i].active = FALSE;
            continue;
        }

        /* hit player? */
        if (abullets[i].x >= player_x - player_w / 2 &&
            abullets[i].x <= player_x + player_w / 2 &&
            abullets[i].y >= player_y &&
            abullets[i].y <= player_y + player_h) {
            abullets[i].active = FALSE;
            lives--;
            ami_noteon(AMI_SYNTH_OUT, 0, 1, EXPLODE_NOTE, INT_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout() + EXPLODE_DUR,
                        1, EXPLODE_NOTE, INT_MAX);
            if (lives <= 0) {
                game_over = TRUE;
            }
        }
    }
}

/*******************************************************************************

Move aliens

*******************************************************************************/

void update_aliens(void)
{
    int step_x, step_y;
    int need_drop;
    int r, c, cx, cy;

    if (aliens_alive == 0) return;

    alien_move_counter++;
    if (alien_move_counter < alien_move_rate) return;
    alien_move_counter = 0;

    step_x = scr_w / 80;
    if (step_x < 2) step_x = 2;
    step_y = alien_h / 2;

    /* check if we need to change direction */
    need_drop = FALSE;
    for (r = 0; r < ALIEN_ROWS; r++) {
        for (c = 0; c < ALIEN_COLS; c++) {
            if (!aliens[r][c].alive) continue;
            alien_pos(r, c, &cx, &cy);
            if (alien_dir == DIR_RIGHT && cx + alien_w / 2 + step_x >= scr_w) {
                need_drop = TRUE;
            }
            if (alien_dir == DIR_LEFT && cx - alien_w / 2 - step_x <= 0) {
                need_drop = TRUE;
            }
        }
    }

    if (need_drop) {
        alien_dir = -alien_dir;
        alien_base_y += step_y;

        /* check if aliens reached player level */
        for (r = 0; r < ALIEN_ROWS; r++) {
            for (c = 0; c < ALIEN_COLS; c++) {
                if (!aliens[r][c].alive) continue;
                alien_pos(r, c, &cx, &cy);
                if (cy + alien_h / 2 >= player_y) {
                    game_over = TRUE;
                    return;
                }
            }
        }
    } else {
        alien_base_x += step_x * alien_dir;
    }
}

/*******************************************************************************

Move UFO

*******************************************************************************/

void update_ufo(void)
{

    int speed;

    if (!ufo.active) return;

    speed = scr_w / 200;
    if (speed < 1) speed = 1;

    ufo.x += speed * ufo.dx;

    if (ufo.x < -ufo_w || ufo.x > scr_w + ufo_w) {
        ufo.active = FALSE;
    }

}

/*******************************************************************************

Spawn a UFO

*******************************************************************************/

void spawn_ufo(void)
{

    if (ufo.active || game_over) return;

    ufo.active = TRUE;
    ufo.y = scr_h / 16;
    if (rand() % 2 == 0) {
        ufo.x = -ufo_w;
        ufo.dx = 1;
    } else {
        ufo.x = scr_w + ufo_w;
        ufo.dx = -1;
    }

}

/*******************************************************************************

Check for wave completion

*******************************************************************************/

int check_wave_complete(void)
{

    if (aliens_alive == 0 && !game_over) {
        wave++;
        init_aliens();
        /* partially restore shields each wave */
        init_shields();
        return TRUE;
    }
    return FALSE;

}

/*******************************************************************************

Main program

*******************************************************************************/

int main(void)
{
    ami_evtrec er;
    int move_step;

    srand(time(NULL));

    ami_title(stdout, "Defenders");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    /* stay in buffered mode for ami_select double buffering */
    ami_font(stdout, AMI_FONT_SIGN);
    ami_bold(stdout, TRUE);
    ami_binvis(stdout);
    ami_frametimer(stdout, TRUE);
    flip = FALSE;

    ami_opensynthout(AMI_SYNTH_OUT);
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_LEAD_1_SQUARE);
    ami_starttimeout();

    setup_menu();

    calc_metrics();
    init_game();
    draw_all();

    /* start game timers */
    ami_timer(stdout, TIMER_AFIRE, AFIRE_MIN + rand() % (AFIRE_MAX - AFIRE_MIN),
              FALSE);
    ami_timer(stdout, TIMER_UFO, UFO_INTERVAL, FALSE);

    mouse_x = scr_w / 2;

    do {
        ami_event(stdin, &er);
if (er.etype == ami_ettim) fprintf(stderr, "tim:%d", er.timnum);

        if (er.etype == ami_etresize) {
            /* don't resize buffer - let framework handle it */
        }

        else if (er.etype == ami_etredraw) {
            draw_all();
        }

        else if (er.etype == ami_etmoumovg) {
            mouse_x = er.moupxg;
            if (game_started && !game_over) {
                player_x = mouse_x;
                if (player_x < player_w / 2) player_x = player_w / 2;
                if (player_x > scr_w - player_w / 2)
                    player_x = scr_w - player_w / 2;
            }
        }

        else if (er.etype == ami_etmouba && er.amoubn == 1) {
            if (game_started && !game_over) {
                player_fire();
            }
        }

        else if (er.etype == ami_etenter) {
            if (game_started && !game_over) {
                player_fire();
            }
        }

        else if (er.etype == ami_etleft) {
            if (game_started && !game_over) {
                move_step = scr_w / 50;
                if (move_step < 5) move_step = 5;
                player_x -= move_step;
                if (player_x < player_w / 2) player_x = player_w / 2;
            }
        }

        else if (er.etype == ami_etright) {
            if (game_started && !game_over) {
                move_step = scr_w / 50;
                if (move_step < 5) move_step = 5;
                player_x += move_step;
                if (player_x > scr_w - player_w / 2)
                    player_x = scr_w - player_w / 2;
            }
        }

        else if (er.etype == ami_etframe) {
            if (game_started && !game_over) {
                update_aliens();
                update_pbullets();
                update_abullets();
                update_ufo();
                check_wave_complete();
            }
            /* double buffer: select offscreen, draw, flip */
            ami_select(stdout, !flip+1, flip+1);
            draw_all();
            flip = !flip;
        }

        else if (er.etype == ami_ettim) {

            if (er.timnum == TIMER_AFIRE && game_started && !game_over) {
                alien_fire();
                /* restart alien fire timer with random interval */
                ami_timer(stdout, TIMER_AFIRE,
                          AFIRE_MIN + rand() % (AFIRE_MAX - AFIRE_MIN), FALSE);
            }

            else if (er.timnum == TIMER_UFO && game_started && !game_over) {
                spawn_ufo();
                ami_timer(stdout, TIMER_UFO, UFO_INTERVAL, FALSE);
            }

        }

        else if (er.etype == ami_etmenus) {
            switch (er.menuid) {
                case MENU_NEW:
                    init_game();
                    ami_timer(stdout, TIMER_AFIRE,
                              AFIRE_MIN + rand() % (AFIRE_MAX - AFIRE_MIN),
                              FALSE);
                    ami_timer(stdout, TIMER_UFO, UFO_INTERVAL, FALSE);
                    draw_all();
                    break;
                case MENU_EXIT:
                    er.etype = ami_etterm;
                    break;
                case MENU_ABOUT:
                    ami_alert("About Defenders",
                              "Defenders - A Space Invaders style game\n"
                              "Use arrows or mouse to move, click to fire\n"
                              "Copyright (C) 2026 S. A. Franco");
                    draw_all();
                    break;
            }
        }

    } while (er.etype != ami_etterm);

    ami_closesynthout(AMI_SYNTH_OUT);

}
