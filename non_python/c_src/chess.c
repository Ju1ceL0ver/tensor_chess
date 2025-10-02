#include "chess.h"

#include <string.h>
#include <stdio.h>

#define BIT(sq) (1ULL << (sq))
#define COLOR_FLIP(c) ((Color)(1 - (c)))

static Bitboard knight_attacks[64];
static Bitboard king_attacks[64];
static Bitboard pawn_attacks[2][64];
static Bitboard pawn_pushes[2][64];

static const Bitboard FILE_MASKS[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL,
    0x0404040404040404ULL, 0x0808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL,
    0x4040404040404040ULL, 0x8080808080808080ULL
};

static const Bitboard LIGHT_SQUARES = 0x55AA55AA55AA55AAULL;

static inline Bitboard north(Bitboard b) {
    return b << 8;
}

static inline Bitboard south(Bitboard b) {
    return b >> 8;
}

static inline Bitboard east(Bitboard b) {
    return (b << 1) & ~FILE_MASKS[0];
}

static inline Bitboard west(Bitboard b) {
    return (b >> 1) & ~FILE_MASKS[7];
}

static inline Bitboard north_east(Bitboard b) {
    return (b << 9) & ~FILE_MASKS[0];
}

static inline Bitboard north_west(Bitboard b) {
    return (b << 7) & ~FILE_MASKS[7];
}

static inline Bitboard south_east(Bitboard b) {
    return (b >> 7) & ~FILE_MASKS[0];
}

static inline Bitboard south_west(Bitboard b) {
    return (b >> 9) & ~FILE_MASKS[7];
}

static Bitboard sliding_attacks(int sq, Bitboard occ, const int *deltas, int count) {
    Bitboard attacks = 0;
    int sr = square_rank((Square)sq);
    int sf = square_file((Square)sq);
    for (int i = 0; i < count; ++i) {
        int r = sr;
        int f = sf;
        for (;;) {
            r += deltas[i * 2 + 0];
            f += deltas[i * 2 + 1];
            if (r < 0 || r > 7 || f < 0 || f > 7) {
                break;
            }
            int s = r * 8 + f;
            attacks |= BIT(s);
            if (occ & BIT(s)) {
                break;
            }
        }
    }
    return attacks;
}

static Bitboard rook_attacks(int sq, Bitboard occ) {
    static const int deltas[] = {
        1, 0,
        -1, 0,
        0, 1,
        0, -1
    };
    return sliding_attacks(sq, occ, deltas, 4);
}

static Bitboard bishop_attacks(int sq, Bitboard occ) {
    static const int deltas[] = {
        1, 1,
        1, -1,
        -1, 1,
        -1, -1
    };
    return sliding_attacks(sq, occ, deltas, 4);
}

static Bitboard queen_attacks(int sq, Bitboard occ) {
    return rook_attacks(sq, occ) | bishop_attacks(sq, occ);
}

void chess_init_tables(void) {
    const Bitboard not_file_a = ~FILE_MASKS[0];
    const Bitboard not_file_h = ~FILE_MASKS[7];
    const Bitboard not_file_ab = ~(FILE_MASKS[0] | FILE_MASKS[1]);
    const Bitboard not_file_gh = ~(FILE_MASKS[6] | FILE_MASKS[7]);
    for (int sq = 0; sq < 64; ++sq) {
        Bitboard b = BIT(sq);
        Bitboard attacks = 0;
        attacks |= (b << 17) & not_file_a;
        attacks |= (b << 15) & not_file_h;
        attacks |= (b << 10) & not_file_ab;
        attacks |= (b << 6)  & not_file_gh;
        attacks |= (b >> 17) & not_file_h;
        attacks |= (b >> 15) & not_file_a;
        attacks |= (b >> 10) & not_file_gh;
        attacks |= (b >> 6)  & not_file_ab;
        knight_attacks[sq] = attacks;

        attacks = 0;
        attacks |= north(b);
        attacks |= south(b);
        attacks |= east(b);
        attacks |= west(b);
        attacks |= north_east(b);
        attacks |= north_west(b);
        attacks |= south_east(b);
        attacks |= south_west(b);
        king_attacks[sq] = attacks;

        pawn_attacks[COLOR_WHITE][sq] = north_west(b) | north_east(b);
        pawn_attacks[COLOR_BLACK][sq] = south_west(b) | south_east(b);
        pawn_pushes[COLOR_WHITE][sq] = north(b);
        pawn_pushes[COLOR_BLACK][sq] = south(b);
    }
}

