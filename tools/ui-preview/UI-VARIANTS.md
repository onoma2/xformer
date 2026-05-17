# Stochastic Track UI Variants

This document describes every preview image in `stochastic-previews/`.
Each entry explains **what the visual means** and **what information it carries**.

---

## How to read these previews

All images are 256×64 pixel OLED frames rendered at 4× scale. The font is the exact `tiny5x5` bitmap parsed live from the firmware headers. Every preview uses the same header/footer grammar (`WindowPainter`) so you can compare surfaces side-by-side without visual noise.

---

## Canvas Geometry

The Performer OLED is **256×64 px**. The `WindowPainter` reserves fixed bands at the top and bottom, leaving a safe content area in the middle.

### Header (y = 0 … 10)
| Element | Position | Size |
|---------|----------|------|
| Clock text | x=2, y=6 | Tiny font, ~5 px tall |
| Track / pattern badges | x=40, y=6 | Inverted text, 7 px tall box |
| Mode label | right-aligned, y=6 | Tiny font |
| Active function | x=100, y=6 | Tiny font |
| Accumulator | x=176, y=6 | Tiny font (optional) |
| **Separator line** | y = 10 | 1 px horizontal rule across 256 px |

**Header occupies 11 px** (y = 0 … 10 inclusive).

### Footer (y = 53 … 63)
| Element | Position | Size |
|---------|----------|------|
| **Separator line** | y = 53 | 1 px horizontal rule across 256 px |
| Function-key dividers | x = 51, 102, 153, 204 | 1 px vertical rules, 10 px tall |
| Function-key labels | y = 61 | Tiny font, centered in each 51 px column |

**Footer occupies 12 px** (y = 53 … 64 inclusive).

### Safe Content Area
```
y = 11  ← first usable pixel row
    …
y = 52  ← last usable pixel row
```
**Height = 42 px** (rows 11–52 inclusive).
**Width = 256 px** (full canvas width).

When designing new UI elements, draw only inside this 256×42 rectangle. Anything touching y ≤ 10 or y ≥ 53 will collide with the header/footer chrome.

---

## Baseline / Existing Pages

### `note-edit`
Standard Performer Note track step grid. 16 steps × 16 px cells. Gate square, note name below, probability bar under the gate when probability layers are active. This is the existing idiom every stochastic variant is measured against.

### `discrete-map-edit`
DiscreteMap threshold timeline. One horizontal bar with stage markers at threshold positions. Input cursor shows live CV position. This is the reference for fence/range visual language.

### `tuesday-edit`
Tuesday four-column parameter console. Value text + horizontal bar per parameter. Right-side live status box shows current note, gate state, step position. This is the reference for compact track-level surfaces.

### `stochastic-edit`
Legacy placeholder. Simple 16-step grid with gate probability shading (brightness by probability). Kept as the before-state.

---

## Primary Stochastic Pages (MVP design from UI-DESIGN.md)

### `stochastic-steps`
The default step-editing surface. 16-step grid with two probability bars under each cell:
- **Top bar** = gate probability (always visible, faint when low).
- **Bottom bar** = note probability.
- **Note name** printed below if gate is on.
This means you see gate likelihood and pitch likelihood simultaneously without switching layers. F5 `NEXT` cycles layer banks (gate/note/length/retrigger → probabilities → offsets/conditions).

### `stochastic-dice`
64-step probability matrix. 4 rows of 16 compressed cells. Each cell brightness = probability for the active axis (gate/note/length/retrigger/rest). Current playhead is a bright outline; selected cells are medium-bright. This means you see the entire probability landscape of one axis at once. It answers the question "where are my weak gates?" in one glance.

### `stochastic-pitch`
Scale-degree ticket field. One vertical bar per active scale degree (up to 32, demo shows 8). Bar height = ticket count.
- **X** = excluded degree (scale mask off).
- **Hollow bar** = included but zero tickets (will not be chosen).
- **Filled bar** = raffle tickets (higher = more likely).
Range fences mark min/max allowed degree. This means pitch probability is spatial: you see which degrees are loaded and which are locked out.

### `stochastic-marbles`
Distribution shape page. Left side = histogram preview with gaussian-ish envelope derived from spread/bias/steps. Right side = Tuesday-style live status box. Parameter values shown as horizontal bars. This means you edit abstract distribution parameters and immediately see the resulting probability field.

