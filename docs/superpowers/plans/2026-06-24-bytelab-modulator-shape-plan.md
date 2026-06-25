# ByteLab Modulator Shape Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add ByteLab as a per-slot modulator shape that byte-shapes another non-ByteLab modulator source through gain/bias, divisor hold, bit operation, LUT/bypass, mix, depth, and offset.

**Architecture:** ByteLab is not a Geode-style bank mode. `Modulator` owns the persisted `ByteLabConfig`; `ModulatorEngine` owns per-slot `ByteLabState` and pure byte/LUT helpers; `Engine.cpp` updates normal modulators first, then ByteLab slots from allowed non-ByteLab modulator inputs. `ModulatorPage` exposes two ByteLab pages and a bit-cursor editor; `Page+Encoder click` toggles the selected slot between ByteLab and the previous normal shape.

**Tech Stack:** C++11 firmware, existing unit test harness in `src/tests/unit/sequencer`, STM32 release build, Python `ui-preview/` renderer for OLED validation.

---

## Source Spec

- Spec: `docs/superpowers/specs/2026-06-24-leibniz-modulator-bank-design.md`
- Behavior prototype: `ui-preview/bytelab-live/index.html`
- Current modulator model: `src/apps/sequencer/model/Modulator.h`
- Current modulator engine: `src/apps/sequencer/engine/ModulatorEngine.h`
- Current modulator UI: `src/apps/sequencer/ui/pages/ModulatorPage.cpp`

## File Structure

- Create: `src/apps/sequencer/model/ByteLabConfig.h`
  - Persisted per-slot ByteLab settings and editing helpers.
- Modify: `src/apps/sequencer/model/Modulator.h`
  - Add `Shape::ByteLab`, embed `ByteLabConfig`, serialize it, and remember previous normal shape for Page+Encoder toggle.
- Modify: `src/apps/sequencer/engine/ModulatorEngine.h`
  - Add `ByteLabState`, pure byte helpers, LUT helpers, source validation, and per-slot ByteLab tick.
- Modify: `src/apps/sequencer/engine/Engine.cpp`
  - Two-pass modulator update: normal shapes first, ByteLab second.
- Modify: `src/apps/sequencer/ui/pages/ModulatorPage.cpp`
  - ByteLab footer pages, bit editor, Page+Encoder toggle, JustF guard.
- Modify: `ui-preview/pages_modulator.py`
  - Update ByteLab OLED preview to match final two-page labels and bit cursor editor.
- Modify: `ui-preview/generate.py`
  - Add/keep ByteLab preview slugs.
- Test: `src/tests/unit/sequencer/TestByteLabConfig.cpp`
- Test: `src/tests/unit/sequencer/TestByteLabEngine.cpp`
- Test: extend `src/tests/unit/sequencer/TestModulator.cpp`
- Test: add focused UI behavior tests only if the local UI test harness already has ModulatorPage coverage; otherwise use `ui-preview/` render as the UI gate.

## TDD Tasks

### Task 1: ByteLabConfig Model

**Files:**
- Create: `src/apps/sequencer/model/ByteLabConfig.h`
- Test: `src/tests/unit/sequencer/TestByteLabConfig.cpp`

- [ ] **Step 1: Write failing default/clamp tests**

Add `src/tests/unit/sequencer/TestByteLabConfig.cpp`:

```cpp
#include "UnitTest.h"

#include "apps/sequencer/model/ByteLabConfig.h"
#include "apps/sequencer/model/Routing.h"

UNIT_TEST("ByteLabConfig") {

CASE("clear defaults to neutral direct shaper") {
    ByteLabConfig c;
    c.clear();
    expectEqual(int(c.inputSource()), int(Routing::Source::Mod1), "default source M1");
    expectEqual(c.gain(), 128, "unity input gain");
    expectEqual(c.bias(), 0, "zero input bias");
    expectEqual(c.divisor(), 1, "no hold");
    expectEqual(int(c.bitOp()), int(ByteLabConfig::BitOp::Xor), "XOR default");
    expectEqual(c.mask(), 0, "mask bypass");
    expectEqual(int(c.lut()), int(ByteLabConfig::Lut::Bypass), "LUT bypass");
    expectEqual(c.mix(), 127, "fully wet");
    expectEqual(c.depth(), 127, "full output depth");
    expectEqual(c.offset(), 0, "zero output offset");
}

CASE("setters clamp to firmware ranges") {
    ByteLabConfig c;
    c.setGain(-1);       expectEqual(c.gain(), 0, "gain low clamp");
    c.setGain(999);      expectEqual(c.gain(), 255, "gain high clamp");
    c.setBias(-200);     expectEqual(c.bias(), -128, "bias low clamp");
    c.setBias(200);      expectEqual(c.bias(), 127, "bias high clamp");
    c.setDivisor(0);     expectEqual(c.divisor(), 1, "divisor low clamp");
    c.setDivisor(99);    expectEqual(c.divisor(), 8, "divisor high clamp");
    c.setMask(999);      expectEqual(c.mask(), 255, "mask high clamp");
    c.setMix(-1);        expectEqual(c.mix(), 0, "mix low clamp");
    c.setMix(999);       expectEqual(c.mix(), 127, "mix high clamp");
    c.setDepth(999);     expectEqual(c.depth(), 127, "depth high clamp");
    c.setOffset(-200);   expectEqual(c.offset(), -64, "offset low clamp");
    c.setOffset(200);    expectEqual(c.offset(), 63, "offset high clamp");
}

}
```

- [ ] **Step 2: Run test and verify RED**

Run:

```bash
cd src/tests/unit
make TestByteLabConfig
```

Expected: compile failure because `ByteLabConfig.h` does not exist.

- [ ] **Step 3: Add minimal ByteLabConfig**

Create `src/apps/sequencer/model/ByteLabConfig.h`:

```cpp
#pragma once

#include "Routing.h"
#include "Serialize.h"
#include "core/math/Math.h"

#include <cstdint>

class ByteLabConfig {
public:
    enum class BitOp : uint8_t { Xor, And, Or, MsbFold, Last };
    enum class Lut : uint8_t { Bypass, Fold, Sine, Steps, Cheby, Last };

    ByteLabConfig() { clear(); }

    Routing::Source inputSource() const { return _inputSource; }
    void setInputSource(Routing::Source s) { _inputSource = s; }

    int gain() const { return _gain; }
    void setGain(int v) { _gain = clamp(v, 0, 255); }

    int bias() const { return _bias; }
    void setBias(int v) { _bias = clamp(v, -128, 127); }

    int divisor() const { return _divisor; }
    void setDivisor(int v) { _divisor = clamp(v, 1, 8); }

    BitOp bitOp() const { return _bitOp; }
    void setBitOp(BitOp op) { _bitOp = op; if (_bitOp >= BitOp::Last) _bitOp = BitOp::Xor; }

    int mask() const { return _mask; }
    void setMask(int v) { _mask = clamp(v, 0, 255); }

    Lut lut() const { return _lut; }
    void setLut(Lut lut) { _lut = lut; if (_lut >= Lut::Last) _lut = Lut::Bypass; }

    int mix() const { return _mix; }
    void setMix(int v) { _mix = clamp(v, 0, 127); }

    int depth() const { return _depth; }
    void setDepth(int v) { _depth = clamp(v, 0, 127); }

    int offset() const { return _offset; }
    void setOffset(int v) { _offset = clamp(v, -64, 63); }

    void clear() {
        _inputSource = Routing::Source::Mod1;
        _gain = 128;
        _bias = 0;
        _divisor = 1;
        _bitOp = BitOp::Xor;
        _mask = 0;
        _lut = Lut::Bypass;
        _mix = 127;
        _depth = 127;
        _offset = 0;
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_inputSource);
        writer.write(_gain);
        writer.write(_bias);
        writer.write(_divisor);
        writer.write(_bitOp);
        writer.write(_mask);
        writer.write(_lut);
        writer.write(_mix);
        writer.write(_depth);
        writer.write(_offset);
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_inputSource);
        reader.read(_gain);
        reader.read(_bias);
        reader.read(_divisor);
        reader.read(_bitOp);
        reader.read(_mask);
        reader.read(_lut);
        reader.read(_mix);
        reader.read(_depth);
        reader.read(_offset);

        setGain(_gain);
        setBias(_bias);
        setDivisor(_divisor);
        setBitOp(_bitOp);
        setMask(_mask);
        setLut(_lut);
        setMix(_mix);
        setDepth(_depth);
        setOffset(_offset);
    }

private:
    Routing::Source _inputSource = Routing::Source::Mod1;
    uint8_t _gain = 128;
    int8_t _bias = 0;
    uint8_t _divisor = 1;
    BitOp _bitOp = BitOp::Xor;
    uint8_t _mask = 0;
    Lut _lut = Lut::Bypass;
    uint8_t _mix = 127;
    uint8_t _depth = 127;
    int8_t _offset = 0;
};
```

