# Teletype v2 Phase 1 Plan

Goal: define the native Teletype++ data model before interpreter work.

Phase 1 must answer where state lives. It should not implement op execution, parser lowering, bridge deletion, or old project migration.

## Step 1: Native Program Storage

Define the persistent program shape.

Own:

- dialect version;
- script command storage;
- script lengths;
- boot script/config flags;
- native pattern storage;
- saved output/routing intent where it belongs to the script program.

Rules:

- fixed-size storage only;
- no heap strings;
- no `std::string`;
- no dependency on `scene_state_t` as save/load truth;
- no old project migration ceremony during dev-stage work.

Deliverable:

- a compact native program struct;
- constants for script count, command count, pattern count, and packed command width;
- compile-time assertions for bounds that must never drift silently.

Verification:

- host or simulator compile if available;
- STM32 release size probe once the struct exists;
- record struct size in the Phase 1 notes or follow-up diff.

## Step 2: Native Runtime State

Define the volatile execution state.

Own:

- variables;
- stack;
- delay queue;
- metro state;
- random state;
- active script/context;
- execution flags;
- last error/status for unsupported ops and runtime faults.

Rules:

- runtime state is not persistent program storage;
- runtime state does not call `tele_*`;
- runtime state does not require `TeletypeBridge`;
- runtime state does not depend on `g_activeEngine`;
- keep timing state explicit enough for metro/delay tests later.

Deliverable:

- a compact native runtime struct;
- reset/init helpers only where they prevent repeated hand-written clearing;
- size probes for runtime storage.

Verification:

- reset test or probe showing deterministic initial state;
- size measurement against current `TeletypeTrackEngine` and model/container gates.

## Step 3: Native Output State

Define the direct Performer-facing output state.

Own:

- 8 CV output values;
- CV slew/off/calibration state if needed for existing `CV.*` behavior;
- 8 trigger output levels;
- trigger pulse timing;
- trigger polarity/time settings;
- dirty flags or event records for applying output changes to Performer.

Rules:

- `CV 1 5000` must have an obvious direct target in this state;
- `CV 5 5000` and `TR.P 8` must fit the same model;
- `.ALL` forms must not require special storage architecture;
- output state must not emulate Monome hardware slots;
- output state must not route through bridge callbacks.

Deliverable:

- a compact native output state struct;
- named output count constants for 8 CV and 8 trigger outputs;
- clear reset defaults for CV, slew, trigger level, trigger time, and polarity.

Verification:

- size probe after adding output state;
- simple state-level tests for reset/defaults and output index bounds;
- no interpreter execution yet.

## Phase 1 Exit Gate

Phase 1 is complete when:

- native program, runtime, and output state exist;
- no heap-backed runtime/script storage was added;
- `scene_state_t` is no longer the planned data model truth for v2;
- size probes have current numbers;
- `.data + .bss` and `.ccmram_bss` impact is known from STM32 release build;
- Phase 2 can lower parsed commands into this state without inventing new ownership.
