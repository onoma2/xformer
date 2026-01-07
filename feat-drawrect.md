# Teletype Script View: I/O Grid Visualization Implementation Plan

This document outlines the implementation for the Teletype I/O visualization grid.

## 1. Design & Placement
-   **Right Margin:** 2px (Right edge at `x=254`).
-   **Grid Height:** 32px (y=15 to y=47, clearing the 8px header).
-   **Layout Order (Left to Right):** `[BUS] [GAP] [MAIN GRID] [IN/PARAM]`

| Block | Function | X Position | Width | Description |
| :--- | :--- | :--- | :--- | :--- |
| **BUS** | BUS 1-4 | `196, 204` | 16px | 2x2 block. Top: B1/B2, Bot: B3/B4. |
| **GAP** | Separator | `212` | 2px | Vertical spacing. |
| **MAIN** | TI/TO/CV 1-4 | `214 - 245` | 32px | 4 columns. TI(8px), TO(8px), CV(16px). |
| **EXT** | IN / PARAM | `246` | 8px | Single column. Top: IN, Bot: PARAM. |

## 2. Rendering Rules

### General Color Rules (Matching TI-TR1-4)
- **NONE (Unassigned/Mismatch):** Color: `Low`.
- **ASSIGNED (Owned):** Color: `Medium` (Static) or `MediumBright` (Active Signal).

### Component Logic

1. **TI (Trigger Inputs) - Row 1**
   - **Data:** `trackEngine.inputState(i)`.
   - **Rule:** `track.triggerInputSource(i) == None ? Low : (Active ? Bright : Medium)`.
   - **Visual:** 6x6 Filled Rect.

2. **TO (Trigger Outputs) - Row 2**
   - **Data:** `trackEngine.gateOutput(gateIdx)`.
   - **Ownership:** `_project.gateOutputTrack(gateIdx) == currentTrack`.
   - **Rule:** `!Owned ? Low : (Active ? Bright : Medium)`.
   - **Visual:** 6x6 Filled Rect.

3. **CV Outputs - Row 3+4**
   - **Ownership:** `_project.cvOutputTrack(cvIdx) == currentTrack`.
   - **Rule:** `!Owned ? Low : MediumBright`.
   - **Visual:** Bipolar Bar Graph (Center y=8, +/- 5px swing).

4. **IN / PARAM - Right Column**
   - **IN (Top 16px):** `track.cvInSource() == None ? Low : MediumBright`.
   - **PARAM (Bot 16px):** `track.cvParamSource() == None ? Low : MediumBright`.
   - **Visual:** Bipolar Bar Graph.

5. **BUS 1-4 - Left Block**
   - **Data:** `_engine.busCv(i)`.
   - **Rule:** Always `MediumBright`.
   - **Visual:** Bipolar Bar Graph.

## 3. Implementation Methods

### `drawBipolarBar(Canvas &canvas, int x, int y, uint16_t value, Color color)`
- Draws a 6x14 hollow frame (`Color::Low`).
- Draws center line (1px) and bipolar bar (5px max swing) using `color`.

### `drawIoGrid(Canvas &canvas)`
- Iterates and calculates positions for all blocks.
- Fetches data from `TeletypeTrackEngine`.
- Respects `_project` layout ownership.

---

# Overview Page: Note Track Step Rendering Specifications

This section details the 16-step view for Note Tracks in the `OverviewPage`.

## 1. Geometry
- **Cell Size:** 8x8 pixels.
- **Rectangle Size:** 6x6 pixels (at `x+1, y+1`).
- **Layout:** 16 steps horizontally (`x=64` to `191`).

## 2. Color Intensities
| State | Condition | Color Constant |
| :--- | :--- | :--- |
| **Idle / No Gate** | `!playing && !gate` | `Color::Low` |
| **Idle / Gate** | `!playing && gate` | `Color::Medium` |
| **Playing / No Gate** | `playing && !gate` | `Color::MediumBright` |
| **Playing / Gate** | `playing && gate` | `Color::Bright` |