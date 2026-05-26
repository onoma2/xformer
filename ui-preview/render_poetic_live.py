#!/usr/bin/env python3
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, Color, BlendMode, framebuffer_to_image

PAGE_W = 256
PAGE_H = 64

def draw_header(canvas, title):
    canvas.set_color(Color.Bright)
    canvas.set_font(Font.Small)
    canvas.draw_text(8, 8, title)
    canvas.set_color(Color.Low)
    canvas.hline(0, 12, PAGE_W)

def draw_footer(canvas, labels):
    canvas.set_font(Font.Tiny)
    slot_w = PAGE_W // len(labels)
    for i, lbl in enumerate(labels):
        if lbl is None: continue
        canvas.set_color(Color.Medium)
        canvas.draw_text(i * slot_w + 4, PAGE_H - 2, lbl)

def draw_info(canvas):
    # Just mock the info rows so it's consistent
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    top_labels = ["DURA5", "VARI0", "REST0"]
    bot_labels = ["CMPX50", "CONT0", "BIAS50", "SPRE50", "REPT0"]
    for c, txt in enumerate(top_labels):
        canvas.draw_text(8 + c * 40, 18, txt)
    bot_x = [8, 56, 104, 152, 200]
    for c, txt in enumerate(bot_labels):
        canvas.draw_text(bot_x[c], 24, txt)

def get_mock_events(n_events, vp_left, vp_right, vp_top, vp_bot, bias_y, spread_y):
    rng = random.Random(0xDEADBEEF)
    positions = []
    x = vp_right - 10
    for n in range(n_events):
        stride = rng.randint(8, 20)
        x -= stride
        if x < vp_left:
            break
        y = bias_y + int((rng.random() - 0.5) * spread_y * 1.5)
        y = max(vp_top + 2, min(vp_bot - 2, y))
        is_rest = (rng.random() < 0.1)
        burst_count = rng.randint(0, 2) if not is_rest and rng.random() < 0.3 else 0
        positions.append({"x": x, "y": y, "is_rest": is_rest, "age": n, "burst": burst_count})
    return positions

def render_constellations(canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)
    draw_header(canvas, "LIVE: CONSTELLATIONS")
    draw_footer(canvas, ["LoopR", "LiveM", "NewR", "NewM", "NEXT"])
    draw_info(canvas)

    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 25, 53
    bias_y = (vp_top + vp_bot) // 2
    spread_y = 10.0

    # Background cosmic dust
    rng = random.Random(1234)
    canvas.set_color(Color.Low)
    for _ in range(60):
        xx = rng.randint(vp_left, vp_right)
        yy = bias_y + int(rng.gauss(0, spread_y))
        if vp_top <= yy <= vp_bot:
            canvas.point(xx, yy)

    events = get_mock_events(15, vp_left, vp_right, vp_top, vp_bot, bias_y, spread_y)
    
    # Draw constellation lines first (legato/slides)
    for i in range(len(events) - 1):
        e1 = events[i]
        e2 = events[i+1]
        if not e1["is_rest"] and not e2["is_rest"]:
            fade = max(0, 100 - e1["age"] * 8)
            col = Color.MediumLow if fade > 50 else Color.Low
            canvas.set_color(col)
            canvas.line(e1["x"], e1["y"], e2["x"], e2["y"])

    # Draw stars
    for e in events:
        if e["is_rest"]:
            canvas.set_color(Color.Low)
            canvas.hline(e["x"]-2, bias_y, 5)
            continue
        
        fade = max(0, 100 - e["age"] * 8)
        if fade > 75: col = Color.Bright
        elif fade > 40: col = Color.MediumBright
        elif fade > 20: col = Color.Medium
        else: col = Color.Low
        
        canvas.set_color(col)
        # Star shape
        canvas.point(e["x"], e["y"])
        canvas.point(e["x"]-1, e["y"])
        canvas.point(e["x"]+1, e["y"])
        canvas.point(e["x"], e["y"]-1)
        canvas.point(e["x"], e["y"]+1)

        # Burst children (satellites)
        if e["burst"] > 0:
            canvas.set_color(Color.MediumLow)
            for b in range(e["burst"]):
                bx = e["x"] + rng.randint(-4, 4)
                by = e["y"] + rng.randint(-4, 4)
                canvas.point(bx, by)
                
    # Active star (newest)
    now_x = vp_right - 10
    now_y = bias_y + 4
    canvas.set_color(Color.Bright)
    canvas.fill_rect(now_x - 1, now_y - 1, 3, 3)
    canvas.point(now_x, now_y - 3)
    canvas.point(now_x, now_y + 3)
    canvas.point(now_x - 3, now_y)
    canvas.point(now_x + 3, now_y)

