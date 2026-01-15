
 1. Remove Working Member Duplication

  Current: I/O mappings stored twice
  // Working members (TeletypeTrack)
  _triggerInputSource[4]
  _cvInSource, _cvParamSource, _cvXSource, _cvYSource, _cvZSource
  _triggerOutputDest[4]
  _cvOutputDest[4]
  _cvOutputRange[4], _cvOutputOffset[4], _cvOutputQuantizeScale[4], _cvOutputRootNote[4]
  _midiSource
  _bootScriptIndex, _timeBase, _clockDivisor, _clockMultiplier, _resetMetroOnLoad

  // Duplicated in PatternSlot
  patternSlot.triggerInputSource[4]
  patternSlot.cvInSource, ...
  // ... all the same fields

  Target: Active slot IS the working storage
  // Getters return active slot directly
  auto& triggerInputSource() { return _patternSlots[_activePatternSlot].triggerInputSource; }
  const auto& triggerInputSource() const { return _patternSlots[_activePatternSlot].triggerInputSource; }

  Changes:
  ┌───────────────────┬───────────────────────────────────────────────────────────────────────────────────────────────┐
  │       File        │                                            Action                                             │
  ├───────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────┤
  │ TeletypeTrack.h   │ Remove 15+ working members, add inline getters                                                │
  ├───────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────┤
  │ TeletypeTrack.cpp │ Remove from clear(), simplify applyPatternSlot(), remove mapping sync from syncToActiveSlot() │
  ├───────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────┤
  │ UI pages          │ May need updates if accessing members directly                                                │
  └───────────────────┴───────────────────────────────────────────────────────────────────────────────────────────────┘
  Benefits:
  - ~100 bytes saved per track
  - Eliminates syncActiveSlotMappings() logic entirely
  - Single source of truth

  Risk: Breaking change if UI accesses members directly

  ---
  2. Dirty Flag Sync

  Current: Sync called eagerly at every potential change point
  void setTriggerInputSource(...) {
      _triggerInputSource[index] = source;
      syncToActiveSlot();  // Called every single time
  }

  Target: Mark dirty, sync lazily before save/switch
  void setTriggerInputSource(...) {
      _patternSlots[_activePatternSlot].triggerInputSource[index] = source;
      // No sync needed - writing directly to slot
  }

  // Only sync VM state (scripts/patterns) when needed
  void ensureVMSynced() {
      if (_vmDirty) {
          syncVMToActiveSlot();  // Only scripts + patterns
          _vmDirty = false;
      }
  }

  When to call ensureVMSynced():
  - Before write()
  - Before onPatternChanged() (slot switch)
  - Before patternSlotSnapshot()

  Benefits:
  - Fewer memcpy operations
  - Cleaner setter code

  ---
  3. Implementation Order

  Option A: Do #1 first (removes duplication)
    └── This automatically simplifies #2 since mappings don't need sync
    └── Higher risk (touches more code)

  Option B: Do #2 first (dirty flag)
    └── Lower risk, smaller change
    └── Still have duplication but less sync overhead

  Recommendation: Option A - removing duplication makes dirty flag trivial (only VM state needs sync)

  ---
  Effort Estimate
  ┌────────────────────────┬────────────────┬───────────────────┐
  │         Change         │ Lines Modified │       Risk        │
  ├────────────────────────┼────────────────┼───────────────────┤
  │ Remove working members │ ~200           │ Medium (check UI) │
  ├────────────────────────┼────────────────┼───────────────────┤
  │ Dirty flag             │ ~30            │ Low               │
  └────────────────────────┴────────────────┴───────────────────┘