static void clear_position(Position *pos) {
    memset(pos, 0, sizeof(*pos));
    pos->en_passant_square = -1;
    pos->fullmove_number = 1;
}

void position_set_start(Position *pos) {
    clear_position(pos);
    pos->pieces[COLOR_WHITE][PIECE_PAWN] = 0x000000000000FF00ULL;
    pos->pieces[COLOR_WHITE][PIECE_ROOK] = 0x0000000000000081ULL;
    pos->pieces[COLOR_WHITE][PIECE_KNIGHT] = 0x0000000000000042ULL;
    pos->pieces[COLOR_WHITE][PIECE_BISHOP] = 0x0000000000000024ULL;
    pos->pieces[COLOR_WHITE][PIECE_QUEEN] = 0x0000000000000008ULL;
    pos->pieces[COLOR_WHITE][PIECE_KING] = 0x0000000000000010ULL;

    pos->pieces[COLOR_BLACK][PIECE_PAWN] = 0x00FF000000000000ULL;
    pos->pieces[COLOR_BLACK][PIECE_ROOK] = 0x8100000000000000ULL;
    pos->pieces[COLOR_BLACK][PIECE_KNIGHT] = 0x4200000000000000ULL;
    pos->pieces[COLOR_BLACK][PIECE_BISHOP] = 0x2400000000000000ULL;
    pos->pieces[COLOR_BLACK][PIECE_QUEEN] = 0x0800000000000000ULL;
    pos->pieces[COLOR_BLACK][PIECE_KING] = 0x1000000000000000ULL;

    pos->occupancy[COLOR_WHITE] = 0;
    pos->occupancy[COLOR_BLACK] = 0;
    for (int p = 0; p < 6; ++p) {
        pos->occupancy[COLOR_WHITE] |= pos->pieces[COLOR_WHITE][p];
        pos->occupancy[COLOR_BLACK] |= pos->pieces[COLOR_BLACK][p];
    }
    pos->all = pos->occupancy[COLOR_WHITE] | pos->occupancy[COLOR_BLACK];
    pos->castling_rights = CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN | CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN;
    pos->en_passant_square = -1;
    pos->side_to_move = COLOR_WHITE;
    pos->ply = 0;
    pos->halfmove_clock = 0;
    pos->fullmove_number = 1;
}

static Piece piece_on(const Position *pos, Square sq, Color *color_out) {
    Bitboard mask = BIT(sq);
    if (pos->occupancy[COLOR_WHITE] & mask) {
        if (color_out) {
            *color_out = COLOR_WHITE;
        }
        for (int p = 0; p < 6; ++p) {
            if (pos->pieces[COLOR_WHITE][p] & mask) {
                return (Piece)p;
            }
        }
    } else if (pos->occupancy[COLOR_BLACK] & mask) {
        if (color_out) {
            *color_out = COLOR_BLACK;
        }
        for (int p = 0; p < 6; ++p) {
            if (pos->pieces[COLOR_BLACK][p] & mask) {
                return (Piece)p;
            }
        }
    }
    return PIECE_NONE;
}

static char piece_to_char(Piece piece, Color color) {
    char c = '?';
    switch (piece) {
        case PIECE_PAWN:   c = 'p'; break;
        case PIECE_KNIGHT: c = 'n'; break;
        case PIECE_BISHOP: c = 'b'; break;
        case PIECE_ROOK:   c = 'r'; break;
        case PIECE_QUEEN:  c = 'q'; break;
        case PIECE_KING:   c = 'k'; break;
        default:           c = '?'; break;
    }
    if (color == COLOR_WHITE) {
        c = (char)(c - 32);
    }
    return c;
}

