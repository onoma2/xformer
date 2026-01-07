# Teletype Script View: I/O Grid Visualization Implementation Plan

This document outlines the plan to implement the I/O visualization grid in C++.

## 1. Visual Design
-   **Grid:** 32x32 pixels, located at `x=222, y=15`.
-   **Structure:**
    -   Row 1 (TI): Trigger Inputs (Status).
    -   Row 2 (TO): Trigger Outputs (Assignment & Pulse).
    -   Row 3+4 (CV): CV Outputs (Assignment & Value).

## 2. Implementation Steps

### A. Header Updates (`TeletypeScriptViewPage.h`)
Add the helper method declaration:
```cpp
private:
    void drawIoGrid(Canvas &canvas);
```

### B. Implementation Logic (`TeletypeScriptViewPage.cpp`)

1.  **Constants:**
    Define the grid layout constants at the top of the file (namespace):
    ```cpp
    constexpr int kGridX = 222;
    constexpr int kGridY = 15;
    constexpr int kGridColW = 8;
    constexpr int kGridRowH = 8;
    constexpr int kGridCvH = 16;
    ```

2.  **`drawIoGrid` Method:**
    Implement the drawing loop.

    **Dependencies:**
    -   `_project`: To access `gateOutputTrack` and `cvOutputTrack`.
    -   `_engine`: To access `TeletypeTrackEngine` state (`inputState`, `gateOutput`, `cvRaw`).
    -   `track`: The current `TeletypeTrack` model (for IO mapping).

    **Logic - Row 1 (TI):**
    -   Iterate 0..3.
    -   Check `track.triggerInputSource(i) != TriggerInputSource::None`.
    -   Check `engine.inputState(i)`.
    -   Draw 6x6 rect.

    **Logic - Row 2 (TO):**
    -   Iterate 0..3.
    -   Get `dest = track.triggerOutputDest(i)`.
    -   Get `gateIdx` from `dest`.
    -   **Ownership Check:** `_project.gateOutputTrack(gateIdx) == _project.selectedTrackIndex()`.
    -   Check Pulse: `engine.gateOutput(gateIdx)`.
    -   Draw 6x6 rect (Low if not owned, Medium if owned, Bright if pulse).

### Row 3: CV Outputs (CV)
-   **Ownership Check:** Verify if the target `CvOut` is assigned to the current track in the Project Layout (`_project.cvOutputTrack()`).
-   **Container:** A 6x14 pixel outline (Color: `Low`) is always drawn.
-   **Bar Graph (Bipolar):**
    -   **Midpoint:** 8192 (0V) is center (7px from top).
    -   **Range:** +/- 8192 maps to +/- 7px height.
    -   **Positive (>8192):** Grow UP from center.
    -   **Negative (<8192):** Grow DOWN from center.
    -   **Owned:** Bar is `MediumBright`.
    -   **Mismatch:** Bar is not drawn.

3.  **Integration:**
    Call `drawIoGrid(canvas)` inside `draw(Canvas &canvas)`, after drawing the script lines.

### C. Validation
-   Compile and run in simulator.
-   Check alignment with header text (ensure no overlap).
-   Verify colors change based on I/O assignments in Layout Page.

## 3. Code Snippet (Draft)

