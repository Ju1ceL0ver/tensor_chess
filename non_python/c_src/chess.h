#ifndef CHESS_H
#define CHESS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t Bitboard;

typedef enum {
    COLOR_WHITE = 0,
    COLOR_BLACK = 1
} Color;

typedef enum {
    PIECE_PAWN = 0,
    PIECE_KNIGHT,
    PIECE_BISHOP,
    PIECE_ROOK,
    PIECE_QUEEN,
    PIECE_KING,
    PIECE_NONE
} Piece;

typedef enum {
    SQUARE_A1 = 0, SQUARE_B1, SQUARE_C1, SQUARE_D1, SQUARE_E1, SQUARE_F1, SQUARE_G1, SQUARE_H1,
    SQUARE_A2, SQUARE_B2, SQUARE_C2, SQUARE_D2, SQUARE_E2, SQUARE_F2, SQUARE_G2, SQUARE_H2,
    SQUARE_A3, SQUARE_B3, SQUARE_C3, SQUARE_D3, SQUARE_E3, SQUARE_F3, SQUARE_G3, SQUARE_H3,
    SQUARE_A4, SQUARE_B4, SQUARE_C4, SQUARE_D4, SQUARE_E4, SQUARE_F4, SQUARE_G4, SQUARE_H4,
    SQUARE_A5, SQUARE_B5, SQUARE_C5, SQUARE_D5, SQUARE_E5, SQUARE_F5, SQUARE_G5, SQUARE_H5,
    SQUARE_A6, SQUARE_B6, SQUARE_C6, SQUARE_D6, SQUARE_E6, SQUARE_F6, SQUARE_G6, SQUARE_H6,
    SQUARE_A7, SQUARE_B7, SQUARE_C7, SQUARE_D7, SQUARE_E7, SQUARE_F7, SQUARE_G7, SQUARE_H7,
    SQUARE_A8, SQUARE_B8, SQUARE_C8, SQUARE_D8, SQUARE_E8, SQUARE_F8, SQUARE_G8, SQUARE_H8,
    SQUARE_NONE = 64
} Square;

static inline int square_rank(Square sq) {
    return sq >> 3;
}

static inline int square_file(Square sq) {
    return sq & 7;
}

typedef enum {
    CASTLE_WHITE_KING  = 1u << 0,
    CASTLE_WHITE_QUEEN = 1u << 1,
    CASTLE_BLACK_KING  = 1u << 2,
    CASTLE_BLACK_QUEEN = 1u << 3
} Castling;

typedef enum {
    MOVE_FLAG_NONE        = 0,
    MOVE_FLAG_CAPTURE     = 1u << 0,
    MOVE_FLAG_PROMOTION   = 1u << 1,
    MOVE_FLAG_EN_PASSANT  = 1u << 2,
    MOVE_FLAG_DOUBLE_PAWN = 1u << 3,
    MOVE_FLAG_CASTLE      = 1u << 4
} MoveFlag;

typedef struct {
    uint8_t from;
    uint8_t to;
    uint8_t promotion;
    uint8_t flags;
} Move;

typedef struct {
    Bitboard pieces[2][6];
    Bitboard occupancy[2];
    Bitboard all;
    uint8_t castling_rights;
    int8_t en_passant_square;
    Color side_to_move;
    uint16_t ply;
    uint16_t halfmove_clock;
    uint16_t fullmove_number;
} Position;

void chess_init_tables(void);
void position_set_start(Position *pos);
int position_from_fen(Position *pos, const char *fen);
void position_make_move(Position *pos, const Move *move);
void position_unmake_move(Position *pos, const Move *move, const Position *previous);
size_t generate_legal_moves(const Position *pos, Move *out_moves, size_t max_moves);
int is_square_attacked(const Position *pos, Square sq, Color by);
int in_check(const Position *pos, Color side);
int is_checkmate(const Position *pos);
int is_stalemate(const Position *pos);
int position_to_fen(const Position *pos, char *buffer, size_t buffer_len);
int position_has_legal_moves(const Position *pos);
int position_is_insufficient_material(const Position *pos);

#ifdef __cplusplus
}
#endif

#endif