int position_from_fen(Position *pos, const char *fen) {
    if (!fen) {
        return -1;
    }
    clear_position(pos);

    const char *cursor = fen;
    int rank = 7;
    int file = 0;
    while (*cursor && *cursor != ' ') {
        char c = *cursor++;
        if (c == '/') {
            if (file != 8) {
                return -1;
            }
            if (rank == 0) {
                return -1;
            }
            --rank;
            file = 0;
            continue;
        }
        if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) {
                return -1;
            }
            continue;
        }
        if (file >= 8) {
            return -1;
        }
        Color color = (c >= 'a') ? COLOR_BLACK : COLOR_WHITE;
        Piece piece = PIECE_NONE;
        switch (c | 32) {
            case 'p': piece = PIECE_PAWN; break;
            case 'n': piece = PIECE_KNIGHT; break;
            case 'b': piece = PIECE_BISHOP; break;
            case 'r': piece = PIECE_ROOK; break;
            case 'q': piece = PIECE_QUEEN; break;
            case 'k': piece = PIECE_KING; break;
            default: return -1;
        }
        int sq = rank * 8 + file;
        pos->pieces[color][piece] |= BIT(sq);
        ++file;
    }
    if (rank != 0 || file != 8) {
        return -1;
    }
    if (*cursor != ' ') {
        return -1;
    }
    ++cursor;

    if (*cursor == 'w') {
        pos->side_to_move = COLOR_WHITE;
    } else if (*cursor == 'b') {
        pos->side_to_move = COLOR_BLACK;
    } else {
        return -1;
    }
    ++cursor;
    if (*cursor != ' ') {
        return -1;
    }
    ++cursor;

    pos->castling_rights = 0;
    if (*cursor == '-') {
        ++cursor;
    } else {
        while (*cursor && *cursor != ' ') {
            switch (*cursor) {
                case 'K': pos->castling_rights |= CASTLE_WHITE_KING; break;
                case 'Q': pos->castling_rights |= CASTLE_WHITE_QUEEN; break;
                case 'k': pos->castling_rights |= CASTLE_BLACK_KING; break;
                case 'q': pos->castling_rights |= CASTLE_BLACK_QUEEN; break;
                default: return -1;
            }
            ++cursor;
        }
    }
    if (*cursor != ' ') {
        return -1;
    }
    ++cursor;

    pos->en_passant_square = -1;
    if (*cursor == '-') {
        ++cursor;
    } else {
        char file_char = *cursor++;
        char rank_char = *cursor++;
        if (file_char < 'a' || file_char > 'h' || rank_char < '1' || rank_char > '8') {
            return -1;
        }
        int file_idx = file_char - 'a';
        int rank_idx = rank_char - '1';
        pos->en_passant_square = (int8_t)(rank_idx * 8 + file_idx);
    }
    if (*cursor != ' ') {
        return -1;
    }
    ++cursor;

    pos->halfmove_clock = 0;
    while (*cursor && *cursor != ' ') {
        if (*cursor < '0' || *cursor > '9') {
            return -1;
        }
        pos->halfmove_clock = (uint16_t)(pos->halfmove_clock * 10 + (*cursor - '0'));
        ++cursor;
    }
    if (*cursor != ' ') {
        return -1;
    }
    ++cursor;

    uint16_t fullmove = 0;
    while (*cursor && *cursor != ' ') {
        if (*cursor < '0' || *cursor > '9') {
            return -1;
        }
        fullmove = (uint16_t)(fullmove * 10 + (uint16_t)(*cursor - '0'));
        ++cursor;
    }
    if (fullmove == 0) {
        fullmove = 1;
    }
    pos->fullmove_number = fullmove;
    pos->ply = (uint16_t)((fullmove - 1) * 2 + (pos->side_to_move == COLOR_BLACK ? 1 : 0));

    for (int c = 0; c < 2; ++c) {
        pos->occupancy[c] = 0;
        for (int p = 0; p < 6; ++p) {
            pos->occupancy[c] |= pos->pieces[c][p];
        }
    }
    pos->all = pos->occupancy[COLOR_WHITE] | pos->occupancy[COLOR_BLACK];
    return 0;
}

static inline void update_occupancy(Position *pos) {
    pos->occupancy[COLOR_WHITE] = 0;
    pos->occupancy[COLOR_BLACK] = 0;
    for (int p = 0; p < 6; ++p) {
        pos->occupancy[COLOR_WHITE] |= pos->pieces[COLOR_WHITE][p];
        pos->occupancy[COLOR_BLACK] |= pos->pieces[COLOR_BLACK][p];
    }
    pos->all = pos->occupancy[COLOR_WHITE] | pos->occupancy[COLOR_BLACK];
}

static inline void remove_piece(Position *pos, Square sq, Color color, Piece piece) {
    pos->pieces[color][piece] &= ~BIT(sq);
}

