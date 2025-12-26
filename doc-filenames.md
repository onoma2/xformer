# PEW|FORMER Track Navigation: S0, S1, S2 Page Files

This document provides a comprehensive list of the files associated with each Page + S0, S1, S2 navigation option for each track type, including second press behaviors.

## Note Track

### Page + S0 (SequenceEdit)
- **File**: `NoteSequenceEditPage` (`src/apps/sequencer/ui/pages/NoteSequenceEditPage.h/cpp`)
- **Second Press**: N/A (stays in sequence edit mode)

### Page + S1 (Sequence)
- **File**: `NoteSequencePage` (`src/apps/sequencer/ui/pages/NoteSequencePage.h/cpp`)
- **Second Press**: Switches to `AccumulatorPage` (`src/apps/sequencer/ui/pages/AccumulatorPage.h/cpp`)

### Page + S2 (Track)
- **File**: `TrackPage` (`src/apps/sequencer/ui/pages/TrackPage.h/cpp`)
- **Second Press**: Switches to `HarmonyPage` (`src/apps/sequencer/ui/pages/HarmonyPage.h/cpp`)

---

## Curve Track

### Page + S0 (SequenceEdit)
- **File**: `CurveSequenceEditPage` (`src/apps/sequencer/ui/pages/CurveSequenceEditPage.h/cpp`)
- **Second Press**: N/A (stays in sequence edit mode)

### Page + S1 (Sequence)
- **File**: `CurveSequencePage` (`src/apps/sequencer/ui/pages/CurveSequencePage.h/cpp`)
- **Second Press**: N/A (stays in curve sequence mode, no cycling)

### Page + S2 (Track)
- **File**: `TrackPage` (`src/apps/sequencer/ui/pages/TrackPage.h/cpp`)
- **Second Press**: N/A (stays in track mode, no cycling)

---

## Tuesday Track

### Page + S0 (SequenceEdit)
- **File**: `TuesdayEditPage` (`src/apps/sequencer/ui/pages/TuesdayEditPage.h/cpp`)
- **Second Press**: N/A (stays in edit mode)

### Page + S1 (Sequence)
- **File**: `TuesdaySequencePage` (`src/apps/sequencer/ui/pages/TuesdaySequencePage.h/cpp`)
- **Second Press**: N/A (stays in sequence mode, no cycling)

### Page + S2 (Track)
- **File**: `TrackPage` (`src/apps/sequencer/ui/pages/TrackPage.h/cpp`)
- **Second Press**: N/A (stays in track mode, no cycling)

---

## DiscreteMap Track

### Page + S0 (SequenceEdit)
- **File**: `DiscreteMapSequencePage` (`src/apps/sequencer/ui/pages/DiscreteMapSequencePage.h/cpp`)
- **Second Press**: N/A (stays in sequence mode)

### Page + S1 (Sequence)
- **File**: `DiscreteMapStagesPage` (`src/apps/sequencer/ui/pages/DiscreteMapStagesPage.h/cpp`)
- **Second Press**: Switches to `DiscreteMapSequenceListPage` (`src/apps/sequencer/ui/pages/DiscreteMapSequenceListPage.h/cpp`)

### Page + S2 (Track)
- **File**: `TrackPage` (`src/apps/sequencer/ui/pages/TrackPage.h/cpp`)
- **Second Press**: N/A (stays in track mode, no cycling)

---

## Indexed Track

### Page + S0 (SequenceEdit)
- **File**: `IndexedSequenceEditPage` (`src/apps/sequencer/ui/pages/IndexedSequenceEditPage.h/cpp`)
- **Second Press**: N/A (stays in sequence edit mode)

### Page + S1 (Sequence)
- **File**: `IndexedSequencePage` (`src/apps/sequencer/ui/pages/IndexedSequencePage.h/cpp`)
- **Second Press**: Switches to `IndexedStepsPage` (`src/apps/sequencer/ui/pages/IndexedStepsPage.h/cpp`)

### Page + S2 (Track)
- **File**: `TrackPage` (`src/apps/sequencer/ui/pages/TrackPage.h/cpp`)
- **Second Press**: N/A (stays in track mode, no cycling)

---

## MidiCv Track

### Page + S0 (SequenceEdit)
- **File**: `TrackPage` (`src/apps/sequencer/ui/pages/TrackPage.h/cpp`) - *Note: No dedicated sequence edit*
- **Second Press**: N/A (stays in track mode)

### Page + S1 (Sequence)
- **File**: `TrackPage` (`src/apps/sequencer/ui/pages/TrackPage.h/cpp`) - *Note: No dedicated sequence page*
- **Second Press**: N/A (stays in track mode)

### Page + S2 (Track)
- **File**: `TrackPage` (`src/apps/sequencer/ui/pages/TrackPage.h/cpp`)
- **Second Press**: N/A (stays in track mode, no cycling)

---

## Summary of Second Press Behaviors

### Dual Sequence Views (cycles between views):
- **Note**: NoteSequence ↔ Accumulator
- **DiscreteMap**: DiscreteMapStages ↔ DiscreteMapSequenceList
- **Indexed**: IndexedSequence ↔ IndexedSteps

### Dual Track Views (cycles between views):
- **Note**: Track ↔ Harmony (only track type with this feature)

### No Cycling (stays in same view):
- **Curve**, **Tuesday**, **MidiCv** tracks

### Special Case:
- **MidiCv**: Uses only TrackPage for all navigation options (no dedicated sequence views)