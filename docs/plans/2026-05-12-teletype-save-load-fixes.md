# T9type Save/Load Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve stack buffer overflows during Teletype script text export and streamline binary `.PRO` serialization by removing backward compatibility code for legacy I/O mapping.

**Architecture:**
- **FileManager:** We will replace the vulnerable `char buffer[32]` with `char buffer[256]` in the `writeScriptSection` and `writeScriptSectionRaw` functions. This size matches the existing `ttLineBuffer` read limit and is safe for the File Task stack (2048 bytes total).
- **TeletypeTrack:** The `read` and `write` methods will be simplified to operate entirely on the `PatternSlot` array, dropping the separate legacy I/O struct reads/writes that originally occupied the top of the file payload.
- **Testing:** This project relies on a custom `UnitTest.h` framework. Simulator-first verification (`build/sim/debug`) is mandatory.

**Tech Stack:** C++, STM32/FreeRTOS constraints, custom Binary/Text Serializers.

---

### Task 1: Fix Text Export Buffer Overflow in FileManager

**Files:**
- Modify: `src/apps/sequencer/model/FileManager.cpp:555-585`

- [ ] **Step 1: Update `writeScriptSection` write buffer**
In `src/apps/sequencer/model/FileManager.cpp`, locate `writeScriptSection` and change the buffer size.

```cpp
static void writeScriptSection(fs::FileWriter &writer, const char *header,
                               const std::array<tele_command_t, TeletypeTrack::ScriptLineCount> &lines,
                               uint8_t length) {
    writeLine(writer, header);
    char buffer[256] = {};
    const uint8_t count = clamp<uint8_t>(length, 0, TeletypeTrack::ScriptLineCount);
    for (uint8_t i = 0; i < count; ++i) {
        buffer[0] = '\0';
        if (lines[i].length > 0) {
            print_command(&lines[i], buffer);
        }
        writeLine(writer, buffer);
    }
}
```

- [ ] **Step 2: Update `writeScriptSectionRaw` write buffer**
In the same file, locate `writeScriptSectionRaw` and apply the same fix.

```cpp
static void writeScriptSectionRaw(fs::FileWriter &writer, const char *header,
                                  const tele_command_t *lines, uint8_t length) {
    writeLine(writer, header);
    char buffer[256] = {};
    const uint8_t count = clamp<uint8_t>(length, 0, TeletypeTrack::ScriptLineCount);
    for (uint8_t i = 0; i < count; ++i) {
        buffer[0] = '\0';
        if (lines[i].length > 0) {
            print_command(&lines[i], buffer);
        }
        writeLine(writer, buffer);
    }
}
```

- [ ] **Step 3: Compile Simulator**

Run: `make -C build/sim/debug`
Expected: Successful compilation without warnings.

- [ ] **Step 4: Commit**

```bash
git add src/apps/sequencer/model/FileManager.cpp
git commit -m "fix(teletype): prevent stack overflow during script export by expanding line buffer to 256 bytes"
```

---

### Task 2: Streamline TeletypeTrack::write

**Files:**
- Modify: `src/apps/sequencer/model/TeletypeTrack.cpp:180-268`

- [ ] **Step 1: Simplify binary `write` implementation**
Remove the legacy `activeSlot()` I/O dump at the top of the function. The loop over `EditableScriptCount` is also slightly bloated. We will streamline the whole function to only write scripts, patterns, and the `_patternSlots` array.

