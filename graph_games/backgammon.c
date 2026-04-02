/*******************************************************************************
*                                                                              *
*                             BACKGAMMON GAME                                  *
*                                                                              *
*                       COPYRIGHT (C) 2026 S. A. FRANCO                        *
*                                                                              *
* A graphical backgammon game with resizable window and menus. Implements full *
* backgammon rules including hitting, bearing off, bar re-entry, and doubles.  *
* Supports human vs human and human vs computer play.                          *
* Computer uses evaluation-based search over all legal move combinations.      *
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

Sound defines

*******************************************************************************/

#define DICE_NOTE    (AMI_NOTE_G+AMI_OCTAVE_5)
#define MOVE_NOTE    (AMI_NOTE_E+AMI_OCTAVE_5)
#define HIT_NOTE     (AMI_NOTE_C+AMI_OCTAVE_6)
#define BEAROFF_NOTE (AMI_NOTE_A+AMI_OCTAVE_5)
#define WIN_NOTE     (AMI_NOTE_C+AMI_OCTAVE_4)
#define DICE_DUR     200
#define MOVE_DUR     100
#define HIT_DUR      200
#define BEAROFF_DUR  250
#define WIN_DUR      800

/*******************************************************************************

Color helper

*******************************************************************************/

#define CLR(v) ((v) * (INT_MAX / 255))

/*******************************************************************************

Game constants

*******************************************************************************/

#define NUM_POINTS    24
#define NUM_CHECKERS  15
#define MAX_STACK     15

/* Players: P1 = white, moves 24->1; P2 = black, moves 1->24 */
#define P1  0
#define P2  1

/* Menu ids */
#define MENU_NEW     100
#define MENU_EXIT    101
#define MENU_ABOUT   102
#define MENU_PVP     103
#define MENU_PVC_W   104
#define MENU_PVC_B   105
#define MENU_UNDO    106
#define MENU_DOUBLE  107

/* Game modes */
#define MODE_PVP     0
#define MODE_PVC_W   1   /* player is white (P1), computer is black (P2) */
#define MODE_PVC_B   2   /* player is black (P2), computer is white (P1) */

/* AI timer */
#define TIMER_AI     2
#define TIMER_ANIM   3
#define ANIM_TICK    800   /* ~80ms per animation frame */

/* Game states */
#define GS_ROLL      0   /* waiting to roll dice */
#define GS_MOVE      1   /* selecting moves */
#define GS_GAMEOVER  2   /* game finished */

/* Max move sequences for AI evaluation */
#define MAX_COMBOS   5000

/* Undo history */
#define MAX_UNDO 20
typedef struct {
    int board[NUM_POINTS];
    int bar[2];
    int off[2];
    int dice[2];
    int used[4];
    int ndice;
    int turn;
    int selected_point;
} undo_state;
undo_state undo_stack[MAX_UNDO];
int undo_top;  /* index of next free slot */

/*******************************************************************************

Board state

*******************************************************************************/

/* board[0..23] = number of checkers on each point (1-indexed conceptually,
   but 0-indexed in array: board[0] = point 1, board[23] = point 24)
   Positive = P1 (white), Negative = P2 (black) */
int board[NUM_POINTS];
int bar[2];           /* checkers on the bar: bar[P1], bar[P2] */
int off[2];           /* checkers borne off: off[P1], off[P2] */
int turn;             /* P1 or P2 */
int gamestate;        /* GS_ROLL, GS_MOVE, GS_GAMEOVER */
int gamemode;         /* MODE_PVP, MODE_PVC_W, MODE_PVC_B */
int ai_pending;

int dice[2];          /* current dice values */
int used[4];          /* which dice are used (up to 4 for doubles) */
int ndice;            /* 2 normally, 4 for doubles */
int selected_point;   /* currently selected source point, -1 = none,
                         24 = bar for P1, 25 = bar for P2 */
int mousex, mousey;
int sound_enabled;

/* timed status message overlay */
char overlay_msg[200];
int overlay_active;
#define TIMER_MSG 4
#define MSG_DURATION 20000  /* 2 seconds */

/* doubling cube */
int cube_value;       /* current cube value: 1, 2, 4, 8, 16, 32, 64 */
int cube_owner;       /* -1 = center (either can double), P1 or P2 = owner */
int score[2];         /* cumulative match score */

/* animation state */
#define ANIM_FRAMES 15
int animating;         /* TRUE during move animation */
int anim_frame;        /* current frame 0..ANIM_FRAMES */
int anim_sx, anim_sy;  /* source pixel position */
int anim_dx, anim_dy;  /* destination pixel position */
int anim_player;       /* which player's checker is moving */
/* deferred move data - applied after animation completes */
int anim_src, anim_dest, anim_dv, anim_from_bar, anim_bear_off;
int anim_was_hit;      /* TRUE if this move hits opponent */
int anim_hit_player;   /* which player gets hit */

/* forward declarations */
int is_computer_turn(void);
void draw_all(void);
void animate_move(int src, int dest, int player, int from_bar, int bear_off);
void undo_push(void);
int undo_pop(void);

/*******************************************************************************

Initialization

*******************************************************************************/

void init_board(void)
{
    int i;

    memset(board, 0, sizeof(board));
    bar[P1] = 0; bar[P2] = 0;
    off[P1] = 0; off[P2] = 0;

    /* Standard backgammon starting position.
       P1 (white, positive) moves from point 24 toward point 1.
       P2 (black, negative) moves from point 1 toward point 24.
       board[i] represents point (i+1). */

    /* P1: 2 on point 24, 5 on point 13, 3 on point 8, 5 on point 6 */
    board[23] = 2;   /* point 24 */
    board[12] = 5;   /* point 13 */
    board[7]  = 3;   /* point 8 */
    board[5]  = 5;   /* point 6 */

    /* P2: 2 on point 1, 5 on point 12, 3 on point 17, 5 on point 19 */
    board[0]  = -2;  /* point 1 */
    board[11] = -5;  /* point 12 */
    board[16] = -3;  /* point 17 */
    board[18] = -5;  /* point 19 */

    turn = P1;
    gamestate = GS_ROLL;
    ai_pending = FALSE;
    selected_point = -1;
    dice[0] = 0; dice[1] = 0;
    ndice = 0;
    undo_top = 0;
    cube_value = 1;
    cube_owner = -1; /* center */
    for (i = 0; i < 4; i++) used[i] = FALSE;
}

/*******************************************************************************

Dice

*******************************************************************************/

void roll_dice(void)
{
    dice[0] = rand() % 6 + 1;
    dice[1] = rand() % 6 + 1;
    if (dice[0] == dice[1])
        ndice = 4;
    else
        ndice = 2;
    used[0] = FALSE; used[1] = FALSE;
    used[2] = FALSE; used[3] = FALSE;
}

/* Get the value of die slot i */
int die_val(int i)
{
    if (ndice == 4)
        return dice[0]; /* all four are same value */
    if (i < 2)
        return dice[i];
    return 0;
}

/* Get available (unused) dice values into array, return count */
int get_available_dice(int *vals)
{
    int i, n = 0;
    for (i = 0; i < ndice; i++)
        if (!used[i])
            vals[n++] = die_val(i);
    return n;
}

/* Count remaining unused dice */
int dice_remaining(void)
{
    int i, n = 0;
    for (i = 0; i < ndice; i++)
        if (!used[i]) n++;
    return n;
}

/* Mark a die with given value as used. Returns index used, or -1 */
int use_die(int val)
{
    int i;
    for (i = 0; i < ndice; i++)
        if (!used[i] && die_val(i) == val) { used[i] = TRUE; return i; }
    return -1;
}

/* Unuse a die at index */
void unuse_die(int idx)
{
    if (idx >= 0 && idx < 4) used[idx] = FALSE;
}

/*******************************************************************************

Move legality

*******************************************************************************/

/* Check if all of player's checkers are in their home board */
int all_in_home(int player)
{
    int i;
    if (player == P1) {
        if (bar[P1] > 0) return FALSE;
        for (i = 6; i < 24; i++)
            if (board[i] > 0) return FALSE;
    } else {
        if (bar[P2] > 0) return FALSE;
        for (i = 0; i < 18; i++)
            if (board[i] < 0) return FALSE;
    }
    return TRUE;
}

/* Check if a point is occupied by opponent (more than 1 checker) */
int point_blocked(int pt, int player)
{
    if (pt < 0 || pt > 23) return FALSE;
    if (player == P1)
        return board[pt] < -1;
    else
        return board[pt] > 1;
}

/* Check if a point has a single opponent checker (blot) */
int point_is_blot(int pt, int player)
{
    if (pt < 0 || pt > 23) return FALSE;
    if (player == P1)
        return board[pt] == -1;
    else
        return board[pt] == 1;
}

/* Can player move a checker from the bar with die value dv? */
int can_enter(int player, int dv)
{
    int dest;
    if (player == P1)
        dest = 24 - dv;  /* entering P2's home (points 19-24, idx 18-23) */
    else
        dest = dv - 1;   /* entering P1's home (points 1-6, idx 0-5) */
    if (dest < 0 || dest > 23) return FALSE;
    return !point_blocked(dest, player);
}

/* Destination point when moving from src with die value dv.
   Returns -1 for bear off, -2 for invalid. */
