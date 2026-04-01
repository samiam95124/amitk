/*******************************************************************************
*                                                                              *
*                                 CHESS GAME                                   *
*                                                                              *
*                       COPYRIGHT (C) 2026 S. A. FRANCO                        *
*                                                                              *
* A graphical chess game with resizable window and menus. Implements full       *
* chess rules including castling, en passant, and pawn promotion.              *
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
#define CHECK_NOTE   (PA_NOTE_C+PA_OCTAVE_6)
#define MATE_NOTE    (PA_NOTE_C+PA_OCTAVE_4)
#define MOVE_DUR     150
#define CHECK_DUR    300
#define MATE_DUR     800

/*******************************************************************************

Piece and board definitions

*******************************************************************************/

#define EMPTY   0
#define PAWN    1
#define KNIGHT  2
#define BISHOP  3
#define ROOK    4
#define QUEEN   5
#define KING    6

#define WHITE_SIDE  0
#define BLACK_SIDE  1

#define MAKEPIECE(color, type) (((color) << 3) | (type))
#define PTYPE(p)  ((p) & 7)
#define PCOLOR(p) (((p) >> 3) & 1)

#define WP MAKEPIECE(WHITE_SIDE, PAWN)
#define WN MAKEPIECE(WHITE_SIDE, KNIGHT)
#define WB MAKEPIECE(WHITE_SIDE, BISHOP)
#define WR MAKEPIECE(WHITE_SIDE, ROOK)
#define WQ MAKEPIECE(WHITE_SIDE, QUEEN)
#define WK MAKEPIECE(WHITE_SIDE, KING)
#define BP MAKEPIECE(BLACK_SIDE, PAWN)
#define BN MAKEPIECE(BLACK_SIDE, KNIGHT)
#define BB MAKEPIECE(BLACK_SIDE, BISHOP)
#define BR MAKEPIECE(BLACK_SIDE, ROOK)
#define BQ MAKEPIECE(BLACK_SIDE, QUEEN)
#define BK MAKEPIECE(BLACK_SIDE, KING)

/* menu ids */
#define MENU_NEW     100
#define MENU_EXIT    101
#define MENU_ABOUT   102
#define MENU_PVP     103
#define MENU_PVC_W   104
#define MENU_PVC_B   105

/* game mode */
#define MODE_PVP     0   /* player vs player */
#define MODE_PVC_W   1   /* player is white, computer is black */
#define MODE_PVC_B   2   /* player is black, computer is white */

/* AI search depth */
#define AI_DEPTH 4

/* AI timer id */
#define TIMER_AI 2

#define MAXMOVES 256

/*******************************************************************************

Global state

*******************************************************************************/

int board[8][8];
int turn;
int selected;
int selr, selc;
int gamestate;    /* 0=playing, 1=checkmate, 2=stalemate */
int gamemode;     /* MODE_PVP, MODE_PVC_W, MODE_PVC_B */
int ai_pending;   /* AI move is pending (timer set) */

int wk_castle, wq_castle;
int bk_castle, bq_castle;
int ep_col;

typedef struct { int fr, fc, tr, tc; } chessmove;
chessmove legalmoves[MAXMOVES];
int nmoves;

int mousex, mousey;

/* picture ids: white pieces light 1-6, white dark 7-12,
   black light 13-18, black dark 19-24 */
int pic_id(int pcolor, int ptype, int light)
{
    return pcolor * 12 + (light ? 0 : 6) + ptype;
}

/*******************************************************************************

Board initialization

*******************************************************************************/

