# Teletype I/O Page UI Design

**Date:** 2025-01-21
**Status:** Design Draft
**Branch:** t9type

## Overview

Matrix-style UI for configuring Teletype track I/O routing. Two-panel layout with grouped columns showing source/destination assignments at a glance.

## Data to Display

### Inputs (TI-* variables)
- **TI-TR1 to TI-TR4** - 4 trigger inputs
  - Sources: CvIn1-4, GateOut1-8, LogicalGate1-8, None
- **TI-IN** - CV input for Teletype IN variable
- **TI-PARAM** - CV input for Teletype PARAM variable
- **TI-X, TI-Y, TI-Z, TI-T** - CV inputs for X, Y, Z, T variables
  - Sources: CvIn1-4, CvOut1-8, CvRoute1-4, LogicalCv1-8, None

### Outputs (TO-* variables)
- **TO-TRA to TO-TRD** - 4 trigger outputs → GateOut1-8
- **TO-CV1 to TO-CV4** - 4 CV outputs → CvOut1-8
  - Per-output settings: Range, Offset, Quantize Scale, Root Note

## Layout Structure

**Display:** 256×64 pixels, split into two panels (128px each)

```
┌─────────── INPUTS (128px) ──────────┐┌─────────── OUTPUTS (128px) ─────────┐
│ Header: "INPUTS" (8px)              ││ Header: "OUTPUTS" (8px)             │
│                                     ││                                     │
│ TRIGGER SECTION (24px, 4 rows)      ││ TRIGGER SECTION (24px, 4 rows)      │
│ ┌──┬───┬───┬───┐                    ││ ┌──┬─────┐                          │
│ │  │ C │ G │ L │  ← Source groups   ││ │  │ →G  │  ← Destination           │
│ │T1│ 3 │   │   │                    ││ │TA│  1  │                          │
│ │T2│   │ 1 │   │                    ││ │TB│  2  │                          │
│ │T3│   │ 6 │   │                    ││ │TC│  3  │                          │
│ │T4│ 4 │   │   │                    ││ │TD│  4  │                          │
│ └──┴───┴───┴───┘                    ││ └──┴─────┘                          │
│                                     ││                                     │
│ CV SECTION (24px, 4-6 rows)         ││ CV SECTION (24px, 4 rows)           │
│ ┌──┬───┬───┬───┬───┐                ││ ┌──┬─────┐                          │
│ │  │ C │ O │ R │ L │                ││ │  │ →O  │                          │
│ │IN│ 1 │   │   │   │                ││ │C1│  1  │                          │
│ │PA│ 2 │   │   │   │                ││ │C2│  2  │                          │
│ │ X│   │ 3 │   │   │                ││ │C3│  3  │                          │
│ │ Y│   │   │ 1 │   │                ││ │C4│  4  │                          │
│ │Z:C4  T:L1       │ ← compact row   ││ └──┴─────┘                          │
│ └──┴───┴───┴───┴───┘                ││                                     │
│                                     ││                                     │
│ Footer (6px)                        ││ Footer (6px)                        │
└─────────────────────────────────────┘└─────────────────────────────────────┘
```

## Column Group Abbreviations

| Abbrev | Full Name | Range | Used For |
|--------|-----------|-------|----------|
| **C** | CV Inputs | 1-4 | CvIn1-CvIn4 |
| **G** | Gate Outputs | 1-8 | GateOut1-GateOut8 |
| **L** | Logical | 1-8 | LogicalGate/LogicalCv (track outputs) |
| **O** | CV Outputs | 1-8 | CvOut1-CvOut8 (readback) |
| **R** | CV Router | 1-4 | CvRoute1-CvRoute4 |
| **→G** | Gate Dest | 1-8 | Output destination |
| **→O** | CV Dest | 1-8 | Output destination |

## Pixel Coordinates

### Global Layout

```
X:  0                              128                            256
    │                               │                              │
Y=0 ┌───────── LEFT PANEL ──────────┬──────── RIGHT PANEL ─────────┐
Y=2 │ "INPUTS"                      │ "OUTPUTS"                    │
Y=8 ├───────────────────────────────┼──────────────────────────────┤
    │ Trigger Input Matrix          │ Trigger Output List          │
Y=32├───────────────────────────────┼──────────────────────────────┤
    │ CV Input Matrix               │ CV Output List               │
Y=58├───────────────────────────────┴──────────────────────────────┤
    │ Footer: [F1] [F2] [F3] [F4] [F5]                             │
Y=64└──────────────────────────────────────────────────────────────┘
```

