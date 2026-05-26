#!/usr/bin/env python3
"""
Stochastic LIVE — Concept 3: Mycelium Path

Events are fungal nodes; hyphae curve organically between them.
Pitch = branch direction. Rests = dead-end stubs. Burst = spore clusters.
Newest growth tip pulses with bioluminescence.

Inspired by flora.lua (L-system branching) + luck.lua (node topology) + less-concepts (cellular growth).
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
class NodeEvent:
    def __init__(self, cv, rest=False, children=0):
        self.cv = cv
        self.rest = rest
        self.children = children

NODE_EVENTS = [
    NodeEvent(1.2,  False, 0),
    NodeEvent(0.5,  False, 2),
    NodeEvent(1.8,  False, 0),
    NodeEvent(0.0,  True,  0),
    NodeEvent(-0.9, False, 0),
    NodeEvent(-1.4, False, 3),
    NodeEvent(0.3,  False, 0),
    NodeEvent(1.1,  False, 1),
    NodeEvent(0.0,  True,  0),
    NodeEvent(-0.6, False, 0),
    NodeEvent(0.8,  False, 2),
    NodeEvent(1.5,  False, 0),
    NodeEvent(-1.2, False, 0),
]

VP_LEFT   = 8
VP_RIGHT  = PageWidth - 8
VP_TOP    = 25
VP_BOT    = 53
VP_CY     = (VP_TOP + VP_BOT) // 2


def _quad_bezier(canvas, x0, y0, cx, cy, x1, y1, steps=12):
    """Draw quadratic bezier from (x0,y0) to (x1,y1) with control point (cx,cy)."""
    prev_x, prev_y = x0, y0
    for i in range(1, steps + 1):
        t = i / steps
        inv = 1 - t
        x = int(inv * inv * x0 + 2 * inv * t * cx + t * t * x1)
        y = int(inv * inv * y0 + 2 * inv * t * cy + t * t * y1)
        canvas.line(prev_x, prev_y, x, y)
        prev_x, prev_y = x, y


def render_live_mycelium(canvas: Canvas):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LIVE")
    WindowPainter.draw_footer(canvas, ["LoopR", "LiveM", "NewR", "NewM", "NEXT"])

    canvas.set_blend_mode(BlendMode.Set)

    # --- faint substrate grid ---
    canvas.set_color(Color.Low)
    for y in range(VP_TOP + 3, VP_BOT, 6):
        for x in range(VP_LEFT, VP_RIGHT + 1, 4):
            canvas.point(x, y)

    # --- build nodes ---
    stride = (VP_RIGHT - VP_LEFT - 10) // (len(NODE_EVENTS) - 1)
    nodes = []
    for i, ev in enumerate(NODE_EVENTS):
        px = VP_RIGHT - 4 - i * stride
        py = VP_CY - int(ev.cv * 6)
        py = max(VP_TOP + 2, min(VP_BOT - 2, py))
        nodes.append((px, py, ev, i))

    # --- draw hyphae (organic curves, oldest first) ---
    prev = None
    for px, py, ev, idx in reversed(nodes):
        if ev.rest:
            prev = None
            continue
        if prev is not None:
            ppx, ppy = prev
            # organic wobble midpoint
            mx = (ppx + px) // 2 + int(math.sin(idx * 1.7) * 2.5)
            my = (ppy + py) // 2 + int(math.cos(idx * 2.3) * 1.5)
            canvas.set_color(Color.Low)
            _quad_bezier(canvas, ppx, ppy, mx, my, px, py)
        prev = (px, py)

    # --- draw nodes ---
    for px, py, ev, idx in nodes:
        if ev.rest:
            # dead end stub
            canvas.set_color(Color.Low)
            canvas.line(px, py - 2, px, py + 2)
            continue

        # glow for newest
        if idx < 3:
            glow_r = 3 + idx
            canvas.set_color(Color.Low)
            for gx in range(-glow_r, glow_r + 1):
                for gy in range(-glow_r, glow_r + 1):
                    if abs(gx) + abs(gy) <= glow_r + 1:
                        canvas.point(px + gx, py + gy)

        # node core
        col = Color.Bright if idx == 0 else (Color.MediumBright if idx < 3 else Color.Medium)
        canvas.set_color(col)
        canvas.fill_rect(px - 1, py - 1, 3, 3)

        # spore cluster (burst children)
        for c in range(ev.children):
            angle = (c / max(1, ev.children)) * 2 * math.pi + idx * 0.5
            dist = 2.5 + c * 0.8
            sx = int(px + math.cos(angle) * dist)
            sy = int(py + math.sin(angle) * dist)
            canvas.set_color(Color.MediumLow)
            canvas.point(sx, sy)
            canvas.point(sx + 1, sy)
            canvas.point(sx, sy + 1)

    # --- newest growth tip: bioluminescent pulse ---
    tip_x, tip_y = nodes[0][0], nodes[0][1]
    canvas.set_color(Color.MediumBright)
    # crosshair ring
    for d in range(-3, 4):
        canvas.point(tip_x + d, tip_y)
        canvas.point(tip_x, tip_y + d)
    canvas.set_color(Color.Bright)
    canvas.point(tip_x, tip_y)


def main():
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)
    render_live_mycelium(canvas)
    img = framebuffer_to_image(fb, scale=4)
    out = os.path.join(os.path.dirname(__file__), "kimi-live-mycelium.png")
    img.save(out)
    print(f"Saved {out}")


if __name__ == '__main__':
    main()