int move_dest(int src, int dv, int player)
{
    int dest;
    if (player == P1) {
        dest = src - dv;
        if (dest < 0) {
            /* bearing off */
            if (!all_in_home(P1)) return -2;
            if (dest == -1) return -1; /* exact bear off from point (dv) */
            /* Can only bear off with higher die if no checker on higher point */
            {
                int i;
                for (i = src + 1; i < 6; i++)
                    if (board[i] > 0) return -2;
            }
            return -1;
        }
    } else {
        dest = src + dv;
        if (dest > 23) {
            if (!all_in_home(P2)) return -2;
            if (dest == 24) return -1;
            {
                int i;
                for (i = src - 1; i >= 18; i--)
                    if (board[i] < 0) return -2;
            }
            return -1;
        }
    }
    if (point_blocked(dest, player)) return -2;
    return dest;
}

/* Can player move from src with die value dv? src is board index 0-23 */
int can_move_from(int src, int dv, int player)
{
    if (bar[player] > 0) return FALSE; /* must enter from bar first */
    if (player == P1 && board[src] <= 0) return FALSE;
    if (player == P2 && board[src] >= 0) return FALSE;
    return move_dest(src, dv, player) >= -1;
}

/* Check if player has ANY legal move with remaining dice */
int has_any_move(int player)
{
    int vals[4], nv, i, j;
    nv = get_available_dice(vals);
    if (nv == 0) return FALSE;

    if (bar[player] > 0) {
        for (i = 0; i < nv; i++)
            if (can_enter(player, vals[i])) return TRUE;
        return FALSE;
    }

    for (j = 0; j < 24; j++) {
        if (player == P1 && board[j] <= 0) continue;
        if (player == P2 && board[j] >= 0) continue;
        for (i = 0; i < nv; i++)
            if (can_move_from(j, vals[i], player)) return TRUE;
    }
    return FALSE;
}

/*******************************************************************************

Execute a move

*******************************************************************************/

void show_message(const char *msg)
{
    strncpy(overlay_msg, msg, sizeof(overlay_msg) - 1);
    overlay_msg[sizeof(overlay_msg) - 1] = 0;
    overlay_active = TRUE;
    ami_timer(stdout, TIMER_MSG, MSG_DURATION, FALSE);
}

void play_sound(int note, int dur)
{
    if (!sound_enabled) return;
    ami_noteon(AMI_SYNTH_OUT, 0, 1, note, INT_MAX);
    ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+dur, 1, note, INT_MAX);
}

#define WAVE_SLIDE 1
#define WAVE_DICE  2

void play_slide_sound(void)
{
    if (!sound_enabled) return;
    ami_playwave(AMI_WAVE_OUT, 0, WAVE_SLIDE);
}

/* Execute move: from bar if from_bar, else from src. dv is die value.
   Returns: 0=normal, 1=hit, 2=bear off */
int do_move_internal(int src, int dv, int player, int from_bar)
{
    int dest, result = 0;

    if (from_bar) {
        bar[player]--;
        if (player == P1)
            dest = 24 - dv;
        else
            dest = dv - 1;
    } else {
        dest = move_dest(src, dv, player);
        /* remove from source */
        if (player == P1) board[src]--;
        else board[src]++;
    }

    if (dest == -1) {
        /* bearing off */
        off[player]++;
        result = 2;
    } else {
        /* check for hit */
        if (point_is_blot(dest, player)) {
            if (player == P1) {
                board[dest] = 0;
                bar[P2]++;
            } else {
                board[dest] = 0;
                bar[P1]++;
            }
            result = 1;
        }
        /* place checker */
        if (player == P1) board[dest]++;
        else board[dest]--;
    }

    use_die(dv);
    return result;
}

/* Animated move: record positions, then execute */
int do_move(int src, int dv, int player, int from_bar)
{
    int dest, bear_off;
    int src_pt;

    /* save state for undo */
    undo_push();

    /* figure out destination for animation */
    if (from_bar) {
        src_pt = 24; /* bar */
        if (player == P1) dest = 24 - dv;
        else dest = dv - 1;
        bear_off = FALSE;
    } else {
        src_pt = src;
        dest = move_dest(src, dv, player);
        bear_off = (dest == -1);
    }

    /* start animation BEFORE changing board state */
    animate_move(src_pt, dest, player, from_bar, bear_off);

    /* now execute the actual move */
    return do_move_internal(src, dv, player, from_bar);
}

/* Undo a move (for AI search) */
void undo_move(int src, int dest, int dv, int player, int from_bar,
               int was_hit, int die_idx)
{
    if (dest == -1) {
        /* undo bear off */
        off[player]--;
        if (from_bar) {
            bar[player]++;
        } else {
            if (player == P1) board[src]++;
            else board[src]--;
        }
    } else {
        /* remove checker from dest */
        if (player == P1) board[dest]--;
        else board[dest]++;

        /* undo hit */
        if (was_hit) {
            if (player == P1) {
                board[dest] = -1;
                bar[P2]--;
            } else {
                board[dest] = 1;
                bar[P1]--;
            }
        }

        if (from_bar) {
            bar[player]++;
        } else {
            if (player == P1) board[src]++;
            else board[src]--;
        }
    }

    unuse_die(die_idx);
}

/*******************************************************************************

End of turn / game over check

*******************************************************************************/

void end_turn(void)
{
    if (off[turn] >= NUM_CHECKERS) {
        gamestate = GS_GAMEOVER;
        play_sound(WIN_NOTE, WIN_DUR);
        return;
    }
    turn = 1 - turn;
    gamestate = GS_ROLL;
    selected_point = -1;
}

/*******************************************************************************

Pip count calculation

*******************************************************************************/

int pip_count(int player)
{
    int i, pips = 0;
    if (player == P1) {
        pips += bar[P1] * 25;
        for (i = 0; i < 24; i++)
            if (board[i] > 0)
                pips += board[i] * (i + 1);
    } else {
        pips += bar[P2] * 25;
        for (i = 0; i < 24; i++)
            if (board[i] < 0)
                pips += (-board[i]) * (24 - i);
    }
    return pips;
}

/*******************************************************************************

Board coordinate helpers

*******************************************************************************/

int scr_w, scr_h;
int brd_x, brd_y, brd_w, brd_h;  /* board rectangle */
int bar_w;                          /* central bar width */
int point_w;                        /* width of each point/triangle */
int bear_w;                         /* bearing off tray width */
int checker_r;                      /* checker radius */

void calc_metrics(void)
{
    scr_w = ami_maxxg(stdout);
    scr_h = ami_maxyg(stdout);

    brd_w = scr_w * 78 / 100;
    brd_h = scr_h * 88 / 100;
    bar_w = brd_w / 18;
    bear_w = brd_w / 14;
    point_w = (brd_w - bar_w - bear_w) / 12;

    /* recalculate brd_w to be exact */
    brd_w = point_w * 12 + bar_w + bear_w;
    brd_x = (scr_w - brd_w) / 2;
    brd_y = scr_h / 15;
    brd_h = scr_h - brd_y * 2;

    checker_r = point_w * 2 / 5;
    if (checker_r < 4) checker_r = 4;
}

/* Get pixel x for center of a point (0-23).
   Top row: points 12-23 left to right (index 12 at left, 23 at right)
   Bottom row: points 11-0 left to right (index 11 at left, 0 at right) */
int point_cx(int pt)
{
    int col, x;
    if (pt >= 12) {
        /* top row: point 12 at left, point 23 at right */
        col = pt - 12;
    } else {
        /* bottom row: point 11 at left, point 0 at right */
        col = 11 - pt;
    }
    x = brd_x + col * point_w + point_w / 2;
    /* account for bar in center (after first 6 columns) */
    if (col >= 6) x += bar_w;
    return x;
}

/* Get pixel y for a checker at position 'pos' (0 = base) on a point */
int checker_cy(int pt, int pos)
{
    int spacing = checker_r * 2 + 2;
    if (pos > 4) pos = 4; /* stack limit visually */
    if (pt >= 12) {
        /* top row: triangles point down */
        return brd_y + checker_r + 4 + pos * spacing;
    } else {
        /* bottom row: triangles point up */
        return brd_y + brd_h - checker_r - 4 - pos * spacing;
    }
}

/* Get source position for animation - where the checker is now */
void get_checker_src_pos(int src, int player, int *px, int *py)
{
    if (src == 24 || src == 25) {
        /* from bar */
        *px = brd_x + point_w * 6 + bar_w / 2;
        if (player == P1)
            *py = brd_y + brd_h / 2 + checker_r + 4 + (bar[P1] - 1) * (checker_r * 2 + 2);
        else
            *py = brd_y + brd_h / 2 - checker_r - 4 - (bar[P2] - 1) * (checker_r * 2 + 2);
    } else {
        int cnt = board[src] > 0 ? board[src] : -board[src];
        *px = point_cx(src);
        *py = checker_cy(src, cnt - 1);
    }
}

/* Get destination position for animation - where the checker will land */
void get_checker_dest_pos(int dest, int player, int bear_off, int *px, int *py)
{
    if (bear_off) {
        /* bear off tray */
        *px = brd_x + point_w * 12 + bar_w + bear_w / 2;
        int spacing = checker_r;
        if (spacing < 4) spacing = 4;
        if (player == P1)
            *py = brd_y + brd_h - 8 - off[P1] * spacing;
        else
            *py = brd_y + 8 + off[P2] * spacing;
    } else {
        int cnt = board[dest] > 0 ? board[dest] : -board[dest];
        /* if landing on opponent single, it'll be replaced, so count = 0 */
        if (board[dest] != 0 && ((player == P1 && board[dest] < 0) ||
                                  (player == P2 && board[dest] > 0))) {
            cnt = 0;
        }
        *px = point_cx(dest);
        *py = checker_cy(dest, cnt);
    }
}

/* Start a visual animation from src to dest position.
   Call AFTER the board state has already been updated.
   The animation just shows the checker sliding visually. */
