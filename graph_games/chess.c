/*******************************************************************************
*                                                                              *
*                                 CHESS GAME                                   *
*                                                                              *
*                       COPYRIGHT (C) 2026 S. A. FRANCO                        *
*                                                                              *
* A graphical chess game with resizable window and menus. Implements full       *
* chess rules including castling, en passant, and pawn promotion.              *
* Two player (human vs human) on a single screen.                              *
*                                                                              *
*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <localdefs.h>
#include <graphics.h>

/*******************************************************************************

Piece and board definitions

*******************************************************************************/

/* piece types */
#define EMPTY   0
#define PAWN    1
#define KNIGHT  2
#define BISHOP  3
#define ROOK    4
#define QUEEN   5
#define KING    6

/* colors */
#define WHITE_SIDE  0
#define BLACK_SIDE  1

/* encode piece: color in bit 3, type in bits 0-2 */
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
#define MENU_NEW    100
#define MENU_EXIT   101
#define MENU_ABOUT  102

/* maximum legal moves per position (generous) */
#define MAXMOVES 256

/*******************************************************************************

Global state

*******************************************************************************/

int board[8][8];    /* board[row][col], row 0 = rank 1 (white's back rank) */
int turn;           /* WHITE_SIDE or BLACK_SIDE */
int selected;       /* a square is selected */
int selr, selc;     /* selected square row, col */
int gamestate;      /* 0=playing, 1=checkmate, 2=stalemate */

/* castling rights */
int wk_castle;      /* white can castle kingside */
int wq_castle;      /* white can castle queenside */
int bk_castle;      /* black can castle kingside */
int bq_castle;      /* black can castle queenside */

/* en passant target: if last move was a double pawn push, this is the
   column (0-7) of the pawn, otherwise -1 */
int ep_col;

/* legal move list for selected piece */
typedef struct { int fr, fc, tr, tc; } chessmove;
chessmove legalmoves[MAXMOVES];
int nmoves;

/* piece letters for display */
static const char *piece_char[] = {
    " ", "P", "N", "B", "R", "Q", "K"
};

/*******************************************************************************

Board initialization

*******************************************************************************/

void init_board(void)
{
    int r, c;

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            board[r][c] = EMPTY;

    /* white pieces - rank 1 */
    board[0][0] = WR; board[0][1] = WN; board[0][2] = WB; board[0][3] = WQ;
    board[0][4] = WK; board[0][5] = WB; board[0][6] = WN; board[0][7] = WR;
    for (c = 0; c < 8; c++) board[1][c] = WP;

    /* black pieces - rank 8 */
    board[7][0] = BR; board[7][1] = BN; board[7][2] = BB; board[7][3] = BQ;
    board[7][4] = BK; board[7][5] = BB; board[7][6] = BN; board[7][7] = BR;
    for (c = 0; c < 8; c++) board[6][c] = BP;

    turn = WHITE_SIDE;
    selected = FALSE;
    gamestate = 0;
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
    bh = ami_maxyg(stdout) - 40; /* leave room for status bar */
    sz = bw < bh ? bw : bh;
    return sz / 8;
}

/* top-left corner of the board in pixels */
int boardx0(void)
{
    return (ami_maxxg(stdout) - sqsize() * 8) / 2;
}

int boardy0(void)
{
    return (ami_maxyg(stdout) - 40 - sqsize() * 8) / 2;
}

/* convert board row/col to pixel coordinates (top-left of square) */
/* row 0 is white's back rank, displayed at bottom */
void sq2px(int r, int c, int *px, int *py)
{
    int sz = sqsize();
    *px = boardx0() + c * sz;
    *py = boardy0() + (7 - r) * sz; /* flip so white is at bottom */
}

/* convert pixel to board row/col, returns FALSE if off board */
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

/* check if square is on the board */
int onboard(int r, int c)
{
    return r >= 0 && r < 8 && c >= 0 && c < 8;
}

/* find king position */
void findking(int color, int *kr, int *kc)
{
    int r, c;
    int king = MAKEPIECE(color, KING);

    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (board[r][c] == king) {
                *kr = r; *kc = c; return;
            }
    *kr = -1; *kc = -1;
}