```cpp
void TeletypeTrack::write(VersionedSerializedWriter &writer) const {
    auto *self = const_cast<TeletypeTrack *>(this);
    self->syncToActiveSlot();

    scene_state_t &state = const_cast<scene_state_t &>(_state);
    for (int script = 0; script < EditableScriptCount; ++script) {
        writer.write(state.scripts[script].l);
        for (int line = 0; line < ScriptLineCount; ++line) {
            const tele_command_t *cmd = ss_get_script_command(&state, script, line);
            writer.write(cmd, sizeof(tele_command_t));
        }
    }
    for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
        writer.write(_state.patterns[pattern]);
    }

    writer.write(_activePatternSlot);
    for (int slotIdx = 0; slotIdx < PatternSlotCount; ++slotIdx) {
        const auto &patternSlot = _patternSlots[slotIdx];
        writer.write(patternSlot.slotScriptLength);
        writer.write(patternSlot.metroLength);
        writer.write(patternSlot.slotScript);
        writer.write(patternSlot.metro);
        for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
            writer.write(patternSlot.patterns[pattern]);
        }
        for (int i = 0; i < 4; ++i) {
            writer.write(uint8_t(patternSlot.triggerInputSource[i]));
        }
        writer.write(uint8_t(patternSlot.cvInSource));
        writer.write(uint8_t(patternSlot.cvParamSource));
        writer.write(uint8_t(patternSlot.cvXSource));
        writer.write(uint8_t(patternSlot.cvYSource));
        writer.write(uint8_t(patternSlot.cvZSource));
        writer.write(uint8_t(patternSlot.cvTSource));
        for (int i = 0; i < 4; ++i) {
            writer.write(uint8_t(patternSlot.triggerOutputDest[i]));
        }
        for (int i = 0; i < 4; ++i) {
            writer.write(uint8_t(patternSlot.cvOutputDest[i]));
        }
        patternSlot.midiSource.write(writer);
        writer.write(uint8_t(patternSlot.bootScriptIndex));
        writer.write(uint8_t(patternSlot.timeBase));
        writer.write(patternSlot.clockDivisor);
        writer.write(patternSlot.clockMultiplier);
        for (int i = 0; i < 4; ++i) {
            writer.write(uint8_t(patternSlot.cvOutputRange[i]));
            writer.write(patternSlot.cvOutputOffset[i]);
        }
        for (int i = 0; i < 4; ++i) {
            writer.write(patternSlot.cvOutputQuantizeScale[i]);
            writer.write(patternSlot.cvOutputRootNote[i]);
        }
        writer.write(uint8_t(patternSlot.resetMetroOnLoad));
    }
}
```

- [ ] **Step 2: Compile Simulator**

Run: `make -C build/sim/debug`
Expected: Successful compilation without warnings.

- [ ] **Step 3: Commit**

```bash
git add src/apps/sequencer/model/TeletypeTrack.cpp
git commit -m "refactor(teletype): streamline binary write by removing legacy slot duplication"
```

---

### Task 3: Streamline TeletypeTrack::read

**Files:**
- Modify: `src/apps/sequencer/model/TeletypeTrack.cpp:269-400`

- [ ] **Step 1: Simplify binary `read` implementation**
Remove the legacy `legacySlot` data reading block from the start of the `read` method to match the new `write` payload structure.

