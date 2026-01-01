# Teletype MVP: Minimal Viable Product Checklist

## 1. Goal
Get the core Teletype interpreter running as a Performer track, executing scripts, and producing CV/Gate output. 
**Constraint:** Must fit in `NoteTrack` memory footprint (< 11.2 KB).

## 2. Core Decisions (MVP Scope)
*   [ ] **No Grid:** Compile with `NO_GRID`.
*   [ ] **No External Routing (Input):** Hardcode inputs initially (e.g., Script 1 = Clock, Script 2 = Gate In 1).
*   [ ] **No Routing Targets (Variable Mod):** Skip variable modulation for MVP.
*   [ ] **Simple UI:** Single pane editor (no split view), simple encoder input.
*   [ ] **Storage:** Only Project serialization (no standalone `.tt` files).

## 3. Optimization Strategy (Refined "Teletype Lite")
**Target State Size:** ~9 KB (Fits comfortably in 11.2 KB Container).

To achieve this without breaking compatibility (keeping 6 lines/script), we apply these specific cuts:

1.  **Buffer Tuning:**
    *   `DELAY_SIZE` 64 → 16 (Saves ~6.9 KB).
    *   `STACK_OP_SIZE` 16 → 8 (Saves ~0.4 KB).
    *   `Q_LENGTH` 64 → 16 (Minor save).
2.  **Struct Optimization (Manual Packing):**
    *   Do **NOT** use `__attribute__((packed))` blindly (risks unaligned access faults on STM32).
    *   Change `tele_word_t` from `enum` (4 bytes) to `uint8_t`.
    *   This shrinks `tele_data_t` and `tele_command_t` significantly (~136B → ~60-70B per line) while maintaining alignment.
3.  **Strict NO_GRID:**
    *   Exclude `grid` struct, `fader_ranges`, `fader_scales`, `cal_data` faders.

**Result:** ~8-9 KB total state size.

## 4. Step-by-Step Implementation Checklist

### Step 1: Core VM Integration (The "Brain")
*   [ ] **Task 1.1:** Create `src/apps/sequencer/teletype/` and copy/port core Teletype C files.
    *   Files: `teletype.c`, `state.c`, `ops/*.c` (exclude grid/i2c).
*   [ ] **Task 1.2:** Configure `CMakeLists.txt` to build the library.
*   [ ] **Task 1.3:** Create `TeletypeTrack` Model class (Empty shell).
*   [ ] **Task 1.4:** **CRITICAL:** Add `TeletypeTrack` to `Track::Data` union and verify compilation size.
    *   *Success Criteria:* Build passes, firmware size is acceptable, no stack overflows.
*   [ ] **Task 1.5:** Write Unit Test: Initialize VM, run `A = 1 + 1`, verify `A == 2`.

### Step 2: Engine & Hardware Bridge (The "Body")
*   [ ] **Task 2.1:** Implement `TeletypeHardwareAdapter` (Stubs).
    *   `setCvOutput` -> `printf("CV %d: %d\n")`.
*   [ ] **Task 2.2:** Implement `TeletypeTrackEngine`.
    *   Instantiate `teletype` struct.
    *   Implement `update()` loop (accumulate dt, call `tele_tick`).
*   [ ] **Task 2.3:** Connect Output Logic.
    *   Map `TeletypeHardwareAdapter::setCvOutput` -> `TrackEngine::cvOutput`.
*   [ ] **Task 2.4:** Connect Input Logic.
    *   In `tick()`, call `tele_run_script(SCENE, 1)` on every 1/16th note (Simulate Metro).

### Step 3: Minimal UI (The "Face")
*   [ ] **Task 3.1:** Create `TeletypeTrackPage`.
*   [ ] **Task 3.2:** Render Script 1 text (Hardcoded "Hello World" script).
*   [ ] **Task 3.3:** Implement basic Editor.
    *   Encoder 1: Select Line.
    *   Encoder 2: Scroll Char (A-Z, 0-9).
    *   Encoder 3: Insert Space/Delete.

### Step 4: Verification
*   [ ] **Task 4.1:** Integration Test.
    *   Write script: `CV 1 N 60`.
    *   Run firmware.
    *   Verify Track CV Output 1 changes to ~0V (C4).
*   [ ] **Task 4.2:** Memory Check.
    *   Verify `NoteTrack` footprint is not exceeded.

## 4. Post-MVP Features (To be added later)
*   [ ] Advanced Input Routing (Map any trigger source to any script).
*   [ ] Variable Modulation (Routing Targets -> Vars A-Z).
*   [ ] Split-Pane UI.
*   [ ] Standalone `.tt` Library.
*   [ ] T9 Input.
