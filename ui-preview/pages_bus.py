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


def _footer(canvas, names):
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
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)


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
