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

def render_stochastic_pitch_less_concepts(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """Dense less-concepts-derived pitch page. 8-bit seed pattern, large note name, parameter text, preset grid."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH DNS")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    tickets = sequence.degree_tickets()
    scale_size = sequence.scale_size()
    canvas.set_blend_mode(BlendMode.Set)
    for i in range(min(8, scale_size)):
        t = tickets[i] if i < len(tickets) else 0
        canvas.set_color(Color.Bright if i == selected_degree else (Color.Medium if t > 0 else Color.Low))
        canvas.fill_rect(5 * i, 14, 4, 4)
    canvas.set_color(Color.Bright)
    canvas.hline(5 * selected_degree, 13, 4)
    current_note = track_engine.current_note()
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    nn = note_names[current_note % 12]
    canvas.set_color(Color.Bright)
    canvas.draw_text(60, 28, nn)
    canvas.set_color(Color.Medium)
    canvas.draw_text(62, 26, nn)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(120, 20, f"deg:{selected_degree+1}")
    canvas.draw_text(120, 28, f"tix:{tickets[selected_degree] if selected_degree < len(tickets) else 0}")
    canvas.draw_text(120, 36, f"rot:{sequence.degree_rotation():+d}")
    canvas.draw_text(120, 44, f"lin:{sequence.linearity()}")
    canvas.draw_text(180, 20, f"min:{sequence.min_degree()}")
    canvas.draw_text(180, 28, f"max:{sequence.max_degree()}")
    canvas.draw_text(180, 36, f"root:{sequence.scale_root()}")
    # Preset grid moved up from y=53-59 (footer collision) to y=40-47 (safe area)
    grid_y0 = 40
    for i in range(2):
        for j in range(8):
            idx = j + i * 8
            active = idx < scale_size and tickets[idx] >= 0
            canvas.set_color(Color.Medium if active else Color.Low)
            canvas.fill_rect(j * 5, grid_y0 + i * 5, 4, 4)
            if idx == selected_degree:
                canvas.set_color(Color.Bright)
                canvas.draw_rect(j * 5 - 1, grid_y0 - 1 + i * 5, 6, 6)



