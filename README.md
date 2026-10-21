# CLCENnetPy v1.2

**A pure-Python NNUE chess engine.** It uses only the Python standard library, with no NumPy and no pip installs. Run it under PyPy3 and it plays at roughly **2600 Elo** at 5+3 on a single core.

Author: **K7** (K7 ChessLab Collection)

CLCENnetPy is a Python port of **K7CustomNetV2** (C), plus a few cheap improvements from V3. It plays with K7's own neural networks (K7NN format, trained with `k7_train`) and speaks UCI, so it works in Arena, KULTRA, Cute Chess, Banksia and other UCI GUIs.

---

## Contents

| File | What it is |
|---|---|
| `CLCENnetPy.py` | The engine (single file) |
| `k7net26k.k7nn` | The default network |
| `book.txt` | Opening book (688 lines, SAN text) |
| `pypy/` | PyPy3 for Windows, included and ready to use |
| `CLCENnetPy.c` | Optional launcher source, builds `CLCENnetPy.exe` |
| `pgn/` | Test games used for the rating estimate below |

---

## Quick start

### Option 1 — PyPy3 (recommended)

PyPy3 for Windows is **already included** in the `pypy` folder, so there is nothing to install just extracted from the zip make sure folder is call pypy.

1. Open a command window in the engine folder.
2. Run:
   ```
   pypy\pypy3.exe CLCENnetPy.py
   ```
3. Type `uci`, then `isready`. You should see:
   ```
   info string NetFile: loaded k7net26k.k7nn (768 -> ...x2 -> 1, ... bytes)
   ```

### Option 2 — Launcher EXE (for Arena / KULTRA)

GUIs want an `.exe`. The launcher starts PyPy with the script and passes the UCI text straight through.

```
gcc -O2 -s -static -municode -o CLCENnetPy.exe CLCENnetPy.c
```

Folder layout:

```
CLCENnetPy\
  CLCENnetPy.exe      <- point the GUI here
  CLCENnetPy.py
  k7net26k.k7nn
  book.txt
  pypy\pypy3.exe ...
```

The launcher finds the script by its own name, so a renamed copy (`MyEngine.exe`) runs `MyEngine.py`. It also uses a Windows Job Object, so PyPy and any helper processes close when the GUI closes the engine. An optional `CLCENnetPy.ini` can override the paths:

```
interpreter=pypy\pypy3.exe
script=CLCENnetPy.py
args=
```

> **Note:** Arena's status bar shows the launcher's own memory (about 4 MB), not the engine's hash. Your Hash setting still applies.

### Option 3 — Regular Python

`python CLCENnetPy.py` works, but without PyPy's JIT it is about 5–10 times slower and roughly **400–500 Elo weaker** (about 2100–2200).

---

## UCI options

| Option | Default | Description |
|---|---|---|
| `Hash` | 64 | Hash table size in MB |
| `Processes` | 1 | 1–4 PyPy processes searching together (Lazy SMP). Arena moves an option named `Threads` into its own CPU setting, so it is called `Processes` here; `Threads` is still accepted. |
| `NetFile` | `k7net26k.k7nn` | Any K7NN v1 net (`.bin` / `.k7nn`), any hidden size |
| `OwnBook` | true | Use `book.txt` for the opening |
| `BookFile` | `book.txt` | Opening book file |
| `RFP` | true | Reverse futility pruning |
| `NullMove+` | true | Adaptive null-move pruning |
| `Futility` | true | Futility pruning of quiet moves |
| `LMP` | true | Late move pruning |
| `LMR+` | true | Log-table late move reductions |
| `SEE` | true | Static exchange evaluation (ordering + pruning) |
| `Delta` | false | Delta pruning in quiescence (tested −40 Elo in C, off) |
| `IIR` | true | Internal iterative reduction |
| `Ordering+` | true | Counter-moves + history gravity/malus |
| `QSearchTT` | true | Hash table in quiescence (from V3) |
| `TTEval` | true | Hash score refines the pruning eval (from V3) |
| `Fifty` | true | Eval shrinks as the 50-move counter rises (from V3) |

Turning every search box off gives the original V1 search.

### Console commands

| Command | Description |
|---|---|
| `perft N` | Move-generator test (`perft 5` from the start = 4865609) |
| `bench [depth]` | Fixed-depth search of 14 positions; prints nodes and nps |
| `eval` | Net score of the current position |
| `go nodes N` | Fixed-node search |

---

## How it works

### Board and search
- 10x12 mailbox board with sentinel squares, 32-bit packed moves.
- PVS alpha-beta with iterative deepening and aspiration windows.
- Null move, LMR, LMP, RFP, futility, SEE pruning, IIR, check extensions, mate-distance pruning.
- Killers, history with gravity/malus, counter-moves, MVV/LVA + SEE ordering.
- Transposition table with aging; draw detection (repetition, 50-move rule, insufficient material).

