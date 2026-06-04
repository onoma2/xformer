"""
Proposed routing UI — SPREAD / unique view.

Reached by pressing SPREAD on a routed param row: the unified mini-map unfolds into one
row per member track, each with its OWN depth (and shape). Same source, per-track value =
unique. Non-member tracks are not shown.
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

# the param being spread, its shared source, and per-member-track (track#, engine, depth)
PARAM = "TRANSPOSE"
SOURCE = "CV1"
MEMBERS = [
    (1, "NOTE", +5),
    (3, "NOTE", +2),
    (5, "NOTE", -3),
]
DEPTH_MIN, DEPTH_MAX = -60, 60

ROW_H = 11
TOP_Y = 16
BAR_X = 96
BAR_W = 150


def _header(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "A 120.0")
    canvas.draw_text(54, 7, "ROUTING")
    label = f"{PARAM} < {SOURCE}"
    canvas.draw_text(PW - canvas.text_width(label) - 2, 7, label)
    canvas.set_color(Color.Medium)
    canvas.hline(0, 10, PW)


def _footer(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["SHAPE", "", "UNIFY", "", "BACK"]
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


def _bar(canvas: Canvas, x: int, y: int, w: int, value: int):
    # centered bipolar bar: zero in the middle, fill toward the sign
    mid = x + w // 2
    canvas.set_color(Color.MediumLow)
    canvas.hline(x, y + 3, w)
    canvas.vline(mid, y, 7)
    span = (w // 2) * value // DEPTH_MAX
    canvas.set_color(Color.Bright)
    if span >= 0:
        canvas.fill_rect(mid, y + 1, max(1, span), 5)
    else:
        canvas.fill_rect(mid + span, y + 1, -span, 5)


def render_routing_spread(canvas: Canvas, selected_row: int = 0):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()
    _header(canvas)
    _footer(canvas)

    canvas.set_font(Font.Tiny)
    for i, (tnum, eng, depth) in enumerate(MEMBERS):
        y = TOP_Y + i * ROW_H
        if i == selected_row:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(0, y - 1, PW, ROW_H - 1)
            canvas.set_blend_mode(BlendMode.Sub)

        canvas.set_color(Color.Bright)
        canvas.draw_text(4, y + 6, f"T{tnum}")
        canvas.set_color(Color.Medium)
        canvas.draw_text(24, y + 6, eng)
        ds = f"{depth:+d}"
        canvas.set_color(Color.Bright)
        canvas.draw_text(64, y + 6, ds)

        if i == selected_row:
            canvas.set_blend_mode(BlendMode.Set)
        _bar(canvas, BAR_X, y, BAR_W, depth)
