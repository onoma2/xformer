"""
Proposed: the tab editor's bottom track-chip strip promoted to an interactive
scope / BY TYPE selector (reusing the existing read-only mini-map at
RoutingPage.cpp:1002-1030).

Faithful to the built tab editor layout: left tab column (PITCH/CLOCK/PROB/GLOB),
band param rows with right-aligned depth, footer, and the bottom strip. The strip
gains: member (filled), cursor (underline), by-type group (outline = same engine as
the cursor chip), plus a scope summary line. T1-T8 toggle a chip; BY TYPE (F4)
expands the cursor chip's engine to all matching tracks.

Two scenarios:
  scenario 1 — 8 Note tracks, Transpose on T2/T4/T7 (explicit toggle)
  scenario 2 — mixed layout, Clock Divisor on all Note tracks (BY TYPE)
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

TABS = ["PITCH", "CLOCK", "PROB", "GLOB"]
# band -> param rows (name, routed-depth-string or None)
BANDS = {
    0: [("Scale", None), ("Root Note", None), ("Transpose", "+5"), ("Octave", None)],
    1: [("Divisor", "+30%"), ("Clock Mult", None)],
    2: [("Gate Prob", None), ("Retrig Prob", None), ("Length", None), ("Note Prob", None)],
    3: [("Tempo", None), ("Swing", None), ("CVR Scan", None), ("CVR Route", None)],
}

MAP_X = 167
MAP_Y = 45
CHIP_W = 11
ROW_CX = 46
LIST_TOP = 15
ROW_H = 6
DEPTH_R = PW - 4


def _header(canvas: Canvas, active_param: str):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(2, 7, "A 120.0")
    canvas.set_color(Color.Bright)
    canvas.draw_text(58, 7, "ROUTE")
    # active param (drawActiveFunction style)
    canvas.set_color(Color.MediumBright)
    canvas.fill_rect(150, 0, PW - 150, 9)
    canvas.set_blend_mode(BlendMode.Sub)
    canvas.draw_text(PW - canvas.text_width(active_param) - 4, 7, active_param)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, 10, PW)


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
        canvas.set_color(Color.Medium)
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)


def _tabs(canvas: Canvas, active: int):
    canvas.set_font(Font.Tiny)
    tab_x, tab_w, tab_top, tab_h = 2, 40, 14, 9
    for i, t in enumerate(TABS):
        y = tab_top + i * tab_h
        if i == active:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(tab_x, y, tab_w, tab_h - 1)
            canvas.set_blend_mode(BlendMode.Sub)
            canvas.draw_text(tab_x + 3, y + 6, t)
            canvas.set_blend_mode(BlendMode.Set)
        else:
            canvas.set_color(Color.Low)
            canvas.draw_text(tab_x + 3, y + 6, t)
    canvas.set_color(Color.Low)
    canvas.vline(tab_w + 2, 12, PH - 23)


def _rows(canvas: Canvas, band: int, cursor_row: int):
    canvas.set_font(Font.Tiny)
    for i, (name, depth) in enumerate(BANDS[band]):
        y = LIST_TOP + i * ROW_H
        cursor = i == cursor_row
        if cursor:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(ROW_CX - 2, y - 1, PW - ROW_CX - 2, ROW_H)
            canvas.set_blend_mode(BlendMode.Sub)
        canvas.set_color(Color.Bright if depth else Color.Low)
        canvas.draw_text(ROW_CX, y + 4, name)
        if depth:
            canvas.set_color(Color.Medium)
            canvas.draw_text(DEPTH_R - canvas.text_width(depth), y + 4, depth)
        else:
            hint = "+ADD" if cursor else "--"
            canvas.set_color(Color.Medium if cursor else Color.Low)
            canvas.draw_text(DEPTH_R - canvas.text_width(hint), y + 4, hint)
        if cursor:
            canvas.set_blend_mode(BlendMode.Set)


def _strip(canvas: Canvas, engines, mask: int, cursor_track: int, show_group: bool):
    canvas.set_font(Font.Tiny)
    cur_engine = engines[cursor_track] if cursor_track is not None else None
    for t in range(8):
        px = MAP_X + t * CHIP_W
        letter = engines[t]
        member = bool(mask & (1 << t))
        in_group = show_group and cur_engine is not None and letter == cur_engine
        if member:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px, MAP_Y, 10, 7)
            canvas.set_blend_mode(BlendMode.Sub)
            canvas.draw_text(px + 3, MAP_Y + 5, letter)
            canvas.set_blend_mode(BlendMode.Set)
        elif in_group:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px, MAP_Y, 10, 7)
            canvas.draw_text(px + 3, MAP_Y + 5, letter)
        else:
            canvas.set_color(Color.Low)
            canvas.draw_text(px + 3, MAP_Y + 5, letter)
        # cursor underline
        if t == cursor_track:
            canvas.set_color(Color.Bright)
            canvas.hline(px, MAP_Y + 8, 10)


def _combine(canvas: Canvas, mask: int, absolute: bool):
    # bottom-left: combine indicator for the focused route (chips carry the scope,
    # so no text summary — the strip itself is the readout). "global" when no tracks.
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    if mask == 0:
        canvas.draw_text(ROW_CX, MAP_Y + 5, "global")
    else:
        canvas.draw_text(ROW_CX, MAP_Y + 5, "ABS" if absolute else "MOD")


def render_routing_tabscope(canvas: Canvas, scenario: int = 1):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()

    if scenario == 1:
        engines = ["N"] * 8
        mask = (1 << 1) | (1 << 3) | (1 << 6)   # T2 T4 T7
        band, cursor_row, active, absolute = 0, 2, "TRANSPOSE", False
        cursor_track, show_group = None, False
    else:
        engines = ["N", "C", "N", "A", "N", "D", "P", "S"]
        mask = (1 << 0) | (1 << 2) | (1 << 4)   # T1 T3 T5 (all Note)
        band, cursor_row, active, absolute = 1, 0, "DIVISOR", False
        cursor_track, show_group = 0, True

    _header(canvas, active)
    _tabs(canvas, band)
    _rows(canvas, band, cursor_row)
    _strip(canvas, engines, mask, cursor_track, show_group)
    _combine(canvas, mask, absolute)
    _footer(canvas)
