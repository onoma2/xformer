"""
Hybrid stochastic page renderers.
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter, SequencePainter
from tracks import (
    DiscreteMapSequence, DiscreteMapTrackEngine,
    NoteSequence, NoteTrackEngine,
    TuesdaySequence, TuesdayTrackEngine,
    StochasticSequence, StochasticTrackEngine,
    _ALGO_NAMES,
)

def render_stochastic_steps_bline(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    bline-derived stacked pattern bar charts.
    5 rows: gate, note, length, retrigger, condition.
    Each row = 16 vertical bars (3px wide). Bar top highlighted at current step.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS BLN")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    bar_w = 3
    bar_gap = 1
    base_y = 48
    rows = [
        ("G", lambda s: s.gate_probability(), 6),
        ("N", lambda s: s.note(), 12),
        ("L", lambda s: s.length() * 3, 6),
        ("R", lambda s: s.retrigger() * 2, 4),
        ("C", lambda s: s.condition(), 4),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter, scale) in enumerate(rows):
        row_y = base_y - row_idx * 9
        # Row label
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y - 1, label)

        for i in range(16):
            step_index = step_offset + i
            step = sequence.step(step_index)
            x = 12 + i * (bar_w + bar_gap)
            val = getter(step)
            h = min(8, val // scale) if scale > 0 else 0
            is_current = (step_index == current_step)

            # Bar body
            canvas.set_color(Color.MediumBright if is_current else Color.Low)
            canvas.vline(x, row_y - h, h)
            # Bar top
            canvas.set_color(Color.Bright if is_current else Color.Medium)
            canvas.point(x, row_y - h - 1)





def render_stochastic_pitch_bline(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """
    bline-derived pattern bars for pitch degrees.
    Improved version matching C++ implementation.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STOCHASTIC TICKETS")
    WindowPainter.draw_footer(canvas, ["TIX", "DROT", "MROT", "", "NEXT"])

    tickets = sequence.degree_tickets()
    scale_size = sequence.scale_size()
    
    # Simple bank logic for preview (show bank 1)
    bank = 0
    bank_size = min(16, scale_size - bank * 16)
    
    base_y = 46
    bar_max_h = 26
    bar_w = 5
    gap = 2
    
    total_w = bank_size * (bar_w + gap) - gap
    x_offset = max(8, (256 - total_w) // 2)

    if total_w > 256 - 16:
        bar_w = 3
        gap = 1
        total_w = bank_size * (bar_w + gap) - gap
        x_offset = max(8, (256 - total_w) // 2)

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(bank_size):
        degree_index = bank * 16 + i
        t = tickets[degree_index] if degree_index < len(tickets) else 0
        x = x_offset + i * (bar_w + gap)
        selected = (degree_index == selected_degree)

        if t < 0:
            # Excluded
            canvas.set_color(Color.Medium)
            canvas.line(x, base_y - 5, x + bar_w - 1, base_y)
            canvas.line(x, base_y, x + bar_w - 1, base_y - 5)
        elif t == 0:
            # Included but 0 tickets
            canvas.set_color(Color.Low)
            canvas.draw_rect(x, base_y - 2, bar_w - 1, 2)
        else:
            # Bar
            h = (t * bar_max_h) // 100
            h = max(1, h)
            canvas.set_color(Color.Bright if selected else Color.Medium)
            canvas.fill_rect(x, base_y - h, bar_w, h)

        if selected:
            canvas.set_color(Color.Bright)
            canvas.hline(x - 1, base_y + 2, bar_w + 2)

    # Info text
    canvas.set_color(Color.Bright)
    t_val = tickets[selected_degree] if selected_degree < len(tickets) else 0
    canvas.draw_text(8, 18, f"DEG {selected_degree + 1}: {t_val if t_val >= 0 else 'EXCL'}")
    
    # Page indicator if paged
    if scale_size > 16:
        canvas.set_color(Color.Low)
        canvas.draw_text(8, 28, f"P{bank + 1}/{(scale_size + 15) // 16}")

    canvas.set_color(Color.Medium)
    rot_str = f"DROT:{sequence.degree_rotation():+d} MROT:{sequence.mask_rotation():+d}"
    canvas.set_font(Font.Tiny)
    canvas.draw_text(256 - canvas.text_width(rot_str) - 12, 18, rot_str)

# ---------------------------------------------------------------------------
# Dice variants
# ---------------------------------------------------------------------------





def render_stochastic_marbles_bline(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """
    bline-derived XY position grid with crosshairs.
    Spread = grid cursor size. Bias = cursor position.
    4-quadrant parameter display.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE XY")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)

    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()

    # XY grid background
    grid_x = 10
    grid_y = 16
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    for row in range(4):
        for col in range(4):
            canvas.draw_rect(grid_x + col * 10, grid_y + row * 10, 9, 9)

    # Cursor position from bias
    cursor_col = 1 + (bias + 50) // 25
    cursor_row = 1 + (spread // 25)
    cursor_col = max(0, min(3, cursor_col))
    cursor_row = max(0, min(3, cursor_row))

    # Highlight current cell
    canvas.set_color(Color.Medium)
    canvas.fill_rect(grid_x + cursor_col * 10, grid_y + cursor_row * 10, 9, 9)

    # Crosshairs
    cx = grid_x + cursor_col * 10 + 4
    cy = grid_y + cursor_row * 10 + 4
    canvas.set_color(Color.Bright)
    for x in range(grid_x, grid_x + 40, 2):
        canvas.point(x, cy)
    for y in range(grid_y, grid_y + 40, 2):
        canvas.point(cx, y)

    # 4-quadrant param display right side
    params = [("SPRD", spread), ("BIAS", bias), ("STEP", sequence.marbles_steps()), ("MODE", sequence.marbles_mode())]
    for i, (name, val) in enumerate(params):
        y = 16 + i * 10
        canvas.set_color(Color.Bright if i == slot else Color.Medium)
        canvas.draw_text(60, y + 5, name)
        canvas.draw_text(100, y + 5, str(val))

# ---------------------------------------------------------------------------
# Lock variants
# ---------------------------------------------------------------------------





def render_stochastic_track_bline(canvas: Canvas, sequence, track_engine,
                                   current_page: int = 0, selected_slot: int = 0):
    """
    bline-derived 4-quadrant 2x2 parameter grid.
    Each cell = 19x19 px. Selected = bright background.
    """
    param_pages = [
        [("OCT", "octave"), ("TRANS", "transpose"), ("DIV", "division"), ("RESET", "reset_mode")],
        [("GBIAS", "gbias"), ("NBIAS", "nbias"), ("LBIAS", "lbias"), ("RBIAS", "rbias")],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    cell_w = 19
    cell_h = 19
    grid_x = 10
    grid_y = 14

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK BLN P{current_page+1}")

    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr) in enumerate(page):
        col = i % 2
        row = i // 2
        x = grid_x + col * (cell_w + 1)
        y = grid_y + row * (cell_h + 1)
        val = getattr(sequence, attr)()
        selected = (i == selected_slot)

        # Cell background
        if selected:
            canvas.set_color(Color.Medium)
            canvas.fill_rect(x, y, cell_w, cell_h)
            text_color = Color.Bright
        else:
            canvas.set_color(Color.Low)
            canvas.fill_rect(x, y, cell_w, cell_h)
            text_color = Color.Medium

        # Label
        canvas.set_color(text_color)
        canvas.draw_text(x + 2, y + 6, name)
        # Value
        canvas.set_color(text_color)
        canvas.draw_text(x + 2, y + 14, str(val))

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)