### `stochastic-lock`
Captured loop surface. 4 rows of 16 event cells. Each cell shows:
- Gate fill (bright = locked gate on, dim = locked gate off).
- Tiny vertical bar = captured note pitch.
- Dot = retrigger marker.
Window first/last drawn as bracket rectangles. This means captured performance is visually separated from source step data. You see what the locked loop actually played, not what the source steps say.

### `stochastic-track`
Compact track console (Tuesday-style). Four 51 px parameter columns with value text + bar. Right-side status box. Four pages of track-level parameters (octave/transpose/division → biases → accent/legato/fill → octave bounds). This means global track settings stay in a shallow, visual console instead of a long list model.

---

## Hybrid Crossover Pages

These pages take a stochastic track concept and render it through the visual language of an external reference app. They are design experiments, not final implementations.

---

### Steps Variants

#### `stochastic-steps-skylines`
**Inspiration:** *skylines* (M-185 city skyline bar chart).

What it means: Instead of a grid of squares, each step is a vertical bar. Height = gate probability.
- **Foreground bright bar** = gate probability.
- **Background dim bar** = note probability.
- **Note name** ticked at the top of tall bars.

This shows probability as immediately comparable vertical magnitude. You read the sequence like a skyline: tall buildings = certain gates, short buildings = rare gates. Two layers (gate + note) stacked as foreground/background keep the density high without clutter.

#### `stochastic-steps-delinquencer`
**Inspiration:** *delinquencer* (norns 8×8 probability grid).

What it means: 4 rows of 16 small cells (5×5 px). Each cell intensity maps to gate probability:
- **Bright fill** = 100%.
- **Medium fill** = 50–99%.
- **Dim fill** = 1–49%.
- **Empty frame** = 0%.

The delinquencer innovation: **corner-pixel erosion**. If probability is < 100%, 1–4 corner pixels are erased from the cell fill. Four erased corners = ~25%, one erased corner = ~75%. This means sub-certainty is visible as damage to the cell, not as a separate number. Current step gets a 2×2 dot; selected cell gets an outline frame.

#### `stochastic-steps-euclid`
**Inspiration:** *squares-and-circles* Euclidean ring.

What it means: 16 steps placed trigonometrically on a ring (radius 20 px). The square/circle binary encodes determinism:
- **Filled square** (3×3) = deterministic gate on (probability ≥ 75%).
- **Hollow circle** (3×3 frame) = probabilistic/pending (probability > 0 but < 75%).
- **Tiny dot** = off (probability 0%).

This means the step grid becomes a cycle. You read clockwise; the ring naturally represents looping. A second ring on the right shows note probability with the same encoding. The square/circle distinction is not decorative—it *is* the information architecture: squares hold (certain), circles suggest (possible).

---

### Pitch Variants

#### `stochastic-pitch-prob-melod`
**Inspiration:** *ProbabilityMelody* (O_C ProbMeloD).

What it means: Full 12-semitone keyboard at bottom (white key outlines, black key fills). Above each key: a vertical dotted line with a horizontal tick at the weight height.
- **Tick high** = many tickets.
- **No tick** = zero tickets.
- **X marker** = excluded from scale mask.

This means pitch probability is read as elevation. The keyboard is the legend—you need no labels. Range fences drawn as vertical lines at the min/max degree positions. This is the most literal chromatic view; it works best when the active scale is 12-degree or chromatic.

#### `stochastic-pitch-circle`
**Inspiration:** *Automatonnetz* / `visualize_pitch_classes` (O_C pitch-class circle).

What it means: Scale degrees arranged on a circle (radius 20 px). Each degree is a dot whose radius = ticket weight.
- **Large filled dot** = high ticket count.
- **Small dot** = low tickets.
- **No dot** = excluded.
- **Hollow square** = zero tickets but included.

The top 3 weighted degrees are connected by lines forming a chord triangle. This means tonal center and chord quality are visible as geometry. The circle naturally shows octave equivalence—degrees wrap around. Bias and spread could be shown as arc rotation and width (not yet implemented in this preview).

#### `stochastic-pitch-less-concepts`
**Inspiration:** *less-concepts* (norns cellular automaton).

What it means: Dense information layout.
- **Top-left**: Ticket pattern as 5×4 px filled rectangles (like an 8-bit seed). One rect per degree.
- **Center**: Large note name of the current generated pitch.
- **Right**: Parameter text list (degree, tickets, rotation, linearity, min/max, root).
- **Bottom**: 2×8 preset grid as 4×4 px rects.

This means maximum density. Every dimension is visible at once, but it requires reading rather than seeing. It is the expert-mode view: good for debugging, too busy for performance.

---

### Dice Variants