void init_board(void)
{
    int c;

    memset(board, 0, sizeof(board));

    board[0][0] = WR; board[0][1] = WN; board[0][2] = WB; board[0][3] = WQ;
    board[0][4] = WK; board[0][5] = WB; board[0][6] = WN; board[0][7] = WR;
    for (c = 0; c < 8; c++) board[1][c] = WP;

    board[7][0] = BR; board[7][1] = BN; board[7][2] = BB; board[7][3] = BQ;
    board[7][4] = BK; board[7][5] = BB; board[7][6] = BN; board[7][7] = BR;
    for (c = 0; c < 8; c++) board[6][c] = BP;

    turn = WHITE_SIDE;
    selected = FALSE;
    gamestate = 0;
    ai_pending = FALSE;
    wk_castle = TRUE; wq_castle = TRUE;
    bk_castle = TRUE; bq_castle = TRUE;
    ep_col = -1;
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

/*******************************************************************************

Move generation

*******************************************************************************/

int onboard(int r, int c)
{
    return r >= 0 && r < 8 && c >= 0 && c < 8;
}

void findking(int color, int *kr, int *kc)
{
    int r, c;
    int king = MAKEPIECE(color, KING);

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (board[r][c] == king) { *kr = r; *kc = c; return; }
    *kr = -1; *kc = -1;
}

int is_attacked(int r, int c, int attacker)
{
    int dr, dc, i, nr, nc, p;
    int kd[][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    int dd[][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
    int sd[][2] = {{-1,0},{1,0},{0,-1},{0,1}};

    /* pawn attacks */
    if (attacker == WHITE_SIDE) {
        if (onboard(r-1, c-1) && board[r-1][c-1] == WP) return TRUE;
        if (onboard(r-1, c+1) && board[r-1][c+1] == WP) return TRUE;
    } else {
        if (onboard(r+1, c-1) && board[r+1][c-1] == BP) return TRUE;
        if (onboard(r+1, c+1) && board[r+1][c+1] == BP) return TRUE;
    }

    for (i = 0; i < 8; i++) {
        nr = r + kd[i][0]; nc = c + kd[i][1];
        if (onboard(nr, nc) && board[nr][nc] == MAKEPIECE(attacker, KNIGHT))
            return TRUE;
    }

    for (dr = -1; dr <= 1; dr++)
        for (dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            nr = r + dr; nc = c + dc;
            if (onboard(nr, nc) && board[nr][nc] == MAKEPIECE(attacker, KING))
                return TRUE;
        }

    for (i = 0; i < 4; i++) {
        int j;
        for (j = 1; j < 8; j++) {
            nr = r + dd[i][0]*j; nc = c + dd[i][1]*j;
            if (!onboard(nr, nc)) break;
            p = board[nr][nc];
            if (p != EMPTY) {
                if (PCOLOR(p) == attacker &&
                    (PTYPE(p) == BISHOP || PTYPE(p) == QUEEN))
                    return TRUE;
                break;
            }
        }
    }

    for (i = 0; i < 4; i++) {
        int j;
        for (j = 1; j < 8; j++) {
            nr = r + sd[i][0]*j; nc = c + sd[i][1]*j;
            if (!onboard(nr, nc)) break;
            p = board[nr][nc];
            if (p != EMPTY) {
                if (PCOLOR(p) == attacker &&
                    (PTYPE(p) == ROOK || PTYPE(p) == QUEEN))
                    return TRUE;
                break;
            }
        }
    }

    return FALSE;
}

int in_check(int color)
{
    int kr, kc;
    findking(color, &kr, &kc);
    if (kr < 0) return FALSE;
    return is_attacked(kr, kc, 1 - color);
}

int move_legal(int fr, int fc, int tr, int tc)
{
    int captured, result;

    captured = board[tr][tc];
    board[tr][tc] = board[fr][fc];
    board[fr][fc] = EMPTY;
    result = !in_check(PCOLOR(board[tr][tc]));
    board[fr][fc] = board[tr][tc];
    board[tr][tc] = captured;
    return result;
}

void addmove(int fr, int fc, int tr, int tc)
{
    if (nmoves < MAXMOVES && move_legal(fr, fc, tr, tc)) {
        legalmoves[nmoves].fr = fr; legalmoves[nmoves].fc = fc;
        legalmoves[nmoves].tr = tr; legalmoves[nmoves].tc = tc;
        nmoves++;
    }
}

void gen_piece_moves(int r, int c)
{
    int p, color, type, nr, nc, i, dr, dc;

    nmoves = 0;
    p = board[r][c];
    if (p == EMPTY) return;
    color = PCOLOR(p);
    type = PTYPE(p);

    switch (type) {

        case PAWN: {
            int dir = (color == WHITE_SIDE) ? 1 : -1;
            int start = (color == WHITE_SIDE) ? 1 : 6;

            nr = r + dir;
            if (onboard(nr, c) && board[nr][c] == EMPTY) {
                addmove(r, c, nr, c);
                if (r == start && board[r + 2*dir][c] == EMPTY)
                    addmove(r, c, r + 2*dir, c);
            }
            for (dc = -1; dc <= 1; dc += 2) {
                nc = c + dc;
                if (onboard(nr, nc)) {
                    if (board[nr][nc] != EMPTY && PCOLOR(board[nr][nc]) != color)
                        addmove(r, c, nr, nc);
                    if (nr == (color == WHITE_SIDE ? 5 : 2) &&
                        ep_col == nc && board[nr][nc] == EMPTY)
                        addmove(r, c, nr, nc);
                }
            }
            break;
        }

        case KNIGHT: {
            int kd[][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},
                           {1,-2},{1,2},{2,-1},{2,1}};
            for (i = 0; i < 8; i++) {
                nr = r + kd[i][0]; nc = c + kd[i][1];
                if (onboard(nr, nc) &&
                    (board[nr][nc] == EMPTY || PCOLOR(board[nr][nc]) != color))
                    addmove(r, c, nr, nc);
            }
            break;
        }

        case BISHOP: {
            int dd[][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
            for (i = 0; i < 4; i++) {
                int j;
                for (j = 1; j < 8; j++) {
                    nr = r + dd[i][0]*j; nc = c + dd[i][1]*j;
                    if (!onboard(nr, nc)) break;
                    if (board[nr][nc] == EMPTY) addmove(r, c, nr, nc);
                    else {
                        if (PCOLOR(board[nr][nc]) != color)
                            addmove(r, c, nr, nc);
                        break;
                    }
                }
            }
            break;
        }

        case ROOK: {
            int sd[][2] = {{-1,0},{1,0},{0,-1},{0,1}};
            for (i = 0; i < 4; i++) {
                int j;
                for (j = 1; j < 8; j++) {
                    nr = r + sd[i][0]*j; nc = c + sd[i][1]*j;
                    if (!onboard(nr, nc)) break;
                    if (board[nr][nc] == EMPTY) addmove(r, c, nr, nc);
                    else {
                        if (PCOLOR(board[nr][nc]) != color)
                            addmove(r, c, nr, nc);
                        break;
                    }
                }
            }
            break;
        }

        case QUEEN: {
            int qd[][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},
                           {0,1},{1,-1},{1,0},{1,1}};
            for (i = 0; i < 8; i++) {
                int j;
                for (j = 1; j < 8; j++) {
                    nr = r + qd[i][0]*j; nc = c + qd[i][1]*j;
                    if (!onboard(nr, nc)) break;
                    if (board[nr][nc] == EMPTY) addmove(r, c, nr, nc);
                    else {
                        if (PCOLOR(board[nr][nc]) != color)
                            addmove(r, c, nr, nc);
                        break;
                    }
                }
            }
            break;
        }

        case KING: {
            for (dr = -1; dr <= 1; dr++)
                for (dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    nr = r + dr; nc = c + dc;
                    if (onboard(nr, nc) &&
                        (board[nr][nc] == EMPTY ||
                         PCOLOR(board[nr][nc]) != color))
                        addmove(r, c, nr, nc);
                }
            if (color == WHITE_SIDE && r == 0 && c == 4 && !in_check(color)) {
                if (wk_castle && board[0][5] == EMPTY && board[0][6] == EMPTY &&
                    board[0][7] == WR &&
                    !is_attacked(0, 5, BLACK_SIDE) &&
                    !is_attacked(0, 6, BLACK_SIDE))
                    addmove(0, 4, 0, 6);
                if (wq_castle && board[0][3] == EMPTY && board[0][2] == EMPTY &&
                    board[0][1] == EMPTY && board[0][0] == WR &&
                    !is_attacked(0, 3, BLACK_SIDE) &&
                    !is_attacked(0, 2, BLACK_SIDE))
                    addmove(0, 4, 0, 2);
            }
            if (color == BLACK_SIDE && r == 7 && c == 4 && !in_check(color)) {
                if (bk_castle && board[7][5] == EMPTY && board[7][6] == EMPTY &&
                    board[7][7] == BR &&
                    !is_attacked(7, 5, WHITE_SIDE) &&
                    !is_attacked(7, 6, WHITE_SIDE))
                    addmove(7, 4, 7, 6);
                if (bq_castle && board[7][3] == EMPTY && board[7][2] == EMPTY &&
                    board[7][1] == EMPTY && board[7][0] == BR &&
                    !is_attacked(7, 3, WHITE_SIDE) &&
                    !is_attacked(7, 2, WHITE_SIDE))
                    addmove(7, 4, 7, 2);
            }
            break;
        }
    }
}

/* generate all legal moves for a given color into the provided array */
int gen_all_moves(int color, chessmove *moves)
{
    int r, c, count = 0, savenmoves, i;

    savenmoves = nmoves;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (board[r][c] != EMPTY && PCOLOR(board[r][c]) == color) {
                gen_piece_moves(r, c);
                for (i = 0; i < nmoves; i++)
                    if (count < MAXMOVES) moves[count++] = legalmoves[i];
            }
    nmoves = savenmoves;
    return count;
}

int has_legal_moves(int color)
{
    int r, c, savenmoves;

    savenmoves = nmoves;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (board[r][c] != EMPTY && PCOLOR(board[r][c]) == color) {
                gen_piece_moves(r, c);
                if (nmoves > 0) { nmoves = savenmoves; return TRUE; }
            }
    nmoves = savenmoves;
    return FALSE;
}

/*******************************************************************************

Execute a move on the board

*******************************************************************************/

void play_sound(int note, int dur)
{
    ami_noteon(PA_SYNTH_OUT, 0, 1, note, INT_MAX);
    ami_noteoff(PA_SYNTH_OUT, ami_curtimeout()+dur, 1, note, INT_MAX);
}

int sound_enabled; /* only play sounds for real moves, not AI search */

void do_move(int fr, int fc, int tr, int tc)
{
    int p = board[fr][fc];
    int color = PCOLOR(p);
    int type = PTYPE(p);
    int is_capture = (board[tr][tc] != EMPTY);

    /* en passant is also a capture */
    if (type == PAWN && tc != fc && board[tr][tc] == EMPTY)
        is_capture = TRUE;

    if (type == PAWN && tc != fc && board[tr][tc] == EMPTY)
        board[fr][tc] = EMPTY;

    if (type == KING && fc == 4 && tc == 6) {
        board[fr][5] = board[fr][7]; board[fr][7] = EMPTY;
    }
    if (type == KING && fc == 4 && tc == 2) {
        board[fr][3] = board[fr][0]; board[fr][0] = EMPTY;
    }

    if (type == PAWN && (tr - fr == 2 || fr - tr == 2))
        ep_col = fc;
    else
        ep_col = -1;

    if (type == KING) {
        if (color == WHITE_SIDE) { wk_castle = FALSE; wq_castle = FALSE; }
        else { bk_castle = FALSE; bq_castle = FALSE; }
    }
    if (type == ROOK) {
        if (color == WHITE_SIDE) {
            if (fr == 0 && fc == 0) wq_castle = FALSE;
            if (fr == 0 && fc == 7) wk_castle = FALSE;
        } else {
            if (fr == 7 && fc == 0) bq_castle = FALSE;
            if (fr == 7 && fc == 7) bk_castle = FALSE;
        }
    }
    if (tr == 0 && tc == 0) wq_castle = FALSE;
    if (tr == 0 && tc == 7) wk_castle = FALSE;
    if (tr == 7 && tc == 0) bq_castle = FALSE;
    if (tr == 7 && tc == 7) bk_castle = FALSE;

    board[tr][tc] = p;
    board[fr][fc] = EMPTY;

    if (type == PAWN && (tr == 7 || tr == 0))
        board[tr][tc] = MAKEPIECE(color, QUEEN);

    turn = 1 - turn;

    if (!has_legal_moves(turn)) {
        if (in_check(turn))
            gamestate = 1;
        else
            gamestate = 2;
    } else {
        gamestate = 0;
    }

    /* play sound only for real moves */
    if (sound_enabled) {
        if (gamestate == 1)
            play_sound(MATE_NOTE, MATE_DUR);
        else if (in_check(turn))
            play_sound(CHECK_NOTE, CHECK_DUR);
        else if (is_capture)
            play_sound(CAPTURE_NOTE, MOVE_DUR);
        else
            play_sound(MOVE_NOTE, MOVE_DUR);
    }
}

/*******************************************************************************

Computer AI - minimax with alpha-beta pruning

*******************************************************************************/

/* piece values for evaluation */
static const int piece_val[] = { 0, 100, 320, 330, 500, 900, 20000 };

/* piece-square tables for positional evaluation (from white's perspective) */
static const int pawn_pst[8][8] = {
    { 0,  0,  0,  0,  0,  0,  0,  0},
    { 5, 10, 10,-20,-20, 10, 10,  5},
    { 5, -5,-10,  0,  0,-10, -5,  5},
    { 0,  0,  0, 20, 20,  0,  0,  0},
    { 5,  5, 10, 25, 25, 10,  5,  5},
    {10, 10, 20, 30, 30, 20, 10, 10},
    {50, 50, 50, 50, 50, 50, 50, 50},
    { 0,  0,  0,  0,  0,  0,  0,  0}
};

static const int knight_pst[8][8] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50}
};

