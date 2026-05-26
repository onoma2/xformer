# Graph Report - .graphify-teletype-scope  (2026-05-26)

## Corpus Check
- Corpus is ~19,547 words - fits in a single context window. You may not need a graph.

## Summary
- 335 nodes · 593 edges · 22 communities (19 shown, 3 thin omitted)
- Extraction: 98% EXTRACTED · 2% INFERRED · 0% AMBIGUOUS · INFERRED: 13 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Bridge Handlers|Bridge Handlers]]
- [[_COMMUNITY_Script View Page|Script View Page]]
- [[_COMMUNITY_Bridge UIDashboard|Bridge UI/Dashboard]]
- [[_COMMUNITY_Track Model Data|Track Model Data]]
- [[_COMMUNITY_Pattern View Page|Pattern View Page]]
- [[_COMMUNITY_TrackEngine Ticks|TrackEngine Ticks]]
- [[_COMMUNITY_Track List Model|Track List Model]]
- [[_COMMUNITY_Note Integration|Note Integration]]
- [[_COMMUNITY_Subcommunity 10|Subcommunity 10]]
- [[_COMMUNITY_Subcommunity 11|Subcommunity 11]]
- [[_COMMUNITY_Subcommunity 12|Subcommunity 12]]
- [[_COMMUNITY_Subcommunity 13|Subcommunity 13]]
- [[_COMMUNITY_Subcommunity 14|Subcommunity 14]]
- [[_COMMUNITY_Subcommunity 15|Subcommunity 15]]
- [[_COMMUNITY_Subcommunity 16|Subcommunity 16]]
- [[_COMMUNITY_Subcommunity 18|Subcommunity 18]]
- [[_COMMUNITY_Subcommunity 19|Subcommunity 19]]
- [[_COMMUNITY_Subcommunity 20|Subcommunity 20]]

## God Nodes (most connected - your core abstractions)
1. `activeEngine()` - 77 edges
2. `keyPress()` - 17 edges
3. `keyboard()` - 13 edges
4. `showMessage()` - 12 edges
5. `keyPress()` - 12 edges
6. `keyboard()` - 12 edges
7. `update()` - 11 edges
8. `loadEditBuffer()` - 10 edges
9. `handleCv()` - 8 edges
10. `syncPattern()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `keyboard()` --calls--> `showMessage()`  [INFERRED]
  /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp → /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/engine/TeletypeTrackEngine.cpp
- `read()` --calls--> `CvInputSource()`  [INFERRED]
  /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/model/TeletypeTrack.cpp → /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/model/TeletypeTrack.h
- `draw()` --calls--> `get_dashboard_value()`  [INFERRED]
  /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp → /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/engine/TeletypeBridge.cpp
- `commitLine()` --calls--> `showMessage()`  [INFERRED]
  /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp → /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/engine/TeletypeTrackEngine.cpp
- `copyLine()` --calls--> `showMessage()`  [INFERRED]
  /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp → /Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/engine/TeletypeTrackEngine.cpp

## Communities (22 total, 3 thin omitted)

### Community 1 - "Bridge Handlers"
Cohesion: 0.04
Nodes (49): activeEngine(), tele_bar(), tele_cv_off(), tele_cv_slew(), tele_env_decay(), tele_env_eor(), tele_env_loop(), tele_env_target() (+41 more)

### Community 2 - "Script View Page"
Cohesion: 0.12
Nodes (42): get_dashboard_value(), showMessage(), backspace(), clampTextToWidth(), commentLine(), commitLine(), commitLineAndAdvance(), contextAction() (+34 more)

### Community 3 - "Bridge UI/Dashboard"
Cohesion: 0.04
Nodes (22): tele_cv(), tele_env_attack(), tele_env_eoc(), tele_env_offset(), tele_env_trigger(), tele_g_bar(), tele_g_get_tune_den(), tele_g_get_val() (+14 more)

### Community 4 - "Track Model Data"
Cohesion: 0.13
Nodes (14): applyActivePatternSlot(), applyPatternSlot(), captureActiveClip(), clear(), clearClipForPerformerPattern(), copyClipForPerformerPattern(), CvInputSource(), loadActiveClipIntoVm() (+6 more)

### Community 5 - "Pattern View Page"
Cohesion: 0.21
Nodes (18): backspaceDigit(), commitEdit(), deleteRow(), encoder(), ensureRowVisible(), insertDigit(), insertRow(), keyboard() (+10 more)

### Community 6 - "TrackEngine Ticks"
Cohesion: 0.14
Nodes (18): advanceClockTime(), advanceTime(), applyCvQuantize(), beginPulse(), clearPulse(), handleCv(), handleTr(), panic() (+10 more)

### Community 7 - "Track List Model"
Cohesion: 0.33
Nodes (7): cell(), edit(), editValue(), formatName(), formatValue(), itemForRow(), itemName()

### Community 9 - "Note Integration"
Cohesion: 0.29
Nodes (7): noteGateGet(), noteGateHere(), noteGateSet(), noteNoteGet(), noteNoteHere(), noteNoteSet(), noteSequenceForTrack()

### Community 10 - "Subcommunity 10"
Cohesion: 0.29
Nodes (6): applyScriptLine(), installBootScript(), loadScriptsFromModel(), reset(), seedScriptsFromPresets(), TeletypeTrackEngine()

### Community 11 - "Subcommunity 11"
Cohesion: 0.33
Nodes (6): runBootScript(), runBootScriptNow(), setMetroActive(), setMetroPeriod(), syncMetroFromState(), tick()

### Community 12 - "Subcommunity 12"
Cohesion: 0.47
Nodes (6): measureFraction(), measureFractionBars(), normalizeBipolar(), normalizeUnipolar(), triggerGeodeVoice(), updateGeode()

### Community 13 - "Subcommunity 13"
Cohesion: 0.4
Nodes (5): inputState(), runScript(), triggerManualScript(), triggerScript(), updateInputTriggers()

### Community 14 - "Subcommunity 14"
Cohesion: 0.5
Nodes (4): clearMidiMonitoring(), processMidiMessage(), receiveMidi(), runMidiTriggeredScript()

### Community 15 - "Subcommunity 15"
Cohesion: 0.67
Nodes (3): tele_clock_mode_notice(), tele_get_ticks(), ticksMs()

### Community 16 - "Subcommunity 16"
Cohesion: 0.67
Nodes (3): beatMsForActiveEngine(), tele_wms(), tele_wtu()

## Knowledge Gaps
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `showMessage()` connect `Script View Page` to `TrackEngine Parameters`, `Pattern View Page`?**
  _High betweenness centrality (0.420) - this node is a cross-community bridge._
- **Why does `get_dashboard_value()` connect `Script View Page` to `Bridge UI/Dashboard`?**
  _High betweenness centrality (0.347) - this node is a cross-community bridge._
- **Are the 11 inferred relationships involving `showMessage()` (e.g. with `commitLine()` and `copyLine()`) actually correct?**
  _`showMessage()` has 11 INFERRED edges - model-reasoned connections that need verification._
- **Should `TrackEngine Parameters` be split into smaller, more focused modules?**
  _Cohesion score 0.03 - nodes in this community are weakly interconnected._
- **Should `Bridge Handlers` be split into smaller, more focused modules?**
  _Cohesion score 0.04 - nodes in this community are weakly interconnected._
- **Should `Script View Page` be split into smaller, more focused modules?**
  _Cohesion score 0.12 - nodes in this community are weakly interconnected._
- **Should `Bridge UI/Dashboard` be split into smaller, more focused modules?**
  _Cohesion score 0.04 - nodes in this community are weakly interconnected._