# Harmony Feature - Hardware Testing Guide

Complete testing instructions for the Harmonàig-style harmony sequencing feature on PEW|FORMER hardware.

## Prerequisites

- PEW|FORMER hardware with harmony firmware flashed
- At least 3 CV outputs connected to oscillators
- Clock source (internal or external)
- Basic familiarity with PER|FORMER sequencer operation

## Navigation

**Access Harmony Page:**
1. Select a Note track (press T1-T8)
2. Press **Sequence** button (S2) repeatedly to cycle views:
   - 1st press: NoteSequence page
   - 2nd press: Accumulator page (ACCUM)
   - 3rd press: **HARMONY page** ← You're here!
   - 4th press: Cycles back to NoteSequence

**Harmony Parameters:**
- **ROLE**: Track's harmony role (OFF/MASTER/ROOT/3RD/5TH/7TH)
- **MASTER**: Which track to follow (T1-T8)
- **SCALE**: Harmony mode (IONIAN/DORIAN/PHRYGN/LYDIAN/MIXOLY/AEOLIN/LOCRIN)

---

## Test 1: Basic 3-Note Chord (Root + 3rd + 5th)

**Goal:** Create a major triad following a master melody.

### Setup

**Track 1 - Master Melody:**
1. Press **T1** to select Track 1
2. Press **S2** three times → Harmony page
3. Set parameters:
   - ROLE: `MASTER`
   - MASTER: `T1` (doesn't matter for master)
   - SCALE: `IONIAN`
4. Press **S2** once → NoteSequence page
5. Add a simple melody:
   - Enable steps: S1, S5, S9, S13
   - Set notes (press Note button F3, then adjust with encoder):
     - S1: C (0)
     - S5: D (2)
     - S9: E (4)
     - S13: G (7)

**Track 2 - Root Follower:**
1. Press **T2** to select Track 2
2. Press **S2** three times → Harmony page
3. Set parameters:
   - ROLE: `ROOT`
   - MASTER: `T1`
   - SCALE: `IONIAN`

**Track 3 - Third Follower:**
1. Press **T3** to select Track 3
2. Press **S2** three times → Harmony page
3. Set parameters:
   - ROLE: `3RD`
   - MASTER: `T1`
   - SCALE: `IONIAN`

**Track 4 - Fifth Follower:**
1. Press **T4** to select Track 4
2. Press **S2** three times → Harmony page
3. Set parameters:
   - ROLE: `5TH`
   - MASTER: `T1`
   - SCALE: `IONIAN`

### Test

1. Press **PLAY**
2. **Expected Result:**
   - Track 1 plays: C → D → E → G (master melody)
   - Track 2 plays: C → D → E → G (root = same as master)
   - Track 3 plays: E → F → G → B (thirds in C major)
   - Track 4 plays: G → A → B → D (fifths in C major)

3. **Listen for:** Major triads harmonizing the melody

### Verification

- All three voices should move together
- Each step should produce a consonant C major chord
- S1: C-E-G (C major)
- S5: D-F-A (D minor)
- S9: E-G-B (E minor)
- S13: G-B-D (G major)

---

## Test 2: Full 4-Note Chord (Root + 3rd + 5th + 7th)

**Goal:** Create jazz-style 7th chords.

### Setup

Use the same Track 1-4 setup from Test 1, but add:

**Track 5 - Seventh Follower:**
1. Press **T5** to select Track 5
2. Press **S2** three times → Harmony page
3. Set parameters:
   - ROLE: `7TH`
   - MASTER: `T1`
   - SCALE: `IONIAN`

### Test

1. Press **PLAY**
2. **Expected Result:**
   - Track 1: C → D → E → G (master)
   - Track 2: C → D → E → G (root)
   - Track 3: E → F → G → B (3rd)
   - Track 4: G → A → B → D (5th)
   - Track 5: B → C → D → F (7th)

3. **Listen for:** Rich 7th chord voicings

### Verification

- S1: C-E-G-B (Cmaj7)
- S5: D-F-A-C (Dm7)
- S9: E-G-B-D (Em7)
- S13: G-B-D-F (G7)

---

## Test 3: Multiple Harmony Groups (2 Independent Chord Sets)

**Goal:** Two separate master tracks, each with their own followers.

### Setup

**Group 1: Tracks 1-3 (Bass + Harmony)**

**Track 1 - Bass Master:**
1. Press **T1**
2. Harmony page → Set:
   - ROLE: `MASTER`
   - SCALE: `DORIAN`
3. NoteSequence → Add bass line:
   - S1: D (2)
   - S5: D (2)
   - S9: E (4)
   - S13: C (0)

**Track 2 - Bass Third:**
1. Press **T2**
2. Harmony page → Set:
   - ROLE: `3RD`
   - MASTER: `T1`
   - SCALE: `DORIAN`

**Track 3 - Bass Fifth:**
1. Press **T3**
2. Harmony page → Set:
   - ROLE: `5TH`
   - MASTER: `T1`
   - SCALE: `DORIAN`

**Group 2: Tracks 5-7 (Lead + Harmony)**

**Track 5 - Lead Master:**
1. Press **T5**
2. Harmony page → Set:
   - ROLE: `MASTER`
   - SCALE: `LYDIAN`
3. NoteSequence → Add melody:
   - S1: F (5)
   - S3: G (7)
   - S5: A (9)
   - S7: B (11)
   - S9: C (0)
   - S11: D (2)
   - S13: E (4)
   - S15: F (5)

**Track 6 - Lead Third:**
1. Press **T6**
2. Harmony page → Set:
   - ROLE: `3RD`
   - MASTER: `T5`
   - SCALE: `LYDIAN`

**Track 7 - Lead Fifth:**
1. Press **T7**
2. Harmony page → Set:
   - ROLE: `5TH`
   - MASTER: `T5`
   - SCALE: `LYDIAN`

### Test

1. Press **PLAY**
2. **Expected Result:**
   - Tracks 1-3 play harmonized bass line in D Dorian
   - Tracks 5-7 play harmonized lead line in F Lydian
   - Both groups operate independently
   - Different modal colors (Dorian vs Lydian)

### Verification

- Bass group sounds darker (Dorian minor quality)
- Lead group sounds brighter (Lydian major quality)
- Groups don't interfere with each other

---

## Test 4: Different Master Track Sources

**Goal:** Test followers tracking different master tracks.

### Setup

**Track 1 - Master A:**
1. ROLE: `MASTER`, SCALE: `IONIAN`
2. Pattern: C-E-G-C (whole notes, S1, S5, S9, S13)

**Track 2 - Master B:**
1. ROLE: `MASTER`, SCALE: `MIXOLYDIAN`
2. Pattern: G-A-B-D (whole notes, S1, S5, S9, S13)

**Track 3 - Follows Master A (T1):**
1. ROLE: `3RD`, MASTER: `T1`, SCALE: `IONIAN`

**Track 4 - Follows Master B (T2):**
1. ROLE: `3RD`, MASTER: `T2`, SCALE: `MIXOLYDIAN`

**Track 5 - Follows Master A (T1):**
1. ROLE: `5TH`, MASTER: `T1`, SCALE: `IONIAN`

**Track 6 - Follows Master B (T2):**
1. ROLE: `5TH`, MASTER: `T2`, SCALE: `MIXOLYDIAN`

### Test

1. Press **PLAY**
2. **Expected Result:**
   - Tracks 1, 3, 5 form C major chords (Ionian)
   - Tracks 2, 4, 6 form G mixolydian chords
   - Clear distinction between the two harmonic centers

---

## Test 5: Modal Exploration - All 7 Modes

**Goal:** Hear the difference between all 7 modal scales.

### Setup Template

**Master Track (T1):**
- ROLE: `MASTER`
- Pattern: Scale degrees 1-2-3-4-5-6-7-1 (all 8 steps)

**Follower Tracks:**
- T2: ROOT
- T3: 3RD
- T4: 5TH
- T5: 7TH

### Test Each Mode

Run this sequence 7 times, changing SCALE on all tracks each time:

1. **IONIAN** (Major) - Bright, happy
   - Master & followers: SCALE = `IONIAN`
   - Expected: Major 7 chords, uplifting sound

2. **DORIAN** (Minor with raised 6th) - Jazz minor
   - Master & followers: SCALE = `DORIAN`
   - Expected: Minor 7 chords, sophisticated

3. **PHRYGN** (Phrygian - Minor with b2) - Spanish/Dark
   - Master & followers: SCALE = `PHRYGN`
   - Expected: Minor 7 chords, exotic flavor

4. **LYDIAN** (Major with #4) - Dreamy
   - Master & followers: SCALE = `LYDIAN`
   - Expected: Major 7#11 chords, floating quality

5. **MIXOLY** (Mixolydian - Major with b7) - Dominant
   - Master & followers: SCALE = `MIXOLY`
   - Expected: Dominant 7 chords, bluesy

6. **AEOLIN** (Aeolian - Natural Minor)
   - Master & followers: SCALE = `AEOLIN`
   - Expected: Minor 7 chords, melancholic

7. **LOCRIN** (Locrian - Diminished)
   - Master & followers: SCALE = `LOCRIN`
   - Expected: Half-diminished chords, unstable/tense

---

## Test 6: Mixed Modes (Advanced)

**Goal:** Followers use different modes than master.

### Setup

**Track 1 - Master:**
- ROLE: `MASTER`, SCALE: `IONIAN`
- Pattern: C-D-E-F (S1-S4)

**Track 2 - Root in Dorian:**
- ROLE: `ROOT`, MASTER: `T1`, SCALE: `DORIAN`

**Track 3 - Third in Lydian:**
- ROLE: `3RD`, MASTER: `T1`, SCALE: `LYDIAN`

**Track 4 - Fifth in Mixolydian:**
- ROLE: `5TH`, MASTER: `T1`, SCALE: `MIXOLY`

### Test

1. Press **PLAY**
2. **Expected Result:**
   - Each follower interprets the master melody through a different modal lens
   - Creates complex, colorful harmonies
   - May sound dissonant (intentional - modal mixture)

---

## Test 7: Master Track Switching

**Goal:** Verify MASTER parameter correctly selects different tracks.

### Setup

**Create 8 different master tracks (T1-T8):**
- Each plays a different note (C, D, E, F, G, A, B, C)
- All set to ROLE: `MASTER`, SCALE: `IONIAN`

**Test track (use Track 1 after testing):**
1. Pick any unused track or reconfigure T1
2. Set ROLE: `3RD`, SCALE: `IONIAN`

### Test

1. Set MASTER: `T1`, press PLAY → Should harmonize T1's note
2. Set MASTER: `T2`, press PLAY → Should harmonize T2's note
3. Continue for T3-T8
4. **Expected:** Follower always harmonizes the currently selected master

---

## Test 8: Step Synchronization

**Goal:** Verify followers track master at the same step index.

### Setup

**Track 1 - Master:**
- ROLE: `MASTER`, SCALE: `IONIAN`
- Pattern with varying rhythm:
  - S1-S4: C (all gates on)
  - S5-S8: E (all gates on)
  - S9-S12: G (all gates on)
  - S13-S16: C (all gates on)

**Track 2 - Follower:**
- ROLE: `3RD`, MASTER: `T1`, SCALE: `IONIAN`
- Gates: Only S1, S5, S9, S13 enabled

### Test

1. Press **PLAY**
2. **Expected Result:**
   - Follower should play harmonized notes at steps 1, 5, 9, 13
   - Even though master plays continuously, follower only fires on its enabled gates
   - Harmony notes should match the master's note at the SAME step index

---

## Troubleshooting

### No harmony output
- Check ROLE is not `OFF`
- Verify MASTER points to correct track
- Ensure master track has notes programmed
- Check CV outputs are routed correctly

### Wrong notes
- Verify SCALE setting matches desired mode
- Check master track note values
- Confirm MASTER parameter points to intended track

### All followers play same note
- This is correct for ROOT followers
- Use 3RD, 5TH, 7TH for harmonic intervals

### Dissonant/unexpected harmony
- Check if master and followers use compatible SCALE settings
- Try setting all to IONIAN first (most consonant)
- Verify master track is set to ROLE: `MASTER`

---

## Musical Applications

### Instant Chord Pads
- 1 master + 3 followers (ROOT/3RD/5TH) = instant chords
- Program master melody, get full harmonization

### Dual Chord Progressions
- Tracks 1-4: Bass harmony group
- Tracks 5-8: Lead harmony group
- Independent progressions, rich texture

### Modal Interchange
- Switch SCALE parameter in real-time
- Same melody, different harmonic colors
- Great for live performance variation

### Jazz Voicings
- Use all 4 voices (ROOT/3RD/5TH/7TH)
- Set SCALE to DORIAN or MIXOLY
- Instant jazz chord progressions

---

## Advanced Tips

1. **Octave Separation:**
   - Set master to low octave
   - Set 3RD/5TH followers to higher octaves
   - Creates wide voicings

2. **Rhythmic Independence:**
   - Master plays sustained notes
   - Followers have syncopated gate patterns
   - Harmony follows pitch, rhythm independent

3. **Pattern Variations:**
   - Master uses Pattern 0
   - Followers can use Pattern 0 or 1
   - If follower uses Pattern 1, it harmonizes Pattern 0 (master pattern)

4. **Accumulator + Harmony:**
   - Enable accumulator on follower tracks
   - Harmony provides base pitch
   - Accumulator adds modulation
   - Creates evolving harmonic movement

---

## Expected Behavior Summary

- **Synchronized Playback:** Followers harmonize master at the same step index
- **Independent Gates:** Follower gates control when harmony fires
- **Modal Flexibility:** Each track can use different SCALE setting
- **Multiple Masters:** Up to 8 independent master tracks possible
- **Real-time Updates:** Changing SCALE or MASTER takes effect immediately

Enjoy creating complex harmonic sequences! 🎵
