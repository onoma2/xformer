# Numeric Entry for Step Values — Task Plan (v2)

Status: Ready to implement
Priority: High
Parent: performer-keyboard-shortcuts

## Overview

Add direct typed input to `NoteSequenceEditPage`. When steps are selected, typing builds a small text buffer. Pressing `Enter` parses and applies the value to the current layer for all selected steps. Pressing `Escape` or changing selection cancels.

The buffer accepts **three kinds of input** depending on the current layer:
1. **Numbers** — `5`, `-3`, `0` (works on all layers)
2. **Note names** — `C#4`, `Eb3`, `F` (Note and NoteVariationRange layers only)
3. **Enum names** — `fill`, `pre`, `hold`, `root`, `seq` (Condition, GateMode, HarmonyRoleOverride, InversionOverride layers)

This bypasses the encoder entirely — instead of spinning 40 clicks for +40 semitones, you type `C#4`. Instead of scrolling through 5 harmony roles, you type `root`.

---

## Architecture Decisions

### 1. Character Buffer, Not Integer

Store typed input as a small character array (`char _typeBuf[8]`) rather than an integer. This allows:
- Note names (`C#4`, `Eb3`)
- Enum fuzzy matching (`fill`, `pre`, `hold`)
- Still supports raw numbers (`5`, `-3`)

Buffer is 7 chars + null. Longer inputs are ignored. No dynamic allocation.

### 2. Parse-on-Enter, Not Accumulate-as-Integer

On `Enter`, inspect the buffer content and dispatch to the appropriate parser:
- Starts with `A-G` or `a-g` → **note parser** (Note/NoteVariationRange layers only; falls back to numeric on other layers)
- Starts with `-` or digit → **numeric parser** (all layers)
- Anything else → **enum parser** (layers with named enums only; falls back to numeric if no match)

### 3. Reuse `_showDetail` / `drawDetail()` Overlay

Same approach as v1: typing mode overrides the detail popup to show the buffer content and a hint (`C#4 → ENTER`). Keeps visual feedback where users already expect it.

### 4. Clamp via Existing Setters

All parsing paths end with `setLayerValue(layer(), clamped)` or the layer-specific setter. No per-layer logic in the keyboard handler beyond parser dispatch.

---

## Key Files

| File | Lines to Change | Role |
|------|-----------------|------|
| `src/apps/sequencer/ui/pages/NoteSequenceEditPage.h` | ~+8 | Add `_typeBuf[]`, `_typeLen`, `_typing` members |
| `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` | ~+120 | `keyboard()` handler, three parsers, draw override |
| `src/apps/sequencer/model/Types.h` | 0 | No changes — `printNote()` and `conditionInfos[]` already exist |
| `src/apps/sequencer/model/Scale.h` | 0 | No changes — `noteFromVolts()` already exists |

---

## Implementation Details

### Step 1: Header Changes (`NoteSequenceEditPage.h`)

Replace the v1 numeric members with a text buffer:

```cpp
    static constexpr int TypeBufSize = 8;
    char _typeBuf[TypeBufSize] = {};
    uint8_t _typeLen = 0;
    bool _typing = false;
```

### Step 2: `keyboard()` Handler (`NoteSequenceEditPage.cpp`)

Replace the existing `keyboard()` method with the expanded version:

