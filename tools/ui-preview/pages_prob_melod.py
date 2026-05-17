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