int evaluate(void)
{
    int r, c, score = 0, p, type, color, pstval;

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++) {
            p = board[r][c];
            if (p == EMPTY) continue;
            type = PTYPE(p);
            color = PCOLOR(p);

            /* material */
            pstval = piece_val[type];

            /* positional bonus from piece-square tables */
            if (type == PAWN) {
                if (color == WHITE_SIDE)
                    pstval += pawn_pst[r][c];
                else
                    pstval += pawn_pst[7-r][c];
            } else if (type == KNIGHT) {
                if (color == WHITE_SIDE)
                    pstval += knight_pst[r][c];
                else
                    pstval += knight_pst[7-r][c];
            }

            if (color == WHITE_SIDE)
                score += pstval;
            else
                score -= pstval;
        }

    return score;
}

/* save/restore board state for search */
typedef struct {
    int board[8][8];
    int turn, ep_col;
    int wk_castle, wq_castle, bk_castle, bq_castle;
    int gamestate;
} boardstate;

void save_state(boardstate *s)
{
    memcpy(s->board, board, sizeof(board));
    s->turn = turn; s->ep_col = ep_col;
    s->wk_castle = wk_castle; s->wq_castle = wq_castle;
    s->bk_castle = bk_castle; s->bq_castle = bq_castle;
    s->gamestate = gamestate;
}

