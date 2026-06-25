#!/usr/bin/env python3
"""
Harmony Track OLED hero pages (proposals) — standard edit-page idiom.

  1) recipe — 16-step grid, layer-group footer (CHORD·PITCH·STRUM·GATE·NEXT)
  2) scale  — chromatic mask as NoteTrack-style squares, up to 2 rows
  3) chord  — live chord override panel + 4-voice monitor

Three-stop F5=NEXT hero ring (Recipe → Scale → Chord). Recipe + Scale borrow
NoteSequenceEditPage step-square geometry. Outputs to ui-preview/harmony-hero/.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, framebuffer_to_image
from painters import WindowPainter, SequencePainter

W, H = 256, 64

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def _new():
    fb = FrameBuffer(W, H)
    c = Canvas(fb, brightness=1.0)
    c.set_font(Font.Tiny)
    return fb, c


def _tt(c, x, ytop, text, color=Color.Bright):
    c.set_font(Font.Tiny)
    c.set_color(color)
    c.draw_text(x, ytop + 5, text)


def _tc(c, x, ytop, w, text, color=Color.Bright):
    c.set_font(Font.Tiny)
    c.set_color(color)
    c.draw_text(x + (w - c.text_width(text)) // 2, ytop + 5, text)


# ---------------------------------------------------------------------------
# Shared 16-step grid (NoteSequenceEditPage geometry)
# ---------------------------------------------------------------------------
def _draw_step_grid(c, values, rest, current, selected, first=0, last=15):
    step_width = 256 // 16  # 16
    loop_y = 16
    y = 20

    c.set_color(Color.Bright)
    SequencePainter.draw_loop_start(c, first * step_width + 1, loop_y, step_width - 2)
    SequencePainter.draw_loop_end(c, last * step_width + 1, loop_y, step_width - 2)

    for i in range(16):
        x = i * step_width
        is_cur = (i == current)
        is_sel = (i in selected)
        is_rest = (i in rest)

        if first < i <= last:
            c.set_color(Color.Bright)
            c.point(x, loop_y)

        c.set_color(Color.Bright if is_cur else Color.Medium)
        c.draw_rect(x + 2, y + 2, step_width - 4, step_width - 4)

        if is_sel and not is_rest:
            c.set_color(Color.MediumLow)
            c.fill_rect(x + 3, y + 3, step_width - 6, step_width - 6)

        if not is_rest:
            c.set_color(Color.Bright if (is_cur or is_sel) else Color.Medium)
            c.fill_rect(x + 4, y + 4, step_width - 8, step_width - 8)

        c.set_font(Font.Tiny)
        if is_rest:
            txt, col = "z", Color.MediumLow
        else:
            txt = values[i]
            col = Color.Bright if (is_cur or is_sel) else Color.MediumBright
        c.set_color(col)
        c.draw_text(x + (step_width - c.text_width(txt) + 1) // 2, y + 20, txt)


# ---------------------------------------------------------------------------
# 1) Recipe — note-edit grid, layer-group footer, active layer up top
# ---------------------------------------------------------------------------
def render_recipe(c, current=5, selected=(5,)):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=4, mode="HARMONY")
    WindowPainter.draw_active_function(c, "QUAL")
    WindowPainter.draw_footer(c, ["CHORD", "PITCH", "STRUM", "GATE", "NEXT"],
                              highlight=0)

    qual = ["M7", "m7", "7", "M7", "m7", "7", "o7", "h7",
            "M7", "m7", "7", "M7", "m7", "7", "M7", "m7"]
    rest = {3, 11}
    _draw_step_grid(c, qual, rest, current, set(selected))


# ---------------------------------------------------------------------------
# Scale — chromatic mask as NoteTrack-style squares, up to 2 rows
# ---------------------------------------------------------------------------
def _draw_scale_squares(c, n, sounding, mask, labels):
    step_width = 256 // 16  # 16
    enabled = set(mask)
    rows = 1 if n <= 16 else 2

    if rows == 1:
        # centre the single row vertically inside the safe area
        row_tops = [22]
    else:
        row_tops = [12, 31]

    for i in range(n):
        row = i // 16
        col = i % 16
        x = col * step_width
        y = row_tops[row]

        is_sounding = (i == sounding)
        is_enabled = (i in enabled)

        if is_sounding:
            sq_col = Color.Bright
        elif is_enabled:
            sq_col = Color.Medium
        else:
            sq_col = Color.Low

        c.set_color(sq_col)
        c.draw_rect(x + 2, y + 2, step_width - 4, step_width - 4)

        if is_enabled:
            c.set_color(Color.Bright if is_sounding else Color.Medium)
            c.fill_rect(x + 4, y + 4, step_width - 8, step_width - 8)

        _tc(c, x, y + 14, step_width, labels[i],
            Color.Bright if is_sounding
            else (Color.Medium if is_enabled else Color.Low))


def render_scale(c, root=0, sounding=4, mask=(0, 2, 4, 5, 7, 9, 11)):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=4, mode="HARMONY")
    WindowPainter.draw_active_function(c, "SCALE")
    WindowPainter.draw_footer(c, ["MODE", "ROOT", "MROT", "", "NEXT"],
                              highlight=-1)

    labels = [NOTE_NAMES[(root + i) % 12] for i in range(12)]
    _draw_scale_squares(c, 12, sounding, mask, labels)


def render_scale_2row(c, root=0, sounding=4,
                      mask=(0, 2, 4, 6, 8, 10, 12, 14, 16, 18)):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=4, mode="HARMONY")
    WindowPainter.draw_active_function(c, "SCALE")
    WindowPainter.draw_footer(c, ["MODE", "ROOT", "MROT", "", "NEXT"],
                              highlight=-1)

    n = 19
    # derived name: degree's pitch (degree*12/n semitones) rounded to nearest
    # chromatic letter, root-relative — the tuning's own noteName idiom
    labels = [NOTE_NAMES[(root + round(i * 12 / n)) % 12] for i in range(n)]
    _draw_scale_squares(c, n, sounding, mask, labels)


# ---------------------------------------------------------------------------
# Chord — live chord override panel + 4-voice monitor
# ---------------------------------------------------------------------------
def render_chord(c, symbol="Cmaj7",
                 overrides=(("QUAL", "maj7"), ("INV", "root"),
                            ("VOIC", "close"), ("ROT", "0")),
                 voices=(("C", 3, True), ("E", 3, True),
                         ("G", 3, True), ("B", 3, True))):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=4, mode="HARMONY")
    WindowPainter.draw_active_function(c, "CHORD")
    WindowPainter.draw_footer(c, ["QUAL", "INV", "VOIC", "ROT", "NEXT"],
                              highlight=-1)

    c.set_font(Font.Small)
    c.set_color(Color.Bright)
    c.draw_text(8, 24, symbol)

    ox = [8, 66]
    oy = [32, 42]
    for k, (label, val) in enumerate(overrides):
        x = ox[k % 2]
        y = oy[k // 2]
        c.set_font(Font.Tiny)
        c.set_color(Color.Medium)
        c.draw_text(x, y + 5, label)
        c.set_color(Color.Bright)
        c.draw_text(x + 24, y + 5, val)

    vx0 = 120
    colw = (W - vx0 - 8) // 4
    for j, (name, octv, active) in enumerate(voices):
        x = vx0 + j * colw
        _tc(c, x, 13, colw, f"V{j + 1}", Color.Medium)

        gx = x + colw // 2 - 3
        if active:
            c.set_color(Color.Bright); c.fill_rect(gx, 22, 6, 6)
        else:
            c.set_color(Color.Low); c.draw_rect(gx, 22, 6, 6)

        _tc(c, x, 33, colw, f"{name}{octv}",
            Color.Bright if active else Color.Low)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    outdir = os.path.join(here, "harmony-hero")
    os.makedirs(outdir, exist_ok=True)

    # remove the obsolete Tuesday A/B split renders
    for stale in ("recipe-a.png", "recipe-b.png"):
        p = os.path.join(outdir, stale)
        if os.path.exists(p):
            os.remove(p)
            print(f"removed {stale}")

    pages = {
        "recipe": render_recipe,
        "scale": render_scale,
        "scale-2row": render_scale_2row,
        "chord": render_chord,
    }
    for slug, fn in pages.items():
        fb, c = _new()
        fn(c)
        framebuffer_to_image(fb, scale=4).save(os.path.join(outdir, f"{slug}.png"))
        print(f"saved {slug}")


if __name__ == "__main__":
    main()
