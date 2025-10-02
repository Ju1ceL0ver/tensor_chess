
# tensor_chess API Reference / Полный справочник API

## English

### Importing the Package
- `from tensor_chess import Position`
  - Imports the compiled extension and exposes the `Position` type plus constants listed below.
  - The package raises an informative `ImportError` if the native module has not been built (install the project with `pip install tensor-chess` or `pip install .`).

### Module-Level Constants
- Colors: `COLOR_WHITE = 0`, `COLOR_BLACK = 1`.
- Pieces: `PIECE_PAWN`, `PIECE_KNIGHT`, `PIECE_BISHOP`, `PIECE_ROOK`, `PIECE_QUEEN`, `PIECE_KING`.
- Move flags (bitmask combined in the `flags` field of move tuples):
  - `MOVE_FLAG_CAPTURE`
  - `MOVE_FLAG_PROMOTION`
  - `MOVE_FLAG_EN_PASSANT`
  - `MOVE_FLAG_DOUBLE_PAWN`
  - `MOVE_FLAG_CASTLE`
- Castling rights (reported by `castling_rights`):
  - `CASTLE_WHITE_KING`, `CASTLE_WHITE_QUEEN`, `CASTLE_BLACK_KING`, `CASTLE_BLACK_QUEEN`.

### Move Tuple Format
- Moves are represented as `(from, to, flags, promotion)`.
- `from`, `to`: 0–63 board indices (`0` for a1, `63` for h8`).
- `flags`: combination of `MOVE_FLAG_*` bits.
- `promotion`: one of the `PIECE_*` constants when promotion is present; otherwise `0`.

### `tensor_chess.Position`
Constructor: `Position(fen: str | None = None)`.
- `None` (default) → standard initial position.
- Invalid FEN raises `ValueError`.

#### Read-only Properties
- `side_to_move`: whose turn it is (`0` for white, `1` for black).
- `castling_rights`: bitmask of available castling.
- `en_passant_square`: `None` or the capture square index (0–63).
- `ply`: number of plies since the start position.
- `halfmove_clock`: number of half-moves for the fifty-move rule.
- `fullmove_number`: full-move count (starts at 1).
- `stack_depth`: size of the push/pop history.

#### Core Methods
- `set_fen(fen: str) -> None`: load a new position (history is cleared).
- `set_start() -> None`: reset to the initial chess position.
- `fen() -> str`: export the current FEN string.
- `board() -> str`: ASCII board view with uppercase white pieces.
- `clone() -> Position`: deep copy of the position.

#### Move Handling
- `generate_legal_moves(*, as_strings: bool = False) -> list`
  - Returns move tuples or UCI strings when `as_strings=True`.
- `legal_move_count() -> int`
- `has_legal_moves() -> bool`
- `make_move(move) -> None`: apply a legal move without storing history.
- `push(move) -> tuple`: apply a legal move, store it on the internal stack, and return the canonical tuple.
- `pop() -> tuple`: undo the last pushed move (raises `IndexError` if the stack is empty).
- `peek() -> tuple`: inspect the last pushed move without undoing it.
- `clear_stack() -> None`
- `stack_size() -> int`

#### Game-State Queries
- `in_check(color: int | None = None) -> bool`
- `is_checkmate() -> bool`
- `is_stalemate() -> bool`
- `is_insufficient_material() -> bool`
- `is_game_over(*, claim_draw: bool = False) -> bool`
  - Considers checkmate, stalemate, insufficient material, and (optionally) the fifty-move rule via `claim_draw=True`.
- `result(*, claim_draw: bool = False) -> str`
  - Returns `'1-0'`, `'0-1'`, `'1/2-1/2'`, or `'*'`.

#### Bitboards and Tensor Export
- `bitboards() -> ((int, ...), (int, ...))`
  - Two tuples of six bitboards (white and black pieces in order pawn→king).
- `occupancy() -> (int, int)`
- `all() -> int`
- `to_tensor(out: buffer | None = None) -> bytes | buffer`
  - Produces 15 planes × 8 × 8 bytes in the following order:
    1–6 white pieces, 7–12 black pieces, 13 side-to-move plane, 14 castling plane, 15 en-passant plane.
  - When `out` is provided, it must be a writable contiguous buffer of length `960`.

### Exceptions
- `ValueError`: invalid FEN, illegal move, buffer size mismatch.
- `TypeError`: incorrect argument types.
- `IndexError`: history underflow (`pop`/`peek` with empty stack).
- `RuntimeError`: internal FEN formatting errors.

### Example
```python
from tensor_chess import Position

pos = Position()
line = ("e2e4", "e7e5", "g1f3", "b8c6")
for mv in line:
    pos.push(mv)

print(pos.board())
print("Game over?", pos.is_game_over(claim_draw=True))
tensor = pos.to_tensor()
print(len(tensor))  # 960 bytes
```

## Русский

### Импорт пакета
- `from tensor_chess import Position`
  - Импортирует скомпилированное расширение и предоставляет тип `Position` и константы.
  - При отсутствии собранного модуля будет выброшен информативный `ImportError` (установите пакет командой `pip install tensor-chess` или `pip install .`).

### Константы
- Цвета: `COLOR_WHITE = 0`, `COLOR_BLACK = 1`.
- Фигуры: `PIECE_PAWN`, `PIECE_KNIGHT`, `PIECE_BISHOP`, `PIECE_ROOK`, `PIECE_QUEEN`, `PIECE_KING`.
- Флаги хода: `MOVE_FLAG_CAPTURE`, `MOVE_FLAG_PROMOTION`, `MOVE_FLAG_EN_PASSANT`, `MOVE_FLAG_DOUBLE_PAWN`, `MOVE_FLAG_CASTLE`.
- Права на рокировку: `CASTLE_WHITE_KING`, `CASTLE_WHITE_QUEEN`, `CASTLE_BLACK_KING`, `CASTLE_BLACK_QUEEN`.

### Формат хода
- `(from, to, flags, promotion)` с индексами 0–63 и кодами `PIECE_*`.

### `tensor_chess.Position`
Конструктор: `Position(fen: str | None = None)`.
- Без аргументов — стартовая позиция.
- Неверный FEN → `ValueError`.

#### Свойства
- `side_to_move`, `castling_rights`, `en_passant_square`, `ply`, `halfmove_clock`, `fullmove_number`, `stack_depth`.

#### Основные методы
- `set_fen(fen)`, `set_start()`, `fen()`, `board()`, `clone()`.

#### Работа с ходами
- `generate_legal_moves(as_strings=False)`.
- `legal_move_count()`, `has_legal_moves()`.
- `make_move(move)`, `push(move)`, `pop()`, `peek()`, `clear_stack()`, `stack_size()`.

#### Состояние партии
- `in_check(color=None)`, `is_checkmate()`, `is_stalemate()`, `is_insufficient_material()`.
- `is_game_over(claim_draw=False)` — учитывает 50 ходов при `claim_draw=True`.
- `result(claim_draw=False)` — `'1-0'`, `'0-1'`, `'1/2-1/2'` или `'*'`.

#### Битборды и тензоры
- `bitboards()`, `occupancy()`, `all()`.
- `to_tensor(out=None)` — 15 каналов по 64 байта (порядок: белые фигуры, черные фигуры, ходящая сторона, рокировка, взятие на проходе).

### Исключения
- `ValueError`, `TypeError`, `IndexError`, `RuntimeError`.

### Пример
```python
from tensor_chess import Position

pos = Position()
while not pos.is_game_over(claim_draw=True):
    mv = input("Ход (uci) > ")
    try:
        pos.push(mv)
    except Exception as exc:
        print("Нелегальный ход:", exc)
        continue
    print(pos.board())
print("Результат:", pos.result(claim_draw=True))
```
