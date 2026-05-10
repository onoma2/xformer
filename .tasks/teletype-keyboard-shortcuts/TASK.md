# Teletype Keyboard Shortcuts Port — Task Status

Status: Pending
Priority: Medium

## Overview

Port the highest-value hardware Teletype keyboard shortcuts to the Performer's TeletypeScriptViewPage and TeletypePatternViewPage. These are safe additions that don't conflict with existing key handlers.

## Key Differences from Hardware Teletype

**Performer (TELETYPE_TRACK_LITE=1):**
- `REGULAR_SCRIPT_COUNT = 4` → scripts 0, 1, 2, 3
- `METRO_SCRIPT = 4`
- No dedicated `INIT_SCRIPT` slot (INIT_SCRIPT = NO_SCRIPT)
- Total editable: scripts 0-4 (5 scripts)

**Hardware Teletype (non-lite):**
- `REGULAR_SCRIPT_COUNT = 8` → scripts 0-7
- `METRO_SCRIPT = 8`
- `INIT_SCRIPT = 9`
- Total editable: scripts 0-9 (10 scripts)

## Port Candidates

### 1. F1-F4: Run scripts 0-3 (ScriptViewPage)
- Hardware: `F1-F8` → `run_script(&scene_state, k - HID_F1)` (scripts 0-7)
- Performer: F1-F4 → scripts 0-3
- **Keycodes**: 0x3A-0x3D (HID_F1-HID_F4)
- **Conflict check**: No existing F-key handlers in Performer
- **Implementation**: Add to TeletypeScriptViewPage::keyboard(), call track engine runScript()

### 2. F5: Run metro script (ScriptViewPage)
- Hardware: `F9` → `run_script(&scene_state, METRO_SCRIPT)`
- Performer: F5 → metro script (index 4 / METRO_SCRIPT)
- **Keycode**: 0x3E (HID_F5) — maps to metro since no F9
- **Conflict check**: None
- **Implementation**: Same as F1-F4 but with METRO_SCRIPT index

### 3. Alt+F1-F4: Jump to edit scripts 0-3
- Hardware: `Alt+F1-F8` → `set_edit_mode_script(k - HID_F1); set_mode(M_EDIT)`
- Performer: Alt+F1-F4 → `setScriptIndex(k - HID_F1)`
- **Conflict check**: No existing Alt+Fn handlers
- **Implementation**: In ScriptViewPage::keyboard(), detect Alt+F1-F4 keycodes

### 4. Alt+F5: Jump to edit metro script
- Hardware: `Alt+F9` → edit metro
- Performer: Alt+F5 → `setScriptIndex(METRO_SCRIPT)`

### 5. Alt+/ : Toggle line comment (ScriptViewPage)
- Hardware: `edit_mode.c` → `match_alt(m, k, HID_SLASH)` → `ss_toggle_script_comment()`
- Performer: Already has `commentLine()` method → just wire Alt+/
- **Keycode**: 0x38 (HID_SLASH → `/` or `?` when shifted)
- **Conflict**: Check that Alt+/ doesn't conflict — `/` keycode 0x38, no Alt+ handler exists
- **Implementation**: In ScriptViewPage::keyboard(), check `event.alt() && keycode == 0x38`, call `commentLine()`

### 6. Alt+Left / Alt+Right: First/last pattern column (PatternViewPage)
- Hardware: `pattern_mode.c` → `match_alt(m, k, HID_LEFT)` → `base = 0; offset = 0`
- Performer: No Alt+arrow handlers in PatternViewPage
- **Keycodes**: Alt+Left (0x50), Alt+Right (0x4F)
- **Conflict check**: Currently Alt+Up/Down used for page up/down in PatternViewPage. Alt+Left/Right are free.
- **Implementation**: In TeletypePatternViewPage::keyboard(), add Alt+Left → pattern 0, Alt+Right → pattern 7

## Implementation Plan

1. Add F1-F4 run script + Alt+F1-F4 jump to edit in ScriptViewPage::keyboard()
2. Add F5 run metro + Alt+F5 edit metro in ScriptViewPage::keyboard()
3. Add Alt+/ toggle comment in ScriptViewPage::keyboard()
4. Add Alt+Left/Right first/last pattern in PatternViewPage::keyboard()
5. Test build
6. Hardware test

## Files to modify
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` — keyboard() handler
- `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` — keyboard() handler