void start_move_anim(int sx, int sy, int dx, int dy, int player)
{
    anim_sx = sx; anim_sy = sy;
    anim_dx = dx; anim_dy = dy;
    anim_player = player;
    anim_frame = 0;
    animating = TRUE;
    ami_timer(stdout, TIMER_ANIM, ANIM_TICK, TRUE);
}

void finish_animation(void)
{
    animating = FALSE;
    ami_killtimer(stdout, TIMER_ANIM);
}

/* Record positions and start animation for a move about to happen.
   Call BEFORE the board update, then do the board update after this. */
void animate_move(int src, int dest, int player, int from_bar, int bear_off)
{
    int sx, sy, dx, dy;

    get_checker_src_pos(src, player, &sx, &sy);
    get_checker_dest_pos(dest, player, bear_off, &dx, &dy);
    start_move_anim(sx, sy, dx, dy, player);
    play_slide_sound();
}

/* Convert mouse position to a point index, or special values.
   Returns: 0-23 for board points, 24 for bar area, 25 for bear off area,
   -1 for nothing. */
int mouse_to_point(int mx, int my)
{
    int col, pt;
    int half_h = brd_h / 2;
    int bar_left = brd_x + point_w * 6;
    int bar_right = bar_left + bar_w;

    if (mx < brd_x || mx > brd_x + brd_w) return -1;
    if (my < brd_y || my > brd_y + brd_h) return -1;

    /* check bear off area */
    if (mx > brd_x + point_w * 12 + bar_w) return 25;

    /* check bar area */
    if (mx >= bar_left && mx <= bar_right) return 24;

    /* determine column (0-11) */
    if (mx >= bar_right) {
        col = (mx - bar_right) / point_w + 6;
    } else {
        col = (mx - brd_x) / point_w;
    }
    if (col < 0) col = 0;
    if (col > 11) col = 11;

    /* determine top or bottom half */
    if (my < brd_y + half_h) {
        /* top row: col 0 = point 12, col 11 = point 23 */
        pt = col + 12;
    } else {
        /* bottom row: col 0 = point 11, col 11 = point 0 */
        pt = 11 - col;
    }
    return pt;
}

/*******************************************************************************

Get valid destinations for selected source and available dice

*******************************************************************************/

#define MAX_DESTS 8

typedef struct {
    int dest;  /* -1 = bear off, 0-23 = point */
    int dv;    /* die value used */
} destinfo;

int get_valid_dests(int src, int from_bar, destinfo *dests)
{
    int vals[4], nv, i, nd = 0;
    int seen[30]; /* track unique destinations */

    memset(seen, 0, sizeof(seen));
    nv = get_available_dice(vals);

    for (i = 0; i < nv; i++) {
        int dest;
        if (from_bar) {
            if (!can_enter(turn, vals[i])) continue;
            if (turn == P1) dest = 24 - vals[i];
            else dest = vals[i] - 1;
        } else {
            if (!can_move_from(src, vals[i], turn)) continue;
            dest = move_dest(src, vals[i], turn);
        }
        /* Use dest+1 as index (dest can be -1 for bear off) */
        if (!seen[dest + 1]) {
            seen[dest + 1] = TRUE;
            if (nd < MAX_DESTS) {
                dests[nd].dest = dest;
                dests[nd].dv = vals[i];
                nd++;
            }
        }
    }
    return nd;
}

/*******************************************************************************

Drawing

*******************************************************************************/

void draw_board_bg(void)
{
    int i, x1, y1, x2, y2, cx, tri_h;
    int mid_x;

    /* background */
    ami_fcolorg(stdout, CLR(50), CLR(50), CLR(50));
    ami_frect(stdout, 1, 1, scr_w, scr_h);

    /* board border (wood frame) */
    ami_fcolorg(stdout, CLR(101), CLR(67), CLR(33));
    ami_frect(stdout, brd_x - 6, brd_y - 6,
              brd_x + brd_w + 6, brd_y + brd_h + 6);

    /* board felt background (dark green) */
    ami_fcolorg(stdout, CLR(0), CLR(80), CLR(40));
    ami_frect(stdout, brd_x, brd_y, brd_x + brd_w, brd_y + brd_h);

    /* central bar (dark wood) */
    mid_x = brd_x + point_w * 6;
    ami_fcolorg(stdout, CLR(90), CLR(55), CLR(25));
    ami_frect(stdout, mid_x, brd_y, mid_x + bar_w, brd_y + brd_h);

    /* bearing off tray */
    x1 = brd_x + point_w * 12 + bar_w;
    ami_fcolorg(stdout, CLR(80), CLR(50), CLR(20));
    ami_frect(stdout, x1, brd_y, x1 + bear_w, brd_y + brd_h);
    /* dividing line */
    ami_fcolorg(stdout, CLR(60), CLR(35), CLR(10));
    ami_line(stdout, x1, brd_y + brd_h / 2, x1 + bear_w, brd_y + brd_h / 2);

    /* Draw triangles (points) */
    tri_h = brd_h * 2 / 5;

    for (i = 0; i < 24; i++) {
        int color_idx = i % 2;
        cx = point_cx(i);
        x1 = cx - point_w / 2 + 1;
        x2 = cx + point_w / 2 - 1;

        if (color_idx == 0)
            ami_fcolorg(stdout, CLR(139), CLR(90), CLR(43));  /* dark brown */
        else
            ami_fcolorg(stdout, CLR(222), CLR(198), CLR(158)); /* cream/tan */

        if (i >= 12) {
            /* top row: triangle points down */
            y1 = brd_y + 1;
            y2 = brd_y + tri_h;
            /* draw triangle as series of horizontal lines */
            {
                int row;
                for (row = 0; row <= tri_h; row++) {
                    int lx, rx;
                    int half_w = (x2 - x1) * (tri_h - row) / (2 * tri_h);
                    lx = cx - half_w;
                    rx = cx + half_w;
                    ami_frect(stdout, lx, y1 + row, rx, y1 + row);
                }
            }
        } else {
            /* bottom row: triangle points up */
            y1 = brd_y + brd_h - tri_h;
            y2 = brd_y + brd_h - 1;
            {
                int row;
                for (row = 0; row <= tri_h; row++) {
                    int lx, rx;
                    int half_w = (x2 - x1) * (tri_h - row) / (2 * tri_h);
                    lx = cx - half_w;
                    rx = cx + half_w;
                    ami_frect(stdout, lx, y2 - row, rx, y2 - row);
                }
            }
        }
    }
}

void draw_checker(int cx, int cy, int player, int highlight)
{
    int r = checker_r;

    /* outline */
    ami_fcolorg(stdout, CLR(30), CLR(30), CLR(30));
    ami_fellipse(stdout, cx - r, cy - r, cx + r, cy + r);

    /* body */
    r -= 2;
    if (r < 2) r = 2;
    if (player == P1) {
        if (highlight)
            ami_fcolorg(stdout, CLR(200), CLR(255), CLR(200));
        else
            ami_fcolorg(stdout, CLR(240), CLR(235), CLR(220));
    } else {
        if (highlight)
            ami_fcolorg(stdout, CLR(150), CLR(100), CLR(100));
        else
            ami_fcolorg(stdout, CLR(50), CLR(40), CLR(35));
    }
    ami_fellipse(stdout, cx - r, cy - r, cx + r, cy + r);

    /* inner ring for 3D effect */
    r = r * 2 / 3;
    if (r < 1) r = 1;
    if (player == P1)
        ami_fcolorg(stdout, CLR(255), CLR(250), CLR(240));
    else
        ami_fcolorg(stdout, CLR(70), CLR(55), CLR(50));
    ami_fellipse(stdout, cx - r, cy - r, cx + r, cy + r);

    /* center dot */
    r = r / 3;
    if (r < 1) r = 1;
    if (player == P1)
        ami_fcolorg(stdout, CLR(220), CLR(215), CLR(200));
    else
        ami_fcolorg(stdout, CLR(45), CLR(35), CLR(30));
    ami_fellipse(stdout, cx - r, cy - r, cx + r, cy + r);
}

void draw_checkers(void)
{
    int i, j, cnt, player, cx, cy;
    int is_sel;

    for (i = 0; i < 24; i++) {
        if (board[i] == 0) continue;
        player = board[i] > 0 ? P1 : P2;
        cnt = board[i] > 0 ? board[i] : -board[i];
        is_sel = (selected_point == i);

        for (j = 0; j < cnt; j++) {
            cx = point_cx(i);
            cy = checker_cy(i, j);
            /* If last checker in selected stack, highlight */
            draw_checker(cx, cy, player, is_sel && j == cnt - 1);
        }
    }

    /* Draw bar checkers */
    {
        int bar_cx = brd_x + point_w * 6 + bar_w / 2;
        int spacing = checker_r * 2 + 2;

        /* P1 bar (bottom half of bar) */
        for (j = 0; j < bar[P1]; j++) {
            cy = brd_y + brd_h / 2 + checker_r + 4 + j * spacing;
            is_sel = (selected_point == 24 && turn == P1);
            draw_checker(bar_cx, cy, P1, is_sel && j == bar[P1] - 1);
        }

        /* P2 bar (top half of bar) */
        for (j = 0; j < bar[P2]; j++) {
            cy = brd_y + brd_h / 2 - checker_r - 4 - j * spacing;
            is_sel = (selected_point == 24 && turn == P2);
            draw_checker(bar_cx, cy, P2, is_sel && j == bar[P2] - 1);
        }
    }

    /* Draw borne off checkers in tray */
    {
        int tray_x = brd_x + point_w * 12 + bar_w + bear_w / 2;
        int spacing = checker_r;
        if (spacing < 4) spacing = 4;

        /* P1 borne off (bottom half) */
        for (j = 0; j < off[P1]; j++) {
            cy = brd_y + brd_h - 8 - j * spacing;
            draw_checker(tray_x, cy, P1, FALSE);
        }

        /* P2 borne off (top half) */
        for (j = 0; j < off[P2]; j++) {
            cy = brd_y + 8 + j * spacing;
            draw_checker(tray_x, cy, P2, FALSE);
        }
    }
}

