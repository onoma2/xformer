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
- Page + `Key::Step4` → Rhythm menu (`3/9`, `5/20`, `7/28`, `3-5/5-7`, `M-TALA`)
- Page + `Key::Step5` → Waveform menu (`TRI`, `2TRI`, `3TRI`, `5TRI`)
- Page + `Key::Step6` → Melodic menu (`SCALE`, `ARP`, `CHORD`, `MODAL`, `M-MEL`)
- Page + `Key::Step14` → Duration/Transform menu (`D-LOG`, `D-EXP`, `D-TRI`, `REV`, `MIRR`)

Files
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp`

Quick edits
- Page + `Key::Step8` → Split
- Page + `Key::Step9` → Merge
- Page + `Key::Step10` → Swap (hold+encoder)
- Page + `Key::Step11` → Run Mode (list item)
- Page + `Key::Step12` → Piano Voicing (hold+encoder)
- Page + `Key::Step13` → Guitar Voicing (hold+encoder)

Files
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp`
- `src/apps/sequencer/ui/model/IndexedSequenceListModel.h`

## DiscreteMap

Macros
- Page + `Key::Step4` → Distribution menu (`I-8`, `I-5(2)`, `I-7(3)`, `I-9(4)`, `I-FRET`)
- Page + `Key::Step5` → Cluster menu (`M-CLUSTER`, `M-AR4`, `M-SWELL`, `M-ISWELL`, `M-STRUM`)
- Page + `Key::Step6` → Distribute Active menu (`E-ACT`, `E-RISE`, `E-FALL`, `E-BOTH`, `NORM`)
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

## Future macros to think about

- M-KHAND: 5/7 alternating gap weights
- M-TIHAI: 2-1-2 phrase repeated 3x
- M-DHAMAR: 5-2-3-4 gap weights
