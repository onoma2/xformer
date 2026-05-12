# Performer Improvements — Non-Launchpad Fork Integrations

## Overview

This task documents all non-Launchpad-related improvements from the VinxScorza, Modulove, and Mebitek performer forks that should be integrated into the XFORMER project, reorganized against current XFORMER codebase with feasibility analysis and design decisions.

Launchpad-specific improvements (LP Style, Circuit Keyboard, Performer Mode, etc.) live in `launchpad-track-port`.

## Current XFORMER Codebase Analysis

### Track Types (7 types, 8 tracks total)
| Track | XFORMER Status | In Vinx? | In Modulove? | In Mebitek? |
|-------|---------------|----------|-------------|------------|
| Note Circus | Current | Yes | Yes | Yes |
| Curve Studio | Current | Yes | Yes | Yes |
| MIDI/CV | Current | Yes | Yes | Yes |
| Algo(Tuesday) | XFORMER-only | No | No | No |
| Discrete | XFORMER-only | No | No | No |
| Indexed | XFORMER-only | No | No | No |
| T9type (Teletype) | XFORMER-only | No | No | No |

### Key Architecture Differences
- **8 tracks** (CONFIG_TRACK_COUNT=8) vs Modulove's 16-track dual bank
- **No built-in LFO modulators** — no ModulatorEngine exists in codebase
- **No microtiming recording** — no microtiming fields in NoteSequence::Step
- **Unique track types** — Tuesday, Discrete, Indexed, Teletype have no counterpart in other forks
- **No Logic/Stochastic/Arp tracks** — XFORMER does not have these track types
- **Unique Harmony system** — NoteSequence has HarmonyRole, InversionOverride, VoicingOverride not found in other forks

---

## VinxScorza Improvements

### 1. Core Sequencing Stabilization
**Source:** `temp-ref/vinx-performer/src/apps/sequencer/engine/`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Step engine stabilization (all tracks) | High | Extend existing NoteTrackEngine, CurveTrackEngine |
| Crash path fixes | High | Add to all 7 track types |
| Non-zero subrange shifting | High | Enhance existing step selection in NoteSequence |
| Curve Gate Offset/Gate Length | Already present | No action needed |
| Curve undo restoration | High | Add undo state to CurveTrackEngine |
| Scale/MIDI capture consistency | High | Improve existing StepRecorder |
| Chaos → Vandalize + Wreck with A/B safe | Medium | Requires generator page refactor |
| Random gen preview/apply | Medium | Extend RandomizePage with A/B preview |

**Files to modify:**
- `src/apps/sequencer/engine/NoteTrackEngine.h`
- `src/apps/sequencer/engine/CurveTrackEngine.h`
- `src/apps/sequencer/engine/StepRecorder.h`
- `src/apps/sequencer/ui/page/RandomizePage.h`
- `src/apps/sequencer/ui/page/GeneratorPage.h`

### 2. Generator Preview/Apply Workflow
**Source:** `temp-ref/vinx-performer/src/apps/sequencer/ui/page/GeneratorPage.h`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Preview/apply flow for all generators | Medium | Add A/B preview state to GeneratorPage base class |
| ORIGINAL state entry with explicit first reroll | Medium | Add state machine to generator workflow |
| Safer commit/cancel flow | Medium | Extend context menu commit logic |
| Uniform footer/context layouts | Low | Cosmetic — lower priority |

**Relation to `launchpad-track-port`:** The core A/B preview state machine and GeneratorPage refactor live here. The Launchpad grid trigger for generators (Generators Mode) lives in `launchpad-track-port`.

**Files to modify:**
- `src/apps/sequencer/ui/page/GeneratorPage.h`
- `src/apps/sequencer/ui/page/GeneratorPage.cpp`

### 3. UI & Display Improvements
**Source:** `temp-ref/vinx-performer/src/apps/sequencer/ui/`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| 64-step context + 16-step bank visualization | Medium | Enhance step multiplex rendering in SequencerPage |
| Layer graphics 30fps skip unchanged frames | Medium | Add dirty-rect tracking to Canvas |
| Menu wrap | High | Simple modulo in UI navigation code |
| Screensaver/wake refinement | High | Extend existing screensaver code |