### LEFT PANEL: INPUTS (X: 0-127)

**Header:**
- X: 4, Y: 2, Text: "INPUTS", Font: Tiny, Color: Medium

**Trigger Input Section (Y: 8-31)**

| Element | X | Y | Width | Height |
|---------|---|---|-------|--------|
| Header "C" | 24 | 8 | 20 | 6 |
| Header "G" | 48 | 8 | 20 | 6 |
| Header "L" | 72 | 8 | 20 | 6 |
| Label "T1" | 4 | 14 | 16 | 6 |
| Label "T2" | 4 | 20 | 16 | 6 |
| Label "T3" | 4 | 26 | 16 | 6 |
| Label "T4" | 4 | 32 | 16 | 6 |

**Cell Grid (Trigger Inputs):**
```
        C(X=24) G(X=48) L(X=72)
T1(Y=14) [24,14] [48,14] [72,14]
T2(Y=20) [24,20] [48,20] [72,20]
T3(Y=26) [24,26] [48,26] [72,26]
T4(Y=32) [24,32] [48,32] [72,32]
```
- Cell width: 20px
- Cell height: 6px
- Content: Number (1-8) or empty

**CV Input Section (Y: 34-57)**

| Element | X | Y | Width | Height |
|---------|---|---|-------|--------|
| Header "C" | 20 | 34 | 18 | 6 |
| Header "O" | 40 | 34 | 18 | 6 |
| Header "R" | 60 | 34 | 18 | 6 |
| Header "L" | 80 | 34 | 18 | 6 |
| Label "IN" | 2 | 40 | 14 | 5 |
| Label "PA" | 2 | 45 | 14 | 5 |
| Label "X" | 2 | 50 | 14 | 5 |
| Label "Y" | 2 | 55 | 14 | 5 |
| Compact "Z:__ T:__" | 2 | 60 | 96 | 5 |

**Cell Grid (CV Inputs):**
```
        C(X=20) O(X=40) R(X=60) L(X=80)
IN(Y=40) [20,40] [40,40] [60,40] [80,40]
PA(Y=45) [20,45] [40,45] [60,45] [80,45]
 X(Y=50) [20,50] [40,50] [60,50] [80,50]
 Y(Y=55) [20,55] [40,55] [60,55] [80,55]
```
- Cell width: 18px
- Cell height: 5px
- Z and T shown as compact inline row at Y=60

### RIGHT PANEL: OUTPUTS (X: 128-255)

**Header:**
- X: 132, Y: 2, Text: "OUTPUTS", Font: Tiny, Color: Medium

**Trigger Output Section (Y: 8-31)**

| Element | X | Y | Width | Height |
|---------|---|---|-------|--------|
| Header "→G" | 160 | 8 | 24 | 6 |
| Label "TA" | 132 | 14 | 20 | 6 |
| Label "TB" | 132 | 20 | 20 | 6 |
| Label "TC" | 132 | 26 | 20 | 6 |
| Label "TD" | 132 | 32 | 20 | 6 |
| Values | 160 | 14-32 | 24 | 6 each |

**CV Output Section (Y: 34-57)**

| Element | X | Y | Width | Height |
|---------|---|---|-------|--------|
| Header "→O" | 160 | 34 | 24 | 6 |
| Label "C1" | 132 | 40 | 20 | 6 |
| Label "C2" | 132 | 46 | 20 | 6 |
| Label "C3" | 132 | 52 | 20 | 6 |
| Label "C4" | 132 | 58 | 20 | 6 |
| Values | 160 | 40-58 | 24 | 6 each |

### Footer (Y: 58-63)

| Function | X | Width | Label |
|----------|---|-------|-------|
| F1 | 0 | 51 | "C" |
| F2 | 51 | 51 | "G/O" |
| F3 | 102 | 51 | "R/L" |
| F4 | 153 | 51 | "L" |
| F5 | 204 | 52 | "◄►" |

## Editing Interaction

