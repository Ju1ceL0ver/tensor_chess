#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "chess.h"

#include <stdbool.h>
#include <string.h>

#define BIT(sq) (1ULL << (sq))

#define TENSOR_CHANNELS 15
#define TENSOR_SIZE (TENSOR_CHANNELS * 64)

typedef struct {
    Position position;
    Move move;
} PositionHistoryEntry;

typedef struct {
    PyObject_HEAD
    Position pos;
    PositionHistoryEntry *history;
    Py_ssize_t history_size;
    Py_ssize_t history_capacity;
} PositionObject;

static inline void square_to_coord(uint8_t sq, char out[3]) {
    out[0] = (char)('a' + (sq & 7));
    out[1] = (char)('1' + (sq >> 3));
    out[2] = '\0';
}

static inline void move_to_uci(const Move *move, char out[6]) {
    char from[3];
    char to[3];
    square_to_coord(move->from, from);
    square_to_coord(move->to, to);
    out[0] = from[0];
    out[1] = from[1];
    out[2] = to[0];
    out[3] = to[1];
    int idx = 4;
    if (move->flags & MOVE_FLAG_PROMOTION) {
        char promo = 'q';
        switch (move->promotion) {
            case PIECE_QUEEN: promo = 'q'; break;
            case PIECE_ROOK: promo = 'r'; break;
            case PIECE_BISHOP: promo = 'b'; break;
            case PIECE_KNIGHT: promo = 'n'; break;
            default: promo = 'q'; break;
        }
        out[idx++] = promo;
    }
    out[idx] = '\0';
}

static const int PIECE_CHANNELS[2][6] = {
    {0, 1, 2, 3, 4, 5},
    {6, 7, 8, 9, 10, 11}
};

