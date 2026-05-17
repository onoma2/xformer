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

def render_stochastic_pitch_circle(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """Pitch class circle (Automatonnetz / OC_menus derived). Dot radius = ticket weight. Chord triangle connecting top 3."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH CIR")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    import math
    cx = 80
    cy = 34
    radius = 20
    tickets = sequence.degree_tickets()
    scale_size = min(12, sequence.scale_size())
    positions = []
    for i in range(scale_size):
        angle = math.radians(i * 360 / scale_size - 90)
        positions.append((int(cx + radius * math.cos(angle)), int(cy + radius * math.sin(angle))))
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    sorted_degrees = sorted(range(scale_size), key=lambda i: tickets[i] if i < len(tickets) and tickets[i] >= 0 else -999, reverse=True)
    top3 = [d for d in sorted_degrees[:3] if d < len(tickets) and tickets[d] >= 0]
    if len(top3) >= 3:
        canvas.set_color(Color.MediumBright)
        for i in range(len(top3)):
            x1, y1 = positions[top3[i]]
            x2, y2 = positions[top3[(i + 1) % len(top3)]]
            dx, dy = x2 - x1, y2 - y1
            steps = max(abs(dx), abs(dy))
            if steps > 0:
                for s in range(steps + 1):
                    canvas.point(x1 + dx * s // steps, y1 + dy * s // steps)
    for i in range(scale_size):
        px, py = positions[i]
        selected = (i == selected_degree)
        t = tickets[i] if i < len(tickets) else 0
        if t < 0:
            canvas.set_color(Color.Low)
            canvas.draw_text(px - 2, py - 2, "X")
        elif t == 0:
            canvas.set_color(Color.Medium if selected else Color.Low)
            canvas.draw_rect(px - 1, py - 1, 3, 3)
        else:
            r = min(4, 1 + t // 2)
            canvas.set_color(Color.Bright if selected else Color.MediumBright)
            canvas.fill_rect(px - r, py - r, r * 2 + 1, r * 2 + 1)
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 3, cy - 2, f"{track_engine.current_note()}")
    canvas.set_color(Color.Medium)
    canvas.draw_text(140, 20, f"ROOT:{sequence.scale_root()}")
    canvas.draw_text(140, 28, f"SIZE:{scale_size}")
    canvas.draw_text(140, 36, f"LIN:{sequence.linearity()}")