Based on CvRoutePage and RoutingPage patterns.

### Navigation

| Control | Action |
|---------|--------|
| **F1-F4** | Select column in active section |
| **F5** | Toggle between INPUT ↔ OUTPUT panel |
| **Same F-key again** | Cycle to next row in that column |
| **Track keys T1-T4** | Select trigger rows directly |
| **Track keys T5-T8** | Select CV rows directly |

### Encoder Behavior

**Direct editing** (no press-to-edit, matches CvRoutePage):
- Turn encoder → immediately cycles through valid sources/destinations
- Shift + encoder → larger steps (for numeric values)

### Cycling Logic Per Cell

| Section | Column | Encoder Cycles Through |
|---------|--------|------------------------|
| Trigger Input | C | None → CvIn1 → CvIn2 → CvIn3 → CvIn4 → None |
| Trigger Input | G | None → GateOut1 → ... → GateOut8 → None |
| Trigger Input | L | None → LogicalGate1 → ... → LogicalGate8 → None |
| CV Input | C | None → CvIn1 → CvIn2 → CvIn3 → CvIn4 → None |
| CV Input | O | None → CvOut1 → ... → CvOut8 → None |
| CV Input | R | None → CvRoute1 → ... → CvRoute4 → None |
| CV Input | L | None → LogicalCv1 → ... → LogicalCv8 → None |
| Trigger Output | →G | GateOut1 → ... → GateOut8 (no None) |
| CV Output | →O | CvOut1 → ... → CvOut8 (no None) |

### Visual Feedback

| State | Display |
|-------|---------|
| Active cell | `Color::Bright` |
| Inactive with value | `Color::Medium` |
| Inactive empty | `Color::Low` or blank |
| Panel not focused | Dimmed overall |

## State Variables

```cpp
enum class EditPanel { Inputs, Outputs };
enum class EditSection { Triggers, CvInputs };  // for input panel

EditPanel _panel = EditPanel::Inputs;
EditSection _section = EditSection::Triggers;
int _activeCol = 0;   // 0-3 for source group columns
int _activeRow = 0;   // 0-3 for TR1-4, 0-5 for IN/PA/X/Y/Z/T
```

## Conflict Detection

The TeletypeTrack model already has conflict detection methods:
- `isCvOutputUsedAsOutput()` / `isCvOutputUsedAsInput()`
- `isGateOutputUsedAsOutput()` / `isGateOutputUsedAsInput()`
- `getAvailableTriggerInputSources()` / `getAvailableCvInputSources()`
- `getAvailableTriggerOutputDests()` / `getAvailableCvOutputDests()`

When cycling, skip conflicting options (like CvRoutePage does for Bus conflicts).

## Reference Implementation

See existing pages for patterns:
- `src/apps/sequencer/ui/pages/CvRoutePage.cpp` - Matrix layout, direct encoder editing
- `src/apps/sequencer/ui/pages/RoutingPage.cpp` - Bias/Depth overlay, function key navigation

## ASCII Reference

```
X:  0    20   40   60   80  100  120 128  152  176  200  224  256
    │     │    │    │    │    │    │   │    │    │    │    │    │
Y=0 ┌─INPUTS─────────────────────────┬─OUTPUTS────────────────────┐
    │                                │                            │
Y=8 │     C    G    L                │      →G                    │
    │    ───  ───  ───               │     ────                   │
Y=14│ T1  3              ●           │  TA   1                    │
Y=20│ T2       1                     │  TB   2                    │
Y=26│ T3       6                     │  TC   3                    │
Y=32│ T4  4                          │  TD   4                    │
    │────────────────────────────────│────────────────────────────│
Y=34│     C    O    R    L           │      →O                    │
    │    ───  ───  ───  ───          │     ────                   │
Y=40│ IN  1                          │  C1   1                    │
Y=46│ PA  2                          │  C2   2                    │
Y=52│  X       3                     │  C3   3                    │
Y=55│  Y            1                │  C4   4                    │
Y=58│ Z:C4  T:L1                     │                            │
    ├────────────────────────────────┴────────────────────────────┤
Y=62│ [C]  [G/O] [R/L]  [L]                              [◄►]     │
Y=64└─────────────────────────────────────────────────────────────┘
```
