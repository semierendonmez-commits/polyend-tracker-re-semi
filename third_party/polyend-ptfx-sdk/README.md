# Polyend .ptfx Sample Editor plugin SDK (vendored)

`tracker_plugin.h`, `ptfx_builder.py` and `effects/*.c` are copied unchanged
from `editor-fx-sdk/` of [polyend/tracker-ce](https://github.com/polyend/tracker-ce)
(commit 9b9a13b, CE 1.0.0). That directory is MIT licensed (see `LICENSE`,
Part 3 of tracker-ce's LICENSE); the rest of tracker-ce is not, and none of it
is used here.

Used for:
- the plugin format (`src/core/app/ptfx.cpp` reads it; the code there is our own);
- building the fork's effects as `.ptfx` (`ptfx/`, `make ptfx`);
- host tests that run the reference effects through the fork's loader.