- [ ] **Step 4: Run test and verify GREEN**

Run:

```bash
cd src/tests/unit
make TestByteLabConfig
```

Expected: PASS.

### Task 2: Add ByteLab Shape To Modulator

**Files:**
- Modify: `src/apps/sequencer/model/Modulator.h`
- Test: extend `src/tests/unit/sequencer/TestModulator.cpp`

- [ ] **Step 1: Write failing tests**

Add to `UNIT_TEST("Modulator")` in `src/tests/unit/sequencer/TestModulator.cpp`:

```cpp
CASE("ByteLab shape name and config are available per slot") {
    Modulator m;
    m.clear();
    m.setShape(Modulator::Shape::ByteLab);
    expectTrue(std::strcmp(Modulator::shapeName(m.shape()), "ByteLab") == 0, "shape name");
    expectEqual(m.byteLab().gain(), 128, "embedded ByteLabConfig default");
}

CASE("toggleByteLab stores and restores previous normal shape") {
    Modulator m;
    m.clear();
    m.setShape(Modulator::Shape::Triangle);
    m.toggleByteLabShape();
    expectEqual(int(m.shape()), int(Modulator::Shape::ByteLab), "entered ByteLab");
    m.toggleByteLabShape();
    expectEqual(int(m.shape()), int(Modulator::Shape::Triangle), "restored previous shape");
}
```

- [ ] **Step 2: Run test and verify RED**

Run:

```bash
cd src/tests/unit
make TestModulator
```

Expected: compile failure for missing `Shape::ByteLab`, `byteLab()`, and `toggleByteLabShape()`.

- [ ] **Step 3: Implement minimal model integration**

Modify `src/apps/sequencer/model/Modulator.h`:

```cpp
#include "ByteLabConfig.h"
```

Add `ByteLab` before `Last` in `enum class Shape`:

```cpp
ByteLab,
Last
```

Add to `shapeName`:

```cpp
case Shape::ByteLab: return "ByteLab";
```

Add accessors and toggle:

```cpp
ByteLabConfig &byteLab() { return _byteLab; }
const ByteLabConfig &byteLab() const { return _byteLab; }

void toggleByteLabShape() {
    if (_shape == Shape::ByteLab) {
        _shape = _previousNormalShape;
    } else {
        if (_shape != Shape::ByteLab) _previousNormalShape = _shape;
        _shape = Shape::ByteLab;
    }
}
```

Add to `clear()`:

```cpp
_previousNormalShape = Shape::Sine;
_byteLab.clear();
```

Add private fields:

```cpp
Shape _previousNormalShape = Shape::Sine;
ByteLabConfig _byteLab;
```

- [ ] **Step 4: Run test and verify GREEN**

Run:

```bash
cd src/tests/unit
make TestModulator
```

Expected: PASS.

### Task 3: Serialize ByteLabConfig

**Files:**
- Modify: `src/apps/sequencer/model/Modulator.h`
- Test: extend `src/tests/unit/sequencer/TestModulator.cpp`

- [ ] **Step 1: Write failing roundtrip test**

Add these includes near the top of `src/tests/unit/sequencer/TestModulator.cpp`:

```cpp
#include "MemoryReaderWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "core/io/VersionedSerializedWriter.h"
```

Add this case to `UNIT_TEST("Modulator")`:

```cpp
CASE("ByteLab config serializes with Modulator") {
    Modulator a;
    a.clear();
    a.setShape(Modulator::Shape::ByteLab);
    a.byteLab().setGain(200);
    a.byteLab().setBias(-17);
    a.byteLab().setDivisor(3);
    a.byteLab().setMask(0x5a);
    a.byteLab().setMix(64);
    a.byteLab().setDepth(90);
    a.byteLab().setOffset(-12);

    uint8_t data[128];
    MemoryWriter memoryWriter(data, sizeof(data));
    VersionedSerializedWriter writer(
        [&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); },
        1);
    a.write(writer);

    Modulator b;
    b.clear();
    MemoryReader memoryReader(data, sizeof(data));
    VersionedSerializedReader reader(
        [&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); },
        1);
    b.read(reader);

    expectEqual(int(b.shape()), int(Modulator::Shape::ByteLab), "shape roundtrip");
    expectEqual(b.byteLab().gain(), 200, "gain roundtrip");
    expectEqual(b.byteLab().bias(), -17, "bias roundtrip");
    expectEqual(b.byteLab().divisor(), 3, "divisor roundtrip");
    expectEqual(b.byteLab().mask(), 0x5a, "mask roundtrip");
    expectEqual(b.byteLab().mix(), 64, "mix roundtrip");
    expectEqual(b.byteLab().depth(), 90, "depth roundtrip");
    expectEqual(b.byteLab().offset(), -12, "offset roundtrip");
}
```

- [ ] **Step 2: Run test and verify RED**

Run:

```bash
cd src/tests/unit
make TestModulator
```

Expected: FAIL because ByteLab fields are not serialized.

- [ ] **Step 3: Append ByteLab fields to Modulator serialization**

Modify `Modulator::write`:

```cpp
writer.write(_previousNormalShape);
_byteLab.write(writer);
```

Modify `Modulator::read` after existing reads:

```cpp
reader.read(_previousNormalShape);
_byteLab.read(reader);
_previousNormalShape = ModelUtils::clampedEnum(_previousNormalShape);
if (_previousNormalShape == Shape::ByteLab) _previousNormalShape = Shape::Sine;
```

Keep this as dev-branch serialization. Do not bump `ProjectVersion`.

- [ ] **Step 4: Run test and verify GREEN**

Run:

```bash
cd src/tests/unit
make TestModulator
```

Expected: PASS.

### Task 4: Pure ByteLab Engine Helpers

**Files:**
- Modify: `src/apps/sequencer/engine/ModulatorEngine.h`
- Test: `src/tests/unit/sequencer/TestByteLabEngine.cpp`

- [ ] **Step 1: Write failing helper tests**

Create `src/tests/unit/sequencer/TestByteLabEngine.cpp`:

```cpp
#include "UnitTest.h"

#include "apps/sequencer/engine/ModulatorEngine.h"
#include "apps/sequencer/model/ByteLabConfig.h"

UNIT_TEST("ByteLabEngine") {

CASE("byteLabAdc applies gain and bias before quantization") {
    expectEqual(ModulatorEngine::byteLabAdc(0.5f, 128, 0), 128, "unity midpoint");
    expectEqual(ModulatorEngine::byteLabAdc(0.0f, 128, 0), 0, "unity low");
    expectEqual(ModulatorEngine::byteLabAdc(1.0f, 128, 0), 255, "unity high");
    expectTrue(ModulatorEngine::byteLabAdc(0.5f, 255, 0) == 128, "gain keeps centre fixed");
    expectTrue(ModulatorEngine::byteLabAdc(0.5f, 128, 32) > 128, "positive bias raises centre");
}

CASE("byteLabBitOp applies mask operations") {
    using Op = ByteLabConfig::BitOp;
    expectEqual(ModulatorEngine::byteLabBitOp(0xaa, Op::Xor, 0xff), 0x55, "xor");
    expectEqual(ModulatorEngine::byteLabBitOp(0xaa, Op::And, 0x0f), 0x0a, "and");
    expectEqual(ModulatorEngine::byteLabBitOp(0xa0, Op::Or, 0x0f), 0xaf, "or");
    expectEqual(ModulatorEngine::byteLabBitOp(0x80, Op::MsbFold, 0x00), 0x7f, "msb fold mirrors upper half");
}

CASE("byteLabLut bypass and steps are deterministic") {
    using Lut = ByteLabConfig::Lut;
    expectEqual(ModulatorEngine::byteLabLut(0x42, Lut::Bypass), 0x42, "bypass");
    expectEqual(ModulatorEngine::byteLabLut(0x00, Lut::Steps), 0x00, "steps low");
    expectEqual(ModulatorEngine::byteLabLut(0xff, Lut::Steps), 0xff, "steps high");
    expectEqual(ModulatorEngine::byteLabLutTableSize(), 256, "fixed 256-entry tables");
}

CASE("byteLabOutput applies dry wet mix depth and offset") {
    expectEqual(ModulatorEngine::byteLabOutput(128, 255, 127, 127, 0), 127, "wet high");
    expectEqual(ModulatorEngine::byteLabOutput(0, 255, 0, 127, 0), 0, "dry low");
    expectTrue(ModulatorEngine::byteLabOutput(128, 255, 127, 64, 0) < 127, "lower depth reduces swing");
    expectTrue(ModulatorEngine::byteLabOutput(128, 128, 127, 127, 20) > 64, "positive offset raises");
}

}
```

