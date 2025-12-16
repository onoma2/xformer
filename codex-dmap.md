# DiscreteMap changes (session summary)

## Current architecture
- **Model (`src/apps/sequencer/model/DiscreteMapSequence.{h,cpp}`)**: Per-track sequence with 8 stages. Each stage stores `direction` (Rise/Fall/Off), `threshold` int (clamped to [-127,127]), and `noteIndex` ([-63,64]). Sequence globals: `clockSource` (Internal ramp or External routed input), `divisor`, `loop`, `thresholdMode` (Position uses raw thresholds; Length distributes range by absolute threshold weights), `scaleSource` (Project/Track), `scale`, `rootNote`, `slewEnabled`, plus track index for routing. Persistence clamps thresholds via the setter.
- **Engine (`src/apps/sequencer/engine/DiscreteMapTrackEngine.{h,cpp}`)**: Drives input and outputs. Internal clock uses a ±5 V ramp based on `divisor`; External input TODO (stubbed). Thresholds: Position maps [-127,127] linearly to range; Length caches proportional breakpoints from abs(threshold). Stage activation scans in order for first threshold crossing matching direction vs previous/current input. Gate is on when a stage is active; CV output is stage note → volts via selected scale with root offset (chromatic only), optional simple slew toward target. Reports activity on stage change.
- **UI wiring**: TopPage routes Page+S2 to the stage list for DiscreteMap tracks and keeps Page+S3 on the Track page (`src/apps/sequencer/ui/pages/TopPage.cpp`, `Pages.h`, `CMakeLists.txt`).

## UI
- **Stage list (Page+S2)**: New two-column ListPage at `src/apps/sequencer/ui/pages/DiscreteMapStagesPage.{h,cpp}`. Three rows per stage:
  - `Dir`: cycles Rise/Fall/Off via encoder while in edit mode.
  - `Thresh`: threshold edit (±8, Shift=±1), stored as int8-safe.
  - `Note`: edits note index (±1, Shift=±12) and shows scale-aware names when project context is available.
  - Entering the page binds to the selected DiscreteMap sequence; non-DiscreteMap tracks clear the model.
- **Page wiring**: Page+S2 on a DiscreteMap track opens the stage list; Page+S3 remains the Track page so the standard track params stay accessible (`src/apps/sequencer/ui/pages/TopPage.cpp`, `Pages.h`).
- **Track page (Page+S3)**: Still uses `DiscreteMapSequenceListModel` for clock/divisor/loop/threshold mode/scale/root/slew (`src/apps/sequencer/ui/pages/TrackPage.cpp`, `src/apps/sequencer/ui/model/DiscreteMapSequenceListModel.h`), unchanged except wiring.
- **Sequence page**: Graphical stage view retained; only threshold normalization updated (`src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp`).

## Engine / model
- **Threshold safety**: Stage thresholds now clamp to `[-127, 127]` and accept `int` to avoid int8 overflow (`src/apps/sequencer/model/DiscreteMapSequence.h`); deserialization clamps through the setter (`src/apps/sequencer/model/DiscreteMapSequence.cpp`).
- **Normalization**: Position-mode threshold mapping updated to match new range in engine and UI (`src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp`, `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp`).
- **Tests**: Clamp expectation adjusted in `src/tests/unit/sequencer/TestDiscreteMapSequence.cpp`. (Simulator/hardware tests not run in this session.)

## Behavior recap
- Page+S2: edit per-stage Direction/Threshold/Note in a simple list.
- Page+S3: standard Track page for DiscreteMap (global params).
- Sequence cycling (Page+S1/Page key combinations): DiscreteMap sequence view remains the graphical page.

## Uncommitted file map (dmap-related)
- `src/apps/sequencer/ui/pages/DiscreteMapStagesPage.{h,cpp}`: New stage list page (Dir/Thresh/Note rows, encoder editing) and binding to the selected DiscreteMap sequence/project context.
- `src/apps/sequencer/ui/pages/TopPage.cpp` and `Pages.h`: Route Page+S2 to the new stage list for DiscreteMap tracks; keep Page+S3 on Track page; register the new page instance.
- `src/apps/sequencer/CMakeLists.txt`: Adds the new page to the sequencer UI build.
- `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp`: Uses updated threshold normalization to match clamped range.
- `src/apps/sequencer/model/DiscreteMapSequence.{h,cpp}`: Threshold setter now takes `int`, clamps to [-127,127], and deserialization clamps via the setter.
- `src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp`: Normalizes thresholds against the new range in position mode.
- `src/tests/unit/sequencer/TestDiscreteMapSequence.cpp`: Updates clamp expectations for the new threshold limits.
