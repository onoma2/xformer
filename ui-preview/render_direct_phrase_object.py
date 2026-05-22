#!/usr/bin/env python3
"""
DIRECT-page preview: center-anchored phrase object.

This is a visual prototype only. It keeps the current 256x64 OLED constraints
but makes each Direct control change visible mass instead of 1px decoration.
"""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, Color, BlendMode, framebuffer_to_image

PAGE_W = 256
PAGE_H = 64


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def draw_header(canvas, title):
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    canvas.draw_text(8, 8, title)
    canvas.set_color(Color.Low)
    canvas.hline(0, 12, PAGE_W)


def draw_footer(canvas, labels):
    canvas.set_font(Font.Tiny)
    slot_w = PAGE_W // len(labels)
    canvas.set_color(Color.Medium)
    for i, label in enumerate(labels):
        if label:
            canvas.draw_text(i * slot_w + 4, PAGE_H - 2, label)


def thick_line(canvas, x0, y0, x1, y1, thickness=1):
    for o in range(-(thickness // 2), thickness // 2 + 1):
        canvas.line(x0, y0 + o, x1, y1 + o)


def draw_bias_spread_field(canvas, vp_left, vp_right, vp_top, vp_bot, bias_y, spread, amp):
    # Sparse bell field: the old background language, with lower density and
    # enough air for the phrase object to stay readable.
    spread_y = max(3, (spread * (vp_bot - vp_top)) // 170)
    for y in range(vp_top, vp_bot + 1):
        dy = y - bias_y
        env = 255 - (dy * dy * 160) // max(1, spread_y * spread_y)
        if env < 45:
            continue
        step = 3 if env > 170 else 4 if env > 95 else 5
        offset = (y * 5 + spread) % step
        canvas.set_color(Color.MediumLow if env > 205 else Color.Low)
        for x in range(vp_left + offset, vp_right, step):
            if ((x + y * 3) % 11) < 2:
                continue
            canvas.point(x, y)

    canvas.set_color(Color.MediumLow)
    canvas.hline(vp_left + 8, bias_y, vp_right - vp_left - 16)
    canvas.set_color(Color.Low)
    top = clamp(bias_y - amp, vp_top + 1, vp_bot - 1)
    bot = clamp(bias_y + amp, vp_top + 1, vp_bot - 1)
    for x in range(vp_left + 12, vp_right - 12, 3):
        canvas.point(x, top)
        canvas.point(x, bot)


def node(canvas, x, y, rx, ry, color):
    canvas.set_color(color)
    canvas.hline(x - rx, y, rx * 2 + 1)
    for yy in range(1, ry + 1):
        w = max(1, rx * 2 + 1 - yy * 2)
        canvas.hline(x - w // 2, y - yy, w)
        canvas.hline(x - w // 2, y + yy, w)


def make_phrase(params):
    rng = random.Random(0x5170C4A5)
    count = 10
    duration = params["duration"]
    variation = params["variation"]
    rest = params["rest"]
    range_ = params["range"]
    complexity = params["complexity"]
    contour = params["contour"]
    bias = params["bias"]
    spread = params["spread"]
    burst = params["burst"]
    burst_count = params["burst_count"]
    burst_rate = params["burst_rate"]

    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 26, 53
    cx = (vp_left + vp_right) // 2
    bias_y = vp_top + int((100 - bias) * (vp_bot - vp_top) / 100)

    raw_span = 64 + duration * 12
    span = clamp(raw_span, 60, (vp_right - vp_left) - 22)
    stride = span / (count - 1)
    amp = 3 + range_ * 3 + spread // 14
    amp = clamp(amp, 4, 15)
    complexity_amp = 1 + complexity // 14
    contour_step = int(contour * 6 / 100)

    events = []
    for i in range(count):
        t = i / (count - 1)
        x = int(cx - span / 2 + i * stride)
        x += int((variation * rng.randint(-4, 4)) / 85)

        wave = [0, -2, 3, -1, 5, 1, -4, 2, -3, 0][i]
        y = bias_y + int(wave * amp / 5)
        y += int((rng.randint(-complexity_amp, complexity_amp) * complexity) / 55)
        y += int((i - (count - 1) / 2) * contour_step / 4)
        y = clamp(y, vp_top + 3, vp_bot - 4)

        is_rest = (i in {2, 7} and rest >= 30) or (i in {1, 5, 8} and rest >= 70)
        has_burst = (burst > 0) and (i in {3, 6, 8}) and not is_rest
        satellites = 0
        if has_burst:
            satellites = 1 + clamp(burst_count // 22, 0, 4)
        events.append({
            "x": x,
            "y": y,
            "rest": is_rest,
            "satellites": satellites,
            "burst_gap": clamp(7 - burst_rate // 18, 2, 7),
        })
    return events, bias_y, amp, span


def render(canvas, **overrides):
    params = {
        "duration": 5,
        "variation": 30,
        "rest": 10,
        "range": 2,
        "burst": 45,
        "burst_count": 45,
        "burst_rate": 55,
        "burst_pitch": 1,
        "complexity": 55,
        "contour": 25,
        "bias": 50,
        "spread": 45,
        "repeat": 45,
        "gate": 55,
        "slide": 65,
        "legato": 45,
        "held": -1,
    }
    params.update(overrides)

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)
    draw_header(canvas, "DIRECT")

    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 26, 53
    events, bias_y, amp, span = make_phrase(params)

    draw_bias_spread_field(canvas, vp_left, vp_right, vp_top, vp_bot,
                           bias_y, params["spread"], amp)

    # Repeat ghosts behind the phrase.
    if params["repeat"] > 20:
        ghost_offset = 2 + params["repeat"] // 25
        canvas.set_color(Color.Low)
        for ev in events:
            if not ev["rest"]:
                canvas.fill_rect(ev["x"] - 1 - ghost_offset, ev["y"] - 1, 3, 3)

    # Legato and slide create readable foreground mass.
    for a, b in zip(events, events[1:]):
        if a["rest"] or b["rest"]:
            continue
        if params["legato"] > 35:
            canvas.set_color(Color.MediumLow)
            rail_y = min(vp_bot - 2, max(a["y"], b["y"]) + 3)
            canvas.hline(a["x"], rail_y, max(1, b["x"] - a["x"] + 1))
            canvas.hline(a["x"] + 1, rail_y + 1, max(1, b["x"] - a["x"] - 1))
        if params["slide"] > 0:
            canvas.set_color(Color.Medium if params["slide"] > 55 else Color.MediumLow)
            thick_line(canvas, a["x"], a["y"], b["x"], b["y"], 3 if params["slide"] > 70 else 1)

    # Complexity branches: visible alternate candidates.
    if params["complexity"] > 35:
        canvas.set_color(Color.MediumLow if params["complexity"] < 75 else Color.Medium)
        branch_count = 1 + params["complexity"] // 35
        for i, ev in enumerate(events):
            if ev["rest"] or i % 2:
                continue
            for b in range(branch_count):
                yy = clamp(ev["y"] + (-4 if b % 2 == 0 else 4), vp_top + 2, vp_bot - 2)
                canvas.line(ev["x"], ev["y"], ev["x"] + 5, yy)
                canvas.point(ev["x"] + 6, yy)

    radius_x = 2 + params["gate"] // 35
    radius_y = 1 + params["gate"] // 60
    for i, ev in enumerate(events):
        x, y = ev["x"], ev["y"]
        if ev["rest"]:
            canvas.set_color(Color.Medium)
            canvas.hline(x - 4, y, 9)
            canvas.hline(x - 2, y + 1, 5)
            canvas.set_color(Color.Low)
            canvas.vline(x, y - 3, 7)
            continue

        is_head = i == len(events) - 1
        strength = 100 - i * 5 + params["repeat"] // 2
        color = Color.Bright if is_head else (
            Color.MediumBright if strength > 75 else Color.Medium
        )
        node(canvas, x, y, radius_x + (1 if is_head else 0),
             radius_y + (1 if is_head else 0), color)
        if is_head:
            canvas.set_color(Color.MediumBright)
            canvas.point(x - radius_x - 2, y)
            canvas.point(x + radius_x + 2, y)
            canvas.point(x, y - radius_y - 2)
            canvas.point(x, y + radius_y + 2)

        if ev["satellites"]:
            canvas.set_color(Color.MediumBright)
            for s in range(ev["satellites"]):
                sx = x + (s + 1) * ev["burst_gap"]
                sy = y
                if params["burst_pitch"]:
                    sy += [-3, 2, -1, 4, -4][s % 5]
                if sx < vp_right - 3:
                    sy = clamp(sy, vp_top + 1, vp_bot - 1)
                    canvas.hline(sx - 1, sy, 3)
                    canvas.point(sx, sy - 1)
                    canvas.point(sx, sy + 1)

    labels = [
        ("DUR", params["duration"], 8, 18),
        ("VAR", params["variation"], 40, 18),
        ("RES", params["rest"], 72, 18),
        ("RNG", params["range"], 104, 18),
        ("BUR", params["burst"], 136, 18),
        ("BCN", params["burst_count"], 168, 18),
        ("BRT", params["burst_rate"], 200, 18),
        ("BPI", "G" if params["burst_pitch"] else "P", 232, 18),
        ("CMP", params["complexity"], 8, 24),
        ("CON", f"{params['contour']:+d}", 56, 24),
        ("BIA", params["bias"], 104, 24),
        ("SPR", params["spread"], 152, 24),
        ("REP", params["repeat"], 200, 24),
    ]
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    for name, value, x, y in labels:
        canvas.draw_text(x, y, f"{name}{value}")

    draw_footer(canvas, ["LiveR", "LiveM", "NewR", "NewM", "NEXT"])


def save_case(out_dir, name, **params):
    fb = FrameBuffer(PAGE_W, PAGE_H)
    canvas = Canvas(fb, brightness=1.0)
    render(canvas, **params)
    img = framebuffer_to_image(fb, scale=4)
    path = os.path.join(out_dir, f"{name}.png")
    img.save(path)
    return path


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "direct-phrase-object")
    os.makedirs(out_dir, exist_ok=True)

    cases = [
        ("phrase-default", {}),
        ("phrase-duration-low", {"duration": 1}),
        ("phrase-duration-high", {"duration": 7}),
        ("phrase-rest-high", {"rest": 75}),
        ("phrase-burst-high", {"burst": 90, "burst_count": 90, "burst_rate": 90}),
        ("phrase-complexity-low", {"complexity": 10}),
        ("phrase-complexity-high", {"complexity": 95}),
        ("phrase-contour-down", {"contour": -80}),
        ("phrase-gate-slide-legato", {"gate": 95, "slide": 90, "legato": 90}),
    ]

    paths = []
    for name, params in cases:
        path = save_case(out_dir, name, **params)
        paths.append(path)
        print(path)

    from PIL import Image
    images = [Image.open(path) for path in paths]
    w, h = images[0].size
    sheet = Image.new("L", (w * 3, h * 3), 0)
    for i, img in enumerate(images):
        sheet.paste(img, ((i % 3) * w, (i // 3) * h))
    sheet_path = os.path.join(out_dir, "contact-sheet.png")
    sheet.save(sheet_path)
    print(sheet_path)


if __name__ == "__main__":
    main()