```cpp
void NoteSequenceEditPage::keyboard(KeyboardEvent &event) {
    // Existing F1-F5 handling
    switch (event.keycode()) {
    case KeyboardEvent::KeyF1: pressFunctionButton(0, event.shift()); event.consume(); break;
    case KeyboardEvent::KeyF2: pressFunctionButton(1, event.shift()); event.consume(); break;
    case KeyboardEvent::KeyF3: pressFunctionButton(2, event.shift()); event.consume(); break;
    case KeyboardEvent::KeyF4: pressFunctionButton(3, event.shift()); event.consume(); break;
    case KeyboardEvent::KeyF5: pressFunctionButton(4, event.shift()); event.consume(); break;
    default: break;
    }

    // Typed entry mode
    if (_stepSelection.any() && !event.consumed()) {
        char ch = event.ch();

        // Backspace removes last char
        if (event.keycode() == KeyboardEvent::KeyBackspace && _typing) {
            if (_typeLen > 0) {
                _typeBuf[--_typeLen] = '\0';
            }
            if (_typeLen == 0) {
                _typing = false;
            }
            _showDetail = true;
            _showDetailTicks = os::ticks();
            event.consume();
        }

        // Append printable ASCII to buffer
        if (ch >= 32 && ch <= 126 && _typeLen < TypeBufSize - 1) {
            if (!_typing) {
                _typing = true;
                _typeLen = 0;
                _typeBuf[0] = '\0';
            }
            _typeBuf[_typeLen++] = ch;
            _typeBuf[_typeLen] = '\0';
            _showDetail = true;
            _showDetailTicks = os::ticks();
            event.consume();
        }

        // Apply on Enter
        if (event.keycode() == KeyboardEvent::KeyEnter && _typing) {
            applyTypedValue();
            _typing = false;
            _typeLen = 0;
            _typeBuf[0] = '\0';
            _showDetail = true;
            _showDetailTicks = os::ticks();
            event.consume();
        }

        // Cancel on Escape
        if (event.keycode() == KeyboardEvent::KeyEscape && _typing) {
            _typing = false;
            _typeLen = 0;
            _typeBuf[0] = '\0';
            event.consume();
        }
    }

    BasePage::keyboard(event);
}
```

### Step 3: Parser Dispatcher (`applyTypedValue()`)

Add a new private method to `NoteSequenceEditPage`:

```cpp
void NoteSequenceEditPage::applyTypedValue() {
    auto &sequence = _project.selectedNoteSequence();
    Layer currentLayer = layer();
    int value = 0;
    bool parsed = false;

    char first = _typeBuf[0];
    bool startsWithNoteLetter = (first >= 'A' && first <= 'G') || (first >= 'a' && first <= 'g');
    bool startsWithDigitOrMinus = (first >= '0' && first <= '9') || first == '-';

    // Try note name parser on Note layers
    if (startsWithNoteLetter && (currentLayer == Layer::Note || currentLayer == Layer::NoteVariationRange)) {
        parsed = parseNoteName(value);
    }

    // Try enum parser on enum layers (if not already parsed as note)
    if (!parsed && !startsWithDigitOrMinus) {
        parsed = parseEnumName(value);
    }

    // Fallback to numeric parser
    if (!parsed) {
        parsed = parseNumeric(value);
    }

    if (!parsed) {
        return; // Invalid input, silently ignore (user sees buffer stay)
    }

    // Clamp and apply
    auto range = NoteSequence::layerRange(currentLayer);
    int clamped = clamp(value, range.min, range.max);

    for (size_t i = 0; i < _stepSelection.size(); ++i) {
        if (_stepSelection[i]) {
            sequence.step(i).setLayerValue(currentLayer, clamped);
        }
    }

    if (currentLayer == Layer::Note || currentLayer == Layer::NoteVariationRange) {
        updateMonitorStep();
    }
}
```

### Step 4: Numeric Parser

```cpp
bool NoteSequenceEditPage::parseNumeric(int &out) {
    // Simple atoi on _typeBuf
    const char *p = _typeBuf;
    bool negative = false;
    if (*p == '-') {
        negative = true;
        ++p;
    }
    int value = 0;
    bool hasDigit = false;
    while (*p >= '0' && *p <= '9') {
        hasDigit = true;
        value = value * 10 + (*p - '0');
        // Prevent overflow — cap at 999
        if (value > 999) value = 999;
        ++p;
    }
    if (!hasDigit) return false;
    out = negative ? -value : value;
    return true;
}
```

### Step 5: Note Name Parser

Parse formats: `C`, `C#`, `Cb`, `C4`, `C#4`, `Cb3`

