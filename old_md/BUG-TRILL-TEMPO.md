# Bug Report: Tuesday Track Trill Timing is Not Tempo-Synced

## 1. Description

A bug was identified where the new trill/re-trigger feature on the `TuesdayTrack` has a fixed timing. Unlike the `NoteTrack`'s re-triggers, the speed of the trills does not change when the project tempo or the track's clock `divisor` is changed.

## 2. Root Cause

The timing calculation for the trill's speed (specifically, the `_retriggerPeriod`) was incorrectly based on a fixed system constant (`CONFIG_SEQUENCE_PPQN`).

The correct implementation, as seen in the `NoteTrackEngine`, is to base this calculation on the track's tempo-synced `divisor`, which represents the actual duration of a step in high-resolution ticks.

**Incorrect Logic:**
`_retriggerPeriod = CONFIG_SEQUENCE_PPQN / 3;`

**Correct Logic:**
`_retriggerPeriod = divisor / 3;`

## 3. Current Status

The bug is **FIXED**.

## 4. Fix Applied

The following changes were made to `TuesdayTrackEngine.cpp`:

1. **Line 2323** (finite/buffered loops):
   - Changed `_retriggerPeriod = CONFIG_SEQUENCE_PPQN / 3;`
   - To `_retriggerPeriod = divisor / 3;`

2. **Line 3116** (infinite loops/DRILL algorithm):
   - Changed `_retriggerPeriod = CONFIG_SEQUENCE_PPQN / 3;`
   - To `_retriggerPeriod = divisor / 3;`

3. **Line 3125** (gate percent calculation):
   - Changed `_gatePercent = (_retriggerLength * 100) / CONFIG_SEQUENCE_PPQN;`
   - To `_gatePercent = (_retriggerLength * 100) / divisor;`

The trill timing is now properly synced to the project tempo via the track's `divisor`, matching how `NoteTrackEngine` handles retriggers.