
from __future__ import annotations

try:
    from ._tensor_chess import *
except ModuleNotFoundError as exc:
    raise ImportError(
        "tensor_chess was imported without its native extension. Install the project (e.g. `pip install .`) to build the module."
    ) from exc

try:
    from ._tensor_chess import __all__ as _native_all
except ImportError:
    _native_all = ()

__all__ = tuple(_native_all)
