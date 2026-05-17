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

def render_stochastic_dice_shredder(canvas: Canvas, sequence, track_engine, axis: int = 0, active_cell: int = 5):
    """8x8 Cartesian grid (Shredder-derived) for 64-step dice view.
    Exact replica of Shredder.h DrawGrid() and DrawMeters().
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE SHR")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    # From Shredder.h: gfxFrame(1 + (8 * (s % 4)), 26 + (8 * (s / 4)), 5, 5)
    # 4x4 grid of 5x5 frames at 8px spacing, starting at (1, 26)
    # But we need 8x8 for 64 steps, so scale: 8x8 at 4px spacing with 3x3 frames
    grid_x0 = 1
    grid_y0 = 26
    cell_spacing = 4
    cell_size = 3
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    # Draw 8x8 grid frames
    for row in range(8):
        for col in range(8):
            step_index = row * 8 + col
            step = sequence.step(step_index)
            x = grid_x0 + col * cell_spacing
            y = grid_y0 + row * cell_spacing
            if axis == 0: prob = step.gate_probability()
            elif axis == 1: prob = step.note_probability()
            elif axis == 2: prob = step.length_probability()
            else: prob = step.retrigger_probability()
            is_current = (step_index == current_step)
            is_active = (step_index == active_cell)
            # Frame
            canvas.set_color(Color.Medium)
            canvas.draw_rect(x, y, cell_size, cell_size)
            # Fill by probability
            if prob >= 75: canvas.set_color(Color.Bright)
            elif prob >= 50: canvas.set_color(Color.MediumBright)
            elif prob > 0: canvas.set_color(Color.Medium)
            else: canvas.set_color(Color.None_)
            if prob > 0 or is_current:
                canvas.fill_rect(x + 1, y + 1, cell_size - 2, cell_size - 2)
            # Active cell: filled rect per Shredder.h line 305
            if is_active:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x, y, cell_size, cell_size)
    # Crosshairs: gfxDottedLine(3 + (8 * cxx), 26, 3 + (8 * cxx), 58, 2)
    # For 8x8 at 4px spacing: crosshair x = grid_x0 + 1 + (active_cell % 8) * cell_spacing
    cxx = active_cell % 8
    cxy = active_cell // 8
    ax = grid_x0 + 1 + cxx * cell_spacing
    ay = grid_y0 + 1 + cxy * cell_spacing
    canvas.set_color(Color.Bright)
    # Vertical dotted line through column
    for y in range(grid_y0, grid_y0 + 8 * cell_spacing, 2):
        canvas.point(ax, y)
    # Horizontal dotted line through row
    for x in range(grid_x0, grid_x0 + 8 * cell_spacing, 2):
        canvas.point(x, ay)