/* Get the landing stack position for a checker arriving at dest.
   Accounts for existing checkers and potential hits. */
int landing_pos(int dest, int player)
{
    int cnt;

    if (dest < 0 || dest >= 24) return 0;
    cnt = board[dest];
    if (player == P1) {
        if (cnt < 0) return 0; /* hit: replaces the blot */
        return cnt;
    } else {
        if (cnt > 0) return 0;
        return -cnt;
    }
}

/* Draw a highlight circle at a destination point */
void draw_dest_circle(int dest, int stack_pos, int cr, int cg, int cb, int lw)
{
    int cx, cy;

    if (dest == -1) {
        /* bear off tray */
        int tx = brd_x + point_w * 12 + bar_w;
        ami_fcolorg(stdout, CLR(cr), CLR(cg), CLR(cb));
        ami_linewidth(stdout, lw);
        ami_rect(stdout, tx + 2, brd_y + 2,
                 tx + bear_w - 2, brd_y + brd_h - 2);
        ami_linewidth(stdout, 1);
    } else {
        cx = point_cx(dest);
        cy = checker_cy(dest, stack_pos);
        ami_fcolorg(stdout, CLR(cr), CLR(cg), CLR(cb));
        ami_linewidth(stdout, lw);
        ami_ellipse(stdout, cx - checker_r, cy - checker_r,
                    cx + checker_r, cy + checker_r);
        ami_linewidth(stdout, 1);
    }
}

/* Recursively find combo destinations reachable by using multiple dice.
   depth = number of dice used so far (1 = single, 2+ = combo).
   cur_pt = current simulated position.
   dice_used = bitmask of which dice slots are used in this path.
   seen[depth] tracks which destinations already shown at each depth. */
void find_combo_dests(int src, int cur_pt, int depth, int max_depth,
                      int dice_used, int single_seen[30],
                      int combo_seen[4][30])
{
    int vals[4], nv, i;

    nv = get_available_dice(vals);

    for (i = 0; i < nv; i++) {
        int bit = 1 << i;
        int mid_dest;

        /* for non-doubles, each die used once */
        if (ndice < 4) {
            if (dice_used & bit) continue;
        } else {
            /* doubles: count how many we've used vs available */
            int used_count = 0, avail = 0, k;
            for (k = 0; k < ndice; k++) {
                if (!used[k]) avail++;
            }
            /* count bits set in dice_used */
            for (k = 0; k < 4; k++)
                if (dice_used & (1 << k)) used_count++;
            if (used_count >= avail) continue;
            /* for doubles, don't revisit same combo path */
            if (dice_used & bit) {
                /* find next unused bit */
                int found = FALSE;
                int k2;
                for (k2 = i + 1; k2 < 4; k2++) {
                    if (!(dice_used & (1 << k2))) {
                        bit = 1 << k2;
                        found = TRUE;
                        break;
                    }
                }
                if (!found) continue;
            }
        }

        if (!can_move_from(cur_pt, vals[i], turn)) continue;

        /* simulate move */
        int save_cur = board[cur_pt];
        if (turn == P1) board[cur_pt]--;
        else board[cur_pt]++;

        mid_dest = move_dest(cur_pt, vals[i], turn);

        if (mid_dest >= 0 && mid_dest < 24) {
            /* check if blocked */
            if ((turn == P1 && board[mid_dest] < -1) ||
                (turn == P2 && board[mid_dest] > 1)) {
                board[cur_pt] = save_cur;
                continue;
            }

            int save_mid = board[mid_dest];
            /* simulate landing */
            if ((turn == P1 && board[mid_dest] == -1) ||
                (turn == P2 && board[mid_dest] == 1))
                board[mid_dest] = 0; /* hit */
            if (turn == P1) board[mid_dest]++;
            else board[mid_dest]--;

            int idx = mid_dest + 1;
            if (depth >= 1 && !single_seen[idx] && !combo_seen[depth][idx]) {
                combo_seen[depth][idx] = TRUE;
                int sp = landing_pos(mid_dest, turn);
                draw_dest_circle(mid_dest, sp, 0, 220, 255, 5);
            }

            /* recurse for deeper combos */
            if (depth + 1 < max_depth) {
                find_combo_dests(src, mid_dest, depth + 1, max_depth,
                                 dice_used | bit, single_seen, combo_seen);
            }

            board[mid_dest] = save_mid;
        } else if (mid_dest == -1 && depth >= 1) {
            /* bear off combo */
            if (!combo_seen[depth][-1 + 1]) {
                combo_seen[depth][-1 + 1] = TRUE;
                draw_dest_circle(-1, 0, 0, 220, 255, 5);
            }
        }

        board[cur_pt] = save_cur;
    }
}

void draw_highlight_dests(void)
{
    destinfo dests[MAX_DESTS];
    int nd, i;
    int from_bar;
    int single_seen[30];
    int combo_seen[4][30];
    int avail;

    if (selected_point < 0) return;
    from_bar = (selected_point == 24);
    nd = get_valid_dests(from_bar ? -1 : selected_point, from_bar, dests);

    memset(single_seen, 0, sizeof(single_seen));
    memset(combo_seen, 0, sizeof(combo_seen));

    /* Draw single-die destinations in yellow */
    for (i = 0; i < nd; i++) {
        int sp = (dests[i].dest >= 0) ? landing_pos(dests[i].dest, turn) : 0;
        draw_dest_circle(dests[i].dest, sp, 255, 255, 0, 5);
        if (dests[i].dest >= -1)
            single_seen[dests[i].dest + 1] = TRUE;
    }

    /* Draw combo destinations in cyan */
    if (!from_bar) {
        /* count available dice */
        avail = 0;
        for (i = 0; i < ndice; i++)
            if (!used[i]) avail++;
        if (avail >= 2) {
            find_combo_dests(selected_point, selected_point, 1,
                             avail, 0, single_seen, combo_seen);
        }
    }
}

void draw_dice_display(void)
{
    int dx, dy, dsz, i, val;
    int dot_r;

    if (gamestate == GS_ROLL && dice[0] == 0) return;

    dsz = brd_h / 10;
    if (dsz < 20) dsz = 20;
    if (dsz > 60) dsz = 60;
    dot_r = dsz / 8;
    if (dot_r < 2) dot_r = 2;

    /* Position dice in center of board */
    dx = brd_x + brd_w / 2 - dsz - dsz / 4;
    dy = brd_y + brd_h / 2 - dsz / 2;

    for (i = 0; i < 2; i++) {
        int x1, y1, x2, y2, cx, cy;
        int is_used = TRUE;

        val = dice[i];
        if (val == 0) continue;

        /* Check if any die with this value is still available */
        {
            int k;
            for (k = 0; k < ndice; k++)
                if (!used[k] && die_val(k) == val) { is_used = FALSE; break; }
        }

        x1 = dx + i * (dsz + dsz / 2);
        y1 = dy;
        x2 = x1 + dsz;
        y2 = y1 + dsz;
        cx = (x1 + x2) / 2;
        cy = (y1 + y2) / 2;

        /* die background */
        if (is_used)
            ami_fcolorg(stdout, CLR(120), CLR(120), CLR(120));
        else
            ami_fcolorg(stdout, CLR(240), CLR(230), CLR(210));
        ami_frect(stdout, x1, y1, x2, y2);

        /* die border */
        ami_fcolorg(stdout, CLR(60), CLR(50), CLR(40));
        ami_rect(stdout, x1, y1, x2, y2);

        /* dots */
        if (is_used)
            ami_fcolorg(stdout, CLR(80), CLR(80), CLR(80));
        else
            ami_fcolorg(stdout, CLR(20), CLR(20), CLR(20));

        {
            int dx1 = x1 + dsz / 4;
            int dx2 = cx;
            int dx3 = x2 - dsz / 4;
            int dy1 = y1 + dsz / 4;
            int dy2 = cy;
            int dy3 = y2 - dsz / 4;

            if (val == 1 || val == 3 || val == 5)
                ami_fellipse(stdout, dx2-dot_r, dy2-dot_r,
                             dx2+dot_r, dy2+dot_r);
            if (val >= 2)
                ami_fellipse(stdout, dx1-dot_r, dy3-dot_r,
                             dx1+dot_r, dy3+dot_r);
            if (val >= 2)
                ami_fellipse(stdout, dx3-dot_r, dy1-dot_r,
                             dx3+dot_r, dy1+dot_r);
            if (val >= 4)
                ami_fellipse(stdout, dx1-dot_r, dy1-dot_r,
                             dx1+dot_r, dy1+dot_r);
            if (val >= 4)
                ami_fellipse(stdout, dx3-dot_r, dy3-dot_r,
                             dx3+dot_r, dy3+dot_r);
            if (val == 6) {
                ami_fellipse(stdout, dx1-dot_r, dy2-dot_r,
                             dx1+dot_r, dy2+dot_r);
                ami_fellipse(stdout, dx3-dot_r, dy2-dot_r,
                             dx3+dot_r, dy2+dot_r);
            }
        }
    }

    /* Show remaining dice count for doubles */
    if (ndice == 4) {
        int rem = dice_remaining();
        char buf[20];
        int fsz = dsz / 3;
        if (fsz < 10) fsz = 10;
        ami_fontsiz(stdout, fsz);
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(200));
        sprintf(buf, "x%d left", rem);
        ami_cursorg(stdout, dx, dy + dsz + 4);
        printf("%s", buf);
    }
}

