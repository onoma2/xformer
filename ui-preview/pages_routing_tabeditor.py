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
    cursor_trigger = rows[cursor_row][0] in ("Run", "Reset", "Play", "Ply Tgl", "Record", "Rec Tgl", "Tap Tmp")
    vtag = "SOURCE" if cursor_trigger else {"source": "SOURCE", "shaper": "SHAPER"}.get(view, "DEPTH")
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
        # Trigger rows (Run/Reset/transport) ignore depth/scale/shaper -> always SOURCE-style.
        trigger = name in ("Run", "Reset", "Play", "Ply Tgl", "Record", "Rec Tgl", "Tap Tmp")
        row_view = "source" if trigger else view
        canvas.set_color(Color.Bright if cursorRowSel else Color.Medium)
        canvas.draw_text(2, y + 6, _clip(canvas, name, nameW - 2))
        for t in range(8):
            cx = gridL + t * colW + colW // 2
            isCursor = cursorRowSel and t == cursor_col
            if row_view == "source":
                txt = source if routed else "."
            elif row_view == "shaper":
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

# Source tokens mirror Routing::printSourceAbbrev (CvIn->IN1-4, CvOut->O1-8, BusCv->B1-4,
# GateOut->G1-8, Mod->M1-8, None->'-'); shaper tokens mirror shaperToken (FLD/F30/F70/CRS/C10/C90).
# Global band, with CvOutRot / GateOutRot folded in (6 rows)
_GLOBAL_ROWS = [
    ("Tempo",    True,  True,  "IN1", {0: 20}, "MODULATE", "FLD"),
    ("Swing",    True,  False, "",    {},      "MODULATE", "-"),
    ("CVR Scan", True,  True,  "M1",  {0: 40}, "MODULATE", "CRS"),
    ("CVR Rout", True,  False, "",    {},      "MODULATE", "-"),
    ("CV Rot",   False, True,  "B2",  {0: 50, 1: 50}, "ABSOLUTE", "-"),
    ("Gate Rot", False, False, "",    {},      "MODULATE", "-"),
]

# Clock band, with Run / Reset folded in (4 rows)
_CLOCK_ROWS = [
    ("Divisor",  False, True,  "IN2", {0: 15, 3: 15}, "MODULATE", "F30"),
    ("Clk Mult", False, False, "",    {},      "MODULATE", "-"),
    ("Run",      False, True,  "G1",  {2: 100}, "ABSOLUTE", "-"),
    ("Reset",    False, False, "",    {},      "MODULATE", "-"),
    ("Play",     True,  True,  "IN1", {0: 100}, "ABSOLUTE", "-"),
    ("Ply Tgl",  True,  False, "",    {},      "ABSOLUTE", "-"),
    ("Record",   True,  False, "",    {},      "ABSOLUTE", "-"),
    ("Rec Tgl",  True,  False, "",    {},      "ABSOLUTE", "-"),
    ("Tap Tmp",  True,  True,  "G2",  {0: 100}, "ABSOLUTE", "-"),
    ("Mute",     False, True,  "IN1", {3: 100}, "ABSOLUTE", "-"),
    ("Fill",     False, False, "",    {},      "MODULATE", "-"),
    ("Fill Amt", False, True,  "IN1", {1: 60}, "ABSOLUTE", "-"),
    ("Pattern",  False, False, "",    {},      "MODULATE", "-"),
]


def render_routing_tabeditor(canvas):
    _tab(canvas, "GLOB", _LAYOUT, _GLOBAL_ROWS, view="depth", cursor_row=4, cursor_col=0, slots="3/16")


def render_routing_tabeditor_clock(canvas):
    _tab(canvas, "CLOCK", _LAYOUT, _CLOCK_ROWS, view="depth", cursor_row=4, cursor_col=0, scroll=4, slots="6/16")


def render_routing_tabeditor_source(canvas):
    _tab(canvas, "GLOB", _LAYOUT, _GLOBAL_ROWS, view="source", cursor_row=0, cursor_col=0, slots="3/16")


def render_routing_tabeditor_shaper(canvas):
    _tab(canvas, "GLOB", _LAYOUT, _GLOBAL_ROWS, view="shaper", cursor_row=0, cursor_col=0, slots="3/16")