```cpp
bool NoteSequenceEditPage::parseNoteName(int &out) {
    const char *p = _typeBuf;
    char noteLetter = *p++;
    if (noteLetter >= 'a' && noteLetter <= 'g') noteLetter -= 32; // to uppercase

    static const int noteIndices[] = { 9, 11, 0, 2, 4, 5, 7 }; // A=9, B=11, C=0, D=2, E=4, F=5, G=7
    int noteIndex = noteIndices[noteLetter - 'A'];

    // Accidental
    int accidental = 0;
    if (*p == '#' || *p == 's' || *p == 'S') {
        accidental = 1;
        ++p;
    } else if (*p == 'b' || *p == 'f' || *p == 'B' || *p == 'F') {
        accidental = -1;
        ++p;
    }

    // Octave (optional, defaults to 4)
    int octave = 4;
    if (*p >= '0' && *p <= '9') {
        octave = *p - '0';
        ++p;
        if (*p >= '0' && *p <= '9') {
            octave = octave * 10 + (*p - '0');
            ++p;
        }
    }

    // Reject trailing junk
    if (*p != '\0') return false;

    int midiNote = octave * 12 + noteIndex + accidental;

    // Convert MIDI note to scale-relative note value
    auto &sequence = _project.selectedNoteSequence();
    const auto &scale = sequence.selectedScale(_project.scale());
    float volts = (midiNote - 60) * (1.f / 12.f);
    out = scale.noteFromVolts(volts);
    return true;
}
```

**Important:** `scale.noteFromVolts()` returns a scale-relative note index (e.g., in C major, D4 = +1 step). This matches exactly how the encoder edits notes.

### Step 6: Enum Parser

Fuzzy-match enum names against the buffer. Case-insensitive, prefix match.

```cpp
static bool strEqualsCI(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a >= 'a' && *a <= 'z' ? *a - 32 : *a;
        char cb = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
        if (ca != cb) return false;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

static bool strStartsWithCI(const char *str, const char *prefix) {
    while (*prefix) {
        char cs = *str >= 'a' && *str <= 'z' ? *str - 32 : *str;
        char cp = *prefix >= 'a' && *prefix <= 'z' ? *prefix - 32 : *prefix;
        if (cs != cp) return false;
        ++str; ++prefix;
    }
    return true;
}

bool NoteSequenceEditPage::parseEnumName(int &out) {
    Layer currentLayer = layer();

    switch (currentLayer) {
    case Layer::GateMode: {
        if (strEqualsCI(_typeBuf, "A") || strEqualsCI(_typeBuf, "ALL")) { out = 0; return true; }
        if (strEqualsCI(_typeBuf, "1") || strEqualsCI(_typeBuf, "FIRST")) { out = 1; return true; }
        if (strEqualsCI(_typeBuf, "H") || strEqualsCI(_typeBuf, "HOLD")) { out = 2; return true; }
        if (strEqualsCI(_typeBuf, "1L") || strEqualsCI(_typeBuf, "FIRSTLAST")) { out = 3; return true; }
        break;
    }
    case Layer::HarmonyRoleOverride: {
        if (strEqualsCI(_typeBuf, "SEQ") || strEqualsCI(_typeBuf, "S")) { out = 0; return true; }
        if (strEqualsCI(_typeBuf, "ROOT") || strEqualsCI(_typeBuf, "R")) { out = 1; return true; }
        if (strEqualsCI(_typeBuf, "3RD") || strEqualsCI(_typeBuf, "3")) { out = 2; return true; }
        if (strEqualsCI(_typeBuf, "5TH") || strEqualsCI(_typeBuf, "5")) { out = 3; return true; }
        if (strEqualsCI(_typeBuf, "7TH") || strEqualsCI(_typeBuf, "7")) { out = 4; return true; }
        if (strEqualsCI(_typeBuf, "OFF")) { out = 5; return true; }
        break;
    }
    case Layer::InversionOverride: {
        if (strEqualsCI(_typeBuf, "SEQ") || strEqualsCI(_typeBuf, "S")) { out = 0; return true; }
        if (strEqualsCI(_typeBuf, "ROOT") || strEqualsCI(_typeBuf, "R")) { out = 1; return true; }
        if (strEqualsCI(_typeBuf, "1ST") || strEqualsCI(_typeBuf, "1")) { out = 2; return true; }
        if (strEqualsCI(_typeBuf, "2ND") || strEqualsCI(_typeBuf, "2")) { out = 3; return true; }
        if (strEqualsCI(_typeBuf, "3RD") || strEqualsCI(_typeBuf, "3")) { out = 4; return true; }
        break;
    }
    case Layer::Condition: {
        // Try short names from Types::conditionInfos first
        for (int i = 0; i < int(Types::Condition::Loop); ++i) {
            const auto &info = Types::conditionInfos[i];
            if (strEqualsCI(_typeBuf, info.short1) || strEqualsCI(_typeBuf, info.name)) {
                out = i;
                return true;
            }
        }
        // Loop conditions: "1:3", "2:4", "!1:3" etc.
        // Parse manually or skip for v1
        break;
    }
    default:
        break;
    }

    return false;
}
```

