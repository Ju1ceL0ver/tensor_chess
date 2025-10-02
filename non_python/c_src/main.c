#include "chess.h"

#include <stdio.h>
#include <string.h>

static void square_to_string(Square sq, char out[3]) {
    out[0] = (char)('a' + square_file(sq));
    out[1] = (char)('1' + square_rank(sq));
    out[2] = '\0';
}

static void move_to_string(const Move *move, char out[6]) {
    char from[3];
    char to[3];
    square_to_string((Square)move->from, from);
    square_to_string((Square)move->to, to);
    out[0] = from[0];
    out[1] = from[1];
    out[2] = to[0];
    out[3] = to[1];
    int idx = 4;
    if (move->flags & MOVE_FLAG_PROMOTION) {
        switch (move->promotion) {
            case PIECE_QUEEN: out[idx++] = 'q'; break;
            case PIECE_ROOK: out[idx++] = 'r'; break;
            case PIECE_BISHOP: out[idx++] = 'b'; break;
            case PIECE_KNIGHT: out[idx++] = 'n'; break;
            default: out[idx++] = 'q'; break;
        }
    }
    out[idx] = '\0';
}

int main(int argc, char **argv) {
    chess_init_tables();

    Position pos;
    if (argc > 1) {
        if (position_from_fen(&pos, argv[1]) != 0) {
            fprintf(stderr, "Invalid FEN.\n");
            return 1;
        }
    } else {
        position_set_start(&pos);
    }

    Move moves[256];
    size_t count = generate_legal_moves(&pos, moves, 256);

    printf("Side to move: %s\n", pos.side_to_move == COLOR_WHITE ? "white" : "black");
    printf("In check: %s\n", in_check(&pos, pos.side_to_move) ? "yes" : "no");
    printf("Checkmate: %s\n", is_checkmate(&pos) ? "yes" : "no");
    printf("Stalemate: %s\n", is_stalemate(&pos) ? "yes" : "no");
    printf("Legal moves: %zu\n", count);

    size_t preview = count < 10 ? count : 10;
    for (size_t i = 0; i < preview; ++i) {
        char buf[6];
        move_to_string(&moves[i], buf);
        printf("  %s\n", buf);
    }

    return 0;
}
