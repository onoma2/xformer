"""
Engine pages — static per-engine routing tabs (in-use engines only, TrackMode
enum order, after the fixed Pitch/Clock/SlideTime/Global bands, before Bus).
Each page = that engine's ParamTable rows minus the shared Pitch/Clock/SlideTime/
Global keys, in a param x track grid. >4 rows scroll (right-edge scrollbar).
"""
from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64


def _clear(canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()


def _footer(canvas, names):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    for i in range(1, 5):
        canvas.vline((PW * i) // 5, PH - 10, 10)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    for i, name in enumerate(names):
        if not name:
            continue
        x0 = (PW * i) // 5
        x1 = (PW * (i + 1)) // 5
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)


def _clip(canvas, name, maxw):
    s = name
    while len(s) > 1 and canvas.text_width(s) > maxw:
        s = s[:-1]
    return s


def _engine_page(canvas, tab, engines, eligible_letter, rows, cursor_row, cursor_col,
                 view="depth", combine="MODULATE"):
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    name_w = 56                      # wider gutter: engine names run longer than band names
    grid_l = name_w + 2
    bar_x = 252                      # right-edge scrollbar column
    col_w = (bar_x - grid_l - 1) // 8

    # header: engine tab name + centered view tag (clears long names) + combine
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, tab)
    canvas.set_color(Color.Medium)
    vtag = {"source": "SOURCE", "depth": "DEPTH", "scale": "SCALE", "shaper": "SHAPER"}.get(view, "DEPTH")
    canvas.draw_text((PW - canvas.text_width(vtag)) // 2, 7, vtag)
    canvas.draw_text(bar_x - canvas.text_width(combine) - 2, 7, combine)
    canvas.hline(0, 10, PW)

    # engine column headers; cursor column bright, this-engine medium, others low
    hdr_y = 17
    for t in range(8):
        cx = grid_l + t * col_w + col_w // 2
        lbl = f"{t+1}{engines[t]}"
        if t == cursor_col:
            canvas.set_color(Color.Bright)
        else:
            canvas.set_color(Color.Medium if engines[t] == eligible_letter else Color.Low)
        canvas.draw_text(cx - canvas.text_width(lbl) // 2, hdr_y, lbl)

    top, rh, visible = 20, 8, 4
    scroll = max(0, min(cursor_row - visible + 1, len(rows) - visible))
    scroll = max(0, scroll)
    for vi in range(visible):
        r = scroll + vi
        if r >= len(rows):
            break
        name, source, depths = rows[r][:3]
        shaper = rows[r][3] if len(rows[r]) > 3 else "-"   # per-row shaper abbrev
        scale = rows[r][4] if len(rows[r]) > 4 else None    # per-row scaleSource abbrev (None = no scale)
        y = top + vi * rh
        canvas.set_color(Color.Bright if r == cursor_row else Color.Medium)
        canvas.draw_text(2, y + 6, _clip(canvas, name, name_w - 2))
        for t in range(8):
            cx = grid_l + t * col_w + col_w // 2
            elig = engines[t] == eligible_letter
            is_cursor = (r == cursor_row and t == cursor_col)
            if not elig:
                canvas.set_color(Color.Low)
                canvas.draw_text(cx - 2, y + 6, "-")
                continue
            d = depths.get(t) if depths else None
            if d is not None:
                if view == "source":
                    txt = source
                elif view == "shaper":
                    txt = shaper
                elif view == "scale":
                    txt = scale if scale else "."   # None scaleSource = no gain
                else:
                    txt = f"{d:+d}"
                canvas.set_color(Color.Bright if is_cursor else Color.Medium)
            else:
                txt = "."
                canvas.set_color(Color.Bright if is_cursor else Color.MediumLow)
            canvas.draw_text(cx - canvas.text_width(txt) // 2, y + 6, txt)

    # right-edge scrollbar (only when the list overflows the visible window)
    if len(rows) > visible:
        track_y, track_h = 19, top + visible * rh - 19
        canvas.set_color(Color.Low)
        canvas.vline(bar_x + 2, track_y, track_h)
        thumb_h = max(4, track_h * visible // len(rows))
        thumb_y = track_y + track_h * scroll // len(rows)
        canvas.set_color(Color.Bright)
        canvas.fill_rect(bar_x + 1, thumb_y, 3, thumb_h)

    _footer(canvas, ["VIEW", "EDIT", combine == "ABSOLUTE" and "ABS" or "MOD", "CANCEL", "COMMIT"])


# tracks 1,2,5 = Curve; rest other engines
_CURVE_ENGINES = ["C", "C", "N", "A", "C", "D", "P", "I"]
_CURVE_ROWS = [
    ("Offset", "CV1", {0: 20, 1: -10, 4: 30}, "FLD", "LFO1"),
    ("Shp Bias", "B2", {0: 15, 4: 15}, "-"),
    ("Gate Bias", None, None),
    ("C.Rate", "LFO1", {1: 40}, "-"),
    ("Phase", None, None),
    ("Run Mode", None, None),
    ("First St", None, None),
    ("Last St", None, None),
    ("Fold", "CV3", {0: 50, 4: 50}, "FLD", "CV2"),
    ("Gain", None, None),
    ("DJ Filter", None, None),
    ("Chaos Amt", "B1", {1: 25}),
    ("Chaos Rate", None, None),
    ("Chaos P1", None, None),
    ("Chaos P2", None, None),
]

_NOTE_ENGINES = ["N", "N", "C", "N", "A", "D", "P", "I"]
_NOTE_ROWS = [
    ("Gate Bias", "B2", {0: 20, 1: 20, 3: 20}),
    ("Rtg Bias", None, None),
    ("Len Bias", None, None),
    ("Note Bias", "LFO2", {3: -15}),
    ("Run Mode", None, None),
    ("First St", None, None),
    ("Last St", None, None),
]


def render_engine_curve(canvas):
    _engine_page(canvas, "CURVE", _CURVE_ENGINES, "C", _CURVE_ROWS,
                 cursor_row=6, cursor_col=4)


def render_engine_curve_shaper(canvas):
    _engine_page(canvas, "CURVE", _CURVE_ENGINES, "C", _CURVE_ROWS,
                 cursor_row=0, cursor_col=4, view="shaper")


def render_engine_curve_scale(canvas):
    _engine_page(canvas, "CURVE", _CURVE_ENGINES, "C", _CURVE_ROWS,
                 cursor_row=0, cursor_col=4, view="scale")


def render_engine_note(canvas):
    _engine_page(canvas, "NOTE", _NOTE_ENGINES, "N", _NOTE_ROWS,
                 cursor_row=1, cursor_col=0)
