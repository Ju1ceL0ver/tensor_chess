
# tensor_chess

Optimized chess position engine with Python bindings and tensor export designed for fast reinforcement-learning and analytics workflows.

## Features
- High-performance C move generator exposed as `tensor_chess.Position`.
- Push/pop stack for reversible play, legal move generation, state inspection, and FEN export.
- Direct tensor export to 15×8×8 uint8 planes ready for ML pipelines.
- Dead-draw detection, result reporting, and bitboard access for advanced heuristics.

## Installation
```bash
pip install tensor-chess
```

## Quick Start
```python
from tensor_chess import Position

pos = Position()
pos.push("e2e4")
pos.push("e7e5")
tensor = pos.to_tensor()

print(pos.board())
print(len(tensor))  # 960 bytes
```

## Documentation
See `docs/API_REFERENCE.md` for the full API.
