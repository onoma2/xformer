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

def render_stochastic_track_rndwalk(canvas: Canvas, sequence, track_engine, current_page: int = 0, selected_slot: int = 0):
    """RndWalk-derived inverted horizontal bar meter console.
    Exact params from RndWalk.h: gfxInvert(31, 48+(8*ch), w, 7).
    Scaled to 256px canvas: x=128, y=20+8*i, h=7, max_w=100.
    """
    param_pages = [
        [("OCT", "octave", -4, 4), ("TRANS", "transpose", -12, 12), ("DIV", "division", 1, 16), ("RESET", "reset_mode", 0, 4)],
        [("GBIAS", "gbias", -50, 50), ("NBIAS", "nbias", -50, 50), ("LBIAS", "lbias", -50, 50), ("RBIAS", "rbias", -50, 50)],
        [("ACC", "accent", 0, 100), ("LEG", "legato", 0, 100), ("FILL", "fill", 0, 100), ("REST", "rest_prob", 0, 100)],
    ]
    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK RW P{current_page+1}")
    # Exact RndWalk params scaled to 256px canvas
    bar_x = 128       # scaled from 31
    bar_h = 7         # exact from RndWalk.h
    max_w = 100       # scaled from 31
    y0 = 20           # scaled from 48, adjusted to clear header
    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr, minv, maxv) in enumerate(page):
        y = y0 + i * 8  # exact 8px spacing from RndWalk.h
        val = getattr(sequence, attr)()
        canvas.set_color(Color.Bright if selected_slot == i else Color.Medium)
        canvas.draw_text(4, y + 5, name)
        if minv < 0:
            w = (abs(val) * max_w) // max(abs(minv), abs(maxv))
            if val >= 0:
                canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
                canvas.fill_rect(bar_x, y, w, bar_h)
            else:
                canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
                canvas.fill_rect(bar_x - w, y, w, bar_h)
            canvas.set_color(Color.Medium)
            canvas.vline(bar_x, y, bar_h)
        else:
            w = (val * max_w) // maxv
            canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
            canvas.fill_rect(bar_x - max_w // 2, y, w, bar_h)
        canvas.set_color(Color.Bright)
        canvas.draw_text(bar_x + max_w + 8, y + 5, str(val))
    box_x, box_y = 180, 14
    canvas.set_color(Color.Medium)
    canvas.draw_rect(box_x, box_y, 72, 36)
    canvas.set_color(Color.Bright)
    canvas.draw_text(box_x + 4, box_y + 7, f"S{track_engine.current_step()+1}")
    canvas.draw_text(box_x + 4, box_y + 15, "L" if sequence.lock_enabled() else "-")
    canvas.draw_text(box_x + 4, box_y + 23, f"N{track_engine.current_note()}")
    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)



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



def render_stochastic_track_bline(canvas: Canvas, sequence, track_engine,
                                   current_page: int = 0, selected_slot: int = 0):
    """
    bline-derived 4-quadrant 2x2 parameter grid.
    Each cell = 19x19 px. Selected = bright background.
    """
    param_pages = [
        [("OCT", "octave"), ("TRANS", "transpose"), ("DIV", "division"), ("RESET", "reset_mode")],
        [("GBIAS", "gbias"), ("NBIAS", "nbias"), ("LBIAS", "lbias"), ("RBIAS", "rbias")],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    cell_w = 19
    cell_h = 19
    grid_x = 10
    grid_y = 14

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK BLN P{current_page+1}")

    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr) in enumerate(page):
        col = i % 2
        row = i // 2
        x = grid_x + col * (cell_w + 1)
        y = grid_y + row * (cell_h + 1)
        val = getattr(sequence, attr)()
        selected = (i == selected_slot)

        # Cell background
        if selected:
            canvas.set_color(Color.Medium)
            canvas.fill_rect(x, y, cell_w, cell_h)
            text_color = Color.Bright
        else:
            canvas.set_color(Color.Low)
            canvas.fill_rect(x, y, cell_w, cell_h)
            text_color = Color.Medium

        # Label
        canvas.set_color(text_color)
        canvas.draw_text(x + 2, y + 6, name)
        # Value
        canvas.set_color(text_color)
        canvas.draw_text(x + 2, y + 14, str(val))

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)




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





