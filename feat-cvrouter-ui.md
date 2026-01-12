# Feature Specification: CV Router UI Improvements

## Overview
Enhance the `CvRoutePage` to provide a more intuitive and informative user interface for the CV Router functionality. The current implementation uses a basic text-based grid which lacks visual cues about signal flow and real-time activity.

## Design Goals
1.  **Visual Matrix/Flow:** Replace the text list with a graphical representation of the "Scan & Pan" architecture.
2.  **Real-time Feedback:** Visualize the signal levels at inputs, the intermediate mix point, and outputs.
3.  **Clear Navigation:** Improve the selection cursor and editing interaction.
4.  **Route/Scan Visualization:** Graphically show the position of the "Scan" (Read) and "Route" (Write) heads relative to the lanes.

## Proposed Layout

```
+--------------------------------------------------+
| CV ROUTER                                        |
+--------------------------------------------------+
|                                                  |
|  [IN 1]   [IN 2]   [IN 3]   [IN 4]    SCAN: 33%  |
|    |        |        |        |         [--|--]  |
|    |        |        |        |                  |
|    +--------+---V----+--------+       [MIX BAR]  |
|                 |                                |
|    +--------+---^----+--------+                  |
|    |        |        |        |                  |
|    |        |        |        |       ROUTE: 66% |
|  [OUT 1]  [OUT 2]  [OUT 3]  [OUT 4]     [----|]  |
|                                                  |
+--------------------------------------------------+
|  CV IN 1  BUS 2    OFF      CV IN 4     NEXT     |
+--------------------------------------------------+
```

### Key Elements

1.  **Input/Output Lanes (Columns 0-3):**
    *   **Top Row:** Input Sources.
    *   **Bottom Row:** Output Destinations.
    *   **Selection:** Box highlight around the active cell.
    *   **Content:** Short labels (e.g., "CV1", "BUS2", "OFF") or icons.
    *   **Activity Bars:** Small vertical bars next to each cell showing the real-time signal level (bipolar).

2.  **Scan/Route Controls (Column 4):**
    *   **Top:** SCAN parameter value and a horizontal slider/bar indicating position.
    *   **Bottom:** ROUTE parameter value and horizontal slider/bar.
    *   **Visual Aid:** The slider position should visually correlate with the "weighted" lanes. For example, if Scan is 0%, the indicator aligns with Lane 1.

3.  **Signal Flow Visualization (Center Area):**
    *   **Connection Lines:** Draw lines connecting Inputs to a central "Bus" line and then to Outputs.
    *   **Active Path:** Highlight the lines corresponding to the current Scan/Route positions. For example, if Scan is reading from Input 2, highlight the line from Input 2 to the mix bus.
    *   **Mix Indicator:** A central bar or meter showing the signal level of the currently scanned/mixed signal before it is routed.

## Implementation Details

### 1. `CvRoutePage::draw` Refactoring

*   **Grid Layout:** Maintain the 5-column layout but use graphical elements instead of just text.
*   **DrawBipolarBar:** Reuse `TeletypeScriptViewPage::drawBipolarBar` logic (or move it to `WindowPainter` or a common helper) to draw signal indicators.
    *   *Challenge:* Accessing real-time signal levels from the UI thread. The `CvRouterEngine` (inside `Engine`) processes at 1kHz. We need a way to peek at these values.
    *   *Solution:* Add `cvRouteInput(lane)` and `cvRouteOutput(lane)` accessors to `Engine` or `CvRouteEngine` that return the latest processed sample.

### 2. Interaction Improvements

*   **Encoder Navigation:**
    *   **Horizontal:** Function keys F1-F5 (already implemented) or Encoder push to toggle columns? The current F-key selection is good for random access.
    *   **Vertical:** Toggle between Input and Output rows (already implemented).
    *   **Value Change:** Encoder turn changes the selected parameter (Source, Dest, Scan, Route).

*   **Shortcuts:**
    *   `Shift + Encoder`: Coarse adjustment for Scan/Route (already implemented).
    *   `Encoder Push`: Toggle Input/Output row? Or "Quick Select" for sources?

### 3. Visual Polish

*   **Labels:** Use compact labels: "I:CV1", "I:BS1", "I:---" / "O:CV1", "O:BS1", "O:---".
*   **Highlighting:**
    *   Bright white for selected parameter.
    *   Dim white for active signal paths.
    *   Grey for inactive paths.

## Code Changes Required

1.  **`src/apps/sequencer/engine/Engine.h` / `.cpp`:**
    *   Expose `cvRouteInputs` and `cvRouteOutputs` (arrays of float) for UI visualization.

2.  **`src/apps/sequencer/ui/painters/WindowPainter.h` / `.cpp`:**
    *   Add `drawBipolarBar(Canvas &canvas, int x, int y, int w, int h, float value)` helper.

3.  **`src/apps/sequencer/ui/pages/CvRoutePage.cpp`:**
    *   Rewrite `draw()` to implement the new graphical layout.
    *   Implement signal visualization using the new Engine accessors.

## Example Flow Visualization

*   **Scan = 0%:**
    *   Highlight Input 1 Box.
    *   Draw bright line from Input 1 to Center Mix.
    *   Input 2, 3, 4 lines are dim.
*   **Scan = 50%:**
    *   Highlight Input 2 and Input 3 Boxes (partial).
    *   Draw semi-bright lines from Input 2 and Input 3 to Center Mix.
*   **Route = 100%:**
    *   Highlight Output 4 Box.
    *   Draw bright line from Center Mix to Output 4.
