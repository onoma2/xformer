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

def render_stochastic_steps_hiswing(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    Hiswing-derived vertical line chart.
    Exact params from Hiswing.lua: width=120/16, x=4+(i-1)*width, base_y=64.
    Scaled 2x for 256px canvas: width=15, x=8+i*15, base_y=52.
    Swing offset for even steps: +3px (scaled from +1..5 range).
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS HSW")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    # Exact Hiswing params scaled 2x for 256px canvas
    base_y = 52       # scaled from 64, clearing footer at 53
    step_w = 15       # 120/16 = 7.5, scaled 2x = 15
    x0 = 8            # scaled from 4

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        # Exact formula: x = 4 + (i-1)*width, scaled: x = 8 + i*15
        x = x0 + i * step_w

        # Swing offset for even steps (Hiswing get_visual_swing_offset)
        if i % 2 == 1:
            x += 3  # scaled swing offset

        # Height from note value (Hiswing: util.linlin(24, 84, 4, 40, note))
        note_h = min(30, (step.note() * 4) // 3 + 4)
        prob = step.gate_probability()
        is_current = (step_index == current_step)
        is_selected = (i == 3)  # demo selected

        if is_current:
            # Current step: line 4px higher (Hiswing selected_step behavior)
            canvas.set_color(Color.Bright)
            canvas.vline(x, base_y - note_h - 4, note_h + 4)
        elif is_selected:
            canvas.set_color(Color.MediumBright)
            canvas.vline(x, base_y - note_h, note_h)
        else:
            # Probability = brightness
            if prob >= 75:
                canvas.set_color(Color.Medium)
            elif prob >= 50:
                canvas.set_color(Color.MediumBright)
            elif prob > 0:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.None_)
            canvas.vline(x, base_y - note_h, note_h)




