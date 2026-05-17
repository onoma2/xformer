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

def render_stochastic_steps_euclid(canvas: Canvas, sequence, track_engine, section: int = 0):
    """Steps arranged on a Euclidean ring (squares-and-circles derived). Square = deterministic gate on. Circle = probabilistic/off."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS EUC")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    import math
    # From EuclidRythm.cpp: draw_eclid_cyrcle(x, y, ...) with circle at (x+3, 31+3), radius=21
    # Two rings side by side at x=0 and x=64 on 128px screen
    cx = 3 + 32   # left ring center x (scaled to fit 256px canvas: 3*2=6, but use 35 for centering)
    cy = 34       # circle center y = 31+3
    radius = 21   # exact radius from EuclidRythm.cpp
    step_offset = section * 16
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    # Guide circle
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        angle = math.radians(i * 360 / 16 - 90)
        px = int(cx + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))
        prob = step.gate_probability()
        # Cursor: 5x5 hollow rect at (x+1, y+1) per EuclidRythm.cpp
        if step_index == current_step:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(px + 1, py + 1, 5, 5)
        # Active/deterministic: 3x3 fillRect at (x+2, y+2)
        if step.gate() and prob >= 75:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px + 2, py + 2, 3, 3)
        elif prob > 0:
            # Probabilistic: drawCircle(x+3, y+3, 3) -> hollow circle radius 3
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px + 1, py + 1, 5, 5)
        else:
            canvas.set_color(Color.Low)
            canvas.point(px + 3, py + 3)
    # Second ring (note probability) at x=64 on 128px screen -> 128 on 256px
    cx2 = 128 + 3
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx2 + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    for i in range(16):
        step = sequence.step(step_offset + i)
        angle = math.radians(i * 360 / 16 - 90)
        px = int(cx2 + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))
        np = step.note_probability()
        if np >= 75:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px + 2, py + 2, 3, 3)
        elif np > 0:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px + 1, py + 1, 5, 5)
        else:
            canvas.set_color(Color.Low)
            canvas.point(px + 3, py + 3)