```cpp
void TeletypeTrack::read(VersionedSerializedReader &reader) {
    clear();

    // Read VM scripts
    for (int script = 0; script < EditableScriptCount; ++script) {
        ss_clear_script(&_state, script);
        uint8_t scriptLen = 0;
        reader.read(scriptLen);
        _state.scripts[script].l = clamp<uint8_t>(scriptLen, 0, ScriptLineCount);
        for (int line = 0; line < ScriptLineCount; ++line) {
            reader.read(_state.scripts[script].c[line], 0);
        }
    }
    for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
        reader.read(_state.patterns[pattern], 0);
    }

    // Read pattern slots
    uint8_t activeSlotVal = 0;
    reader.read(activeSlotVal);
    _activePatternSlot = clamp<uint8_t>(activeSlotVal, 0, PatternSlotCount - 1);
    for (int slotIdx = 0; slotIdx < PatternSlotCount; ++slotIdx) {
        auto &patternSlot = _patternSlots[slotIdx];
        reader.read(patternSlot.slotScriptLength, 0);
        reader.read(patternSlot.metroLength, 0);
        reader.read(patternSlot.slotScript, 0);
        reader.read(patternSlot.metro, 0);
        for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
            reader.read(patternSlot.patterns[pattern], 0);
        }
        for (int i = 0; i < 4; ++i) {
            uint8_t val;
            reader.read(val, 0);
            patternSlot.triggerInputSource[i] = ModelUtils::clampedEnum(TriggerInputSource(val));
        }
        uint8_t cvInVal2, cvParamVal2, cvXVal2, cvYVal2, cvZVal2, cvTVal2;
        reader.read(cvInVal2, 0);
        reader.read(cvParamVal2, 0);
        reader.read(cvXVal2, 0);
        reader.read(cvYVal2, 0);
        reader.read(cvZVal2, 0);
        reader.read(cvTVal2, 0);
        patternSlot.cvInSource = ModelUtils::clampedEnum(CvInputSource(cvInVal2));
        patternSlot.cvParamSource = ModelUtils::clampedEnum(CvInputSource(cvParamVal2));
        patternSlot.cvXSource = ModelUtils::clampedEnum(CvInputSource(cvXVal2));
        patternSlot.cvYSource = ModelUtils::clampedEnum(CvInputSource(cvYVal2));
        patternSlot.cvZSource = ModelUtils::clampedEnum(CvInputSource(cvZVal2));
        patternSlot.cvTSource = ModelUtils::clampedEnum(CvInputSource(cvTVal2));
        for (int i = 0; i < 4; ++i) {
            uint8_t val;
            reader.read(val, 0);
            patternSlot.triggerOutputDest[i] = ModelUtils::clampedEnum(TriggerOutputDest(val));
        }
        for (int i = 0; i < 4; ++i) {
            uint8_t val;
            reader.read(val, 0);
            patternSlot.cvOutputDest[i] = ModelUtils::clampedEnum(CvOutputDest(val));
        }
        patternSlot.midiSource.read(reader);
        uint8_t bootVal = 0;
        reader.read(bootVal, 0);
        patternSlot.bootScriptIndex = clamp<uint8_t>(bootVal, 0, ScriptSlotCount - 1);
        uint8_t tbVal = 0;
        reader.read(tbVal, 0);
        patternSlot.timeBase = ModelUtils::clampedEnum(TimeBase(tbVal));
        reader.read(patternSlot.clockDivisor, 0);
        patternSlot.clockDivisor = ModelUtils::clampDivisor(patternSlot.clockDivisor);
        reader.read(patternSlot.clockMultiplier, 0);
        patternSlot.clockMultiplier = clamp<int16_t>(patternSlot.clockMultiplier, 50, 150);
        for (int i = 0; i < 4; ++i) {
            uint8_t rangeVal;
            reader.read(rangeVal, 0);
            patternSlot.cvOutputRange[i] = ModelUtils::clampedEnum(Types::VoltageRange(rangeVal));
            reader.read(patternSlot.cvOutputOffset[i], 0);
        }
        for (int i = 0; i < 4; ++i) {
            reader.read(patternSlot.cvOutputQuantizeScale[i], 0);
            reader.read(patternSlot.cvOutputRootNote[i], 0);
        }
        uint8_t resetMetro = 0;
        reader.read(resetMetro, 0);
        patternSlot.resetMetroOnLoad = resetMetro != 0;
    }
}
```

- [ ] **Step 2: Compile Simulator**

Run: `make -C build/sim/debug`
Expected: Successful compilation without warnings.

- [ ] **Step 3: Commit**

```bash
git add src/apps/sequencer/model/TeletypeTrack.cpp
git commit -m "refactor(teletype): streamline binary read by removing legacy slot parsing"
```

---

### Task 4: Bump Project Version

**Files:**
- Modify: `src/apps/sequencer/model/ProjectVersion.h:58-65`

- [ ] **Step 1: Bump the Last/Latest version**
Because we dropped data from the binary format (breaking backwards compatibility), we should increment the project version so future files are written and verified with the new tag.

```cpp
    // added NoteSequence::Mode and NoteSequence::divisorY
    // added CvRoute settings
    // added Project::busSafety
    Version34 = 34,

    // streamlined TeletypeTrack storage format
    Version35 = 35,

    // automatically derive latest version
    Last,
    Latest = Last - 1,
```

- [ ] **Step 2: Compile Simulator**

Run: `make -C build/sim/debug`
Expected: Successful compilation.

- [ ] **Step 3: Commit**

```bash
git add src/apps/sequencer/model/ProjectVersion.h
git commit -m "chore(project): bump project version to 35 for teletype binary changes"
```

---
