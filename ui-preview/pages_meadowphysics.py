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

def render_stochastic_steps_meadowphysics(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    meadowphysics-derived horizontal tracker strips.
    4 rows of 2x2 px dots across 16 steps. Dot brightness = probability.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS MP")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    dot_size = 2
    padding = 6
    offset_x = 12
    offset_y = 16
    rows = [
        ("GATE", lambda s: s.gate_probability()),
        ("NOTE", lambda s: s.note_probability()),
        ("LEN", lambda s: s.length_probability()),
        ("RTRG", lambda s: s.retrigger_probability()),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter) in enumerate(rows):
        row_y = offset_y + padding * row_idx
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y + 2, label[:1])

        for i in range(16):
            step_index = step_offset + i
            step = sequence.step(step_index)
            x = offset_x + padding * i
            prob = getter(step)
            is_current = (step_index == current_step)

            if is_current:
                canvas.set_color(Color.Bright)
            elif prob >= 75:
                canvas.set_color(Color.MediumBright)
            elif prob > 0:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)

            canvas.fill_rect(x, row_y, dot_size, dot_size)

# ---------------------------------------------------------------------------
# Pitch variants
# ---------------------------------------------------------------------------





def render_stochastic_lock_meadowphysics(canvas: Canvas, sequence, track_engine):
    """
    meadowphysics-derived multi-voice tracker strips.
    4 rows of 2x2 dots = gate, note, length, retrigger of captured events.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK MP")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])

    current_step = track_engine.current_step()
    dot_size = 2
    padding = 6
    offset_x = 12
    offset_y = 16
    first = sequence.lock_first()
    last = sequence.lock_last()
    rows = [
        ("G", lambda e: e.gate()),
        ("N", lambda e: e.note()),
        ("L", lambda e: e.length()),
        ("R", lambda e: e.retrigger()),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter) in enumerate(rows):
        row_y = offset_y + padding * row_idx
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y + 2, label)

        for i in range(16):
            evt = sequence.lock_event(i)
            x = offset_x + padding * i
            val = getter(evt)
            is_current = (i == current_step)
            in_window = first <= i <= last

            if is_current:
                canvas.set_color(Color.Bright)
            elif in_window and val:
                canvas.set_color(Color.MediumBright)
            elif in_window:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)

            canvas.fill_rect(x, row_y, dot_size, dot_size)




