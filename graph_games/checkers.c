/*******************************************************************************
*                                                                              *
*                               CHECKERS GAME                                  *
*                                                                              *
*                       COPYRIGHT (C) 2026 S. A. FRANCO                        *
*                                                                              *
* A graphical checkers game with resizable window and menus. Implements full   *
* checkers rules including mandatory jumps, multi-jumps, and king promotion.   *
* Supports human vs human and human vs computer play.                          *
* Computer uses minimax search with alpha-beta pruning.                        *
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

/* sound defines */
#define MOVE_NOTE    (PA_NOTE_E+PA_OCTAVE_5)
#define CAPTURE_NOTE (PA_NOTE_G+PA_OCTAVE_5)
#define KING_NOTE    (PA_NOTE_C+PA_OCTAVE_6)
#define WIN_NOTE     (PA_NOTE_C+PA_OCTAVE_4)
#define MOVE_DUR     150
#define KING_DUR     300
#define WIN_DUR      800

/*******************************************************************************

Piece and board definitions

*******************************************************************************/

#define EMPTY   0
#define MAN     1
#define KING    2

#define RED_SIDE    0
#define BLACK_SIDE  1

#define MAKEPIECE(color, type) (((color) << 2) | (type))
#define PTYPE(p)  ((p) & 3)
#define PCOLOR(p) (((p) >> 2) & 1)

#define RM MAKEPIECE(RED_SIDE, MAN)
#define RK MAKEPIECE(RED_SIDE, KING)
#define BM MAKEPIECE(BLACK_SIDE, MAN)
#define BK MAKEPIECE(BLACK_SIDE, KING)

/* menu ids */
#define MENU_NEW     100
#define MENU_EXIT    101
#define MENU_ABOUT   102
#define MENU_PVP     103
#define MENU_PVC_R   104
#define MENU_PVC_B   105

/* game mode */
#define MODE_PVP     0   /* player vs player */
#define MODE_PVC_R   1   /* player is red, computer is black */
#define MODE_PVC_B   2   /* player is black, computer is red */

/* AI search depth */
#define AI_DEPTH 6

/* AI timer id */
#define TIMER_AI 2

#define MAXMOVES 128

/*******************************************************************************

Global state

*******************************************************************************/

int board[8][8];
int turn;           /* RED_SIDE or BLACK_SIDE */
int selected;       /* a piece is currently selected */
int selr, selc;     /* selected piece row, col */
int gamestate;      /* 0=playing, 1=game over */
int gamemode;       /* MODE_PVP, MODE_PVC_R, MODE_PVC_B */
int ai_pending;     /* AI move is pending (timer set) */
int must_jump;      /* jumps are available and mandatory */
int multi_jump;     /* in the middle of a multi-jump sequence */
int multi_jr, multi_jc; /* piece doing the multi-jump */

typedef struct { int fr, fc, tr, tc, is_jump; } checkmove;
checkmove legalmoves[MAXMOVES];
int nmoves;

int mousex, mousey;
int sound_enabled;

/*******************************************************************************

Board initialization

*******************************************************************************/

void init_board(void)
{
    int r, c;

    memset(board, 0, sizeof(board));

    /* Red at bottom (rows 0-2), black at top (rows 5-7) */
    /* Pieces only on dark squares: where (r+c) is even */
    for (r = 0; r < 3; r++)
        for (c = 0; c < 8; c++)
            if ((r + c) % 2 == 0)
                board[r][c] = RM;

    for (r = 5; r < 8; r++)
        for (c = 0; c < 8; c++)
            if ((r + c) % 2 == 0)
                board[r][c] = BM;

    turn = RED_SIDE;
    selected = FALSE;
    gamestate = 0;
    ai_pending = FALSE;
    must_jump = FALSE;
    multi_jump = FALSE;
    nmoves = 0;
}

/*******************************************************************************

Board coordinate helpers

*******************************************************************************/

int sqsize(void)
{
    int bw, bh, sz;

    bw = ami_maxxg(stdout);
    bh = ami_maxyg(stdout) - ami_maxyg(stdout) / 12;
    sz = bw < bh ? bw : bh;
    return sz / 8;
}