void draw_point_labels(void)
{
    int i, cx, fsz;
    char buf[4];

    fsz = point_w / 3;
    if (fsz < 8) fsz = 8;
    ami_fontsiz(stdout, fsz);
    ami_fcolorg(stdout, CLR(180), CLR(170), CLR(150));

    for (i = 0; i < 24; i++) {
        cx = point_cx(i);
        sprintf(buf, "%d", i + 1);
        if (i >= 12) {
            /* top: label above */
            ami_cursorg(stdout, cx - ami_strsiz(stdout, buf) / 2,
                        brd_y - fsz - 2);
        } else {
            /* bottom: label below */
            ami_cursorg(stdout, cx - ami_strsiz(stdout, buf) / 2,
                        brd_y + brd_h + 4);
        }
        printf("%s", buf);
    }
}

void draw_status(void)
{
    int fsz, sy;
    char msg[160];
    char buf[80];

    fsz = scr_h / 30;
    if (fsz < 12) fsz = 12;
    sy = brd_y + brd_h + fsz + 8;

    ami_fontsiz(stdout, fsz);

    /* Pip counts on sides */
    ami_fcolorg(stdout, CLR(240), CLR(235), CLR(220));
    sprintf(buf, "White: %d pips", pip_count(P1));
    ami_cursorg(stdout, brd_x, sy);
    printf("%s", buf);

    ami_fcolorg(stdout, CLR(160), CLR(150), CLR(140));
    sprintf(buf, "Black: %d pips", pip_count(P2));
    ami_cursorg(stdout, brd_x + brd_w - ami_strsiz(stdout, buf), sy);
    printf("%s", buf);

    /* Center status message */
    ami_fcolorg(stdout, CLR(255), CLR(255), CLR(200));

    if (gamestate == GS_GAMEOVER) {
        /* check for gammon/backgammon multiplier */
        int loser = 1 - turn;
        int mult = 1;
        if (off[loser] == 0) {
            mult = 2; /* gammon */
            if (bar[loser] > 0) mult = 3; /* backgammon */
            else {
                /* check if loser has checkers in winner's home */
                int k2;
                for (k2 = 0; k2 < 24; k2++) {
                    if (loser == P1 && board[k2] > 0 && k2 >= 18) { mult = 3; break; }
                    if (loser == P2 && board[k2] < 0 && k2 <= 5) { mult = 3; break; }
                }
            }
        }
        int pts = cube_value * mult;
        score[turn] += pts;
        if (mult == 3)
            sprintf(msg, "%s wins BACKGAMMON! (+%d pts)",
                    turn == P1 ? "White" : "Black", pts);
        else if (mult == 2)
            sprintf(msg, "%s wins GAMMON! (+%d pts)",
                    turn == P1 ? "White" : "Black", pts);
        else
            sprintf(msg, "%s wins! (+%d pts)",
                    turn == P1 ? "White" : "Black", pts);
    } else if (gamestate == GS_ROLL) {
        if (is_computer_turn())
            sprintf(msg, "%s (computer) rolling...",
                    turn == P1 ? "White" : "Black");
        else
            sprintf(msg, "%s - Click to roll dice",
                    turn == P1 ? "White" : "Black");
    } else {
        if (is_computer_turn())
            sprintf(msg, "%s (computer) thinking...",
                    turn == P1 ? "White" : "Black");
        else if (selected_point >= 0)
            sprintf(msg, "%s - Click destination",
                    turn == P1 ? "White" : "Black");
        else if (bar[turn] > 0)
            sprintf(msg, "%s - Click bar checker to enter",
                    turn == P1 ? "White" : "Black");
        else
            sprintf(msg, "%s - Select a checker to move",
                    turn == P1 ? "White" : "Black");
    }
    ami_cursorg(stdout, scr_w / 2 - ami_strsiz(stdout, msg) / 2, sy);
    printf("%s", msg);

    /* Score and doubling cube */
    {
        int sy2 = sy + fsz + 4;
        int csz = fsz;
        int ccx, ccy;
        char cbuf[16];

        /* draw scores */
        ami_fcolorg(stdout, CLR(240), CLR(235), CLR(220));
        sprintf(buf, "Score: %d", score[P1]);
        ami_cursorg(stdout, brd_x, sy2);
        printf("%s", buf);

        ami_fcolorg(stdout, CLR(160), CLR(150), CLR(140));
        sprintf(buf, "Score: %d", score[P2]);
        ami_cursorg(stdout, brd_x + brd_w - ami_strsiz(stdout, buf), sy2);
        printf("%s", buf);

        /* draw doubling cube */
        sprintf(cbuf, "%d", cube_value);
        ccx = scr_w / 2;
        ccy = sy2;

        /* cube background */
        ami_fcolorg(stdout, CLR(220), CLR(220), CLR(200));
        ami_frect(stdout, ccx - csz, ccy - csz / 4,
                  ccx + csz, ccy + csz);
        ami_fcolorg(stdout, CLR(60), CLR(60), CLR(60));
        ami_rect(stdout, ccx - csz, ccy - csz / 4,
                 ccx + csz, ccy + csz);

        /* cube value */
        ami_fcolorg(stdout, CLR(30), CLR(30), CLR(30));
        ami_fontsiz(stdout, csz * 3 / 4);
        ami_cursorg(stdout, ccx - ami_strsiz(stdout, cbuf) / 2,
                    ccy + csz / 8);
        printf("%s", cbuf);
        ami_fontsiz(stdout, fsz); /* restore */

        /* owner indicator */
        if (cube_owner == P1) {
            ami_fcolorg(stdout, CLR(240), CLR(235), CLR(220));
            ami_cursorg(stdout, ccx - csz - ami_strsiz(stdout, "W") - 4, ccy);
            printf("W");
        } else if (cube_owner == P2) {
            ami_fcolorg(stdout, CLR(100), CLR(90), CLR(80));
            ami_cursorg(stdout, ccx + csz + 4, ccy);
            printf("B");
        }
    }
}

void draw_all(void)
{
    calc_metrics();
    draw_board_bg();
    draw_checkers();

    /* draw the animated checker if animation is in progress */
    if (animating) {
        float t = (float)anim_frame / ANIM_FRAMES;
        int cx = anim_sx + (int)((anim_dx - anim_sx) * t);
        int cy = anim_sy + (int)((anim_dy - anim_sy) * t);
        draw_checker(cx, cy, anim_player, TRUE);
    }

    draw_highlight_dests();
    draw_dice_display();
    draw_point_labels();
    draw_status();

    /* draw overlay message if active */
    if (overlay_active) {
        int fsz = scr_h / 20;
        int mw, mh, mx, my;
        if (fsz < 14) fsz = 14;
        ami_fontsiz(stdout, fsz);
        mw = ami_strsiz(stdout, overlay_msg) + fsz * 2;
        mh = fsz * 3;
        mx = scr_w / 2 - mw / 2;
        my = scr_h / 2 - mh / 2;

        /* dark background box */
        ami_fcolorg(stdout, CLR(20), CLR(20), CLR(40));
        ami_frect(stdout, mx, my, mx + mw, my + mh);
        ami_fcolorg(stdout, CLR(200), CLR(180), CLR(80));
        ami_rect(stdout, mx, my, mx + mw, my + mh);

        /* text */
        ami_fcolorg(stdout, CLR(255), CLR(255), CLR(200));
        ami_cursorg(stdout, mx + fsz, my + fsz);
        printf("%s", overlay_msg);
    }
}

/*******************************************************************************

Computer AI

*******************************************************************************/

int is_computer_turn(void)
{
    if (gamemode == MODE_PVP) return FALSE;
    if (gamemode == MODE_PVC_W && turn == P2) return TRUE;
    if (gamemode == MODE_PVC_B && turn == P1) return TRUE;
    return FALSE;
}

/* Board evaluation from P1's perspective (higher = better for P1) */
/* Count how many opponent checkers can hit a blot at point pt */
int blot_exposure(int pt, int opponent)
{
    int hits = 0, i, dist;

    for (i = 0; i < 24; i++) {
        if (opponent == P1 && board[i] <= 0) continue;
        if (opponent == P2 && board[i] >= 0) continue;
        if (opponent == P1) dist = i - pt;
        else dist = pt - i;
        if (dist > 0 && dist <= 12) hits++; /* within direct/combo range */
        if (dist > 0 && dist <= 6) hits++;  /* within direct shot range */
    }
    /* bar checkers can also hit */
    if (bar[opponent] > 0) {
        if (opponent == P1 && pt >= 18 && pt <= 23) hits += 2;
        if (opponent == P2 && pt >= 0 && pt <= 5) hits += 2;
    }
    return hits;
}