- [ ] **Step 2: Run test and verify RED**

Run:

```bash
cd src/tests/unit
make TestByteLabEngine
```

Expected: compile failure for missing helper methods.

- [ ] **Step 3: Add pure helpers**

Add to `ModulatorEngine` public section in `src/apps/sequencer/engine/ModulatorEngine.h`:

```cpp
static uint8_t byteLabAdc(float source01, int gain, int bias) {
    float adc = (source01 - 0.5f) * (gain / 128.f) + 0.5f + (bias / 128.f);
    if (adc < 0.f) adc = 0.f;
    if (adc > 1.f) adc = 1.f;
    return uint8_t(adc * 255.f + 0.5f);
}

static uint8_t byteLabBitOp(uint8_t byte, ByteLabConfig::BitOp op, uint8_t mask) {
    switch (op) {
    case ByteLabConfig::BitOp::And: return byte & mask;
    case ByteLabConfig::BitOp::Or: return byte | mask;
    case ByteLabConfig::BitOp::MsbFold: return (byte & 0x80) ? uint8_t(255 - byte) ^ (mask & 0x7f) : byte ^ (mask & 0x7f);
    case ByteLabConfig::BitOp::Xor:
    case ByteLabConfig::BitOp::Last:
        break;
    }
    return byte ^ mask;
}

static uint8_t byteLabLut(uint8_t byte, ByteLabConfig::Lut lut) {
    static const uint8_t bypass[256] = {
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
        16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
        32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
        48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
        64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
        80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
        112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
        144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
        176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
        208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
        240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
    };
    static const uint8_t steps[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,
        72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,
        109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,
        145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,145,
        182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,
        218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,218,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
    };
    switch (lut) {
    case ByteLabConfig::Lut::Steps: return steps[byte];
    case ByteLabConfig::Lut::Sine:
    case ByteLabConfig::Lut::Cheby:
    case ByteLabConfig::Lut::Fold:
    case ByteLabConfig::Lut::Bypass:
    case ByteLabConfig::Lut::Last:
        break;
    }
    return bypass[byte];
}

static int byteLabLutTableSize() {
    return 256;
}

static uint8_t byteLabOutput(uint8_t dry, uint8_t wet, int mix, int depth, int offset) {
    int mixed = (int(dry) * (127 - mix) + int(wet) * mix) / 127;
    int centered = mixed - 64;
    int scaled = 64 + (centered * depth) / 127 + offset;
    return uint8_t(clamp(scaled, 0, 127));
}
```

Add include:

```cpp
#include "model/ByteLabConfig.h"
```

This step starts with `Bypass` and `Steps` as table-backed LUTs. Add `Fold`,
`Sine`, and `Cheby` as 256-entry `static const uint8_t[256]` tables before
marking ByteLab complete; do not compute those LUTs per sample at runtime.

- [ ] **Step 4: Run test and verify GREEN**

Run:

```bash
cd src/tests/unit
make TestByteLabEngine
```

Expected: PASS.

### Task 5: Per-Slot ByteLab Runtime State

**Files:**
- Modify: `src/apps/sequencer/engine/ModulatorEngine.h`
- Test: extend `src/tests/unit/sequencer/TestByteLabEngine.cpp`

