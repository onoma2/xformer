#!/usr/bin/env python3
"""
Proposed TT2 script-page live-value HUD (U3b).

Renders the script page (edit mode) with the right-side HUD restored from TT2
state: CV(8) / TR(8) / IN / PARAM / BUS(4) activity. NO routing assignment or
ownership coloring (deferred). Output: ui-preview/tt2-script/<variant>.png
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, Color, BlendMode, framebuffer_to_image

WIDTH = 256
HEIGHT = 64

# Left edit area (mirrors the .cpp constants)
ROW_START_Y = 4
ROW_STEP_Y = 8
LABEL_X = 4
TEXT_X = 16
EDIT_LINE_Y = 54

# HUD region
HUD_X = 192          # left text is clamped to HUD_X - 2
COL_PITCH = 8        # 8 output columns
COL_W = 6
COL_X0 = HUD_X + 0


def draw_vbar(canvas, x, y, w, h, value_raw, fill, outline):
    """Bipolar bar: 0..16383, 8192 = center. Mirrors drawBipolarBar."""
    canvas.set_color(outline)
    canvas.draw_rect(x, y, w, h)
    val = value_raw - 8192
    half = (h - 2) // 2
    bar = int(abs(val) * half / 8192)
    bar = max(0, min(half, bar))
    cy = y + h // 2
    if bar > 0:
        canvas.set_color(fill)
        if val >= 0:
            canvas.fill_rect(x + 1, cy - bar, w - 2, bar)
        else:
            canvas.fill_rect(x + 1, cy + 1, w - 2, bar)


def render_tt2_script(canvas, lines, edit_text, script_label,
                      cv, tr, ti, cv_in, param, bus):
    canvas.set_color(Color.None_)
    canvas.fill()
    canvas.set_blend_mode(BlendMode.Set)

    # --- script label (top right) --- (Tele font unavailable in harness; Tiny stands in)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    lbl_w = canvas.text_width(script_label)
    canvas.draw_text(WIDTH - 2 - lbl_w, 8, script_label)

    # --- script lines (edit mode) ---
    max_text_w = HUD_X - 2 - TEXT_X
    for i, text in enumerate(lines):
        y = ROW_START_Y + i * ROW_STEP_Y
        canvas.set_color(Color.Bright if i == 0 else Color.Medium)
        canvas.draw_text(LABEL_X, y + 4, str(i + 1))
        # clamp
        t = text
        while t and canvas.text_width(t) > max_text_w:
            t = t[:-1]
        canvas.draw_text(TEXT_X, y + 4, t)

    # --- edit line ---
    canvas.set_color(Color.Bright)
    canvas.draw_text(LABEL_X, EDIT_LINE_Y + 4, "> " + edit_text)

    # --- HUD ---
    canvas.set_font(Font.Tiny)

    # CV row (8 bipolar bars)
    cv_top, cv_h = 12, 15
    for i in range(8):
        x = COL_X0 + i * COL_PITCH
        draw_vbar(canvas, x, cv_top, COL_W, cv_h, cv[i],
                  Color.MediumBright, Color.Low)

    # TR row (8 gate cells) - same width as the CV bars above; firing lights
    # only the inner surface (frame always drawn, inner fill inset).
    tr_top = 30
    for i in range(8):
        x = COL_X0 + i * COL_PITCH
        canvas.set_color(Color.Low)
        canvas.draw_rect(x, tr_top, COL_W, COL_W)
        if tr[i]:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x + 1, tr_top + 1, COL_W - 2, COL_W - 2)

    # bottom row: TI squares + IN / PARAM / BUS bars. No letter labels -
    # fixed positions, learned by location. Squares vs bars + brightness group
    # them: TI = gate squares, IN/PARAM = bright bipolar, BUS = dim bipolar.
    # TI squares align under the top columns 0-3 (left edge flush with the top row).
    # 8 trigger inputs as 2 rows (1-4 top, 5-8 bottom) under columns 0-3, brought
    # up so the dashboard stays clear of the IN/PARAM/BUS bars on the right.
    sq_y0, sq_pitch = 38, 7
    for i in range(8):
        col, row = i % 4, i // 4
        x = COL_X0 + col * COL_PITCH
        y = sq_y0 + row * sq_pitch
        canvas.set_color(Color.Low)
        canvas.draw_rect(x, y, COL_W, COL_W)
        if i < len(ti) and ti[i]:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x + 1, y + 1, COL_W - 2, COL_W - 2)
    # IN / PARAM / BUS bars fill the right span; last BUS edge flush with the top row.
    bar_y, bar_h = 38, 13
    draw_vbar(canvas, 225, bar_y, 4, bar_h, cv_in, Color.MediumBright, Color.Low)   # IN
    draw_vbar(canvas, 230, bar_y, 4, bar_h, param, Color.MediumBright, Color.Low)   # PARAM
    for i, x in enumerate((235, 240, 245, 250)):       # 4 BUS lanes (250+4 = 254)
        draw_vbar(canvas, x, bar_y, 4, bar_h, bus[i], Color.MediumLow, Color.Low)


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tt2-script")
    os.makedirs(out_dir, exist_ok=True)

    lines = [
        "TR.P 1",
        "CV 1 N 7",
        "CV 2 RND V 5",
        "IF EZ X : TR 3 1",
        "M.ACT 1",
        "",
    ]
    edit_text = "CV 3 V 2"
    cv = [12000, 4000, 8192, 16000, 6200, 8192, 10500, 2000]
    tr = [1, 0, 1, 0, 0, 1, 0, 0]
    ti = [1, 0, 1, 1, 0, 1, 1, 0]

    fb = FrameBuffer(WIDTH, HEIGHT)
    canvas = Canvas(fb, brightness=1.0)
    render_tt2_script(canvas, lines, edit_text, "S1", cv, tr, ti,
                      cv_in=11000, param=5000, bus=[9000, 8192, 12500, 3000])
    img = framebuffer_to_image(fb, scale=4)
    path = os.path.join(out_dir, "tt2-script-hud-2row.png")
    img.save(path)
    print("Saved", path)


if __name__ == "__main__":
    main()