```cpp
void TeletypeScriptViewPage::drawIoGrid(Canvas &canvas) {
    const int trackIndex = _project.selectedTrackIndex();
    const auto &track = _project.selectedTrack().teletypeTrack();
    // Safe cast check handled in draw()
    const auto &trackEngine = _engine.selectedTrackEngine().as<TeletypeTrackEngine>();

    for (int i = 0; i < 4; ++i) {
        int x = kGridX + i * kGridColW;

        // --- Row 1: TI ---
        int yTi = kGridY;
        bool tiAssigned = track.triggerInputSource(i) != TeletypeTrack::TriggerInputSource::None;
        bool tiActive = trackEngine.inputState(i);

        canvas.setColor(tiAssigned ? (tiActive ? Color::MediumBright : Color::Medium) : Color::Low);
        canvas.fillRect(x + 1, yTi + 1, 6, 6);

        // --- Row 2: TO ---
        int yTo = kGridY + kGridRowH;
        auto toDest = track.triggerOutputDest(i);
        int gateIdx = int(toDest); // GateOut1=0...
        bool toOwned = _project.gateOutputTrack(gateIdx) == trackIndex;
        bool toActive = trackEngine.gateOutput(gateIdx);

        if (toOwned) {
            canvas.setColor(toActive ? Color::MediumBright : Color::Medium);
        } else {
            canvas.setColor(Color::Low);
        }
        canvas.fillRect(x + 1, yTo + 1, 6, 6);

        // --- Row 3: CV ---
        int yCv = kGridY + kGridRowH * 2;
        auto cvDest = track.cvOutputDest(i);
        int cvIdx = int(cvDest); // CvOut1=0...
        bool cvOwned = _project.cvOutputTrack(cvIdx) == trackIndex;
        uint16_t raw = trackEngine.cvRaw(i);

        // Container
        canvas.setColor(Color::Low);
        canvas.drawRect(x + 1, yCv + 1, 6, 14);

        if (cvOwned) {
            canvas.setColor(Color::MediumBright);
            int32_t val = int32_t(raw) - 8192;
            // Max swing 5px to fit within 12px inner height (yCv+2 to yCv+13)
            // Center is at yCv+8
            int h = (std::abs(val) * 5) / 8192;
            h = clamp(h, 0, 5);

            int centerY = yCv + 8;

            // Draw Center Line
            canvas.fillRect(x + 2, centerY, 4, 1);

            if (h > 0) {
                if (val >= 0) {
                    // Grow Up: y = CenterY - h
                    canvas.fillRect(x + 2, centerY - h, 4, h);
                } else {
                    // Grow Down: y = CenterY + 1
                    canvas.fillRect(x + 2, centerY + 1, 4, h);
                }
            }
        }
    }
}
`
# Overview Page: Note Track Step Rendering Specifications

This document details the exact dimensions, colors, and logic used to render the 16-step view for Note Tracks in the `OverviewPage`.

## 1. Geometry

Each step is allocated an **8x8 pixel** area. The actual "step" is rendered as a centered **6x6 pixel** rectangle to provide a 1px border on all sides.

- **Cell Size:** 8x8 pixels
- **Rectangle Size:** 6x6 pixels
- **Internal Padding:** 1 pixel (rendered at `x + 1, y + 1`)
- **Horizontal Range:** `x = 64` to `x = 191` (16 steps * 8 pixels = 128 pixels)
- **Vertical Alignment:** `y = trackIndex * 8`

## 2. Color Intensities

The rendering uses four distinct intensities from the `Color` enum based on the intersection of **Playhead State** and **Gate State**.

| State | Condition | Color Constant | Intensity Level |
| :--- | :--- | :--- | :--- |
| **Idle / No Gate** | `!playing && !gate` | `Color::Low` | Lowest |
| **Idle / Gate** | `!playing && gate` | `Color::Medium` | Low-Mid |
| **Playing / No Gate** | `playing && !gate` | `Color::MediumBright` | Mid-High |
| **Playing / Gate** | `playing && gate` | `Color::Bright` | Maximum |

## 3. Implementation Logic

The `drawNoteTrack` function implements this using `canvas.fillRect`.

### Blend Mode
- **Mode:** `BlendMode::Set` (overwrites background pixels)

### Code Snippet
```cpp
// Dimensions: 6x6 rectangle within an 8x8 grid
int x = 64 + i * 8;
int y = trackIndex * 8;

if (trackEngine.currentStep() == stepIndex) {
    // PLAYHEAD IS ON THIS STEP
    canvas.setColor(step.gate() ? Color::Bright : Color::MediumBright);
    canvas.fillRect(x + 1, y + 1, 6, 6);
} else {
    // STEP IS IDLE
    canvas.setColor(step.gate() ? Color::Medium : Color::Low);
    canvas.fillRect(x + 1, y + 1, 6, 6);
}
```

## 4. Layout Overview

The Overview Page divides the 256x64 display into the following horizontal sections:
- **0 - 63:** Track Labels (T1-T8) and Pattern Numbers (P1-P16)
- **64 - 191:** Step Sequence View (The 16-step window)
- **192 - 255:** Output Monitoring (Gate indicator and CV Voltage)
