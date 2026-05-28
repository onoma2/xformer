"""
Window-size preview — show Bounce and Exp curves at sizes 100 / 90 / 80 / 70.

Window math (symmetric center-shrink): both input AND output of the curve are
mapped to an inner square of side = size%. Outside the window on the input
axis, the curve clamps to its boundary value.

  x_curve = clamp((x_display - winLow) / size, 0, 1)
  y_curve = curve(x_curve)
  y_display = y_curve * size + winLow
"""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, WindowPainter


import math


def bell(x: float) -> float:
    # C++ Curve::Bell — 0.5 - 0.5 * cos(2π·x), full period, returns to 0 at x=1.
    return 0.5 - 0.5 * math.cos(2.0 * math.pi * x)


def _exp_down(x: float) -> float:
    return (1.0 - x) * (1.0 - x)


def bounce(x: float) -> float:
    # C++ Curve::ExpDown3x — three cascading exp drops, the actual PhaseFlux Bounce.
    if x >= 1.0:
        return 0.0
    return _exp_down((3.0 * x) % 1.0)


def is_in_window(x_display: float, mode: str, size_pct: int) -> bool:
    """Window modes:
        Off         — full curve visible.
        Focus N%    — center N% kept; engine evaluates middle, outer hidden.
        Polarize N% — outer N% kept (N/2 each side); engine evaluates edges,
                      middle hidden.
    """
    if mode == "Off":
        return True
    if mode == "Focus":
        half = (size_pct / 100.0) / 2.0
        return (0.5 - half) <= x_display <= (0.5 + half)
    # Polarize
    band_w = (size_pct / 100.0) / 2.0
    return x_display <= band_w or x_display >= (1.0 - band_w)


def eval_pipeline(curve_fn, x_display: float, repeat: int, mode: str,
                  size_pct: int, order: str):
    """Returns (visible, y) — visible=False means the engine doesn't evaluate
    at this x (skip drawing). order = "RW" (Repeat→Window) or "WR" (Window→Repeat)."""
    if order == "RW":
        # Window check on original x; repeat applies inside.
        if not is_in_window(x_display, mode, size_pct):
            return False, 0.0
        x_curve = (x_display * repeat) % 1.0
        return True, curve_fn(x_curve)
    # WR: window check happens on the REPEATED domain.
    x_repeated = (x_display * repeat) % 1.0
    if not is_in_window(x_repeated, mode, size_pct):
        return False, 0.0
    return True, curve_fn(x_repeated)


def draw_panel(canvas: Canvas, x0: int, y0: int, w: int, h: int,
               curve_fn, repeat: int, mode: str, size_pct: int,
               order: str, label: str):
    # Outline.
    canvas.set_color(Color.Low)
    canvas.draw_rect(x0, y0, w, h)
    # Curve trace.
    canvas.set_color(Color.Bright)
    prev_py = None
    prev_in = False
    for px in range(w - 2):
        x_disp = px / float(w - 3)
        visible, y_curve = eval_pipeline(curve_fn, x_disp, repeat, mode,
                                         size_pct, order)
        cx = x0 + 1 + px
        if not visible:
            prev_in = False
            continue
        py = y0 + h - 2 - int(y_curve * (h - 3))
        if prev_in:
            canvas.line(cx - 1, prev_py, cx, py)
        prev_py = py
        prev_in = True
    # Label — top-left of panel, inside.
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.MediumBright)
    canvas.draw_text(x0 + 2, y0 + 6, label)


def render(canvas: Canvas):
    WindowPainter.clear(canvas)

    # Compare both pipeline orders with Repeat 3 + Window F50/P50.
    # RW = Repeat then Window (current spec). WR = Window then Repeat.
    settings = [
        ("F50", "Focus",    50, "RW"),
        ("F50", "Focus",    50, "WR"),
        ("P50", "Polarize", 50, "RW"),
        ("P50", "Polarize", 50, "WR"),
    ]
    curves = [(bell, "Bell"), (bounce, "Bnc")]
    repeat = 3

    panel_w = 60
    panel_h = 28
    gap_x = 2
    gap_y = 4
    margin_x = 4
    margin_y = 2

    for row, (curve_fn, curve_name) in enumerate(curves):
        for col, (win_tag, mode, size, order) in enumerate(settings):
            x0 = margin_x + col * (panel_w + gap_x)
            y0 = margin_y + row * (panel_h + gap_y)
            label = f"{curve_name} R3-{win_tag}{order}"
            draw_panel(canvas, x0, y0, panel_w, panel_h,
                       curve_fn, repeat, mode, size, order, label)


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "window-preview")
    os.makedirs(out_dir, exist_ok=True)
    fb = FrameBuffer(PageWidth, PageHeight)
    c = Canvas(fb)
    render(c)
    img = framebuffer_to_image(fb, scale=4)
    path = os.path.join(out_dir, "window-order-compare.png")
    img.save(path)
    print(f"wrote {path}")


if __name__ == "__main__":
    main()
