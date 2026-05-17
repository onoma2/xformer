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

def render_stochastic_track_pitter_patter(canvas: Canvas, sequence, track_engine,
                                           current_page: int = 0, selected_slot: int = 0):
    """
    pitter-patter-derived grid squares + parameter text.
    4 columns with 5x5 brightness square + value below.
    """
    param_pages = [
        [("OCT", "octave"), ("TRANS", "transpose"), ("DIV", "division"), ("RESET", "reset_mode")],
        [("GBIAS", "gbias"), ("NBIAS", "nbias"), ("LBIAS", "lbias"), ("RBIAS", "rbias")],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    col_width = 51

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK PP P{current_page+1}")

    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr) in enumerate(page):
        x = i * col_width
        val = getattr(sequence, attr)()
        selected = (i == selected_slot)

        # 5x5 brightness square
        sq_x = x + (col_width - 5) // 2
        sq_y = 20
        if selected:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(sq_x, sq_y, 5, 5)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(sq_x, sq_y, 5, 5)

        # Label
        canvas.set_color(Color.Bright if selected else Color.Medium)
        canvas.draw_text(x + (col_width - canvas.text_width(name)) // 2, 30, name)

        # Value
        val_str = str(val)
        canvas.set_color(Color.Bright)
        canvas.draw_text(x + (col_width - canvas.text_width(val_str)) // 2, 38, val_str)

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)






