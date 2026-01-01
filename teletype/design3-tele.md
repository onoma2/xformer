# design3-tele.md

## Key similarities
- All four plans treat Teletype as a new track mode with a model + engine wrapper around the mostly-unchanged Teletype VM/interpreter. (`codex-tele.md:39`, `qwen-tele.md:103`, `gemini-tele.md:24`)
- Core ops are kept while I2C/grid-dependent ops are excluded or stubbed. (`codex-tele.md:55`, `codex-tele.md:61`, `qwen-tele.md:175`)
- A hardware/IO bridge is required to map Teletype CV/TR/IN/PARAM to Performer routing and track outputs. (`codex-tele.md:71`, `codex-tele.md:103`, `qwen-tele.md:132`)
- Timing should be driven by Performer's clock/tick loop, with `tele_tick` adapted via an accumulated `dt` or tick-derived ms. (`codex-tele.md:151`, `codex-tele.md:155`, `gemini-tele.md:39`)
- Teletype should behave like native tracks: divisor, octave/transpose, CV update mode, and scale quantization integration are all expected. (`codex-tele.md:174`, `codex-tele.md:186`, `gemini-tele.md:28`, `claude-tele.md:274`)
- UI support is expected for script editing/monitoring and (optionally) external script libraries. (`codex-tele.md:204`, `gemini-tele.md:75`, `codex-tele.md:359`)

## Key differences and conflicts
- RAM feasibility varies wildly: Qwen estimates ~16-21 KB per track and tight headroom, while Claude claims ~6-9 KB per track if grid data is removed; Gemini claims ~9.2 KB but later states ~20 KB total per track. (`qwen-tele.md:16`, `qwen-tele.md:21`, `claude-tele.md:37`, `claude-tele.md:45`, `gemini-tele.md:15`, `gemini-tele.md:104`)
- Track container strategy is inconsistent: Codex warns a large Teletype state would inflate the container size for every track, while Gemini assumes Teletype can fit under the existing max and be "free." (`codex-tele.md:4`, `codex-tele.md:7`, `gemini-tele.md:6`, `gemini-tele.md:18`)
- Metro timing policy differs: Gemini pushes a divisor-driven metro, Codex suggests ms-based timing with an optional divisor mode, and Claude proposes explicit dual modes. (`gemini-tele.md:50`, `codex-tele.md:177`, `claude-tele.md:362`)
- Output expansion strategy diverges: Codex lists multiple approaches (mapping tables, TELEX emulation, banks, routing fan-out) while Claude proposes explicit CV/TR 1-20 cross-track routing and Qwen proposes per-output destination mapping. (`codex-tele.md:82`, `claude-tele.md:228`, `claude-tele.md:4341`, `qwen-tele.md:260`)
- Script library formats and locations conflict across docs (.tele, .TT, .tts, and Teletype-style sets in different directories). (`codex-tele.md:363`, `qwen-tele.md:1367`, `claude-tele.md:96`, `gemini-tele.md:67`)
- Script count/exposure is inconsistent: Codex focuses on 8+M+I, while Qwen/Claude include LIVE/DELAY scripts; Gemini's standalone save format only mentions 1-8/M/I. (`codex-tele.md:21`, `qwen-tele.md:183`, `claude-tele.md:183`, `gemini-tele.md:71`)

## Decisions to make (questions)

**Memory and feasibility**
- Should Teletype state live inside the Track container (requiring size <= current max track type) or be allocated externally to avoid inflating all tracks? (`codex-tele.md:4`, `codex-tele.md:7`, `gemini-tele.md:6`)
- What is the real `scene_state_t` footprint after grid removal, and which estimate do we trust (6-9 KB, 9.2 KB, 16-21 KB, or ~20 KB total)? (`claude-tele.md:37`, `gemini-tele.md:15`, `qwen-tele.md:16`, `gemini-tele.md:104`)
- How many simultaneous Teletype tracks will be officially supported, and will there be a runtime cap based on measured RAM? (`qwen-tele.md:22`, `claude-tele.md:75`, `gemini-tele.md:105`)
- Are we committing to stripping grid structures from `scene_state_t` at compile time (flag) or via manual edits? (`claude-tele.md:33`, `claude-tele.md:107`, `codex-tele.md:397`)

