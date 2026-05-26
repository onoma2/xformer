#!/usr/bin/env python3
"""
Stochastic LIVE — Concept 2: Tidal Bloom

Each note is a droplet; ripples drift left and fade. Amplitude = pitch.
Rests = flat still water. Burst = harmonic inner rings. Wavefront connects peaks.

Inspired by spirals.lua (organic growth) + flora.lua (L-system branching) + dunes.lua (wave topography).
"""

import os
import sys
import math

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import WindowPainter, PageWidth

# ---------------------------------------------------------------------------
# Simulated engine history with time offsets
# ---------------------------------------------------------------------------
class TimedEvent:
    def __init__(self, cv, rest=False, children=0, t=0):
        self.cv = cv
        self.rest = rest
        self.children = children
        self.t = t

TIMED_EVENTS = [
    TimedEvent(1.2,  False, 0,  0),
    TimedEvent(-0.8, False, 2,  2),
    TimedEvent(2.1,  False, 0,  5),
    TimedEvent(0.0,  True,  0,  7),
    TimedEvent(-1.5, False, 0,  8),
    TimedEvent(0.9,  False, 3, 11),
    TimedEvent(1.8,  False, 0, 14),
    TimedEvent(-0.3, False, 1, 16),
    TimedEvent(0.5,  True,  0, 18),
    TimedEvent(-1.1, False, 0, 19),
    TimedEvent(1.4,  False, 2, 22),
    TimedEvent(0.2,  False, 0, 25),
]

NOW_T = 28

VP_LEFT   = 8
VP_RIGHT  = PageWidth - 8
VP_TOP    = 25
VP_BOT    = 53
VP_CY     = (VP_TOP + VP_BOT) // 2


def _ellipse_points(cx, cy, rx, ry, steps=24):
    pts = []
    for i in range(steps):
        t = 2 * math.pi * i / steps
        pts.append((int(cx + math.cos(t) * rx), int(cy + math.sin(t) * ry)))
    return pts


def render_live_tidal(canvas: Canvas):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LIVE")
    WindowPainter.draw_footer(canvas, ["LoopR", "LiveM", "NewR", "NewM", "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)

    # --- water surface baseline ---
    canvas.set_color(Color.Low)
    for x in range(VP_LEFT, VP_RIGHT + 1, 2):
        canvas.point(x, VP_CY)

    # --- draw each event's ripple as it would appear now ---
    for ev in reversed(TIMED_EVENTS):
        age = NOW_T - ev.t
        origin_x = VP_RIGHT - 4 - int(age * 3.5)
        if origin_x < VP_LEFT:
            continue
        amp = -ev.cv * 6  # pitch = splash height

        if ev.rest:
            # still water: tiny flat reflection
            canvas.set_color(Color.Low)
            for ox in range(-2, 3):
                canvas.point(origin_x + ox, VP_CY)
            continue

        fade = max(0.08, 1.0 - age / 14)
        # parent ripple (ellipse)
        rx = int(age * 2.5 + 2)
        ry = max(2, int(abs(amp) * 0.4 + 1.5))
        center_y = VP_CY + int(amp * 0.3)

        col = Color.Medium if fade > 0.3 else Color.Low
        canvas.set_color(col)
        pts = _ellipse_points(origin_x, center_y, rx, ry)
        for i in range(len(pts)):
            x0, y0 = pts[i]
            x1, y1 = pts[(i + 1) % len(pts)]
            canvas.line(x0, y0, x1, y1)

        # inner bright ring for newer
        if age < 3:
            canvas.set_color(Color.MediumBright)
            pts2 = _ellipse_points(origin_x, center_y, max(1, rx // 2), max(1, ry // 2))
            for i in range(len(pts2)):
                x0, y0 = pts2[i]
                x1, y1 = pts2[(i + 1) % len(pts2)]
                canvas.line(x0, y0, x1, y1)

        # peak dot
        peak_y = VP_CY + int(amp)
        canvas.set_color(Color.Bright if age < 2 else Color.MediumBright)
        canvas.point(origin_x, peak_y)

        # burst children = harmonic ripples
        for c in range(ev.children):
            cpx = origin_x + (c + 1) * 2
            cpy = center_y + (-2 if c % 2 == 0 else 2)
            crx = max(1, rx // 3 + c)
            cry = max(1, ry // 3 + c)
            canvas.set_color(Color.MediumLow)
            cpts = _ellipse_points(cpx, cpy, crx, cry, steps=12)
            for i in range(len(cpts)):
                x0, y0 = cpts[i]
                x1, y1 = cpts[(i + 1) % len(cpts)]
                canvas.line(x0, y0, x1, y1)

    # --- wavefront envelope connecting active peaks ---
    canvas.set_color(Color.Low)
    prev_x, prev_y = -1, -1
    for ev in TIMED_EVENTS:
        if ev.rest:
            continue
        age = NOW_T - ev.t
        x = VP_RIGHT - 4 - int(age * 3.5)
        if x < VP_LEFT:
            continue
        y = VP_CY + int(-ev.cv * 6)
        if prev_x >= 0:
            canvas.line(prev_x, prev_y, x, y)
        prev_x, prev_y = x, y


def main():
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)
    render_live_tidal(canvas)
    img = framebuffer_to_image(fb, scale=4)
    out = os.path.join(os.path.dirname(__file__), "kimi-live-tidal.png")
    img.save(out)
    print(f"Saved {out}")


if __name__ == '__main__':
    main()
