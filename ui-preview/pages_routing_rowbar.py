"""
Proposed: per-row inline bipolar depth bar.

The TeletypeScriptView bipolar bar rotated 90deg (horizontal) and placed in the dead
space between a param row's NAME (left) and its VALUE (right). Each routed row shows
its depth as a bipolar bar: centre tick = base/neutral, fill right = +, fill left = -.
No separate lane, no vertical-room problem — it lives in space already wasted.

Bottom strip stays the letter chips (scope/membership); depth now reads per-row.
"""

from canvas import BlendMode, Canvas, Color, Font
from pages_routing_tabscope import _header, _tabs, _strip, _combine, _footer

PW = 256
PH = 64

ROW_CX = 46
LIST_TOP = 15
ROW_H = 6
DEPTH_R = PW - 4

BAR_L = 104
BAR_R = 222
BAR_C = (BAR_L + BAR_R) // 2
BAR_HALF = BAR_C - BAR_L

# (name, depth%  | None = unrouted)
ROWS = [("Scale", None), ("Root Note", None), ("Transpose", 60), ("Octave", -30)]


def _rows_bars(canvas: Canvas, cursor_row: int):
    canvas.set_font(Font.Tiny)
    for i, (name, depth) in enumerate(ROWS):
        y = LIST_TOP + i * ROW_H
        cursor = i == cursor_row
        routed = depth is not None
        if cursor:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(ROW_CX - 2, y - 1, PW - ROW_CX - 2, ROW_H)
            canvas.set_blend_mode(BlendMode.Sub)
        canvas.set_color(Color.Bright if routed else Color.Low)
        canvas.draw_text(ROW_CX, y + 4, name)

        if routed:
            # faint bipolar field + centre tick
            canvas.set_color(Color.Low)
            canvas.hline(BAR_L, y + 2, BAR_R - BAR_L)
            canvas.vline(BAR_C, y, 5)
            # fill
            span = int(abs(depth) / 100 * BAR_HALF)
            canvas.set_color(Color.Bright if not cursor else Color.MediumBright)
            if depth >= 0:
                if span > 0:
                    canvas.fill_rect(BAR_C, y + 1, span, 3)
            else:
                if span > 0:
                    canvas.fill_rect(BAR_C - span, y + 1, span, 3)
            # value
            dep = f"{depth:+d}%"
            canvas.set_color(Color.Medium)
            canvas.draw_text(DEPTH_R - canvas.text_width(dep), y + 4, dep)
        else:
            hint = "+ADD" if cursor else "--"
            canvas.set_color(Color.Medium if cursor else Color.Low)
            canvas.draw_text(DEPTH_R - canvas.text_width(hint), y + 4, hint)

        if cursor:
            canvas.set_blend_mode(BlendMode.Set)


def render_routing_rowbar(canvas: Canvas, scenario: int = 1):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()

    engines = ["N"] * 8
    mask = (1 << 1) | (1 << 3) | (1 << 6)   # T2 T4 T7

    _header(canvas, "TRANSPOSE")
    _tabs(canvas, 0)
    _rows_bars(canvas, cursor_row=2)
    _strip(canvas, engines, mask, None, False)
    _combine(canvas, mask, False)
    _footer(canvas)