int boardx0(void)
{
    return (ami_maxxg(stdout) - sqsize() * 8) / 2;
}

int boardy0(void)
{
    return (ami_maxyg(stdout) - ami_maxyg(stdout) / 12 - sqsize() * 8) / 2;
}

void sq2px(int r, int c, int *px, int *py)
{
    int sz = sqsize();
    *px = boardx0() + c * sz;
    *py = boardy0() + (7 - r) * sz;
}

int px2sq(int px, int py, int *r, int *c)
{
    int sz = sqsize();
    int bx = boardx0();
    int by = boardy0();

    if (px < bx || py < by) return FALSE;
    *c = (px - bx) / sz;
    *r = 7 - (py - by) / sz;
    if (*c < 0 || *c > 7 || *r < 0 || *r > 7) return FALSE;
    return TRUE;
}

int onboard(int r, int c)
{
    return r >= 0 && r < 8 && c >= 0 && c < 8;
}

/*******************************************************************************

Move generation

*******************************************************************************/

/* Check if a piece at (r,c) can jump in direction (dr,dc) */
int can_jump(int r, int c, int dr, int dc)
{
    int mr, mc, lr, lc, p;

    mr = r + dr; mc = c + dc;       /* middle square */
    lr = r + 2*dr; lc = c + 2*dc;   /* landing square */

    if (!onboard(lr, lc)) return FALSE;
    if (board[lr][lc] != EMPTY) return FALSE;

    p = board[mr][mc];
    if (p == EMPTY) return FALSE;
    if (PCOLOR(p) == PCOLOR(board[r][c])) return FALSE;

    return TRUE;
}

/* Check if any piece of given color has a jump available */
int any_jump_available(int color)
{
    int r, c, p, type;
    int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    int i;

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++) {
            p = board[r][c];
            if (p == EMPTY || PCOLOR(p) != color) continue;
            type = PTYPE(p);
            for (i = 0; i < 4; i++) {
                /* men can only move forward */
                if (type == MAN) {
                    if (color == RED_SIDE && dirs[i][0] < 0) continue;
                    if (color == BLACK_SIDE && dirs[i][0] > 0) continue;
                }
                if (can_jump(r, c, dirs[i][0], dirs[i][1]))
                    return TRUE;
            }
        }
    return FALSE;
}

/* Check if a specific piece at (r,c) has a jump available */
int piece_has_jump(int r, int c)
{
    int p, type, color, i;
    int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};

    p = board[r][c];
    if (p == EMPTY) return FALSE;
    color = PCOLOR(p);
    type = PTYPE(p);

    for (i = 0; i < 4; i++) {
        if (type == MAN) {
            if (color == RED_SIDE && dirs[i][0] < 0) continue;
            if (color == BLACK_SIDE && dirs[i][0] > 0) continue;
        }
        if (can_jump(r, c, dirs[i][0], dirs[i][1]))
            return TRUE;
    }
    return FALSE;
}

/* Generate moves for a specific piece at (r,c).
   If jumps_only is set, only generate jump moves. */