i
 TeletypeTrack Integration: Optimization & Refactoring Ideas

  1. Memory Optimization

  A. Eliminate redundant pattern storage

  Current: 3 copies of patterns
  - _state.patterns[4] - VM runtime
  - _patterns[4] - master copy (redundant after our fix)
  - _patternSlots[2].patterns[4] - per-slot

  Proposal: Remove _patterns[], use _state.patterns[] as single source of truth
  // Before sync
  patternSlot.patterns[i] = _state.patterns[i];

  // Getter returns VM state directly
  const scene_pattern_t& pattern(int i) const { return _state.patterns[i]; }

  Savings: ~552 bytes (4 × 138 bytes per scene_pattern_t)

  B. Lazy slot allocation

  Current: Both slots always allocated, even if only using slot 0

  Proposal: Only allocate slot 1 when first accessed
  std::unique_ptr<PatternSlot> _patternSlots[2];  // nullptr until needed

  C. Share scripts between slots

  Current: Global scripts S1-S3 duplicated conceptually (stored in _state, synced to slots)

  Proposal: Only store slot-specific scripts (S4, Metro) in slots, keep S1-S3 separate
  struct PatternSlot {
      // Remove: scripts S1-S3 are global, not per-slot
      std::array<tele_command_t, ScriptLineCount> slotScript;  // S4 only
      std::array<tele_command_t, ScriptLineCount> metro;       // Metro only
      std::array<scene_pattern_t, PATTERN_COUNT> patterns;
      // I/O mappings...
  };

  ---
  2. Code Simplification

  A. Unified sync function

  Current: 3 separate sync functions called together everywhere
  syncActiveSlotScripts();
  syncActiveSlotPatterns();
  syncActiveSlotMappings();

  Proposal: Single function
  void syncToActiveSlot() {
      auto &slot = _patternSlots[_activePatternSlot];
      // Scripts
      slot.slotScriptLength = _state.scripts[SlotScriptIndex].l;
      slot.metroLength = _state.scripts[METRO_SCRIPT].l;
      std::memcpy(slot.slotScript.data(), _state.scripts[SlotScriptIndex].c, ...);
      std::memcpy(slot.metro.data(), _state.scripts[METRO_SCRIPT].c, ...);
      // Patterns
      for (int i = 0; i < PATTERN_COUNT; ++i) {
          slot.patterns[i] = _state.patterns[i];
      }
      // Mappings
      slot.triggerInputSource = _triggerInputSource;
      // ...
  }

  B. Remove working member duplication

  Current: I/O mappings stored in both working members AND active slot
  _triggerInputSource[4]      // Working member
  _patternSlots[active].triggerInputSource[4]  // Slot copy

  Proposal: Use slot directly as working storage
  // Getter
  auto& triggerInputSource() { return _patternSlots[_activePatternSlot].triggerInputSource; }

  Eliminates syncActiveSlotMappings() entirely.

  ---
  3. Architecture Cleanup

  A. Separate VM wrapper from track model

  Current: TeletypeTrack mixes model concerns with VM state

  Proposal: Extract VM wrapper
  class TeletypeVM {
      scene_state_t _state;
  public:
      void init();
      void tick(uint32_t ms);
      scene_state_t& state() { return _state; }
      // Script/pattern accessors
  };

  class TeletypeTrack {
      TeletypeVM _vm;
      std::array<PatternSlot, 2> _slots;
      // I/O routing only
  };

  B. Event-based pattern sync

  Current: Manual sync calls scattered everywhere

  Proposal: Dirty flag + lazy sync
  class TeletypeTrack {
      bool _vmDirty = false;

      void markDirty() { _vmDirty = true; }

      void ensureSynced() {
          if (_vmDirty) {
              syncToActiveSlot();
              _vmDirty = false;
          }
      }
  };

  Engine calls ensureSynced() before save or slot switch.

  C. Immutable slot snapshots

  Current: PatternSlot is mutable, passed by reference

  Proposal: Copy-on-write semantics
  PatternSlot snapshotActiveSlot() const;  // Returns copy
  void applySlot(const PatternSlot& slot); // Applies copy

  ---
  4. Performance

  A. Reduce memcpy in slot switch

  Current: Full slot copy on every pattern change
  std::memcpy(_state.scripts[SlotScriptIndex].c, patternSlot.slotScript.data(), ...);

  Proposal: Pointer swap if slots are heap-allocated
  _activeSlotPtr = &_patternSlots[newSlot];
  // VM uses pointer directly

  B. Incremental pattern sync

  Current: All 4 patterns synced even if only 1 changed

  Proposal: Track dirty patterns
  uint8_t _dirtyPatternMask = 0;

  void syncDirtyPatterns() {
      for (int i = 0; i < 4; ++i) {
          if (_dirtyPatternMask & (1 << i)) {
              slot.patterns[i] = _state.patterns[i];
          }
      }
      _dirtyPatternMask = 0;
  }

  C. Batch script parsing

  Current: Scripts parsed one line at a time on load

  Proposal: Bulk parse with error collection
  struct ParseResult {
      std::vector<int> errorLines;
      bool success;
  };
  ParseResult parseScript(const char* text, scene_script_t& out);

  ---
  5. Feature Enhancements

  A. More than 2 pattern slots

  Current: Hard-coded PatternSlotCount = 2

  Proposal: Configurable, e.g., 4 slots for 4 distinct setups per track

  B. Script presets/library

  Current: Scripts only in project or .tt files

  Proposal: Built-in script library
  enum class ScriptPreset { Euclidean, Arpeggio, RandomWalk, ... };
  void loadPreset(ScriptPreset preset, int scriptIndex);

  C. Cross-track TT communication

  Current: Each TT track is isolated

  Proposal: Shared variable bank across TT tracks
  // In Engine
  int16_t _sharedTTVars[16];

  // TT op: SHARED.GET n, SHARED.SET n v

  D. CV/Gate routing matrix UI

  Current: Individual source/dest selection

  Proposal: Matrix view showing all routings at once

  ---
  Priority Ranking
  ┌───────────────────────────────────┬────────┬────────┬─────────────────┐
  │              Change               │ Effort │ Impact │    Priority     │
  ├───────────────────────────────────┼────────┼────────┼─────────────────┤
  │ Unified sync function             │ Low    │ Medium │ High            │
  ├───────────────────────────────────┼────────┼────────┼─────────────────┤
  │ Remove _patterns[] redundancy     │ Low    │ Low    │ High            │
  ├───────────────────────────────────┼────────┼────────┼─────────────────┤
  │ Remove working member duplication │ Medium │ Medium │ Medium          │
  ├───────────────────────────────────┼────────┼────────┼─────────────────┤
  │ Dirty flag sync                   │ Medium │ Medium │ Medium          │
  ├───────────────────────────────────┼────────┼────────┼─────────────────┤
  │ Separate VM wrapper               │ High   │ High   │ Low (breaking)  │
  ├───────────────────────────────────┼────────┼────────┼─────────────────┤
  │ More pattern slots                │ Medium │ High   │ Feature request │


 Working Members to Remove (lines 672-692)

  // These are ALL duplicated in PatternSlot:
  MidiSourceConfig _midiSource;
  std::array<TriggerInputSource, TriggerInputCount> _triggerInputSource;
  CvInputSource _cvInSource, _cvParamSource, _cvXSource, _cvYSource, _cvZSource;
  std::array<TriggerOutputDest, TriggerOutputCount> _triggerOutputDest;
  std::array<CvOutputDest, CvOutputCount> _cvOutputDest;
  int8_t _bootScriptIndex;
  TimeBase _timeBase;
  uint16_t _clockDivisor;
  int16_t _clockMultiplier;
  std::array<Types::VoltageRange, CvOutputCount> _cvOutputRange;
  std::array<int16_t, CvOutputCount> _cvOutputOffset;
  std::array<int8_t, CvOutputCount> _cvOutputQuantizeScale;
  std::array<int8_t, CvOutputCount> _cvOutputRootNote;

  Members to KEEP

  int8_t _trackIndex;              // Track identity
  scene_state_t _state;            // VM state (scripts, patterns, variables)
  std::array<PatternSlot, 2> _patternSlots;
  uint8_t _activePatternSlot;
  bool _resetMetroOnLoad;          // Runtime flag (consumed once)
  bool _bootScriptRequested;       // Runtime flag (consumed once)

  Accessor Pattern Change

  Before:
  CvInputSource cvInSource() const { return _cvInSource; }
  void setCvInSource(CvInputSource source) {
      _cvInSource = ModelUtils::clampedEnum(source);
      syncToActiveSlot();  // Sync to slot
  }

  After:
  CvInputSource cvInSource() const {
      return _patternSlots[_activePatternSlot].cvInSource;
  }
  void setCvInSource(CvInputSource source) {
      _patternSlots[_activePatternSlot].cvInSource = ModelUtils::clampedEnum(source);
      // No sync needed - writing directly to slot!
  }

  Estimated Changes
  ┌───────────────────────────────────┬───────────────┐
  │               Area                │ Lines Changed │
  ├───────────────────────────────────┼───────────────┤
  │ Remove 15 members                 │ -20           │
  ├───────────────────────────────────┼───────────────┤
  │ Update ~25 getters                │ ~50           │
  ├───────────────────────────────────┼───────────────┤
  │ Update ~25 setters (remove sync)  │ ~25           │
  ├───────────────────────────────────┼───────────────┤
  │ Simplify clear()                  │ ~20           │
  ├───────────────────────────────────┼───────────────┤
  │ Simplify applyPatternSlot()       │ -30           │
  ├───────────────────────────────────┼───────────────┤
  │ Simplify syncToActiveSlot()       │ -20           │
  ├───────────────────────────────────┼───────────────┤
  │ Update conflict detection methods │ ~20           │
  └───────────────────────────────────┴───────────────┘
  Net: ~150 lines removed

  ---
  This is a 30-45 minute refactor with testing. Want me to proceed, or save it for another session?