void restore_state(boardstate *s)
{
    memcpy(board, s->board, sizeof(board));
    turn = s->turn; ep_col = s->ep_col;
    wk_castle = s->wk_castle; wq_castle = s->wq_castle;
    bk_castle = s->bk_castle; bq_castle = s->bq_castle;
    gamestate = s->gamestate;
}

int minimax(int depth, int alpha, int beta, int maximizing)
{
    chessmove moves[MAXMOVES];
    int nmov, i, val;
    boardstate saved;
    int color = maximizing ? WHITE_SIDE : BLACK_SIDE;

    if (depth == 0)
        return evaluate();

    nmov = gen_all_moves(color, moves);

    if (nmov == 0) {
        if (in_check(color))
            return maximizing ? -100000 + (AI_DEPTH - depth) :
                                 100000 - (AI_DEPTH - depth);
        return 0; /* stalemate */
    }

    if (maximizing) {
        val = -200000;
        for (i = 0; i < nmov; i++) {
            save_state(&saved);
            do_move(moves[i].fr, moves[i].fc, moves[i].tr, moves[i].tc);
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
            do_move(moves[i].fr, moves[i].fc, moves[i].tr, moves[i].tc);
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
    chessmove moves[MAXMOVES];
    int nmov, i, bestidx;
    boardstate saved;
    int ai_color = turn;
    int maximizing = (ai_color == WHITE_SIDE);

    nmov = gen_all_moves(ai_color, moves);
    if (nmov == 0) return;

    sound_enabled = FALSE; /* silence during search */
    bestidx = 0;
    if (maximizing) {
        int bestval = -200000;
        for (i = 0; i < nmov; i++) {
            save_state(&saved);
            do_move(moves[i].fr, moves[i].fc, moves[i].tr, moves[i].tc);
            int val = minimax(AI_DEPTH - 1, -200000, 200000, FALSE);
            restore_state(&saved);
            if (val > bestval) { bestval = val; bestidx = i; }
        }
    } else {
        int bestval = 200000;
        for (i = 0; i < nmov; i++) {
            save_state(&saved);
            do_move(moves[i].fr, moves[i].fc, moves[i].tr, moves[i].tc);
            int val = minimax(AI_DEPTH - 1, -200000, 200000, TRUE);
            restore_state(&saved);
            if (val < bestval) { bestval = val; bestidx = i; }
        }
    }
    sound_enabled = TRUE; /* re-enable for the actual move */

    do_move(moves[bestidx].fr, moves[bestidx].fc,
            moves[bestidx].tr, moves[bestidx].tc);
}

/* check if it is the computer's turn */
int is_computer_turn(void)
{
    if (gamemode == MODE_PVP) return FALSE;
    if (gamemode == MODE_PVC_W && turn == BLACK_SIDE) return TRUE;
    if (gamemode == MODE_PVC_B && turn == WHITE_SIDE) return TRUE;
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
#define SEL_R   (130 * (INT_MAX/255))
#define SEL_G   (151 * (INT_MAX/255))
#define SEL_B   (105 * (INT_MAX/255))
#define MOVE_R  (170 * (INT_MAX/255))
#define MOVE_G  (162 * (INT_MAX/255))
#define MOVE_B  (58  * (INT_MAX/255))

void load_pieces(void)
{
    static const char *names[] = {
        "", "pawn", "knight", "bishop", "rook", "queen", "king"
    };
    static const char *colors[] = { "w", "b" };
    static const char *sqs[] = { "l", "d" };
    char path[128];
    int color, type, sq;

    for (color = 0; color < 2; color++)
        for (sq = 0; sq < 2; sq++)
            for (type = 1; type <= 6; type++) {
                sprintf(path, "graph_games/chess_pieces/%s%s_%s.bmp",
                        colors[color], names[type], sqs[sq]);
                ami_loadpict(stdout, pic_id(color, type, sq == 0), path);
            }
}

void draw_piece(int r, int c, int px, int py, int sz)
{
    int pcolor = PCOLOR(board[r][c]);
    int ptype = PTYPE(board[r][c]);
    int light = (r + c) % 2 == 1;
    int pid = pic_id(pcolor, ptype, light);
    int margin = sz / 10;

    ami_picture(stdout, pid,
                px + margin, py + margin,
                px + sz - margin - 1, py + sz - margin - 1);
}

void draw_square(int r, int c)
{
    int px, py, sz, islight, i;

    sz = sqsize();
    sq2px(r, c, &px, &py);

    islight = (r + c) % 2 == 1;

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
        else if (islight)
            ami_fcolorg(stdout, LIGHT_R, LIGHT_G, LIGHT_B);
        else
            ami_fcolorg(stdout, DARK_R, DARK_G, DARK_B);
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
        sprintf(msg, "Checkmate! %s wins.",
                turn == WHITE_SIDE ? "Black" : "White");
    } else if (gamestate == 2) {
        strcpy(msg, "Stalemate - draw.");
    } else if (is_computer_turn()) {
        strcpy(msg, "Computer is thinking...");
    } else if (in_check(turn)) {
        sprintf(msg, "%s to move - CHECK!",
                turn == WHITE_SIDE ? "White" : "Black");
    } else {
        sprintf(msg, "%s to move.",
                turn == WHITE_SIDE ? "White" : "Black");
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
        newmenuitem(FALSE, TRUE, FALSE, MENU_PVC_W, "Play White vs Computer"));
    appendmenu(&mode_items,
        newmenuitem(FALSE, TRUE, FALSE, MENU_PVC_B, "Play Black vs Computer"));
    mode_menu->branch = mode_items;

    /* Help menu */
    help_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Help");
    appendmenu(&menu, help_menu);
    help_items = NULL;
    appendmenu(&help_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_ABOUT, "About Chess"));
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

    ami_title(stdout, "Chess");
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

    gamemode = MODE_PVC_W;
    setup_menu();
    load_pieces();
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
                if (selected) {
                    int i, found = FALSE;
                    for (i = 0; i < nmoves; i++) {
                        if (legalmoves[i].tr == mr &&
                            legalmoves[i].tc == mc) {
                            found = TRUE; break;
                        }
                    }
                    if (found) {
                        do_move(selr, selc, mr, mc);
                        selected = FALSE;
                        nmoves = 0;
                        draw_all();
                        /* schedule AI response */
                        if (gamestate == 0 && is_computer_turn()) {
                            ami_timer(stdout, TIMER_AI, 100, FALSE);
                            ai_pending = TRUE;
                            draw_all();
                        }
                    } else {
                        selected = FALSE;
                        nmoves = 0;
                        if (board[mr][mc] != EMPTY &&
                            PCOLOR(board[mr][mc]) == turn) {
                            gen_piece_moves(mr, mc);
                            if (nmoves > 0) {
                                selected = TRUE;
                                selr = mr; selc = mc;
                            }
                        }
                        draw_all();
                    }
                } else {
                    if (board[mr][mc] != EMPTY &&
                        PCOLOR(board[mr][mc]) == turn) {
                        gen_piece_moves(mr, mc);
                        if (nmoves > 0) {
                            selected = TRUE;
                            selr = mr; selc = mc;
                            draw_all();
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
                case MENU_PVC_W:
                    gamemode = MODE_PVC_W;
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
                    ami_alert("About Chess",
                              "Chess for Amitk\nHuman vs human or vs computer\n"
                              "Copyright (C) 2026 S. A. Franco");
                    draw_all();
                    break;
            }
        }

    } while (er.etype != ami_etterm);

    ami_closesynthout(PA_SYNTH_OUT);
    return 0;
}