void gen_piece_moves(int r, int c, int jumps_only)
{
    int p, color, type, i, nr, nc;
    int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};

    nmoves = 0;
    p = board[r][c];
    if (p == EMPTY) return;
    color = PCOLOR(p);
    type = PTYPE(p);

    /* Generate jump moves */
    for (i = 0; i < 4; i++) {
        if (type == MAN) {
            if (color == RED_SIDE && dirs[i][0] < 0) continue;
            if (color == BLACK_SIDE && dirs[i][0] > 0) continue;
        }
        if (can_jump(r, c, dirs[i][0], dirs[i][1])) {
            nr = r + 2 * dirs[i][0];
            nc = c + 2 * dirs[i][1];
            if (nmoves < MAXMOVES) {
                legalmoves[nmoves].fr = r;
                legalmoves[nmoves].fc = c;
                legalmoves[nmoves].tr = nr;
                legalmoves[nmoves].tc = nc;
                legalmoves[nmoves].is_jump = TRUE;
                nmoves++;
            }
        }
    }

    if (jumps_only) return;
    if (nmoves > 0) return; /* if jumps exist, simple moves not allowed */

    /* Generate simple (non-jump) moves */
    for (i = 0; i < 4; i++) {
        if (type == MAN) {
            if (color == RED_SIDE && dirs[i][0] < 0) continue;
            if (color == BLACK_SIDE && dirs[i][0] > 0) continue;
        }
        nr = r + dirs[i][0];
        nc = c + dirs[i][1];
        if (onboard(nr, nc) && board[nr][nc] == EMPTY) {
            if (nmoves < MAXMOVES) {
                legalmoves[nmoves].fr = r;
                legalmoves[nmoves].fc = c;
                legalmoves[nmoves].tr = nr;
                legalmoves[nmoves].tc = nc;
                legalmoves[nmoves].is_jump = FALSE;
                nmoves++;
            }
        }
    }
}

/* Generate all legal moves for a given color */
int gen_all_moves(int color, checkmove *moves)
{
    int r, c, count = 0, savenmoves, i;
    int jumps_exist;

    jumps_exist = any_jump_available(color);
    savenmoves = nmoves;

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (board[r][c] != EMPTY && PCOLOR(board[r][c]) == color) {
                gen_piece_moves(r, c, jumps_exist);
                for (i = 0; i < nmoves; i++)
                    if (count < MAXMOVES) moves[count++] = legalmoves[i];
            }

    nmoves = savenmoves;
    return count;
}

int has_legal_moves(int color)
{
    checkmove moves[MAXMOVES];
    return gen_all_moves(color, moves) > 0;
}

/* Count pieces of a given color */
int count_pieces(int color)
{
    int r, c, count = 0;

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (board[r][c] != EMPTY && PCOLOR(board[r][c]) == color)
                count++;
    return count;
}

/*******************************************************************************

Execute a move on the board

*******************************************************************************/

void play_sound(int note, int dur)
{
    ami_noteon(PA_SYNTH_OUT, 0, 1, note, INT_MAX);
    ami_noteoff(PA_SYNTH_OUT, ami_curtimeout()+dur, 1, note, INT_MAX);
}

/* Execute a move. Returns TRUE if the piece can continue jumping. */
int do_move(int fr, int fc, int tr, int tc)
{
    int p = board[fr][fc];
    int color = PCOLOR(p);
    int is_jump = (abs(tr - fr) == 2);
    int promoted = FALSE;
    int can_continue = FALSE;

    if (is_jump) {
        /* remove jumped piece */
        int mr = (fr + tr) / 2;
        int mc = (fc + tc) / 2;
        board[mr][mc] = EMPTY;
    }

    board[tr][tc] = p;
    board[fr][fc] = EMPTY;

    /* check for promotion */
    if (PTYPE(board[tr][tc]) == MAN) {
        if (color == RED_SIDE && tr == 7) {
            board[tr][tc] = MAKEPIECE(RED_SIDE, KING);
            promoted = TRUE;
        }
        if (color == BLACK_SIDE && tr == 0) {
            board[tr][tc] = MAKEPIECE(BLACK_SIDE, KING);
            promoted = TRUE;
        }
    }

    /* Check if multi-jump is possible (only if this was a jump and
       the piece was not just promoted) */
    if (is_jump && !promoted && piece_has_jump(tr, tc))
        can_continue = TRUE;

    if (!can_continue) {
        /* Turn is over */
        turn = 1 - turn;

        /* Check for game over */
        if (!has_legal_moves(turn) || count_pieces(turn) == 0)
            gamestate = 1;
    }

    /* Play sounds */
    if (sound_enabled) {
        if (gamestate == 1)
            play_sound(WIN_NOTE, WIN_DUR);
        else if (promoted)
            play_sound(KING_NOTE, KING_DUR);
        else if (is_jump)
            play_sound(CAPTURE_NOTE, MOVE_DUR);
        else
            play_sound(MOVE_NOTE, MOVE_DUR);
    }

    return can_continue;
}