**Note on Condition loops:** The loop conditions (`1:3`, `2:5`, `!1:4`) are harder to parse. For v1, the numeric fallback handles them (user types `7` for `Loop3:3` or whatever index). The named parser handles the common ones (`Fill`, `Pre`, `First`, `!Fill`, etc.). Loop condition naming can be added in v2.

### Step 7: Cancel Typing on Selection Change

In `draw()`, before the detail check:

```cpp
    if (_typing && _stepSelection.none()) {
        _typing = false;
        _typeLen = 0;
        _typeBuf[0] = '\0';
    }
```

Also cancel when switching layers (optional — in `switchLayer()`):

```cpp
void NoteSequenceEditPage::switchLayer(int functionKey, bool shift) {
    _typing = false;
    _typeLen = 0;
    _typeBuf[0] = '\0';
    // ... existing switchLayer logic ...
}
```

### Step 8: Draw the Typing Buffer (`drawDetail()`)

```cpp
void NoteSequenceEditPage::drawDetail(Canvas &canvas, const NoteSequence::Step &step) {
    FixedStringBuilder<32> str;

    if (_typing) {
        WindowPainter::drawFrame(canvas, 64, 16, 128, 32);
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(Color::Bright);

        // Show typed buffer
        str("%s", _typeBuf);
        if (_stepSelection.count() > 1) {
            str(" *%zu", _stepSelection.count());
        }
        canvas.setFont(Font::Small);
        canvas.drawTextCentered(64, 16, 64, 24, str);

        // Hint based on layer
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Bright);
        switch (layer()) {
        case Layer::Note:
        case Layer::NoteVariationRange:
            canvas.drawTextCentered(64, 42, 64, 8, "C#4 / 12");
            break;
        case Layer::Condition:
            canvas.drawTextCentered(64, 42, 64, 8, "fill / 3");
            break;
        case Layer::GateMode:
            canvas.drawTextCentered(64, 42, 64, 8, "hold / 2");
            break;
        case Layer::HarmonyRoleOverride:
            canvas.drawTextCentered(64, 42, 64, 8, "root / 3");
            break;
        case Layer::InversionOverride:
            canvas.drawTextCentered(64, 42, 64, 8, "1st / 2");
            break;
        default:
            canvas.drawTextCentered(64, 42, 64, 8, "ENTER to set");
            break;
        }
        return;
    }

    // ... rest of existing drawDetail() unchanged ...
}
```

### Step 9: Keep Detail Popup Alive While Typing

In `draw()`, add `_typing` guard to the auto-hide logic:

```cpp
        if (!_typing && _stepSelection.isPersisted() && os::ticks() > _showDetailTicks + os::time::ms(500)) {
            _showDetail = false;
        }
```

---

## Parser Reference

### Note Name Parser

| Input | Parsed As | MIDI | Scale-relative |
|-------|-----------|------|----------------|
| `C` | C4 | 60 | 0 (in C major) |
| `C#` | C#4 | 61 | +1 semitone |
| `Db` | Db4 | 61 | +1 semitone |
| `C4` | C4 | 60 | 0 |
| `C#3` | C#3 | 49 | -11 |
| `Eb5` | Eb5 | 75 | +15 |