**Files to modify:**
- `src/apps/sequencer/ui/page/SequencerPage.h`
- `src/apps/sequencer/ui/page/SequencerPage.cpp`
- `src/apps/sequencer/ui/Canvas.h`

### 4. System & Defaults
**Source:** `temp-ref/vinx-performer/src/apps/sequencer/model/UserSettings.h`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Harden system save flow | High | Add checksum + atomic write to FileManager |
| Chaos defaults persistence | High | Add chaos defaults to UserSettings |
| Save prompts for unsaved changes | High | Add dirty-flag check on project switch |
| Memory optimization | Medium | Already have AGENTS.md analysis on this |

---

## Modulove Improvements

### 1. 8 LFO Modulators
**Source:** `temp-ref/modulove-performer/src/apps/sequencer/model/Modulator.h`

| Requirement | Feasibility | Effort |
|-------------|------------|--------|
| New Modulator data model | High | 1 file |
| New ModulatorEngine | Medium | shares engine tick |
| Waveform preview (oscilloscope) | Medium | needs Canvas rendering |
| Quick-map popup for routing | Medium | extends routing UI |
| Triplet/dotted note divisions | High | simple divisor addition |

**Files to create:**
- `src/apps/sequencer/model/Modulator.h`
- `src/apps/sequencer/engine/ModulatorEngine.h`
- `src/apps/sequencer/ui/page/ModulatorPage.h`

### 2. Microtiming Recording
**Source:** `temp-ref/modulove-performer/src/apps/sequencer/model/NoteSequence.h`

| Requirement | Feasibility | Effort |
|-------------|------------|--------|
| 7-bit gate offset (-63 to +63) | High | Extend NoteSequence::Step bitfield |
| Capture timing during live record | High | Extend StepRecorder |
| Timing quantize (0-100%) | High | Add quantize logic in engine |
| Bidirectional timing (early/late) | High | Simple signed arithmetic |

**Files to modify:**
- `src/apps/sequencer/model/NoteSequence.h` — add offset field (7 bits)
- `src/apps/sequencer/engine/StepRecorder.h` — capture timing
- `src/apps/sequencer/engine/NoteTrackEngine.h` — apply offset during playback

### 3. Enhanced Performer Page
**Source:** `temp-ref/modulove-performer/src/apps/sequencer/ui/page/PerformerPage.h`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Green/Red/Yellow LED coding | High | Extend LedPainter |
| Dimmed pattern numbers for muted | High | Add alpha/grey in canvas |
| All-tracks view for 8 tracks | High | Already have 8, just enhance |
| Consistent LED across banks | N/A | No bank system in XFORMER |

**Relation to `launchpad-track-port`:** This is about on-device OLED/LED rendering (PerformerPage). The Launchpad-based Performer Mode (scene mute/solo/fill via Launchpad grid) lives in `launchpad-track-port`. They are independent.

---

## Mebitek Improvements

### 1. Track Features
**Source:** `temp-ref/mebitek-performer/src/apps/sequencer/`

| Improvement | Feasibility | Notes |
|-------------|------------|-------|
| Arpeggiator Track | Low | XFORMER has Tuesday instead — no Arp track type |
| Stochastic Track enhancements | N/A | XFORMER has no Stochastic track |
| Logic Track per-step operators | N/A | XFORMER has no Logic track |
| Curve CV controllable min/max | Already present | No action needed |

### 2. Performance Features

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Quick octave change (Step+F1-F5) | High | Add page-level shortcut handler |
| Quick gate accent on Launchpad | High | Lives in `launchpad-track-port` (LP work) |
| Steps to stop feature | High | Add to Project properties + ClockEngine |

**Files to modify:**
- `src/apps/sequencer/model/Project.h` — add stepsToStop field
- `src/apps/sequencer/engine/ClockEngine.h` — check stepsToStop