/*******************************************************************************

Computer AI - minimax with alpha-beta pruning

*******************************************************************************/

/* Piece-square table for positional evaluation (center and advancement bonus) */
static const int man_pst[8][8] = {
    { 0,  0,  0,  0,  0,  0,  0,  0},
    { 1,  0,  1,  0,  1,  0,  1,  0},
    { 0,  2,  0,  2,  0,  2,  0,  2},
    { 3,  0,  4,  0,  4,  0,  3,  0},
    { 0,  4,  0,  5,  0,  5,  0,  4},
    { 5,  0,  6,  0,  6,  0,  5,  0},
    { 0,  6,  0,  7,  0,  7,  0,  6},
    { 8,  0,  8,  0,  8,  0,  8,  0}
};

static const int king_pst[8][8] = {
    { 0,  0,  0,  0,  0,  0,  0,  0},
    { 0,  2,  0,  2,  0,  2,  0,  2},
    { 0,  2,  0,  3,  0,  3,  0,  2},
    { 0,  2,  0,  4,  0,  4,  0,  2},
    { 0,  2,  0,  4,  0,  4,  0,  2},
    { 0,  2,  0,  3,  0,  3,  0,  2},
    { 0,  2,  0,  2,  0,  2,  0,  2},
    { 0,  0,  0,  0,  0,  0,  0,  0}
};

int evaluate(void)
{
    int r, c, score = 0, p, type, color;

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++) {
            p = board[r][c];
            if (p == EMPTY) continue;
            type = PTYPE(p);
            color = PCOLOR(p);

            if (type == MAN) {
                if (color == RED_SIDE)
                    score += 100 + man_pst[r][c];
                else
                    score -= 100 + man_pst[7-r][c];
            } else { /* KING */
                if (color == RED_SIDE)
                    score += 160 + king_pst[r][c];
                else
                    score -= 160 + king_pst[r][c];
            }
        }

    return score;
}

/* Save/restore board state for search */
typedef struct {
    int board[8][8];
    int turn;
    int gamestate;
} boardstate;

void save_state(boardstate *s)
{
    memcpy(s->board, board, sizeof(board));
    s->turn = turn;
    s->gamestate = gamestate;
}

void restore_state(boardstate *s)
{
    memcpy(board, s->board, sizeof(board));
    turn = s->turn;
    gamestate = s->gamestate;
}

/* Recursively execute a move, handling multi-jumps for AI.
   After the move, if a multi-jump is possible, pick the best
   continuation greedily (most captures). */
void do_move_full(int fr, int fc, int tr, int tc)
{
    int can_continue;

    can_continue = do_move(fr, fc, tr, tc);

    /* If multi-jump, do a greedy continuation */
    while (can_continue) {
        int i, best_i;
        int savenmoves;

        savenmoves = nmoves;
        gen_piece_moves(tr, tc, TRUE); /* jumps only */
        if (nmoves == 0) {
            nmoves = savenmoves;
            /* end the turn */
            turn = 1 - turn;
            if (!has_legal_moves(turn) || count_pieces(turn) == 0)
                gamestate = 1;
            break;
        }

        /* pick first available jump */
        best_i = 0;
        fr = legalmoves[best_i].fr;
        fc = legalmoves[best_i].fc;
        tr = legalmoves[best_i].tr;
        tc = legalmoves[best_i].tc;
        nmoves = savenmoves;

        can_continue = do_move(fr, fc, tr, tc);
    }
}

