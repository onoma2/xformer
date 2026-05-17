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

def render_stochastic_track_sequencex(canvas: Canvas, sequence, track_engine,
                                       current_page: int = 0, selected_slot: int = 0):
    """
    SequenceX-derived vertical slider columns.
    Dotted line per param + 5x3 rect indicator at value.
    """
    param_pages = [
        [("OCT", "octave", -4, 4), ("TRANS", "transpose", -12, 12),
         ("DIV", "division", 1, 16), ("RESET", "reset_mode", 0, 4)],
        [("GBIAS", "gbias", -50, 50), ("NBIAS", "nbias", -50, 50),
         ("LBIAS", "lbias", -50, 50), ("RBIAS", "rbias", -50, 50)],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK SX P{current_page+1}")

    slider_y0 = 18
    slider_h = 36
    col_width = 51
    zero_y = slider_y0 + slider_h // 2

    canvas.set_blend_mode(BlendMode.Set)

    for i, (name, attr, minv, maxv) in enumerate(page):
        x = i * col_width + col_width // 2
        val = getattr(sequence, attr)()

        # Dotted zero line
        canvas.set_color(Color.Low)
        for dx in range(x - 2, x + 3, 2):
            canvas.point(dx, zero_y)

        # Dotted slider line
        canvas.set_color(Color.Medium)
        for y in range(slider_y0, slider_y0 + slider_h, 2):
            canvas.point(x, y)

        # Value indicator rect (5x3)
        range_span = maxv - minv
        pos_y = slider_y0 + slider_h - 2 - ((val - minv) * (slider_h - 4) // range_span)
        pos_y = max(slider_y0, min(slider_y0 + slider_h - 3, pos_y))

        if i == selected_slot:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x - 2, pos_y, 5, 3)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(x - 2, pos_y, 5, 3)

        # Label
        canvas.set_color(Color.Bright if i == selected_slot else Color.Medium)
        canvas.draw_text(x - canvas.text_width(name) // 2, slider_y0 + slider_h + 2, name)

        # Value text
        canvas.set_color(Color.Bright)
        canvas.draw_text(x - canvas.text_width(str(val)) // 2, slider_y0 - 4, str(val))

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)