### Network (K7NN v1)
```
768 inputs -> H x 2 (SCReLU) -> 1
```
- Features: colour × piece type × square, from each side's point of view (board flipped for Black).
- **Incremental accumulators.** The accumulator is built once at the root, and every move below it only adds and subtracts a few weight rows. Kings are ordinary features, so king moves never need a full refresh. This is what makes a net practical in Python.
- Eval cache keyed by the position hash.
- If no net loads, the engine says so and plays on material only.

### Lazy SMP with processes (`Processes` 2–4)
Python threads cannot search in parallel (the GIL), so CLCENnetPy starts extra **PyPy processes** instead:
- All processes share one hash table in shared memory (lockless, XOR-checked 64-bit entries, as in K7CustomNetV3).
- Helpers start once and stay alive between moves, so the JIT warm-up happens only once.
- Only the main process talks to the GUI. It reports the **total** nodes and nps of all processes, its own PV, and the bestmove.
- Expect about 1–2 extra plies with 4 processes (roughly +50–100 Elo). Give the opponent the same number of threads in a fair test.

### Opening book
`book.txt` holds one game per line in SAN from the start position, for example:
```
d4 Nf6 c4 e6 Nf3 b6 g3 Ba6 ...
```
Lines starting with `#` are comments. Each move, the engine keeps the lines that match the game so far, picks one **at random** and plays its next move, so moves that appear in more lines are played more often. Once the game leaves the book it stops looking until the next `ucinewgame`. A lookup takes about 0.02 s. The book is used only from the standard start position.

---

## Version history

| Version | Changes |
|---|---|
| **v1.0** | Python port of K7CustomNetV2: full V2 search, K7NN nets with incremental accumulators, UCI, `perft`/`bench`/`eval`. Cheap V3 extras: QSearchTT, TTEval, Fifty scaling, TT aging, the PV fix (no stale PV after TT cutoffs), and the root accumulator built once per search. |
| **v1.1** | Lazy SMP: `Processes` 1–4 with a shared-memory hash table, combined node/nps reporting and helper diagnostics (`CLCENnetPy_helpers.log`). The option is named `Processes` because Arena hides `Threads`. New C launcher `CLCENnetPy.c`. |
| **v1.2** | Opening book: `OwnBook` and `BookFile` options, random line selection from `book.txt`. Default net is now `k7net26k.k7nn`. |

---

## Strength

All games at **5 min + 3 s**, CLCENnetPy on **PyPy3, 1 process**, single-threaded opponents, Intel i9-9900K. Ratings fitted by maximum likelihood with the anchors fixed.

### Results against anchors

| Opponent | Anchor rating | Games | W | D | L | Score |
|---|---|---|---|---|---|---|
| Pawny 1.1 | 2700 (CCRL) | 15 | 7 | 2 | 6 | 53% |
| CLMaster64 (Greko 2018 derivative) | ~2643 | 15 | 3 | 3 | 9 | 30% |
| TheKing 3.50 (original) | 2700 (listed) | 15 | 0 | 2 | 13 | 7% |

### Estimate

| Anchors used | Games | Rating | 95% range |
|---|---|---|---|
| Pawny + CLMaster | 30 | **~2610** | 2480–2740 |
| Pawny + CLMaster + TheKing 3.50 | 45 | ~2530 | 2420–2640 |

**Best estimate: about 2600 ± 100** (5+3, PyPy3, 1 process).

TheKing 3.50 clearly plays above its listed 2700 on modern hardware (Pawny and TheKing can't both be 2700 when CLCENnetPy scores 53% against one and 7% against the other), which is why the two-anchor figure is preferred.

### Other test results (not used in the rating)
- **CLNeptune64** (Greko 2020 derivative) was left out: that build showed 0.00 evals even at depth 22 and lost every game in 17–28 moves, a sign of a broken eval.
- **TheKing350 UCI legacy** (K7's UCI port of TheKing 3.50) scored about 75% against CLCENnetPy, close to the original.
- At 120+6: beat CLVeloci (~2600), K7Homebrew v5.1 (~2350) and CLK7HomebrewPy; drew TheKing350 UCI twice; lost to CL28KELO (~2800).

### Where the limits are
- At 120+6 it reaches about **depth 13–14** in the middlegame (15–17 with 4 processes). Engines that see 18+ plies can win with ideas past that horizon.
- Net size matters in Python: **256–512 hidden** nets give the best balance. 768 and 1024 nets judge positions better but lose too much depth.
- The same engine on regular Python reaches only depth 8–11.

These ratings come from K7's own testing on one machine. More games will narrow the range, and independent testing is welcome.

---

## Requirements

- Nothing to install on Windows: PyPy3 is included in the `pypy` folder just extracted from zip file. (Python 3.8+ also works, but much slower.)
- Standard library only.
- Optional: MinGW64 `gcc` to build the launcher.
- also if wanna just play the standalone without pypy or installing any python software, and straight into arena the engine is compile already into exe in 2100 elo engine folder but you have point out the net for it in the engine configuration either k726k.k7nn or k7net.bin which is also in the 2100 elo engine folder
## License / credits

CLCENnetPy and the K7NN networks by **K7**, K7 ChessLab Collection.
