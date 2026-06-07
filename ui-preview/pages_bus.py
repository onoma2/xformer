"""
Bus hub page — the next Left/Right tab after the routing-matrix bands.
4 CV lanes (+-5V, sum+clamp). Per lane: bipolar value bar + which systems write it
(R=routing routes w/ count, C=CV-router, T=Teletype BUS op) + reader count. The cursor
lane is brighter; F2 ADD creates a route targeting it (routing bus-setup lives here).
"""
from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64


def _clear(canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()


def _footer(canvas, names, highlight=-1):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    for i in range(1, 5):
        canvas.vline((PW * i) // 5, PH - 10, 10)
    canvas.set_font(Font.Tiny)
    for i, name in enumerate(names):
        if not name:
            continue
        x0 = (PW * i) // 5
        x1 = (PW * (i + 1)) // 5
        if i == highlight:
            canvas.set_color(Color.Medium)
            canvas.fill_rect(x0 + 1, PH - 10, (x1 - x0) - 1, 9)
            canvas.set_color(Color.None_)
        else:
            canvas.set_color(Color.Bright)
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)


def _depth_bar(canvas, x, y, w, pct, focus):
    # bipolar -100..+100, centre tick = 0; fill from centre toward sign
    cx = x + w // 2
    canvas.set_color(Color.Low)
    canvas.hline(x, y + 1, w)
    canvas.vline(cx, y - 1, 5)
    span = int(min(abs(pct), 100) / 100.0 * (w // 2))
    canvas.set_color(Color.Bright if focus else Color.Medium)
    if span > 0:
        if pct >= 0:
            canvas.fill_rect(cx, y, span, 3)
        else:
            canvas.fill_rect(cx - span, y, span, 3)


def _volt_bar(canvas, x, y, w, volts, focus):
    # bipolar -5..+5V, centre tick = 0
    cx = x + w // 2
    canvas.set_color(Color.Low)
    canvas.hline(x, y + 1, w)
    canvas.vline(cx, y - 1, 5)
    span = int(min(abs(volts), 5.0) / 5.0 * (w // 2))
    canvas.set_color(Color.Bright if focus else Color.Medium)
    if span > 0:
        if volts >= 0:
            canvas.fill_rect(cx, y, span, 3)
        else:
            canvas.fill_rect(cx - span, y, span, 3)


def render_bus(canvas):
    _clear(canvas)
    canvas.set_font(Font.Tiny)

    # column geometry: fixed LEVEL block | divider | variable CONNECTIONS
    DIV = 104           # vertical divider x
    VAL_R = 98          # right edge of the value column (right-aligned)

    # header: band name + zone labels (no static law / no mode status)
    canvas.set_color(Color.Bright)
    canvas.draw_text(3, 7, "BUS")
    canvas.set_color(Color.Low)
    canvas.draw_text(DIV + 4, 7, "<sources")
    canvas.draw_text(PW - canvas.text_width("readers>") - 3, 7, "readers>")
    canvas.hline(0, 10, PW)
    canvas.vline(DIV, 11, PH - 22)

    # (label, volts, writers-in, readers-out)
    lanes = [
        ("B1", 2.3, ["T8", "T2", "TT"], ["O1"]),
        ("B2", -1.0, ["CVR"], []),
        ("B3", 4.8, ["TT"], ["O3", "T5"]),
        ("B4", 0.0, [], []),
    ]
    focus_lane = 0
    top = 14
    rh = 10
    for i, (label, volts, writers, readers) in enumerate(lanes):
        y = top + i * rh
        focus = (i == focus_lane)
        if focus:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(0, y - 1, 2, rh - 1)
        # fixed LEVEL block: label · bar · right-aligned value
        canvas.set_color(Color.Bright if focus else Color.Medium)
        canvas.draw_text(6, y + 4, label)
        _volt_bar(canvas, 22, y + 1, 40, volts, focus)
        vtxt = ("%+.1fV" % volts) if volts != 0 else "0.0V"
        canvas.draw_text(VAL_R - canvas.text_width(vtxt), y + 4, vtxt)
        # variable CONNECTIONS zone
        cx = DIV + 6
        if not writers and not readers:
            canvas.set_color(Color.Low)
            canvas.draw_text(cx, y + 4, "idle")
        else:
            for w in writers:
                canvas.set_color(Color.Bright if focus else Color.Medium)
                canvas.draw_text(cx, y + 4, w)
                cx += canvas.text_width(w) + 7
            for r in readers:
                canvas.set_color(Color.Low)
                canvas.draw_text(cx, y + 4, r)
                cx += canvas.text_width(r) + 7

    _footer(canvas, ["VIEW", "ADD", "SAFE", "REMOVE", ""])


# ---------------------------------------------------------------------------
# PROPOSED: bus lane edits reuse the band-tab draft flow (EDIT/VIEW/COMMIT).
# A lane = one routing source (this screen) summed with CV-router + Teletype.
# Footer mirrors the band tabs: VIEW EDIT <MOD> CANCEL COMMIT; Shift+F3 = SAFE.
# ---------------------------------------------------------------------------
DIV = 104
VAL_R = 98


def _bus_header(canvas, view_tag, slots):
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(3, 7, "BUS")
    if view_tag:
        canvas.set_color(Color.Medium)
        canvas.draw_text(40, 7, view_tag)
        canvas.set_color(Color.Medium)
        canvas.draw_text(VAL_R - canvas.text_width(slots), 7, slots)
    else:
        canvas.set_color(Color.Low)
        canvas.draw_text(DIV + 4, 7, "<sources")
        canvas.draw_text(PW - canvas.text_width("readers>") - 3, 7, "readers>")
    canvas.hline(0, 10, PW)
    canvas.vline(DIV, 11, PH - 22)


# (label, volts, writers, readers). Writers = the single routing source abbrev +
# CVR + TT (only domains that write this frame); TT is the lone live-only glyph.
# Readers = param targets reading the lane as a source.
_LANES = [
    ("B1", 2.3, ["I2", "TT"], ["PIT"]),
    ("B2", -1.0, ["CVR"], []),
    ("B3", 4.8, ["O3", "TT"], ["TR5"]),
    ("B4", 0.0, [], []),
]


def _lane_level(canvas, y, label, volts, focus):
    if focus:
        canvas.set_color(Color.Bright)
        canvas.fill_rect(0, y - 1, 2, 9)
    canvas.set_color(Color.Bright if focus else Color.Medium)
    canvas.draw_text(6, y + 4, label)
    _volt_bar(canvas, 22, y + 1, 40, volts, focus)
    vtxt = ("%+.1fV" % volts) if volts != 0 else "0.0V"
    canvas.draw_text(VAL_R - canvas.text_width(vtxt), y + 4, vtxt)


def _lane_connections(canvas, y, writers, readers, focus):
    cx = DIV + 6
    if not writers and not readers:
        canvas.set_color(Color.Low)
        canvas.draw_text(cx, y + 4, "idle")
        return
    for w in writers:
        canvas.set_color(Color.Bright if focus else Color.Medium)
        canvas.draw_text(cx, y + 4, w)
        cx += canvas.text_width(w) + 7
    for r in readers:
        canvas.set_color(Color.Low)
        canvas.draw_text(cx, y + 4, r)
        cx += canvas.text_width(r) + 7


def _render_proposed(canvas, edit=None):
    # edit = None (nav) | dict(lane, view='source'|'depth', source, depth, combine)
    _clear(canvas)
    view_tag = ""
    if edit:
        view_tag = "SOURCE" if edit["view"] == "source" else "DEPTH"
    _bus_header(canvas, view_tag, "2/64")

    top, rh = 14, 10
    for i, (label, volts, writers, readers) in enumerate(_LANES):
        y = top + i * rh
        editing_this = edit is not None and edit["lane"] == i
        focus = editing_this or (edit is None and i == 0)
        _lane_level(canvas, y, label, volts, focus)

        if editing_this:
            cx = DIV + 6
            if edit["view"] == "source":
                canvas.set_color(Color.Medium)
                canvas.draw_text(cx, y + 4, "SRC")
                cx += canvas.text_width("SRC") + 4
                canvas.set_color(Color.Bright)
                canvas.draw_text(cx, y + 4, chr(0x10) + " " + edit["source"])
            else:
                cb = "A" if edit["combine"] == "abs" else "M"
                canvas.set_color(Color.Medium)
                canvas.draw_text(cx, y + 4, cb)
                cx += canvas.text_width(cb) + 4
                _depth_bar(canvas, cx, y + 1, 60, edit["depth"], True)
                cx += 60 + 5
                canvas.set_color(Color.Bright)
                canvas.draw_text(cx, y + 4, "%+d%%" % edit["depth"])
        else:
            _lane_connections(canvas, y, writers, readers, focus)

    if edit is None:
        _footer(canvas, ["VIEW", "EDIT", "", "", ""], highlight=-1)
    else:
        combine = "ABS" if edit["combine"] == "abs" else "MOD"
        _footer(canvas, ["VIEW", "EDIT", combine, "CANCEL", "COMMIT"])


def render_bus_nav_proposed(canvas):
    _render_proposed(canvas, None)


# ---------------------------------------------------------------------------
# PROPOSED v2: connections zone as a fixed grid, two groups divided by a
# separator. Group 1 = the routing source set here + its depth (editable).
# Group 2 = CVR + TT activity lights (set up elsewhere, just lit when writing).
# ---------------------------------------------------------------------------
SRC_X = 110         # routing source abbrev column
DPT_X = 130         # routing depth column
SEP_X = 172         # group separator
CVR_X = 178         # CV-router light
TT_X = 210          # Teletype light

# (label, volts, route_src, route_depth, cvr_live, tt_live)
_GRID_LANES = [
    ("B1", 2.3, "I2", 60, False, True),
    ("B2", -1.0, None, 0, True, False),
    ("B3", 4.8, "O3", -40, False, True),
    ("B4", 0.0, None, 0, False, False),
]


def render_bus_grid_proposed(canvas):
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    # header: title + dim column labels for the two groups
    canvas.set_color(Color.Bright)
    canvas.draw_text(3, 7, "BUS")
    canvas.set_color(Color.Low)
    canvas.draw_text(SRC_X, 7, "SRC")
    canvas.draw_text(DPT_X, 7, "DEPTH")
    canvas.draw_text(CVR_X, 7, "LIVE")
    canvas.hline(0, 10, PW)
    canvas.vline(DIV, 11, PH - 22)
    canvas.set_color(Color.Low)
    canvas.vline(SEP_X, 11, PH - 22)   # group separator

    top, rh = 14, 10
    for i, (label, volts, src, depth, cvr, tt) in enumerate(_GRID_LANES):
        y = top + i * rh
        focus = (i == 0)
        _lane_level(canvas, y, label, volts, focus)
        # group 1: routing source + depth
        if src:
            canvas.set_color(Color.Bright if focus else Color.Medium)
            canvas.draw_text(SRC_X, y + 4, src)
            canvas.set_color(Color.Medium)
            canvas.draw_text(DPT_X, y + 4, "%+d%%" % depth)
        else:
            canvas.set_color(Color.Low)
            canvas.draw_text(SRC_X, y + 4, "\x10")  # +ADD glyph (empty slot)
        # group 2: CVR / TT activity lights
        canvas.set_color(Color.Bright if cvr else Color.Low)
        canvas.draw_text(CVR_X, y + 4, "CVR")
        canvas.set_color(Color.Bright if tt else Color.Low)
        canvas.draw_text(TT_X, y + 4, "TT")

    _footer(canvas, ["VIEW", "EDIT", "", "", ""])


def render_bus_edit_source_proposed(canvas):
    _render_proposed(canvas, dict(lane=3, view="source", source="CV In 2",
                                  depth=75, combine="mod"))


def render_bus_edit_depth_proposed(canvas):
    _render_proposed(canvas, dict(lane=3, view="depth", source="CV In 2",
                                  depth=75, combine="mod"))
