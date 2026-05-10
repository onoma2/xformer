# Performer Keyboard Shortcuts — Task Status

Status: Paused
Priority: Medium

## Overview

Propose and implement USB keyboard shortcuts for the full Performer UI, going beyond the already-implemented Teletype pages. The hardware has ~40 buttons (8 track + 16 step + 6 global + encoder), and the firmware has ~50 page types. The goal is to provide keyboard equivalents for as many actions as possible.

## Key Architecture Constraints

- **PageManager** uses a stack (max depth 8). Events flow top-to-bottom. Modals intercept before lower pages.
- **TopPage** is always at stack position 0 and handles all **global navigation** (track selection, mode switching via Page+Key combos).
- Modifier keys: **Page** (= Alt?) and **Shift** (= Shift?). On hardware, Page+Key navigates between pages; Shift+Key modifies actions.
- Key events are dispatched as `KeyboardEvent(keycode, modifiers, ch)` to whichever page is on top of the stack.
- The **keyboard()** handler on each page only receives events if that page is the top of the stack (or dispatches downwards if not consumed + not modal).

## Proposed Leader Key Scheme

### Approach A: Alt as Page modifier (recommended for Phase 1)

The hardware has two modifier keys: **Page** (page navigation) and **Shift** (action variant). For keyboard:

| Keyboard | Hardware equivalent | Purpose |
|----------|-------------------|---------|
| **Ctrl** (alone) | — | Text/Teletype shortcuts (already used) |
| **Alt** | Page | Page navigation, global commands |
| **Shift** | Shift | Action variant (same as hardware) |
| **Alt+Shift** | Page+Shift | Context menu |
| **Ctrl+Shift** | — | Reserved for text operations |
| **Super/Win/Cmd** | — | Future expansion |

### Approach B: Vim-style leader key (consider for later)

An alternative to Alt+Key is a vim-style leader key — a dedicated key that enters "command mode" so the next keypress is interpreted as a navigation/action command.

