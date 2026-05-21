#!/usr/bin/env python3
"""
Render all 11 level-hero pages into ui-preview/level-previews/*.png
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, framebuffer_to_image
from pages_levels import (
    LevelDemo,
    render_core_tide, render_core_orbit, render_core_rain,
    render_direct_pulse, render_direct_vu, render_direct_pond,
    render_marbles_bell, render_marbles_spray, render_marbles_magnet,
    render_loop_tape, render_mutation_rune,
)
from tracks import (
    StochasticSequence, StochasticSequenceStep, StochasticTrackEngine,
)


def _make_demo_sequence():
    """Sequence with sensible state across all axes."""
    lock_events = []
    for i in range(64):
        lock_events.append(StochasticSequenceStep(
            gate=(i % 3) != 0,
            gate_probability=100,
            note=(i * 5) % 12,
            length=2 + (i % 5),
            retrigger=i % 3,
        ))
    seq = StochasticSequence(
        steps=[StochasticSequenceStep() for _ in range(64)],
        degree_tickets=[3, -1, 5, 0, 8, 2, -1, 6],
        scale_size=8,
        marbles_spread=58,
        marbles_bias=-12,
        marbles_steps=6,
        marbles_mode=1,
        lock_enabled=True,
        lock_first=6,
        lock_last=26,
        lock_events=lock_events,
    )
    eng = StochasticTrackEngine(current_step=7, current_note=4, gate_output=True)
    return seq, eng


def render_one(render_fn, demo, sequence=None, scale=4):
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)
    if sequence is not None:
        render_fn(canvas, demo, sequence)
    else:
        render_fn(canvas, demo)
    return framebuffer_to_image(fb, scale=scale)


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'level-previews')
    os.makedirs(out_dir, exist_ok=True)

    seq, _eng = _make_demo_sequence()
    demo = LevelDemo()

    targets = [
        # (filename, render_fn, needs_sequence)
        ('core-tide',         render_core_tide,    False),
        ('core-orbit',        render_core_orbit,   False),
        ('core-rain',         render_core_rain,    False),
        ('direct-pulse',      render_direct_pulse, False),
        ('direct-vu',         render_direct_vu,    False),
        ('direct-pond',       render_direct_pond,  False),
        ('marbles-bell',      render_marbles_bell, True),
        ('marbles-spray',     render_marbles_spray, True),
        ('marbles-magnet',    render_marbles_magnet, True),
        ('loop-tape',         render_loop_tape,    True),
        ('mutation-rune',     render_mutation_rune, True),
    ]

    for name, fn, needs_seq in targets:
        img = render_one(fn, demo, sequence=seq if needs_seq else None)
        path = os.path.join(out_dir, f'{name}.png')
        img.save(path)
        print(f'  {name:20s} -> {path}')

    print(f'\nRendered {len(targets)} previews into {out_dir}')


if __name__ == '__main__':
    main()