int minimax(int depth, int alpha, int beta, int maximizing)
{
    checkmove moves[MAXMOVES];
    int nmov, i, val;
    boardstate saved;
    int color = maximizing ? RED_SIDE : BLACK_SIDE;

    if (depth == 0 || gamestate != 0)
        return evaluate();

    nmov = gen_all_moves(color, moves);

    if (nmov == 0) {
        /* no moves = loss */
        return maximizing ? -100000 + (AI_DEPTH - depth) :
                             100000 - (AI_DEPTH - depth);
    }

    if (maximizing) {
        val = -200000;
        for (i = 0; i < nmov; i++) {
            save_state(&saved);
            do_move_full(moves[i].fr, moves[i].fc,
                         moves[i].tr, moves[i].tc);
            int score = minimax(depth - 1, alpha, beta, FALSE);
            restore_state(&saved);
            if (score > val) val = score;
            if (val > alpha) alpha = val;
            if (beta <= alpha) break;
        }
    } else {
        val = 200000;
        for (i = 0; i < nmov; i++) {
            save_state(&saved);
            do_move_full(moves[i].fr, moves[i].fc,
                         moves[i].tr, moves[i].tc);
            int score = minimax(depth - 1, alpha, beta, TRUE);
            restore_state(&saved);
            if (score < val) val = score;
            if (val < beta) beta = val;
            if (beta <= alpha) break;
        }
    }

    return val;
}

void ai_move(void)
{
    checkmove moves[MAXMOVES];
    int nmov, i, bestidx;
    boardstate saved;
    int ai_color = turn;
    int maximizing = (ai_color == RED_SIDE);

    nmov = gen_all_moves(ai_color, moves);
    if (nmov == 0) return;

    sound_enabled = FALSE; /* silence during search */
    bestidx = 0;

    if (maximizing) {
        int bestval = -200000;
        for (i = 0; i < nmov; i++) {
            save_state(&saved);
            do_move_full(moves[i].fr, moves[i].fc,
                         moves[i].tr, moves[i].tc);
            int val = minimax(AI_DEPTH - 1, -200000, 200000, FALSE);
            restore_state(&saved);
            if (val > bestval) { bestval = val; bestidx = i; }
        }
    } else {
        int bestval = 200000;
        for (i = 0; i < nmov; i++) {
            save_state(&saved);
            do_move_full(moves[i].fr, moves[i].fc,
                         moves[i].tr, moves[i].tc);
            int val = minimax(AI_DEPTH - 1, -200000, 200000, TRUE);
            restore_state(&saved);
            if (val < bestval) { bestval = val; bestidx = i; }
        }
    }
    sound_enabled = TRUE; /* re-enable for the actual move */

    /* Execute the chosen move (with multi-jump handling) */
    do_move_full(moves[bestidx].fr, moves[bestidx].fc,
                 moves[bestidx].tr, moves[bestidx].tc);
    multi_jump = FALSE;
}

/* Check if it is the computer's turn */
int is_computer_turn(void)
{
    if (gamemode == MODE_PVP) return FALSE;
    if (gamemode == MODE_PVC_R && turn == BLACK_SIDE) return TRUE;
    if (gamemode == MODE_PVC_B && turn == RED_SIDE) return TRUE;
    return FALSE;
}

/*******************************************************************************

Drawing

*******************************************************************************/

#define LIGHT_R (240 * (INT_MAX/255))
#define LIGHT_G (217 * (INT_MAX/255))
#define LIGHT_B (181 * (INT_MAX/255))
#define DARK_R  (181 * (INT_MAX/255))
#define DARK_G  (136 * (INT_MAX/255))
#define DARK_B  (99  * (INT_MAX/255))
#define SEL_R   (100 * (INT_MAX/255))
#define SEL_G   (180 * (INT_MAX/255))
#define SEL_B   (100 * (INT_MAX/255))
#define MOVE_R  (200 * (INT_MAX/255))
#define MOVE_G  (200 * (INT_MAX/255))
#define MOVE_B  (80  * (INT_MAX/255))

#define RED_PIECE_R  (200 * (INT_MAX/255))
#define RED_PIECE_G  (30  * (INT_MAX/255))
#define RED_PIECE_B  (30  * (INT_MAX/255))
#define BLK_PIECE_R  (40  * (INT_MAX/255))
#define BLK_PIECE_G  (40  * (INT_MAX/255))
#define BLK_PIECE_B  (40  * (INT_MAX/255))
#define OUTLINE_R    (20  * (INT_MAX/255))
#define OUTLINE_G    (20  * (INT_MAX/255))
#define OUTLINE_B    (20  * (INT_MAX/255))
#define CROWN_R      (255 * (INT_MAX/255))
#define CROWN_G      (215 * (INT_MAX/255))
#define CROWN_B      (0   * (INT_MAX/255))

