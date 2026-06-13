#!/usr/bin/env python3
"""Footer fit-check for the TEMP P1 Len slot armed as "Len-TR" (F3 transfer).

Renders the real WindowPainter footer with the live TEMP P1 labels, slot 0
swapped to "Len-TR", to verify the 6-char label fits the 51px cell without
overflowing its neighbours.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from canvas import Canvas, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, WindowPainter


def render(names, out):
    fb = FrameBuffer(PageWidth, PageHeight)
    c = Canvas(fb)
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=0, play_pattern=0, edit_pattern=0, mode="PHFLX")
    WindowPainter.draw_active_function(c, "ST1")
    WindowPainter.draw_footer(c, names=names, highlight=0)
    framebuffer_to_image(fb, scale=4).save(out)


here = os.path.dirname(os.path.abspath(__file__))
render(["Len", "FlipV", "FlipH", "Mask", "Next"], os.path.join(here, "len-plain.png"))
render(["Len-TR", "FlipV", "FlipH", "Mask", "Next"], os.path.join(here, "len-tr.png"))
print("wrote len-plain.png, len-tr.png")
