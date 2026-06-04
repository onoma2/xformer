"""
Proposed routing UI — scope lens (root = tracks).

The entry screen for authoring a route: a layout map of T1-T8 each labelled with the
engine running on it, plus a GLOBAL option. Multi-select the scope set (press track keys);
the set's shared mode gates the downstream target menu. Picking the scope IS the
global/unified/unique choice: none = global, a set = unified (default) -> spread = unique.
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

# (track engine tag) for a fully-populated project
LAYOUT = ["NOTE", "CURV", "NOTE", "ALGO", "NOTE", "DMAP", "PFLX", "STOC"]

CELL_W = 30
CELL_GAP = 2
ORIGIN_X = 4
CELL_Y = 16
CELL_H = 20


def _header(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "A 120.0")
    canvas.draw_text(54, 7, "ROUTING")
    label = "SCOPE"
    canvas.draw_text(PW - canvas.text_width(label) - 2, 7, label)
    canvas.set_color(Color.Medium)
    canvas.hline(0, 10, PW)


def _footer(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["GLOBAL", "BY TYPE", "ALL", "", "NEXT"]
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


def render_routing_scope(canvas: Canvas, selected=(2, 4), cursor=2):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()
    _header(canvas)
    _footer(canvas)

    canvas.set_font(Font.Tiny)
    sel = set(selected)
    for i, eng in enumerate(LAYOUT):
        x = ORIGIN_X + i * (CELL_W + CELL_GAP)
        on = i in sel
        cur = i == cursor

        if on:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(x, CELL_Y, CELL_W, CELL_H)
            canvas.set_blend_mode(BlendMode.Sub)
        else:
            canvas.set_color(Color.MediumLow)
            canvas.draw_rect(x, CELL_Y, CELL_W, CELL_H)
            canvas.set_blend_mode(BlendMode.Set)

        # track number (bright), engine tag (medium)
        canvas.set_color(Color.Bright if on else Color.Medium)
        tnum = f"T{i + 1}"
        canvas.draw_text(x + (CELL_W - canvas.text_width(tnum)) // 2, CELL_Y + 8, tnum)
        canvas.set_color(Color.Bright if on else Color.MediumLow)
        canvas.draw_text(x + (CELL_W - canvas.text_width(eng)) // 2, CELL_Y + 17, eng)

        canvas.set_blend_mode(BlendMode.Set)
        # cursor underline
        if cur:
            canvas.set_color(Color.Bright)
            canvas.hline(x, CELL_Y + CELL_H + 1, CELL_W)

    # scope summary line
    canvas.set_color(Color.Bright)
    if not sel:
        summary = "SCOPE: GLOBAL"
    else:
        tracks = " ".join(f"T{i + 1}" for i in sorted(sel))
        modes = {LAYOUT[i] for i in sel}
        mode = next(iter(modes)) if len(modes) == 1 else "MIXED"
        summary = f"SCOPE: {tracks}   {mode} x{len(sel)}"
    canvas.draw_text(4, CELL_Y + CELL_H + 12, summary)
