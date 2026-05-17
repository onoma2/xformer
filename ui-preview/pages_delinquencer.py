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

def render_stochastic_steps_delinquencer(canvas: Canvas, sequence, track_engine, section: int = 0):
    """Steps as 4x16 delinquencer grid with corner-pixel probability erosion."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS DEL")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    step_offset = section * 16
    current_step = track_engine.current_step()
    cell_size = 5
    cell_gap = 2
    grid_x0 = 8
    grid_y0 = 14
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(4):
        for col in range(16):
            step_index = step_offset + row * 16 + col
            if step_index >= 64: break
            step = sequence.step(step_index)
            x = grid_x0 + col * (cell_size + cell_gap)
            y = grid_y0 + row * (cell_size + cell_gap)
            prob = step.gate_probability()
            if prob == 100: canvas.set_color(Color.Bright)
            elif prob >= 50: canvas.set_color(Color.MediumBright)
            elif prob > 0: canvas.set_color(Color.Medium)
            else: canvas.set_color(Color.Low)
            canvas.draw_rect(x, y, cell_size, cell_size)
            if prob > 0:
                canvas.fill_rect(x + 1, y + 1, cell_size - 2, cell_size - 2)
            if 0 < prob < 100:
                corners = 4 - (prob // 25)
                canvas.set_blend_mode(BlendMode.Set)
                canvas.set_color(Color.None_)
                if corners >= 1: canvas.point(x + 1, y + 1)
                if corners >= 2: canvas.point(x + cell_size - 2, y + 1)
                if corners >= 3: canvas.point(x + 1, y + cell_size - 2)
                if corners >= 4: canvas.point(x + cell_size - 2, y + cell_size - 2)
                canvas.set_blend_mode(BlendMode.Set)
            if step_index == current_step:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x + 2, y + 2, 2, 2)



