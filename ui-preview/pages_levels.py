"""
Level-hero pages — one poetic image per layer.

Norns-style: image carries the state, minimal text. Each render is a
self-contained 256×64 frame. Safe content area is y=11..52.

Layers:
  CORE     — L1 macros (Density, Complexity, Shape)
  DIRECT   — live output state (gate stream + CV value)
  MARBLES  — distribution shape (Bias, Spread, Steps)
  LOOPER   — lock buffer
  MUTATION — mutation rune field
"""

import math
import random
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font
from painters import PageWidth, PageHeight, WindowPainter


# ============================================================================
# Demo macro state — CORE level synth values used by the three CORE pages
# ============================================================================
class LevelDemo:
    def __init__(self):
        self.density = 65       # 0..100 — how full the pattern is
        self.complexity = 45    # 0..100 — variance / irregularity
        self.shape = 1          # 0=smooth, 1=sharp, 2=noise
        # Direct level — live snapshot
        self.live_cv = 0.6      # -1..+1 normalized
        self.live_gate = True
        self.recent_gates = [1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1,
                             1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1]
        self.recent_cv = [0.1, 0.3, 0.5, 0.7, 0.6, 0.4, 0.2, 0.0,
                          -0.1, 0.1, 0.3, 0.5, 0.7, 0.8, 0.6, 0.4]
        # Mutation
        self.mutation = 30      # 0..100
        # Loop
        self.loop_on = True


# ============================================================================
# Helpers
# ============================================================================
def _brightness_color(v: float) -> Color:
    """Map 0..1 to a discrete color step."""
    if v >= 0.85:
        return Color.Bright
    if v >= 0.60:
        return Color.MediumBright
    if v >= 0.35:
        return Color.Medium
    if v >= 0.12:
        return Color.MediumLow
    return Color.Low


def _arc(canvas, cx, cy, r, a0, a1, color):
    """Draw an arc by sampling angles. a0..a1 in radians."""
    canvas.set_color(color)
    steps = max(8, int(abs(a1 - a0) * r))
    for i in range(steps + 1):
        a = a0 + (a1 - a0) * i / steps
        x = int(cx + r * math.cos(a))
        y = int(cy + r * math.sin(a))
        canvas.point(x, y)


def _dashed_arc(canvas, cx, cy, r, a0, a1, color, dash_count):
    """Draw a dashed arc — dash_count = number of dashes around the arc."""
    canvas.set_color(color)
    span = a1 - a0
    steps = max(16, int(abs(span) * r))
    for i in range(steps + 1):
        a = a0 + span * i / steps
        # alternating dashes
        phase = (i * dash_count * 2) // steps
        if phase % 2 == 0:
            x = int(cx + r * math.cos(a))
            y = int(cy + r * math.sin(a))
            canvas.point(x, y)


# ============================================================================
# CORE
# ============================================================================
def render_core_tide(canvas: Canvas, demo: LevelDemo):
    """Horizon waveline. Waterline y = Density, amplitude = Complexity, shape = Shape."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="CORE")
    WindowPainter.draw_footer(canvas, ["DENS", "CMPX", "SHAPE", None, "NEXT"])

    y_top = 12
    y_bot = 51
    span = y_bot - y_top
    # Higher density = higher water = waterline closer to top
    waterline = int(y_bot - (demo.density / 100.0) * (span - 4))
    amp = int(2 + (demo.complexity / 100.0) * 6)

    # Water body — dim fill below the waveline
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    for y in range(waterline + 2, y_bot + 1):
        canvas.hline(0, y, PageWidth)
    canvas.set_color(Color.MediumLow)
    canvas.hline(0, waterline + 1, PageWidth)

    # Wave line
    canvas.set_color(Color.MediumBright)
    rng = random.Random(7)
    for x in range(PageWidth):
        t = x / 18.0
        if demo.shape == 0:        # smooth sine
            y = waterline - int(amp * math.sin(t))
        elif demo.shape == 1:      # sharp saw
            frac = (t % math.pi) / math.pi
            y = waterline - int(amp * (2 * frac - 1))
        else:                      # noise
            y = waterline - int(amp * (rng.random() * 2 - 1))
        canvas.point(x, y)


def render_core_orbit(canvas: Canvas, demo: LevelDemo):
    """Three concentric rings. Inner = Density, mid = Complexity, outer dash = Shape."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="CORE")
    WindowPainter.draw_footer(canvas, ["DENS", "CMPX", "SHAPE", None, "NEXT"])

    cx = PageWidth // 2
    cy = (12 + 51) // 2

    # Hub
    canvas.set_color(Color.Bright)
    canvas.fill_rect(cx - 1, cy - 1, 3, 3)

    # Inner ring — Density (filled arc fraction)
    r_inner = 7
    canvas.set_color(Color.Low)
    _arc(canvas, cx, cy, r_inner, 0, 2 * math.pi, Color.Low)
    span = 2 * math.pi * (demo.density / 100.0)
    _arc(canvas, cx, cy, r_inner, -math.pi / 2, -math.pi / 2 + span, Color.Bright)

    # Middle ring — Complexity (filled arc fraction)
    r_mid = 13
    _arc(canvas, cx, cy, r_mid, 0, 2 * math.pi, Color.Low)
    span = 2 * math.pi * (demo.complexity / 100.0)
    _arc(canvas, cx, cy, r_mid, -math.pi / 2, -math.pi / 2 + span, Color.MediumBright)

    # Outer ring — Shape (dash pattern: 0=solid, 1=4 dashes, 2=many dashes)
    r_outer = 19
    if demo.shape == 0:
        _arc(canvas, cx, cy, r_outer, 0, 2 * math.pi, Color.Medium)
    elif demo.shape == 1:
        _dashed_arc(canvas, cx, cy, r_outer, 0, 2 * math.pi, Color.Medium, 6)
    else:
        _dashed_arc(canvas, cx, cy, r_outer, 0, 2 * math.pi, Color.Medium, 16)


