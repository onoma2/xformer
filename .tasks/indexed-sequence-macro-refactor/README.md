# IndexedSequence Macro Refactor

## Goal

Extract ~700 lines of macro logic from `IndexedSequenceEditPage.cpp` (UI layer) into `IndexedSequence` model methods, so macros can be invoked from both the LCD edit page and the Launchpad controller.

**Why this matters:** The Launchpad Macro Grid v2 plan requires IndexedSequence macros (20+ batch operations). Currently these macros are trapped in UI code and cannot be called from `LaunchpadController.cpp`. Duplicating them into the controller would create a maintenance nightmare. The correct fix is to move them into the model.

**Blocks:** `launchpad-track-port` Phase 4 (Macro Grid v2 for Indexed).

---

## Key Files

| File | Role |
|------|------|
| `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp:1605-2304` | ~700 lines of macro UI logic to extract |
| `src/apps/sequencer/model/IndexedSequence.h` | Destination for new macro methods |
| `src/apps/sequencer/model/IndexedSequence.cpp` | Implementation file (if created; currently header-only) |
| `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp` | Must be updated to call model methods instead of inline logic |

---

## Macro Inventory

From `IndexedSequenceEditPage.cpp`, the following macros need model methods:

### Rhythm Macros (6)
| Name | Description |
|------|-------------|
| `3/9` | Euclidean 3 or 9 hits |
| `5/20` | Clave 5 or 20 |
| `7/28` | Tuplet 7 or 28 |
| `3-5` | Polyrhythm 3 against 5 |
| `5-7` | Polyrhythm 5 against 7 |
| `M-TALA` | Indian tala (Khand/Tihai/Dhamar) |

### Waveform Macros (5)
| Name | Description |
|------|-------------|
| `TRI` | Triangle wave durations |
| `2TRI` | 2-cycle triangle |
| `3TRI` | 3-cycle triangle |
| `2SAW` | 2-cycle sawtooth |
| `3SAW` | 3-cycle sawtooth |

### Melodic Macros (5)
| Name | Description |
|------|-------------|
| `SCALE` | Fill notes from scale |
| `ARP` | Arpeggio pattern |
| `CHORD` | Chord voicings |
| `MODAL` | Modal pattern |
| `M-MEL` | Melodic pattern |

### Duration Macros (4)
| Name | Description |
|------|-------------|
| `D-LOG` | Logarithmic duration shaping |
| `D-EXP` | Exponential duration shaping |
| `Q-MEAS` | Quantize durations to measure |
| `REV` | Reverse step order |
| `MIRR` | Mirror duration values |

**Total: 20 macros** (some sources count 18/21 depending on how sub-variants are grouped).

---

## Proposed API

```cpp
// In IndexedSequence.h

enum class Macro {
    Euclidean3_9,
    Clave5_20,
    Tuplet7_28,
    Polyrhythm3_5,
    Polyrhythm5_7,
    Tala,
    Triangle,
    Triangle2,
    Triangle3,
    Sawtooth2,
    Sawtooth3,
    ScaleFill,
    Arpeggio,
    Chord,
    Modal,
    Melodic,
    DurationLog,
    DurationExp,
    DurationQuantize,
    Reverse,
    Mirror,
    Last
};

void applyMacro(Macro macro);
void applyMacro(Macro macro, int firstStep, int lastStep);
```

Alternative: individual named methods (matches `CurveSequence` pattern):

```cpp
void populateWithMacroEuclidean(int firstStep, int lastStep, int hits);
void populateWithMacroClave(int firstStep, int lastStep);
void populateWithMacroTriangle(int firstStep, int lastStep, int cycles);
void populateWithMacroScaleFill(int firstStep, int lastStep);
void transformReverse(int firstStep, int lastStep);
void transformMirror(int firstStep, int lastStep);
// ... etc
```

**Recommendation:** Use individual named methods (like `CurveSequence::populateWithMacroInit()`, `transformInvert()`, etc.) for consistency with existing codebase patterns.

---

## Open Questions

- [ ] Should macros operate on `0..activeLength()-1` or accept `firstStep`/`lastStep` range like CurveSequence?
- [ ] Do any macros need the project `Scale` context (e.g. `SCALE`, `ARP`, `CHORD`)?
- [ ] Should `IndexedSequence.cpp` be created (currently header-only) or keep everything in `.h`?

---

## Effort Estimate

| Task | Effort |
|------|--------|
| Extract and catalog all macro logic from `IndexedSequenceEditPage.cpp` | ~30 min |
| Design model API (individual methods vs. enum dispatch) | ~15 min |
| Implement 20 macro methods in `IndexedSequence` | ~2.5h |
| Update `IndexedSequenceEditPage.cpp` to call model methods | ~45 min |
| Test: verify LCD UI still works after refactor | ~30 min |
| Add `isEdited()` (safe, no serialization impact) | ~15 min |

**Total: ~4.5h**

---

## Status

⚪ Ready — not started. Independent of Launchpad track port; can be done in parallel or before Phase 4.

## Next Action

Read `IndexedSequenceEditPage.cpp` lines 1605-2304, catalog macro functions, design API.