void draw_piece(int r, int c, int px, int py, int sz)
{
    int p = board[r][c];
    int color = PCOLOR(p);
    int type = PTYPE(p);
    int cx, cy, rx, ry, margin;

    margin = sz / 8;
    cx = px + sz / 2;
    cy = py + sz / 2;
    rx = sz / 2 - margin;
    ry = sz / 2 - margin;

    /* Draw dark outline circle (slightly larger) */
    ami_fcolorg(stdout, OUTLINE_R, OUTLINE_G, OUTLINE_B);
    ami_fellipse(stdout, cx - rx, cy - ry, cx + rx, cy + ry);

    /* Draw piece body (slightly smaller for outline effect) */
    rx -= sz / 16;
    ry -= sz / 16;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;

    if (color == RED_SIDE)
        ami_fcolorg(stdout, RED_PIECE_R, RED_PIECE_G, RED_PIECE_B);
    else
        ami_fcolorg(stdout, BLK_PIECE_R, BLK_PIECE_G, BLK_PIECE_B);
    ami_fellipse(stdout, cx - rx, cy - ry, cx + rx, cy + ry);

    /* King marking: smaller inner circle in gold */
    if (type == KING) {
        int krx = rx / 2;
        int kry = ry / 2;

        if (krx < 1) krx = 1;
        if (kry < 1) kry = 1;
        ami_fcolorg(stdout, CROWN_R, CROWN_G, CROWN_B);
        ami_fellipse(stdout, cx - krx, cy - kry, cx + krx, cy + kry);
    }
}

void draw_square(int r, int c)
{
    int px, py, sz, isdark, i;

    sz = sqsize();
    sq2px(r, c, &px, &py);

    isdark = (r + c) % 2 == 0;

    if (selected && r == selr && c == selc) {
        ami_fcolorg(stdout, SEL_R, SEL_G, SEL_B);
    } else {
        int ismove = FALSE;
        if (selected) {
            for (i = 0; i < nmoves; i++)
                if (legalmoves[i].tr == r && legalmoves[i].tc == c) {
                    ismove = TRUE; break;
                }
        }
        if (ismove)
            ami_fcolorg(stdout, MOVE_R, MOVE_G, MOVE_B);
        else if (isdark)
            ami_fcolorg(stdout, DARK_R, DARK_G, DARK_B);
        else
            ami_fcolorg(stdout, LIGHT_R, LIGHT_G, LIGHT_B);
    }
    ami_frect(stdout, px, py, px + sz - 1, py + sz - 1);

    if (board[r][c] != EMPTY)
        draw_piece(r, c, px, py, sz);
}

void draw_board(void)
{
    int r, c;

    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            draw_square(r, c);

    /* rank and file labels */
    {
        int sz = sqsize();
        int bx = boardx0();
        int by = boardy0();
        char label[2] = {0, 0};

        ami_fontsiz(stdout, sz / 4);
        ami_fcolorg(stdout, 80 * (INT_MAX/255), 80 * (INT_MAX/255),
                    80 * (INT_MAX/255));

        for (c = 0; c < 8; c++) {
            label[0] = 'a' + c;
            ami_cursorg(stdout, bx + c * sz + sz / 2 -
                        ami_strsiz(stdout, label) / 2,
                        by + 8 * sz + 2);
            printf("%s", label);
        }
        for (r = 0; r < 8; r++) {
            label[0] = '1' + r;
            ami_cursorg(stdout, bx - sz / 3,
                        by + (7 - r) * sz + sz / 2 - sz / 8);
            printf("%s", label);
        }
    }
}

