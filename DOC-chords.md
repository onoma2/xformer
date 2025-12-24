Generator blueprint that keeps your two voicing banks exactly as written, and makes them work for any scale without per‑scale tables.

  1) Data model (canonical semitone lists)

  - Two banks: PianoVoicings[], GuitarVoicings[]
  - Each entry: {name, semis[], count} where semis are your offsets.
  - Keep the lists exactly as provided (no scale baked in).

  2) Runtime mapping to scale degrees
  For each semitone offset s:

  volts     = s / 12.0f
  degree    = scale.noteFromVolts(volts)   // quantize to scale
  noteIndex = rootIndex + degree

  This works for chromatic and any other scale.

  3) Preserve voicing size (avoid collapsing)
  If you want the voicing to keep all notes even when scale quantizes to duplicates:

  for each noteIndex:
    while noteIndex <= prevNote:
        noteIndex += scale.notesPerOctave()

  This keeps the list strictly ascending and preserves count.

  4) Apply to steps

  - Use current root from UI (step note or track root).
  - Write the computed noteIndex list to selected steps (in order).
  - Clamp to note range if needed (-63..64).

  5) Generator UI

  - Bank: Piano / Guitar
  - Voicing: list of names
  - Option: Quantize (strict) vs Preserve (bump duplicates)

  Pseudocode

  struct Voicing { const char* name; int8_t semis[6]; uint8_t count; };

  vector<int> buildVoicingNotes(const Voicing& v, int rootIndex,
                                const Scale& scale, bool preserve) {
    vector<int> out;
    int prev = -999;
    for i in 0..v.count-1:
      float volts = v.semis[i] / 12.0f;
      int degree = scale.noteFromVolts(volts);
      int note = rootIndex + degree;
      if (preserve) {
        while (note <= prev) note += scale.notesPerOctave();
      }
      out.push_back(note);
      prev = note;
    }
    return out;
  }


## Piano Voicing
  Maj13   [0,4,7,11,14,21]
  Maj6/9  [0,4,7,9,14]

  Min13   [0,3,7,10,14,21]
  Min6/9  [0,3,7,9,14]
  MinMaj9 [0,3,7,11,14]

  Dom13   [0,4,7,10,14,21]
  m9b5    [0,3,6,10,14]
  Dim7    [0,3,6,9]

  Aug9    [0,4,8,10,14]
  AugMaj9 [0,4,8,11,14]

  Sus2(9)   [0,2,7,10,14]
  Sus4(11)  [0,5,7,10,14,17]

## Guitar voicing
  Maj (C‑shape, root 5th)     [0,4,7,12,16]
  Min (E‑shape, root 6th)     [0,7,12,15,19]
  7   (C‑shape, root 5th)     [0,4,10,12,16]
  Maj7 (A‑shape, root 5th)    [0,7,11,16,19]
  Min7 (A‑shape, root 5th)    [0,7,10,15,19]
  6   (C‑shape, root 5th)     [0,4,9,12,16]
  Min6 (E‑shape, root 6th)    [0,7,12,15,21]
  9   (E‑shape, root 6th)     [0,7,10,16,26]
  13  (E‑shape, root 6th)     [0,7,10,16,21]
  Sus2 (A‑shape, root 5th)    [0,7,12,14,19]
  Sus4 (E‑shape, root 6th)    [0,7,12,17,19]
  Add9 (C‑shape, root 5th)    [0,4,7,14,19]
  Aug  (E‑shape, root 6th)    [0,8,12,16,20]
  m7b5 (E‑shape, root 6th)    [0,6,10,15,22]
  Dim7 (E‑shape, root 6th)    [0,6,12,15,21]


