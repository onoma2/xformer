#!/usr/bin/env python3
"""
CV Router (CvRoutePage) OLED render for the manual.

Mirrors src/apps/sequencer/ui/pages/CvRoutePage.cpp draw() exactly:
 - "X" crossover logo (two crossing line pairs) at the top
 - optional "FB" feedback flag (when a Bus input AND a Bus output both exist)
 - Row 1: four input-lane cells + "<scan> SCAN" in column 5
 - Row 2: four output-lane cells + "<route> ROUTE" in column 5
 - lane-cell colour tracks the Scan (row1) / Route (row2) interpolation focus

Outputs to ui-preview/cvrouter-manual/.
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, framebuffer_to_image

W, H = 256, 64  # CONFIG_LCD_WIDTH x height

START_X = 8
START_Y = 30
ROW_HEIGHT = 12
COL_COUNT = 5
COL_WIDTH = (W - START_X * 2) // COL_COUNT


def _new():
    fb = FrameBuffer(W, H)
    c = Canvas(fb, brightness=1.0)
    c.set_font(Font.Tiny)
    return fb, c


def _draw_cell(c, col, y, text, color):
    col_x = START_X + col * COL_WIDTH
    tw = c.text_width(text)
    x = col_x + (COL_WIDTH - tw) // 2
    c.set_color(color)
    c.draw_text(x, y, text)


def _lane_color(value, lane):
    """Port of CvRoutePage::draw's laneColor lambda."""
    lane_value = 0 if lane == 0 else (33 if lane == 1 else (66 if lane == 2 else 100))
    if value == lane_value:
        return Color.Bright
    if 0 < value < 33:
        if lane in (0, 1):
            if lane == 0 and value <= 8:
                return Color.MediumBright
            if lane == 1 and value >= 25:
                return Color.MediumBright
            return Color.Medium
    if 33 < value < 66:
        if lane in (1, 2):
            if lane == 1 and value <= 41:
                return Color.MediumBright
            if lane == 2 and value >= 58:
                return Color.MediumBright
            return Color.Medium
    if 66 < value < 100:
        if lane in (2, 3):
            if lane == 2 and value <= 74:
                return Color.MediumBright
            if lane == 3 and value >= 92:
                return Color.MediumBright
            return Color.Medium
    return Color.Low


def render(c, inputs, outputs, scan, route, active_col=0, edit_row="input"):
    """inputs/outputs are lists of (label) strings for the 4 lanes."""
    c.set_color(Color.None_)
    c.fill_rect(0, 0, W, H)
    c.set_font(Font.Tiny)

    # "X" crossover logo
    logo_x = START_X
    logo_w = W - START_X * 2
    mid_x = logo_x + logo_w // 2
    logo_y1, logo_y2 = 8, 12
    c.set_color(Color.MediumBright)
    c.line(logo_x, logo_y2, mid_x, logo_y1)
    c.line(mid_x, logo_y1, logo_x + logo_w, logo_y2)
    c.set_color(Color.Medium)
    c.line(logo_x, logo_y1, mid_x, logo_y2)
    c.line(mid_x, logo_y2, logo_x + logo_w, logo_y1)

    # FB feedback flag (a Bus input AND a Bus output both present)
    has_bus_in = any("BUS" in s for s in inputs)
    has_bus_out = any("BUS" in s for s in outputs)
    if has_bus_in and has_bus_out:
        fb = "FB"
        tw = c.text_width(fb)
        c.set_color(Color.MediumBright)
        c.draw_text(W - START_X - tw, 4, fb)

    # Row 1: inputs + SCAN
    for lane in range(4):
        color = _lane_color(scan, lane)
        if edit_row == "input" and active_col == lane:
            color = Color.Bright
        _draw_cell(c, lane, START_Y, inputs[lane], color)
    scan_str = f"{scan} SCAN"
    color = Color.Bright if (edit_row == "input" and active_col == 4) else Color.Medium
    _draw_cell(c, 4, START_Y, scan_str, color)

    # Row 2: outputs + ROUTE
    row2_y = START_Y + ROW_HEIGHT + 8
    for lane in range(4):
        color = _lane_color(route, lane)
        if edit_row == "output" and active_col == lane:
            color = Color.Bright
        _draw_cell(c, lane, row2_y, outputs[lane], color)
    route_str = f"{route} ROUTE"
    color = Color.Bright if (edit_row == "output" and active_col == 4) else Color.Medium
    _draw_cell(c, 4, row2_y, route_str, color)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    outdir = os.path.join(here, "cvrouter-manual")
    os.makedirs(outdir, exist_ok=True)

    # default: cleared route is all CV in N -> CV R N, scan 0 / route 0
    default_in = ["CV in 1", "CV in 2", "CV in 3", "CV in 4"]
    default_out = ["CV R 1", "CV R 2", "CV R 3", "CV R 4"]

    # patched: a feedback-capable bus example with mid scan/route, input lane focused
    bus_in = ["CV in 1", "BUS 2", "M3", "0V"]
    bus_out = ["CV R 1", "BUS 2", "CV R 3", "NONE"]

    pages = {
        "cvrouter-default": lambda c: render(
            c, default_in, default_out, scan=0, route=0,
            active_col=0, edit_row="input"),
        "cvrouter-mix": lambda c: render(
            c, bus_in, bus_out, scan=50, route=66,
            active_col=4, edit_row="input"),
    }
    for slug, fn in pages.items():
        fb, c = _new()
        fn(c)
        framebuffer_to_image(fb, scale=4).save(os.path.join(outdir, f"{slug}.png"))
        print(f"saved {slug}")


if __name__ == "__main__":
    main()
