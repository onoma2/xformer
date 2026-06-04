#!/usr/bin/env python3
"""Render the context-menu footer (mirrors ContextMenuPage::draw) for the phase-1
modulation labels, to check that MODULATE / REMOVE MOD fit the 5-slot 51px footer."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from canvas import FrameBuffer, Canvas, Color, Font, framebuffer_to_image

WIDTH, HEIGHT = 256, 64

def draw_menu(items):
    fb = FrameBuffer(WIDTH, HEIGHT)
    c = Canvas(fb)
    c.set_font(Font.Tiny)
    BarHeight = 12
    # separators
    c.set_color(Color.Bright)
    c.hline(0, HEIGHT - BarHeight, WIDTH)
    for i in range(1, 5):
        c.vline((WIDTH * i) // 5, HEIGHT - BarHeight, BarHeight)
    # titles (centered per slot, exactly like the C++)
    for i in range(5):
        if i >= len(items):
            continue
        title, enabled = items[i]
        if not title:
            continue
        x = (WIDTH * i) // 5
        w = WIDTH // 5
        c.set_color(Color.Bright if enabled else Color.Medium)
        tw = c.text_width(title)
        c.draw_text(x + (w - tw + 1) // 2, HEIGHT - 4, title)
        # mark overflow: a slot is 51px; flag if the glyph run exceeds it
        if tw > w:
            print(f"  OVERFLOW slot {i}: '{title}' textWidth={tw} > slot {w}")
        else:
            print(f"  ok slot {i}: '{title}' textWidth={tw} <= slot {w}")
    return fb

variants = {
    # state-dependent single slot: the legacy ROUTE slot toggles MOD+ / MOD-
    "modulation-ctx-note-add":    [("INIT",1),("COPY",1),("PASTE",1),("DUPL",1),("MOD+",1)],
    "modulation-ctx-note-remove": [("INIT",1),("COPY",1),("PASTE",1),("DUPL",1),("MOD-",1)],
    "modulation-ctx-track-add":   [("INIT",1),("COPY",1),("PASTE",1),("MOD+",1),("RESEED",1)],
    "modulation-ctx-project-add": [("INIT",1),("LOAD",1),("SAVE",1),("SAVE AS",1),("MOD+",1)],
    "modulation-ctx-phaseflux":   [("INIT",1),("MOD+",1)],
}

os.makedirs(os.path.join(os.path.dirname(__file__), "modulation"), exist_ok=True)
for slug, items in variants.items():
    print(f"[{slug}]")
    fb = draw_menu(items)
    img = framebuffer_to_image(fb, scale=4)
    out = os.path.join(os.path.dirname(__file__), "modulation", slug + ".png")
    img.save(out)
    print(f"  -> {out}")
