"""
Individual edit page renderers.
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

def render_ref_prob_melod(canvas: Canvas, weights=None, active_note=4, active_octave=0):
    """
    ProbabilityMelody reference screen.
    12 semitone weight bars + piano keyboard.
    weights: list of 12 ints (-1..10). -1 = excluded (X), 0..10 = ticket height.
    """
    if weights is None:
        weights = [2, 0, 5, 0, 8, 3, 0, 7, 0, 4, 0, 1]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="PROBMELO")
    WindowPainter.draw_footer(canvas, ["W1", "W2", "W3", "W4", "NEXT"])

    # 12 semitone x-positions (scaled from O_C 128px to our 256px)
    semitone_x = [int(x * 2) for x in [6, 14, 22, 30, 38, 46, 54, 62, 70, 78, 86, 94]]
    # Black key pattern: indices 1,3,6,8,10 are black keys
    black_keys = {1, 3, 6, 8, 10}

    bar_base_y = 22
    bar_max_h = 20

    canvas.set_blend_mode(BlendMode.Set)
    for i, w in enumerate(weights):
        x = semitone_x[i]
        # dotted vertical line
        canvas.set_color(Color.Medium)
        for y in range(bar_base_y, bar_base_y + bar_max_h, 2):
            canvas.point(x, y)

        if w < 0:
            # excluded: X marker
            canvas.set_color(Color.Bright)
            canvas.draw_text(x - 2, bar_base_y + bar_max_h - 6, "X")
        elif w > 0:
            # horizontal tick at weight height
            tick_y = bar_base_y + bar_max_h - (w * bar_max_h // 10)
            canvas.set_color(Color.Bright)
            canvas.hline(x - 2, tick_y, 5)
        else:
            # zero weight: no tick (hollow)
            pass

    # Keyboard at bottom
    key_y = 46
    white_key_w = 10
    black_key_w = 6
    key_h = 10
    # White keys (0,2,4,5,7,9,11)
    white_indices = [0, 2, 4, 5, 7, 9, 11]
    for idx in white_indices:
        x = semitone_x[idx] - white_key_w // 2
        canvas.set_color(Color.Medium)
        canvas.draw_rect(x, key_y, white_key_w, key_h)
    # Black keys
    for idx in black_keys:
        x = semitone_x[idx] - black_key_w // 2
        canvas.set_color(Color.Bright)
        canvas.fill_rect(x, key_y, black_key_w, key_h - 3)

    # Active note triangles (simplified as text indicator)
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    canvas.set_color(Color.Bright)
    canvas.draw_text(semitone_x[active_note] - 3, key_y + key_h + 2, note_names[active_note])
    canvas.draw_text(220, key_y + key_h + 2, f"O{active_octave}")



def render_ref_shredder(canvas: Canvas, grid_values=None, active_cell=5):
    """
    Shredder reference screen.
    4x4 Cartesian grid (5x5px frames, 8px spacing) with crosshairs.
    """
    if grid_values is None:
        grid_values = [i % 7 for i in range(16)]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="SHREDDER")
    WindowPainter.draw_footer(canvas, ["RANGE", "QUANT", "BIPOL", "RATE", "NEXT"])

    grid_x0 = 80
    grid_y0 = 20
    cell_size = 5
    cell_spacing = 8

    canvas.set_blend_mode(BlendMode.Set)
    # Draw grid frames
    for i in range(16):
        cx = grid_x0 + (i % 4) * cell_spacing
        cy = grid_y0 + (i // 4) * cell_spacing
        canvas.set_color(Color.Medium)
        canvas.draw_rect(cx, cy, cell_size, cell_size)

    # Active cell filled
    ax = grid_x0 + (active_cell % 4) * cell_spacing
    ay = grid_y0 + (active_cell // 4) * cell_spacing
    canvas.set_color(Color.Bright)
    canvas.fill_rect(ax, ay, cell_size, cell_size)

    # Crosshair lines through active row/col
    canvas.set_color(Color.Bright)
    # horizontal dotted line through active row
    row_y = ay + cell_size // 2
    for x in range(grid_x0, grid_x0 + 4 * cell_spacing, 2):
        canvas.point(x, row_y)
    # vertical dotted line through active col
    col_x = ax + cell_size // 2
    for y in range(grid_y0, grid_y0 + 4 * cell_spacing, 2):
        canvas.point(col_x, y)

    # Bar meters on right side (simplified as 4 vertical bars)
    meter_x = 140
    for i in range(4):
        val = grid_values[i]
        h = abs(val) * 2
        my = 40 - h if val >= 0 else 40
        canvas.set_color(Color.Bright if i == active_cell % 4 else Color.Medium)
        canvas.fill_rect(meter_x + i * 8, my, 5, h)
    # center zero line
    canvas.set_color(Color.Low)
    canvas.hline(meter_x, 40, 32)



def render_ref_euclid(canvas: Canvas, steps=16, hits=5, rotation=0, current_step=3):
    """
    Euclidean ring reference screen (squares-and-circles derived).
    Steps arranged on a 21px radius circle.
    Filled rect = active/deterministic, hollow circle = inactive/probabilistic.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="EUCLID")
    WindowPainter.draw_footer(canvas, ["LEN", "HITS", "ROT", "SWING", "NEXT"])

    import math
    cx = 60
    cy = 34
    radius = 21

    # Compute euclidean pattern
    def euclidean(n, k, r):
        pattern = [0] * n
        if k <= 0:
            return pattern
        idx = 0
        for _ in range(k):
            pattern[idx % n] = 1
            idx += n // k if n // k > 0 else 1
        # Rotate
        r = r % n
        return pattern[-r:] + pattern[:-r]

    pattern = euclidean(steps, hits, rotation)

    canvas.set_blend_mode(BlendMode.Set)

    # Outer guide circle
    if steps >= 7:
        canvas.set_color(Color.Medium)
        # Approximate circle with points
        for angle in range(0, 360, 5):
            rad = math.radians(angle)
            px = int(cx + radius * math.cos(rad))
            py = int(cy + radius * math.sin(rad))
            canvas.point(px, py)

    # Step dots
    for i in range(steps):
        angle = math.radians(i * 360 / steps - 90)
        px = int(cx + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))

        active = pattern[i]
        is_current = (i == current_step)
        is_rotation = (i == 0)

        if is_current:
            # Playhead: 5x5 hollow rect
            canvas.set_color(Color.Bright)
            canvas.draw_rect(px - 2, py - 2, 5, 5)
        elif is_rotation:
            # Rotation offset: 7x7 hollow rect
            canvas.set_color(Color.MediumBright)
            canvas.draw_rect(px - 3, py - 3, 7, 7)

        if active:
            # Filled 3x3 rect = deterministic
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px - 1, py - 1, 3, 3)
        else:
            # Hollow 3px circle = probabilistic
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px - 1, py - 1, 3, 3)

    # Center info
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 6, cy - 2, f"{hits}")

    # Second ring (side-by-side, like EuclidRythm.cpp)
    cx2 = 180
    pattern2 = euclidean(steps, hits + 2, rotation + 1)
    if steps >= 7:
        canvas.set_color(Color.Medium)
        for angle in range(0, 360, 5):
            rad = math.radians(angle)
            px = int(cx2 + radius * math.cos(rad))
            py = int(cy + radius * math.sin(rad))
            canvas.point(px, py)
    for i in range(steps):
        angle = math.radians(i * 360 / steps - 90)
        px = int(cx2 + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))
        active = pattern2[i]
        is_current = (i == (current_step + 2) % steps)
        if is_current:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(px - 2, py - 2, 5, 5)
        if active:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px - 1, py - 1, 3, 3)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px - 1, py - 1, 3, 3)



