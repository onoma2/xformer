#!/usr/bin/env python3
"""
Curve Track OLED renders for CURVE_MANUAL.html — actual-state, mirrors the
firmware CurveSequenceEditPage::draw() exactly.

The Curve track has ONE edit page; F5 cycles edit modes
(Step -> Global Phase -> Wavefolder -> Chaos). There are no separate hero pages.

  1) curve-step      — default Step mode: 16-step curve lane, loop markers,
                       dotted grid, step numbers, gate-event row. (Layer = Shape)
  2) curve-wavefolder — F5 Wavefolder mode: 5-column FOLD/GAIN/FILTER/XFADE signal chain.
  3) curve-chaos      — F5 Chaos mode: 4-column AMT/HZ/HEAT/DAMP + range/algo labels.

Mirrors src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp draw():
  Step UI       lines 374-512  (loopY=16, curveY=24, curveHeight=20, bottomY=48)
  Wavefolder UI lines 222-293  (colWidth=51, valueY=26, barY=32, barH=4, barW=40)
  Chaos UI      lines 295-372  (same geometry, range@y=44 col0, algo@y=44/50 col1)
Curve shape math from src/apps/sequencer/model/Curve.cpp.

Outputs to ui-preview/curve-manual/.
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, BlendMode, framebuffer_to_image
from painters import WindowPainter, SequencePainter

W, H = 256, 64
StepCount = 16

# Curve shape functions — mirror src/apps/sequencer/model/Curve.cpp.
def _ramp_up(x):       return x
def _ramp_down(x):     return 1.0 - x
def _exp_up(x):        return x * x
def _exp_down(x):      return (1.0 - x) * (1.0 - x)
def _log_up(x):        return math.sqrt(x)
def _smooth_up(x):     return x * x * (3.0 - 2.0 * x)
def _triangle(x):      return (x if x < 0.5 else 1.0 - x) * 2.0
def _bell(x):          return 0.5 - 0.5 * math.cos(x * 2.0 * math.pi)
def _high(x):          return 1.0


def _new():
    fb = FrameBuffer(W, H)
    c = Canvas(fb, brightness=1.0)
    c.set_font(Font.Tiny)
    return fb, c


def _draw_curve(c, x, y, w, h, last_y, function, vmin, vmax):
    """Mirror drawCurve() in CurveSequenceEditPage.cpp:137."""
    def eval_(xx):
        return (1.0 - (function(xx) * (vmax - vmin) + vmin)) * h
    fy0 = y + eval_(0.0)
    if last_y[0] >= 0.0 and last_y[0] != fy0:
        c.line(x, int(round(last_y[0])), x, int(round(fy0)))
    i = 0
    while i < w:
        fy1 = y + eval_((float(i) + 1) / w)
        c.line(x + i, int(round(fy0)), x + i + 1, int(round(fy1)))
        fy0 = fy1
        i += 1
    last_y[0] = fy0


def _draw_gate_pattern(c, x, y, w, h, gate):
    """Mirror drawGatePattern() at line 164."""
    gs = w // 4
    gw = w // 8
    for i in range(4):
        c.set_color(Color.Bright if (gate & (1 << i)) else Color.Medium)
        c.fill_rect(x + i * gs, y, gw, h)


# ---------------------------------------------------------------------------
# 1) Step mode (default). CurveSequenceEditPage.cpp:374-512
# ---------------------------------------------------------------------------
def render_step(c):
    WindowPainter.clear(c)
    # drawHeader(canvas, model, engine, "STEPS")
    WindowPainter.draw_header(c, track=2, play_pattern=0, edit_pattern=0, mode="STEPS")
    # layer() == Shape -> active function "SHAPE" (CurveSequence::layerName)
    WindowPainter.draw_active_function(c, "SHAPE")
    WindowPainter.draw_footer(c, ["SHAPE", "MIN", "MAX", "GATE", "PHASE"], highlight=0)

    step_width = W // StepCount   # 16
    loop_y = 16
    curve_y = 24
    curve_height = 20
    bottom_y = 48
    first_step, last_step = 0, 15

    # A representative edited sequence (shape, min, max as 0..255, gate-event mask).
    shapes = [_ramp_up, _exp_up, _bell, _triangle, _ramp_down, _exp_down,
              _smooth_up, _high, _log_up, _ramp_up, _bell, _exp_down,
              _triangle, _ramp_down, _exp_up, _ramp_down]
    mins = [0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0]
    maxs = [255, 255, 255, 255, 200, 255, 255, 255, 220, 255, 255, 180, 255, 255, 255, 160]
    gates = [0, 1, 0, 3, 0, 0, 2, 0, 0, 1, 0, 0, 3, 0, 0, 0]

    # loop start/end markers (line 405-406)
    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.Bright)
    SequencePainter.draw_loop_start(c, (first_step) * step_width + 1, loop_y, step_width - 2)
    SequencePainter.draw_loop_end(c, (last_step) * step_width + 1, loop_y, step_width - 2)

    # dotted grid (line 409-417)
    c.set_color(Color.Low)
    for step_index in range(1, StepCount):
        x = step_index * step_width
        y = 0
        while y <= curve_height:
            c.point(x, curve_y + y)
            y += 2

    last_y = [-1.0]
    for i in range(StepCount):
        vmin = mins[i] / 255.0
        vmax = maxs[i] / 255.0
        x = i * step_width
        y = 20

        # loop-band dot on steps inside loop (line 432-435)
        if i > first_step and i <= last_step:
            c.set_blend_mode(BlendMode.Set)
            c.set_color(Color.Bright)
            c.point(x, loop_y)

        # step number (line 437-441), selected=Bright else Medium; none selected here
        c.set_blend_mode(BlendMode.Set)
        c.set_color(Color.Medium)
        s = str(i + 1)
        c.draw_text(x + (step_width - c.text_width(s) + 1) // 2, y - 2, s)

        # curve (line 443-448)
        c.set_color(Color.Bright)
        c.set_blend_mode(BlendMode.Add)
        _draw_curve(c, x, curve_y, step_width, curve_height, last_y, shapes[i], vmin, vmax)

        # Shape layer: no extra bottom row drawn (Layer::Shape break). Gate-event row
        # belongs to the GATE layer; show it muted here is wrong, so omit — Shape layer.

    # playhead (isActiveSequence) at step ~3.4 (line 488-492)
    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.Bright)
    cur = 3.4
    c.vline(int(cur * step_width), curve_y, curve_height)


# ---------------------------------------------------------------------------
# 1b) Step mode showing GATE layer (gate-event row visible). Same page, F4=GATE.
# ---------------------------------------------------------------------------
def render_step_gate(c):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=2, play_pattern=0, edit_pattern=0, mode="STEPS")
    # layer() == Gate -> active function "GATE EVENTS" (line 385)
    WindowPainter.draw_active_function(c, "GATE EVENTS")
    WindowPainter.draw_footer(c, ["SHAPE", "MIN", "MAX", "GATE", "PHASE"], highlight=3)

    step_width = W // StepCount
    loop_y = 16
    curve_y = 24
    curve_height = 20
    bottom_y = 48
    first_step, last_step = 0, 15

    shapes = [_ramp_up, _exp_up, _bell, _triangle, _ramp_down, _exp_down,
              _smooth_up, _high, _log_up, _ramp_up, _bell, _exp_down,
              _triangle, _ramp_down, _exp_up, _ramp_down]
    maxs = [255, 255, 255, 255, 200, 255, 255, 255, 220, 255, 255, 180, 255, 255, 255, 160]
    # gate event mask bits: Peak=1, Trough=2, ZeroRise=4, ZeroFall=8
    gates = [0, 1, 0, 3, 0, 0, 2, 0, 0, 1, 0, 0, 3, 0, 5, 0]

    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.Bright)
    SequencePainter.draw_loop_start(c, first_step * step_width + 1, loop_y, step_width - 2)
    SequencePainter.draw_loop_end(c, last_step * step_width + 1, loop_y, step_width - 2)

    c.set_color(Color.Low)
    for step_index in range(1, StepCount):
        x = step_index * step_width
        y = 0
        while y <= curve_height:
            c.point(x, curve_y + y)
            y += 2

    last_y = [-1.0]
    for i in range(StepCount):
        vmax = maxs[i] / 255.0
        x = i * step_width
        y = 20
        if i > first_step and i <= last_step:
            c.set_blend_mode(BlendMode.Set)
            c.set_color(Color.Bright)
            c.point(x, loop_y)
        c.set_blend_mode(BlendMode.Set)
        c.set_color(Color.Medium)
        s = str(i + 1)
        c.draw_text(x + (step_width - c.text_width(s) + 1) // 2, y - 2, s)
        c.set_color(Color.Bright)
        c.set_blend_mode(BlendMode.Add)
        _draw_curve(c, x, curve_y, step_width, curve_height, last_y, shapes[i], 0.0, vmax)
        # GATE layer: gate-event pattern at bottomY (line 476-480)
        c.set_color(Color.Bright)
        c.set_blend_mode(BlendMode.Set)
        _draw_gate_pattern(c, x, bottom_y, step_width, 2, gates[i])

    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.Bright)
    c.vline(int(3.4 * step_width), curve_y, curve_height)


# ---------------------------------------------------------------------------
# 2) Wavefolder mode. CurveSequenceEditPage.cpp:222-293
# ---------------------------------------------------------------------------
def render_wavefolder(c):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=2, play_pattern=0, edit_pattern=0, mode="STEPS")
    WindowPainter.draw_active_function(c, "WAVEFOLDER")
    WindowPainter.draw_footer(c, ["FOLD", "GAIN", "FILTER", "XFADE", "NEXT"], highlight=0)

    col_width = 51
    value_y = 26
    bar_y = 32
    bar_height = 4
    bar_width = 40

    # representative values; printed exactly as firmware formats them
    fold = 0.45          # %.2f
    gain = 1.20          # %.2f, max 2.0
    dj = -0.30           # %+.2f, bipolar max 1.0
    xfade = 0.80         # %.2f
    rows = 0             # FOLD selected (highlight)

    cols = [
        (fold, 1.0, False, f"{fold:.2f}"),
        (gain, 2.0, False, f"{gain:.2f}"),
        (dj, 1.0, True, f"{dj:+.2f}"),
        (xfade, 1.0, False, f"{xfade:.2f}"),
    ]

    for i in range(5):
        x = i * col_width
        bar_x = x + (col_width - bar_width) // 2
        if i < 4:
            value, vmax, bipolar, vstr = cols[i]
            c.set_font(Font.Tiny)
            c.set_color(Color.Bright if i == rows else Color.Medium)
            tw = c.text_width(vstr)
            c.draw_text(x + (col_width - tw) // 2, value_y, vstr)
            c.set_color(Color.Bright)
            if bipolar:
                center = bar_x + bar_width // 2
                if value > 0:
                    fw = int(value * bar_width / 2 / vmax)
                    c.fill_rect(center, bar_y, fw, bar_height)
                elif value < 0:
                    fw = int(-value * bar_width / 2 / vmax)
                    c.fill_rect(center - fw, bar_y, fw, bar_height)
                c.set_color(Color.Medium)
                c.vline(center, bar_y, bar_height)
            else:
                fw = int(value * bar_width / vmax)
                if fw > 0:
                    c.fill_rect(bar_x, bar_y, fw, bar_height)


# ---------------------------------------------------------------------------
# 3) Chaos mode. CurveSequenceEditPage.cpp:295-372
# ---------------------------------------------------------------------------
def _chaos_hz(rate):
    # mirror chaosHz() in CurveSequence.h:473
    n = rate / 127.0
    if n < 0.33:
        t = n / 0.33
        return 0.01 + t * 0.09
    elif n < 0.66:
        t = (n - 0.33) / 0.33
        return 0.1 + (t * t) * 1.9
    else:
        t = (n - 0.66) / 0.34
        return 2.0 + (t * t * t) * 48.0


def render_chaos(c):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=2, play_pattern=0, edit_pattern=0, mode="STEPS")
    WindowPainter.draw_active_function(c, "CHAOS")
    WindowPainter.draw_footer(c, ["AMT", "HZ", "HEAT", "DAMP", "NEXT"], highlight=0)

    col_width = 51
    value_y = 26
    bar_y = 32
    bar_height = 4
    bar_width = 40

    amount = 60      # %d%%, max 100
    rate = 65        # printed as Hz; max 127
    param1 = 45      # %d, max 100  (HEAT)
    param2 = 62      # %d, max 100  (DAMP)
    range_name = "Mid"
    chaos_row = 0    # AMT selected

    # range label under col 0 (line 307-312)
    c.set_font(Font.Tiny)
    c.set_color(Color.Medium)
    rx = 0 + (col_width - c.text_width(range_name)) // 2
    c.draw_text(rx, 44, range_name)

    # algo label under col 1 — Latoocarfian renders as two lines (line 315-321)
    line1, line2 = "Latoo-", "carfian"
    x1 = col_width + (col_width - c.text_width(line1)) // 2
    x2 = col_width + (col_width - c.text_width(line2)) // 2
    c.draw_text(x1, 44, line1)
    c.draw_text(x2, 50, line2)

    hz = _chaos_hz(rate)
    hz_str = (f"{hz:.2f}Hz" if hz < 1.0 else (f"{hz:.1f}Hz" if hz < 10.0 else f"{hz:.0f}Hz"))
    cols = [
        (amount, 100.0, f"{amount}%"),
        (rate, 127.0, hz_str),
        (param1, 100.0, f"{param1}"),
        (param2, 100.0, f"{param2}"),
    ]

    for i in range(4):
        x = i * col_width
        bar_x = x + (col_width - bar_width) // 2
        value, vmax, vstr = cols[i]
        c.set_font(Font.Tiny)
        c.set_color(Color.Bright if i == chaos_row else Color.Medium)
        tw = c.text_width(vstr)
        c.draw_text(x + (col_width - tw) // 2, value_y, vstr)
        c.set_color(Color.Bright)
        fw = int(value * bar_width / vmax)
        if fw > 0:
            c.fill_rect(bar_x, bar_y, fw, bar_height)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    outdir = os.path.join(here, "curve-manual")
    os.makedirs(outdir, exist_ok=True)
    pages = {
        "curve-step": render_step,
        "curve-step-gate": render_step_gate,
        "curve-wavefolder": render_wavefolder,
        "curve-chaos": render_chaos,
    }
    for slug, fn in pages.items():
        fb, c = _new()
        fn(c)
        framebuffer_to_image(fb, scale=4).save(os.path.join(outdir, f"{slug}.png"))
        print(f"saved {slug}")


if __name__ == "__main__":
    main()