static inline void add_piece(Position *pos, Square sq, Color color, Piece piece) {
    pos->pieces[color][piece] |= BIT(sq);
}

static inline void move_piece(Position *pos, Square from, Square to, Color color, Piece piece) {
    pos->pieces[color][piece] &= ~BIT(from);
    pos->pieces[color][piece] |= BIT(to);
}

void position_make_move(Position *pos, const Move *move) {
    Color side = pos->side_to_move;
    Color enemy = COLOR_FLIP(side);
    Piece moving_piece = piece_on(pos, (Square)move->from, NULL);
    Piece captured_piece = piece_on(pos, (Square)move->to, NULL);

    if (move->flags & MOVE_FLAG_EN_PASSANT) {
        int ep_rank = (side == COLOR_WHITE) ? square_rank((Square)move->to) - 1 : square_rank((Square)move->to) + 1;
        Square captured_sq = (Square)(ep_rank * 8 + square_file((Square)move->to));
        captured_piece = PIECE_PAWN;
        remove_piece(pos, captured_sq, enemy, captured_piece);
    }

    if (captured_piece != PIECE_NONE && !(move->flags & MOVE_FLAG_EN_PASSANT)) {
        remove_piece(pos, (Square)move->to, enemy, captured_piece);
    }

    move_piece(pos, (Square)move->from, (Square)move->to, side, moving_piece);

    if ((move->flags & MOVE_FLAG_PROMOTION) && moving_piece == PIECE_PAWN) {
        remove_piece(pos, (Square)move->to, side, PIECE_PAWN);
        add_piece(pos, (Square)move->to, side, (Piece)move->promotion);
    }

    if (move->flags & MOVE_FLAG_CASTLE) {
        if (move->to == SQUARE_G1) {
            move_piece(pos, SQUARE_H1, SQUARE_F1, COLOR_WHITE, PIECE_ROOK);
        } else if (move->to == SQUARE_C1) {
            move_piece(pos, SQUARE_A1, SQUARE_D1, COLOR_WHITE, PIECE_ROOK);
        } else if (move->to == SQUARE_G8) {
            move_piece(pos, SQUARE_H8, SQUARE_F8, COLOR_BLACK, PIECE_ROOK);
        } else if (move->to == SQUARE_C8) {
            move_piece(pos, SQUARE_A8, SQUARE_D8, COLOR_BLACK, PIECE_ROOK);
        }
    }

    pos->en_passant_square = -1;
    if (moving_piece == PIECE_PAWN && (move->flags & MOVE_FLAG_DOUBLE_PAWN)) {
        int dir = (side == COLOR_WHITE) ? 1 : -1;
        int from_rank = square_rank((Square)move->from);
        int file = square_file((Square)move->from);
        int target_rank = square_rank((Square)move->to);
        int left_file = square_file((Square)move->to) - 1;
        int right_file = square_file((Square)move->to) + 1;
        int store = 0;
        if (left_file >= 0) {
            Square left_sq = (Square)(target_rank * 8 + left_file);
            if (pos->pieces[enemy][PIECE_PAWN] & BIT(left_sq)) {
                store = 1;
            }
        }
        if (!store && right_file <= 7) {
            Square right_sq = (Square)(target_rank * 8 + right_file);
            if (pos->pieces[enemy][PIECE_PAWN] & BIT(right_sq)) {
                store = 1;
            }
        }
        if (store) {
            pos->en_passant_square = (int8_t)((from_rank + dir) * 8 + file);
        }
    }

    if (moving_piece == PIECE_KING) {
        if (side == COLOR_WHITE) {
            pos->castling_rights &= ~(CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN);
        } else {
            pos->castling_rights &= ~(CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN);
        }
    } else if (moving_piece == PIECE_ROOK) {
        if (side == COLOR_WHITE) {
            if (move->from == SQUARE_A1) pos->castling_rights &= ~CASTLE_WHITE_QUEEN;
            if (move->from == SQUARE_H1) pos->castling_rights &= ~CASTLE_WHITE_KING;
        } else {
            if (move->from == SQUARE_A8) pos->castling_rights &= ~CASTLE_BLACK_QUEEN;
            if (move->from == SQUARE_H8) pos->castling_rights &= ~CASTLE_BLACK_KING;
        }
    }

    if (captured_piece == PIECE_ROOK) {
        if (move->to == SQUARE_A1) pos->castling_rights &= ~CASTLE_WHITE_QUEEN;
        if (move->to == SQUARE_H1) pos->castling_rights &= ~CASTLE_WHITE_KING;
        if (move->to == SQUARE_A8) pos->castling_rights &= ~CASTLE_BLACK_QUEEN;
        if (move->to == SQUARE_H8) pos->castling_rights &= ~CASTLE_BLACK_KING;
    }

    pos->side_to_move = enemy;
    pos->halfmove_clock = (moving_piece == PIECE_PAWN || captured_piece != PIECE_NONE) ? 0 : (uint16_t)(pos->halfmove_clock + 1);
    pos->ply = (uint16_t)(pos->ply + 1);
    if (side == COLOR_BLACK) {
        pos->fullmove_number = (uint16_t)(pos->fullmove_number + 1);
    }

    update_occupancy(pos);
}

