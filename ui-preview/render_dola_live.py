#!/usr/bin/env python3
"""DOLA Live Page Concepts - Three poetic redesigns.
1. Pond Ripples - Events as expanding water ripples
2. Star Drift - Events as drifting stars 
3. Spiral Rotation - Events arranged along golden-ratio spiral
"""
import math
import random
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, BlendMode, framebuffer_to_image
from painters import PageWidth, PageHeight, WindowPainter

PAGE_W = PageWidth
PAGE_H = PageHeight


class SimEvent:
    def __init__(self, cv, rest=False, children=0):
        self.cv = cv
        self.rest = rest
        self.children = children


def generate_events():
    rng = random.Random(42)
    events = []
    base_cv = 0.0
    for i in range(12):
        if rng.random() < 0.15:
            events.append(SimEvent(0.0, rest=True, children=0))
        else:
            step = (rng.random() - 0.5) * 1.5
            base_cv = max(-2.0, min(2.0, base_cv + step))
            burst = rng.randint(0, 3) if rng.random() < 0.3 else 0
            events.append(SimEvent(base_cv, rest=False, children=burst))
    return events


def brightness_color(v):
    if v >= 0.85: return Color.Bright
    if v >= 0.60: return Color.MediumBright
    if v >= 0.35: return Color.Medium
    if v >= 0.12: return Color.MediumLow
    return Color.Low


def draw_arc(canvas, cx, cy, r, a0, a1, color):
    canvas.set_color(color)
    steps = max(8, int(abs(a1 - a0) * r))
    for i in range(steps + 1):
        a = a0 + (a1 - a0) * i / float(steps)
        x = int(cx + r * math.cos(a))
        y = int(cy + r * math.sin(a))
        canvas.point(x, y)


def gaussian(x, center, width):
    d = (x - center) / float(width)
    return math.exp(-d * d * 2.0)


def cv_to_y(cv, bias_y, vp_h):
    return bias_y - int(cv * (vp_h / 4.5))


def render_pond_ripples(canvas, events, bias=50, spread=50):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LIVE-POND")
    
    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 25, 53
    vp_h = vp_bot - vp_top
    vp_cy = (vp_top + vp_bot) // 2
    
    bias_y = vp_top + int((100 - bias) * vp_h / 100)
    spread_y = max(4, int(spread * vp_h / 200))
    
    canvas.set_blend_mode(BlendMode.Set)
    for y in range(vp_top, vp_bot + 1):
        env = gaussian(y, bias_y, spread_y + 2)
        if env < 0.15: continue
        col = Color.Low if env < 0.5 else Color.MediumLow
        canvas.set_color(col)
        step = 3 if env < 0.7 else 2
        offset = (y * 7) % step
        for x in range(vp_left + offset, vp_right, step):
            if ((x + y * 3) % 11) < 2:
                canvas.point(x, y)
    
    ripple_data = []
    x = vp_right - 4
    for i, ev in enumerate(reversed(events)):
        x -= max(8, 14)
        if x < vp_left: break
        if not ev.rest:
            y = cv_to_y(ev.cv, bias_y, vp_h)
            y = max(vp_top + 2, min(vp_bot - 2, y))
            ripple_data.append((i, x, y, ev))
    
    for age, x, y, ev in reversed(ripple_data):
        fade = max(0.15, 1.0 - age * 0.08)
        base_r = 3
        
        if ev.children > 0:
            for c in range(ev.children + 1):
                r = base_r + c * 3
                bright = fade * (0.9 - c * 0.15)
                if bright < 0.1: break
                draw_arc(canvas, x, y, r, 0, 2 * math.pi, brightness_color(bright * 0.8))
            canvas.set_color(Color.Bright if fade > 0.5 else Color.MediumBright)
            canvas.fill_rect(x - 1, y - 1, 3, 3)
        else:
            draw_arc(canvas, x, y, base_r + 2, 0, 2 * math.pi, brightness_color(fade * 0.8))
            if age == 0:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x - 1, y - 1, 3, 3)
                canvas.set_color(Color.MediumBright)
                canvas.point(x - 4, y)
                canvas.point(x + 4, y)
                canvas.point(x, y - 4)
                canvas.point(x, y + 4)
            else:
                canvas.set_color(brightness_color(fade))
                canvas.point(x, y)
    
    WindowPainter.draw_footer(canvas, ["RISE", "FALL", "SPREAD", "BIAS", "NEXT"])


