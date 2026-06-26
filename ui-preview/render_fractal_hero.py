#!/usr/bin/env python3
"""
Fractal Track OLED hero pages (proposals) — standard edit-page idiom.

  1) trunk    — captured loop as an ADAPTIVE TAPE (Stochastic-loop idiom):
                each cell a pitch-height bar, loop window highlighted, ornament
                zone band, playhead. step_w = avail/N -> scales 16..128 cells.
  2) branches — Trunk->B1..BN chain, Path route, transform pool (scales to 8)
  3) ornament — rate/intensity bars (tier ticks), scale/root, zone, last-fired

Three-stop F5=NEXT hero ring (Trunk -> Branches -> Ornament).
Outputs to ui-preview/fractal-hero/.
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, framebuffer_to_image
from painters import WindowPainter

W, H = 256, 64


def _new():
    fb = FrameBuffer(W, H)
    c = Canvas(fb, brightness=1.0)
    c.set_font(Font.Tiny)
    return fb, c


def _tt(c, x, ytop, text, color=Color.Bright):
    c.set_font(Font.Tiny)
    c.set_color(color)
    c.draw_text(x, ytop + 5, text)


def _tc(c, x, ytop, w, text, color=Color.Bright):
    c.set_font(Font.Tiny)
    c.set_color(color)
    c.draw_text(x + (w - c.text_width(text)) // 2, ytop + 5, text)


def _tr(c, xr, ytop, text, color=Color.Bright):
    c.set_font(Font.Tiny)
    c.set_color(color)
    c.draw_text(xr - c.text_width(text), ytop + 5, text)


def _bar(c, x, ytop, w, h, frac, ticks=()):
    c.set_color(Color.Low)
    c.draw_rect(x, ytop, w, h)
    fw = max(1, int((w - 2) * max(0.0, min(1.0, frac))))
    c.set_color(Color.Bright)
    c.fill_rect(x + 1, ytop + 1, fw, h - 2)
    for t in ticks:
        tx = x + 1 + int((w - 2) * t)
        c.set_color(Color.MediumLow)
        c.vline(tx, ytop, h)


# ---------------------------------------------------------------------------
# 1) Trunk — adaptive tape (Stochastic-loop idiom). Scales 16..128 cells.
# ---------------------------------------------------------------------------
def render_trunk(c, N=16):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=5, mode="FRACTAL")
    WindowPainter.draw_active_function(c, "LOOP")
    WindowPainter.draw_footer(c, ["REC", "LOOP", "ORN", "DIV", "NEXT"], highlight=1)

    loop_first, loop_last = int(N * 0.00), int(N * 0.75) - 1
    orn_first, orn_last = int(N * 0.25), int(N * 0.50) - 1
    cur = int(N * 0.34)

    tape_top, tape_bot = 22, 44
    tape_h = tape_bot - tape_top + 1
    margin = 4
    step_w = max(1, (W - 2 * margin) // N)
    used = step_w * N
    x0 = (W - used) // 2

    # tape outline
    c.set_color(Color.Low)
    c.draw_rect(x0, tape_top, used, tape_h)

    for i in range(N):
        x = x0 + i * step_w
        rest = (i % 8 == 7)
        in_loop = loop_first <= i <= loop_last
        is_cur = (i == cur)
        if rest:
            continue
        pitch = 0.5 + 0.45 * math.sin(i * (4.2 / N) * 3)
        bar_h = max(1, int((tape_h - 3) * max(0.05, min(1.0, pitch))))  # height = pitch
        gl = 15 if (i % 5 == 0) else (3 if (i % 7 == 3) else 9)         # gate length 0..15
        bw = max(1, step_w * gl // 15)                                  # width = gate length
        col = Color.Bright if is_cur else (Color.MediumBright if in_loop else Color.Low)
        c.set_color(col)
        c.fill_rect(x, tape_bot - bar_h, bw, bar_h)

    # ornament eligibility zone — band above the tape
    c.set_color(Color.MediumBright)
    c.hline(x0 + orn_first * step_w, tape_top - 3, (orn_last - orn_first + 1) * step_w)
    # loop window — bracket ticks on the tape top edge
    c.set_color(Color.Bright)
    c.vline(x0 + loop_first * step_w, tape_top - 2, 4)
    c.vline(x0 + (loop_last + 1) * step_w - 1, tape_top - 2, 4)
    # playhead
    c.set_color(Color.Bright)
    c.vline(x0 + cur * step_w + step_w // 2, tape_top, tape_h)

    _tt(c, 2, 11, f"{N}c", Color.Medium)
    _tr(c, 254, 11, f"orn {orn_first}-{orn_last}", Color.Medium)
    _tt(c, 90, 46, f"loop {loop_first}-{loop_last}", Color.Medium)


# ---------------------------------------------------------------------------
# 2) Branches — Trunk->B1..BN chain, Path route, transform pool (to 8)
# ---------------------------------------------------------------------------
def render_branches(c, N=3):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=5, mode="FRACTAL")
    WindowPainter.draw_active_function(c, "BRANCH")
    WindowPainter.draw_footer(c, ["CNT", "PATH", "POOL", "SEED", "NEXT"], highlight=0)

    routes = {3: "T>B1>B3>B2", 7: "T>B1>B2>B5>B7>B6>B4>B3"}
    _tt(c, 2, 12, "Route", Color.Medium)
    c.set_color(Color.Bright)
    c.draw_text(40, 17, routes.get(N, "T>B1.."))
    _tr(c, 254, 12, f"N={N}", Color.MediumBright)

    segs = ["T"] + [f"B{j}" for j in range(1, N + 1)]
    tab = (["raw", "Rot+3", "Exp x2", "Rev"] if N <= 3
           else ["raw", "Rv", "Tr3", "Ex2", "In", "RI", "Ro", "Gt"])
    cols = len(segs)
    bw = W // cols
    y = 26
    for j, s in enumerate(segs):
        x = j * bw
        on = (j == 0)
        c.set_color(Color.Bright if on else Color.Medium)
        c.draw_rect(x + 1, y, bw - 2, 10)
        _tc(c, x + 1, y + 1, bw - 2, s, Color.Bright if on else Color.MediumBright)
        _tc(c, x, y + 12, bw, tab[j], Color.MediumBright if on else Color.Medium)

    pool = ["Tr", "Rv", "In", "RI", "Ro", "Co", "Ex", "Gt"]
    enabled = [1, 1, 1, 1, 1, 1, 1, 0]
    pw = W // 8
    py = 45
    for k, (name, en) in enumerate(zip(pool, enabled)):
        x = k * pw
        c.set_color(Color.Bright if en else Color.Low)
        c.draw_rect(x + 1, py, 6, 6)
        if en:
            c.fill_rect(x + 2, py + 1, 4, 4)
        _tt(c, x + 10, py, name, Color.MediumBright if en else Color.Low)


# ---------------------------------------------------------------------------
# 3) Ornament — rate/intensity bars (tier ticks), scale/root, zone, last
# ---------------------------------------------------------------------------
def render_ornament(c):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=5, mode="FRACTAL")
    WindowPainter.draw_active_function(c, "ORN")
    WindowPainter.draw_footer(c, ["RATE", "INT", "SCALE", "ZONE", "NEXT"], highlight=0)

    _tt(c, 2, 12, "Rate", Color.Medium)
    _bar(c, 40, 13, 96, 6, 0.35)
    _tr(c, 254, 12, "35%", Color.MediumBright)

    _tt(c, 2, 23, "Int", Color.Medium)
    _bar(c, 40, 24, 96, 6, 0.60, ticks=(0.40, 0.75))
    _tr(c, 254, 23, "60% 4-step", Color.MediumBright)

    _tt(c, 2, 34, "Scale", Color.Medium)
    c.set_color(Color.Bright)
    c.draw_text(40, 39, "C Major")
    _tr(c, 254, 34, "zone 4-11", Color.MediumBright)

    _tt(c, 2, 45, "Last", Color.Medium)
    c.set_color(Color.Bright)
    c.draw_text(40, 50, "Mordent ^")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    outdir = os.path.join(here, "fractal-hero")
    os.makedirs(outdir, exist_ok=True)
    # clear obsolete renders
    for stale in ("trunk-64.png",):
        p = os.path.join(outdir, stale)
        if os.path.exists(p):
            os.remove(p)
    pages = {
        "trunk": lambda c: render_trunk(c, 16),
        "trunk-64c": lambda c: render_trunk(c, 64),
        "trunk-128c": lambda c: render_trunk(c, 128),
        "branches": lambda c: render_branches(c, 3),
        "branches-8": lambda c: render_branches(c, 7),
        "ornament": render_ornament,
    }
    for slug, fn in pages.items():
        fb, c = _new()
        fn(c)
        framebuffer_to_image(fb, scale=4).save(os.path.join(outdir, f"{slug}.png"))
        print(f"saved {slug}")


if __name__ == "__main__":
    main()
