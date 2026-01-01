# Performer Teletype Track: RAM Re-evaluation & Feasibility

**Status: VERY HIGH FEASIBILITY**

### 1. Performer's Allocation Strategy
Performer uses a `Container` template that pre-allocates a fixed-size buffer for every one of the 8 tracks. This buffer is sized to the `maxsizeof` of all supported track types.
*   **Current Max:** The **Note Track** (~11.2 KB) is the current largest type.
*   **The Baseline:** Every track slot in Performer already "pays" for ~11.2 KB of SRAM at startup, regardless of its active mode.

### 2. Teletype Footprint (Optimized)
By removing the `scene_grid_t` (Grid hardware state, ~2-4 KB), the Teletype VM state fits comfortably:
*   **Variables/Patterns:** ~1 KB
*   **Scripts (Tokenized):** ~4.4 KB
*   **Delay/Wait Queue:** ~3.8 KB
*   **Total:** **~9.2 KB**

### 3. Conclusion
Adding `TeletypeTrack` **will not increase total system RAM usage**. It simply utilizes the pre-allocated 11.2 KB slot already reserved for each track. The integration is "free" memory-wise as long as we keep the Teletype state under the Note Track baseline.

---

# Performer Teletype Track Integration Plan

## 1. Core Architecture: `TeletypeTrack` and `TeletypeTrackEngine`

We will implement a new track type, `TeletypeTrack` (Model) and `TeletypeTrackEngine` (Engine), wrapping the Teletype codebase as a library.

*   **Model (`TeletypeTrack`):**
    *   Stores `scene_state_t` (variables, scripts, patterns) as the track data.
    *   **Track Parity:** Includes standard Performer track properties:
        *   `Divisor` (Clock division)
        *   `Octave`, `Transpose` (Pitch offsets)
        *   `Route Notes` (Output routing for pitch)
        *   `CV Update Mode` (Gated vs Free)
    *   **Serialization:** Use `serialize_scene` logic adapted for Performer's `VersionedSerializedWriter`.

*   **Engine (`TeletypeTrackEngine`):**
    *   **VM Instance:** Embeds the Teletype logic (parsing, execution, state).
    *   **Tick Handling:**
        *   Performer calls `tick(uint32_t tick)`.
        *   Teletype expects `tele_tick()` called at 10ms intervals for delays. We can adapt this:
            *   Run `tele_tick` logic based on `dt` accumulation in `update(float dt)`.
    *   **Execution:**
        *   `run_script(i)` is called based on Triggers (from Performer Sequences? Or internal logic?).

## 2. Track Behavior Parity

To feel like a native Performer track, the Teletype track must obey standard musical controls.

### Clock & Metronome
The `METRO` script (Script 9) is traditionally driven by a millisecond timer (`M` variable).
*   **Performer Mode:** Use the **Track Divisor** to drive the Metronome.
*   **Logic:** Trigger `METRO` script every time the Track Engine advances a step (based on Divisor).

### Scale & Pitch Integration
Teletype has internal quantization (`QT`, `N.S`). We will override or wrap these to use Performer's Project Scale.
*   **Note Ops (`N`, `QT`):** Intercept these to use `model.project().selectedScale()`, adding `Track::octave()` and `Track::transpose()`.

### CV Update Modes
*   **Free Mode:** `CV` ops update hardware outputs immediately.
*   **Gated Mode:** `CV` ops write to a buffer, hardware updates only on a Gate On event.

## 3. Independent Script Management (Standalone Files)

Scripts will be savable and loadable independent of Performer projects to allow for an "Algorithm Library".

*   **File Format:** `.TT` (Plain text files using Teletype's standard scene format).
*   **Storage Path:** `/SCRIPTS/*.TT` on the SD Card.
*   **UI Integration:**
    *   A "File" menu within the Teletype track page.
    *   **Save Script:** Exports all 10 scripts (1-8, M, I) + Pattern data to a `.TT` file.
    *   **Load Script:** Imports a `.TT` file into the current Teletype track, overwriting existing scripts.
    *   **Project Backup:** The scripts are *also* saved within the Performer `.PPR` project file for consistency. Standalone files are for sharing and reuse.

## 4. UI Design (256x64 OLED Optimization)

With double the horizontal resolution of the original Teletype (128x64 vs 256x64), we will implement a **Split-Pane UI**.

### Layout Map
*   **Left Pane (128x64):** **The Script Editor**.
    *   Displays 6 lines of code for the active script.
    *   Top Bar: Script ID (1-8, M, I) + Script Name (if provided in header).
    *   Bottom Bar: Status messages (errors, return values).
*   **Right Pane (128x64):** **The Live Monitor**.
    *   **View A (Default):** Activity monitor (CV levels, Gate states, Metro heartbeat).
    *   **View B (Toggle):** Variable Watcher (A, B, X, Y, etc.).
    *   **View C (Toggle):** Pattern Overview (Mini tracker view).

### Controls (Physical Mapping)
*   **Step Buttons 1-8:** Direct jump to Script 1-8.
*   **Function + Step 1-2:** Jump to `METRO` and `INIT` scripts.
*   **Encoder 1:** Vertical navigation (Line Select).
*   **Encoder 2:** Horizontal navigation (Cursor Position).
*   **Encoder 3:** Value/Token Selector (scrolls through common ops and characters).
*   **Left/Right Buttons:** Previous/Next Page (Editor -> Vars -> Patterns -> I/O Map).
*   **Shift + Step Button:** Manual trigger of that script (for testing).

## 5. Resource Evaluation

### Memory (RAM)
*   **State Buffer:** `scene_state_t` is approximately **12-15 KB** per track. 
*   **Text Buffers:** Editing requires a raw text buffer for the current line (~64-128 bytes).
*   **Undo Buffer:** 3-depth undo for one script is ~2 KB.
*   **Total:** ~20 KB per Teletype track. 
    *   *Constraint:* Performer has ~128 KB of CCM RAM and ~192 KB of SRAM. Supporting 8 Teletype tracks simultaneously (~160 KB) might be tight. We may limit the number of Teletype tracks to **4** or use dynamic allocation.

### CPU (Processing)
*   **Interpreter:** Extremely efficient (written in pure C). Executing a 6-line script takes microseconds.
*   **Parsing:** Tokenizing a line on "Enter" is faster than physical screen refresh.
*   **Metronome:** Low overhead; handled by the existing Performer engine tick.
*   **Impact:** Negligible impact on the main engine loop unless users write infinite `WHILE` loops (protected by `WHILE_DEPTH` limit).

### Storage (SD Card)
*   **Script Files:** A full scene is ~4-8 KB. An 8GB SD card can hold millions of scripts.

## 6. Implementation Code Ideas (Standalone File Loading)

```cpp
// src/apps/sequencer/model/TeletypeTrack.cpp
void TeletypeTrack::loadStandaloneScript(const char* path) {
    File file(path, File::Mode::Read);
    if (file.isOpen()) {
        TeletypeDeserializer stream(file); // Adapter for tt_deserializer_t
        deserialize_scene(&stream, &_sceneState, &_sceneText);
        runScript(INIT_SCRIPT);
    }
}
```
