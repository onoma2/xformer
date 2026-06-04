"""
Proposed routing UI — unified lens (root = the unified/infra params).

Separately accessible from the scope lens. Lists the unified-tier params (Divisor, Clock
Mult, Run, Reset, Pattern, Mute, ...) and shows each route's track scope as a COMPACT
per-row track+engine mini-map: 8 cells, one per track, engine letter inside, filled = the
route hits that track. Lets "set Divisor on T3+T5" read at a glance across all unified routes.
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

# project layout: engine letter per track (N=Note C=Curve A=Algo D=DMap P=PFlux S=Stoch I=Idx)
LAYOUT = ["N", "C", "N", "A", "N", "D", "P", "S"]
# an all-Indexed layout, to show the engine tab (A/B) over a same-mode scope
LAYOUT_IDX = ["I", "I", "I", "I", "I", "I", "I", "I"]
# only T4 is Indexed; A/B can target T4 only, the rest are ineligible
LAYOUT_IDX1 = ["N", "C", "N", "I", "N", "D", "P", "S"]
ELIGIBLE_IDX1 = 0b00001000

# topic tabs (shared with the editor); unified is a fan-mode applicable to ALL per-track
# params, so musical params live here too. Each row: (name, source|None, depth, member-mask)
TABS = ["PITCH", "CLOCK", "PROB", "TRANSP"]
TAB_ROWS = {
    "PITCH": [
        ("Transpose", "CV1",  "+5",   0b00010101),   # T1,T3,T5 unified together
        ("Octave",    None,   "",     0b00000000),
        ("Scale",     "M.C3", "+2",   0b00010100),   # T3,T5
        ("Root Note", None,   "",     0b00000000),
    ],
    "CLOCK": [
        ("Divisor",    "CV1",  "+1",   0b00010100),   # T3,T5
        ("Clock Mult", None,   "",     0b00000000),
    ],
    "TRANSP": [
        ("Run",     "CV2",  "trig", 0b11111111),
        ("Reset",   None,   "",     0b00000000),
        ("Pattern", "M.C1", "+2",   0b00010101),
        ("Mute",    None,   "",     0b00000000),
    ],
}

# Indexed-scope tab set: shared tabs + the engine tab IDX holding A/B
TABS_IDX = ["PITCH", "CLOCK", "TRANSP", "IDX"]
TAB_ROWS_IDX1 = {
    "IDX": [
        ("Index A", "CV1", "+40%", 0b00001000),
        ("Index B", None,  "",     0b00000000),
    ],
}
TAB_ROWS_IDX = {
    "IDX": [
        ("Index A", "CV1",  "+40%", 0b00000011),   # T1,T2
        ("Index B", "M.C2", "-25%", 0b00001100),   # T3,T4
    ],
    "PITCH": [
        ("Transpose", None, "", 0b00000000),
        ("Octave",    None, "", 0b00000000),
        ("Scale",     None, "", 0b00000000),
        ("Root Note", None, "", 0b00000000),
    ],
}

# vertical tab column on the left frees the horizontal strip's row (vertical span is scarce)
TAB_X = 2
TAB_W = 40
TAB_TOP = 14
TAB_H = 9

CONTENT_X = TAB_W + 6        # rows start right of the tab column
X_NAME = CONTENT_X
X_SRC = CONTENT_X + 56
X_ARROW = CONTENT_X + 84
MAP_CELL = 11
MAP_X = PW - 8 * MAP_CELL - 1   # right-aligned strip, no edge gap
X_DEPTH_R = MAP_X - 6
ROW_H = 6
TOP_Y = 14


def _header(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "A 120.0")
    canvas.draw_text(54, 7, "ROUTING")
    label = "UNIFIED"
    canvas.draw_text(PW - canvas.text_width(label) - 2, 7, label)
    canvas.set_color(Color.Medium)
    canvas.hline(0, 10, PW)


def _footer(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["SRC", "LEARN", "TRACKS", "", "EXIT"]
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


def _tabs(canvas: Canvas, tabs, selected: int):
    # vertical tab column on the left; selected = filled block, rest dim
    canvas.set_font(Font.Tiny)
    for i, t in enumerate(tabs):
        y = TAB_TOP + i * TAB_H
        if i == selected:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(TAB_X, y, TAB_W, TAB_H - 1)
            canvas.set_blend_mode(BlendMode.Sub)
            canvas.draw_text(TAB_X + 3, y + 6, t)
            canvas.set_blend_mode(BlendMode.Set)
        else:
            canvas.set_color(Color.MediumLow)
            canvas.draw_text(TAB_X + 3, y + 6, t)
    # divider between tab column and content
    canvas.set_color(Color.MediumLow)
    canvas.vline(TAB_W + 2, 12, PH - 23)


def _minimap(canvas: Canvas, x: int, y: int, mask: int, active: bool, layout, eligible: int = 0xFF):
    # 8 engine letters; member = filled block w/ knocked-out letter, non-member = dim letter.
    # No outline boxes (they break at 1px) and no inter-cell gap (kills the dark striping).
    for t in range(8):
        cx = x + t * MAP_CELL
        letter = layout[t]
        lx = cx + (MAP_CELL - canvas.text_width(letter)) // 2
        if not (eligible & (1 << t)):
            # ineligible: wrong engine, cannot host this param -> dim dash
            canvas.set_color(Color.MediumLow)
            canvas.hline(cx + 3, y + 3, MAP_CELL - 7)
        elif mask & (1 << t):
            canvas.set_color(Color.Bright if active else Color.MediumBright)
            canvas.fill_rect(cx, y, MAP_CELL - 1, 7)
            canvas.set_blend_mode(BlendMode.Sub)
            canvas.draw_text(lx, y + 5, letter)
            canvas.set_blend_mode(BlendMode.Set)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_text(lx, y + 5, letter)


def render_routing_unified(canvas: Canvas, tab_idx: int = 0, selected_row: int = 0,
                           engine: str = "note"):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()
    idx1 = engine == "indexed1"
    idx = engine == "indexed" or idx1
    tabs = TABS_IDX if idx else TABS
    tab_rows = (TAB_ROWS_IDX1 if idx1 else TAB_ROWS_IDX) if idx else TAB_ROWS
    layout = (LAYOUT_IDX1 if idx1 else LAYOUT_IDX) if idx else LAYOUT
    eligible = ELIGIBLE_IDX1 if idx1 else 0xFF
    _header(canvas)
    _tabs(canvas, tabs, tab_idx)
    _footer(canvas)

    rows = tab_rows[tabs[tab_idx]]
    canvas.set_font(Font.Tiny)
    for i, (name, src, depth, mask) in enumerate(rows):
        y = TOP_Y + i * ROW_H
        rowsel = (i == selected_row)
        routed = src is not None

        if rowsel:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(CONTENT_X - 2, y - 1, MAP_X - CONTENT_X - 2, ROW_H)
            canvas.set_blend_mode(BlendMode.Sub)

        canvas.set_color(Color.Bright if routed else Color.MediumLow)
        canvas.draw_text(X_NAME, y + 4, name)
        if routed:
            canvas.set_color(Color.Medium)
            canvas.draw_text(X_SRC, y + 4, src)
            canvas.draw_text(X_ARROW, y + 4, ">")
            canvas.set_color(Color.Bright)
            canvas.draw_text(X_DEPTH_R - canvas.text_width(depth), y + 4, depth)
        else:
            canvas.set_color(Color.MediumLow)
            canvas.draw_text(X_SRC, y + 4, "--")

        if rowsel:
            canvas.set_blend_mode(BlendMode.Set)

        _minimap(canvas, MAP_X, y - 1, mask, rowsel, layout, eligible)
