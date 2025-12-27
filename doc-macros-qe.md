# Macros + Quick Edit Buttons (Page + Key::Step)

Only buttons pressed with the Page modifier are listed here.

## Curve

Macros
- Page + `Key::Step4` → Macro menu (`MM-INIT`, `M-FM`, `M-DAMP`, `M-BOUNCE`, `M-RSTR`)
- Page + `Key::Step5` → LFO menu (`TRI`, `SINE`, `SAW`, `SQUA`, `MM-RND`)
- Page + `Key::Step6` → Transform menu (`T-INV`, `T-REV`, `T-MIRR`, `T-ALGN`, `T-WALK`)
- Page + `Key::Step14` → Gate Presets (`ZC+`, `EOC/EOR`, `RISING`, `FALLING`, `>50%`)

Files
- `src/apps/sequencer/ui/pages/CurveSequencePage.cpp`
- `src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp`
- `src/apps/sequencer/model/CurveSequence.cpp`

Quick edits
- None (Curve uses macro menus instead of Page+Step quick edits)

## Indexed

Macros
- Page + `Key::Step4` → Rhythm menu (`EUCLIDEAN`, `CLAVE`, `TUPLET`, `POLY`, `M-RHY`)
- Page + `Key::Step5` → Waveform menu (`TRI`, `SINE`, `SAW`, `PULSE`, `TARGET`)
- Page + `Key::Step6` → Melodic menu (`SCALE`, `ARP`, `CHORD`, `MODAL`, `M-MEL`)
- Page + `Key::Step14` → Duration/Transform menu (`D-LOG`, `D-EXP`, `D-TRI`, `REV`, `MIRR`)

Files
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp`

Quick edits
- Page + `Key::Step9` → Split
- Page + `Key::Step10` → Swap
- Page + `Key::Step11` → Merge
- Page + `Key::Step12` → Set First
- Page + `Key::Step13` → Length (list item)
- Page + `Key::Step14` → Run Mode (list item)
- Page + `Key::Step15` → Reset Measure (list item)

Files
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp`
- `src/apps/sequencer/ui/model/IndexedSequenceListModel.h`

## DiscreteMap

Macros
- Page + `Key::Step4` → Distribution menu (`EVEN8`, `EVEN16`, `EVEN-ALL`, `EVEN-GRP`, `EVEN-INV`)
- Page + `Key::Step5` → Cluster menu (`M-CLUSTER`)
- Page + `Key::Step6` → Distribute Active menu (`ACTIVE`, `RISE`, `FALL`, `BOTH`, `NORMALIZE`)
- Page + `Key::Step14` → Transform menu (`DIR FLIP`, `T-REVERSE`, `T-INVERT`, `N-MIRROR`, `N-REVERSE`)

Files
- `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp`

Quick edits
- Page + `Key::Step9` → Range High
- Page + `Key::Step10` → Range Low
- Page + `Key::Step11` → Divisor
- Page + `Key::Step12` → Piano Voicing (hold+encoder)
- Page + `Key::Step13` → Guitar Voicing (hold+encoder)

Files
- `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp`
- `src/apps/sequencer/ui/model/DiscreteMapSequenceListModel.h`