def render_ref_delinquencer(canvas: Canvas, probs=None, current_step=5, selected_cell=12):
    """
    delinquencer reference screen.
    8x8 binary sequencer grid (5x5px cells, 7px spacing).
    Probability shown as erased corner pixels.
    """
    if probs is None:
        probs = [100, 80, 60, 40, 20, 0, 100, 50,
                 75, 25, 100, 0, 50, 50, 100, 30,
                 100, 100, 0, 80, 60, 40, 20, 10,
                 50, 50, 50, 100, 100, 0, 75, 25,
                 100, 0, 100, 50, 25, 75, 100, 0,
                 60, 40, 80, 20, 100, 100, 50, 50,
                 0, 100, 25, 75, 50, 50, 100, 0,
                 100, 100, 100, 0, 50, 75, 25, 100]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="DELINQ")
    WindowPainter.draw_footer(canvas, ["PROB", "X", "Y", "PAT", "NEXT"])

    grid_x0 = 16
    grid_y0 = 16
    cell_size = 5
    cell_spacing = 7

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(64):
        col = i % 8
        row = i // 8
        x = grid_x0 + col * cell_spacing
        y = grid_y0 + row * cell_spacing
        prob = probs[i]

        # Base brightness by status
        if prob == 100:
            canvas.set_color(Color.Bright)
        elif prob > 50:
            canvas.set_color(Color.MediumBright)
        elif prob > 0:
            canvas.set_color(Color.Medium)
        else:
            canvas.set_color(Color.Low)

        # Draw cell frame
        canvas.draw_rect(x, y, cell_size, cell_size)
        if prob > 0:
            canvas.fill_rect(x + 1, y + 1, cell_size - 2, cell_size - 2)

        # Probability < 100%: erase corner pixels
        if prob < 100 and prob > 0:
            canvas.set_blend_mode(BlendMode.Set)
            canvas.set_color(Color.None_)
            # Erase 1-4 corners based on probability
            corners = 4 - (prob // 25)
            if corners >= 1:
                canvas.point(x + 1, y + 1)  # top-left
            if corners >= 2:
                canvas.point(x + cell_size - 2, y + 1)  # top-right
            if corners >= 3:
                canvas.point(x + 1, y + cell_size - 2)  # bottom-left
            if corners >= 4:
                canvas.point(x + cell_size - 2, y + cell_size - 2)  # bottom-right
            canvas.set_blend_mode(BlendMode.Set)

        # Current step: 2px dot
        if i == current_step:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x + 2, y + 2, 2, 2)

        # Selected cell: 4px rect outline
        if i == selected_cell:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(x - 1, y - 1, cell_size + 2, cell_size + 2)

    # Page dots bottom-right
    canvas.set_color(Color.Bright)
    canvas.fill_rect(240, 50, 3, 3)
    canvas.set_color(Color.Medium)
    for i in range(1, 4):
        canvas.draw_rect(240 + i * 5, 50, 3, 3)



