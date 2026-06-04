"""
Proposed: the tab editor's bottom track lane drawn as TeletypeScriptView-style
bipolar bars (drawBipolarBar idiom, TeletypeScriptViewPage.cpp:299) laid out
horizontally — one cell per track.

Each cell: present bar = the track is a member of the focused route; bar height +
direction = that track's depth (bipolar, up=+, down=-). Non-member = empty outline.
This folds scope (which tracks) AND per-track spread (each track's depth) into one
lane — the spread is visible inline, no separate Page+S5 needed to *see* it.
"""

from canvas import BlendMode, Canvas, Color, Font
from pages_routing_tabscope import _header, _tabs, _rows

PW = 256
PH = 64

LANE_L = 46
LANE_R = 253
LANE_TOP = 40
LANE_H = 13


def _lane(canvas: Canvas, engines, depths, mask):
    slot_w = (LANE_R - LANE_L) // 8
    center_y = LANE_TOP + LANE_H // 2
    canvas.set_font(Font.Tiny)
    for t in range(8):
        base_x = LANE_L + t * slot_w
        cx = base_x + slot_w // 2
        member = bool(mask & (1 << t))
        # cell outline
        canvas.set_color(Color.Low)
        canvas.draw_rect(cx - 5, LANE_TOP, 10, LANE_H)
        # centre baseline
        canvas.hline(cx - 5, center_y, 10)
        if member:
            d = depths[t]
            h = int(abs(d) / 100 * (LANE_H // 2 - 1))
            canvas.set_color(Color.Bright)
            if d >= 0:
                if h > 0:
                    canvas.fill_rect(cx - 3, center_y - h, 6, h)
            else:
                if h > 0:
                    canvas.fill_rect(cx - 3, center_y + 1, 6, h)


def _footer(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["BACK", "COMBINE", "SRC", "", ""]
    for i in range(1, 5):
        canvas.vline((PW * i) // 5, PH - 10, 10)
    canvas.set_font(Font.Tiny)
    for i, name in enumerate(names):
        if not name:
            continue
        x0 = (PW * i) // 5
        x1 = (PW * (i + 1)) // 5
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)


def render_routing_tablane(canvas: Canvas, scenario: int = 1):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()

    # Transpose with a per-track spread: T2 +60, T4 +30, T7 -40
    engines = ["N"] * 8
    depths = [0, 60, 0, 30, 0, 0, -40, 0]
    mask = (1 << 1) | (1 << 3) | (1 << 6)
    band, cursor_row, active = 0, 2, "TRANSPOSE"

    _header(canvas, active)
    _tabs(canvas, band)
    _rows(canvas, band, cursor_row)
    _lane(canvas, engines, depths, mask)
    _footer(canvas)
