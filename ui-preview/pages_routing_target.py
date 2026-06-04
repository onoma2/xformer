"""
Proposed routing UI — target lens (root = a param).

Entry for "Transpose across everything": pick the param, see it on every track at once.
One row per track that *owns* the param (engine has it in its table), showing whether it is
routed, from what source, at what depth. Tracks whose engine lacks the param are dim n/a.
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

PARAM = "TRANSPOSE"
# per track: (engine, has-param, source|None, depth)
TRACKS = [
    ("NOTE", True,  "CV1",  "+5"),
    ("CURV", False, None,   ""),     # Curve has no Transpose -> n/a
    ("NOTE", True,  "CV1",  "+2"),
    ("ALGO", True,  None,   ""),     # owns it, not routed
    ("NOTE", True,  "CV1",  "-3"),
    ("DMAP", True,  None,   ""),
    ("PFLX", True,  "M.C2", "+7"),
    ("STOC", True,  None,   ""),
]

# 8 fixed tracks -> two columns of four, not one tall list
ROW_H = 9
TOP_Y = 15
COL_W = 128
# offsets within a column
O_TRK = 4
O_ENG = 22
O_SRC = 48
O_ARROW = 74
O_DEPTH_R = 120


def _header(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "A 120.0")
    canvas.draw_text(54, 7, "ROUTING")
    label = f"TARGET {PARAM}"
    canvas.draw_text(PW - canvas.text_width(label) - 2, 7, label)
    canvas.set_color(Color.Medium)
    canvas.hline(0, 10, PW)


def _footer(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["PARAM", "SRC", "LEARN", "", "EXIT"]
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


def render_routing_target(canvas: Canvas, selected_row: int = 0):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()
    _header(canvas)
    _footer(canvas)

    canvas.set_font(Font.Tiny)
    # column divider
    canvas.set_color(Color.MediumLow)
    canvas.vline(COL_W, 12, PH - 23)

    for i, (eng, has, src, depth) in enumerate(TRACKS):
        col = i // 4
        row = i % 4
        bx = col * COL_W + (2 if col else 0)
        y = TOP_Y + row * ROW_H
        routed = src is not None

        if i == selected_row:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(bx, y - 1, COL_W - 4, ROW_H - 1)
            canvas.set_blend_mode(BlendMode.Sub)

        canvas.set_color(Color.Bright if has else Color.MediumLow)
        canvas.draw_text(bx + O_TRK, y + 5, f"T{i + 1}")
        canvas.set_color(Color.Medium if has else Color.MediumLow)
        canvas.draw_text(bx + O_ENG, y + 5, eng)

        if not has:
            canvas.set_color(Color.MediumLow)
            canvas.draw_text(bx + O_SRC, y + 5, "n/a")
        elif routed:
            canvas.set_color(Color.Medium)
            canvas.draw_text(bx + O_SRC, y + 5, src)
            canvas.draw_text(bx + O_ARROW, y + 5, ">")
            canvas.set_color(Color.Bright)
            canvas.draw_text(bx + O_DEPTH_R - canvas.text_width(depth), y + 5, depth)
        else:
            canvas.set_color(Color.MediumLow)
            canvas.draw_text(bx + O_SRC, y + 5, "--")

        if i == selected_row:
            canvas.set_blend_mode(BlendMode.Set)