void position_unmake_move(Position *pos, const Move *move, const Position *previous) {
    (void)move;
    *pos = *previous;
}

int position_to_fen(const Position *pos, char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return -1;
    }

    char tmp[128];
    size_t idx = 0;

    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Square sq = (Square)(rank * 8 + file);
            Color color;
            Piece piece = piece_on(pos, sq, &color);
            if (piece == PIECE_NONE) {
                ++empty;
            } else {
                if (empty > 0) {
                    if (idx >= sizeof(tmp) - 1) {
                        return -1;
                    }
                    tmp[idx++] = (char)('0' + empty);
                    empty = 0;
                }
                if (idx >= sizeof(tmp) - 1) {
                    return -1;
                }
                tmp[idx++] = piece_to_char(piece, color);
            }
        }
        if (empty > 0) {
            if (idx >= sizeof(tmp) - 1) {
                return -1;
            }
            tmp[idx++] = (char)('0' + empty);
        }
        if (rank > 0) {
            if (idx >= sizeof(tmp) - 1) {
                return -1;
            }
            tmp[idx++] = '/';
        }
    }

    if (idx >= sizeof(tmp) - 1) {
        return -1;
    }
    tmp[idx++] = ' ';
    if (idx >= sizeof(tmp) - 1) {
        return -1;
    }
    tmp[idx++] = (pos->side_to_move == COLOR_WHITE) ? 'w' : 'b';
    if (idx >= sizeof(tmp) - 1) {
        return -1;
    }
    tmp[idx++] = ' ';
    if (pos->castling_rights == 0) {
        if (idx >= sizeof(tmp) - 1) {
            return -1;
        }
        tmp[idx++] = '-';
    } else {
        if (pos->castling_rights & CASTLE_WHITE_KING) {
            if (idx >= sizeof(tmp) - 1) {
                return -1;
            }
            tmp[idx++] = 'K';
        }
        if (pos->castling_rights & CASTLE_WHITE_QUEEN) {
            if (idx >= sizeof(tmp) - 1) {
                return -1;
            }
            tmp[idx++] = 'Q';
        }
        if (pos->castling_rights & CASTLE_BLACK_KING) {
            if (idx >= sizeof(tmp) - 1) {
                return -1;
            }
            tmp[idx++] = 'k';
        }
        if (pos->castling_rights & CASTLE_BLACK_QUEEN) {
            if (idx >= sizeof(tmp) - 1) {
                return -1;
            }
            tmp[idx++] = 'q';
        }
    }
    if (idx >= sizeof(tmp) - 1) {
        return -1;
    }
    tmp[idx++] = ' ';
    if (pos->en_passant_square < 0) {
        if (idx >= sizeof(tmp) - 1) {
            return -1;
        }
        tmp[idx++] = '-';
    } else {
        Square ep = (Square)pos->en_passant_square;
        char file_char = (char)('a' + square_file(ep));
        char rank_char = (char)('1' + square_rank(ep));
        if (idx >= sizeof(tmp) - 2) {
            return -1;
        }
        tmp[idx++] = file_char;
        tmp[idx++] = rank_char;
    }

    size_t remaining = sizeof(tmp) - idx;
    int written = snprintf(tmp + idx, remaining, " %u %u", pos->halfmove_clock, pos->fullmove_number);
    if (written < 0 || (size_t)written >= remaining) {
        return -1;
    }
    idx += (size_t)written;

    if (idx + 1 > buffer_len) {
        return -1;
    }
    memcpy(buffer, tmp, idx);
    buffer[idx] = '\0';
    return 0;
}