static Piece position_piece_on_local(const Position *pos, Square sq, Color *color_out) {
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

static char piece_display_char(Piece piece, Color color) {
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

static PyObject *move_to_tuple_obj(const Move *move) {
    return Py_BuildValue("(BBBB)",
                         move->from,
                         move->to,
                         move->flags,
                         move->promotion);
}

static bool match_move_tuple(const Move *candidate, PyObject *tuple) {
    if (!PyTuple_Check(tuple) || PyTuple_GET_SIZE(tuple) != 4) {
        return false;
    }
    long from = PyLong_AsLong(PyTuple_GET_ITEM(tuple, 0));
    if (PyErr_Occurred()) return false;
    long to = PyLong_AsLong(PyTuple_GET_ITEM(tuple, 1));
    if (PyErr_Occurred()) return false;
    long flags = PyLong_AsLong(PyTuple_GET_ITEM(tuple, 2));
    if (PyErr_Occurred()) return false;
    long promotion = PyLong_AsLong(PyTuple_GET_ITEM(tuple, 3));
    if (PyErr_Occurred()) return false;
    return candidate->from == (uint8_t)from &&
           candidate->to == (uint8_t)to &&
           candidate->flags == (uint8_t)flags &&
           candidate->promotion == (uint8_t)promotion;
}

static void Position_clear_history(PositionObject *self) {
    self->history_size = 0;
}

static void Position_release_history(PositionObject *self) {
    if (self->history) {
        PyMem_Free(self->history);
        self->history = NULL;
    }
    self->history_size = 0;
    self->history_capacity = 0;
}

static int Position_ensure_history(PositionObject *self, Py_ssize_t extra) {
    if (self->history_size + extra <= self->history_capacity) {
        return 0;
    }
    Py_ssize_t new_capacity = self->history_capacity ? self->history_capacity * 2 : 16;
    while (new_capacity < self->history_size + extra) {
        new_capacity *= 2;
    }
    PositionHistoryEntry *entries = PyMem_Realloc(self->history, (size_t)new_capacity * sizeof(PositionHistoryEntry));
    if (!entries) {
        PyErr_NoMemory();
        return -1;
    }
    self->history = entries;
    self->history_capacity = new_capacity;
    return 0;
}

static int Position_resolve_move(PositionObject *self, PyObject *move_obj, Move *out_move) {
    Move moves[256];
    size_t count = generate_legal_moves(&self->pos, moves, 256);
    const Move *selected = NULL;

    if (PyUnicode_Check(move_obj)) {
        Py_ssize_t len = 0;
        const char *uci = PyUnicode_AsUTF8AndSize(move_obj, &len);
        if (!uci) {
            return -1;
        }
        for (size_t i = 0; i < count; ++i) {
            char buf[6];
            move_to_uci(&moves[i], buf);
            size_t candidate_len = strlen(buf);
            if ((Py_ssize_t)candidate_len == len && strncmp(buf, uci, candidate_len) == 0) {
                selected = &moves[i];
                break;
            }
        }
    } else if (PyTuple_Check(move_obj)) {
        for (size_t i = 0; i < count; ++i) {
            if (match_move_tuple(&moves[i], move_obj)) {
                selected = &moves[i];
                break;
            }
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "move must be a UCI string or a tuple(from,to,flags,promotion)");
        return -1;
    }

    if (!selected) {
        PyErr_SetString(PyExc_ValueError, "move is not legal in current position");
        return -1;
    }

    *out_move = *selected;
    return 0;
}

static void Position_dealloc(PositionObject *self) {
    Position_release_history(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Position_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    (void)args;
    (void)kwds;
    PositionObject *self = (PositionObject *)type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    self->history = NULL;
    self->history_size = 0;
    self->history_capacity = 0;
    position_set_start(&self->pos);
    return (PyObject *)self;
}

static int Position_init(PositionObject *self, PyObject *args, PyObject *kwds) {
    const char *fen = NULL;
    static char *kwlist[] = {"fen", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|z", kwlist, &fen)) {
        return -1;
    }
    if (fen) {
        if (position_from_fen(&self->pos, fen) != 0) {
            PyErr_SetString(PyExc_ValueError, "invalid FEN");
            return -1;
        }
    } else {
        position_set_start(&self->pos);
    }
    Position_clear_history(self);
    return 0;
}

static PyObject *Position_set_fen(PositionObject *self, PyObject *args) {
    const char *fen = NULL;
    if (!PyArg_ParseTuple(args, "s", &fen)) {
        return NULL;
    }
    if (position_from_fen(&self->pos, fen) != 0) {
        PyErr_SetString(PyExc_ValueError, "invalid FEN");
        return NULL;
    }
    Position_clear_history(self);
    Py_RETURN_NONE;
}

static PyObject *Position_set_start(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    position_set_start(&self->pos);
    Position_clear_history(self);
    Py_RETURN_NONE;
}

static PyObject *Position_generate_legal_moves(PositionObject *self, PyObject *args, PyObject *kwds) {
    int as_strings = 0;
    static char *kwlist[] = {"as_strings", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|p", kwlist, &as_strings)) {
        return NULL;
    }
    Move moves[256];
    size_t count = generate_legal_moves(&self->pos, moves, 256);
    PyObject *list = PyList_New(count);
    if (!list) {
        return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        PyObject *value = NULL;
        if (as_strings) {
            char buf[6];
            move_to_uci(&moves[i], buf);
            value = PyUnicode_FromString(buf);
        } else {
            value = move_to_tuple_obj(&moves[i]);
        }
        if (!value) {
            Py_DECREF(list);
            return NULL;
        }
        PyList_SET_ITEM(list, (Py_ssize_t)i, value);
    }
    return list;
}

static PyObject *Position_legal_move_count(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    Move moves[256];
    size_t count = generate_legal_moves(&self->pos, moves, 256);
    return PyLong_FromSize_t(count);
}

static PyObject *Position_in_check(PositionObject *self, PyObject *args) {
    int color = -1;
    if (!PyArg_ParseTuple(args, "|i", &color)) {
        return NULL;
    }
    Color side;
    if (color == -1) {
        side = self->pos.side_to_move;
    } else if (color == 0) {
        side = COLOR_WHITE;
    } else if (color == 1) {
        side = COLOR_BLACK;
    } else {
        PyErr_SetString(PyExc_ValueError, "color must be 0 (white), 1 (black), or omitted");
        return NULL;
    }
    return PyBool_FromLong(in_check(&self->pos, side));
}

static PyObject *Position_is_checkmate(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    return PyBool_FromLong(is_checkmate(&self->pos));
}

static PyObject *Position_is_stalemate(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    return PyBool_FromLong(is_stalemate(&self->pos));
}

static PyObject *Position_make_move(PositionObject *self, PyObject *args) {
    PyObject *move_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &move_obj)) {
        return NULL;
    }
    Move move;
    if (Position_resolve_move(self, move_obj, &move) != 0) {
        return NULL;
    }
    position_make_move(&self->pos, &move);
    Py_RETURN_NONE;
}

static PyObject *Position_push(PositionObject *self, PyObject *args) {
    PyObject *move_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &move_obj)) {
        return NULL;
    }
    Move move;
    if (Position_resolve_move(self, move_obj, &move) != 0) {
        return NULL;
    }
    if (Position_ensure_history(self, 1) != 0) {
        return NULL;
    }
    PositionHistoryEntry *entry = &self->history[self->history_size++];
    entry->position = self->pos;
    entry->move = move;
    position_make_move(&self->pos, &move);
    PyObject *result = move_to_tuple_obj(&move);
    return result;
}