**Timing, routing, and scale**
- Should METRO run strictly from the track divisor, strictly from Teletype's ms timer, or support both modes with a user toggle? (`gemini-tele.md:50`, `codex-tele.md:177`, `claude-tele.md:362`)
- Do we standardize on a `dt` accumulator for `tele_tick`, or retain a fixed 10ms cadence emulation? (`codex-tele.md:155`, `gemini-tele.md:41`)
- Which output expansion model is primary: mapping table, TELEX emulation, output banks, or explicit CV/TR 1-20 routing? (`codex-tele.md:82`, `qwen-tele.md:260`, `claude-tele.md:228`)
- Should TR/IN/PARAM mapping be strictly via routing sources, or do we also allow direct UI/track events as sources? (`codex-tele.md:104`, `qwen-tele.md:229`)
- Do we force project-scale quantization or provide a toggle to keep Teletype's internal scale ops? (`codex-tele.md:195`, `gemini-tele.md:55`, `claude-tele.md:352`, `qwen-tele.md:320`)

**UX, data model, and storage**
- Which scripts are first-class in the UI and serialization: only 1-8/M/I, or also LIVE/DELAY? (`codex-tele.md:21`, `qwen-tele.md:183`, `claude-tele.md:183`, `gemini-tele.md:71`)
- What is the canonical script library format/location (.tele vs .TT vs .tts, and which directory)? (`codex-tele.md:363`, `qwen-tele.md:1367`, `claude-tele.md:96`, `gemini-tele.md:67`)
- Should script sets live only in project files, only as external libraries, or be dual-saved with a source-of-truth policy? (`codex-tele.md:359`, `gemini-tele.md:73`, `claude-tele.md:96`)
- Which UI layout should lead: page-based editor/vars/patterns with op palette, or a split-pane editor + live monitor? (`codex-tele.md:204`, `gemini-tele.md:77`)
- Do we serialize scripts as text and parse on load, or store tokenized commands for faster load? (`codex-tele.md:354`)
- How many routing targets do we expose (only standard track targets, or the full A-Z/Metro/Pattern targets set)? (`claude-tele.md:4384`, `codex-tele.md:140`)

## MVP staging checklist

### Stage 0 - Baseline + memory reality
- [ ] Measure current RAM headroom (heap + stack high-water) and record numbers.
  - Note: SYSTEM → INFO now shows SRAM/CCM usage/free (data/bss); stack high-water requires `INCLUDE_uxTaskGetStackHighWaterMark=1` (enabled in `src/platform/stm32/FreeRTOSConfig.h`).
- [x] Measure `sizeof(scene_state_t)` with grid enabled and with grid removed.
  - Host `sizeof` (libavr32 headers): `scene_state_t` 28976 bytes, `scene_state_no_grid_t` 24008 bytes, `scene_grid_t` 4970 bytes (~4.9 KB delta).
  - Breakdown (key contributors): `scene_delay_t` 9284 bytes (64 queued delayed commands + timing/origin metadata), `scene_script_t` 860 bytes each × 12 scripts = 10320 bytes (6 command slots + EVERY state + last_time per script), `scene_grid_t` 4970 bytes (grid LED + group/button/fader/xypad state).
 Biggest savings (in order):

  - Delay queue (scene_delay_t 9.3 KB).
    Options:
      - Reduce DELAY_SIZE (64 → 16 saves ~7 KB).
      - Make a global delay pool shared across all TT tracks (one queue with track id), so it’s not per‑track.
      - Drop DEL ops entirely.
  - Script storage (scene_script_t 10.3 KB total).
    Options:
      - Reduce SCRIPT_MAX_COMMANDS (6 → 4 saves ~3 KB).
      - Reduce script count (drop LIVE and/or share DELAY temp script globally: save ~0.86 KB per script).
      - Shrink tele_command_t (pack tele_word_t into uint8_t, or reduce COMMAND_MAX_LENGTH 16 → 8): saves ~4–5 KB in scripts + ~4 KB in delay + ~1 KB in
        stack. Biggest win if you can touch command layout.
  - Grid (scene_grid_t 4.9 KB).
    Remove grid + fader ops entirely for track mode: save ~5 KB right away.

  Smaller but still useful cuts:

  - Stack ops (scene_stack_op_t 2.2 KB).
    Reduce STACK_OP_SIZE (16 → 8 saves ~1.1 KB) or move to a shared pool.
  - Calibration + scale arrays in scene_variables_t (~0.7–1 KB).
    If Performer already handles scaling/calibration, you can drop cal_data_t, fader_ranges/scales, in/param scaling for track mode.
  - MIDI state (scene_midi_t ~100 bytes) if you don’t expose TT MIDI ops.

  A pragmatic “track‑sized” recipe:

  1. Remove grid + faders (save ~5 KB).
  2. Make delay queue global or shrink to 16 (save ~7 KB).
  3. Reduce script lines to 4 and drop LIVE (save ~3–4 KB).
     That already takes a no‑grid state (~24 KB) down into the ~10–14 KB range.

  If you want to keep full script syntax, the command layout change (packing tele_word_t + possibly shorter COMMAND_MAX_LENGTH) is the single best “surgical”
  shrink because it hits scripts, delays, and stack all at once.
  - Scripts in `scene_script_t scripts[TOTAL_SCRIPT_COUNT]`: 1–8 + METRO + INIT + DELAY (temp) + LIVE.
  - Proposed track-lite layout: 4 user scripts per track + 1 internal delay scratch script. INIT/METRO are wrapper metadata selecting one of the 4 user scripts. LIVE is dropped; DELAY scratch stays to preserve delayed command execution with origin context.
  - Decision: set `DELAY_SIZE` to 16 for track-lite (saves ~6.9 KB vs 64).
  - Decision: drop grid-related calibration/scales in track mode (`fader_ranges`, `fader_scales`, `cal_data_t`): saves ~1.0–1.1 KB per track.
  - Implemented packed `tele_command_t` storage (parallel `tag[]` + `value[]` arrays with tiny helpers). Scanner/validate/print updated to use helpers; avoids unaligned packing while shrinking command storage.
  - Track-lite size probe (DELAY_SIZE=16, scripts=4+delay, packed commands):
    - `tele_command_t` 52 bytes, `scene_script_t` 356 bytes, `scene_delay_t` 978 bytes.
    - `scene_state_tracklite_t` 4728 bytes after removing `fader_ranges`/`fader_scales` and `cal_data_t` (was 5792 bytes with those included).
  - Track-lite now applied in code (TELETYPE_TRACK_LITE=1):
    - `scene_state_t` 5136 bytes with minimal grid (grid struct 58 bytes, GRID_* counts set to 1).