The medium priority refactor "Remove Working Member Duplication" is complete. Here's a summary of changes:

  Summary of Refactoring

  Removed ~17 redundant working members from TeletypeTrack:
  - _midiSource, _triggerInputSource[], _cvInSource, _cvParamSource, _cvXSource, _cvYSource, _cvZSource
  - _triggerOutputDest[], _cvOutputDest[], _bootScriptIndex
  - _timeBase, _clockDivisor, _clockMultiplier
  - _cvOutputRange[], _cvOutputOffset[], _cvOutputQuantizeScale[], _cvOutputRootNote[]

  Changed Architecture:
  - Before: Working members ↔ Pattern slot (synced via syncToActiveSlot() / applyPatternSlot())
  - After: All accessors read/write directly via activeSlot() helper

  Key Changes:
  1. Added activeSlot() helper method for direct slot access
  2. Updated ~30 getters/setters to use activeSlot() directly (no sync needed)
  3. Simplified clear() to initialize slots directly
  4. Simplified applyPatternSlot() - now only syncs VM state (scripts/patterns)
  5. Simplified syncToActiveSlot() - now only syncs VM state
  6. Updated write()/read() to use active slot for I/O mapping
  7. Updated conflict detection methods to use active slot

  Benefits:
  - Reduced memory footprint by ~200 bytes per TeletypeTrack
  - Eliminated sync bugs - no more forgetting to call syncToActiveSlot()
  - Simpler code - single source of truth for I/O mappings
  - Maintained backwards compatibility for file format
  └───────────────────────────────────┴────────┴────────┴─────────────────┘
