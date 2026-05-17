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

def render_stochastic_dice_register(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """64-step register bar horizon (ShiftReg-derived).
    Exact replica of ShiftReg.h register bars: gfxRect(60-(4*b), 47, 3, 14).
    Four 16-bit registers side-by-side to show 64 steps.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE REG")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    # Exact params from ShiftReg.h: bar width=3, height=14, spacing=4, y=47
    bar_w = 3
    bar_h = 14
    spacing = 4
    base_y = 24  # moved up from 47 to clear footer, keeping exact bar height
    reg_width = 64  # 16 bars * 4px spacing
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(4):
        reg_x0 = row * reg_width
        for col in range(16):
            step_index = row * 16 + col
            step = sequence.step(step_index)
            # Exact x formula from ShiftReg.h: 60 - (4 * b), scaled per register
            x = reg_x0 + (60 - (spacing * col))
            if axis == 0: prob = step.gate_probability()
            elif axis == 1: prob = step.note_probability()
            elif axis == 2: prob = step.length_probability()
            else: prob = step.retrigger_probability()
            is_current = (step_index == current_step)
            # Bar height proportional to probability
            h = (prob * bar_h) // 100
            if is_current:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x, base_y, bar_w, bar_h)
            elif h > 0:
                canvas.set_color(Color.MediumBright if prob >= 50 else Color.Medium)
                canvas.fill_rect(x, base_y + bar_h - h, bar_w, h)
            else:
                canvas.set_color(Color.Low)
                canvas.vline(x + 1, base_y + bar_h - 3, 3)
    # Separator lines between registers
    canvas.set_color(Color.Low)
    for row in range(1, 4):
        canvas.vline(row * reg_width - 1, base_y, bar_h)



