# Comparison: codex-tele.md vs claude-tele.md

## Document Characteristics

| Aspect | codex-tele.md | claude-tele.md |
|--------|---------------|----------------|
| **Length** | 204 lines, concise | 863 lines, comprehensive |
| **Style** | High-level architecture | Detailed implementation guide |
| **Code Examples** | Minimal sketches | Full class definitions |
| **Depth** | Strategic decisions | Tactical implementation |

---

## 1. Architecture & Core Integration

### codex-tele.md Approach
- **Wrapper-centric**: Create `tele_runtime.h/.cpp` wrapper that owns `scene_state_t`
- **Bridge pattern**: `tele_io_bridge.cpp` implements `tele_*` functions
- **Location**: `src/apps/sequencer/teletype/` subdirectory
- **Philosophy**: Keep Teletype as a semi-independent subsystem with clean boundary

```cpp
// Conceptual structure
TeletypeTrackEngine
  └─> TeleRuntime (C++ wrapper)
       └─> scene_state_t (Teletype state)
       └─> tele_io_bridge (implements tele_cv(), tele_tr(), etc.)
```

### claude-tele.md Approach
- **Adapter pattern**: `TeletypeHardwareAdapter` as static singleton
- **Direct integration**: Engine owns `scene_state_t` directly
- **Location**: Spread across engine/, model/, teletype/src/
- **Philosophy**: Tighter integration, Teletype is "just another track engine"

```cpp
// Conceptual structure
TeletypeTrackEngine (owns scene_state_t directly)
  ├─> scene_state_t _sceneState
  ├─> exec_state_t _execState
  └─> TeletypeHardwareAdapter (static, injected reference)
```

**Key Difference**:
- **codex**: Encapsulates Teletype in a Runtime wrapper (more modular, easier to test)
- **claude**: Flatter integration (less abstraction layers, simpler data flow)

**Winner**: **codex** - The runtime wrapper is cleaner architecturally and provides better isolation for testing.

---

## 2. I/O Mapping Strategy

### codex-tele.md Approach

**More Flexible Input Routing:**
```cpp
struct TeleInputMap {
    Routing::Source source;  // ANY routing source
    float threshold;
    bool rising;
    bool falling;
};
```

- **TR inputs 1-8**: Route from ANY Performer source (CV in, CV out, Gate, MIDI CC)
- **IN/PARAM**: Map to any route source with scaling
- **Explicit threshold/debounce**: Configurable per input
- **Philosophy**: Full flexibility - user configures what triggers what

**Advantages:**
- Can route anything to TR inputs (e.g., "TR 1 triggers when Track 3 CV out > 2V")
- More powerful for complex patches
- Aligns with Performer's routing paradigm

**Disadvantages:**
- More complex UI for configuration
- User must understand routing system

### claude-tele.md Approach

**Predefined Source Enum:**
```cpp
enum InputSource {
    PerformerClock,
    GateIn1, GateIn2, GateIn3, GateIn4,
    StepButton,
    FunctionButton,
    TrackMuteToggle,
    // Fixed list
};
```

- **TR inputs 1-8**: Select from predefined list of event sources
- **IN/PARAM**: Select from predefined CV sources (CV In 1-4, Encoder, Tempo, etc.)
- **Philosophy**: Simplified, common-case focused

**Advantages:**
- Simpler UI (dropdown menu)
- Easier to understand for users
- Covers 90% of use cases