int position_has_legal_moves(const Position *pos) {
    Move move;
    return generate_legal_moves(pos, &move, 1) > 0;
}

int position_is_insufficient_material(const Position *pos) {
    if (pos->pieces[COLOR_WHITE][PIECE_PAWN] || pos->pieces[COLOR_BLACK][PIECE_PAWN]) {
        return 0;
    }
    if (pos->pieces[COLOR_WHITE][PIECE_ROOK] || pos->pieces[COLOR_BLACK][PIECE_ROOK]) {
        return 0;
    }
    if (pos->pieces[COLOR_WHITE][PIECE_QUEEN] || pos->pieces[COLOR_BLACK][PIECE_QUEEN]) {
        return 0;
    }

    Bitboard white_bishops = pos->pieces[COLOR_WHITE][PIECE_BISHOP];
    Bitboard black_bishops = pos->pieces[COLOR_BLACK][PIECE_BISHOP];
    Bitboard white_knights = pos->pieces[COLOR_WHITE][PIECE_KNIGHT];
    Bitboard black_knights = pos->pieces[COLOR_BLACK][PIECE_KNIGHT];

    int bishop_count = __builtin_popcountll(white_bishops | black_bishops);
    int knight_count = __builtin_popcountll(white_knights | black_knights);

    if (bishop_count == 0 && knight_count == 0) {
        return 1;
    }

    if (bishop_count + knight_count == 1) {
        return 1;
    }

    if (bishop_count == 2 && knight_count == 0) {
        Bitboard bishops = white_bishops | black_bishops;
        Bitboard light = bishops & LIGHT_SQUARES;
        Bitboard dark = bishops & ~LIGHT_SQUARES;
        if (light == 0 || dark == 0) {
            return 1;
        }
    }

    return 0;
}

int is_square_attacked(const Position *pos, Square sq, Color by) {
    Bitboard occupied = pos->all;
    Bitboard attackers = 0;
    Bitboard mask = BIT(sq);

    if (pawn_attacks[COLOR_FLIP(by)][sq] & pos->pieces[by][PIECE_PAWN]) {
        return 1;
    }
    if (knight_attacks[sq] & pos->pieces[by][PIECE_KNIGHT]) {
        return 1;
    }
    if (king_attacks[sq] & pos->pieces[by][PIECE_KING]) {
        return 1;
    }
    attackers = bishop_attacks(sq, occupied) & (pos->pieces[by][PIECE_BISHOP] | pos->pieces[by][PIECE_QUEEN]);
    if (attackers) {
        return 1;
    }
    attackers = rook_attacks(sq, occupied) & (pos->pieces[by][PIECE_ROOK] | pos->pieces[by][PIECE_QUEEN]);
    if (attackers) {
        return 1;
    }
    (void)mask;
    return 0;
}

int in_check(const Position *pos, Color side) {
    Piece king_piece = PIECE_KING;
    Bitboard king_bb = pos->pieces[side][king_piece];
    if (!king_bb) {
        return 0;
    }
    Square king_sq = (Square)__builtin_ctzll(king_bb);
    return is_square_attacked(pos, king_sq, COLOR_FLIP(side));
}

static inline void push_move(Move *moves, size_t *count, size_t max, uint8_t from, uint8_t to, uint8_t flags, uint8_t promotion) {
    if (*count >= max) {
        return;
    }
    moves[*count].from = from;
    moves[*count].to = to;
    moves[*count].flags = flags;
    moves[*count].promotion = promotion;
    (*count)++;
}

