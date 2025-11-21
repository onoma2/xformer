# UI Plan: Displaying TuesdayTrack on the Monitor Page

## Problem

The Monitor Page, which provides an overview of all tracks, currently displays nothing for tracks set to `Tuesday` mode. This is because the page's UI logic only handles `NoteTrack` and has no implementation for `TuesdayTrack`.

## Investigation

Analysis of the UI code revealed that a UI for `TuesdayTrack` already exists, but it is only displayed in the footer of the main `TracksPage`. This "info box" shows:

*   Current step and total loop length (e.g., `3/16`)
*   The current note value
*   A gate status indicator (`[G]`)

This data is retrieved from the `TuesdayTrackEngine` by calling methods like `currentStep()`, `actualLoopLength()`, and `gateOutput()`. This existing logic can be adapted for the Monitor Page.

## Proposal: Compact UI for Monitor Page

The information from the `TracksPage` info box can be reformatted to fit the compact, single-line layout of the Monitor Page.

The proposed format for each `TuesdayTrack` line is: `[MODE][STEP/LOOP]`

**1. `[MODE]` field (1-2 characters):**
*   **Finite Loop:** Displays `G` when the track's gate is active, and is blank otherwise.
*   **Infinite Loop:** Displays a static infinity symbol `∞` to clearly communicate its non-looping, generative nature.

**2. `[STEP/LOOP]` field:**
*   **Finite Loop:** Displays the current step and total loop length (e.g., `5/32`).
*   **Infinite Loop:** Displays the continuously incrementing step count (e.g., `138`).

### Examples:

*   Finite loop, step 5 of 32, gate is active: `G 5/32`
*   Finite loop, step 6 of 32, gate is off: `  6/32`
*   Infinite loop, step 256, gate is active: `∞ 256`

This solution reuses existing logic to provide a highly informative, at-a-glance status for `TuesdayTrack` that respects the layout constraints and the track's unique modes of operation.