static PyObject *Position_pop(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->history_size == 0) {
        PyErr_SetString(PyExc_IndexError, "pop from empty move stack");
        return NULL;
    }
    PositionHistoryEntry *entry = &self->history[self->history_size - 1];
    Move move = entry->move;
    self->pos = entry->position;
    self->history_size -= 1;
    PyObject *result = move_to_tuple_obj(&move);
    return result;
}

static PyObject *Position_peek(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->history_size == 0) {
        PyErr_SetString(PyExc_IndexError, "peek from empty move stack");
        return NULL;
    }
    return move_to_tuple_obj(&self->history[self->history_size - 1].move);
}

static PyObject *Position_clear_stack(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    Position_clear_history(self);
    Py_RETURN_NONE;
}

static PyObject *Position_stack_size(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    return PyLong_FromSsize_t(self->history_size);
}

static PyObject *Position_fen(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    char buffer[128];
    if (position_to_fen(&self->pos, buffer, sizeof(buffer)) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "failed to encode FEN");
        return NULL;
    }
    return PyUnicode_FromString(buffer);
}

static PyObject *Position_board(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    char buffer[8 * 16];
    size_t idx = 0;
    for (int rank = 7; rank >= 0; --rank) {
        for (int file = 0; file < 8; ++file) {
            Square sq = (Square)(rank * 8 + file);
            Color color;
            Piece piece = position_piece_on_local(&self->pos, sq, &color);
            char symbol = '.';
            if (piece != PIECE_NONE) {
                symbol = piece_display_char(piece, color);
            }
            buffer[idx++] = symbol;
            if (file < 7) {
                buffer[idx++] = ' ';
            }
        }
        if (rank > 0) {
            buffer[idx++] = '\n';
        }
    }
    return PyUnicode_FromStringAndSize(buffer, (Py_ssize_t)idx);
}

static PyObject *Position_has_legal_moves_py(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    return PyBool_FromLong(position_has_legal_moves(&self->pos));
}

static PyObject *Position_is_insufficient_material_py(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    return PyBool_FromLong(position_is_insufficient_material(&self->pos));
}

static PyObject *Position_is_game_over(PositionObject *self, PyObject *args, PyObject *kwds) {
    int claim_draw = 0;
    static char *kwlist[] = {"claim_draw", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|p", kwlist, &claim_draw)) {
        return NULL;
    }
    int over = 0;
    if (is_checkmate(&self->pos) || is_stalemate(&self->pos) || position_is_insufficient_material(&self->pos)) {
        over = 1;
    } else if (claim_draw && self->pos.halfmove_clock >= 100) {
        over = 1;
    }
    return PyBool_FromLong(over);
}

static PyObject *Position_result(PositionObject *self, PyObject *args, PyObject *kwds) {
    int claim_draw = 0;
    static char *kwlist[] = {"claim_draw", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|p", kwlist, &claim_draw)) {
        return NULL;
    }
    if (is_checkmate(&self->pos)) {
        if (self->pos.side_to_move == COLOR_WHITE) {
            return PyUnicode_FromString("0-1");
        }
        return PyUnicode_FromString("1-0");
    }
    if (is_stalemate(&self->pos) || position_is_insufficient_material(&self->pos) || (claim_draw && self->pos.halfmove_clock >= 100)) {
        return PyUnicode_FromString("1/2-1/2");
    }
    return PyUnicode_FromString("*");
}