**Disadvantages:**
- Less flexible (can't route arbitrary sources)
- Requires code changes to add new source types

**Winner**: **codex** - Routing flexibility is a core Performer strength; leveraging it fully makes Teletype more powerful.

---

## 3. CV Input Mapping

### codex-tele.md
- **IN/PARAM**: Any routing source
- **Scaling**: Uses routing min/max → 0-16383
- **Implicit**: Relies on existing routing infrastructure

### claude-tele.md
- **IN/PARAM**: Predefined enum with creative options:
  - `ProjectScale` - current scale as CV
  - `ProjectTempo` - BPM mapped to CV
  - `TrackProgress` - sequence position 0-100%
  - `StepEncoder` - encoder value as CV
- **Explicit adapter code** for each source type

**Key Difference**:
- **codex**: Generic routing (simpler code, requires user config)
- **claude**: Specialized sources (more code, but interesting presets)

**Winner**: **Hybrid** - codex's generic routing PLUS claude's creative presets (ProjectTempo, TrackProgress) as optional convenience mappings.

---

## 4. Clock & Timing Integration

### codex-tele.md Approach

**Musical Alignment:**
```cpp
// Convert Performer ticks to ms using Clock::tickDuration()
tele_time_ms += Clock::tickDuration();
tele_tick(ss, delta_ms);
```

- **Philosophy**: Align `tele_tick()` with musical time
- **WAIT/DEL**: Now tempo-dependent (musical delays)
- **Metronome**: Driven by Performer ticks

**Implication**: WAIT 500 means "500ms at current tempo" - delays stretch/compress with tempo changes.

### claude-tele.md Approach

**Fixed Time Tick:**
```cpp
// Fixed 10ms tick (Teletype native)
accumulator += dt;
while (accumulator >= 0.010f) {
    tele_tick(&_sceneState, 10);
    accumulator -= 0.010f;
}
```

- **Philosophy**: Keep Teletype's original 10ms tick
- **WAIT/DEL**: Always real-time milliseconds (tempo-independent)
- **Metronome**: Separate float accumulator, independent of ticks

**Implication**: WAIT 500 always means "500ms wallclock time" regardless of tempo.

**Key Difference**:
- **codex**: Musical time (tempo-aware delays)
- **claude**: Wallclock time (original Teletype behavior)

**Winner**: **Controversial** - Depends on use case:
- **Teletype purists**: Want wallclock (claude)
- **Musical integration**: Want tempo-sync (codex)
- **Best solution**: Make it configurable per track!

---

## 5. Scale Integration

### codex-tele.md

**Explicit Integration Plan:**
- Override quantization ops (`QT`, `SCALE`, `N.BX`) to use Performer's `Project::selectedScale()`
- Provide shim for pitch-to-volts conversion
- Preserve Teletype bitmask ops as optional
- **Open question**: Keep TT internal scales OR fully replace?

**Philosophy**: Leverage Performer's project-wide scale system for consistency.

### claude-tele.md

**Mentioned but not detailed:**
- Brief mention of "using project scales"
- No concrete implementation strategy
- Assumes Teletype scales stay independent

**Winner**: **codex** - Explicitly addresses scale integration as a design decision, provides migration path.

---

## 6. UI Design Philosophy

### codex-tele.md

**Page-Centric:**
- **Tele Live**: REPL (command entry + output)
- **Tele Edit**: Script editor (1-8, M, I)
- **Tele Patterns**: Pattern tracker
- **Tele Vars**: Variable monitor
- **Tele IO Map**: Input/output routing config

**Text Entry:**
- "Opcode palette + numeric entry"
- "Token insertion via context menu (ops list grouped by category)"
- Less specific about HOW

**Philosophy**: Multiple focused pages, each with specific purpose.

### claude-tele.md

**Mode-Centric within Pages:**
- Page 1: Script Editor with modes (EDIT, VARS, PATT, CFG)
- Context menu on same page
- Encoder selects action

**Text Entry - THREE METHODS:**
1. **Op Menu System**: Function → Categories → Ops
2. **T9-style**: Step buttons like phone keypad
3. **Encoder Scroll**: Scroll through alphabet + ops

**Philosophy**: Fewer pages, modal interaction, multiple entry methods.

**Key Difference**:
- **codex**: Separate pages (more navigation)
- **claude**: Modal UI (less navigation, more context switching)

**Text Entry Winner**: **claude** - Provides THREE concrete methods vs codex's vague "opcode palette". T9 method is particularly clever.

**Page Layout Winner**: **codex** - Dedicated IO Map page is clearer than burying it in a modal submenu.

---

## 7. Op Support Matrix

### codex-tele.md

**Clear Matrix:**
- **Keep**: variables, maths, controlflow, patterns, queue, stack, seed, delay, metronome, midi, turtle, init
- **Wrapper**: hardware.c (CV/TR/IN/PARAM)
- **Omit**: i2c, ansible, crow, justfriends, grid_ops, etc.

**Simplest implementation**: Keep op table intact, stub I2C functions to no-op (lower risk, larger binary).

### claude-tele.md

**Same ops, more detail:**
- Lists each file individually
- Provides rationale for each category
- Suggests error handling for unsupported ops

**Difference**: codex offers "stub to no-op" option; claude suggests removing from op table entirely.

**Winner**: **Tie** - Both cover same ground. codex's "stub" approach is pragmatic for MVP.

---

## 8. Memory & Performance

### codex-tele.md

**Risks section:**
- "Interpreter runs on main engine thread: avoid long loops"
- "`WHILE_DEPTH` protection"
- "`scene_state_t` is large; remove grid fields to reduce footprint"
- **No concrete numbers**

### claude-tele.md

**Performance section with numbers:**
- `scene_state_t`: ~10-15 KB per track
- Script execution: ~100-500 μs
- Target: `update()` < 1ms on STM32
- Teletype ops library: ~50-80 KB code size
- Profiling targets specified

**Winner**: **claude** - Provides concrete memory/timing budgets for validation.

---

## 9. Serialization Strategy

### codex-tele.md

**Two options:**
1. Store scripts as TEXT (easier to edit)
2. Store tokenized `tele_command_t` (faster load)
- Leaves decision open

### claude-tele.md

**TEXT format chosen:**
```cpp
char _scriptText[11][6][64];  // Text storage
// Parse on load, serialize on save
```
- Explicit text storage in model
- Pattern data stored as int16_t arrays

**Winner**: **codex** - Acknowledges trade-off explicitly. TEXT is better for version control/debugging, binary for performance.

---

## 10. Implementation Phases

### codex-tele.md

**6 phases (high-level):**
1. Core VM
2. I/O bridge
3. Track engine
4. UI
5. Scale integration
6. Polish

**Brief, strategic.**

### claude-tele.md

**6 phases (detailed milestones):**
1. Core Integration (2-3 weeks) - "Can run hardcoded script"
2. I/O Integration (1 week) - "Produces CV/Gate"
3. Input Routing (1 week) - "Scripts trigger on events"
4. Basic UI (2 weeks) - "Can edit scripts"
5. Advanced UI (3 weeks) - "Full editor"
6. Polish (1-2 weeks) - "Production ready"

**Total**: 6-9 weeks estimated.

**Winner**: **claude** - Provides concrete milestones and time estimates for project planning.

---

## 11. Missing from codex, Present in claude

1. **Complete class definitions** - Full `TeletypeTrack`, `TeletypeTrackEngine` headers
2. **Hardware adapter pattern** - Singleton adapter with injection
3. **Concrete UI mockups** - ASCII art screen layouts
4. **Edge detection logic** - Rising/falling edge handling code
5. **CV slewing implementation** - Float accumulator with rate calculation
6. **Metro timer** - Float-based accumulator for metro
7. **Unit test examples** - Actual test cases with assertions
8. **CMakeLists.txt changes** - Build system integration
9. **Visual architecture diagram** - Box-and-arrow flow
10. **Known limitations list** - Explicit constraints

---

## 12. Missing from claude, Present in codex

1. **Routing source flexibility** - ANY source to TR inputs, not enum
2. **Scale integration strategy** - Explicit replacement of QT/SCALE ops
3. **Threshold/debounce config** - Per-input edge detection parameters
4. **Word-step editing** - Shift+Left/Right for token navigation
5. **Multiple implementation options** - Stub vs remove for I2C ops
6. **Open decisions section** - Explicit list of unresolved questions
7. **Risks/constraints section** - Thread safety, loop limits

---

## 13. Synthesis - Best Ideas from Each

### Architecture (from codex)
✅ **Use Runtime wrapper pattern** - Better encapsulation than claude's direct approach

### I/O Routing (from codex)
✅ **Full routing flexibility** - Use `Routing::Source` for TR inputs, not fixed enum

### Timing (from codex)
✅ **Musical timing option** - Make tempo-sync vs wallclock configurable

### UI Text Entry (from claude)
✅ **T9 method** - Brilliant use of step buttons
✅ **Three entry methods** - Op menu, T9, encoder scroll

### CV Input Sources (from claude)
✅ **Creative presets** - ProjectTempo, TrackProgress as convenience sources

### Implementation Detail (from claude)
✅ **Concrete code examples** - Full class definitions
✅ **Performance targets** - Specific μs/KB budgets
✅ **Time estimates** - 6-9 week roadmap

### Scale Integration (from codex)
✅ **Explicit QT/SCALE override** - Shim to Project::selectedScale()

### Configuration UI (from codex)
✅ **Dedicated IO Map page** - Clearer than modal submenu

---

## 14. Recommended Hybrid Approach

**Architecture:**
```cpp
TeletypeTrackEngine
  └─> TeletypeRuntime (wrapper, from codex)
       ├─> scene_state_t
       ├─> TeleHardwareAdapter (static, from claude)
       └─> I/O methods
```

**I/O Mapping:**
- **TR Inputs**: Use `Routing::Source` (codex) with threshold/edge config
- **CV Inputs**: Routing sources (codex) PLUS convenience presets (claude): ProjectTempo, TrackProgress

**Timing:**
- **Configurable**: Per-track setting "Musical Time" vs "Wallclock Time"
  - Musical: WAIT syncs to tempo (codex)
  - Wallclock: Original TT behavior (claude)

**UI:**
- **Pages** (codex): Live, Edit, Patterns, Vars, **IO Map**
- **Text Entry** (claude): T9 + Op Menu + Encoder Scroll
- **Edit Controls** (codex): Word-step navigation (Shift+Left/Right)

**Scale Integration:**
- **Override** (codex): QT/SCALE use Project::selectedScale()
- **Optional**: Preserve TT bitmask scales for scripts that need them

**Implementation:**
- **Code detail** (claude): Use full class definitions as templates
- **Phasing** (claude): 6-phase plan with milestones
- **Build system** (claude): CMakeLists integration
- **Open decisions** (codex): Acknowledge trade-offs explicitly

---

## 15. Critical Differences Summary

| Aspect | codex-tele.md | claude-tele.md | Recommended |
|--------|---------------|----------------|-------------|
| **Wrapper Pattern** | Runtime wrapper | Direct integration | Runtime wrapper |
| **I/O Flexibility** | Full routing | Fixed enum | Full routing |
| **Timing Model** | Musical (tempo-sync) | Wallclock (fixed) | Configurable both |
| **UI Layout** | Separate pages | Modal within page | Separate pages |
| **Text Entry** | Vague "palette" | 3 concrete methods | T9 + Op Menu |
| **Scale Integration** | Explicit shim | Not detailed | QT/SCALE override |
| **Code Detail** | Minimal sketches | Full classes | Full classes |
| **Time Estimates** | None | 6-9 weeks | Use claude's |
| **CV Input Sources** | Generic routing | Creative presets | Routing + presets |
| **IO Config UI** | Dedicated page | Modal submenu | Dedicated page |

---

## 16. Final Verdict

**codex-tele.md strengths:**
- Better architectural decisions (wrapper, routing flexibility)
- Explicit handling of open questions and trade-offs
- Scale integration strategy
- Dedicated IO Map page

**claude-tele.md strengths:**
- Far more implementation detail and working code
- Three concrete text entry methods (T9 is genius)
- Performance budgets and time estimates
- Creative CV input presets
- Complete project planning

**Best approach:**
Use **codex architecture** (wrapper, flexible routing, scale integration) with **claude implementation detail** (code templates, T9 UI, phasing plan, performance targets).

The hybrid combines strategic clarity from codex with tactical depth from claude.