- [x] Confirm whether Teletype fits under the current `Container` max track size.
  - Host size check (PLATFORM_SIM): `Container<...>::Size` is 10092 bytes (`CurveTrack`), `Track` is 10120 bytes; `scene_state_t` at 28976 bytes would not fit without increasing the container size.
- [x] Decide container strategy: in-container (track-lite `scene_state_t` ~5.1 KB fits under current `Container` max).
- [x] Decide grid removal method: compile-time config (`TELETYPE_TRACK_LITE` trims grid counts to 1, leaving a minimal struct).
- [ ] Decide max supported Teletype tracks (initial cap). (N/A: in-container size is below current max; no cap implied by memory).

### Stage 1 - Core VM + single track (no UI)
- [x] Build Teletype VM into a new track mode with a stub IO bridge.
  - Implemented `TeletypeTrack` + `TeletypeTrackEngine` + `TeletypeBridge` and wired into engine/track containers.
- [x] Run a hardcoded script on track init and verify no crash.
  - Boot script runs `TR.PULSE 1` (track 8 gate in sim when track 8 is Teletype).
- [x] Integrate `tele_tick` with dt-accumulator timing.
  - Engine accumulates ms and calls `tele_tick` in 1–255 ms chunks.
- [ ] Verify Engine task stack usage remains safe.
- [x] Decision: keep dt-accumulator or emulate fixed 10ms tick.
  - Keep dt-accumulator for now; revisit if metro jitter shows up.
  - Added simple metro runner: METRO script runs every `M` ms with `M.ACT=1`.

### Stage 2 - Minimal IO mapping
- [ ] Map Teletype CV1-4 to track CV outputs.
- [ ] Map Teletype TR1-4 to track Gate outputs.
- [ ] Provide fixed IN/PARAM sources (single route source each).
- [ ] Validate script-triggered TR pulses and CV updates.
- [ ] Decision: output expansion model (mapping table vs TELEX vs banks vs CV/TR 1-20).

### Stage 3 - Metro + timing policy
- [ ] Implement ms-based metro (Teletype `M` behavior).
- [ ] Implement divisor-driven metro (track clock sync).
- [ ] Confirm METRO triggers are stable across tempo changes.
- [ ] Decision: ms-only, divisor-only, or dual-mode toggle.

### Stage 4 - Scale + track parity
- [ ] Apply project scale + root note to Teletype note ops.
- [ ] Apply track octave + transpose to Teletype pitch output.
- [ ] Implement CV update mode (gated vs free).
- [ ] Decision: force project scale vs toggle to retain TT internal scales.

### Stage 5 - Script storage + serialization
- [ ] Pick canonical script file format + directory.
- [ ] Decide project-only vs external library vs dual-save source of truth.
- [ ] Choose which scripts are first-class (1-8/M/I vs full set).
- [ ] Implement load/save for the chosen format.
- [ ] Decide text parse on load vs tokenized storage.

### Stage 6 - Minimal editor UI
- [ ] Implement basic script viewer/editor for the chosen script set.
- [ ] Provide a minimal op/token insertion flow (menu or encoder).
- [ ] Implement run-line and run-script actions.
- [ ] Decision: page-based editor/vars/patterns vs split-pane editor + monitor.
