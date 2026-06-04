"""
Modulation UI renders for spec 013 (docs/plans/2026-06-04-013-modulation-ui-spec.md).
Six screens (sec 10): param-door row (clean), param-door depth-focus+dirty, param-door
spread sub-view, source picker (COMMIT), matrix grid, matrix grid by-type.
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64


def _clear(canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()


def _footer(canvas, names, commit=False):
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
        hot = (name == "COMMIT" and commit)
        if hot:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(x0 + 1, PH - 10, x1 - x0 - 1, 9)
            canvas.set_blend_mode(BlendMode.Sub)
        else:
            canvas.set_color(Color.Medium)
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)
        if hot:
            canvas.set_blend_mode(BlendMode.Set)


def _hbar(canvas, x, y, w, depth, focus):
    # horizontal bipolar bar: centre tick, fill right=+ / left=-
    cx = x + w // 2
    canvas.set_color(Color.Low)
    canvas.hline(x, y + 1, w)
    canvas.vline(cx, y - 1, 5)
    span = int(abs(depth) / 100 * (w // 2))
    canvas.set_color(Color.Bright if focus else Color.Medium)
    if depth >= 0:
        if span > 0:
            canvas.fill_rect(cx, y, span, 3)
    else:
        if span > 0:
            canvas.fill_rect(cx - span, y, span, 3)


def _param_row_page(canvas, depth_focus, dirty):
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "T8")
    canvas.set_color(Color.Medium)
    canvas.draw_text(20, 7, "NOTE")
    canvas.set_color(Color.Medium)
    canvas.draw_text(PW - canvas.text_width("TRACK") - 2, 7, "TRACK")
    canvas.hline(0, 10, PW)

    rows = [
        ("Divisor", None, "1/16"),
        ("Transpose", ("CV1", 40), "+5"),   # modulated
        ("Scale", None, "Major"),
        ("Root Note", None, "C"),
        ("Octave", None, "0"),
    ]
    top, rh = 14, 8
    for i, (name, mod, value) in enumerate(rows):
        y = top + i * rh
        modulated = mod is not None
        is_cursor = (i == 1)
        # cursor highlight on the param name when this row is focused
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Bright if modulated else Color.Medium)
        canvas.draw_text(4, y + 5, name)
        if modulated:
            src, depth = mod
            canvas.set_color(Color.Medium)
            canvas.draw_text(78, y + 5, src)
            canvas.draw_text(96, y + 5, ">")
            _hbar(canvas, 104, y + 1, 92, depth, focus=depth_focus)
        # value, right; highlighted when value-focus on the cursor row
        vf = is_cursor and not depth_focus
        if vf:
            vw = canvas.text_width(value)
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(PW - vw - 6, y, vw + 4, rh - 1)
            canvas.set_blend_mode(BlendMode.Sub)
            canvas.draw_text(PW - vw - 4, y + 5, value)
            canvas.set_blend_mode(BlendMode.Set)
        else:
            canvas.set_color(Color.Bright if is_cursor else Color.Medium)
            canvas.draw_text(PW - canvas.text_width(value) - 4, y + 5, value)

    _footer(canvas, ["", "SRC", "COMBINE", "", "COMMIT"], commit=dirty)


def render_mod_param_row(canvas):
    _param_row_page(canvas, depth_focus=False, dirty=False)


def render_mod_param_depth(canvas):
    _param_row_page(canvas, depth_focus=True, dirty=True)


def render_mod_spread(canvas):
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "TRANSPOSE")
    canvas.set_color(Color.Medium)
    canvas.draw_text(2 + canvas.text_width("TRANSPOSE") + 4, 7, "< CV1")
    canvas.draw_text(PW - canvas.text_width("SPREAD") - 2, 7, "SPREAD")
    canvas.hline(0, 10, PW)

    # 8 vertical bipolar bars, one per track. Transpose is a Note param, so only Note
    # tracks are eligible; others show ineligible (dim, no bar). Labels = number+engine.
    engines = ["N", "C", "N", "A", "N", "D", "N", "S"]
    depths = {0: 40, 2: -30, 4: 60, 6: -15}  # per Note track
    cursor = 4
    slot = PW // 8
    center_y = 26
    half = 14
    label_y = center_y + half + 5
    for t in range(8):
        cx = t * slot + slot // 2
        eligible = engines[t] == "N"
        label = f"{t+1}{engines[t]}"
        if not eligible:
            canvas.set_color(Color.Low)
            canvas.draw_rect(cx - 6, center_y - half, 12, half * 2)
            canvas.draw_text(cx - 6, label_y, label)
            continue
        canvas.set_color(Color.Low)
        canvas.draw_rect(cx - 6, center_y - half, 12, half * 2)
        canvas.hline(cx - 6, center_y, 12)
        d = depths.get(t, 0)
        h = int(abs(d) / 100 * (half - 1))
        canvas.set_color(Color.Bright)
        if d >= 0:
            if h > 0:
                canvas.fill_rect(cx - 4, center_y - h, 8, h)
        else:
            if h > 0:
                canvas.fill_rect(cx - 4, center_y, 8, h)
        # track label + cursor
        canvas.set_color(Color.Bright if t == cursor else Color.Medium)
        canvas.draw_text(cx - 6, label_y, label)
        if t == cursor:
            canvas.set_color(Color.Bright)
            canvas.hline(cx - 8, label_y + 2, 16)

    _footer(canvas, ["", "SRC", "COMBINE", "", "COMMIT"], commit=False)


def render_mod_source(canvas):
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "SOURCE")
    canvas.hline(0, 10, PW)
    items = ["None", "CV In 1", "CV In 2", "CV In 3", "CV In 4"]
    sel = 1
    top, rh = 14, 7
    for i, it in enumerate(items):
        y = top + i * rh
        if i == sel:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(0, y, PW - 6, rh)
            canvas.set_blend_mode(BlendMode.Sub)
        canvas.set_color(Color.Bright)
        canvas.draw_text(4, y + 5, it)
        if i == sel:
            canvas.set_blend_mode(BlendMode.Set)
    # scrollbar
    canvas.set_color(Color.MediumLow)
    canvas.vline(252, 12, PH - 23)
    canvas.set_color(Color.Bright)
    canvas.fill_rect(251, 12, 3, 8)
    _footer(canvas, ["", "", "", "", "COMMIT"], commit=True)  # CV In 1 selected -> committable


def _matrix(canvas, bytype, view="depth"):
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    engines = ["N", "C", "N", "A", "N", "D", "P", "S"]
    name_w = 38
    grid_l = name_w + 2
    col_w = (PW - grid_l - 1) // 8
    # (name, source, {track: depth})
    rows = [
        ("Scale", "B1", {0: 5, 2: 3, 4: 7}),
        ("RootNt", None, None),
        ("Transp", "CV1", {0: 40, 2: 20, 4: 60}),
        ("Octave", None, None),
    ]
    cursor_row, cursor_col = 2, 4

    # header: band + view tag
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "PITCH")
    canvas.set_color(Color.Medium)
    tag = "SOURCE" if view == "source" else "DEPTH"
    canvas.draw_text(40, 7, tag)
    canvas.draw_text(PW - canvas.text_width("MODULATE") - 2, 7, "MODULATE")
    canvas.hline(0, 10, PW)

    # engine column headers (number + engine letter); cursor column brightened
    hdr_y = 17
    for t in range(8):
        cx = grid_l + t * col_w + col_w // 2
        lbl = f"{t+1}{engines[t]}"
        if t == cursor_col:
            canvas.set_color(Color.Bright)
        else:
            canvas.set_color(Color.Medium if engines[t] == "N" else Color.Low)
        canvas.draw_text(cx - canvas.text_width(lbl) // 2, hdr_y, lbl)

    top = 20
    rh = 8
    for r, (name, source, depths) in enumerate(rows):
        y = top + r * rh
        canvas.set_color(Color.Bright if r == cursor_row else Color.Medium)
        canvas.draw_text(2, y + 6, name)
        for t in range(8):
            cx = grid_l + t * col_w + col_w // 2
            eligible = engines[t] == "N"  # PITCH params: Note tracks only
            in_type = bytype and r == cursor_row and engines[t] == engines[cursor_col]
            is_cursor = (r == cursor_row and t == cursor_col)
            if not eligible:
                canvas.set_color(Color.Low)
                canvas.draw_text(cx - 2, y + 6, "-")
                continue
            d = depths.get(t) if depths else None
            if d is not None:
                txt = source if view == "source" else f"{d:+d}"
                canvas.set_color(Color.Bright if is_cursor else Color.Medium)  # cursor = brightness
            else:
                txt = "."
                canvas.set_color(Color.Bright if is_cursor else Color.MediumLow)
            canvas.draw_text(cx - canvas.text_width(txt) // 2, y + 6, txt)
            # by-type group: thin underline (distinct from the brightness cursor cue)
            if in_type:
                canvas.set_color(Color.Medium)
                canvas.hline(cx - col_w // 2 + 3, y + rh - 1, col_w - 5)

    _footer(canvas, ["", "SRC", "COMBINE", "", "COMMIT"], commit=False)


def render_mod_matrix(canvas):
    _matrix(canvas, bytype=False, view="depth")


def render_mod_matrix_bytype(canvas):
    _matrix(canvas, bytype=True, view="depth")


def render_mod_matrix_src(canvas):
    _matrix(canvas, bytype=False, view="source")
