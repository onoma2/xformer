#!/usr/bin/env python3
"""
Note track STEPS edit-page renders for NOTE_MANUAL.html.

Faithful to src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp::draw():
  - header "STEPS", active-function label = NoteSequence::layerName(layer)
  - footer GATE/RETRIG/LENGTH/NOTE/COND (functionNames[], line 40),
    highlight = activeFunctionKey() (Gate->0, Length->2, Note->3, Cond->4)
  - 16-step grid: loop markers, step index, gate rect/fill, per-layer cell

Standalone (shared note-edit renderer in pages_core.py is broken: NOTE_LAYERS
undefined). Reuses canvas.py / painters.py only. Outputs to ui-preview/note-manual/.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, BlendMode, framebuffer_to_image
from painters import WindowPainter, SequencePainter

W, H = 256, 64
STEP_COUNT = 16
STEP_WIDTH = W // STEP_COUNT  # 16

# functionNames[] (NoteSequenceEditPage.cpp:40)
FOOTER = ["GATE", "RETRIG", "LENGTH", "NOTE", "COND"]

# chromatic note names (Types::printNote)
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# Condition short1/short2 for non-loop conditions (Types.cpp conditionInfos)
COND_SHORT = [
    ("-", ""),    # Off
    ("F", ""),    # Fill
    ("!F", ""),   # NotFill
    ("P", ""),    # Pre
    ("!P", ""),   # NotPre
    ("1", ""),    # First
    ("!1", ""),   # NotFirst
]


def _new():
    fb = FrameBuffer(W, H)
    c = Canvas(fb, brightness=1.0)
    c.set_font(Font.Tiny)
    return fb, c


def _centered(canvas, x, y, text):
    canvas.draw_text(x + (STEP_WIDTH - canvas.text_width(text) + 1) // 2, y, text)


def _note_name_chromatic(note, root=0):
    """Major scale is chromatic; replicate Scale::noteName Short1/Short2."""
    note_count = 12
    octave = note // note_count if note >= 0 else -((-note + note_count - 1) // note_count)
    note_index = (note - octave * note_count) + root
    while note_index >= 12:
        note_index -= 12
        octave += 1
    short1 = NOTE_NAMES[note_index] if 0 <= note_index < 12 else ""
    short2 = f"{octave:+d}"
    return short1, short2


def render(canvas, steps, layer, first_step=0, last_step=15,
           current_step=3, step_selection=None):
    """layer: one of 'gate', 'length', 'note', 'condition'."""
    if step_selection is None:
        step_selection = set()

    layer_name = {
        "gate": "GATE", "length": "LENGTH", "note": "NOTE", "condition": "CONDITION",
    }[layer]
    active_fn = {"gate": 0, "length": 2, "note": 3, "condition": 4}[layer]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STEPS")
    WindowPainter.draw_active_function(canvas, layer_name)
    WindowPainter.draw_footer(canvas, FOOTER, highlight=active_fn)

    loop_y = 16
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Bright)
    SequencePainter.draw_loop_start(canvas, first_step * STEP_WIDTH + 1, loop_y, STEP_WIDTH - 2)
    SequencePainter.draw_loop_end(canvas, last_step * STEP_WIDTH + 1, loop_y, STEP_WIDTH - 2)

    for i in range(STEP_COUNT):
        step = steps[i]
        x = i * STEP_WIDTH
        y = 20

        if first_step < i <= last_step:
            canvas.set_color(Color.Bright)
            canvas.point(x, loop_y)

        # step index
        canvas.set_color(Color.Bright if i in step_selection else Color.Medium)
        _centered(canvas, x, y - 2, f"{i + 1}")

        # gate rect + fill
        canvas.set_color(Color.Bright if i == current_step else Color.Medium)
        canvas.draw_rect(x + 2, y + 2, STEP_WIDTH - 4, STEP_WIDTH - 4)
        if step.gate():
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x + 4, y + 4, STEP_WIDTH - 8, STEP_WIDTH - 8)

        if layer == "gate":
            pass
        elif layer == "length":
            if step.length() == 0:
                canvas.set_font(Font.Tiny)
                canvas.set_color(Color.Bright)
                _centered(canvas, x, y + 20, "T")
            else:
                SequencePainter.draw_length(canvas, x + 2, y + 18, STEP_WIDTH - 4, 6,
                                            step.length() + 1, 8)
        elif layer == "note":
            canvas.set_color(Color.Bright)
            s1, s2 = _note_name_chromatic(step.note())
            _centered(canvas, x, y + 20, s1)
            _centered(canvas, x, y + 27, s2)
        elif layer == "condition":
            canvas.set_color(Color.Bright)
            cond = step.condition()
            if cond < len(COND_SHORT):
                s1, s2 = COND_SHORT[cond]
            else:
                s1, s2 = "?", ""
            _centered(canvas, x, y + 20, s1)
            if s2:
                _centered(canvas, x, y + 27, s2)


class _Step:
    def __init__(self, gate=True, length=3, note=0, condition=0):
        self._gate = gate
        self._length = length
        self._note = note
        self._condition = condition

    def gate(self): return self._gate
    def length(self): return self._length
    def note(self): return self._note
    def condition(self): return self._condition


def _sample_steps():
    # A small C-major-ish melody so the renders read clearly across layers.
    notes =   [0, 2, 4, 5, 7, 5, 4, 2, 0, 4, 7, 4, 2, 0, -3, 0]
    gates =   [1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,  1]
    lengths = [3, 5, 0, 2, 7, 4, 0, 1, 3, 0, 6, 2, 0, 5, 3,  7]
    conds =   [0, 0, 1, 0, 3, 0, 0, 5, 0, 0, 2, 0, 6, 0, 4,  0]
    return [_Step(gate=bool(gates[i]), length=lengths[i], note=notes[i],
                  condition=conds[i]) for i in range(16)]


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "note-manual")
    os.makedirs(out_dir, exist_ok=True)
    steps = _sample_steps()

    surfaces = {
        "edit-note": ("note", {3, 4}),
        "edit-gate": ("gate", set()),
        "edit-length": ("length", set()),
        "edit-condition": ("condition", set()),
    }
    for name, (layer, sel) in surfaces.items():
        fb, c = _new()
        render(c, steps, layer, step_selection=sel)
        img = framebuffer_to_image(fb, scale=4)
        path = os.path.join(out_dir, f"{name}.png")
        img.save(path)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
