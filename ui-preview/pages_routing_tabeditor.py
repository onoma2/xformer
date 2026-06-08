"""
Faithful mirror of RoutingPage::drawTabEditor (the current scrollable matrix grid) for
glyph-fit checks. Static tab ring; bands use a 38px name gutter, engine pages 52px. Rows =
param keys; 8 track columns; '-' ineligible, '.' eligible+unrouted, value when routed; right
M/A combine marker; right-edge scrollbar on overflow. View = SOURCE | DEPTH | SHAPER.
"""
from canvas import BlendMode, Canvas, Color, Font

PW, PH = 256, 64


def _clear(canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()


def _clip(canvas, name, maxw):
    buf = name
    while len(buf) > 1 and canvas.text_width(buf) > maxw:
        buf = buf[:-1]
    return buf


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


# A row: (name, global, routed, source, depths, combine, shaper)
#   global=True  -> one shared value across all columns (project target)
#   routed=False -> all eligible cells show '.'
#   depths       -> {trackIndex: pct} for per-track rows; for a global row, value at key 0
def _tab(canvas, tab, layout, rows, view="depth", cursor_row=0, cursor_col=0,
         scroll=0, editing=False, combine="MODULATE", slots="3/16", is_engine=False):
    _clear(canvas)
    canvas.set_font(Font.Tiny)

    # header: tab name + centered view tag + combine + slot counter
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, tab)
    canvas.set_color(Color.Medium)
    vtag = {"source": "SOURCE", "shaper": "SHAPER"}.get(view, "DEPTH")
    canvas.draw_text((PW - canvas.text_width(vtag)) // 2, 7, vtag)
    sx = PW - canvas.text_width(slots) - 2
    canvas.draw_text(sx, 7, slots)
    if editing or rows[cursor_row][2]:
        cb = "ABSOLUTE" if combine == "ABSOLUTE" else "MODULATE"
        canvas.set_color(Color.Bright if editing else Color.Medium)
        canvas.draw_text(sx - 6 - canvas.text_width(cb), 7, cb)
    canvas.set_color(Color.Low)
    canvas.hline(0, 10, PW)

    top, rowH, visible = 20, 8, 4
    n = len(rows)
    # keep the cursor row inside the scroll window (mirrors _tabScroll maintenance)
    if cursor_row < scroll:
        scroll = cursor_row
    elif cursor_row >= scroll + visible:
        scroll = cursor_row - visible + 1
    scroll = max(0, min(scroll, max(0, n - visible)))
    barW = 4 if n > visible else 0
    nameW = 52 if is_engine else 38
    gridL = nameW + 2
    colW = (PW - gridL - 1 - barW) // 8

    # column headers
    hdrY = 17
    cursor_key_global = rows[cursor_row][1]
    for t in range(8):
        cx = gridL + t * colW + colW // 2
        lbl = f"{t+1}{layout[t]}"
        if t == cursor_col:
            canvas.set_color(Color.Bright)
        else:
            canvas.set_color(Color.Medium)   # bands: every column eligible
        canvas.draw_text(cx - canvas.text_width(lbl) // 2, hdrY, lbl)

    for vi in range(visible):
        r = scroll + vi
        if r >= n:
            break
        y = top + vi * rowH
        name, is_global, routed, source, depths, rcombine, shaper = rows[r]
        cursorRowSel = (r == cursor_row)
        canvas.set_color(Color.Bright if cursorRowSel else Color.Medium)
        canvas.draw_text(2, y + 6, _clip(canvas, name, nameW - 2))
        for t in range(8):
            cx = gridL + t * colW + colW // 2
            isCursor = cursorRowSel and t == cursor_col
            if view == "source":
                txt = source if routed else "."
            elif view == "shaper":
                txt = (shaper if routed else ".")
            else:
                if is_global:
                    txt = (f"{depths.get(0,0):+d}" if routed else ".")
                else:
                    d = depths.get(t)
                    txt = (f"{d:+d}" if d is not None else ".")
            hot = routed and (txt != ".")
            canvas.set_color(Color.Bright if isCursor else (Color.Medium if hot else Color.MediumLow))
            canvas.draw_text(cx - canvas.text_width(txt) // 2, y + 6, txt)
        if routed:
            canvas.set_color(Color.Bright if cursorRowSel else Color.Medium)
            canvas.draw_text(PW - 6, y + 6, "A" if rcombine == "ABSOLUTE" else "M")

    if n > visible:
        trackY, trackH = 19, top + visible * rowH - 19
        canvas.set_color(Color.Low)
        canvas.vline(PW - 2, trackY, trackH)
        thumbH = max(4, trackH * visible // n)
        thumbY = trackY + trackH * scroll // n
        canvas.set_color(Color.Bright)
        canvas.fill_rect(PW - 3, thumbY, 3, thumbH)

    cb = "ABS" if combine == "ABSOLUTE" else "MOD"
    fn = ["VIEW", "EDIT", cb if editing else "", "CANCEL" if editing else "", "COMMIT" if editing else ""]
    _footer(canvas, fn, highlight=1 if editing else -1)


_LAYOUT = "NNCATDPI"   # representative engines per track column

# Global band, now with CvOutRot / GateOutRot folded in (6 rows)
_GLOBAL_ROWS = [
    ("Tempo",    True,  True,  "CV1", {0: 20}, "MODULATE", "-"),
    ("Swing",    True,  False, "",    {},      "MODULATE", "-"),
    ("CVR Scan", True,  True,  "LFO1", {0: 40}, "MODULATE", "-"),
    ("CVR Rout", True,  False, "",    {},      "MODULATE", "-"),
    ("CV Rot",   False, True,  "B2",  {0: 50, 1: 50}, "ABSOLUTE", "-"),
    ("Gate Rot", False, False, "",    {},      "MODULATE", "-"),
]

# Clock band, now with Run / Reset folded in (4 rows)
_CLOCK_ROWS = [
    ("Divisor",  False, True,  "CV2", {0: 15, 3: 15}, "MODULATE", "-"),
    ("Clk Mult", False, False, "",    {},      "MODULATE", "-"),
    ("Run",      False, True,  "G1",  {2: 100}, "ABSOLUTE", "-"),
    ("Reset",    False, False, "",    {},      "MODULATE", "-"),
]


def render_routing_tabeditor(canvas):
    _tab(canvas, "GLOB", _LAYOUT, _GLOBAL_ROWS, view="depth", cursor_row=4, cursor_col=0, slots="3/16")


def render_routing_tabeditor_clock(canvas):
    _tab(canvas, "CLOCK", _LAYOUT, _CLOCK_ROWS, view="depth", cursor_row=2, cursor_col=2, slots="2/16")


def render_routing_tabeditor_source(canvas):
    _tab(canvas, "GLOB", _LAYOUT, _GLOBAL_ROWS, view="source", cursor_row=4, cursor_col=0, slots="3/16")
