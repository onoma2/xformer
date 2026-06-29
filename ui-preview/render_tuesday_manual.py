#!/usr/bin/env python3
"""Tuesday edit-page OLED renders for TUESDAY_MANUAL.html.

Drives the shared render_tuesday_edit_page() (pages_core.py) across all three
F5=NEXT param pages. generate.py only reaches page 0; this covers 1 and 2.
Outputs to ui-preview/tuesday-manual/.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, framebuffer_to_image
from pages_core import render_tuesday_edit_page
from tracks import TuesdaySequence, TuesdayTrackEngine

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tuesday-manual")


def _data():
    seq = TuesdaySequence(
        algorithm=5, loop_length=16, flow=12, ornament=14, power=10,
        rotate=3, glide=25, skew=-2, gate_length=60, gate_offset=10,
        step_trill=30, start=0,
    )
    engine = TuesdayTrackEngine(current_step=7, gate_output=True)
    return seq, engine


def _render(page, slot, name):
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    seq, engine = _data()
    render_tuesday_edit_page(canvas, seq, engine, current_page=page, selected_slot=slot)
    img = framebuffer_to_image(fb, scale=4)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    img.save(path)
    print(path)


if __name__ == "__main__":
    _render(0, 0, "tuesday-edit-p1.png")
    _render(1, 0, "tuesday-edit-p2.png")
    _render(2, 0, "tuesday-edit-p3.png")