### 3. UI Shortcuts

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Submenu shortcuts (double-click F1-F5) | High | Add double-click detection to key handler |
| Smart cycling on pattern follow | High | Add Launchpad-connected check |
| Context menu via double-click page | High | Add timer-based key event detection |

### 4. Technical Improvements

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Undo function | Partially present | Enhance existing undo system |
| Curve backward playback | High | Add run mode to CurveTrackEngine |
| Bypass voltage table per step | Already present | NoteSequence already has this |
| Prevent short clock pulses | High | Add min pulse width to ClockEngine |

**Relation to `launchpad-track-port`:** The core undo/redo state management lives here. The Launchpad Shift+Play undo shortcut lives in `launchpad-track-port`.

---

## Implementation Plan (Prioritized)

### Phase 1: High Priority
1. **Microtiming recording** — NoteSequence step offset field + StepRecorder capture
2. **Enhanced Performer Page** — LED color coding, muted display
3. **Quick octave change** — Page shortcut handler
4. **Steps to stop** — Project + ClockEngine
5. **Menu wrap** — UI navigation modulo

### Phase 2: Medium Priority
1. **Generator preview/apply workflow** — GeneratorPage refactor (core state machine; LP trigger in `launchpad-track-port`)
2. **Curve undo restoration** — CurveTrackEngine undo state
3. **Submenu shortcuts** — Double-click detection
4. **Screensaver/wake refinement** — Existing screensaver improvements
5. **Curve backward playback** — Run mode in CurveTrackEngine

### Phase 3: Low Priority
1. **Chaos generators (Vandalize + Wreck)** — Generator page refactor
2. **Random generator preview/apply** — RandomizePage enhancements
3. **LFO modulators** — New ModulatorEngine + ModulatorPage
4. **64-step context visualization** — Step multiplex rendering
5. **Prevent short clock pulses** — ClockEngine min pulse width

### Phase 4: Future
1. **Arpeggiator track** — Evaluate vs Tuesday approach

### Items Not in Phases (System & Defaults)
- Harden system save flow (checksum + atomic write)
- Chaos defaults persistence
- Save prompts for unsaved changes
- Memory optimization
- Step engine stabilization (all tracks)
- Crash path fixes
- Non-zero subrange shifting
- Scale/MIDI capture consistency
- Smart cycling on pattern follow
- Context menu via double-click page
- 30fps dirty-rect tracking

## Success Criteria

- **Phase 1**: Microtiming working, performer page enhanced, shortcuts functional
- **Phase 2**: Undo restored, generators have preview/apply, UI responsiveness improved
- **Phase 3**: Advanced generators working, LFO modulators operational (if done)
- **Phase 4**: Research outputs only — no commitment

## Status
Paused

## Priority
High

## Dependencies

| Task | Relationship |
|------|-------------|
| `launchpad-track-port` | **Independent.** Shared features (Generators, Undo, Performer Page) have core logic here, triggers there. Can implement in either order. |

## Current Codebase Reference Files

- `src/apps/sequencer/Config.h` — CONFIG_TRACK_COUNT=8, CONFIG_PPQN=192
- `src/apps/sequencer/model/Track.h` — 7 track types
- `src/apps/sequencer/model/Project.h` — project structure, tracks array
- `src/apps/sequencer/model/NoteSequence.h` — step bitfields, harmony system
- `src/apps/sequencer/model/CurveSequence.h` — curve sequence layers
- `src/apps/sequencer/engine/NoteTrackEngine.h` — note track engine
- `src/apps/sequencer/engine/CurveTrackEngine.h` — curve track engine
- `src/apps/sequencer/engine/StepRecorder.h` — step recording with timing
- `src/apps/sequencer/engine/ClockEngine.h` — clock/reset/stop logic
- `src/apps/sequencer/ui/page/PerformerPage.h` — performer page
- `src/apps/sequencer/ui/page/GeneratorPage.h` — generator page base