def render_rain_lattice(canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)
    draw_header(canvas, "LIVE: LATTICE MIST")
    draw_footer(canvas, ["LoopR", "LiveM", "NewR", "NewM", "NEXT"])
    draw_info(canvas)

    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 25, 53
    bias_y = (vp_top + vp_bot) // 2
    spread_y = 10.0
    
    # Lattice background
    canvas.set_color(Color.Low)
    for y in range(vp_top, vp_bot, 4):
        for x in range(vp_left, vp_right, 6):
            if (x+y)%3 == 0:
                canvas.point(x, y)

    events = get_mock_events(15, vp_left, vp_right, vp_top, vp_bot, bias_y, spread_y)
    
    # Draw snake / lattice connections
    for i in range(len(events) - 1):
        e1 = events[i]
        e2 = events[i+1]
        if not e1["is_rest"] and not e2["is_rest"]:
            fade = max(0, 100 - e1["age"] * 8)
            col = Color.MediumLow if fade > 50 else Color.Low
            canvas.set_color(col)
            # orthogonal snake path
            canvas.hline(e2["x"], e1["y"], e1["x"] - e2["x"])
            canvas.vline(e2["x"], min(e1["y"], e2["y"]), abs(e1["y"] - e2["y"]))

    # Draw drops
    for e in events:
        if e["is_rest"]:
            canvas.set_color(Color.Low)
            canvas.draw_text(e["x"]-2, vp_bot-4, ".")
            continue
        
        fade = max(0, 100 - e["age"] * 8)
        if fade > 75: col = Color.Bright
        elif fade > 40: col = Color.Medium
        else: col = Color.MediumLow
        
        canvas.set_color(col)
        canvas.fill_rect(e["x"]-1, e["y"]-1, 2, 2)
        # falling tail
        canvas.set_color(Color.Low)
        canvas.vline(e["x"], e["y"]+1, min(4, vp_bot - e["y"]))

def render_kreislauf_arc(canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)
    draw_header(canvas, "LIVE: KREISLAUF ARC")
    draw_footer(canvas, ["LoopR", "LiveM", "NewR", "NewM", "NEXT"])
    draw_info(canvas)

    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 25, 53
    vp_cy = (vp_top + vp_bot) // 2
    
    # Draw polar plot
    cx = 128
    cy = vp_cy + 4  # push down a bit
    base_r = 16
    
    # Background rings
    canvas.set_color(Color.Low)
    for r in [12, 16, 20]:
        for ang in range(180, 361, 5): # top half
            rad = math.radians(ang)
            x = cx + int(math.cos(rad) * r)
            y = cy + int(math.sin(rad) * r)
            if vp_top <= y <= vp_bot:
                canvas.point(x, y)

    events = get_mock_events(12, vp_left, vp_right, vp_top, vp_bot, vp_cy, 8)
    
    # events map to angle. newest = 180 (left), oldest = 360 (right).
    # actually right to left is better: newest = 360 (right), oldest = 180.
    for i in range(len(events) - 1):
        e1 = events[i]
        e2 = events[i+1]
        if not e1["is_rest"] and not e2["is_rest"]:
            a1 = 360 - e1["age"] * 12
            a2 = 360 - e2["age"] * 12
            r1 = base_r + (e1["y"] - vp_cy)
            r2 = base_r + (e2["y"] - vp_cy)
            x1 = cx + int(math.cos(math.radians(a1)) * r1)
            y1 = cy + int(math.sin(math.radians(a1)) * r1)
            x2 = cx + int(math.cos(math.radians(a2)) * r2)
            y2 = cy + int(math.sin(math.radians(a2)) * r2)
            
            fade = max(0, 100 - e1["age"] * 8)
            canvas.set_color(Color.MediumLow if fade > 50 else Color.Low)
            canvas.line(x1, y1, x2, y2)

    for e in events:
        ang = 360 - e["age"] * 12
        r = base_r + (e["y"] - vp_cy)
        x = cx + int(math.cos(math.radians(ang)) * r)
        y = cy + int(math.sin(math.radians(ang)) * r)
        
        if e["is_rest"]:
            canvas.set_color(Color.Low)
            canvas.point(x, y)
            continue
            
        fade = max(0, 100 - e["age"] * 8)
        if fade > 75: col = Color.Bright
        elif fade > 40: col = Color.MediumBright
        else: col = Color.MediumLow
        
        canvas.set_color(col)
        canvas.fill_rect(x-1, y-1, 3, 3)

        if e["age"] == 0:
            # Highlight newest
            canvas.set_color(Color.Bright)
            canvas.draw_rect(x-2, y-2, 5, 5)

def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'poetic-live-previews')
    os.makedirs(out_dir, exist_ok=True)
    
    fb = FrameBuffer(PAGE_W, PAGE_H)
    canvas = Canvas(fb)
    
    render_constellations(canvas)
    framebuffer_to_image(fb, scale=4).save(os.path.join(out_dir, 'live-constellations.png'))
    
    render_rain_lattice(canvas)
    framebuffer_to_image(fb, scale=4).save(os.path.join(out_dir, 'live-lattice.png'))
    
    render_kreislauf_arc(canvas)
    framebuffer_to_image(fb, scale=4).save(os.path.join(out_dir, 'live-kreislauf.png'))
    
    print("Generated previews in poetic-live-previews/")

if __name__ == '__main__':
    main()