static size_t generate_pseudo_moves(const Position *pos, Move *moves, size_t max) {
    Color side = pos->side_to_move;
    Color enemy = COLOR_FLIP(side);
    Bitboard own_occ = pos->occupancy[side];
    Bitboard enemy_occ = pos->occupancy[enemy];
    Bitboard empty = ~pos->all;
    size_t count = 0;

    Bitboard pawns = pos->pieces[side][PIECE_PAWN];
    while (pawns) {
        Square from = (Square)__builtin_ctzll(pawns);
        pawns &= pawns - 1;
        Bitboard single_push = pawn_pushes[side][from] & empty;
        if (single_push) {
            Square to = (Square)__builtin_ctzll(single_push);
            if ((side == COLOR_WHITE && square_rank(to) == 7) || (side == COLOR_BLACK && square_rank(to) == 0)) {
                push_move(moves, &count, max, from, to, MOVE_FLAG_PROMOTION, PIECE_QUEEN);
                push_move(moves, &count, max, from, to, MOVE_FLAG_PROMOTION, PIECE_ROOK);
                push_move(moves, &count, max, from, to, MOVE_FLAG_PROMOTION, PIECE_BISHOP);
                push_move(moves, &count, max, from, to, MOVE_FLAG_PROMOTION, PIECE_KNIGHT);
            } else {
                push_move(moves, &count, max, from, to, MOVE_FLAG_NONE, PIECE_NONE);
                int start_rank = (side == COLOR_WHITE) ? 1 : 6;
                if (square_rank(from) == start_rank) {
                    Bitboard double_push = pawn_pushes[side][to] & empty;
                    if (double_push) {
                        Square target = (Square)__builtin_ctzll(double_push);
                        push_move(moves, &count, max, from, target, MOVE_FLAG_DOUBLE_PAWN, PIECE_NONE);
                    }
                }
            }
        }
        Bitboard captures = pawn_attacks[side][from] & enemy_occ;
        while (captures) {
            Square to = (Square)__builtin_ctzll(captures);
            captures &= captures - 1;
            uint8_t flags = MOVE_FLAG_CAPTURE;
            if ((side == COLOR_WHITE && square_rank(to) == 7) || (side == COLOR_BLACK && square_rank(to) == 0)) {
                flags |= MOVE_FLAG_PROMOTION;
                push_move(moves, &count, max, from, to, flags, PIECE_QUEEN);
                push_move(moves, &count, max, from, to, flags, PIECE_ROOK);
                push_move(moves, &count, max, from, to, flags, PIECE_BISHOP);
                push_move(moves, &count, max, from, to, flags, PIECE_KNIGHT);
            } else {
                push_move(moves, &count, max, from, to, flags, PIECE_NONE);
            }
        }
        if (pos->en_passant_square != -1) {
            Square ep = (Square)pos->en_passant_square;
            if (pawn_attacks[side][from] & BIT(ep)) {
                push_move(moves, &count, max, from, ep, MOVE_FLAG_EN_PASSANT | MOVE_FLAG_CAPTURE, PIECE_NONE);
            }
        }
    }

    Bitboard knights = pos->pieces[side][PIECE_KNIGHT];
    while (knights) {
        Square from = (Square)__builtin_ctzll(knights);
        knights &= knights - 1;
        Bitboard attacks = knight_attacks[from] & ~own_occ;
        while (attacks) {
            Square to = (Square)__builtin_ctzll(attacks);
            attacks &= attacks - 1;
            uint8_t flags = (enemy_occ & BIT(to)) ? MOVE_FLAG_CAPTURE : MOVE_FLAG_NONE;
            push_move(moves, &count, max, from, to, flags, PIECE_NONE);
        }
    }

    Bitboard bishops = pos->pieces[side][PIECE_BISHOP];
    while (bishops) {
        Square from = (Square)__builtin_ctzll(bishops);
        bishops &= bishops - 1;
        Bitboard attacks = bishop_attacks(from, pos->all) & ~own_occ;
        while (attacks) {
            Square to = (Square)__builtin_ctzll(attacks);
            attacks &= attacks - 1;
            uint8_t flags = (enemy_occ & BIT(to)) ? MOVE_FLAG_CAPTURE : MOVE_FLAG_NONE;
            push_move(moves, &count, max, from, to, flags, PIECE_NONE);
        }
    }

    Bitboard rooks = pos->pieces[side][PIECE_ROOK];
    while (rooks) {
        Square from = (Square)__builtin_ctzll(rooks);
        rooks &= rooks - 1;
        Bitboard attacks = rook_attacks(from, pos->all) & ~own_occ;
        while (attacks) {
            Square to = (Square)__builtin_ctzll(attacks);
            attacks &= attacks - 1;
            uint8_t flags = (enemy_occ & BIT(to)) ? MOVE_FLAG_CAPTURE : MOVE_FLAG_NONE;
            push_move(moves, &count, max, from, to, flags, PIECE_NONE);
        }
    }

    Bitboard queens = pos->pieces[side][PIECE_QUEEN];
    while (queens) {
        Square from = (Square)__builtin_ctzll(queens);
        queens &= queens - 1;
        Bitboard attacks = queen_attacks(from, pos->all) & ~own_occ;
        while (attacks) {
            Square to = (Square)__builtin_ctzll(attacks);
            attacks &= attacks - 1;
            uint8_t flags = (enemy_occ & BIT(to)) ? MOVE_FLAG_CAPTURE : MOVE_FLAG_NONE;
            push_move(moves, &count, max, from, to, flags, PIECE_NONE);
        }
    }

    Bitboard king = pos->pieces[side][PIECE_KING];
    if (king) {
        Square from = (Square)__builtin_ctzll(king);
        Bitboard attacks = king_attacks[from] & ~own_occ;
        while (attacks) {
            Square to = (Square)__builtin_ctzll(attacks);
            attacks &= attacks - 1;
            uint8_t flags = (enemy_occ & BIT(to)) ? MOVE_FLAG_CAPTURE : MOVE_FLAG_NONE;
            push_move(moves, &count, max, from, to, flags, PIECE_NONE);
        }
        if (side == COLOR_WHITE) {
            if ((pos->castling_rights & CASTLE_WHITE_KING) && !(pos->all & (BIT(SQUARE_F1) | BIT(SQUARE_G1)))) {
                if (!is_square_attacked(pos, SQUARE_E1, enemy) && !is_square_attacked(pos, SQUARE_F1, enemy) && !is_square_attacked(pos, SQUARE_G1, enemy)) {
                    push_move(moves, &count, max, from, SQUARE_G1, MOVE_FLAG_CASTLE, PIECE_NONE);
                }
            }
            if ((pos->castling_rights & CASTLE_WHITE_QUEEN) && !(pos->all & (BIT(SQUARE_B1) | BIT(SQUARE_C1) | BIT(SQUARE_D1)))) {
                if (!is_square_attacked(pos, SQUARE_E1, enemy) && !is_square_attacked(pos, SQUARE_D1, enemy) && !is_square_attacked(pos, SQUARE_C1, enemy)) {
                    push_move(moves, &count, max, from, SQUARE_C1, MOVE_FLAG_CASTLE, PIECE_NONE);
                }
            }
        } else {
            if ((pos->castling_rights & CASTLE_BLACK_KING) && !(pos->all & (BIT(SQUARE_F8) | BIT(SQUARE_G8)))) {
                if (!is_square_attacked(pos, SQUARE_E8, enemy) && !is_square_attacked(pos, SQUARE_F8, enemy) && !is_square_attacked(pos, SQUARE_G8, enemy)) {
                    push_move(moves, &count, max, from, SQUARE_G8, MOVE_FLAG_CASTLE, PIECE_NONE);
                }
            }
            if ((pos->castling_rights & CASTLE_BLACK_QUEEN) && !(pos->all & (BIT(SQUARE_B8) | BIT(SQUARE_C8) | BIT(SQUARE_D8)))) {
                if (!is_square_attacked(pos, SQUARE_E8, enemy) && !is_square_attacked(pos, SQUARE_D8, enemy) && !is_square_attacked(pos, SQUARE_C8, enemy)) {
                    push_move(moves, &count, max, from, SQUARE_C8, MOVE_FLAG_CASTLE, PIECE_NONE);
                }
            }
        }
    }

    return count;
}

size_t generate_legal_moves(const Position *pos, Move *out_moves, size_t max_moves) {
    Move buffer[256];
    size_t pseudo = generate_pseudo_moves(pos, buffer, 256);
    size_t stored = 0;
    for (size_t i = 0; i < pseudo; ++i) {
        Position next = *pos;
        position_make_move(&next, &buffer[i]);
        if (!in_check(&next, pos->side_to_move)) {
            if (stored < max_moves) {
                out_moves[stored] = buffer[i];
            }
            ++stored;
        }
    }
    return stored;
}

int is_checkmate(const Position *pos) {
    if (!in_check(pos, pos->side_to_move)) {
        return 0;
    }
    Move moves[256];
    size_t count = generate_legal_moves(pos, moves, 256);
    return count == 0;
}

int is_stalemate(const Position *pos) {
    if (in_check(pos, pos->side_to_move)) {
        return 0;
    }
    Move moves[256];
    size_t count = generate_legal_moves(pos, moves, 256);
    return count == 0;
}
