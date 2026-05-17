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

def render_stochastic_pitch_kreislauf(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """
    Kreislauf-derived concentric ring wedges.
    Scale degrees as wedges in a ring. Brightness = ticket count.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH KRS")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])

    import math
    cx = 80
    cy = 34
    r_outer = 22
    r_inner = 12
    tickets = sequence.degree_tickets()
    scale_size = min(16, sequence.scale_size())

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(scale_size):
        t = tickets[i] if i < len(tickets) else 0
        selected = (i == selected_degree)

        angle1 = math.radians(i * 360 / scale_size - 90)
        angle2 = math.radians((i + 1) * 360 / scale_size - 90)

        # Wedge brightness by ticket count
        if t < 0:
            canvas.set_color(Color.Low)
        elif t == 0:
            canvas.set_color(Color.Medium)
        elif t >= 5:
            canvas.set_color(Color.Bright if selected else Color.MediumBright)
        else:
            canvas.set_color(Color.MediumBright if selected else Color.Medium)

        # Draw wedge as filled polygon (4-point)
        p1 = (int(cx + r_outer * math.cos(angle1)), int(cy + r_outer * math.sin(angle1)))
        p2 = (int(cx + r_outer * math.cos(angle2)), int(cy + r_outer * math.sin(angle2)))
        p3 = (int(cx + r_inner * math.cos(angle2)), int(cy + r_inner * math.sin(angle2)))
        p4 = (int(cx + r_inner * math.cos(angle1)), int(cy + r_inner * math.sin(angle1)))

        # Simple fill: scanline between inner and outer radius
        for a in range(int(math.degrees(angle1)), int(math.degrees(angle2)) + 1):
            rad = math.radians(a)
            xo = int(cx + r_outer * math.cos(rad))
            yo = int(cy + r_outer * math.sin(rad))
            xi = int(cx + r_inner * math.cos(rad))
            yi = int(cy + r_inner * math.sin(rad))
            dx = xo - xi
            dy = yo - yi
            steps = max(abs(dx), abs(dy))
            if steps > 0:
                for s in range(steps + 1):
                    canvas.point(xi + dx * s // steps, yi + dy * s // steps)

        # Wedge outline
        canvas.set_color(Color.Bright if selected else Color.Medium)
        for s in range(100):
            canvas.point(p1[0] + (p2[0]-p1[0])*s//100, p1[1] + (p2[1]-p1[1])*s//100)
            canvas.point(p2[0] + (p3[0]-p2[0])*s//100, p2[1] + (p3[1]-p2[1])*s//100)
            canvas.point(p3[0] + (p4[0]-p3[0])*s//100, p3[1] + (p4[1]-p3[1])*s//100)
            canvas.point(p4[0] + (p1[0]-p4[0])*s//100, p4[1] + (p1[1]-p4[1])*s//100)

    # Center info
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 3, cy - 2, f"{scale_size}")





def render_stochastic_dice_kreislauf(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """
    Kreislauf-derived concentric ring probability wheel.
    Exact formula from kreislauf_app.lua: r=6, r=r+(width/num_rings)/2.5, inner=r-6.
    4 rings = 4 probability layers. 16 wedges per ring.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE KRS")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    import math
    cx = 80
    cy = 34
    # Exact kreislauf formula: start r=6, increment=(width/4)/2.5, inner=r-6
    width = 40
    increment = (width / 4) / 2.5  # = 4.0
    r = 6
    ring_radii = []
    for _ in range(4):
        r = r + increment
        ring_radii.append((r - 6, r))  # (inner, outer)
    rings = [
        ("GATE", lambda s: s.gate_probability(), ring_radii[0][1], ring_radii[0][0]),
        ("NOTE", lambda s: s.note_probability(), ring_radii[1][1], ring_radii[1][0]),
        ("LEN", lambda s: s.length_probability(), ring_radii[2][1], ring_radii[2][0]),
        ("RTRG", lambda s: s.retrigger_probability(), ring_radii[3][1], ring_radii[3][0]),
    ]
    current_step = track_engine.current_step()

    canvas.set_blend_mode(BlendMode.Set)
    for ring_idx, (label, getter, r_out, r_in) in enumerate(rings):
        for i in range(16):
            step_index = i
            step = sequence.step(step_index)
            prob = getter(step)
            is_current = (step_index == current_step)

            angle1 = math.radians(i * 360 / 16 - 90)
            angle2 = math.radians((i + 1) * 360 / 16 - 90)

            # Brightness
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

            # Draw wedge radial line (kreislauf stueck style)
            for a in range(int(math.degrees(angle1)), int(math.degrees(angle2)) + 1):
                rad = math.radians(a)
                xo = int(cx + r_out * math.cos(rad))
                yo = int(cy + r_out * math.sin(rad))
                xi = int(cx + r_in * math.cos(rad))
                yi = int(cy + r_in * math.sin(rad))
                dx = xo - xi
                dy = yo - yi
                steps = max(abs(dx), abs(dy))
                if steps > 0:
                    for s in range(steps + 1):
                        canvas.point(xi + dx * s // steps, yi + dy * s // steps)

    # Labels right side
    for ring_idx, (label, getter, r_out, r_in) in enumerate(rings):
        canvas.set_color(Color.Medium)
        canvas.draw_text(140, 16 + ring_idx * 8, label)

# ---------------------------------------------------------------------------
# Marbles variants
# ---------------------------------------------------------------------------




