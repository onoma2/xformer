#!/usr/bin/env python3
"""
Harmony Track OLED preview pages (proposals) — tidy / standard edit-page idiom.

  1) harmony-scale    — scale-degree availability (12-degree toggle row)
  2) harmony-controls — routable panel controls (aligned param rows)
  3) harmony-steps    — per-step recipe overrides (note-edit style grid)
  4) harmony-setup    — track setup (key/value list)

Standalone. Outputs to ui-preview/harmony/.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, BlendMode, framebuffer_to_image
from painters import WindowPainter

W, H = 256, 64

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
IN_SCALE   = [True, False, True, False, True, True, False, True, False, True, False, True]
AUTO_QUAL  = ["M7", "M7", "m7", "M7", "m7", "M7", "o7", "7", "M7", "m7", "7", "h7"]


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


# Scale LUT (mirrors model/Scale.cpp: note values / 128 = semitone index)
SCALE_LUT = {
    "Major":      {0, 2, 4, 5, 7, 9, 11},
    "Minor":      {0, 2, 3, 5, 7, 8, 10},
    "Maj Pent.":  {0, 2, 4, 7, 9},
    "Whole Tone": {0, 2, 4, 6, 8, 10},
}


# ---------------------------------------------------------------------------
# 1) Scale-degree availability — LUT-backed scale selector + degree preview
# ---------------------------------------------------------------------------
def render_harmony_scale(c, root=0, scale_name="Major", selected=2):
    members = [i in SCALE_LUT[scale_name] for i in range(12)]
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=4, mode="HARMONY")
    WindowPainter.draw_active_function(c, "SCALE")
    WindowPainter.draw_footer(c, ["KEY", "SCALE", "MODE", "DIATON", ""], highlight=1)

    cnt = sum(1 for e in members if e)
    # selector line: KEY <root>   SCALE <name>   <count>
    _tt(c, 4, 12, "KEY", Color.Medium)
    _tt(c, 26, 12, NOTE_NAMES[root], Color.Bright)
    _tt(c, 56, 12, "SCALE", Color.Medium)
    nx = 90
    c.set_color(Color.Bright)
    c.draw_rect(nx - 3, 11, c.text_width(scale_name) + 6, 9)   # edit focus box
    _tt(c, nx, 12, scale_name, Color.Bright)
    _tt(c, W - 4 - c.text_width(f"{cnt} notes"), 12, f"{cnt} notes", Color.Medium)

    # degree preview row (reflects the selected scale)
    n = 12
    cw = (W - 4) // n
    x0 = 2
    for i in range(n):
        x = x0 + i * cw
        on = members[i]
        is_root = (i == root)
        is_sel = (i == selected)
        _tc(c, x, 24, cw, NOTE_NAMES[i], Color.Bright if on else Color.Low)
        bx, bw = x + (cw - 13) // 2, 13
        if on:
            c.set_color(Color.Bright if is_root else Color.Medium)
            c.fill_rect(bx, 35, bw, 4)
        else:
            c.set_color(Color.Low)
            c.hline(bx, 37, bw)
        if is_root:
            c.set_color(Color.Bright)
            c.fill_rect(x + cw // 2 - 1, 22, 2, 2)
        if is_sel:
            c.set_color(Color.Bright)
            c.draw_rect(x, 23, cw - 1, 19)


# ---------------------------------------------------------------------------
# 2) Routable panel controls — aligned param rows
# ---------------------------------------------------------------------------
def render_harmony_controls(c, selected=0, routed=(1, 3)):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=4, mode="HARMONY")
    WindowPainter.draw_active_function(c, "CTRL")
    WindowPainter.draw_footer(c, ["TRSP", "INV", "VOIC", "QUAL", "SLEW"], highlight=selected)

    lx = 4          # label column
    bx, bw = 56, 150  # bar
    vx = W - 40       # value column (left edge)
    rx = W - 8        # route dot
    rows = [
        ("TRSP", "+3",   0.62, True),
        ("INV",  "1st",  0.33, False),
        ("VOIC", "drop2", 0.33, False),
        ("QUAL", "auto", -1.0, False),
        ("SLEW", "",     0.0,  False),
    ]
    for i, (lbl, val, fill, bip) in enumerate(rows):
        y = 13 + i * 8
        sel = (i == selected)
        lc = Color.Bright if sel else Color.Medium
        _tt(c, lx, y, lbl, lc)

        if lbl == "SLEW":
            # four mini bars R/3/5/7
            sl = ["R", "3", "5", "7"]
            sv = [0.2, 0.5, 0.85, 0.35]
            seg = (rx - bx + 6) // 4
            for j in range(4):
                sx = bx + j * seg
                _tt(c, sx, y, sl[j], Color.Medium)
                c.set_color(Color.Low); c.hline(sx + 7, y + 3, seg - 14)
                c.set_color(Color.MediumBright)
                c.hline(sx + 7, y + 3, max(1, int((seg - 14) * sv[j])))
            continue

        # track
        c.set_color(Color.Low)
        c.hline(bx, y + 3, bw)
        if fill < 0:  # AUTO = dashed faint track
            c.set_color(Color.MediumLow)
            for xx in range(bx, bx + bw, 4):
                c.point(xx, y + 3)
        elif bip:
            mid = bx + bw // 2
            c.set_color(Color.MediumLow); c.vline(mid, y + 2, 3)
            end = bx + int(bw * fill)
            c.set_color(Color.Bright if sel else Color.Medium)
            lo, hi = (mid, end) if end >= mid else (end, mid)
            c.hline(lo, y + 3, hi - lo + 1)
        else:
            end = bx + int(bw * fill)
            c.set_color(Color.Bright if sel else Color.Medium)
            c.hline(bx, y + 3, max(1, end - bx))

        # value
        _tt(c, vx, y, val, Color.Bright if sel else Color.Medium)
        # route dot (filled = routed)
        if i in routed:
            c.set_color(Color.Bright); c.fill_rect(rx, y + 1, 4, 4)
        else:
            c.set_color(Color.Low); c.draw_rect(rx, y + 1, 4, 4)


# ---------------------------------------------------------------------------
# 3) Per-step recipe overrides — note-edit style grid
# ---------------------------------------------------------------------------
def render_harmony_steps(c, layer="QUAL", current=5, selected=(5, 6)):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=4, mode="HARMONY")
    WindowPainter.draw_active_function(c, "STEP  QUAL")
    WindowPainter.draw_footer(c, ["QUAL", "INV", "VOIC", "TRSP", "REST"], highlight=0)

    qual = [".", ".", "m7", ".", "M7", "7", "M7", ".", "h7", ".", ".", ".", "7", ".", "m7", "M7"]
    rest = {3, 11}
    cell = 16
    for i in range(16):
        x = i * cell
        is_cur = (i == current)
        is_sel = (i in selected)
        is_rest = (i in rest)
        v = qual[i]
        override = (v != ".")
        bright = is_cur or is_sel

        if is_sel:
            c.set_color(Color.MediumLow); c.fill_rect(x + 1, 13, cell - 2, 30)
        if is_cur:
            c.set_color(Color.Bright); c.draw_rect(x + 1, 13, cell - 2, 30)

        # gate / rest square on top
        gx, gy = x + cell // 2 - 4, 17
        if is_rest:
            c.set_color(Color.Medium); c.draw_rect(gx, gy, 8, 8)
            c.hline(gx + 1, gy + 4, 6)
        else:
            c.set_color(Color.Bright if bright else Color.Medium)
            c.fill_rect(gx, gy, 8, 8)

        # value below
        if is_rest:
            _tc(c, x, 32, cell, "rst", Color.MediumLow)
        elif override:
            _tc(c, x, 31, cell, v, Color.Bright if bright else Color.MediumBright)
        else:
            _tc(c, x, 32, cell, "-", Color.Low)

    # bank separators
    c.set_color(Color.Low)
    for b in (4, 8, 12):
        c.vline(b * cell, 13, 30)


# ---------------------------------------------------------------------------
# 4) Setup — key/value list
# ---------------------------------------------------------------------------
def render_harmony_setup(c, selected=0):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=4, mode="HARMONY")
    WindowPainter.draw_active_function(c, "SETUP")
    WindowPainter.draw_footer(c, ["SRC", "KEY", "TRIG", "OUT", "STRUM"], highlight=-1)

    rows = [
        ("SOURCE", "Track 1"),
        ("KEY/SCALE", "C  Ionian"),
        ("TRIGGER", "Delta (auto)"),
        ("OUT MAP", "V1-4 > CV 1-4"),
        ("STRUM", "Up  12t"),
    ]
    y, rowh = 13, 8
    for i, (k, v) in enumerate(rows):
        sel = (i == selected)
        if sel:
            c.set_color(Color.MediumLow); c.fill_rect(0, y - 1, W, rowh - 1)
        _tt(c, 4, y, k, Color.MediumBright if sel else Color.Medium)
        c.set_font(Font.Tiny)
        c.set_color(Color.Bright if sel else Color.Medium)
        c.draw_text(W - 4 - c.text_width(v), y + 5, v)
        y += rowh


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    outdir = os.path.join(here, "harmony")
    os.makedirs(outdir, exist_ok=True)
    pages = {
        "harmony-scale": lambda c: render_harmony_scale(c, scale_name="Major"),
        "harmony-scale-whole": lambda c: render_harmony_scale(c, scale_name="Whole Tone", selected=4),
        "harmony-controls": render_harmony_controls,
        "harmony-steps": render_harmony_steps,
        "harmony-setup": render_harmony_setup,
    }
    for slug, fn in pages.items():
        fb, c = _new()
        fn(c)
        framebuffer_to_image(fb, scale=4).save(os.path.join(outdir, f"{slug}.png"))
        print(f"saved {slug}")


if __name__ == "__main__":
    main()
