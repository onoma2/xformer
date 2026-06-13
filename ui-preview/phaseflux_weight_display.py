#!/usr/bin/env python3
"""PhaseFlux v0.3 weight-display candidates (U5b owner pick).

Two questions rendered against the real tiny5x5 / ati8x8 bitmaps:
  1. value-row format for the Weight slot — raw / share% / musical fraction
  2. grid-cell visual for a weight-0 (removed) cell, distinct from a skipped
     cell (X) and a normal cell (solid outline)

Run:  python3 ui-preview/phaseflux_weight_display.py
Out:  ui-preview/phaseflux-weight-display/weight-display.png
"""
import os
from canvas import FrameBuffer, Canvas, Color, Font, BlendMode, framebuffer_to_image
from PIL import Image

W, H = 256, 64

# Sample sequence: 4 active cells, weights 64/96/32/64 -> Sigma = 256, 1-bar period.
WEIGHTS = [64, 96, 32, 64]
SIGMA = sum(WEIGHTS)
SEL = 0  # selected cell -> weight 64

# Musical fraction of the 1-bar period for the selected cell's weight share.
def frac_label(w, sigma):
    # span = w/sigma of one bar; render as the nearest 1/n note fraction.
    from fractions import Fraction
    f = Fraction(w, sigma).limit_denominator(64)
    return f"{f.numerator}/{f.denominator}" if f != 1 else "1/1"

VALUE_FORMATS = [
    ("raw",     f"{WEIGHTS[SEL]}"),
    ("share%",  f"{round(WEIGHTS[SEL] * 100 / SIGMA)}%"),
    ("1/n bar", frac_label(WEIGHTS[SEL], SIGMA)),
]


def value_panel(canvas, x0, w, title, value):
    # Mimics the edit-page bottom param strip: small title, larger value below.
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.set_font(Font.Tiny)
    canvas.draw_text_aligned(x0, 13, w, 6, 'center', 'top', title)
    # the slot label "Wgt"
    canvas.set_color(Color.MediumLow)
    canvas.draw_text_aligned(x0, 24, w, 6, 'center', 'top', "Wgt")
    # the value, in the bigger font
    canvas.set_color(Color.Bright)
    canvas.set_font(Font.Small)
    canvas.draw_text_aligned(x0, 32, w, 10, 'center', 'top', value)


def cell(canvas, x, y, state):
    # 9x9 cell. state: 'normal' | 'dwelt' | 'zero' | 'skip'
    s = 9
    canvas.set_blend_mode(BlendMode.Set)
    if state == 'skip':
        canvas.set_color(Color.Low)
        canvas.draw_rect(x, y, s, s)
        canvas.line(x + 2, y + 2, x + s - 3, y + s - 3)
        canvas.line(x + s - 3, y + 2, x + 2, y + s - 3)
        return
    if state == 'zero':
        # Proposed weight-0 visual: dim corner ticks only (no full outline, no
        # pulse bar) -> reads as "present but no span", distinct from skip's X.
        canvas.set_color(Color.MediumLow)
        for (cx, cy) in ((x, y), (x + s - 1, y), (x, y + s - 1), (x + s - 1, y + s - 1)):
            canvas.point(cx, cy)
        return
    # normal / dwelt: solid outline + pulse bar; dwelt draws a taller bar.
    canvas.set_color(Color.MediumLow)
    canvas.draw_rect(x, y, s, s)
    canvas.set_color(Color.MediumBright)
    canvas.hline(x + 1, y + s - 2, 4 if state == 'normal' else 6)


def draw_rect(self, x, y, w, h):  # canvas has draw_rect? add if missing
    pass


def main():
    fb = FrameBuffer(W, H)
    c = Canvas(fb)
    # frame
    c.set_color(Color.Low)
    c.draw_rect(0, 0, W, H)

    # title
    c.set_color(Color.Bright)
    c.set_font(Font.Tiny)
    c.draw_text_aligned(0, 1, W, 6, 'center', 'top', "WEIGHT DISPLAY  (cell w=64 of Sigma=256, 1 bar)")

    # three value-format panels
    panel_w = W // 3
    for i, (title, value) in enumerate(VALUE_FORMATS):
        x0 = i * panel_w
        if i > 0:
            c.set_color(Color.Low)
            c.vline(x0, 11, 32)
        value_panel(c, x0, panel_w, title, value)

    # bottom legend: cell states
    c.set_color(Color.Low)
    c.hline(2, 45, W - 4)
    labels = [("normal", 'normal'), ("dwelt", 'dwelt'), ("w=0", 'zero'), ("skip", 'skip')]
    cx = 6
    for label, state in labels:
        cell(c, cx, 49, state)
        c.set_color(Color.Medium)
        c.set_font(Font.Tiny)
        c.draw_text(cx + 12, 55, label)
        cx += 12 + 7 * len(label) + 6

    out_dir = os.path.join(os.path.dirname(__file__), "phaseflux-weight-display")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "weight-display.png")
    img = framebuffer_to_image(fb, scale=4)
    img.save(out)
    print(f"Saved {out}")


if __name__ == '__main__':
    main()