#### `stochastic-dice-shredder`
**Inspiration:** *Shredder* (O_C Cartesian grid).

What it means: 8×8 grid of 5×5 px frames (64 steps total). Each cell fill brightness = probability for the active axis. Active cell is filled solid; dotted crosshair lines run through its row and column. This means positional context is immediate—you always see where the active step sits in both dimensions. The crosshair is the shredder's strongest idea: it turns a flat grid into a coordinate system.

#### `stochastic-dice-register`
**Inspiration:** *ShiftReg* (O_C Turing Machine register bars).

What it means: 64 vertical bars (2 px wide, up to 10 px tall) in a horizon strip. Bar height = probability. Current step is a full-height bright bar (like a playhead flash). This means the entire 64-step probability landscape is visible as a silhouette. It is the most compact possible overview: 64 steps in ~24 px of vertical space. Good for monitoring; not for editing individual values.

---

### Marbles Variants

#### `stochastic-marbles-rndwalk`
**Inspiration:** *RndWalk* (O_C inverted bar meters).

What it means: Four horizontal inverted bars, one per parameter (spread, bias, steps, mode).
- **Unipolar bars** (spread, steps): left-aligned fill from a center track.
- **Bipolar bar** (bias): centered at midpoint, fill extends left or right.
- **Mode**: text label (OFF/SOFT/HARD).

This means continuous parameters feel like meters. The inverted bar (`gfxInvert` semantics simulated as bright fill on dark background) makes value changes feel like physical movement. Real-time modulation would animate these bars smoothly.

#### `stochastic-marbles-constellations`
**Inspiration:** *constellations* (norns star-field density).

What it means: Random star points in a bounded field. Star brightness and size fall off with distance from the bias center. Spread controls the width of the bright cluster; steps controls the star count.
- **Bright large dots** = high-density probability.
- **Dim small dots** = tail probability.
- **Crosshair** at bias position.

This means the distribution is not a chart—it is a field. You read density, not axes. It is the most musical of the marbles views: probability feels like emergent behavior rather than parameter values.

---

### Lock Variants

#### `stochastic-lock-skylines`
**Inspiration:** *skylines* (M-185 bar chart).

What it means: 16 captured events as vertical bars. Foreground bright = locked event (note height = pitch). Background dim = source step gate probability. Window fence brackets mark first/last. This means you compare what was captured against what the source suggested. A tall bright bar on a short dim background = the stochastic engine surprised you.

#### `stochastic-lock-qfwfq`
**Inspiration:** *qfwfq* (norns ASCII ribbon).

What it means: Two rows of 16 characters.
- **Top row** = captured event state: `X` = locked gate on, `.` = locked gate off, `?` = outside window.
- **Bottom row** = source step index (01–16).
- **Underline** = current replay cursor.
- **Dot** = playhead position.

This means the lock buffer is read as a password. The qfwfq "solving" metaphor maps neatly: matched/locked positions are `X`, mismatches are visible as `.` above a source index. Extremely compact; 64 events would be 4 rows of 16 characters.

---

### Track Variants

#### `stochastic-track-rndwalk`
**Inspiration:** *RndWalk* (O_C inverted bar meters).

What it means: Track console with inverted horizontal bars for all four visible parameters. Bipolar params (transpose, biases) are centered; unipolar params (accent, legato, fill) are left-aligned. Value numbers printed to the right. This means the track console becomes a live meter panel. Every parameter occupies the same visual form (bar width), making comparison instantaneous.

#### `stochastic-track-sequencex`
**Inspiration:** *SequenceX* (O_C 8-step vertical sliders).

What it means: Four vertical slider columns. Dotted line from top to bottom; 5×3 px rect indicator at the value position. Zero line marked across all sliders. Selected slider shows a filled rect; others show hollow frames. This means track parameters are read as altitude. The vertical orientation separates them from the horizontal step grid, preventing visual confusion during fast editing.

---

## Norns-Inspired Hybrid Pages (Second Wave)

These pages adapt patterns from **bline, Hiswing, meadowphysics, kreislauf, grd, pitter-patter,** and **pit-orchisstra**. The emphasis is on *line weight, spacing rhythm,* and *grid density* — the qualities that make norns screen UI feel alive.

---

### Steps Variants

#### `stochastic-steps-bline`
**Inspiration:** *bline* (parametric acid bassline sculptor).

What it means: Five stacked horizontal rows of vertical bar charts, one per layer (gate, note, length, retrigger, condition). Each bar is 3 px wide with a 1 px gap. The bar **top** is bright; the **body** is dim. Current step highlights the entire bar top.

