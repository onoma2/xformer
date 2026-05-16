# USB Keyboard Function Keys Extraction — Plan

## Goal
Replace 12-15 lines of duplicated switch block in 17 pages with a single `BasePage::handleFunctionKeys()` call. No behavior change, no RAM increase, pure structural refactor.

## Design

### New method on BasePage

```cpp
// BasePage.h
// Returns true if event was a handled function key (F1-F5 → pressFunctionButton).
// Pages that want custom F-key handling can call this and fall through for other keys.
// Pages that just need F1-F5 + BasePage fallback don't need to override keyboard() at all.
bool handleFunctionKeys(KeyboardEvent &event);
```

```cpp
// BasePage.cpp
bool BasePage::handleFunctionKeys(KeyboardEvent &event) {
    if (event.keycode() >= KeyboardEvent::KeyF1 && event.keycode() <= KeyboardEvent::KeyF5) {
        pressFunctionButton(event.keycode() - KeyboardEvent::KeyF1, event.shift());
        event.consume();
        return true;
    }
    return false;
}
```

### Refactored page patterns

**Pattern A (10 pages) →**
```cpp
void SomePage::keyboard(KeyboardEvent &event) {
    if (handleFunctionKeys(event)) return;
    BasePage::keyboard(event);
}
```

**Pattern B (5 pages) →**
```cpp
void SomePage::keyboard(KeyboardEvent &event) {
    if (!handleFunctionKeys(event)) {
        ListPage::keyboard(event);
    }
}
```

**Pattern C / custom pages** — call `handleFunctionKeys(event)` at the top, then handle remaining cases:
```cpp
void PatternPage::keyboard(KeyboardEvent &event) {
    if (handleFunctionKeys(event)) return;  // handles F3-F5
    switch (event.keycode()) {
    case KeyboardEvent::KeyF1: ... ; break;
    case KeyboardEvent::KeyF2: ... ; break;
    default: BasePage::keyboard(event); break;
    }
}
```

## Scope boundaries (NOT covered)
- ContextMenuPage F1-F5 → closeAndCallback: separate concern, different dispatch target, extract only if trivial
- TextInputPage: too specialized to benefit
- TopPage: global shortcuts, entirely custom — out of scope
- MonitorPage F6: stays inline (only one page uses it)

## Steps

### Phase 1: BasePage helper
- [ ] Add `handleFunctionKeys(KeyboardEvent &event) -> bool` to BasePage.h
- [ ] Implement in BasePage.cpp
- [ ] Verify no existing callers broken (build check)

### Phase 2: Pattern A pages (10 files)
For each file:
- [ ] Replace switch block with `if (handleFunctionKeys(event)) return; BasePage::keyboard(event);`
- [ ] Remove `#include` of KeyboardEvent? (probably still needed)

Pages: CurveSequenceEditPage, NoteSequenceEditPage, IndexedSequenceEditPage, DiscreteMapSequencePage, SongPage, IndexedRouteConfigPage, IndexedMathPage, TuesdayEditPage, MonitorPage (keep F6 inline), ModulatorPage

### Phase 3: Pattern B pages (5 files)
For each file:
- [ ] Replace switch block with `if (!handleFunctionKeys(event)) { ListPage::keyboard(event); }`

Pages: TrackPage, SystemPage, RoutingPage, LayoutPage

### Phase 4: Pattern C pages (2 files)
- [ ] PatternPage: add `handleFunctionKeys(event)` at top, keep custom F1/F2 cases
- [ ] PerformerPage: add `handleFunctionKeys(event)` at top, keep custom F1/F2/F4 cases

### Phase 5: Build verification
- [ ] `make sequencer` in `build/stm32/release` — must compile clean
- [ ] Verify `.data`, `.bss`, `.ccmram_bss` unchanged (structural refactor only)
- [ ] Check `sizeof(Track)` and `sizeof(Engine::TrackEngineContainer)` — must be flat

### Follow-up / optional
- [ ] Evaluate ContextMenuPage F1-F5 → closeAndCallback for similar extraction
- [ ] Evaluate Enter/Escape → closeWithResult for helper extraction (FileSelectPage, ConfirmationPage — only 2 pages, probably not worth it)

## RAM budget check
Expected change: 0 B. `handleFunctionKeys` adds no state, just moves code. Verify with ARM `sizeof` probes in release build.
