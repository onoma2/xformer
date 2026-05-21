#!/usr/bin/env python3
"""
Render marbles-bell at several param values to show hero responds visibly
to encoder turns.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, framebuffer_to_image
from pages_levels import LevelDemo, render_marbles_bell
from tracks import StochasticSequence


def render_one(bias=50, spread=50, steps=4, mode=1):
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)
    seq = StochasticSequence(
        marbles_spread=spread,
        marbles_bias=bias,
        marbles_steps=steps,
        marbles_mode=mode,
    )
    demo = LevelDemo()
    render_marbles_bell(canvas, demo, seq,
                        bias_override=bias, spread_override=spread,
                        steps_override=steps, mode_override=mode)
    return framebuffer_to_image(fb, scale=4)


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'level-previews')
    os.makedirs(out_dir, exist_ok=True)

    cases = [
        # bias sweep (spread=50, steps=4, mode=Soft)
        ('marbles-bell-bias-20',  dict(bias=20,  spread=50, steps=4, mode=1)),
        ('marbles-bell-bias-50',  dict(bias=50,  spread=50, steps=4, mode=1)),
        ('marbles-bell-bias-80',  dict(bias=80,  spread=50, steps=4, mode=1)),
        # spread sweep (bias=50, steps=4, mode=Soft)
        ('marbles-bell-spread-20',  dict(bias=50, spread=20, steps=4, mode=1)),
        ('marbles-bell-spread-50',  dict(bias=50, spread=50, steps=4, mode=1)),
        ('marbles-bell-spread-90',  dict(bias=50, spread=90, steps=4, mode=1)),
        # steps sweep (bias=50, spread=50, mode=Soft)
        ('marbles-bell-steps-2',   dict(bias=50, spread=50, steps=2,  mode=1)),
        ('marbles-bell-steps-8',   dict(bias=50, spread=50, steps=8,  mode=1)),
        ('marbles-bell-steps-16',  dict(bias=50, spread=50, steps=16, mode=1)),
        # mode sweep (bias=50, spread=50, steps=4)
        ('marbles-bell-mode-off',   dict(bias=50, spread=50, steps=4, mode=0)),
        ('marbles-bell-mode-soft',  dict(bias=50, spread=50, steps=4, mode=1)),
        ('marbles-bell-mode-hard',  dict(bias=50, spread=50, steps=4, mode=2)),
    ]

    for name, kwargs in cases:
        img = render_one(**kwargs)
        path = os.path.join(out_dir, f'{name}.png')
        img.save(path)
        print(f'  {name:30s} -> {path}')

    print(f'\nRendered {len(cases)} response variants into {out_dir}')


if __name__ == '__main__':
    main()