This shows the bline idiom of *stacked micro-charts*. Every layer gets its own horizon line. You read vertically to compare the same step across layers, horizontally to see the pattern shape. The tight 3×1 bar geometry is the most space-efficient chart form in the entire set.

#### `stochastic-steps-hiswing`
**Inspiration:** *Hiswing* (Cirklon-style velocity sequencer with TR-909 swing).

What it means: 16 vertical lines rise from the baseline. Height = note pitch (inverted: higher note = taller line). Line brightness = gate probability. Even steps are displaced horizontally by a "swing offset" — the line tilts, creating a visual groove.

This means timing deformation is visible as *spatial displacement*. The Hiswing insight: swing is not a number, it is a stagger. Selected step lines extend 4 px higher, making the edit target pop without a separate cursor.

#### `stochastic-steps-meadowphysics`
**Inspiration:** *meadowphysics* (multi-voice physics sequencer).

What it means: Four horizontal strips of 2×2 px dots. Each strip = one probability layer. Dot brightness = probability value. Current step = bright dot. The meadowphysics aesthetic is *extreme minimalism*: no bars, no frames, just presence/absence in a grid of light.

This means the step editor becomes a *tracker field*. Four layers in 24 px of vertical space. The 2×2 dot is the smallest readable unit on the OLED — anything smaller would dissolve into noise.

---

### Pitch Variants

#### `stochastic-pitch-kreislauf`
**Inspiration:** *kreislauf* (concentric ring wedge sequencer).

What it means: Scale degrees rendered as filled polygon wedges in a ring (outer radius 22 px, inner 12 px). Wedge brightness = ticket count. Excluded degrees = dark wedge outline only. Active degree = bright wedge with outline.

This means pitch probability is read as *angular area*. The kreislauf wedge is the most musical ring representation: area encodes weight, position encodes scale degree, and the hollow center leaves room for a note-name label. The top-3 weighted degrees could be connected by chord lines (stubbed in this preview).

#### `stochastic-pitch-bline`
**Inspiration:** *bline* pattern bar chart.

What it means: Vertical 3 px bars per scale degree, height = ticket count. Bar top highlighted at selected degree. Same stacked-bar grammar as `stochastic-steps-bline` but applied to pitch degrees instead of steps.

This means pitch tickets feel like a *pattern*, not a list. The bline bar chart is compact, immediately comparable, and reads well at a glance. It is the anti-keyboard: no white/black key semantics, just pure magnitude.

---

### Dice Variants

#### `stochastic-dice-grd`
**Inspiration:** *grd* (cellular automaton grid).

What it means: 8×8 grid of 5×5 px filled rects. Brightness mapped directly from step probability (0–100% → None_ to Bright). No frames, no text inside cells — just a field of brightness.

This means the 64-step probability landscape is a *heatmap*. The grd aesthetic is raw density: you see clusters and voids, not individual values. It is the fastest possible overview of where probability mass lives in the sequence.

#### `stochastic-dice-kreislauf`
**Inspiration:** *kreislauf* ring wedges.

What it means: Four concentric rings, one per probability layer (gate, note, length, retrigger). Each ring has 16 wedges. Wedge brightness = probability. Rings get smaller toward the center, like a target.

This means all four probability axes are visible simultaneously in one polar plot. The kreislauf ring packing is dense but readable because each ring has a different radius. Current step highlights one wedge across all four rings — you see layer correlation at a glance.

---

### Marbles Variants

#### `stochastic-marbles-pit-orchisstra`
**Inspiration:** *pit-orchisstra* (snake-pit sequencer).

What it means: Probability field rendered as an 8×8 brightness grid (like grd). A snake path winds through the grid — each segment = one distribution sample. Snake body brightness = sample density. Food dots mark high-probability peaks.

This means the distribution is not a chart but a *terrain* the snake explores. The pit-orchisstra aesthetic turns abstract probability into movement. Spread controls how far the snake wanders; bias shifts the food field.

#### `stochastic-marbles-bline`
**Inspiration:** *bline* XY position page.

What it means: 4×4 grid background with a highlighted current cell. Crosshairs through the active cell. Spread = cursor size (highlighted area). Bias = cursor position. Four parameter labels printed to the right.

This means marbles parameters are read as *position* rather than magnitude. The bline XY grid is the simplest possible 2-D control surface: one point, one field, one crosshair. It maps naturally to spread/bias as spatial coordinates.

---

### Lock Variants