void draw_status(void)
{
    int sy, fsz;
    char msg[120];

    fsz = ami_maxyg(stdout) / 25;
    if (fsz < 14) fsz = 14;
    sy = ami_maxyg(stdout) - fsz * 2;

    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, 1, sy, ami_maxxg(stdout), ami_maxyg(stdout));

    ami_fontsiz(stdout, fsz);
    ami_fcolorg(stdout, 40 * (INT_MAX/255), 40 * (INT_MAX/255),
                40 * (INT_MAX/255));

    if (gamestate == 1) {
        sprintf(msg, "Game over! %s wins.",
                turn == RED_SIDE ? "Black" : "Red");
    } else if (is_computer_turn()) {
        strcpy(msg, "Computer is thinking...");
    } else if (multi_jump) {
        sprintf(msg, "%s must continue jumping!",
                turn == RED_SIDE ? "Red" : "Black");
    } else if (must_jump) {
        sprintf(msg, "%s to move - jump required!",
                turn == RED_SIDE ? "Red" : "Black");
    } else {
        sprintf(msg, "%s to move.",
                turn == RED_SIDE ? "Red" : "Black");
    }

    ami_cursorg(stdout, ami_maxxg(stdout) / 2 -
                ami_strsiz(stdout, msg) / 2, sy + 4);
    printf("%s", msg);
}