**How it would work**:
- A leader key (e.g., Grave/Backtick (\`), or Tab tapped alone) toggles a `_leaderActive` flag in Ui/PageManager.
- The next keypress is intercepted before normal dispatch and decoded as a leader command.
- After the command (or a ~500ms timeout), leader mode exits.
- This avoids modifier key conflicts (Alt interferes with Teletype printable char input) and is familiar to vim users.

**Proposed leader mapping**:

| Sequence | Action |
|----------|--------|
| Leader + 1..8 | Select track |
| Leader + P | Project page |
| Leader + L | Layout page |
| Leader + R | Routing page |
| Leader + S | Sequence page |
| Leader + E | Step edit page |
| Leader + T | Track page |
| Leader + O | Overview |
| Leader + M | Monitor page |
| Leader + G | Performer page (mutes) |
| Leader + F | Pattern page |
| Leader + C | Clock setup |
| Leader + H | Harmony |
| Leader + N | Song page (chain) |
| Leader + / | Search/help overlay |
| Leader + Space | Toggle play |
| Leader + Enter | Toggle record |

**Tradeoffs vs Alt+Key**:
- **Alt+Key**: Zero latency, mirrors hardware Page+Key exactly, simple. But Alt conflicts with Teletype text entry (letters), hard to reach on some keyboards.
- **Leader key**: Clean, no modifier conflicts, works with any letter, familiar to vim users. But ~500ms latency on leader tap, state machine complexity, leader key can't have other bindings.

**Recommendation**: Use Alt+Key for Phase 1 (mirrors hardware). Add leader key as an alternative later for keys that conflict or users who prefer it. Could even make the leader key configurable via Settings.

### Navigation (Alt = Page button)

| Keyboard | Hardware | Action |
|----------|---------|--------|
| Alt+1..8 | Page+Track1..8 | Switch to page: Project (1), Layout (2), Routing (3), MidiOutput (4), UserScale (5), CvRoute (6), (7=empty), System (8) |
| Alt+Q / Alt+W / Alt+E | Page+Step1/2/3 | Sequence page / Track page / Song page |
| Alt+A (or Alt+Z) | Page+Step0 | Sequence Edit (step editing) |
| Alt+S | Page+Step7 | Monitor page |
| Alt+D | Page+Left | Overview page |
| Alt+F | Pattern button | Pattern page |
| Alt+G | Performer button | Performer page (mute/solo/fill) |
| Alt+T | Tempo button | Tempo page |
| Alt+C | Page+Tempo | Clock Setup page |
| Alt+R | — | Routing page (direct) |
| Alt+H | — | Harmony page (direct) |
| Alt+O | — | Overview (also Alt+D) |

### Track Selection (no modifier)

| Keyboard | Action |
|----------|--------|
| 1-8 | Select track 0-7 |
| Shift+1-8 | Select track with shift (same as hardware) |

### Transport

| Keyboard | Action |
|----------|--------|
| **Space** | Toggle play (currently works on PatternViewPage for 0/1 toggle — would need universal handler) |
| **Ctrl+Space** or **Shift+Space** | Toggle play from start? |
| **Ctrl+R** | Toggle recording (overdub) |

### Context Menu (Alt+Shift = Page+Shift)

| Keyboard | Action |
|----------|--------|
| **Alt+Shift** | Open context menu (standard hardware Page+Shift) |

### Pattern Page (modal)

| Keyboard | Hardware | Action |
|----------|---------|--------|
| **L** | F1 (LATCH) | Toggle latch mode |
| **Y** | F2 (SYNC) | Toggle sync mode |
| **S** | F3 (SNAP) | Create/revert snapshot |
| **C** | F4 (COMMIT) | Commit snapshot |
| **X** / **Esc** | F5 (CANCEL) | Cancel pending changes |

### NoteSequenceEdit Page (step editing)

| Keyboard | Hardware | Action |
|----------|---------|--------|
| **G** | F1 cycle | Gate → GateProb → GateOffset → Slide |
| **R** | F2 cycle | Retrigger → RetriggerProb → PulseCount → GateMode |
| **L** | F3 cycle | Length → LengthVarRange → LengthVarProb |
| **N** | F4 cycle | Note → NoteVarRange → NoteVarProb → AccTrigger → Harmony → Voicing |
| **C** | F5 | Condition layer |
| **Left/Right** | Left/Right | Move section (0-3) |
| **Shift+Left/Right** | Shift+Left/Right | Shift selected steps left/right |
| **Page+Step9-16** | Same | QuickEdit (Alt+Shift+digit?) |
| **Q** | — | Quick Edit mode toggle |

### Layout Page

| Keyboard | Hardware | Action |
|----------|---------|--------|
| **M** | F1 (MODE) | Track mode assignment |
| **L** | F2 (LINK) | Track linking |
| **G** | F3 (GATE) | Gate output assignment |
| **V** | F4 (CV) | CV output assignment |
| **Enter** or **C** | F5 (COMMIT) | Commit mode changes |

### Routing Page

| Keyboard | Hardware | Action |
|----------|---------|--------|
| **Left/Right** or **[ / ]** | F1/F2 (PREV/NEXT) | Previous/next route |
| **I** | F3 (INIT) | Clear route |
| **L** | F4 (LEARN) | Toggle MIDI learn |
| **Enter** | F5 (COMMIT) | Commit routes |

### Song Page

| Keyboard | Hardware | Action |
|----------|---------|--------|
| **C** | F1 (CHAIN) | Toggle chain playback |
| **I** | F2 (INSERT) | Insert/Add slot |
| **Delete** / **X** | F3 (REMOVE) | Remove slot |
| **D** | F4 (DUPL) | Duplicate slot |
| **Space** | F5 (PLAY/STOP) | Start/stop song |

### Performer Page (mute/solo/fill)

| Keyboard | Hardware | Action |
|----------|---------|--------|
| **L** | F1 (LATCH) | Latch mode |
| **Y** | F2 (SYNC) | Sync mode |
| **U** | F3 (UNMUTE) | Unmute all |
| **F** | F4 (FILL) | Fill mode (hold) |
| **X** / **Esc** | F5 (CANCEL) | Cancel pending |
| **M** | Track key | Toggle mute for selected track |
| **S** | Shift+Track | Solo selected track |
| **Ctrl+1-8** | — | Toggle fill on track |

### System Page

| Keyboard | Hardware | Action |
|----------|---------|--------|
| **C** | F1 (CAL) | Calibration mode |
| **I** | F2 (INFO) | Memory info |
| **U** | F3 (UTILS) | Utilities |
| **F5** or **U** | F4 (UPDATE) | Firmware update mode |

## Code Changes Required

### 1. Universal Shortcuts in Page base class (Page.h / Page.cpp)
Add a `handleGlobalKeyboard()` method called before page-specific `keyboard()`:
- **Space**: Toggle play/stop (unless consumed by current page)
- **Tab**: Cycle through modes/tracks?
- **Escape**: Pop current page / go back
- **1-8**: Select track
- **Ctrl+R**: Toggle recording

This requires Page to have access to Engine (for transport control) or a callback. BasePage already has `_engine` reference.

### 2. Alt navigation in TopPage (TopPage.cpp)
TopPage::keyboard() needs to intercept Alt+key combos to call `setMode()` — mirroring what Page+hardware-key does. Since TopPage is always at position 0 and events flow top-to-bottom, this naturally works: if no higher page consumes the keyboard event, TopPage handles it.

### 3. F-key mapping per page
Each page's `keyboard()` handler needs to map F1-F5 (and optionally F6-F8) to their hardware F1-F5 equivalents. Since F1-F5 are already in use on Teletype pages, the pattern is established:
- PatternPage: F1(F2/LATCH), F2/F3(SYNC), etc.
- NoteSequenceEditPage: F1(GATE cycle), F2(RETRIG cycle), etc.
- LayoutPage: F1(MODE), F2(LINK), F3(GATE), F4(CV), F5(COMMIT)
- RoutingPage: F1(PREV), F2(NEXT), F3(INIT), F4(LEARN), F5(COMMIT)
- SongPage: F1(CHAIN), F2(INSERT), F3(REMOVE), F4(DUPL), F5(PLAY/STOP)
- PerformerPage: F1(LATCH), F2(SYNC), F3(UNMUTE), F4(FILL), F5(CANCEL)
- SystemPage: F1(CAL), F2(INFO), F3(UTILS), F4(UPDATE)
- TrackPage: F1(TI PRESET), F4(SYNC OUTS)
- MonitorPage: F1(CV IN), F2(CV OUT), F3(MIDI), F4(STATS), F5(VERSION)

### 4. TopPage Alt navigation mapping
Map Alt+letter to page modes in TopPage::keyboard(). This is the largest single code change (~40-50 lines).

### 5. PageManager integration
Add `pageForMode()` helper so keyboard shortcuts can navigate to any page by name, and add keyboard handler dispatch in PageManager.

## Implementation Order (Recommended)

1. **Phase 1**: Universal shortcuts in Page base class (Space=play, Escape=back, 1-8=track select) + TopPage Alt+letter page navigation
2. **Phase 2**: F1-F5 mapping on all "page" pages (PatternPage, NoteSequenceEditPage, LayoutPage, RoutingPage, SongPage, PerformerPage, SystemPage, MonitorPage, TrackPage)
3. **Phase 3**: Letter shortcuts for modal actions (L=Latch, M=Mute, etc.)
4. **Phase 4**: Context menu keyboard access (Alt+Shift)

## Files to Modify

| File | Change |
|------|--------|
| `ui/Page.h/cpp` | Add `globalKeyboard()` virtual, space/escape/track select |
| `ui/PageManager.h/cpp` | Keyboard dispatch routing, page lookup helpers |
| `ui/pages/TopPage.cpp` | Alt+letter navigation mapping |
| `ui/pages/PatternPage.cpp` | F1-F5 + letter mapping |
| `ui/pages/NoteSequenceEditPage.cpp` | F1-F5 layer cycling |
| `ui/pages/LayoutPage.cpp` | F1-F5 mode switching |
| `ui/pages/RoutingPage.cpp` | F1-F5 prev/next/init/learn/commit |
| `ui/pages/SongPage.cpp` | F1-F5 chain/insert/remove/dupl/play |
| `ui/pages/PerformerPage.cpp` | F1-F5 latch/sync/unmute/fill/cancel |
| `ui/pages/SystemPage.cpp` | F1-F5 cal/info/utils/update |
| `ui/pages/TrackPage.cpp` | F1/F4 TI presets/sync outs |
| `ui/pages/MonitorPage.cpp` | F1-F5 view modes |

## Current Hardware Workflow Analysis

The following button sequence is required to set up a 32-step Note track in Ikra mode with various pitches and 3 routed parameters. This reveals the core friction points.

### Phase 1: Track Mode + I/O (Layout page)
1. `Page + T2` → Layout page
2. `F1` (MODE) → scroll to track row → click encoder → turn to "Note"
3. `F5` (COMMIT) → confirm dialog
4. `F3` (GATE) → assign gate outputs → `F5`
5. `F4` (CV) → assign CV outputs → `F5`

### Phase 2: Ikra mode, length, divs, scale (Sequence page)
6. `Page + S2` → Sequence page
7. Encoder scroll+click: Mode → "Ikra"
8. First Step → 1, Last Step → 32
9. Note First → 1, Note Last → 16
10. Div X → 1/4, Div N → 1/8
11. Scale → desired, Root Note → desired

**Friction**: Each parameter requires encoder scroll + click to enter edit, scroll to find value, click to confirm. ~10 repetitive scroll+click cycles.

### Phase 3: Step editing (NoteSequenceEditPage)
12. Double-press `T1` → SequenceEdit page
13. `F4` (Note layer) → select steps via `S1`-`S16` → turn encoder for pitch
14. `←`/`→` to move between sections (0-3)
15. `F1` (Gate layer) → tap steps to toggle gate
16. `F3` (Length layer) → select steps → encoder for length
17. `F5` (Condition layer) → set conditions

**Friction**: Setting 32 different pitches = 32 individual encoder scroll actions. Moving between sections = 3 arrow presses. No batch operations — can't type "C4" directly. No way to address step 27 without navigating to section 2 and counting.

### Phase 4: Route 3 params (Routing page)
18. `Page + T3` → Routing page
19. Route 1: Encoder scroll to Target → "Divisor", Source → "CV In 1", Min/Max → `F5`
20. `F2` (NEXT) → Route 2: same scroll+click per row → `F5`
21. `F2` → Route 3: same → `F5`

**Friction**: Each route = ~6 encoder scroll+click operations. 3 routes = 18 operations. No way to type values directly.

### Phase 5: Play
22. `Play` button

**Total**: ~22 distinct interaction sequences, most involving encoder scroll+click. The Sequence page alone accounts for ~10 encoder cycles. Step editing is the most tedious — each of 32 steps requires individual selection + encoder turn.

---

## Keyboard-Based Simplification Proposals

### Feasibility Assessment

**What already exists (the foundation):**
- `KeyboardEvent` dispatch via `Page::keyboard()` — works and is proven
- `hidKeycodeToAscii()` — maps all printable chars, punctuation, digits
- Teletype `_editBuffer` + `insertChar()` — proves text input/editing works on this hardware
- F-key constants (`KeyF1`-`KeyF5`) — already in `Event.h`

**Critical finding**: No non-Teletype page currently overrides `keyboard()`. This means:
- All pages (TopPage, ListPage subclasses, NoteSequenceEditPage, PatternPage, SongPage, PerformerPage, etc.) are a **clean slate** — zero conflicts
- Adding `keyboard()` to any of them won't break anything
- Events not consumed by any page fall through to TopPage at stack position 0

### Approach A: Form Mode (Tab through fields, type values)

**How it works**: On any list page (Sequence, Track, Routing, Layout, etc.), pressing `Enter` or `Tab` enters a "form mode." Each list row becomes a text field — type the value directly.

**Example for Sequence page setup**:
```
Tab → Tab to "Mode" → type "ikra" → Enter
Tab to "Last Step" → type "32" → Enter
Tab to "Div X" → type "4" → Enter
Tab to "Scale" → type "major" or "C" → Enter
```

**Step editing (NoteSequenceEditPage)**:
```
F4 (Note layer) → click step → type "C4" → Enter
Or: select multiple steps → type "E4" → all selected = E4
```

**Feasibility: Very high.**
- Each `ListPage` subclass just needs a `keyboard()` override (~20 lines per page)
- Friction: `ListModel::edit(row, column, value, shift)` only accepts relative +/-1 values
- Solution A: Add `setAbsolute(row, int value)` to `ListModel` — clean, general
- Solution B: Call the model directly from the page (e.g., `_sequence->setLastStep(32)`) — simplest
- Pattern already proven by TeletypeScriptViewPage's `_editBuffer`

### Approach C: Direct Step Type-In (`5N E4`)

**How it works**: On NoteSequenceEditPage, a Teletype-style command line at the bottom of the screen. Type step number, layer letter, value.

**Examples**:
```
"5N E4"     → step 5, Note = E4
"1-16G 1"   → steps 1-16, Gate = on
"8L 4"      → step 8, Length = 4
"3-5R 2"    → steps 3-5, Retrigger = 2
"1-32N C4"  → all 32 steps, Note = C4 (batch!)
```

**Feasibility: High.**
- Directly modeled on TeletypeScriptViewPage's `_editBuffer` + `insertChar()` + `commitLine()` pattern
- `NoteSequence` already has step setters (`setNote(step, value)`, `setGate(step, bool)`, etc.)
- Parser logic: split on space, parse step range, parse layer letter, parse value — ~50 lines
- Note name parser (C4=60, E4=64, G#3=56) needs a lookup table — already exists in the model layer

### Approach B: Command Palette (`Ctrl+K` overlay)

**How it works**: `Ctrl+K` or `:` opens a search overlay. Type partial name of any parameter. Select with Enter, then type the value.

**Examples**:
```
Ctrl+K → "last step" → Enter → "32" → Enter
Ctrl+K → "run mode" → Enter → "ik" → Enter
Ctrl+K → "route 3 target" → Enter → "root note" → Enter
```

**Feasibility: Moderate.**
- Needs a new `KeyboardPalettePage` pushed as a modal overlay
- Needs a text buffer + search index + action callback mapping
- 128x64 OLED is tight but doable — Teletype already renders multiline text
- ~200 lines of new code (page, renderer, search, dispatch)
- Most powerful but most invasive — recommended for later phases

### Combined Recommendation

**Phase A (form mode) + Phase C (step type-in) first**, then command palette later. The step type-in feature alone eliminates the most painful workflow — setting individual step values. Form mode eliminates the encoder scroll+click for list pages. Together they cover ~90% of the friction with ~100 lines of code.

## Implementation Order (Recommended)

1. **Phase 1**: Universal shortcuts in Page base class (Space=play, Escape=back, 1-8=track select) + TopPage Alt+letter page navigation
2. **Phase 1b**: Form mode on NoteSequencePage (Tab through params, type values directly)
3. **Phase 1c**: Step type-in command line on NoteSequenceEditPage (`5N E4` syntax)
4. **Phase 2**: F1-F5 mapping on all page types
5. **Phase 3**: Form mode on other list pages (Layout, Routing, Track, Song, etc.)
6. **Phase 4**: Command palette (Ctrl+K) for universal search+action

## Files to Modify

| File | Change |
|------|--------|
| `ui/Page.h/cpp` | Add `globalKeyboard()` virtual, space/escape/track select |
| `ui/PageManager.h/cpp` | Keyboard dispatch routing, page lookup helpers |
| `ui/pages/TopPage.cpp` | Alt+letter navigation mapping |
| `ui/pages/NoteSequenceEditPage.h/cpp` | Step type-in command line + F1-F5 layer cycling |
| `ui/pages/NoteSequencePage.cpp` | Form mode (Tab through sequence params) |
| `ui/pages/ListPage.h/cpp` | Optional: add `setAbsolute()` or keyboard handler base |
| `ui/model/ListModel.h` | Optional: add `setAbsolute(row, value)` virtual |
| `ui/pages/PatternPage.cpp` | F1-F5 + letter mapping |
| `ui/pages/LayoutPage.cpp` | F1-F5 mode switching + form mode |
| `ui/pages/RoutingPage.cpp` | F1-F5 prev/next/init/learn/commit + form mode |
| `ui/pages/SongPage.cpp` | F1-F5 chain/insert/remove/dupl/play |
| `ui/pages/PerformerPage.cpp` | F1-F5 latch/sync/unmute/fill/cancel |
| `ui/pages/SystemPage.cpp` | F1-F5 cal/info/utils/update |
| `ui/pages/TrackPage.cpp` | F1/F4 TI presets/sync outs |
| `ui/pages/MonitorPage.cpp` | F1-F5 view modes |

## Testing

- Each page's F1-F5 must not conflict with existing keyboard handlers
- Alt+letter navigation must not conflict with Teletype page text entry (Teletype pages consume all printable chars)
- Track selection (1-8) must not conflict with digit entry on pattern/sequence pages (digits consumed by those pages first)