def render_core_rain(canvas: Canvas, demo: LevelDemo):
    """Falling dot field. Count = Density, jitter = Complexity, wind = Shape."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="CORE")
    WindowPainter.draw_footer(canvas, ["DENS", "CMPX", "SHAPE", None, "NEXT"])

    rng = random.Random(11)
    canvas.set_blend_mode(BlendMode.Set)

    drop_count = 12 + int(demo.density * 0.9)
    jitter = int(2 + demo.complexity * 0.15)
    wind = (-2, 0, 2)[demo.shape] if demo.shape in (0, 1, 2) else 0

    for _ in range(drop_count):
        x = rng.randint(2, PageWidth - 3)
        y = rng.randint(13, 49)
        drop_len = 2 + rng.randint(0, jitter // 2)
        bright = rng.random()
        canvas.set_color(_brightness_color(bright))
        # streak from (x, y) by (wind/3, drop_len)
        for i in range(drop_len):
            px = x + int(wind * i / drop_len)
            py = y + i
            if 11 <= py <= 51:
                canvas.point(px, py)


# ============================================================================
# DIRECT
# ============================================================================
def render_direct_pulse(canvas: Canvas, demo: LevelDemo):
    """Heartbeat scope. Square pulse trace; vertical position = CV; pulse fill = gate."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="DIRECT")
    WindowPainter.draw_footer(canvas, [None, None, None, None, "NEXT"])

    y_mid = 31
    y_span = 16  # ±16 pixels around midline
    canvas.set_blend_mode(BlendMode.Set)

    # Baseline (dim)
    canvas.set_color(Color.Low)
    canvas.hline(0, y_mid, PageWidth)

    # Walk recent_gates left→right. Each cell is 8px wide.
    cell_w = PageWidth // len(demo.recent_gates)
    prev_y = y_mid
    for i, g in enumerate(demo.recent_gates):
        cv = demo.recent_cv[i % len(demo.recent_cv)] if g else 0
        target_y = y_mid - int(cv * y_span)
        x = i * cell_w
        # vertical edge from prev_y to target_y
        if i > 0:
            y0, y1 = sorted([prev_y, target_y])
            canvas.set_color(Color.Medium if i < len(demo.recent_gates) - 4 else Color.MediumBright)
            canvas.vline(x, y0, y1 - y0 + 1)
        # horizontal segment at target_y across this cell
        canvas.set_color(Color.MediumBright if g else Color.Low)
        if i == len(demo.recent_gates) - 1:
            canvas.set_color(Color.Bright if g else Color.Low)
        canvas.hline(x, target_y, cell_w)
        prev_y = target_y


