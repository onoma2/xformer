#!/usr/bin/env python3
"""
DIRECT-page redesign preview — "the walker".

8 knobs:
  Top row    0: DURA  1: VARI  2: REST  (3..7 unused)
  Bottom row 8: CMPX  9: CONT  10: BIAS  11: SPRE  12: REPT  (13..15 unused)

Each knob has a single visible aspect in the walker animation:
  DURA  stride length
  VARI  stride irregularity (horizontal jitter)
  REST  gaps in the trail
  CMPX  step-to-step pitch leap
  CONT  PARTICLE ANGLE — positive contour leans particles up-right (ascending drift)
        negative contour leans down-right (descending drift), zero = horizontal
  BIAS  vertical anchor of the walk
  SPRE  vertical swing amplitude (= Marbles Spread)
  REPT  ghost trail brightness / persistence
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, Color, BlendMode, framebuffer_to_image

PAGE_W = 256
PAGE_H = 64


def draw_header(canvas, title):
    canvas.set_color(Color.Bright)
    canvas.set_font(Font.Small)
    canvas.draw_text(8, 8, title)
    canvas.set_color(Color.Low)
    canvas.hline(0, 12, PAGE_W)


def draw_footer(canvas, labels):
    canvas.set_font(Font.Tiny)
    slot_w = PAGE_W // len(labels)
    for i, lbl in enumerate(labels):
        if lbl is None: continue
        canvas.set_color(Color.Medium)
        canvas.draw_text(i * slot_w + 4, PAGE_H - 2, lbl)


def render_direct(canvas,
                  duration=5, variation=0, rest=0,
                  complexity=50, contour=0, bias=50, spread=50, repeats=0,
                  rhythm_loop=True, melody_loop=True,
                  held_step=-1):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)

    draw_header(canvas, "DIRECT")

    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 25, 53
    vp_w = vp_right - vp_left
    vp_h = vp_bot - vp_top
    vp_cy = (vp_top + vp_bot) // 2

    # --- Marbles bell distribution (background haze) ---------------------------
    bias_y = vp_top + int((100 - bias) * vp_h / 100)
    spread_y = max(2.0, (spread * vp_h) / 200.0)
    for y in range(vp_top, vp_bot + 1):
        d = (y - bias_y) / spread_y
        env = math.exp(-d * d * 1.4)
        if env < 0.25: continue
        col = Color.Low if env < 0.85 else Color.MediumLow
        canvas.set_color(col)
        step = 2 if env >= 0.6 else 3
        offset = (y * 7) % step
        for x in range(vp_left + offset, vp_right, step):
            canvas.point(x, y)

    # --- Walker history --------------------------------------------------------
    rng = random.Random(0xC0FFEE)
    n_events = 12

    base_stride = 6 + (7 - duration) * 5
    swing_h = int((spread * (vp_h - 4)) / 200)

    # Contour → vertical drift component per stride. Positive = ascend over time
    # (particles lean up-right), negative = descend. Scale so |contour|=100 gives
    # a clear ±2 px lift per stride.
    contour_step = int((contour * 4) / 100)  # -4..+4 px per stride

    positions = []
    x = vp_right - 2
    y = bias_y
    for n in range(n_events):
        jit = 0
        if variation > 0:
            jit = rng.randint(-variation // 8, variation // 8)
        x -= max(2, base_stride + jit)
        if x < vp_left:
            break

        leap_range = 1 + int((complexity * swing_h) / 100)
        dy = rng.randint(-leap_range, leap_range)
        y = bias_y + int((rng.random() - 0.5) * 2 * swing_h * (spread / 100))
        y += dy
        # Contour drift: older events sit further DOWN the contour slope. Going
        # back in time = move along -contour_step.
        y += contour_step * n
        y = max(vp_top + 2, min(vp_bot - 2, y))

        is_rest = (rng.random() * 100 < rest)
        positions.append((x, y, is_rest, n))

    # --- Draw ghost trail (oldest first so newer overdraws) --------------------
    # Each particle is rendered as a short slanted segment whose slope encodes
    # CONT — purely angle, length stays constant.
    seg_len = 3
    # Slope: convert contour to dx/dy. dx is +seg_len, dy = -contour_step (so
    # positive contour tilts up to the right).
    slope_dy = -int(round((contour * 1.5) / 100))  # -1..+1 typically
    for x, y, is_rest, age in reversed(positions):
        if is_rest:
            canvas.set_color(Color.Low)
            canvas.hline(x - 1, vp_cy, 3)
            continue
        fade = max(0, 100 - age * 12)
        trail_strength = (fade + repeats) // 2
        if trail_strength >= 70:    col = Color.MediumBright
        elif trail_strength >= 45:  col = Color.Medium
        elif trail_strength >= 20:  col = Color.MediumLow
        else:                       col = Color.Low
        canvas.set_color(col)

        # Slanted segment to encode contour direction. At zero contour this
        # collapses to a small dot/dash.
        if age <= 1:
            # "Now" marker — bigger to stand out.
            canvas.fill_rect(x - 1, y - 1, 3, 3)
        else:
            # Short angled segment: from (x-1, y+slope_dy) to (x+1, y-slope_dy).
            canvas.line(x - 1, y + slope_dy, x + 1, y - slope_dy)
            # A center pixel to anchor it visually.
            canvas.point(x, y)

    # --- Live "now" cross marker -----------------------------------------------
    now_x = vp_right - 2
    now_y = bias_y
    canvas.set_color(Color.Bright)
    canvas.fill_rect(now_x - 2, now_y - 2, 5, 5)
    canvas.set_color(Color.MediumBright)
    canvas.point(now_x + 3, now_y)
    canvas.point(now_x - 3, now_y)
    canvas.point(now_x, now_y + 3)
    canvas.point(now_x, now_y - 3)

    # --- Info row --------------------------------------------------------------
    # 4-letter labels. Step mapping:
    #   0=DURA 1=VARI 2=REST  (top row, 3..7 unused)
    #   8=CMPX 9=CONT 10=BIAS 11=SPRE 12=REPT  (bottom row, 13..15 unused)
    names = {0: "DURA", 1: "VARI", 2: "REST",
             8: "CMPX", 9: "CONT", 10: "BIAS", 11: "SPRE", 12: "REPT"}
    values = {0: duration, 1: variation, 2: rest,
              8: complexity, 9: contour, 10: bias, 11: spread, 12: repeats}

    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    if held_step in names:
        # Bipolar values show sign
        if held_step == 9:  # CONT is bipolar
            canvas.draw_text(8, 19, f"{names[held_step]} {values[held_step]:+d}")
        else:
            canvas.draw_text(8, 19, f"{names[held_step]} {values[held_step]}")
    else:
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Medium)
        # 4 fields per row, two rows. Top row at y=18, bottom row at y=24.
        top_order = [0, 1, 2]
        bot_order = [8, 9, 10, 11, 12]
        for c, idx in enumerate(top_order):
            txt = f"{names[idx][:3]}{values[idx]}"
            canvas.draw_text(8 + c * 40, 18, txt)
        # 5 bottom fields across ~240 px → ~48 px slot each
        bot_x = [8, 56, 104, 152, 200]
        for c, idx in enumerate(bot_order):
            v = values[idx]
            txt = (f"{names[idx][:3]}{v:+d}" if idx == 9 else f"{names[idx][:3]}{v}")
            canvas.draw_text(bot_x[c], 24, txt)

    # Footer mimics LOOP page
    fn_r = "LoopR" if rhythm_loop else "LiveR"
    fn_m = "LoopM" if melody_loop else "LiveM"
    draw_footer(canvas, [fn_r, fn_m, "NewR", "NewM", "NEXT"])


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'direct-walker-previews')
    os.makedirs(out_dir, exist_ok=True)

    cases = [
        ("default",         dict(duration=5, variation=0,  rest=0,  complexity=50, contour=0,    bias=50, spread=50, repeats=0)),
        ("calm",            dict(duration=3, variation=10, rest=10, complexity=20, contour=0,    bias=50, spread=20, repeats=40)),
        ("busy",            dict(duration=6, variation=70, rest=0,  complexity=80, contour=0,    bias=50, spread=80, repeats=0)),
        ("sparse",          dict(duration=2, variation=20, rest=60, complexity=30, contour=0,    bias=50, spread=40, repeats=0)),
        ("low-register",    dict(duration=5, variation=30, rest=10, complexity=40, contour=0,    bias=20, spread=60, repeats=20)),
        ("high-register",   dict(duration=5, variation=30, rest=10, complexity=40, contour=0,    bias=80, spread=60, repeats=20)),
        ("stepwise",        dict(duration=4, variation=20, rest=0,  complexity=10, contour=0,    bias=50, spread=70, repeats=0)),
        ("leap",            dict(duration=4, variation=20, rest=0,  complexity=90, contour=0,    bias=50, spread=70, repeats=0)),
        ("ghosts",          dict(duration=5, variation=20, rest=20, complexity=40, contour=0,    bias=50, spread=50, repeats=80)),
        # Contour sweeps — particle angle changes
        ("cont-asc",        dict(duration=5, variation=20, rest=0,  complexity=40, contour=+80,  bias=50, spread=60, repeats=20)),
        ("cont-desc",       dict(duration=5, variation=20, rest=0,  complexity=40, contour=-80,  bias=50, spread=60, repeats=20)),
        ("cont-flat",       dict(duration=5, variation=20, rest=0,  complexity=40, contour=0,    bias=50, spread=60, repeats=20)),
        # Mode + held
        ("live-live",       dict(duration=5, variation=30, rest=10, complexity=40, contour=0,    bias=50, spread=50, repeats=0, rhythm_loop=False, melody_loop=False)),
        ("split-mode",      dict(duration=5, variation=30, rest=10, complexity=40, contour=0,    bias=50, spread=50, repeats=0, rhythm_loop=True,  melody_loop=False)),
        ("held-bias",       dict(duration=5, variation=20, rest=10, complexity=40, contour=0,    bias=80, spread=60, repeats=30, held_step=10)),
        ("held-cont",       dict(duration=5, variation=20, rest=10, complexity=40, contour=-60,  bias=50, spread=60, repeats=20, held_step=9)),
    ]

    for name, params in cases:
        fb = FrameBuffer(PAGE_W, PAGE_H)
        canvas = Canvas(fb, brightness=1.0)
        canvas.set_font(Font.Small)
        render_direct(canvas, **params)
        img = framebuffer_to_image(fb, scale=4)
        path = os.path.join(out_dir, f"direct-{name}.png")
        img.save(path)
        print(f"  direct-{name:18s} -> {path}")


if __name__ == "__main__":
    main()
