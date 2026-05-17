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

def render_stochastic_dice_grd(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """
    grd-derived 8x8 brightness grid.
    5x5 px rects, brightness = probability for active axis.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE GRD")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    current_step = track_engine.current_step()
    # Exact params from grd.lua: rect(i*6, j*6, 5, 5) -> spacing=6, cell=5
    cell_size = 5
    spacing = 6
    grid_x0 = 8
    grid_y0 = 14

    canvas.set_blend_mode(BlendMode.Set)
    for row in range(8):
        for col in range(8):
            step_index = row * 8 + col
            step = sequence.step(step_index)
            x = grid_x0 + col * spacing
            y = grid_y0 + row * spacing

            if axis == 0:
                prob = step.gate_probability()
            elif axis == 1:
                prob = step.note_probability()
            elif axis == 2:
                prob = step.length_probability()
            else:
                prob = step.retrigger_probability()

            is_current = (step_index == current_step)

            # Brightness mapping
            if is_current:
                canvas.set_color(Color.Bright)
            elif prob >= 75:
                canvas.set_color(Color.MediumBright)
            elif prob >= 50:
                canvas.set_color(Color.Medium)
            elif prob > 0:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.None_)

            canvas.fill_rect(x, y, cell_size - 1, cell_size - 1)