def render_direct_vu(canvas: Canvas, demo: LevelDemo):
    """Two horizontal meters: top = gate density (recent), bottom = CV envelope."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="DIRECT")
    WindowPainter.draw_footer(canvas, [None, None, None, None, "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)
    bar_x = 20
    bar_w = PageWidth - 40

    # Top meter: gate density
    density = sum(demo.recent_gates) / max(1, len(demo.recent_gates))
    top_y = 18
    canvas.set_color(Color.Low)
    canvas.draw_rect(bar_x, top_y, bar_w, 5)
    fill_w = int(bar_w * density)
    canvas.set_color(Color.Bright)
    canvas.fill_rect(bar_x, top_y, fill_w, 5)

    # Bottom: CV envelope as a line trace
    bot_y_top = 30
    bot_y_bot = 48
    bot_mid = (bot_y_top + bot_y_bot) // 2
    canvas.set_color(Color.Low)
    canvas.hline(bar_x, bot_mid, bar_w)
    canvas.set_color(Color.Medium)
    canvas.vline(bar_x, bot_y_top, bot_y_bot - bot_y_top + 1)
    canvas.vline(bar_x + bar_w - 1, bot_y_top, bot_y_bot - bot_y_top + 1)
    # CV trace
    canvas.set_color(Color.MediumBright)
    cells = len(demo.recent_cv)
    for i in range(cells):
        x = bar_x + int(bar_w * (i + 0.5) / cells)
        y = bot_mid - int(demo.recent_cv[i] * (bot_y_bot - bot_y_top) / 2)
        canvas.point(x, y)
        canvas.point(x, y + 1) if y + 1 <= bot_y_bot else None

    # Current value as a bright peak dot at far right
    canvas.set_color(Color.Bright)
    cur_y = bot_mid - int(demo.live_cv * (bot_y_bot - bot_y_top) / 2)
    canvas.fill_rect(bar_x + bar_w - 2, cur_y - 1, 3, 3)


def render_direct_pond(canvas: Canvas, demo: LevelDemo):
    """Single bright point at (current step, current CV). Ripples expand and fade."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="DIRECT")
    WindowPainter.draw_footer(canvas, [None, None, None, None, "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)
    # Pond floor texture (dim baseline at y=31)
    canvas.set_color(Color.Low)
    for x in range(0, PageWidth, 4):
        canvas.point(x, 31)

    # Live point in the right third
    px = int(PageWidth * 0.72)
    py = 31 - int(demo.live_cv * 16)
    # Ripples
    for ring_idx, (r, color) in enumerate([(2, Color.Bright), (5, Color.MediumBright),
                                            (9, Color.Medium), (14, Color.Low)]):
        _arc(canvas, px, py, r, 0, 2 * math.pi, color)
    # Bright center
    canvas.set_color(Color.Bright)
    canvas.fill_rect(px - 1, py - 1, 3, 3)

    # Old fading ripples to the left (echoes of past gates)
    for i, (ox, oy_off, br) in enumerate([(40, 0, Color.Low), (80, -4, Color.MediumLow),
                                            (120, 2, Color.Low), (160, -2, Color.MediumLow)]):
        _arc(canvas, ox, 31 + oy_off, 4 + i, 0, 2 * math.pi, br)


# ============================================================================
# MARBLES
# ============================================================================
def render_marbles_bell(canvas: Canvas, demo: LevelDemo, sequence,
                        bias_override=None, spread_override=None, steps_override=None,
                        mode_override=None):
    """Bell-curve silhouette. Peak = bias (0..100 with 50=center), width = spread, quantization = steps.

    Hero responds to encoder edits: peak shifts left/right with bias, curve narrows/widens with
    spread, quantization ticks change density with steps, mode (Off/Soft/Hard) flattens or sharpens
    the curve.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="MARBLES")
    WindowPainter.draw_footer(canvas, ["BIAS", "SPRD", "STEPS", "MODE", "NEXT"])

    bias = bias_override if bias_override is not None else sequence.marbles_bias()
    spread_val = spread_override if spread_override is not None else sequence.marbles_spread()
    steps_val = steps_override if steps_override is not None else sequence.marbles_steps()
    mode_val = mode_override if mode_override is not None else sequence.marbles_mode()

    canvas.set_blend_mode(BlendMode.Set)
    # Bias 0..100 → maps to x offset from center (50 = center, 0 = far left, 100 = far right)
    # Use a 70-px swing range so the peak visibly travels but stays on screen
    bias_offset = int((bias - 50) * 0.8)  # -40..+40
    cx = PageWidth // 2 + bias_offset
    spread = max(8, int(spread_val * 0.8))
    steps = max(2, steps_val)
    baseline = 50
    peak_y = 14

    # Mode shape: 0=Off (flat), 1=Soft (gaussian), 2=Hard (steeper / boxcar-ish)
    if mode_val == 0:
        # Off — just a horizontal line + faded ticks, no curve at all
        canvas.set_color(Color.Low)
        canvas.hline(8, baseline, PageWidth - 16)
        return
    falloff = 1.4 if mode_val == 1 else 4.5  # Hard = much steeper falloff

    # Quantization bands (vertical guide lines)
    canvas.set_color(Color.Low)
    band_w = max(2, (spread * 4) // steps)
    for k in range(-steps, steps + 1):
        x = cx + k * band_w
        if 0 <= x < PageWidth:
            canvas.vline(x, baseline - 1, 3)

    # Bell curve — sample per-column, fill from baseline up
    for x in range(PageWidth):
        d = (x - cx) / float(spread)
        env = math.exp(-d * d * falloff)
        h = int(env * (baseline - peak_y))
        if h > 0:
            top = baseline - h
            for y in range(top, baseline + 1):
                v = (baseline - y) / max(1, h)
                col = _brightness_color(v * 0.85)
                canvas.set_color(col)
                canvas.point(x, y)

    # Peak marker — small triangle on top of the peak
    canvas.set_color(Color.Bright)
    canvas.point(cx, peak_y - 1)
    canvas.point(cx - 1, peak_y)
    canvas.point(cx + 1, peak_y)


def render_marbles_spray(canvas: Canvas, demo: LevelDemo, sequence):
    """Falling marbles. X = sampled value, density = steps, jitter = spread."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="MARBLES")
    WindowPainter.draw_footer(canvas, ["BIAS", "SPRD", "STEPS", "MODE", "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)
    rng = random.Random(31)
    cx = PageWidth // 2 + sequence.marbles_bias()
    spread = max(10, int(sequence.marbles_spread() * 0.9))
    steps = max(4, sequence.marbles_steps())
    n_marbles = 16 + steps * 6

    # Ground line
    canvas.set_color(Color.Medium)
    canvas.hline(0, 50, PageWidth)

    for _ in range(n_marbles):
        # gaussian-ish using sum of 3 uniform
        s = (rng.random() + rng.random() + rng.random() - 1.5) / 1.5
        x = cx + int(s * spread)
        if not (2 <= x <= PageWidth - 3):
            continue
        # Falling trail
        fall_y = rng.randint(14, 48)
        trail_len = rng.randint(2, 5)
        # trail
        for i in range(trail_len):
            ty = fall_y - i - 1
            if ty < 12:
                break
            canvas.set_color(_brightness_color((trail_len - i) / (trail_len + 1) * 0.5))
            canvas.point(x, ty)
        # marble
        canvas.set_color(Color.Bright)
        canvas.point(x, fall_y)


def render_marbles_magnet(canvas: Canvas, demo: LevelDemo, sequence):
    """Field rings around bias center; particles cluster near it."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="MARBLES")
    WindowPainter.draw_footer(canvas, ["BIAS", "SPRD", "STEPS", "MODE", "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)
    cx = PageWidth // 2 + sequence.marbles_bias()
    cy = 31
    spread = max(5, int(sequence.marbles_spread() * 0.5))
    steps = max(3, sequence.marbles_steps())

    # Field rings — dim concentric arcs, brightness falling outward
    for ring_r, col in [(spread // 2, Color.MediumBright),
                        (spread, Color.Medium),
                        (int(spread * 1.6), Color.MediumLow),
                        (int(spread * 2.4), Color.Low)]:
        _arc(canvas, cx, cy, ring_r, 0, 2 * math.pi, col)

    # Magnet core
    canvas.set_color(Color.Bright)
    canvas.fill_rect(cx - 1, cy - 1, 3, 3)

    # Particles — gaussian distribution around magnet
    rng = random.Random(53)
    n_particles = steps * 8
    for _ in range(n_particles):
        ang = rng.random() * 2 * math.pi
        r = abs(rng.gauss(0, spread / 1.8))
        px = int(cx + r * math.cos(ang))
        py = int(cy + r * math.sin(ang) * 0.6)  # squash vertically (page is wider)
        if not (2 <= px <= PageWidth - 3 and 12 <= py <= 50):
            continue
        # brightness falls with distance
        close = max(0, 1 - r / (spread * 2.5))
        canvas.set_color(_brightness_color(close * 0.9))
        canvas.point(px, py)


# ============================================================================
# LOOPER
# ============================================================================
def render_loop_tape(canvas: Canvas, demo: LevelDemo, sequence):
    """Horizontal tape strip. Window bracket = loop. Mutation = scratches."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LOOP")
    WindowPainter.draw_footer(canvas, ["LOOP", "WIN", "MUT", "RNW", "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)
    tape_top = 22
    tape_bot = 40
    tape_y_mid = (tape_top + tape_bot) // 2
    margin = 10

    # Tape body
    canvas.set_color(Color.MediumLow)
    canvas.fill_rect(margin, tape_top, PageWidth - 2 * margin, tape_bot - tape_top + 1)

    # Event marks across the tape (read from lock buffer)
    n_events = 32
    avail_w = PageWidth - 2 * margin
    for i in range(n_events):
        ev = sequence.lock_event(i)
        x = margin + (i * avail_w) // n_events
        if ev.gate():
            # gate-on tick — vertical line
            canvas.set_color(Color.Bright)
            canvas.vline(x, tape_top + 2, tape_bot - tape_top - 3)
        else:
            canvas.set_color(Color.Low)
            canvas.point(x, tape_y_mid)

    # Mutation scratches — random holes
    rng = random.Random(7)
    n_scratches = int((demo.mutation / 100.0) * 10)
    for _ in range(n_scratches):
        sx = rng.randint(margin + 2, PageWidth - margin - 2)
        sy = rng.randint(tape_top + 1, tape_bot - 1)
        canvas.set_color(Color.None_)
        canvas.fill_rect(sx, sy, 2, 2)

    # Loop window brackets (first/last)
    lf = sequence.lock_first()
    ll = sequence.lock_last()
    bx_first = margin + (lf * avail_w) // n_events
    bx_last = margin + (ll * avail_w) // n_events
    canvas.set_color(Color.Bright)
    # left bracket
    canvas.vline(bx_first - 1, tape_top - 3, tape_bot - tape_top + 7)
    canvas.hline(bx_first - 1, tape_top - 3, 4)
    canvas.hline(bx_first - 1, tape_bot + 3, 4)
    # right bracket
    canvas.vline(bx_last + 1, tape_top - 3, tape_bot - tape_top + 7)
    canvas.hline(bx_last - 2, tape_top - 3, 4)
    canvas.hline(bx_last - 2, tape_bot + 3, 4)

    # Reel direction arrow (right edge if loop_on)
    if demo.loop_on:
        canvas.set_color(Color.MediumBright)
        ax = PageWidth - 8
        canvas.line(ax, tape_y_mid - 2, ax + 4, tape_y_mid)
        canvas.line(ax, tape_y_mid + 2, ax + 4, tape_y_mid)


# ============================================================================
# MUTATION
# ============================================================================
def render_mutation_rune(canvas: Canvas, demo: LevelDemo, sequence):
    """4 rows × 8 cols of glyphs. Each glyph = one captured event. Mutated cells corrupted."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="MUT")
    WindowPainter.draw_footer(canvas, ["RATE", "AXIS", "SEED", "FRZ", "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)
    cols = 8
    rows = 4
    cell_w = 28
    cell_h = 9
    grid_w = cols * cell_w
    grid_h = rows * cell_h
    x0 = (PageWidth - grid_w) // 2
    y0 = 14

    rng = random.Random(19)
    n_mutated = int((demo.mutation / 100.0) * cols * rows * 0.6)
    mutated_cells = set()
    while len(mutated_cells) < n_mutated:
        mutated_cells.add((rng.randint(0, rows - 1), rng.randint(0, cols - 1)))

    for r in range(rows):
        for c in range(cols):
            idx = r * cols + c
            ev = sequence.lock_event(idx)
            cx = x0 + c * cell_w + cell_w // 2
            cy = y0 + r * cell_h + cell_h // 2
            is_mut = (r, c) in mutated_cells

            base = Color.Low if is_mut else Color.Medium
            bright = Color.MediumBright if is_mut else Color.Bright

            if ev.gate():
                # filled center dot — gate on
                canvas.set_color(bright)
                canvas.fill_rect(cx - 1, cy - 1, 3, 3)
                # CV stem above
                cv_h = (ev.note() % 5)
                canvas.set_color(base)
                canvas.vline(cx, cy - 2 - cv_h, cv_h + 1)
                # length tail right
                ln = max(1, ev.length() // 2)
                canvas.hline(cx + 2, cy, ln)
                # retrigger pips below
                rt = ev.retrigger()
                for k in range(rt):
                    canvas.point(cx - 2 + k, cy + 2)
            else:
                # gate off — just a faint dot
                canvas.set_color(Color.Low if not is_mut else Color.None_)
                canvas.point(cx, cy)

            # Corruption mark for mutated cells
            if is_mut:
                canvas.set_color(Color.Low)
                canvas.point(cx + 3, cy - 2)
                canvas.point(cx - 3, cy + 2)
