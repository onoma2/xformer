# Hardware keyboard ← simulator parity

**Goal:** the **simulator's keyboard map is the reference**. Hardware reproduces each sim
key→action where a USB-HID keyboard can physically express it. The sim keeps all its extras
(mouse, scroll-to-rotate, `F10–F12` fake jacks) — hardware doesn't need them.

This is **not** unification and **not** a shared code path. Two input systems stay separate:

- **Simulator** (reference): SDL panel map writes button *indices* via `writeButton`
  (`Frontend.cpp`, `keys[info.index]`). No typing, no HID layer.
- **STM32**: USB-HID → `Engine::receiveKeyboard()` → `KeyboardManager` → page `keyboard()` handlers.
  (`recvKey` is a stub in the sim — `sim/drivers/UsbH.h:34` — so this whole layer is hardware-only.)

The work splits into two buckets:

- **Match bucket** — change hardware so its keys do what the sim's keys do.
- **Keep bucket** — hardware-only keyboard bindings the sim has no key for (it uses mouse/encoder
  instead). **These are NOT touched by this work. Leave every one of them exactly as-is.**

---

## Match bucket — the key→control assignment

This is the only thing this work changes: the table of which key maps to which control. It is data,
applied through the mechanisms that already exist in the Keep bucket — it does not alter those
mechanisms. The assignment is **panel-faithful** (tracks above steps, mirroring the physical panel).

| Key(s) | Control (per sim) | Matches today? |
|--------|-------------------|----------------|
| `Q W E R T Y U I` | Tracks 1–8 | no — change |
| `A S D F G H J K` | Steps 1–8 | no — change |
| `Z X C V B N M ,` | Steps 9–16 | no — change |
| `1 2 3 4` | Play / Tempo / Pattern / Performer | no — change |
| `Space` | Encoder press | no — change |
| `← →` | Left / Right | yes |
| `Shift` | Shift | yes |
| `F1`–`F5` | Function buttons F0–F4 | yes |

These panel meanings apply only outside a text context; in a text/editor page the same keys type,
exactly as today, via the `mapStepKeys` gate. Modifiers (Shift, Page) are produced by the folding
mechanism and are not part of this assignment — they stay as-is.

---

## Keep bucket — hardware-only keyboard bindings (DO NOT TOUCH)

These exist because a USB keyboard lacks what the sim uses (a mouse, scroll wheel, two holdable
modifier buttons). The sim has no key for them and doesn't need them. **This work changes none of
them.** Cataloged here only so they are never accidentally disturbed while the Match bucket lands.

### Why they exist (the HID constraint)

- **Shift and Page are HID modifier bits**, not key events (`Shift`=`0x22`, `Page`=`Ctrl`/`Alt`
  mask). A modifier rides on a carrier keycode; a modifiers-only report fires nothing.
- **`Shift+Page` is unreachable** (two modifiers, no carrier) → `Tab` is its stand-in.
- **No encoder on the keyboard** → `Up`/`Down` synthesize rotation.
- **Modifier folding** makes `Shift+step` / `Page+step` work (step is the carrier).
- **`Alt` disables step mapping** to allow typing a letter on a step page (escape hatch).

### Layer 1 — `KeyboardManager` mechanisms (`KeyboardManager.cpp`)

These are the mechanisms that carry the assignment; they do not change.

| Mechanism | Behavior | Line |
|-----------|----------|------|
| ASCII map | letters / digits / symbols → chars for typing | 3-32 |
| Shift fold | `0x22` → `Key::Shift` (ephemeral) | 108 |
| Page fold | `0x11` (Ctrl) → `Key::Page` (ephemeral) | 109 |
| Alt escape | `0x44` disables step mapping → char passes | 90 |
| Char dispatch | non-step keypress → `KeyboardEvent` to page | 123-126 |
| `mapStepKeys` gate | step mapping off on Teletype script/pattern + text input | Ui.cpp:83 |

### Layer 2 — `BasePage` synthesized shortcuts (`BasePage.cpp`)

| Key | Action | Line |
|-----|--------|------|
| `Tab` | Synthesize `Page+Shift` = context menu | 50 |
| `Up` / `Down` | Encoder rotate +1 / −1 | 89 |
| `Left` / `Right` (+Shift) | Left / Right buttons | 63 / 75 |
| `F1`–`F5` (+Shift) | Function buttons F0–F4 | 40-41 |

### Layer 3 — per-page `keyboard()` overrides

**Delegate-only** (F1–F5 + fall through; add nothing beyond Layer 2):
NoteSequenceEdit `1227`, CurveSequenceEdit `1563`, IndexedSequenceEdit `1930`, TuesdayEdit `907`,
PhaseFluxEdit `430`, DiscreteMapSequence `1998`, TrackPage→ListPage `256`, ModulatorPage `986`,
SystemPage→ListPage `514`, SongPage `420`, RoutingPage (empty) `1188`.

**Custom handlers:**