/* is the given square attacked by 'attacker' color? */
int is_attacked(int r, int c, int attacker)
{
    int dr, dc, i, nr, nc, p;

    /* pawn attacks */
    if (attacker == WHITE_SIDE) {
        if (onboard(r-1, c-1) && board[r-1][c-1] == WP) return TRUE;
        if (onboard(r-1, c+1) && board[r-1][c+1] == WP) return TRUE;
    } else {
        if (onboard(r+1, c-1) && board[r+1][c-1] == BP) return TRUE;
        if (onboard(r+1, c+1) && board[r+1][c+1] == BP) return TRUE;
    }

    /* knight attacks */
    {
        int kd[][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        int ki;
        for (ki = 0; ki < 8; ki++) {
            nr = r + kd[ki][0]; nc = c + kd[ki][1];
            if (onboard(nr, nc) && board[nr][nc] == MAKEPIECE(attacker, KNIGHT))
                return TRUE;
        }
    }

    /* king attacks (for adjacent squares) */
    for (dr = -1; dr <= 1; dr++)
        for (dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            nr = r + dr; nc = c + dc;
            if (onboard(nr, nc) && board[nr][nc] == MAKEPIECE(attacker, KING))
                return TRUE;
        }

    /* sliding pieces: bishop/queen diagonals */
    {
        int dd[][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
        int di;
        for (di = 0; di < 4; di++) {
            for (i = 1; i < 8; i++) {
                nr = r + dd[di][0]*i; nc = c + dd[di][1]*i;
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
    }

    /* sliding pieces: rook/queen straights */
    {
        int sd[][2] = {{-1,0},{1,0},{0,-1},{0,1}};
        int si;
        for (si = 0; si < 4; si++) {
            for (i = 1; i < 8; i++) {
                nr = r + sd[si][0]*i; nc = c + sd[si][1]*i;
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

/* try making a move and see if it leaves us in check */
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

/* add move if legal (doesn't leave own king in check) */
void addmove(int fr, int fc, int tr, int tc)
{
    if (nmoves < MAXMOVES && move_legal(fr, fc, tr, tc)) {
        legalmoves[nmoves].fr = fr; legalmoves[nmoves].fc = fc;
        legalmoves[nmoves].tr = tr; legalmoves[nmoves].tc = tc;
        nmoves++;
    }
}

/* generate all legal moves for piece at (r,c) */
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

            /* forward one */
            nr = r + dir;
            if (onboard(nr, c) && board[nr][c] == EMPTY) {
                addmove(r, c, nr, c);
                /* forward two from start */
                if (r == start && board[r + 2*dir][c] == EMPTY)
                    addmove(r, c, r + 2*dir, c);
            }
            /* captures */
            for (dc = -1; dc <= 1; dc += 2) {
                nc = c + dc;
                if (onboard(nr, nc)) {
                    if (board[nr][nc] != EMPTY && PCOLOR(board[nr][nc]) != color)
                        addmove(r, c, nr, nc);
                    /* en passant */
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
            int ki;
            for (ki = 0; ki < 8; ki++) {
                nr = r + kd[ki][0]; nc = c + kd[ki][1];
                if (onboard(nr, nc) &&
                    (board[nr][nc] == EMPTY || PCOLOR(board[nr][nc]) != color))
                    addmove(r, c, nr, nc);
            }
            break;
        }

        case BISHOP: {
            int dd[][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
            int di;
            for (di = 0; di < 4; di++)
                for (i = 1; i < 8; i++) {
                    nr = r + dd[di][0]*i; nc = c + dd[di][1]*i;
                    if (!onboard(nr, nc)) break;
                    if (board[nr][nc] == EMPTY) { addmove(r, c, nr, nc); }
                    else {
                        if (PCOLOR(board[nr][nc]) != color)
                            addmove(r, c, nr, nc);
                        break;
                    }
                }
            break;
        }

        case ROOK: {
            int sd[][2] = {{-1,0},{1,0},{0,-1},{0,1}};
            int si;
            for (si = 0; si < 4; si++)
                for (i = 1; i < 8; i++) {
                    nr = r + sd[si][0]*i; nc = c + sd[si][1]*i;
                    if (!onboard(nr, nc)) break;
                    if (board[nr][nc] == EMPTY) { addmove(r, c, nr, nc); }
                    else {
                        if (PCOLOR(board[nr][nc]) != color)
                            addmove(r, c, nr, nc);
                        break;
                    }
                }
            break;
        }

        case QUEEN: {
            int qd[][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},
                           {0,1},{1,-1},{1,0},{1,1}};
            int qi;
            for (qi = 0; qi < 8; qi++)
                for (i = 1; i < 8; i++) {
                    nr = r + qd[qi][0]*i; nc = c + qd[qi][1]*i;
                    if (!onboard(nr, nc)) break;
                    if (board[nr][nc] == EMPTY) { addmove(r, c, nr, nc); }
                    else {
                        if (PCOLOR(board[nr][nc]) != color)
                            addmove(r, c, nr, nc);
                        break;
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
            /* castling */
            if (color == WHITE_SIDE && r == 0 && c == 4 && !in_check(color)) {
                /* kingside */
                if (wk_castle && board[0][5] == EMPTY && board[0][6] == EMPTY &&
                    board[0][7] == WR &&
                    !is_attacked(0, 5, BLACK_SIDE) &&
                    !is_attacked(0, 6, BLACK_SIDE))
                    addmove(0, 4, 0, 6);
                /* queenside */
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

/* check if current player has any legal moves at all */
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

void do_move(int fr, int fc, int tr, int tc)
{
    int p = board[fr][fc];
    int color = PCOLOR(p);
    int type = PTYPE(p);

    /* en passant capture */
    if (type == PAWN && tc != fc && board[tr][tc] == EMPTY)
        board[fr][tc] = EMPTY; /* remove captured pawn */

    /* castling - move the rook */
    if (type == KING && fc == 4 && tc == 6) { /* kingside */
        board[fr][5] = board[fr][7]; board[fr][7] = EMPTY;
    }
    if (type == KING && fc == 4 && tc == 2) { /* queenside */
        board[fr][3] = board[fr][0]; board[fr][0] = EMPTY;
    }

    /* update en passant column */
    if (type == PAWN && (tr - fr == 2 || fr - tr == 2))
        ep_col = fc;
    else
        ep_col = -1;

    /* update castling rights */
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
    /* if a rook is captured, remove castling rights */
    if (tr == 0 && tc == 0) wq_castle = FALSE;
    if (tr == 0 && tc == 7) wk_castle = FALSE;
    if (tr == 7 && tc == 0) bq_castle = FALSE;
    if (tr == 7 && tc == 7) bk_castle = FALSE;

    /* make the move */
    board[tr][tc] = p;
    board[fr][fc] = EMPTY;

    /* pawn promotion (auto-queen) */
    if (type == PAWN && (tr == 7 || tr == 0))
        board[tr][tc] = MAKEPIECE(color, QUEEN);

    /* switch turn */
    turn = 1 - turn;

    /* check for checkmate or stalemate */
    if (!has_legal_moves(turn)) {
        if (in_check(turn))
            gamestate = 1; /* checkmate */
        else
            gamestate = 2; /* stalemate */
    } else {
        gamestate = 0;
    }
}

/*******************************************************************************

Drawing

*******************************************************************************/

/* board square colors */
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

void draw_square(int r, int c)
{
    int px, py, sz, islight, i;

    sz = sqsize();
    sq2px(r, c, &px, &py);

    islight = (r + c) % 2 == 1;

    /* check if this square is selected */
    if (selected && r == selr && c == selc) {
        ami_fcolorg(stdout, SEL_R, SEL_G, SEL_B);
    } else {
        /* check if this square is a legal move target */
        int ismove = FALSE;
        if (selected) {
            for (i = 0; i < nmoves; i++)
                if (legalmoves[i].tr == r && legalmoves[i].tc == c) {
                    ismove = TRUE; break;
                }
        }
        if (ismove) {
            ami_fcolorg(stdout, MOVE_R, MOVE_G, MOVE_B);
        } else if (islight) {
            ami_fcolorg(stdout, LIGHT_R, LIGHT_G, LIGHT_B);
        } else {
            ami_fcolorg(stdout, DARK_R, DARK_G, DARK_B);
        }
    }
    ami_frect(stdout, px, py, px + sz - 1, py + sz - 1);

    /* draw piece if present */
    if (board[r][c] != EMPTY) {
        int ptype = PTYPE(board[r][c]);
        int pcolor = PCOLOR(board[r][c]);

        ami_fontsiz(stdout, sz * 2 / 3);
        if (pcolor == WHITE_SIDE)
            ami_fcolorg(stdout, INT_MAX, INT_MAX, INT_MAX);
        else
            ami_fcolorg(stdout, 0, 0, 0);

        /* center the piece character in the square */
        {
            const char *ch = piece_char[ptype];
            int tw = ami_strsiz(stdout, ch);
            int th = sz * 2 / 3; /* approximate font height */
            int tx = px + (sz - tw) / 2;
            int ty = py + (sz - th) / 2;
            ami_cursorg(stdout, tx, ty);
            printf("%s", ch);
        }

        /* draw outline for white pieces so they're visible on light squares */
        if (pcolor == WHITE_SIDE) {
            const char *ch = piece_char[ptype];
            int tw = ami_strsiz(stdout, ch);
            int th = sz * 2 / 3;
            int tx = px + (sz - tw) / 2;
            int ty = py + (sz - th) / 2;
            ami_fcolorg(stdout, 0, 0, 0);
            ami_cursorg(stdout, tx - 1, ty);
            printf("%s", ch);
            ami_cursorg(stdout, tx + 1, ty);
            printf("%s", ch);
            /* redraw white on top */
            ami_fcolorg(stdout, INT_MAX, INT_MAX, INT_MAX);
            ami_cursorg(stdout, tx, ty);
            printf("%s", ch);
        }
    }
}

void draw_board(void)
{
    int r, c;

    /* clear background */
    ami_bcolor(stdout, ami_white);
    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));

    /* draw all squares */
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            draw_square(r, c);

    /* draw rank and file labels */
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
    int sy;
    char msg[80];

    sy = ami_maxyg(stdout) - 30;

    /* clear status area */
    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, 1, sy, ami_maxxg(stdout), ami_maxyg(stdout));

    /* draw status text */
    ami_fontsiz(stdout, 16);
    ami_fcolorg(stdout, 40 * (INT_MAX/255), 40 * (INT_MAX/255),
                40 * (INT_MAX/255));

    if (gamestate == 1) {
        sprintf(msg, "Checkmate! %s wins.",
                turn == WHITE_SIDE ? "Black" : "White");
    } else if (gamestate == 2) {
        strcpy(msg, "Stalemate - draw.");
    } else if (in_check(turn)) {
        sprintf(msg, "%s to move - CHECK!",
                turn == WHITE_SIDE ? "White" : "Black");
    } else {
        sprintf(msg, "%s to move.",
                turn == WHITE_SIDE ? "White" : "Black");
    }

    ami_cursorg(stdout, ami_maxxg(stdout) / 2 -
                ami_strsiz(stdout, msg) / 2, sy + 5);
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
    ami_menuptr help_menu, help_items;

    /* Game menu */
    game_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Game");
    appendmenu(&menu, game_menu);

    game_items = NULL;
    appendmenu(&game_items, newmenuitem(FALSE, FALSE, TRUE, MENU_NEW, "New Game"));
    appendmenu(&game_items, newmenuitem(FALSE, FALSE, FALSE, MENU_EXIT, "Exit"));
    game_menu->branch = game_items;

    /* Help menu */
    help_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Help");
    appendmenu(&menu, help_menu);

    help_items = NULL;
    appendmenu(&help_items, newmenuitem(FALSE, FALSE, FALSE, MENU_ABOUT, "About Chess"));
    help_menu->branch = help_items;

    ami_menu(stdout, menu);
}

/*******************************************************************************

Main program

*******************************************************************************/

int main(void)
{
    ami_evtrec er;
    int r, c;

    /* window setup */
    ami_title(stdout, "Chess");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_buffer(stdout, FALSE); /* unbuffered for resize support */
    ami_font(stdout, PA_FONT_SIGN);
    ami_bold(stdout, TRUE);
    ami_binvis(stdout);

    setup_menu();
    init_board();
    draw_all();

    /* main event loop */
    do {
        ami_event(stdin, &er);

        if (er.etype == ami_etredraw) {
            draw_all();
        }

        else if (er.etype == ami_etresize) {
            draw_all();
        }

        else if (er.etype == ami_etmouba && er.amoubn == 1 && gamestate == 0) {
            /* left mouse button press */
            int mr, mc;
            if (px2sq(er.moupxg, er.moupyg, &mr, &mc)) {
                if (selected) {
                    /* check if clicking a legal move target */
                    int i, found = FALSE;
                    for (i = 0; i < nmoves; i++) {
                        if (legalmoves[i].tr == mr &&
                            legalmoves[i].tc == mc) {
                            found = TRUE; break;
                        }
                    }
                    if (found) {
                        /* execute the move */
                        do_move(selr, selc, mr, mc);
                        selected = FALSE;
                        nmoves = 0;
                        draw_all();
                    } else {
                        /* deselect or select a new piece */
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
                    /* no selection yet, try to select a piece */
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

        else if (er.etype == ami_etmenus) {
            switch (er.menuid) {
                case MENU_NEW:
                    init_board();
                    draw_all();
                    break;
                case MENU_EXIT:
                    goto done;
                case MENU_ABOUT:
                    ami_alert("About Chess",
                              "Chess for Amitk\nTwo player chess game\n"
                              "Copyright (C) 2026 S. A. Franco");
                    draw_all();
                    break;
            }
        }

    } while (er.etype != ami_etterm);

    done:
    return 0;
}