**Accidentals:** `#`, `s`, `S` = sharp; `b`, `f`, `B`, `F` = flat.

**Octave:** Optional, defaults to 4. Supports 0-99 (two digits).

**Scale handling:** Uses `scale.noteFromVolts((midiNote - 60) / 12.0f)` — identical to the existing MIDI input handler in `NoteSequenceEditPage::midi()`. In a non-chromatic scale, `C#4` may map to the same scale step as `C4` if the scale has no sharp.

### Enum Name Parser

#### GateMode
| Input | Value |
|-------|-------|
| `a`, `all` | 0 (All) |
| `1`, `first` | 1 (First) |
| `h`, `hold` | 2 (Hold) |
| `1l`, `firstlast` | 3 (FirstLast) |

#### HarmonyRoleOverride
| Input | Value |
|-------|-------|
| `seq`, `s` | 0 |
| `root`, `r` | 1 |
| `3rd`, `3` | 2 |
| `5th`, `5` | 3 |
| `7th`, `7` | 4 |
| `off` | 5 |

#### InversionOverride
| Input | Value |
|-------|-------|
| `seq`, `s` | 0 |
| `root`, `r` | 1 |
| `1st`, `1` | 2 |
| `2nd`, `2` | 3 |
| `3rd`, `3` | 4 |

#### Condition (named conditions only)
| Input | Value |
|-------|-------|
| `off`, `-` | 0 |
| `fill`, `f` | 1 |
| `!fill`, `!f` | 2 |
| `pre`, `p` | 3 |
| `!pre`, `!p` | 4 |
| `first`, `1` | 5 |
| `!first`, `!1` | 6 |

Loop conditions (`1:3`, `2:4`, etc.) use numeric fallback for v1.

---

## Edge Cases

1. **Note name on non-note layer** → falls through to enum parser, then numeric. Typing `C` on Gate layer = numeric parse fails, nothing happens.
2. **Unknown enum name** → falls through to numeric parse. If that fails too, input is ignored (buffer stays visible, no change).
3. **Out-of-scale note** → `noteFromVolts()` finds the closest scale note. Same behavior as MIDI input.
4. **Very large numbers** → capped at 999 in numeric parser, then clamped by `layerRange()`.
5. **Buffer overflow** → 8-char limit, extra chars ignored.
6. **Empty buffer + Enter** → ignored.
7. **Layer switch mid-type** → typing cancels (buffer cleared). Prevents applying a note name to a gate mode layer.

---

## Future Extensions (Out of Scope)

- **Loop condition names:** Parse `1:3`, `!2:5` format for condition loops
- **Relative entry:** `+5` or `-3` to nudge from current value
- **Quick presets:** Number row `1-8` sets evenly-distributed values
- **Chord entry:** Type chord names (`Cm7`, `F#maj7`) to set harmony role + note simultaneously

---

## Testing Checklist

- [ ] Type `5` `Enter` on Note layer → +5 semitones
- [ ] Type `-3` `Enter` on Note layer → -3 semitones
- [ ] Type `C#4` `Enter` on Note layer → C#4 (scale-relative)
- [ ] Type `Eb3` `Enter` on Note layer → Eb3 (scale-relative)
- [ ] Type `fill` `Enter` on Condition layer → Fill condition
- [ ] Type `hold` `Enter` on GateMode layer → Hold mode
- [ ] Type `root` `Enter` on HarmonyRoleOverride → Root role
- [ ] Type `1st` `Enter` on InversionOverride → 1st inversion
- [ ] Type `999` on GateProbability layer → clamps to 7
- [ ] Press `Backspace` → removes last char
- [ ] Press `Escape` mid-type → cancels
- [ ] Release all step selections → typing cancels
- [ ] Switch layer mid-type → typing cancels
- [ ] Detail popup stays visible while typing
- [ ] Hardware build → no RAM/flash impact beyond ~40 bytes