static PyObject *Position_to_tensor(PositionObject *self, PyObject *args, PyObject *kwds) {
    PyObject *out_obj = NULL;
    static char *kwlist[] = {"out", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &out_obj)) {
        return NULL;
    }

    unsigned char *data = NULL;
    PyObject *result_obj = NULL;
    Py_buffer view;
    int using_buffer = 0;

    if (out_obj) {
        if (PyObject_GetBuffer(out_obj, &view, PyBUF_WRITABLE | PyBUF_CONTIG)) {
            return NULL;
        }
        if ((size_t)view.len != TENSOR_SIZE) {
            PyBuffer_Release(&view);
            PyErr_Format(PyExc_ValueError, "out buffer must have length %zu", (size_t)TENSOR_SIZE);
            return NULL;
        }
        data = (unsigned char *)view.buf;
        using_buffer = 1;
    } else {
        PyObject *bytes = PyBytes_FromStringAndSize(NULL, TENSOR_SIZE);
        if (!bytes) {
            return NULL;
        }
        data = (unsigned char *)PyBytes_AS_STRING(bytes);
        result_obj = bytes;
    }

    memset(data, 0, TENSOR_SIZE);

    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            Bitboard bb = self->pos.pieces[color][piece];
            if (!bb) {
                continue;
            }
            const int channel = PIECE_CHANNELS[color][piece];
            unsigned char *plane = data + channel * 64;
            while (bb) {
                int sq = __builtin_ctzll(bb);
                plane[sq] = 1;
                bb &= bb - 1;
            }
        }
    }

    unsigned char *stm_plane = data + 12 * 64;
    if (self->pos.side_to_move == COLOR_WHITE) {
        memset(stm_plane, 1, 64);
    }

    unsigned char *castle_plane = data + 13 * 64;
    if (self->pos.castling_rights & CASTLE_WHITE_KING) {
        castle_plane[SQUARE_H1] = 1;
    }
    if (self->pos.castling_rights & CASTLE_WHITE_QUEEN) {
        castle_plane[SQUARE_A1] = 1;
    }
    if (self->pos.castling_rights & CASTLE_BLACK_KING) {
        castle_plane[SQUARE_H8] = 1;
    }
    if (self->pos.castling_rights & CASTLE_BLACK_QUEEN) {
        castle_plane[SQUARE_A8] = 1;
    }

    unsigned char *ep_plane = data + 14 * 64;
    if (self->pos.en_passant_square >= 0) {
        ep_plane[self->pos.en_passant_square] = 1;
    }

    if (using_buffer) {
        PyBuffer_Release(&view);
        Py_INCREF(out_obj);
        return out_obj;
    }
    return result_obj;
}

static PyObject *Position_clone(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    PyTypeObject *type = Py_TYPE(self);
    PositionObject *clone = (PositionObject *)type->tp_alloc(type, 0);
    if (!clone) {
        return NULL;
    }
    clone->pos = self->pos;
    clone->history = NULL;
    clone->history_size = 0;
    clone->history_capacity = 0;
    return (PyObject *)clone;
}

static PyObject *Position_bitboards(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    PyObject *white = PyTuple_New(6);
    PyObject *black = PyTuple_New(6);
    if (!white || !black) {
        Py_XDECREF(white);
        Py_XDECREF(black);
        return NULL;
    }
    for (int p = 0; p < 6; ++p) {
        PyObject *w = PyLong_FromUnsignedLongLong(self->pos.pieces[COLOR_WHITE][p]);
        PyObject *b = PyLong_FromUnsignedLongLong(self->pos.pieces[COLOR_BLACK][p]);
        if (!w || !b) {
            Py_XDECREF(w);
            Py_XDECREF(b);
            Py_DECREF(white);
            Py_DECREF(black);
            return NULL;
        }
        PyTuple_SET_ITEM(white, p, w);
        PyTuple_SET_ITEM(black, p, b);
    }
    PyObject *result = PyTuple_New(2);
    if (!result) {
        Py_DECREF(white);
        Py_DECREF(black);
        return NULL;
    }
    PyTuple_SET_ITEM(result, 0, white);
    PyTuple_SET_ITEM(result, 1, black);
    return result;
}

