"""
Stochastic LIVE page — proposed quiet redesign.

Goals (from user direction):
- Readable. Motion comes from knob values changing, not from idle animation.
- Newest event is brightest ("brighter on hit"); older events fade.
- NewR / NewM triggers a refresh of the trail.
- Surface 11 knobs: duration, variation, rest, burst x3 (burst, count, rate),
  repeat, spread, contour, complexity, bias.

Visual grammar:
- Bell haze (vertical): center = Bias, width = Spread. Static between knob turns.
- Event trail of rings, right (newest, bright) -> left (oldest, dim).
- Ring spacing across the strip = Duration (longer note = fewer / further apart).
- Ring radius wobble = Variation.
- Rest event = short dash, no ring.
- Repeat-flagged event = double-stroke ring.
- Child pips around a ring = Burst presence; count = Burst Count; spacing = Burst Rate.
- Trail vertical drift across age = Contour (positive tilts up-right).
- Ring brightness modulation across the trail = Complexity (smooth vs spiky).
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font
from painters import PageWidth, PageHeight, WindowPainter


class LiveEvent:
    __slots__ = ("cv", "rest", "burst_n", "repeat")
    def __init__(self, cv, rest=False, burst_n=0, repeat=False):
        self.cv = cv
        self.rest = rest
        self.burst_n = burst_n
        self.repeat = repeat


def _demo_events():
    """Synthetic history — newest first."""
    return [
        LiveEvent(cv=+0.4, burst_n=2),                  # newest (right edge)
        LiveEvent(cv=+0.7),
        LiveEvent(cv=+0.2, repeat=True),
        LiveEvent(cv=-0.1),
        LiveEvent(cv=0.0, rest=True),
        LiveEvent(cv=-0.5),
        LiveEvent(cv=-0.3, burst_n=3),
        LiveEvent(cv=+0.1),
        LiveEvent(cv=+0.6),
        LiveEvent(cv=+0.3),
        LiveEvent(cv=-0.2),
    ]


def _gaussian(x, center, width):
    d = (x - center) / float(width)
    return math.exp(-d * d * 2.0)


def _brightness(v):
    if v >= 0.85: return Color.Bright
    if v >= 0.60: return Color.MediumBright
    if v >= 0.35: return Color.Medium
    if v >= 0.12: return Color.MediumLow
    return Color.Low


def _draw_ring(canvas, cx, cy, r, color, double=False):
    canvas.set_color(color)
    steps = max(8, int(2 * math.pi * r))
    for i in range(steps + 1):
        a = 2 * math.pi * i / steps
        x = int(round(cx + r * math.cos(a)))
        y = int(round(cy + r * math.sin(a)))
        canvas.point(x, y)
    if double:
        steps2 = max(6, int(2 * math.pi * (r - 1)))
        for i in range(steps2 + 1):
            a = 2 * math.pi * i / steps2
            x = int(round(cx + (r - 2) * math.cos(a)))
            y = int(round(cy + (r - 2) * math.sin(a)))
            canvas.point(x, y)


def _event_xy(idx, vp_left, vp_right, vp_top, vp_bot,
              complexity, contour, seed):
    h = (idx * 2654435761 ^ seed * 0x9E3779B1) & 0xFFFFFFFF
    rx = (h & 0xFFFF) / 65535.0
    ry = ((h >> 16) & 0xFFFF) / 65535.0
    cx = (vp_left + vp_right) / 2.0
    cy = (vp_top + vp_bot) / 2.0
    half_w = (vp_right - vp_left) / 2.0
    half_h = (vp_bot - vp_top) / 2.0
    scale = 0.35 + (complexity / 100.0) * 0.65
    x = cx + (rx - 0.5) * 2 * half_w * scale
    y = cy + (ry - 0.5) * 2 * half_h * scale
    # Contour pulls the constellation along a diagonal — positive = up-right.
    # Stronger for outlying indices so the cloud visibly tilts as a whole.
    tilt = (contour / 100.0) * (idx - 5.5)
    x += tilt * 2.5
    y -= tilt * 1.2
    x = max(vp_left + 4, min(vp_right - 4, x))
    y = max(vp_top + 4, min(vp_bot - 4, y))
    return int(x), int(y)


def render_stochastic_live_constellation(canvas: Canvas,
                                          duration=4, variation=20, rest=15,
                                          burst=40, burst_count=3, burst_rate=50,
                                          repeat=30, spread=50,
                                          contour=20, complexity=35, bias=50,
                                          seed=0xC0FFEE,
                                          slot_count=12,
                                          playing_idx=3,
                                          rest_slots=None, repeat_slots=None,
                                          burst_slots=None,
                                          held_step=-1):
    """
    Constellation Live page: rings live in stable XY positions
    seeded by complexity/contour/seed. Size and density driven by
    duration/rest/variation. The currently-firing ring lights up.
    """
    if rest_slots is None:
        rest_slots = {5}
    if repeat_slots is None:
        repeat_slots = {2, 8}
    if burst_slots is None:
        burst_slots = {3: 2, 9: 4}

    vp_left, vp_right = 8, PageWidth - 8
    vp_top, vp_bot = 25, 51
    vp_h = vp_bot - vp_top

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LIVE")
    canvas.set_blend_mode(BlendMode.Set)

    # Bell haze — Bias center / Spread width.
    bias_y = vp_top + ((100 - bias) * vp_h) // 100
    spread_w = max(3, (spread * vp_h) // 200)
    for y in range(vp_top, vp_bot + 1):
        env = _gaussian(y, bias_y, spread_w + 2)
        if env < 0.18:
            continue
        col = Color.Low if env < 0.55 else Color.MediumLow
        canvas.set_color(col)
        step = 3 if env < 0.72 else 2
        offset = (y * 7) % step
        for x in range(vp_left + offset, vp_right, step):
            if ((x + y * 3) % 11) < 2:
                canvas.point(x, y)

    # Ring base radius from Duration LUT (0..7).
    base_r = 2 + duration // 2  # 2..5
    # Variation adds per-ring wobble.
    # Sparsity: Rest knob removes some rings entirely (replaced by dim dot).
    sparse_threshold = 100 - rest

    # Age-fade table: ring at age 0 = bright/filled; age N = invisible.
    # Each ring has its own seeded XY; age increments as events flow past it.
    def age_color(age):
        if age == 0: return Color.Bright
        if age <= 1: return Color.MediumBright
        if age <= 3: return Color.Medium
        if age <= 5: return Color.MediumLow
        if age <= 8: return Color.Low
        return None  # invisible

    # Build all 12 ring slots oldest-first so newer rings overdraw older fades.
    rings = []
    for age in range(slot_count):
        x, y = _event_xy(age, vp_left, vp_right, vp_top, vp_bot,
                         complexity, contour, seed)
        if variation > 0:
            wob_x = (((age * 17 + seed) % 7) - 3) * (variation // 25)
            wob_y = (((age * 23 + seed) % 5) - 2) * (variation // 35)
            x = max(vp_left + 4, min(vp_right - 4, x + wob_x))
            y = max(vp_top + 4, min(vp_bot - 4, y + wob_y))
        r = base_r
        if variation > 0:
            r += (((age * 11) % 5) - 2) * (variation // 40)
            r = max(2, min(6, r))
        is_rest = age in rest_slots
        keep = ((age * 37 + seed) % 100) < sparse_threshold
        rings.append((age, x, y, r, is_rest, keep))

    for age, x, y, r, is_rest, keep in reversed(rings):
        col = age_color(age)
        if col is None:
            continue
        if is_rest or not keep:
            canvas.set_color(col if age < 4 else Color.Low)
            canvas.point(x, y)
            continue
        if age == 0:
            _draw_ring(canvas, x, y, r + 1, Color.Bright,
                       double=(age in repeat_slots) and repeat > 50)
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x - 1, y - 1, 3, 3)
            canvas.set_color(Color.MediumBright)
            canvas.point(x - r - 2, y)
            canvas.point(x + r + 2, y)
            canvas.point(x, y - r - 2)
            canvas.point(x, y + r + 2)
            if age in burst_slots and burst > 0:
                n = min(burst_slots[age], burst_count - 1)
                petal_r = r + 3 + (burst_rate * 3) // 100
                for k in range(n):
                    a = 2 * math.pi * k / max(1, n) + math.pi / 4
                    bx = int(round(x + petal_r * math.cos(a)))
                    by = int(round(y + petal_r * math.sin(a)))
                    if vp_left <= bx < vp_right and vp_top <= by <= vp_bot:
                        canvas.set_color(Color.MediumBright)
                        canvas.point(bx, by)
        else:
            _draw_ring(canvas, x, y, r, col,
                       double=(age in repeat_slots) and repeat > 50)

    # Chip strip and held label.
    # Grid-locked: label padded to 2 chars, value field is fixed-width so
    # digits don't shift the next chip when a knob changes.
    def chip_text(lbl, v, signed=False):
        return f"{lbl:<2}{v:+4d}" if signed else f"{lbl:<2}{v:3d}"

    canvas.set_font(Font.Tiny)
    if held_step < 0:
        canvas.set_color(Color.Medium)
        top = [("D", duration, False), ("V", variation, False), ("R", rest, False),
               ("BU", burst, False), ("BC", burst_count, False), ("BR", burst_rate, False)]
        bot = [("E", repeat, False), ("S", spread, False), ("O", contour, True),
               ("X", complexity, False), ("I", bias, False)]
        col_step = (PageWidth - 16) // 6
        for j, (lbl, v, signed) in enumerate(top):
            canvas.draw_text(8 + j * col_step, 18, chip_text(lbl, v, signed))
        for j, (lbl, v, signed) in enumerate(bot):
            canvas.draw_text(8 + j * col_step, 24, chip_text(lbl, v, signed))
    else:
        canvas.set_font(Font.Small)
        canvas.set_color(Color.Bright)
        labels = {
            0:  f"DURATION {duration}",
            1:  f"VARIATION {variation}",
            2:  f"REST {rest}",
            6:  f"BURST {burst}",
            7:  f"BURST COUNT {burst_count}",
            8:  f"COMPLEXITY {complexity}",
            9:  f"CONTOUR {contour:+d}",
            10: f"BIAS {bias}",
            11: f"SPREAD {spread}",
            14: f"REPEAT {repeat}",
            15: f"BURST RATE {burst_rate}",
        }
        canvas.draw_text(8, 19, labels.get(held_step, "?"))

    WindowPainter.draw_footer(canvas, ["LoopR", "LoopM", "NewR", "NewM", "NEXT"])


def render_stochastic_live_proposed(canvas: Canvas,
                                    # Important knobs (0..100 or signed where noted).
                                    duration=4,              # 0..7 LUT index
                                    variation=20,
                                    rest=15,
                                    burst=40,
                                    burst_count=3,           # 2..8 total cells
                                    burst_rate=50,
                                    repeat=30,
                                    spread=50,
                                    contour=20,              # -100..+100
                                    complexity=35,
                                    bias=50,
                                    events=None,
                                    held_step=-1):
    if events is None:
        events = _demo_events()

    vp_left, vp_right = 8, PageWidth - 8
    vp_top, vp_bot = 25, 51
    vp_h = vp_bot - vp_top
    vp_w = vp_right - vp_left

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LIVE")

    canvas.set_blend_mode(BlendMode.Set)

    # ---------------------------------------------------------------
    # Bell haze — Bias (center y), Spread (width)
    # ---------------------------------------------------------------
    bias_y = vp_top + ((100 - bias) * vp_h) // 100
    spread_w = max(3, (spread * vp_h) // 200)
    for y in range(vp_top, vp_bot + 1):
        env = _gaussian(y, bias_y, spread_w + 2)
        if env < 0.18:
            continue
        col = Color.Low if env < 0.55 else Color.MediumLow
        canvas.set_color(col)
        step = 3 if env < 0.72 else 2
        offset = (y * 7) % step
        for x in range(vp_left + offset, vp_right, step):
            if ((x + y * 3) % 11) < 2:
                canvas.point(x, y)

    # ---------------------------------------------------------------
    # Event trail — newest on right, oldest on left.
    # Spacing comes from Duration (LUT 0..7 maps to step px).
    # ---------------------------------------------------------------
    # Duration LUT 0..7: short = tight spacing, long = wide spacing.
    spacing = 10 + duration * 3
    max_events = max(1, vp_w // spacing)
    visible = events[:max_events]

    contour_dy = (contour * 6) // 100  # over the whole strip, +/-6 px
    cv_scale = vp_h / 4.5

    rings = []
    x = vp_right - 6
    for i, ev in enumerate(visible):
        if x < vp_left + 4:
            break
        y = bias_y - int(ev.cv * cv_scale) - (contour_dy * i) // max(1, max_events - 1)
        y = max(vp_top + 3, min(vp_bot - 3, y))
        rings.append((i, x, y, ev))
        x -= spacing

    # Draw oldest first so newest overdraws.
    for i, x, y, ev in reversed(rings):
        if ev.rest:
            # Rest marker — small dash at bias_y horizon, not on cv line.
            canvas.set_color(Color.MediumLow if i > 0 else Color.Medium)
            canvas.hline(x - 2, bias_y, 5)
            continue

        # Fade by age, modulated by Complexity (spiky if high).
        age_fade = max(0.18, 1.0 - i * 0.10)
        spike = 1.0
        if complexity > 0:
            spike = 1.0 + ((complexity / 100.0) * (((i * 13 + 7) % 7) - 3) * 0.18)
            spike = max(0.4, min(1.4, spike))
        bright = age_fade * spike
        if i == 0:
            bright = 1.0  # newest is always full bright.

        # Ring radius — Variation wobbles it; base 3.
        base_r = 3
        if variation > 0:
            wob = ((i * 11 + 5) % 5) - 2  # -2..+2
            base_r += (variation * wob) // 100
            base_r = max(2, min(5, base_r))

        col = _brightness(bright)
        _draw_ring(canvas, x, y, base_r, col, double=ev.repeat and repeat > 50)

        # Center dot for the newest ring — the "hit" marker.
        if i == 0:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x - 1, y - 1, 3, 3)
            # 4 crosshair pips for emphasis.
            canvas.set_color(Color.MediumBright)
            canvas.point(x - base_r - 2, y)
            canvas.point(x + base_r + 2, y)
            canvas.point(x, y - base_r - 2)
            canvas.point(x, y + base_r + 2)
        else:
            canvas.set_color(col)
            canvas.point(x, y)

        # Burst children — pips around the ring at Burst Rate spacing.
        if ev.burst_n > 0 and burst > 0:
            # Pip spacing along the right side of the ring: tighter = high rate.
            pip_step = max(2, 8 - (burst_rate * 6) // 100)
            n = min(ev.burst_n, burst_count - 1)
            for k in range(1, n + 1):
                px = x + base_r + 1 + k * pip_step
                if px >= vp_right - 1:
                    break
                pcol = _brightness(bright * 0.7)
                canvas.set_color(pcol)
                canvas.point(px, y)
                canvas.point(px, y - 1)

    # ---------------------------------------------------------------
    # Param chip strip — Tiny font, 2 rows x 6 slots.
    # Top row (rhythm/articulation): D V R BU BC BR
    # Bottom row (pitch shape):       E S O X I  (5 slots, last empty)
    # Held-step Small label overrides the chip strip when one is held.
    # ---------------------------------------------------------------
    canvas.set_font(Font.Tiny)
    if held_step < 0:
        canvas.set_color(Color.Medium)
        top = [
            ("D",  duration),  # show LUT idx as raw — firmware would print fraction
            ("V",  variation),
            ("R",  rest),
            ("BU", burst),
            ("BC", burst_count),
            ("BR", burst_rate),
        ]
        bot = [
            ("E", repeat),
            ("S", spread),
            ("O", contour),
            ("X", complexity),
            ("I", bias),
        ]
        col_step = (PageWidth - 16) // 6
        for j, (lbl, v) in enumerate(top):
            s = f"{lbl}{v:+d}" if lbl == "O" else f"{lbl}{v}"
            canvas.draw_text(8 + j * col_step, 18, s)
        for j, (lbl, v) in enumerate(bot):
            s = f"{lbl}{v:+d}" if lbl == "O" else f"{lbl}{v}"
            canvas.draw_text(8 + j * col_step, 24, s)
    else:
        canvas.set_font(Font.Small)
        canvas.set_color(Color.Bright)
        labels = {
            0:  f"DURATION {duration}",
            1:  f"VARIATION {variation}",
            2:  f"REST {rest}",
            6:  f"BURST {burst}",
            7:  f"BURST COUNT {burst_count}",
            8:  f"COMPLEXITY {complexity}",
            9:  f"CONTOUR {contour:+d}",
            10: f"BIAS {bias}",
            11: f"SPREAD {spread}",
            14: f"REPEAT {repeat}",
            15: f"BURST RATE {burst_rate}",
        }
        canvas.draw_text(8, 19, labels.get(held_step, "?"))

    WindowPainter.draw_footer(canvas, ["LoopR", "LoopM", "NewR", "NewM", "NEXT"])