/* Evaluate for a specific player (positive = good for that player) */
int eval_player(int player)
{
    int i, sc = 0;
    int opp = 1 - player;
    int home_pts = 0;     /* number of home board points held */
    int prime_len = 0;    /* longest consecutive run of held points */
    int cur_prime = 0;
    int anchor_count = 0; /* points held in opponent's home board */
    int total_pip = 0;
    int blots = 0;

    /* borne off is the ultimate goal */
    sc += off[player] * 600;

    /* bar is devastating */
    sc -= bar[player] * 500;

    /* opponent on bar is great for us */
    sc += bar[opp] * 300;

    for (i = 0; i < 24; i++) {
        int cnt, dist;

        if (player == P1) {
            cnt = board[i] > 0 ? board[i] : 0;
            dist = i; /* pip distance for P1 (lower = closer to home) */
        } else {
            cnt = board[i] < 0 ? -board[i] : 0;
            dist = 23 - i;
        }

        if (cnt == 0) {
            cur_prime = 0;
            continue;
        }

        /* pip count */
        total_pip += cnt * (dist + 1);

        /* advancement: reward checkers closer to home */
        sc += cnt * (24 - dist) * 3;

        if (cnt >= 2) {
            /* holding a point */
            sc += 40;
            cur_prime++;
            if (cur_prime > prime_len) prime_len = cur_prime;

            /* home board points are very valuable */
            if (dist < 6) {
                home_pts++;
                sc += 50;
                /* inner home points (1-3) are especially valuable */
                if (dist < 3) sc += 25;
            }

            /* anchors in opponent's home are strategic */
            if (dist >= 18) {
                anchor_count++;
                sc += 35;
            }

            /* stacking more than 5 on one point is wasteful */
            if (cnt > 5) sc -= (cnt - 5) * 15;
            /* 3 is ideal for blocking */
            if (cnt == 3) sc += 10;
        } else {
            /* blot - penalize based on exposure to being hit */
            int exposure = blot_exposure(i, opp);
            sc -= 25 + exposure * 15;
            blots++;

            /* blots in opponent's home board are extra dangerous */
            if (dist >= 18) sc -= 40;
            /* blots near our home are less risky */
            if (dist < 6) sc += 15;
        }
    }

    /* prime bonus: consecutive blocked points are very strong */
    if (prime_len >= 3) sc += prime_len * 40;
    if (prime_len >= 5) sc += 150;  /* 5-prime is very strong */
    if (prime_len >= 6) sc += 300;  /* 6-prime is nearly unpassable */

    /* home board coverage bonus */
    if (home_pts >= 3) sc += 60;
    if (home_pts >= 5) sc += 120;
    if (home_pts >= 6) sc += 200; /* closed home board */

    /* bearing off readiness */
    if (all_in_home(player)) {
        sc += 250;
        /* when bearing off, low pip count is key */
        sc -= total_pip * 2;
    } else {
        /* general pip count matters but less */
        sc -= total_pip;
    }

    /* avoid too many blots */
    if (blots >= 3) sc -= blots * 30;

    return sc;
}

int evaluate(void)
{
    return eval_player(P1) - eval_player(P2);
}

/* Save/restore state for AI */
typedef struct {
    int board[NUM_POINTS];
    int bar[2];
    int off[2];
    int used[4];
} aistate;

void save_ai(aistate *s)
{
    memcpy(s->board, board, sizeof(board));
    memcpy(s->bar, bar, sizeof(bar));
    memcpy(s->off, off, sizeof(off));
    memcpy(s->used, used, sizeof(used));
}

void restore_ai(aistate *s)
{
    memcpy(board, s->board, sizeof(board));
    memcpy(bar, s->bar, sizeof(bar));
    memcpy(off, s->off, sizeof(off));
    memcpy(used, s->used, sizeof(used));
}

/* Push current state onto undo stack */
void undo_push(void)
{
    if (undo_top >= MAX_UNDO) return;
    undo_state *s = &undo_stack[undo_top];
    memcpy(s->board, board, sizeof(board));
    memcpy(s->bar, bar, sizeof(bar));
    memcpy(s->off, off, sizeof(off));
    memcpy(s->dice, dice, sizeof(dice));
    memcpy(s->used, used, sizeof(used));
    s->ndice = ndice;
    s->turn = turn;
    s->selected_point = selected_point;
    undo_top++;
}

/* Pop and restore state from undo stack */
int undo_pop(void)
{
    if (undo_top <= 0) return FALSE;
    undo_top--;
    undo_state *s = &undo_stack[undo_top];
    memcpy(board, s->board, sizeof(board));
    memcpy(bar, s->bar, sizeof(bar));
    memcpy(off, s->off, sizeof(off));
    memcpy(dice, s->dice, sizeof(dice));
    memcpy(used, s->used, sizeof(used));
    ndice = s->ndice;
    turn = s->turn;
    selected_point = s->selected_point;
    return TRUE;
}

/* Recursively find best sequence of moves for current dice.
   Tries all orderings and picks the one with best evaluation.
   best_score is updated. combo_count limits search. */
int ai_best_score;
int ai_combo_count;

void ai_search(int player, int depth)
{
    int vals[4], nv, i, j;
    int any_move = FALSE;
    int from_bar;

    ai_combo_count++;
    if (ai_combo_count > MAX_COMBOS) return;

    nv = get_available_dice(vals);
    if (nv == 0) {
        int sc = evaluate();
        if (player == P1) {
            if (sc > ai_best_score) ai_best_score = sc;
        } else {
            if (sc < ai_best_score) ai_best_score = sc;
        }
        return;
    }

    from_bar = (bar[player] > 0);

    if (from_bar) {
        for (i = 0; i < nv; i++) {
            if (can_enter(player, vals[i])) {
                aistate saved;
                int dest, was_hit;

                if (player == P1) dest = 24 - vals[i];
                else dest = vals[i] - 1;
                was_hit = point_is_blot(dest, player);

                save_ai(&saved);
                use_die(vals[i]);
                bar[player]--;
                if (was_hit) {
                    if (player == P1) { board[dest] = 0; bar[P2]++; }
                    else { board[dest] = 0; bar[P1]++; }
                }
                if (player == P1) board[dest]++;
                else board[dest]--;

                ai_search(player, depth + 1);
                any_move = TRUE;

                restore_ai(&saved);
            }
        }
    } else {
        for (j = 0; j < 24; j++) {
            if (player == P1 && board[j] <= 0) continue;
            if (player == P2 && board[j] >= 0) continue;
            for (i = 0; i < nv; i++) {
                int dest, was_hit;
                aistate saved;

                if (!can_move_from(j, vals[i], player)) continue;
                dest = move_dest(j, vals[i], player);

                was_hit = (dest >= 0 && point_is_blot(dest, player));

                save_ai(&saved);
                use_die(vals[i]);
                if (player == P1) board[j]--;
                else board[j]++;

                if (dest == -1) {
                    off[player]++;
                } else {
                    if (was_hit) {
                        if (player == P1) { board[dest] = 0; bar[P2]++; }
                        else { board[dest] = 0; bar[P1]++; }
                    }
                    if (player == P1) board[dest]++;
                    else board[dest]--;
                }

                ai_search(player, depth + 1);
                any_move = TRUE;

                restore_ai(&saved);
                if (ai_combo_count > MAX_COMBOS) return;
            }
        }
    }

    if (!any_move) {
        /* No moves possible with remaining dice */
        int sc = evaluate();
        if (player == P1) {
            if (sc > ai_best_score) ai_best_score = sc;
        } else {
            if (sc < ai_best_score) ai_best_score = sc;
        }
    }
}

/* AI top level: try each first move, pick the one leading to best score */
void ai_move(void)
{
    int vals[4], nv, i, j;
    int best_src, best_dv, best_from_bar;
    int from_bar;

    sound_enabled = FALSE;

    nv = get_available_dice(vals);
    if (nv == 0) { sound_enabled = TRUE; end_turn(); return; }

    from_bar = (bar[turn] > 0);

    best_src = -1;
    best_dv = 0;
    best_from_bar = FALSE;
    ai_best_score = (turn == P1) ? -1000000 : 1000000;

    if (from_bar) {
        for (i = 0; i < nv; i++) {
            if (can_enter(turn, vals[i])) {
                aistate saved;
                int dest, was_hit;

                if (turn == P1) dest = 24 - vals[i];
                else dest = vals[i] - 1;
                was_hit = point_is_blot(dest, turn);

                save_ai(&saved);
                use_die(vals[i]);
                bar[turn]--;
                if (was_hit) {
                    if (turn == P1) { board[dest] = 0; bar[P2]++; }
                    else { board[dest] = 0; bar[P1]++; }
                }
                if (turn == P1) board[dest]++;
                else board[dest]--;

                ai_combo_count = 0;
                int save_best = ai_best_score;
                ai_best_score = (turn == P1) ? -1000000 : 1000000;
                ai_search(turn, 1);
                int sc = ai_best_score;
                ai_best_score = save_best;

                if ((turn == P1 && sc > ai_best_score) ||
                    (turn == P2 && sc < ai_best_score)) {
                    ai_best_score = sc;
                    best_src = -1;
                    best_dv = vals[i];
                    best_from_bar = TRUE;
                }

                restore_ai(&saved);
            }
        }
    } else {
        for (j = 0; j < 24; j++) {
            if (turn == P1 && board[j] <= 0) continue;
            if (turn == P2 && board[j] >= 0) continue;
            for (i = 0; i < nv; i++) {
                int dest, was_hit;
                aistate saved;

                if (!can_move_from(j, vals[i], turn)) continue;
                dest = move_dest(j, vals[i], turn);
                was_hit = (dest >= 0 && point_is_blot(dest, turn));

                save_ai(&saved);
                use_die(vals[i]);
                if (turn == P1) board[j]--;
                else board[j]++;

                if (dest == -1) {
                    off[turn]++;
                } else {
                    if (was_hit) {
                        if (turn == P1) { board[dest] = 0; bar[P2]++; }
                        else { board[dest] = 0; bar[P1]++; }
                    }
                    if (turn == P1) board[dest]++;
                    else board[dest]--;
                }

                ai_combo_count = 0;
                int save_best = ai_best_score;
                ai_best_score = (turn == P1) ? -1000000 : 1000000;
                ai_search(turn, 1);
                int sc = ai_best_score;
                ai_best_score = save_best;

                if ((turn == P1 && sc > ai_best_score) ||
                    (turn == P2 && sc < ai_best_score)) {
                    ai_best_score = sc;
                    best_src = j;
                    best_dv = vals[i];
                    best_from_bar = FALSE;
                }

                restore_ai(&saved);
            }
        }
    }

    sound_enabled = TRUE;

    if (best_dv == 0 || (best_src < 0 && !best_from_bar)) {
        /* no legal move */
        end_turn();
        return;
    }

    /* Execute the chosen move */
    {
        int result = do_move(best_src, best_dv, turn, best_from_bar);
        if (result == 1) play_sound(HIT_NOTE, HIT_DUR);
        else if (result == 2) play_sound(BEAROFF_NOTE, BEAROFF_DUR);
    }

    /* Check if more moves remain */
    if (off[turn] >= NUM_CHECKERS) {
        gamestate = GS_GAMEOVER;
        play_sound(WIN_NOTE, WIN_DUR);
        return;
    }

    if (dice_remaining() > 0 && has_any_move(turn)) {
        /* Schedule another AI move */
        ami_timer(stdout, TIMER_AI, 300, FALSE);
        ai_pending = TRUE;
    } else {
        end_turn();
    }
}