static PyObject *Position_occupancy(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    PyObject *white = PyLong_FromUnsignedLongLong(self->pos.occupancy[COLOR_WHITE]);
    PyObject *black = PyLong_FromUnsignedLongLong(self->pos.occupancy[COLOR_BLACK]);
    if (!white || !black) {
        Py_XDECREF(white);
        Py_XDECREF(black);
        return NULL;
    }
    PyObject *result = PyTuple_New(2);
    if (!result) {
        Py_DECREF(white);
        Py_DECREF(black);
        return NULL;
    }
    PyTuple_SET_ITEM(result, 0, white);
    PyTuple_SET_ITEM(result, 1, black);
    return result;
}

static PyObject *Position_all(PositionObject *self, PyObject *Py_UNUSED(ignored)) {
    return PyLong_FromUnsignedLongLong(self->pos.all);
}

static PyObject *Position_get_side_to_move(PositionObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->pos.side_to_move);
}

static PyObject *Position_get_castling(PositionObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(self->pos.castling_rights);
}

static PyObject *Position_get_en_passant(PositionObject *self, void *closure) {
    (void)closure;
    if (self->pos.en_passant_square < 0) {
        Py_RETURN_NONE;
    }
    return PyLong_FromLong(self->pos.en_passant_square);
}

static PyObject *Position_get_ply(PositionObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(self->pos.ply);
}

static PyObject *Position_get_halfmove(PositionObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(self->pos.halfmove_clock);
}

static PyObject *Position_get_fullmove(PositionObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(self->pos.fullmove_number);
}

static PyObject *Position_get_stack_length(PositionObject *self, void *closure) {
    (void)closure;
    return PyLong_FromSsize_t(self->history_size);
}

static PyMethodDef Position_methods[] = {
    {"set_fen", (PyCFunction)Position_set_fen, METH_VARARGS, "Load the position from a FEN string."},
    {"set_start", (PyCFunction)Position_set_start, METH_NOARGS, "Reset to the standard chess initial position."},
    {"fen", (PyCFunction)Position_fen, METH_NOARGS, "Return the FEN string for the current position."},
    {"board", (PyCFunction)Position_board, METH_NOARGS, "Return an ASCII representation of the board."},
    {"generate_legal_moves", (PyCFunction)Position_generate_legal_moves, METH_VARARGS | METH_KEYWORDS, "Return legal moves as tuples or UCI strings."},
    {"legal_move_count", (PyCFunction)Position_legal_move_count, METH_NOARGS, "Return the number of legal moves."},
    {"has_legal_moves", (PyCFunction)Position_has_legal_moves_py, METH_NOARGS, "Return True if there is at least one legal move."},
    {"in_check", (PyCFunction)Position_in_check, METH_VARARGS, "Return True if the given side (default: side to move) is in check."},
    {"is_checkmate", (PyCFunction)Position_is_checkmate, METH_NOARGS, "Return True if the current player is checkmated."},
    {"is_stalemate", (PyCFunction)Position_is_stalemate, METH_NOARGS, "Return True if the current player is stalemated."},
    {"is_insufficient_material", (PyCFunction)Position_is_insufficient_material_py, METH_NOARGS, "Return True if the position is a dead draw by insufficient material."},
    {"is_game_over", (PyCFunction)Position_is_game_over, METH_VARARGS | METH_KEYWORDS, "Return True if the game is over (optionally considering draw claims)."},
    {"result", (PyCFunction)Position_result, METH_VARARGS | METH_KEYWORDS, "Return the game result string ('1-0', '0-1', '1/2-1/2', or '*')."},
    {"make_move", (PyCFunction)Position_make_move, METH_VARARGS, "Apply a legal move given as tuple or UCI string."},
    {"push", (PyCFunction)Position_push, METH_VARARGS, "Push a legal move onto the board and store it on the history stack."},
    {"pop", (PyCFunction)Position_pop, METH_NOARGS, "Undo the last pushed move and return it."},
    {"peek", (PyCFunction)Position_peek, METH_NOARGS, "Return the last pushed move without undoing it."},
    {"clear_stack", (PyCFunction)Position_clear_stack, METH_NOARGS, "Clear the internal move history stack."},
    {"stack_size", (PyCFunction)Position_stack_size, METH_NOARGS, "Return the length of the move history stack."},
    {"to_tensor", (PyCFunction)Position_to_tensor, METH_VARARGS | METH_KEYWORDS, "Return a 15x8x8 uint8 tensor (optionally writing into a provided buffer)."},
    {"clone", (PyCFunction)Position_clone, METH_NOARGS, "Return a deep copy of the position."},
    {"bitboards", (PyCFunction)Position_bitboards, METH_NOARGS, "Return per-color piece bitboards."},
    {"occupancy", (PyCFunction)Position_occupancy, METH_NOARGS, "Return occupancy bitboards for white and black."},
    {"all", (PyCFunction)Position_all, METH_NOARGS, "Return the combined occupancy bitboard."},
    {NULL, NULL, 0, NULL}
};