def render_star_drift(canvas, events, bias=50, spread=50, complexity=40):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LIVE-STAR")
    
    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 25, 53
    vp_h = vp_bot - vp_top
    
    bias_y = vp_top + int((100 - bias) * vp_h / 100)
    swing = max(4, int(spread * vp_h / 200))
    
    canvas.set_blend_mode(BlendMode.Set)
    rng_bg = random.Random(0xDEAD)
    canvas.set_color(Color.Low)
    for _ in range(40):
        x = rng_bg.randint(vp_left, vp_right)
        y = rng_bg.randint(vp_top, vp_bot)
        canvas.point(x, y)
    canvas.set_color(Color.MediumLow)
    for _ in range(15):
        x = rng_bg.randint(vp_left, vp_right)
        y = bias_y + int((rng_bg.random() - 0.5) * swing)
        if vp_top <= y <= vp_bot:
            canvas.point(x, y)
    
    star_data = []
    x = vp_right - 3
    base_stride = 10 + (100 - complexity) // 10
    
    for i, ev in enumerate(reversed(events)):
        x -= max(6, base_stride)
        if x < vp_left: break
        if not ev.rest:
            y = cv_to_y(ev.cv, bias_y, vp_h)
            y = max(vp_top + 2, min(vp_bot - 2, y))
            star_data.append((i, x, y, ev))
    
    pitch_groups = {}
    for age, x, y, ev in star_data:
        bucket = y // 2
        if bucket not in pitch_groups:
            pitch_groups[bucket] = []
        pitch_groups[bucket].append((x, y, age))
    
    canvas.set_color(Color.Low)
    for bucket, pts in pitch_groups.items():
        if len(pts) >= 2:
            pts_sorted = sorted(pts, key=lambda p: p[0])
            for i in range(len(pts_sorted) - 1):
                x0, y0, a0 = pts_sorted[i]
                x1, y1, a1 = pts_sorted[i + 1]
                canvas.line(x0, y0, x1, y1)
    
    for age, x, y, ev in reversed(star_data):
        fade = max(0.15, 1.0 - age * 0.08)
        
        if ev.children > 0:
            for c in range(ev.children + 1):
                angle = (c * 2 * math.pi) / max(1, ev.children)
                cxp = x + int(math.cos(angle) * 4)
                cyp = y + int(math.sin(angle) * 3)
                canvas.set_color(brightness_color(fade * (0.9 - c * 0.15)))
                canvas.point(cxp, cyp)
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x - 1, y - 1, 3, 3)
        else:
            if age == 0:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x - 1, y - 1, 3, 3)
                canvas.set_color(Color.MediumBright)
                canvas.point(x - 3, y - 1)
                canvas.point(x + 3, y - 1)
                canvas.point(x - 1, y - 3)
                canvas.point(x - 1, y + 3)
            else:
                canvas.set_color(brightness_color(fade))
                canvas.line(x - 1, y, x + 1, y)
                canvas.point(x, y)
    
    WindowPainter.draw_footer(canvas, ["DRIFT", "CMPLX", "SPREAD", "BIAS", "NEXT"])


PHI = (1 + math.sqrt(5)) / 2


def render_spiral_rotation(canvas, events, bias=50, spread=50):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="LIVE-SPIRAL")
    
    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 25, 53
    vp_h = vp_bot - vp_top
    
    cx = (vp_left + vp_right) // 2
    cy = (vp_top + vp_bot) // 2
    
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    for r in [6, 12, 18, 24]:
        draw_arc(canvas, cx, cy, r, 0, 2 * math.pi, Color.Low)
    
    for i in range(8):
        angle = i * math.pi / 4
        x2 = cx + int(math.cos(angle) * 28)
        y2 = cy + int(math.sin(angle) * 20)
        canvas.line(cx, cy, x2, y2)
    
    spiral_data = []
    max_r = 26
    for i, ev in enumerate(reversed(events)):
        if i >= 16: break
        
        angle = i * 2 * math.pi / PHI
        r = 5 + i * 1.8
        if r > max_r: break
        
        x = cx + int(r * math.cos(angle))
        y = cy + int(r * math.sin(angle) * 0.6)
        
        if y < vp_top or y > vp_bot: continue
        if x < vp_left or x > vp_right: continue
        
        spiral_data.append((i, x, y, ev, angle, r))
    
    for age, x, y, ev, angle, r in reversed(spiral_data):
        fade = max(0.2, 1.0 - age * 0.06)
        
        if ev.rest:
            canvas.set_color(Color.Medium)
            canvas.hline(x - 1, y, 3)
            continue
        
        if ev.children > 0:
            for c in range(ev.children + 1):
                c_angle = angle + (c + 1) * 0.3
                c_r = r - 2
                cxp = cx + int(c_r * math.cos(c_angle))
                cyp = cy + int(c_r * math.sin(c_angle) * 0.6)
                canvas.set_color(brightness_color(fade * 0.6))
                canvas.point(cxp, cyp)
            canvas.set_color(brightness_color(fade * 0.9))
            canvas.fill_rect(x - 1, y - 1, 3, 3)
        else:
            if age == 0:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x - 1, y - 1, 3, 3)
                canvas.set_color(Color.MediumBright)
                canvas.point(x - 4, y)
                canvas.point(x + 4, y)
                canvas.point(x, y - 4)
                canvas.point(x, y + 4)
            else:
                canvas.set_color(brightness_color(fade))
                canvas.fill_rect(x - 1, y - 1, 3, 3)
    
    WindowPainter.draw_footer(canvas, ["ANGLE", "SPREAD", "ROTATE", "CNTR", "NEXT"])


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(script_dir, "dola-live-previews")
    os.makedirs(out_dir, exist_ok=True)
    
    events = generate_events()
    
    fb = FrameBuffer(PAGE_W, PAGE_H)
    canvas = Canvas(fb)
    render_pond_ripples(canvas, events, 50, 50)
    img = framebuffer_to_image(fb, scale=4)
    path = os.path.join(out_dir, "dola-pond-ripples.png")
    img.save(path)
    print(f"  dola-pond-ripples -> {path}")
    
    fb = FrameBuffer(PAGE_W, PAGE_H)
    canvas = Canvas(fb)
    render_star_drift(canvas, events, 50, 50, 40)
    img = framebuffer_to_image(fb, scale=4)
    path = os.path.join(out_dir, "dola-star-drift.png")
    img.save(path)
    print(f"  dola-star-drift -> {path}")
    
    fb = FrameBuffer(PAGE_W, PAGE_H)
    canvas = Canvas(fb)
    render_spiral_rotation(canvas, events, 50, 50)
    img = framebuffer_to_image(fb, scale=4)
    path = os.path.join(out_dir, "dola-spiral-rotation.png")
    img.save(path)
    print(f"  dola-spiral-rotation -> {path}")


if __name__ == "__main__":
    main()