/*******************************************************************************

Try to execute a combo move (using two dice) from src to final_dest.
Returns TRUE if successful.

*******************************************************************************/

int try_combo_move(int src, int final_dest, int from_bar)
{
    int vals[4], nv, i, j;
    int player = turn;

    nv = get_available_dice(vals);
    if (nv < 2) return FALSE;

    for (i = 0; i < nv; i++) {
        int mid;

        if (from_bar) {
            if (!can_enter(player, vals[i])) continue;
            if (player == P1) mid = 24 - vals[i];
            else mid = vals[i] - 1;
        } else {
            if (!can_move_from(src, vals[i], player)) continue;
            mid = move_dest(src, vals[i], player);
        }
        if (mid < 0) continue;

        /* check intermediate is landable */
        if (board[mid] != 0 &&
            ((player == P1 && board[mid] < -1) ||
             (player == P2 && board[mid] > 1)))
            continue;

        for (j = 0; j < nv; j++) {
            int mid_dest;

            if (j == i && ndice < 4) continue;
            if (j == i && ndice >= 4) {
                int k, avail = 0;
                for (k = 0; k < ndice; k++)
                    if (!used[k]) avail++;
                if (avail < 2) continue;
            }

            /* simulate intermediate */
            int save_src = from_bar ? 0 : board[src];
            int save_mid = board[mid];
            int save_bar = bar[player];
            if (from_bar) {
                bar[player]--;
            } else {
                if (player == P1) board[src]--;
                else board[src]++;
            }
            /* handle hit at mid */
            if ((player == P1 && board[mid] == -1) ||
                (player == P2 && board[mid] == 1)) {
                board[mid] = 0;
            }
            if (player == P1) board[mid]++;
            else board[mid]--;

            int can = can_move_from(mid, vals[j], player);
            mid_dest = can ? move_dest(mid, vals[j], player) : -2;

            /* restore */
            if (from_bar) {
                bar[player] = save_bar;
            } else {
                board[src] = save_src;
            }
            board[mid] = save_mid;

            if (mid_dest != final_dest) continue;

            /* found the combo - execute both moves */
            do_move(from_bar ? -1 : src, vals[i], player, from_bar);
            int result = do_move(mid, vals[j], player, FALSE);
            if (result == 1) play_sound(HIT_NOTE, HIT_DUR);
            else if (result == 2) play_sound(BEAROFF_NOTE, BEAROFF_DUR);
            return TRUE;
        }
    }
    return FALSE;
}

/*******************************************************************************

Handle player click

*******************************************************************************/

void handle_click(void)
{
    int pt;

    if (gamestate == GS_GAMEOVER) return;
    if (is_computer_turn()) return;

    if (gamestate == GS_ROLL) {
        /* Roll dice */
        roll_dice();
        if (sound_enabled) ami_playwave(AMI_WAVE_OUT, 0, WAVE_DICE);
        gamestate = GS_MOVE;
        selected_point = -1;

        /* Check if any moves possible */
        if (!has_any_move(turn)) {
            /* No legal moves, end turn after brief display */
            ami_timer(stdout, TIMER_AI, 500, FALSE);
            ai_pending = TRUE;
            return;
        }
        return;
    }

    /* GS_MOVE: selecting checker or destination */
    pt = mouse_to_point(mousex, mousey);
    if (pt < 0) {
        selected_point = -1;
        return;
    }

    /* If must enter from bar */
    if (bar[turn] > 0) {
        if (pt == 24) {
            /* Clicked bar - select it */
            selected_point = 24;
            return;
        }
        if (selected_point == 24 && pt >= 0 && pt <= 23) {
            /* Try to enter at clicked point */
            destinfo dests[MAX_DESTS];
            int nd, i;
            nd = get_valid_dests(-1, TRUE, dests);
            for (i = 0; i < nd; i++) {
                if (dests[i].dest == pt) {
                    int result = do_move(-1, dests[i].dv, turn, TRUE);
                    if (result == 1) play_sound(HIT_NOTE, HIT_DUR);
                    selected_point = -1;

                    if (off[turn] >= NUM_CHECKERS) {
                        gamestate = GS_GAMEOVER;
                        play_sound(WIN_NOTE, WIN_DUR);
                        return;
                    }
                    if (dice_remaining() == 0 || !has_any_move(turn))
                        end_turn();
                    return;
                }
            }
            /* try combo from bar */
            if (try_combo_move(-1, pt, TRUE)) {
                selected_point = -1;
                if (off[turn] >= NUM_CHECKERS) {
                    gamestate = GS_GAMEOVER;
                    play_sound(WIN_NOTE, WIN_DUR);
                    return;
                }
                if (dice_remaining() == 0 || !has_any_move(turn))
                    end_turn();
                return;
            }
        }
        /* auto-select bar if turn player has bar checkers */
        selected_point = 24;
        return;
    }

    /* Normal move (not from bar) */
    if (selected_point >= 0 && selected_point <= 23) {
        /* A piece is selected, check if clicking a valid destination */
        destinfo dests[MAX_DESTS];
        int nd, i;
        nd = get_valid_dests(selected_point, FALSE, dests);

        /* Check if clicked the bear off tray */
        if (pt == 25) {
            for (i = 0; i < nd; i++) {
                if (dests[i].dest == -1) {
                    int result = do_move(selected_point, dests[i].dv,
                                         turn, FALSE);
                    play_sound(BEAROFF_NOTE, BEAROFF_DUR);
                    selected_point = -1;

                    if (off[turn] >= NUM_CHECKERS) {
                        gamestate = GS_GAMEOVER;
                        play_sound(WIN_NOTE, WIN_DUR);
                        return;
                    }
                    if (dice_remaining() == 0 || !has_any_move(turn))
                        end_turn();
                    return;
                }
            }
        }

        /* Check if clicked a valid board destination */
        if (pt >= 0 && pt <= 23) {
            for (i = 0; i < nd; i++) {
                if (dests[i].dest == pt) {
                    int result = do_move(selected_point, dests[i].dv,
                                         turn, FALSE);
                    if (result == 1) play_sound(HIT_NOTE, HIT_DUR);
                    else if (result == 2) play_sound(BEAROFF_NOTE, BEAROFF_DUR);
                    selected_point = -1;

                    if (off[turn] >= NUM_CHECKERS) {
                        gamestate = GS_GAMEOVER;
                        play_sound(WIN_NOTE, WIN_DUR);
                        return;
                    }
                    if (dice_remaining() == 0 || !has_any_move(turn))
                        end_turn();
                    return;
                }
            }
        }

        /* Not a single-die dest - try combo move */
        if (pt >= 0 && pt <= 23) {
            if (try_combo_move(selected_point, pt, FALSE)) {
                selected_point = -1;
                if (off[turn] >= NUM_CHECKERS) {
                    gamestate = GS_GAMEOVER;
                    play_sound(WIN_NOTE, WIN_DUR);
                    return;
                }
                if (dice_remaining() == 0 || !has_any_move(turn))
                    end_turn();
                return;
            }
        }
        /* try combo bear off */
        if (pt == 25) {
            if (try_combo_move(selected_point, -1, FALSE)) {
                selected_point = -1;
                if (off[turn] >= NUM_CHECKERS) {
                    gamestate = GS_GAMEOVER;
                    play_sound(WIN_NOTE, WIN_DUR);
                    return;
                }
                if (dice_remaining() == 0 || !has_any_move(turn))
                    end_turn();
                return;
            }
        }

        /* Not a valid dest - try to select a different piece */
        selected_point = -1;
    }

    /* Try to select a checker */
    if (pt >= 0 && pt <= 23) {
        if (turn == P1 && board[pt] > 0) {
            /* Check if this piece has any valid move */
            destinfo dests[MAX_DESTS];
            int nd = get_valid_dests(pt, FALSE, dests);
            if (nd > 0)
                selected_point = pt;
            else
                selected_point = -1;
        } else if (turn == P2 && board[pt] < 0) {
            destinfo dests[MAX_DESTS];
            int nd = get_valid_dests(pt, FALSE, dests);
            if (nd > 0)
                selected_point = pt;
            else
                selected_point = -1;
        } else {
            selected_point = -1;
        }
    }
}

/*******************************************************************************

Menu setup

*******************************************************************************/