static PyGetSetDef Position_getset[] = {
    {"side_to_move", (getter)Position_get_side_to_move, NULL, "Side to move (0=white, 1=black).", NULL},
    {"castling_rights", (getter)Position_get_castling, NULL, "Castling rights bitmask.", NULL},
    {"en_passant_square", (getter)Position_get_en_passant, NULL, "En passant square index or None.", NULL},
    {"ply", (getter)Position_get_ply, NULL, "Number of plies played.", NULL},
    {"halfmove_clock", (getter)Position_get_halfmove, NULL, "Halfmove clock for the fifty-move rule.", NULL},
    {"fullmove_number", (getter)Position_get_fullmove, NULL, "Fullmove number (starts at 1).", NULL},
    {"stack_depth", (getter)Position_get_stack_length, NULL, "Number of moves stored on the push/pop stack.", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyTypeObject PositionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tensor_chess.Position",
    .tp_basicsize = sizeof(PositionObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)Position_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Position_new,
    .tp_init = (initproc)Position_init,
    .tp_methods = Position_methods,
    .tp_getset = Position_getset,
};

static PyMethodDef module_methods[] = {
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef tensor_chess_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_tensor_chess",
    .m_doc = "High-performance chess move generator exposed to Python.",
    .m_size = -1,
    .m_methods = module_methods,
};

PyMODINIT_FUNC PyInit__tensor_chess(void) {
    if (PyType_Ready(&PositionType) < 0) {
        return NULL;
    }

    chess_init_tables();

    PyObject *mod = PyModule_Create(&tensor_chess_module);
    if (!mod) {
        return NULL;
    }

    Py_INCREF(&PositionType);
    if (PyModule_AddObject(mod, "Position", (PyObject *)&PositionType) < 0) {
        Py_DECREF(&PositionType);
        Py_DECREF(mod);
        return NULL;
    }

    PyModule_AddIntConstant(mod, "COLOR_WHITE", COLOR_WHITE);
    PyModule_AddIntConstant(mod, "COLOR_BLACK", COLOR_BLACK);
    PyModule_AddIntConstant(mod, "PIECE_PAWN", PIECE_PAWN);
    PyModule_AddIntConstant(mod, "PIECE_KNIGHT", PIECE_KNIGHT);
    PyModule_AddIntConstant(mod, "PIECE_BISHOP", PIECE_BISHOP);
    PyModule_AddIntConstant(mod, "PIECE_ROOK", PIECE_ROOK);
    PyModule_AddIntConstant(mod, "PIECE_QUEEN", PIECE_QUEEN);
    PyModule_AddIntConstant(mod, "PIECE_KING", PIECE_KING);
    PyModule_AddIntConstant(mod, "MOVE_FLAG_CAPTURE", MOVE_FLAG_CAPTURE);
    PyModule_AddIntConstant(mod, "MOVE_FLAG_PROMOTION", MOVE_FLAG_PROMOTION);
    PyModule_AddIntConstant(mod, "MOVE_FLAG_EN_PASSANT", MOVE_FLAG_EN_PASSANT);
    PyModule_AddIntConstant(mod, "MOVE_FLAG_DOUBLE_PAWN", MOVE_FLAG_DOUBLE_PAWN);
    PyModule_AddIntConstant(mod, "MOVE_FLAG_CASTLE", MOVE_FLAG_CASTLE);

    PyObject *all = Py_BuildValue("(s)", "Position");
    if (!all) {
        Py_DECREF(&PositionType);
        Py_DECREF(mod);
        return NULL;
    }
    if (PyModule_AddObject(mod, "__all__", all) < 0) {
        Py_DECREF(all);
        Py_DECREF(&PositionType);
        Py_DECREF(mod);
        return NULL;
    }

    return mod;
}
