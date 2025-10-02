#include "chess.h"

#include <stdio.h>
#include <time.h>

#define MOVE_ITERATIONS 5000
#define CHECK_ITERATIONS 200000
#define MATE_ITERATIONS 20000
#define STALE_ITERATIONS 20000

typedef struct {
    const char *name;
    const char *fen;
} PositionSpec;

static const PositionSpec SPECS[] = {
    {"start", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
    {"midgame", "r1bq1rk1/pp1n1ppp/2pbpn2/3p4/3P2P1/2N1PN1P/PPQ1BP2/R1B2RK1 w - - 0 9"},
    {"endgame", "8/3k4/8/8/8/8/4K3/3R4 w - - 0 1"},
    {"checkmate", "7k/5Q2/7R/7K/8/8/8/8 b - - 0 1"},
    {"stalemate", "7k/5Q2/6RK/8/8/8/8/8 b - - 0 1"}
};

static double seconds_between(clock_t start, clock_t end) {
    return (double)(end - start) / (double)CLOCKS_PER_SEC;
}

static void benchmark_position(const PositionSpec *spec) {
    Position pos;
    if (position_from_fen(&pos, spec->fen) != 0) {
        fprintf(stderr, "Failed to parse FEN for %s\n", spec->name);
        return;
    }

    Move moves[256];
    size_t move_accum = 0;
    clock_t start = clock();
    for (size_t i = 0; i < MOVE_ITERATIONS; ++i) {
        move_accum += generate_legal_moves(&pos, moves, 256);
    }
    clock_t end = clock();
    double move_seconds = seconds_between(start, end);

    size_t check_accum = 0;
    start = clock();
    for (size_t i = 0; i < CHECK_ITERATIONS; ++i) {
        check_accum += in_check(&pos, pos.side_to_move);
    }
    end = clock();
    double check_seconds = seconds_between(start, end);

    size_t mate_accum = 0;
    start = clock();
    for (size_t i = 0; i < MATE_ITERATIONS; ++i) {
        mate_accum += is_checkmate(&pos);
    }
    end = clock();
    double mate_seconds = seconds_between(start, end);

    size_t stale_accum = 0;
    start = clock();
    for (size_t i = 0; i < STALE_ITERATIONS; ++i) {
        stale_accum += is_stalemate(&pos);
    }
    end = clock();
    double stale_seconds = seconds_between(start, end);

    printf("%s\n", spec->name);
    printf("  moves     : %zu iter, %.6f s total, %.3f us/iter, sum=%zu\n",
           (size_t)MOVE_ITERATIONS,
           move_seconds,
           (move_seconds / MOVE_ITERATIONS) * 1e6,
           move_accum);
    printf("  check     : %zu iter, %.6f s total, %.1f ns/iter, sum=%zu\n",
           (size_t)CHECK_ITERATIONS,
           check_seconds,
           (check_seconds / CHECK_ITERATIONS) * 1e9,
           check_accum);
    printf("  checkmate : %zu iter, %.6f s total, %.3f us/iter, sum=%zu\n",
           (size_t)MATE_ITERATIONS,
           mate_seconds,
           (mate_seconds / MATE_ITERATIONS) * 1e6,
           mate_accum);
    printf("  stalemate : %zu iter, %.6f s total, %.3f us/iter, sum=%zu\n",
           (size_t)STALE_ITERATIONS,
           stale_seconds,
           (stale_seconds / STALE_ITERATIONS) * 1e6,
           stale_accum);
}

int main(void) {
    chess_init_tables();
    size_t count = sizeof(SPECS) / sizeof(SPECS[0]);
    for (size_t i = 0; i < count; ++i) {
        benchmark_position(&SPECS[i]);
    }
    return 0;
}