| Page | Key | Action | Line |
|------|-----|--------|------|
| TopPage | `Esc` / `Space`(+Shift) | pop page / toggle play | TopPage.cpp:170,179 |
| ListPage *(base)* | `Up`/`Down` | prev / next row | ListPage.cpp:301-308 |
| | `Left`/`Right`(+Shift) | edit value −/＋ | :309-316 |
| | `Enter` | toggle edit mode | :317-320 |
| PerformerPage | `F1`/`F2`/`F4` | latch / sync / commit fills | PerformerPage.cpp:257-269 |
| ContextMenuPage | `F1`–`F5` / `Esc`/`Tab` | select item / close | ContextMenuPage.cpp:106-130 |
| TextInputPage | `Enter`/`Esc`/`Bksp`/`Del`/`←`/`→` | edit / confirm / cancel | TextInputPage.cpp:160-183 |
| | `F1`–`F5` | BS / DEL / CLEAR / CANCEL / OK | :186-196 |
| | `A–Z 0–9 - Space` | insert char (upper-cased) | :198-206 |
| FileSelectPage | `Enter`/`Esc` | confirm / cancel | FileSelectPage.cpp:84-91 |
| MidiOutputPage | `Enter`/`Esc` | commit / pop | MidiOutputPage.cpp:98-106 |
| GeneratorPage | `Enter`/`Esc` | commit / revert | GeneratorPage.cpp:517-524 |
| GeneratorSelectPage | `Enter`/`Esc` | close true / false | GeneratorSelectPage.cpp:64-70 |
| LayoutPage | `Enter`/`Esc` | commit / pop | LayoutPage.cpp:87-101 |
| IndexedRouteConfigPage | `Enter`/`Esc` | commit / pop | IndexedRouteConfigPage.cpp:359-368 |
| IndexedMathPage | `Enter`/`Esc` | apply math / pop | IndexedMathPage.cpp:565-574 |
| ConfirmationPage | `Enter`/`Esc` | confirm / cancel | ConfirmationPage.cpp:67-74 |
| ProjectPage | `Enter` (row 0) | open name text-input | ProjectPage.cpp:105-113 |
| UserScalePage | `Enter` (row 0) | open name text-input | UserScalePage.cpp:85-94 |
| PatternPage | `F1`/`F2` | toggle latch / sync | PatternPage.cpp:388-396 |
| MonitorPage | `F6` | `pressFunctionButton(5)` | MonitorPage.cpp:498-502 |

**TeletypeScriptViewPage** (`TeletypeScriptViewPage.cpp`)

| Key | Action | Line |
|-----|--------|------|
| `F1`–`F8` / `F9` / `F10` | fire scripts / Metro / Init | 849-872 |
| `Alt+F1`–`F10` | edit script / Metro / Init | 878-891 |
| `Alt+/` | toggle line comment | 893-896 |
| `Ctrl+F1`–`F8` / `Ctrl+F9` | mute script / toggle Metro | 902-913 |
| `Ctrl+Home`/`Ctrl+End` | cursor line start / end | 918-924 |
| `Ctrl+C`/`Ctrl+V`/`Ctrl+X` | copy / paste / cut line | 926-938 |
| `Enter` / `Shift+Enter` | commit+advance / commit+blank | 947-957 |
| `Bksp`/`Del`/`←`/`→` | edit / cursor | 960-980 |
| `Up`/`Down` | history (live) or line move (edit) | 984-999 |
| `Esc` / `Tab` | clear buffer / insert space | 1001-1012 |
| `[` / `]` | prev / next script | 1015-1027 |
| `Home`/`End` | buffer start / end | 1029-1037 |
| printable | insert (a–z→A–Z) | 1042-1048 |

**TeletypePatternViewPage** (`TeletypePatternViewPage.cpp`)

| Key | Action | Line |
|-----|--------|------|
| `Ctrl+Home`/`Ctrl+End` | first / last row | 292-302 |
| `Ctrl+C`/`Ctrl+V`/`Ctrl+X` | copy / paste / cut | 304-336 |
| `Up`/`Down` (+`Alt`=±8) | move row | 344-366 |
| `Left`/`Right` (+`Alt`=first/last) | change pattern column | 368-386 |
| `Enter` / `Shift+Enter` | commit+down / insert row | 390-401 |
| `Bksp`/`Del` | backspace digit / delete row | 406-416 |
| `Esc` / `Space` | stop edit / toggle cell 0-1 | 420-434 |
| `-`/`_` / `[` / `]` | negate / −1 / +1 (wraps) | 438-463 |
| `0`–`9` | insert digit | 467-471 |

---

## Simulator reference map

`src/platform/sim/sim/frontend/Frontend.cpp` — SDL key → button index via `keys[info.index]`:

| Keys | Control |
|------|---------|
| `Z X C V B N M ,` | Steps 9–16 |
| `A S D F G H J K` | Steps 1–8 |
| `Q W E R T Y U I` | Tracks 1–8 |
| `1 2 3 4` | Play / Tempo / Pattern / Performer |
| `← →` / `L-Shift` / `L-Alt` | Left/Right / Shift / Page |
| `F1`–`F5` | Function F0–F4 |
| `Space` | Encoder press |
| mouse scroll / drag | Encoder rotate |
| `F10` `F11` `F12` | sim-only: clock-in / reset-in / screenshot |

The Match bucket above is exactly: make hardware's USB keys reproduce this table's key→action rows.
Everything the sim does with the mouse (encoder rotate, jack clicks) and everything in the Keep
bucket stays as-is on each side.