# --- BUS tab: faithful mirror of RoutingPage::drawBus ---
# Each of 4 bus lanes: label, bipolar live-volt bar + value (x..104), then the one routing
# source + depth feeding it (SRC x110 / DEPTH x130), then CVR / TT activity lights.
def _bus_voltbar(canvas, x, y, w, volts, focus):
    midx = x + w // 2
    canvas.set_color(Color.Low)
    canvas.draw_rect(x, y, w, 6)
    canvas.vline(midx, y, 6)
    n = max(-1.0, min(1.0, volts / 5.0))
    canvas.set_color(Color.Bright if focus else Color.Medium)
    if n >= 0:
        canvas.fill_rect(midx + 1, y + 1, int(n * (w // 2 - 1)), 4)
    else:
        fw = int(-n * (w // 2 - 1))
        canvas.fill_rect(midx - fw, y + 1, fw, 4)


# (label, volts, source, depth, cvr, tt)  source="" -> empty lane
_BUS_LANES = [
    ("B1", 2.5, "IN1", 50,  True,  False),
    ("B2", -1.0, "M1", 30,  False, False),
    ("B3", 0.0, "",    0,   False, False),
    ("B4", 0.6, "G1", 100,  False, True),
]


def render_routing_bus(canvas):
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    DIV, VAL_R, SRC_X, DPT_X, SEP_X, CVR_X, TT_X = 104, 98, 110, 130, 172, 178, 210
    canvas.set_color(Color.Bright); canvas.draw_text(3, 7, "BUS")
    canvas.set_color(Color.Low)
    canvas.draw_text(SRC_X, 7, "SRC"); canvas.draw_text(DPT_X, 7, "DEPTH"); canvas.draw_text(CVR_X, 7, "LIVE")
    canvas.set_color(Color.Bright); canvas.draw_text(VAL_R - canvas.text_width("SAFE"), 7, "SAFE")
    canvas.set_color(Color.Low)
    canvas.hline(0, 10, PW); canvas.vline(DIV, 11, PH - 22); canvas.vline(SEP_X, 11, PH - 22)
    top, rowH, focus_lane = 14, 10, 0
    for i, (label, volts, src, depth, cvr, tt) in enumerate(_BUS_LANES):
        y = top + i * rowH
        focus = i == focus_lane
        if focus:
            canvas.set_color(Color.Bright); canvas.fill_rect(0, y - 1, 2, rowH - 1)
        canvas.set_color(Color.Bright if focus else Color.Medium)
        canvas.draw_text(6, y + 4, label)
        _bus_voltbar(canvas, 22, y + 1, 40, volts, focus)
        vtxt = (f"{volts:+.1f}V" if volts != 0 else "0.0V")
        canvas.set_color(Color.Bright if focus else Color.Medium)
        canvas.draw_text(VAL_R - canvas.text_width(vtxt), y + 4, vtxt)
        if src:
            canvas.set_color(Color.Medium); canvas.draw_text(SRC_X, y + 4, src)
            canvas.draw_text(DPT_X, y + 4, f"{depth:+d}%")
        else:
            canvas.set_color(Color.Low); canvas.draw_text(SRC_X, y + 4, "+")
        canvas.set_color(Color.Bright if cvr else Color.Low); canvas.draw_text(CVR_X, y + 4, "CVR")
        canvas.set_color(Color.Bright if tt else Color.Low); canvas.draw_text(TT_X, y + 4, "TT")
    _footer(canvas, ["VIEW", "EDIT", "", "", ""], highlight=-1)


# --- PROPOSED: per-param detail "door" (does NOT exist in firmware yet) ---
# Drills into one (param, track) cell to show the whole route at once. Example: Note T1 Transpose.
def render_routing_door(canvas):
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    # header: param + track context, route slot far right
    canvas.set_color(Color.Bright); canvas.draw_text(3, 7, "TRANSPOSE")
    canvas.set_color(Color.Medium); canvas.draw_text(78, 7, "T1 \xb7 NOTE")
    canvas.set_color(Color.Medium)
    slot = "R3/16"; canvas.draw_text(PW - canvas.text_width(slot) - 2, 7, slot)
    canvas.set_color(Color.Low); canvas.hline(0, 10, PW)

    # two label:value columns
    LBL_L, VAL_L, LBL_R, VAL_R = 6, 60, 138, 196
    rows = [
        ("SOURCE", "IN1",      "COMBINE", "MODULATE"),
        ("DEPTH",  "+50%",     "WINDOW",  "-12..+12"),
        ("SHAPER", "FLD",      "SCALE",   "M3"),
    ]
    y = 20
    for lab_l, val_l, lab_r, val_r in rows:
        canvas.set_color(Color.Medium); canvas.draw_text(LBL_L, y, lab_l)
        canvas.set_color(Color.Bright); canvas.draw_text(VAL_L, y, val_l)
        canvas.set_color(Color.Medium); canvas.draw_text(LBL_R, y, lab_r)
        canvas.set_color(Color.Bright); canvas.draw_text(VAL_R, y, val_r)
        y += 10
    # live applied readout: base + current routed delta -> result
    canvas.set_color(Color.Low); canvas.hline(0, 45, PW)
    canvas.set_color(Color.Medium); canvas.draw_text(6, 51, "NOW")
    canvas.set_color(Color.Bright); canvas.draw_text(34, 51, "base +2  ->  +9 st")
    _footer(canvas, ["BACK", "SRC", "DEPTH", "SHAPE", "WINDOW"], highlight=-1)
