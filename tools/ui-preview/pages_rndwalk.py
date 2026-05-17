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

def render_stochastic_marbles_rndwalk(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """RndWalk-derived inverted horizontal bar meters for Spread/Bias/Steps/Mode.
    Exact params from RndWalk.h: gfxInvert(31, 48+(8*ch), w, 7).
    Scaled to 256px canvas: x=128, y=20+8*i, h=7, max_w=100.
    """
    slots = ["SPRD", "BIAS", "STEP", "MODE"]
    modes = ["OFF", "SOFT", "HARD"]
    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()
    steps = sequence.marbles_steps()
    mode = sequence.marbles_mode()
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE RW")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)
    params = [("SPREAD", spread, 0, 100), ("BIAS", bias, -50, 50), ("STEPS", steps, 0, 16), ("MODE", mode, 0, 2)]
    # Exact RndWalk params scaled to 256px canvas
    bar_x = 128       # scaled from 31 (half of 64px applet -> half of 256px canvas)
    bar_h = 7         # exact from RndWalk.h
    max_w = 100       # scaled from 31
    y0 = 20           # scaled from 48, adjusted to clear header
    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, val, minv, maxv) in enumerate(params):
        y = y0 + i * 8  # exact 8px spacing from RndWalk.h
        canvas.set_color(Color.Bright if i == slot else Color.Medium)
        canvas.draw_text(4, y + 5, name)
        if i == 3:
            mstr = modes[val] if val < len(modes) else "OFF"
            canvas.set_color(Color.Bright if i == slot else Color.Medium)
            canvas.draw_text(50, y + 5, mstr)
            continue
        if minv < 0:
            w = (abs(val) * max_w) // max(abs(minv), abs(maxv))
            if val >= 0:
                canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
                canvas.fill_rect(bar_x, y, w, bar_h)
            else:
                canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
                canvas.fill_rect(bar_x - w, y, w, bar_h)
            canvas.set_color(Color.Medium)
            canvas.vline(bar_x, y, bar_h)
        else:
            w = (val * max_w) // maxv
            canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
            canvas.fill_rect(bar_x - max_w // 2, y, w, bar_h)
        canvas.set_color(Color.Bright)
        canvas.draw_text(bar_x + max_w + 8, y + 5, str(val))




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