- [ ] **Step 1: Write failing divisor/state test**

Add to `TestByteLabEngine.cpp`:

```cpp
CASE("byteLabTick holds output until divisor boundary") {
    ModulatorEngine eng;
    eng.reset();
    Modulator m;
    m.clear();
    m.setShape(Modulator::Shape::ByteLab);
    m.byteLab().setDivisor(3);
    m.byteLab().setMix(127);
    m.byteLab().setDepth(127);
    m.byteLab().setOffset(0);

    eng.tickByteLab(0, m, 0.0f);
    int a = eng.currentValue(0);
    eng.tickByteLab(0, m, 1.0f);
    int b = eng.currentValue(0);
    eng.tickByteLab(0, m, 1.0f);
    int c = eng.currentValue(0);

    expectEqual(a, b, "held before divisor boundary");
    expectTrue(c != b, "updated on divisor boundary");
}
```

- [ ] **Step 2: Run test and verify RED**

Run:

```bash
cd src/tests/unit
make TestByteLabEngine
```

Expected: compile failure for missing `tickByteLab`.

- [ ] **Step 3: Add state and tickByteLab**

Add private state:

```cpp
struct ByteLabState {
    uint8_t adc = 0;
    uint8_t masked = 0;
    uint8_t lut = 0;
    uint8_t counter = 0;
};

ByteLabState _byteLab[CONFIG_MODULATOR_COUNT];
```

In `reset()`, clear it:

```cpp
_byteLab[i] = ByteLabState();
```

Add public method:

```cpp
void tickByteLab(int index, const Modulator &modulator, float source01) {
    if (index < 0 || index >= CONFIG_MODULATOR_COUNT) return;
    const ByteLabConfig &cfg = modulator.byteLab();
    ByteLabState &s = _byteLab[index];
    int div = cfg.divisor();
    if (div < 1) div = 1;
    s.counter = uint8_t((s.counter + 1) % div);
    if (s.counter == 0) {
        s.adc = byteLabAdc(source01, cfg.gain(), cfg.bias());
        s.masked = byteLabBitOp(s.adc, cfg.bitOp(), uint8_t(cfg.mask()));
        s.lut = byteLabLut(s.masked, cfg.lut());
    }
    _currentValue[index] = byteLabOutput(s.adc, s.lut, cfg.mix(), cfg.depth(), cfg.offset());
}
```

- [ ] **Step 4: Run test and verify GREEN**

Run:

```bash
cd src/tests/unit
make TestByteLabEngine
```

Expected: PASS.

### Task 6: Engine Two-Pass Update And Source Guards

**Files:**
- Modify: `src/apps/sequencer/engine/Engine.cpp`
- Modify: `src/apps/sequencer/engine/ModulatorEngine.h`
- Test: add to `src/tests/unit/sequencer/TestByteLabEngine.cpp`

- [ ] **Step 1: Write source validation tests**

Add to `TestByteLabEngine.cpp`:

```cpp
CASE("byteLabInputAllowed rejects self and ByteLab sources") {
    Modulator mods[CONFIG_MODULATOR_COUNT];
    for (int i = 0; i < CONFIG_MODULATOR_COUNT; ++i) mods[i].clear();
    mods[3].setShape(Modulator::Shape::ByteLab);

    expectFalse(ModulatorEngine::byteLabInputAllowed(Routing::Source::Mod3, 2, mods), "self rejected");
    expectFalse(ModulatorEngine::byteLabInputAllowed(Routing::Source::Mod4, 2, mods), "ByteLab source rejected");
    expectTrue(ModulatorEngine::byteLabInputAllowed(Routing::Source::Mod1, 2, mods), "normal mod source allowed");
    expectFalse(ModulatorEngine::byteLabInputAllowed(Routing::Source::CvIn1, 2, mods), "CV inputs deferred");
}
```

- [ ] **Step 2: Run test and verify RED**

Run:

```bash
cd src/tests/unit
make TestByteLabEngine
```

Expected: compile failure for missing `byteLabInputAllowed`.

- [ ] **Step 3: Add source validation helper**

Add static helper to `ModulatorEngine`:

```cpp
static bool byteLabInputAllowed(Routing::Source source, int selfIndex, const Modulator *mods) {
    if (!Routing::isModulatorSource(source)) return false;
    int sourceIndex = Routing::modulatorSourceIndex(source);
    if (sourceIndex < 0 || sourceIndex >= CONFIG_MODULATOR_COUNT) return false;
    if (sourceIndex == selfIndex) return false;
    if (mods[sourceIndex].shape() == Modulator::Shape::ByteLab) return false;
    return true;
}
```

- [ ] **Step 4: Refactor Engine.cpp modulator loop to two passes**

In `src/apps/sequencer/engine/Engine.cpp`, change the modulator update loop so:

```cpp
for each modulator:
    if shape != ByteLab:
        existing normal tick path

for each modulator:
    if shape == ByteLab:
        resolve cfg.inputSource()
        if allowed: source01 = currentValue(sourceIndex) / 127.f
        else: source01 = 0.5f
        _modulatorEngine.tickByteLab(index, modulator, source01)
        _midiOutputEngine.sendModulator(index, _modulatorEngine.currentValue(index))
```

Keep Geode's current bank-wide branch intact. If Geode is active, preserve existing behavior before ByteLab work; do not mix Geode voice assignment with ByteLab slots in this phase.

- [ ] **Step 5: Run test and verify GREEN**

Run:

```bash
cd src/tests/unit
make TestByteLabEngine
```

Expected: PASS.

### Task 7: JustF Interaction Rules

**Files:**
- Modify: `src/apps/sequencer/engine/Engine.cpp`
- Modify: `src/apps/sequencer/ui/pages/ModulatorPage.cpp`
- Test: extend `src/tests/unit/sequencer/TestByteLabEngine.cpp` or `TestModulator.cpp`

- [ ] **Step 1: Write failing helper tests**

Add:

```cpp
CASE("ByteLab is not JustF eligible") {
    Modulator m;
    m.clear();
    expectTrue(ModulatorEngine::justfEligible(m), "normal shape eligible");
    m.setShape(Modulator::Shape::ByteLab);
    expectFalse(ModulatorEngine::justfEligible(m), "ByteLab ignored by JustF");
}
```

- [ ] **Step 2: Run test and verify RED**

Run:

```bash
cd src/tests/unit
make TestByteLabEngine
```

Expected: compile failure for missing `justfEligible`.

- [ ] **Step 3: Add eligibility helper and use it**

Add to `ModulatorEngine`:

```cpp
static bool justfEligible(const Modulator &m) {
    return m.shape() != Modulator::Shape::ByteLab &&
           m.rateDomain() == Modulator::RateDomain::Free;
}
```

In `Engine.cpp` JustF rate override path:

```cpp
if (_modulatorEngine.justfActive() && ModulatorEngine::justfEligible(modulator)) {
    // existing derived-rate path
}
```

In `ModulatorPage.cpp` Shift+RATE handling:

```cpp
if (modulator.shape() == Modulator::Shape::ByteLab) {
    event.consume();
    return;
}
if (_project.modulator(0).shape() == Modulator::Shape::ByteLab) {
    event.consume();
    return;
}
```

In `bakeJustf()`, skip ByteLab slots:

```cpp
if (_project.modulator(i).shape() == Modulator::Shape::ByteLab) continue;
```

- [ ] **Step 4: Run tests and verify GREEN**

Run:

```bash
cd src/tests/unit
make TestByteLabEngine TestModulator
```

Expected: PASS.

### Task 8: ModulatorPage ByteLab UI

**Files:**
- Modify: `src/apps/sequencer/ui/pages/ModulatorPage.cpp`
- Modify: `ui-preview/pages_modulator.py`
- Modify: `ui-preview/generate.py`

- [ ] **Step 1: Render failing/current preview**

Run:

```bash
python3 ui-preview/generate.py --page modulator-bytelab-proposed -o ui-preview/modulator-bytelab/modulator-bytelab-proposed.png
```

Expected: render still shows the older labels or lacks page 1/page 2 and bit cursor semantics.

- [ ] **Step 2: Update preview first**

Update `ui-preview/pages_modulator.py` so ByteLab previews show:

Page 1 footer:

```python
labels = ["IN", "GAIN", "BIAS", "DIV", "BITS"]
values = ["M1", "1.40x", "+0.00", "/3", "B4"]
```

Page 2 footer:

```python
labels = ["OP", "FOLD", "MIX", "DEPTH", "OFFSET"]
values = ["XOR", "BYPASS", "100", "127", "+0"]
```

When `BITS` is focused, draw an 8-bit editor in the left pane:

```text
7 6 5 4 3 2 1 0
1 0 1 1 0 1 0 0
      ^
```

Add slugs in `generate.py`:

```python
modulator-bytelab-page1
modulator-bytelab-page2
modulator-bytelab-bits
```

- [ ] **Step 3: Render and inspect previews**

Run:

```bash
python3 ui-preview/generate.py --page modulator-bytelab-page1 -o ui-preview/modulator-bytelab/page1.png
python3 ui-preview/generate.py --page modulator-bytelab-page2 -o ui-preview/modulator-bytelab/page2.png
python3 ui-preview/generate.py --page modulator-bytelab-bits -o ui-preview/modulator-bytelab/bits.png
open ui-preview/modulator-bytelab/page1.png
open ui-preview/modulator-bytelab/page2.png
open ui-preview/modulator-bytelab/bits.png
```

Expected: no text overlaps; footer labels fit; bit cursor is readable.

- [ ] **Step 4: Implement firmware UI**

In `ModulatorPage.cpp`:

- If selected modulator shape is ByteLab:
  - total pages = 2.
  - page 0 labels `IN/GAIN/BIAS/DIV/BITS`.
  - page 1 labels `OP/FOLD/MIX/DEPTH/OFFSET`.
  - draw left pane as byte strip / bit editor when `BITS` selected.
  - do not show `SHAPE`.
- `Page+Encoder click` toggles current modulator to/from ByteLab via `toggleByteLabShape()`.
- Page button cycles page 0/1.
- Encoder click while `BITS` selected toggles selected bit.
- Encoder turn while `BITS` selected moves bit cursor.
- Shift+encoder while `BITS` selected rotates mask left/right.

- [ ] **Step 5: Manual simulator/UI verification**

Run simulator or UI preview per normal local workflow. Minimum non-sim verification:

```bash
python3 ui-preview/generate.py --page modulator-bytelab-page1 -o ui-preview/modulator-bytelab/page1.png
python3 ui-preview/generate.py --page modulator-bytelab-page2 -o ui-preview/modulator-bytelab/page2.png
python3 ui-preview/generate.py --page modulator-bytelab-bits -o ui-preview/modulator-bytelab/bits.png
```

Expected: preview images match firmware labels and bit editor layout.

### Task 9: Resource And Build Gates

**Files:**
- No source edits unless build failures require them.

- [ ] **Step 1: Run focused unit tests**

Run:

```bash
cd src/tests/unit
make TestByteLabConfig TestByteLabEngine TestModulator
```

Expected: PASS.

- [ ] **Step 2: Run STM32 release build**

Run:

```bash
cd build/stm32/release
make sequencer
```

Expected: PASS.

- [ ] **Step 3: Check RAM and size gates**

Run:

```bash
cd build/stm32/release
/opt/homebrew/bin/arm-none-eabi-size -A src/apps/sequencer/sequencer
```

Expected:

- `.data + .bss` does not cross a new model/engine container cliff without explanation.
- `.ccmram_bss` remains within the current margin.
- If RAM grows, identify exact symbols before accepting the feature.

## Self-Review

- Spec coverage:
  - Per-slot shape: Tasks 2, 6, 8.
  - Full control path: Tasks 1, 4, 5, 8.
  - Source restrictions: Task 6.
  - JustF interaction: Task 7.
  - Bit editor: Task 8.
  - UI preview gate: Task 8.
  - STM32/RAM gate: Task 9.
- Placeholder scan: no `TBD`, no unresolved `TODO`, no unnamed tests.
- Type consistency:
  - `ByteLabConfig`, `ByteLabState`, `Shape::ByteLab`, `tickByteLab`, and helper names are introduced before later tasks use them.

## Execution Options

1. Subagent-Driven: dispatch one fresh worker per task, review between tasks.
2. Inline Execution: execute this plan in this session with review checkpoints.
