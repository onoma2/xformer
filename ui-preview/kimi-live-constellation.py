#!/usr/bin/env python3
"""
Stochastic LIVE — Concept 1: Constellation Draft

Each engine event becomes a star. Pitch = altitude, time = longitude.
Consecutive phrases draw constellations. Newest star has targeting reticle + glow.
Rests = hollow ghost stars. Burst = companion clusters.

Inspired by constellations.lua (star glow + connection lines) + luck.lua (node topology).
"""

import os
import sys
import math

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import WindowPainter, PageWidth

# ---------------------------------------------------------------------------
# Simulated engine history
# ---------------------------------------------------------------------------
class DirectEvent:
    def __init__(self, cv, rest=False, children=0):
        self.cv = cv
        self.rest = rest
        self.children = children

DIRECT_HISTORY = [
    DirectEvent(1.2,  False, 0),
    DirectEvent(0.5,  False, 2),
    DirectEvent(1.8,  False, 0),
    DirectEvent(0.0,  True,  0),
    DirectEvent(-0.9, False, 0),
    DirectEvent(-1.4, False, 3),
    DirectEvent(0.3,  False, 0),
    DirectEvent(1.1,  False, 1),
    DirectEvent(0.0,  True,  0),
    DirectEvent(-1.1, False, 0),
    DirectEvent(1.4,  False, 2),
    DirectEvent(0.2,  False, 0),
    DirectEvent(-0.7, False, 0),
    DirectEvent(1.0,  False, 1),
    DirectEvent(0.0,  True,  0),
    DirectEvent(-1.2, False, 0),
]

# ---------------------------------------------------------------------------
# Render
# ---------------------------------------------------------------------------
VP_LEFT   = 8
VP_RIGHT  = PageWidth - 8
VP_TOP    = 25
VP_BOT    = 53
VP_CY     = (VP_TOP + VP_BOT) // 2
VP_H      = VP_BOT - VP_TOP


def _arc(canvas, cx, cy, r, start, end, color):
    """Draw a circular arc using Bresenham-like point sampling."""
    canvas.set_color(color)
    steps = max(8, int(r * 3))
    for i in range(steps + 1):
        t = start + (end - start) * i / steps
        x = int(cx + math.cos(t) * r)
        y = int(cy + math.sin(t) * r)
        canvas.point(x, y)


def render_live_constellation(canvas: Canvas):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LIVE")
    WindowPainter.draw_footer(canvas, ["LoopR", "LiveM", "NewR", "NewM", "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)

    # --- cosmic dust ---
    for i in range(140):
        x = VP_LEFT + (i * 37 + 13) % (VP_RIGHT - VP_LEFT)
        y = VP_TOP  + (i * 23 + 7)  % (VP_BOT - VP_TOP)
        if (x + y * 3) % 11 < 2:
            continue
        canvas.set_color(Color.Low)
        canvas.point(x, y)

    # --- pitch guide rails (dashed) ---
    canvas.set_color(Color.Low)
    for r in range(-2, 3):
        y = VP_CY + r * 5
        if VP_TOP + 1 <= y <= VP_BOT - 1:
            # dashed line: draw every 3rd pixel
            for x in range(VP_LEFT, VP_RIGHT + 1, 3):
                canvas.point(x, y)

    # --- build star positions ---
    events = DIRECT_HISTORY
    cx = VP_RIGHT - 6
    stride = (VP_RIGHT - VP_LEFT - 12) // (len(events) - 1) if len(events) > 1 else 0

    stars = []
    for idx, ev in enumerate(events):
        px = cx - idx * stride
        py = VP_CY - int(ev.cv * 5)
        py = max(VP_TOP + 2, min(VP_BOT - 2, py))
        stars.append((px, py, ev, idx))

    # --- constellation lines (oldest first so newer overdraw) ---
    prev_x, prev_y = -1, -1
    for px, py, ev, idx in reversed(stars):
        if ev.rest:
            prev_x, prev_y = -1, -1
            continue
        if prev_x >= 0:
            canvas.set_color(Color.Low)
            canvas.line(prev_x, prev_y, px, py)
        prev_x, prev_y = px, py

    # --- draw stars ---
    for px, py, ev, idx in stars:
        fade = max(0.25, 1.0 - idx / 20)

        if ev.rest:
            # hollow ghost star
            canvas.set_color(Color.Low)
            _arc(canvas, px, py, 1.5, 0, 2 * math.pi, Color.Low)
            continue

        # glow halo for newer stars
        if idx < 4:
            glow_r = 2 + idx
            glow_col = Color.Low if idx >= 2 else Color.MediumLow
            _arc(canvas, px, py, glow_r, 0, 2 * math.pi, glow_col)

        # core star
        col = Color.Bright if idx == 0 else (Color.MediumBright if idx < 3 else Color.Medium)
        canvas.set_color(col)
        canvas.fill_rect(px - 1, py - 1, 3, 3)
        if idx == 0:
            canvas.set_color(Color.Bright)
            canvas.point(px, py)

        # burst children = companion stars
        if ev.children > 0:
            for c in range(ev.children):
                cpx = px + (c + 1) * 3
                cpy = py + (-2 if c % 2 == 0 else 2)
                if cpx >= VP_RIGHT - 1:
                    break
                canvas.set_color(Color.MediumLow)
                canvas.point(cpx, cpy)

    # --- targeting reticle on newest ---
    tip = stars[0]
    tx, ty = tip[0], tip[1]
    canvas.set_color(Color.MediumBright)
    canvas.line(tx - 4, ty, tx + 4, ty)
    canvas.line(tx, ty - 4, tx, ty + 4)


def main():
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)
    render_live_constellation(canvas)
    img = framebuffer_to_image(fb, scale=4)
    out = os.path.join(os.path.dirname(__file__), "kimi-live-constellation.png")
    img.save(out)
    print(f"Saved {out}")


if __name__ == '__main__':
    main()