void draw_all(void)
{
    draw_board();
    draw_status();
}

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
    ami_menuptr mode_menu, mode_items;
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

    /* Mode menu */
    mode_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Mode");
    appendmenu(&menu, mode_menu);
    mode_items = NULL;
    appendmenu(&mode_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_PVP, "Player vs Player"));
    appendmenu(&mode_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_PVC_R, "Play Red vs Computer"));
    appendmenu(&mode_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_PVC_B, "Play Black vs Computer"));
    mode_menu->branch = mode_items;

    /* Help menu */
    help_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Help");
    appendmenu(&menu, help_menu);
    help_items = NULL;
    appendmenu(&help_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_ABOUT, "About Checkers"));
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

    ami_title(stdout, "Checkers");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_buffer(stdout, FALSE);
    ami_font(stdout, PA_FONT_SIGN);
    ami_bold(stdout, TRUE);
    ami_binvis(stdout);

    ami_opensynthout(PA_SYNTH_OUT);
    ami_instchange(PA_SYNTH_OUT, 0, 1, PA_INST_ACOUSTIC_GRAND);
    ami_starttimeout();
    sound_enabled = TRUE;

    gamemode = MODE_PVC_R;
    setup_menu();
    init_board();
    draw_all();

    /* if computer goes first, schedule AI move */
    if (is_computer_turn()) {
        ami_timer(stdout, TIMER_AI, 100, FALSE);
        ai_pending = TRUE;
        draw_all();
    }

    do {
        ami_event(stdin, &er);

        if (er.etype == ami_etredraw || er.etype == ami_etresize) {
            draw_all();
        }

        else if (er.etype == ami_etmoumovg) {
            mousex = er.moupxg;
            mousey = er.moupyg;
        }

        else if (er.etype == ami_etmouba && er.amoubn == 1 &&
                 gamestate == 0 && !is_computer_turn()) {
            int mr, mc;
            if (px2sq(mousex, mousey, &mr, &mc)) {

                /* During multi-jump, only the jumping piece can be used */
                if (multi_jump) {
                    if (selected) {
                        int i, found = FALSE;
                        for (i = 0; i < nmoves; i++) {
                            if (legalmoves[i].tr == mr &&
                                legalmoves[i].tc == mc) {
                                found = TRUE; break;
                            }
                        }
                        if (found) {
                            int can_continue;
                            can_continue = do_move(selr, selc, mr, mc);
                            if (can_continue) {
                                /* continue multi-jump with same piece */
                                selr = mr; selc = mc;
                                gen_piece_moves(mr, mc, TRUE);
                                multi_jump = TRUE;
                            } else {
                                selected = FALSE;
                                multi_jump = FALSE;
                                nmoves = 0;
                            }
                            must_jump = any_jump_available(turn);
                            draw_all();
                            /* schedule AI if turn changed */
                            if (!multi_jump && gamestate == 0 &&
                                is_computer_turn()) {
                                ami_timer(stdout, TIMER_AI, 100, FALSE);
                                ai_pending = TRUE;
                                draw_all();
                            }
                        }
                        /* clicking elsewhere during multi-jump: ignore */
                    }
                } else if (selected) {
                    int i, found = FALSE;
                    for (i = 0; i < nmoves; i++) {
                        if (legalmoves[i].tr == mr &&
                            legalmoves[i].tc == mc) {
                            found = TRUE; break;
                        }
                    }
                    if (found) {
                        int can_continue;
                        can_continue = do_move(selr, selc, mr, mc);
                        if (can_continue) {
                            /* start multi-jump */
                            selected = TRUE;
                            selr = mr; selc = mc;
                            gen_piece_moves(mr, mc, TRUE);
                            multi_jump = TRUE;
                        } else {
                            selected = FALSE;
                            multi_jump = FALSE;
                            nmoves = 0;
                        }
                        must_jump = any_jump_available(turn);
                        draw_all();
                        /* schedule AI response */
                        if (!multi_jump && gamestate == 0 &&
                            is_computer_turn()) {
                            ami_timer(stdout, TIMER_AI, 100, FALSE);
                            ai_pending = TRUE;
                            draw_all();
                        }
                    } else {
                        /* deselect and try to select new piece */
                        selected = FALSE;
                        nmoves = 0;
                        if (board[mr][mc] != EMPTY &&
                            PCOLOR(board[mr][mc]) == turn) {
                            must_jump = any_jump_available(turn);
                            /* if jumps are required, only allow selecting
                               pieces that can jump */
                            if (must_jump && !piece_has_jump(mr, mc)) {
                                /* can't select this piece */
                            } else {
                                gen_piece_moves(mr, mc, must_jump);
                                if (nmoves > 0) {
                                    selected = TRUE;
                                    selr = mr; selc = mc;
                                }
                            }
                        }
                        draw_all();
                    }
                } else {
                    /* nothing selected yet */
                    if (board[mr][mc] != EMPTY &&
                        PCOLOR(board[mr][mc]) == turn) {
                        must_jump = any_jump_available(turn);
                        if (must_jump && !piece_has_jump(mr, mc)) {
                            /* can't select this piece - must jump */
                        } else {
                            gen_piece_moves(mr, mc, must_jump);
                            if (nmoves > 0) {
                                selected = TRUE;
                                selr = mr; selc = mc;
                                draw_all();
                            }
                        }
                    }
                }
            }
        }

        else if (er.etype == ami_ettim && er.timnum == TIMER_AI) {
            if (gamestate == 0 && is_computer_turn()) {
                ai_move();
                ai_pending = FALSE;
                draw_all();
            }
        }

        else if (er.etype == ami_etmenus) {
            switch (er.menuid) {
                case MENU_NEW:
                    init_board();
                    selected = FALSE; nmoves = 0;
                    multi_jump = FALSE;
                    must_jump = FALSE;
                    draw_all();
                    if (is_computer_turn()) {
                        ami_timer(stdout, TIMER_AI, 100, FALSE);
                        ai_pending = TRUE;
                        draw_all();
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
                case MENU_PVC_R:
                    gamemode = MODE_PVC_R;
                    init_board();
                    draw_all();
                    break;
                case MENU_PVC_B:
                    gamemode = MODE_PVC_B;
                    init_board();
                    draw_all();
                    if (is_computer_turn()) {
                        ami_timer(stdout, TIMER_AI, 100, FALSE);
                        ai_pending = TRUE;
                        draw_all();
                    }
                    break;
                case MENU_ABOUT:
                    ami_alert("About Checkers",
                              "Checkers for Amitk\n"
                              "Human vs human or vs computer\n"
                              "Copyright (C) 2026 S. A. Franco");
                    draw_all();
                    break;
            }
        }

    } while (er.etype != ami_etterm);

    ami_closesynthout(PA_SYNTH_OUT);
    return 0;
}
