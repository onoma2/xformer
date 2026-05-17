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

def render_stochastic_pitch_prob_melod(canvas: Canvas, sequence, track_engine, weights=None, active_note=4, active_octave=0):
    """Full ProbMeloD keyboard + weight bars for stochastic pitch page.
    Exact replica of ProbabilityMelody.h drawing at lines 342-418.
    Uses exact x-positions, key dimensions, and bar offsets from O_C source.
    """
    if weights is None:
        tickets = sequence.degree_tickets()
        weights = [min(10, max(-1, tickets[i])) if i < len(tickets) else 0 for i in range(12)]
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH KEY")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    # Exact x-table from ProbabilityMelody.h line 230 (128px screen coords)
    semitone_x = [2, 7, 10, 15, 18, 26, 31, 34, 39, 42, 47, 50]
    # Black-key pattern: 1=black, 0=white
    is_black = [0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0]
    # Note name chars (sharps share letter with natural)
    note_letters = ['C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B']
    canvas.set_blend_mode(BlendMode.Set)
    # --- Weight bars (DrawParams, lines 364-384) ---
    for i, w in enumerate(weights):
        # xOffset = x[i] + (p[i] ? 1 : 2)
        x_off = semitone_x[i] + (1 if is_black[i] else 2)
        # yOffset = p[i] ? 31 : 45
        y_off = 31 if is_black[i] else 45
        unmasked = (w >= 0)
        if unmasked:
            # Dotted vertical line
            canvas.set_color(Color.Medium)
            for dy in range(11):
                canvas.point(x_off, y_off + dy)
            # Weight tick: gfxLine(xOffset-1, yOffset+10-ws[i], xOffset+1, yOffset+10-ws[i])
            tick_y = y_off + 10 - w
            canvas.set_color(Color.Bright)
            canvas.hline(x_off - 1, tick_y, 3)
        else:
            # Excluded: draw X at bar position
            canvas.set_color(Color.Low)
            canvas.draw_text(x_off - 2, y_off + 4, "X")
    # --- Keyboard (DrawKeyboard, lines 342-362) ---
    # Keyboard frame: gfxFrame(0, 27, 63, 32)
    canvas.set_color(Color.Medium)
    canvas.draw_rect(0, 27, 63, 32)
    # White-key vertical lines
    for x in range(8):
        if x == 3 or x == 7:
            # Full-height line: gfxLine(x*8, 27, x*8, 58)
            canvas.vline(x * 8, 27, 32)
        else:
            # Lower line: gfxLine(x*8, 43, x*8, 58)
            canvas.vline(x * 8, 43, 16)
    # Black keys: gfxInvert((i*8)+6, 28, 5, 15) for i in [0,1,3,4,5] (skip i=2)
    black_key_indices = [0, 1, 3, 4, 5]
    for i in black_key_indices:
        bx = (i * 8) + 6
        canvas.set_color(Color.Bright)
        canvas.fill_rect(bx, 28, 5, 15)
    # --- Active note triangles (lines 385-389) ---
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    canvas.set_color(Color.Bright)
    # Draw note letter above keyboard for active note
    nx = semitone_x[active_note] + (1 if is_black[active_note] else 2) - 3
    canvas.draw_text(nx, 59, note_names[active_note])
    # Octave indicator at right
    canvas.draw_text(56, 59, f"O{active_octave}")
    # --- Range fences (parameter area, approximated) ---
    canvas.set_color(Color.Bright)
    if 0 <= sequence.min_degree() < 12:
        fx = semitone_x[sequence.min_degree()] + (1 if is_black[sequence.min_degree()] else 2)
        canvas.vline(fx, 20, 30)
    if 0 <= sequence.max_degree() < 12:
        fx = semitone_x[sequence.max_degree()] + (1 if is_black[sequence.max_degree()] else 2)
        canvas.vline(fx, 20, 30)



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
    for i in range(2):
        for j in range(8):
            idx = j + i * 8
            active = idx < scale_size and tickets[idx] >= 0
            canvas.set_color(Color.Medium if active else Color.Low)
            canvas.fill_rect(j * 5, 54 + i * 5, 4, 4)
            if idx == selected_degree:
                canvas.set_color(Color.Bright)
                canvas.draw_rect(j * 5 - 1, 53 + i * 5, 6, 6)



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




def render_stochastic_pitch_bline(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """
    bline-derived pattern bars for pitch degrees.
    3px wide vertical bars, height = tickets. Current degree = bright top.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH BLN")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])

    tickets = sequence.degree_tickets()
    scale_size = sequence.scale_size()
    bar_w = 3
    bar_gap = 1
    base_y = 48
    bar_max_h = 30

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(scale_size):
        t = tickets[i] if i < len(tickets) else 0
        x = 8 + i * (bar_w + bar_gap)
        selected = (i == selected_degree)

        if t < 0:
            canvas.set_color(Color.Low)
            canvas.draw_text(x - 1, base_y - 6, "X")
        elif t == 0:
            h = 2
            canvas.set_color(Color.Medium if selected else Color.Low)
            canvas.vline(x, base_y - h, h)
        else:
            h = min(bar_max_h, t * 3)
            is_current = selected
            # Bar body
            canvas.set_color(Color.MediumBright if is_current else Color.Medium)
            canvas.vline(x, base_y - h, h)
            # Bar top
            canvas.set_color(Color.Bright if is_current else Color.MediumBright)
            canvas.point(x, base_y - h - 1)

        # Degree number
        canvas.set_color(Color.Bright if selected else Color.Medium)
        canvas.draw_text(x - 1, base_y + 2, f"{i+1}")

# ---------------------------------------------------------------------------
# Dice variants
# ---------------------------------------------------------------------------