def render_ref_skylines(canvas: Canvas, voice1=None, voice2=None):
    """
    skylines reference screen.
    M-185 "city skyline" bar chart. Height = repetition count.
    Voice 1 foreground (bright), voice 2 background (dim).
    """
    if voice1 is None:
        voice1 = [3, 1, 4, 2, 5, 1, 3, 2]
    if voice2 is None:
        voice2 = [2, 4, 1, 3, 2, 5, 1, 3]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="SKYLINES")
    WindowPainter.draw_footer(canvas, ["V1", "V2", "MODE", "TRIG", "NEXT"])

    bar_w = 4
    bar_spacing = 2
    base_y = 50
    scale = 5  # px per rep

    canvas.set_blend_mode(BlendMode.Set)

    # Voice 2 background (dim)
    for i, reps in enumerate(voice2):
        x = 10 + i * (bar_w + bar_spacing)
        h = reps * scale
        canvas.set_color(Color.Medium)
        canvas.vline(x + 1, base_y - h, h)
        canvas.vline(x + 2, base_y - h, h)

    # Voice 1 foreground (bright)
    for i, reps in enumerate(voice1):
        x = 10 + i * (bar_w + bar_spacing)
        h = reps * scale
        canvas.set_color(Color.Bright)
        canvas.vline(x, base_y - h, h)
        canvas.vline(x + 1, base_y - h, h)
        canvas.vline(x + 2, base_y - h, h)
        canvas.vline(x + 3, base_y - h, h)

    # Base line
    canvas.set_color(Color.Low)
    canvas.hline(10, base_y, len(voice1) * (bar_w + bar_spacing))



def render_ref_prob_div(canvas: Canvas, weights=None, loop_length=16, cursor=0):
    """
    ProbabilityDivider reference screen.
    4 horizontal sliders with division labels.
    """
    if weights is None:
        weights = [8, 4, 2, 1]
    divs = [1, 2, 4, 8]
    labels = ["/1", "/2", "/4", "/8"]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="PROBDIV")
    WindowPainter.draw_footer(canvas, ["/1", "/2", "/4", "/8", "NEXT"], highlight=cursor)

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(4):
        y = 16 + i * 10
        # Division label
        canvas.set_color(Color.Bright)
        canvas.draw_text(4, y + 5, labels[i])

        # Slider track (dotted line)
        slider_x = 30
        slider_w = 80
        canvas.set_color(Color.Medium)
        for x in range(slider_x, slider_x + slider_w, 2):
            canvas.point(x, y + 3)

        # Knob (2x8 rect)
        val = weights[i]
        knob_x = slider_x + (val * slider_w // 16)
        canvas.set_color(Color.Bright if cursor == i else Color.Medium)
        canvas.fill_rect(knob_x, y, 3, 7)

    # Loop section bottom
    canvas.set_color(Color.Bright)
    canvas.draw_text(4, 56, "LOOP")
    canvas.draw_text(34, 56, str(loop_length) if loop_length > 0 else "off")
    if cursor == 4:
        canvas.set_color(Color.Bright)
        canvas.draw_text(60, 56, "<")


# =============================================================================
# Hybrid Stochastic Pages — Reference Design Crossovers (First Wave)
# =============================================================================


