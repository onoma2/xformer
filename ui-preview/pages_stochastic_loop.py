"""
Stochastic Loop page — proposed layout with side bars for MaskR/TiltR
(left margin) and MaskM/TiltM (right margin), replacing the four scattered
tiny chips that were colliding with the held-step Small label (y=18) and
the footer band (y=53+).
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter


def render_stochastic_loop_proposed(canvas: Canvas,
                                    patience_r=50, patience_m=70,
                                    mutate=30, jump=2, sleep=0,
                                    mask_r=40, tilt_r=60,
                                    mask_m=80, tilt_m=20,
                                    first=2, last=13, size=16, rotate=0,
                                    current_step=5,
                                    held_step=-1,
                                    fill_r_pct=35, fill_m_pct=55):
    """
    256x64 OLED render of the Stochastic LOOP page with the proposed
    side-bar treatment for the four mask/tilt knobs.
    """
    margin = 10
    tape_top = 22
    tape_bot = 40
    tape_h = tape_bot - tape_top + 1
    avail_w = PageWidth - 2 * margin

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LOOP")

    canvas.set_blend_mode(BlendMode.Set)

    # ---------------------------------------------------------------
    # Side bars — MaskR / TiltR (left), MaskM / TiltM (right).
    # 2 px wide each, 2 px gap, climb from tape bottom proportional
    # to value/100 across tape_h rows.
    # ---------------------------------------------------------------
    def draw_side_bar(x, value, default=50):
        col = Color.Low if value == default else Color.Medium
        canvas.set_color(Color.Low)
        canvas.draw_rect(x, tape_top, 2, tape_h)
        fill_h = (value * (tape_h - 2)) // 100
        if fill_h > 0:
            canvas.set_color(col)
            canvas.fill_rect(x, tape_bot - fill_h, 2, fill_h)

    # Left margin (R domain): outer = Mask, inner = Tilt
    draw_side_bar(2, mask_r)
    draw_side_bar(6, tilt_r)
    # Right margin (M domain): mirror — inner = Tilt, outer = Mask
    draw_side_bar(PageWidth - 4, mask_m)
    draw_side_bar(PageWidth - 8, tilt_m)

    # ---------------------------------------------------------------
    # Tape outline + step cells
    # ---------------------------------------------------------------
    canvas.set_color(Color.Low)
    canvas.draw_rect(margin, tape_top, avail_w, tape_h)

    # Simple even slots for the preview (16 slots).
    step_w = avail_w // 16
    for i in range(16):
        x = margin + i * step_w
        in_window = first <= i <= last
        is_active = i == current_step
        # Cell pitch shading — fake values for the preview.
        pitch_norm = ((i * 7 + 3) % 13) / 12.0
        bar_h = 1 + int(pitch_norm * (tape_h - 3))
        if is_active:
            cell_col = Color.Bright
            fill_col = Color.Bright
        elif in_window:
            cell_col = Color.MediumBright
            fill_col = Color.Medium
        else:
            cell_col = Color.Low
            fill_col = Color.Low
        canvas.set_color(cell_col)
        canvas.draw_rect(x, tape_top, step_w, tape_h)
        canvas.set_color(fill_col)
        canvas.fill_rect(x + 1, tape_bot - bar_h, step_w - 2, bar_h)

    # ---------------------------------------------------------------
    # Window brackets
    # ---------------------------------------------------------------
    bx_first = margin + first * step_w
    bx_last = margin + (last + 1) * step_w - 1
    canvas.set_color(Color.Bright)
    canvas.vline(bx_first - 1, tape_top - 3, tape_h + 6)
    canvas.hline(bx_first - 1, tape_top - 3, 4)
    canvas.hline(bx_first - 1, tape_bot + 3, 4)
    canvas.vline(bx_last + 1, tape_top - 3, tape_h + 6)
    canvas.hline(bx_last - 2, tape_top - 3, 4)
    canvas.hline(bx_last - 2, tape_bot + 3, 4)

    # ---------------------------------------------------------------
    # Patience halves under the tape (y=47..49)
    # ---------------------------------------------------------------
    gap = 2
    half_w = (avail_w - gap) // 2
    left_x = margin
    right_x = margin + half_w + gap
    canvas.set_color(Color.Low)
    canvas.draw_rect(left_x, tape_bot + 7, half_w, 3)
    canvas.draw_rect(right_x, tape_bot + 7, half_w, 3)
    canvas.set_color(Color.MediumBright)
    canvas.fill_rect(left_x, tape_bot + 7, (fill_r_pct * half_w) // 100, 3)
    canvas.fill_rect(right_x, tape_bot + 7, (fill_m_pct * half_w) // 100, 3)

    # ---------------------------------------------------------------
    # Top readout — Tiny font, only when no step held.
    # Five chips (PR/PM/MU/JU/SL), center-aligned in the band between
    # the side bars.
    # ---------------------------------------------------------------
    canvas.set_font(Font.Tiny)
    if held_step < 0:
        chips = [
            f"PR{patience_r}",
            f"PM{patience_m}",
            f"MU{mutate}",
            f"JU{jump}",
            f"SL{sleep}",
        ]
        gap = 6
        widths = [canvas.text_width(c) for c in chips]
        total = sum(widths) + gap * (len(chips) - 1)
        band_x0 = margin
        band_x1 = PageWidth - margin
        x = band_x0 + ((band_x1 - band_x0) - total) // 2
        canvas.set_color(Color.Medium)
        for c, w in zip(chips, widths):
            canvas.draw_text(x, 16, c)
            x += w + gap
    else:
        canvas.set_color(Color.Bright)
        labels = {
            0:  f"PATIENCE R {patience_r}",
            1:  f"PATIENCE M {patience_m}",
            2:  f"MUTATE {mutate}",
            3:  f"JUMP {jump}",
            4:  f"SLEEP {sleep}",
            6:  f"MASK MELODY {mask_m}",
            7:  f"TILT MELODY {tilt_m}",
            8:  f"FIRST {first}",
            9:  f"SIZE {size}",
            10: f"ROTATE {rotate:+d}",
            14: f"MASK RHYTHM {mask_r}",
            15: f"TILT RHYTHM {tilt_r}",
        }
        canvas.set_font(Font.Small)
        canvas.draw_text(8, 18, labels.get(held_step, "?"))

    # ---------------------------------------------------------------
    # Footer
    # ---------------------------------------------------------------
    WindowPainter.draw_footer(canvas, ["LoopR", "LoopM", "NewR", "NewM", "NEXT"])