ami_menuptr newmenuitem(int onoff, int oneof, int isbar, int id,
                        const char *face)
{
    ami_menuptr mp;

    mp = malloc(sizeof(ami_menurec));
    mp->next = NULL;
    mp->branch = NULL;
    mp->onoff = onoff;
    mp->oneof = oneof;
    mp->bar = isbar;
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
    ami_menuptr mode_menu, mode_items;
    ami_menuptr help_menu, help_items;

    /* Game menu */
    game_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Game");
    appendmenu(&menu, game_menu);
    game_items = NULL;
    appendmenu(&game_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_NEW, "New Game"));
    appendmenu(&game_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_UNDO, "Undo Move"));
    appendmenu(&game_items,
        newmenuitem(FALSE, FALSE, TRUE, MENU_DOUBLE, "Double"));
    appendmenu(&game_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_EXIT, "Exit"));
    game_menu->branch = game_items;

    /* Mode menu */
    mode_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Mode");
    appendmenu(&menu, mode_menu);
    mode_items = NULL;
    appendmenu(&mode_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_PVP, "Player vs Player"));
    appendmenu(&mode_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_PVC_W,
                    "Play White vs Computer"));
    appendmenu(&mode_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_PVC_B,
                    "Play Black vs Computer"));
    mode_menu->branch = mode_items;

    /* Help menu */
    help_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Help");
    appendmenu(&menu, help_menu);
    help_items = NULL;
    appendmenu(&help_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_ABOUT, "About Backgammon"));
    help_menu->branch = help_items;

    ami_menu(stdout, menu);
}

/*******************************************************************************

Main program

*******************************************************************************/

int main(void)
{
    ami_evtrec er;

    srand(time(NULL));

    ami_title(stdout, "Backgammon");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_autohold(FALSE); /* override automatic hold */
    ami_buffer(stdout, FALSE);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_bold(stdout, TRUE);
    ami_binvis(stdout);

    /* restore saved window size/position if available */
    {
        FILE *pf;
        pf = fopen("backgammon.pos", "r");
        if (pf) {
            int pw, ph, ppx, ppy;
            char line[256];

            /* skip header line */
            if (fgets(line, sizeof(line), pf) &&
                fgets(line, sizeof(line), pf) && /* blank line */
                fgets(line, sizeof(line), pf)) {
                if (sscanf(line, "width: %d height: %d position x: %d position y: %d",
                           &pw, &ph, &ppx, &ppy) >= 2) {
                    if (pw > 100 && ph > 100) {
                        ami_setsizg(stdout, pw, ph);
                    }
                }
            }
            fclose(pf);
        }
    }

    ami_opensynthout(AMI_SYNTH_OUT);
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_WOODBLOCK);
    ami_starttimeout();
    ami_loadwave(WAVE_SLIDE, "graph_games/slide.wav");
    ami_loadwave(WAVE_DICE, "graph_games/dice.wav");
    ami_openwaveout(AMI_WAVE_OUT);
    sound_enabled = TRUE;

    gamemode = MODE_PVC_W;
    score[P1] = 0;
    score[P2] = 0;
    setup_menu();
    init_board();
    calc_metrics();
    draw_all();

    do {
        ami_event(stdin, &er);

        if (er.etype == ami_etredraw || er.etype == ami_etresize) {
            draw_all();
        }

        else if (er.etype == ami_etmoumovg) {
            mousex = er.moupxg;
            mousey = er.moupyg;
        }

        else if (er.etype == ami_etmouba && er.amoubn == 1 && !animating) {
            handle_click();
            draw_all();

            /* If it became computer's turn after roll state */
            if (gamestate == GS_ROLL && is_computer_turn() && !ai_pending) {
                ami_timer(stdout, TIMER_AI, 300, FALSE);
                ai_pending = TRUE;
            }
        }

        else if (er.etype == ami_ettim && er.timnum == TIMER_ANIM) {
            if (animating) {
                anim_frame++;
                if (anim_frame >= ANIM_FRAMES)
                    finish_animation();
                draw_all();
            }
        }

        else if (er.etype == ami_ettim && er.timnum == TIMER_MSG) {
            overlay_active = FALSE;
            draw_all();
        }

        else if (er.etype == ami_ettim && er.timnum == TIMER_AI) {
            ai_pending = FALSE;

            if (gamestate == GS_MOVE && !has_any_move(turn)) {
                /* No moves available after roll - just end turn */
                end_turn();
                draw_all();
                if (gamestate == GS_ROLL && is_computer_turn()) {
                    ami_timer(stdout, TIMER_AI, 300, FALSE);
                    ai_pending = TRUE;
                }
            } else if (gamestate == GS_ROLL && is_computer_turn()) {
                /* Computer rolls */
                roll_dice();
                if (sound_enabled) ami_playwave(AMI_WAVE_OUT, 0, WAVE_DICE);
                gamestate = GS_MOVE;
                draw_all();

                if (has_any_move(turn)) {
                    ami_timer(stdout, TIMER_AI, 400, FALSE);
                    ai_pending = TRUE;
                } else {
                    end_turn();
                    draw_all();
                    if (gamestate == GS_ROLL && is_computer_turn()) {
                        ami_timer(stdout, TIMER_AI, 300, FALSE);
                        ai_pending = TRUE;
                    }
                }
            } else if (gamestate == GS_MOVE && is_computer_turn()) {
                /* Computer makes a move */
                ai_move();
                draw_all();

                /* ai_move schedules another timer if more moves remain,
                   or calls end_turn. If turn ended, check if still
                   computer's turn. */
                if (gamestate == GS_ROLL && is_computer_turn() && !ai_pending) {
                    ami_timer(stdout, TIMER_AI, 500, FALSE);
                    ai_pending = TRUE;
                }
            }
        }

        else if (er.etype == ami_etmenus) {
            switch (er.menuid) {
                case MENU_NEW:
                    init_board();
                    draw_all();
                    if (is_computer_turn()) {
                        ami_timer(stdout, TIMER_AI, 300, FALSE);
                        ai_pending = TRUE;
                    }
                    break;
                case MENU_UNDO:
                    if (undo_top > 0 && gamestate == GS_MOVE &&
                        !is_computer_turn() && !animating) {
                        undo_pop();
                        gamestate = GS_MOVE;
                        draw_all();
                    }
                    break;
                case MENU_DOUBLE:
                    /* player offers to double - only before rolling */
                    if (gamestate != GS_ROLL) {
                        show_message("You can only double before rolling.");
                        draw_all();
                    } else if (is_computer_turn()) {
                        /* not your turn */
                    } else if (cube_owner != -1 && cube_owner != turn) {
                        show_message(
                                  "Only the player who was last doubled can redouble.");
                        draw_all();
                    } else {
                        int opp = 1 - turn;
                        char msg[120];
                        if (gamemode == MODE_PVP) {
                            /* PvP: show offer to opponent */
                            sprintf(msg, "%s doubles to %d!\nDoes %s accept?",
                                    turn == P1 ? "White" : "Black",
                                    cube_value * 2,
                                    opp == P1 ? "White" : "Black");
                            show_message(msg);
                            /* In PvP we auto-accept for now (no decline UI) */
                            cube_value *= 2;
                            cube_owner = opp;
                            draw_all();
                        } else {
                            /* vs computer: AI decides */
                            int ai_accepts;
                            /* AI uses simple heuristic: accept if pip count
                               difference is not too large */
                            int my_pip = pip_count(opp);
                            int their_pip = pip_count(turn);
                            ai_accepts = (cube_value < 16) &&
                                         (my_pip <= their_pip * 3 / 2);
                            if (ai_accepts) {
                                cube_value *= 2;
                                cube_owner = opp;
                                sprintf(msg, "Computer accepts the double.\nCube is now at %d.",
                                        cube_value);
                                show_message(msg);
                                draw_all();
                            } else {
                                sprintf(msg, "Computer declines the double.\n%s wins %d point%s!",
                                        turn == P1 ? "White" : "Black",
                                        cube_value,
                                        cube_value > 1 ? "s" : "");
                                score[turn] += cube_value;
                                show_message(msg);
                                init_board();
                                draw_all();
                            }
                        }
                    }
                    break;
                case MENU_EXIT:
                    er.etype = ami_etterm;
                    break;
                case MENU_PVP:
                    gamemode = MODE_PVP;
                    init_board();
                    draw_all();
                    break;
                case MENU_PVC_W:
                    gamemode = MODE_PVC_W;
                    init_board();
                    draw_all();
                    if (is_computer_turn()) {
                        ami_timer(stdout, TIMER_AI, 300, FALSE);
                        ai_pending = TRUE;
                    }
                    break;
                case MENU_PVC_B:
                    gamemode = MODE_PVC_B;
                    init_board();
                    draw_all();
                    if (is_computer_turn()) {
                        ami_timer(stdout, TIMER_AI, 300, FALSE);
                        ai_pending = TRUE;
                    }
                    break;
                case MENU_ABOUT:
                    show_message("Backgammon for Amitk - (C) 2026 S. A. Franco");
                    draw_all();
                    break;
            }
        }

    } while (er.etype != ami_etterm);

    /* save window position/size */
    {
        FILE *pf;
        int wx, wy;

        ami_getsizg(stdout, &wx, &wy);
        pf = fopen("backgammon.pos", "w");
        if (pf) {
            fprintf(pf, "Backgammon position\n\n");
            fprintf(pf, "width: %d height: %d position x: 0 position y: 0\n",
                    wx, wy);
            fclose(pf);
        }
    }

    ami_closewaveout(AMI_WAVE_OUT);
    ami_closesynthout(AMI_SYNTH_OUT);
    return 0;
}