#### `stochastic-lock-meadowphysics`
**Inspiration:** *meadowphysics* tracker strips.

What it means: Four horizontal strips of 2×2 px dots. Each strip = captured event property (gate, note, length, retrigger). Brightness = event value. Outside the lock window = dim. Current step = bright dot.

This means the lock buffer is a *multi-track tracker*. The meadowphysics minimal dot grammar prevents visual clutter when showing 64 events × 4 properties. Gate strip shows on/off instantly; note strip shows pitch contour as a brightness wave.

#### `stochastic-lock-pit-orchisstra`
**Inspiration:** *pit-orchisstra* snake trail.

What it means: 16×8 grid background. Captured events drawn as a snake path: each event = one segment. Gate-on segments are bright; gate-off segments are dim. The snake winds left-to-right, top-to-bottom through the grid.

This means the lock buffer is a *path*, not a table. Time flows along the snake body. You see rhythmic patterns in the bright/dim alternation. It is the most narrative of the lock views: the sequence tells a story as a trail.

---

### Track Variants

#### `stochastic-track-bline`
**Inspiration:** *bline* 4-quadrant parameter page.

What it means: 2×2 grid of 19×19 px parameter cells. Selected cell = medium fill background + bright text. Unselected = low fill + medium text. Label and value centered in each cell.

This means the track console becomes a *control pad*. The bline quadrant layout is tactile: each parameter occupies the same square footprint, making encoder navigation feel like moving between physical buttons. No bars, no sliders — just value and label in a bright box.

#### `stochastic-track-pitter-patter`
**Inspiration:** *pitter-patter* (grid-based drum sequencer).

What it means: Four parameter columns. Each column has a 5×5 px brightness square at top (selected = filled bright, unselected = hollow frame). Label below the square. Value below the label.

This means track parameters are read as *status lights*. The pitter-patter square is a compact on/off/selected indicator. It is the sparsest possible track console: four squares, four numbers, four labels. Good for performance distance reading.

---

## Pure Reference Screens

These are exact (or near-exact) replicas of the external apps, rendered with the same Canvas/Font system. They are included so you can compare the original reference against the stochastic adaptations.

| Screen | Source | What it shows |
|--------|--------|---------------|
| `ref-prob-melod` | O_C ProbMeloD | 12 weight bars over piano keyboard. Active note triangles. |
| `ref-shredder` | O_C Shredder | 4×4 Cartesian grid, crosshairs, bar meters, imprint animation frames. |
| `ref-euclid` | Squares-and-Circles | Dual Euclidean rings (21 px radius). Square/circle step dots. |
| `ref-delinquencer` | norns delinquencer | 8×8 grid, corner-pixel erosion, intensity levels, page dots. |
| `ref-skylines` | norns skylines | Two-voice city skyline, height = repetition count. |
| `ref-prob-div` | O_C ProbabilityDivider | 4 horizontal sliders with knob rects, division labels, loop display. |

---

## Visual Language Glossary

| Pattern | Meaning | Origin |
|---------|---------|--------|
| **Filled square** | Deterministic / locked / certain | Squares-and-Circles |
| **Hollow circle** | Probabilistic / pending / uncertain | Squares-and-Circles |
| **Corner erosion** | Sub-100% probability (damage to certainty) | delinquencer |
| **Vertical bar height** | Magnitude (probability, repetition, pitch) | skylines, ProbMeloD |
| **Inverted bar** | Real-time continuous value | RndWalk |
| **Dotted line** | Neutral axis / zero / guide | SequenceX, Shredder |
| **Crosshair** | Active position / cursor target | Shredder, constellations |
| **Chord triangle** | Relationship between top-weighted pitches | Automatonnetz |
| **ASCII ribbon** | Dense event state encoding | qfwfq |
| **Ring layout** | Cyclic / looping / periodic | Euclidean, Kreislauf |
| **Stacked micro-chart** | Layered bar horizons in minimal vertical space | bline |
| **Swing stagger** | Horizontal displacement encodes timing feel | Hiswing |
| **Tracker dot** | 2×2 presence/absence indicator | meadowphysics |
| **Wedge polygon** | Angular area encodes weight | Kreislauf |
| **Brightness heatmap** | Raw density field without frames | grd |
| **Snake path** | Sequential event history as movement | pit-orchisstra |
| **XY position grid** | 2-D control surface with crosshairs | bline |
| **Quadrant pad** | Equal-square parameter cells | bline |
| **Status light** | 5×5 square as selected/unselected indicator | pitter-patter |
