#!/usr/bin/env python3
"""
Three art-forward Live page concepts synthesized from norns visual idioms.

1. TIDE POOL  — growing ripples from circles + dunes terrain + constellation twinkle
2. ROOT SYSTEM — L-system branches from flora + dot tracker from meadowphysics
3. SAND DUNES  — terrain silhouette from dunes + star field from constellations

Coordinate system: header y=0-10, content y=11-52 (42px), footer y=53-63.

Usage:
    cd ui-preview
    python3 render_stochastic_art.py
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, Color, BlendMode, framebuffer_to_image

PAGE_W = 256
PAGE_H = 64
HEADER_SEP_Y = 10
FOOTER_SEP_Y = 53
CONTENT_TOP = 11
CONTENT_BOT = 52
CONTENT_H = CONTENT_BOT - CONTENT_TOP + 1
FOOTER_DIVIDERS = [51, 102, 153, 204]
FOOTER_LABEL_Y = 61
TINY_Y_MIN = CONTENT_TOP + 4      # 15
SMALL_Y_MIN = CONTENT_TOP + 6     # 17


def draw_header(canvas, title, label=None):
    canvas.set_color(Color.Bright)
    canvas.set_font(Font.Small)
    canvas.draw_text(8, 6, title)
    if label:
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Medium)
        tw = len(label) * 4
        canvas.draw_text(PAGE_W - 8 - tw, 6, label)
    canvas.set_color(Color.Low)
    canvas.hline(0, HEADER_SEP_Y, PAGE_W)


def draw_footer(canvas, labels, active=-1):
    canvas.set_color(Color.Low)
    canvas.hline(0, FOOTER_SEP_Y, PAGE_W)
    for dx in FOOTER_DIVIDERS:
        canvas.vline(dx, FOOTER_SEP_Y + 1, PAGE_H - FOOTER_SEP_Y - 1)
    slot_w = PAGE_W // len(labels)
    canvas.set_font(Font.Tiny)
    for i, lbl in enumerate(labels):
        if lbl is None:
            continue
        cx = i * slot_w + slot_w // 2
        if i == active:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(i * slot_w + 1, FOOTER_SEP_Y + 2, slot_w - 2, 8)
            canvas.set_color(Color.None_)
        else:
            canvas.set_color(Color.Medium)
        tw = len(lbl) * 4
        canvas.draw_text(cx - tw // 2, FOOTER_LABEL_Y, lbl)


def blank_canvas():
    fb = FrameBuffer(PAGE_W, PAGE_H)
    c = Canvas(fb, brightness=1.0)
    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.None_)
    c.fill_rect(0, 0, PAGE_W, PAGE_H)
    return fb, c


def save(fb, name, out_dir):
    img = framebuffer_to_image(fb, scale=4)
    path = os.path.join(out_dir, f"{name}.png")
    img.save(path)
    print(f"  {name:40s} -> {path}")


# Simulated event trail (same seed for consistency)
def make_trail(n=12, bias=50, spread=50, contour=0, rng_seed=0xC0FFEE):
    rng = random.Random(rng_seed)
    events = []
    for i in range(n):
        cv = (rng.random() - 0.5) * 2 * spread / 50
        cv += contour * i * 0.004
        cv = max(-1.0, min(1.0, cv))
        is_rest = rng.random() < 0.12
        has_children = rng.random() < 0.15
        events.append((cv, is_rest, has_children))
    return events


# ═══════════════════════════════════════════════════════════════════════
# CONCEPT 1: TIDE POOL
# Circles (norns) + dunes terrain + constellation twinkle
#
# The field is a luminous fog (the bell). Events drop as stones into
# still water — each one radiates concentric ripples. The newest stone
# is bright and sharp; older stones have wide, faded rings that blend
# into the fog. Pitch determines where the stone hits the surface.
# Rests are silent — no ripple, just a ghost dot.
#
# The whole surface breathes. It is not a cockpit — it is weather.
# ═══════════════════════════════════════════════════════════════════════

def render_tide_pool(canvas):
    draw_header(canvas, "LIVE", "tide")

    margin = 8
    vp_top, vp_bot = CONTENT_TOP, CONTENT_BOT
    vp_h = vp_bot - vp_top
    vp_cy = (vp_top + vp_bot) // 2
    rng = random.Random(0xA7EA)

    # Luminous fog — the bell envelope as a soft vertical gradient
    bias, spread = 50, 50
    bias_y = vp_top + int((100 - bias) * vp_h / 100)
    spread_y = max(2.0, (spread * vp_h) / 200.0)
    for y in range(vp_top, vp_bot + 1):
        d = (y - bias_y) / spread_y
        env = math.exp(-d * d * 1.4)
        if env < 0.08:
            continue
        col = (Color.Low if env < 0.3 else
               Color.MediumLow if env < 0.65 else Color.Medium)
        canvas.set_color(col)
        # Sparse horizontal haze lines, not solid fill
        step = 4 if env < 0.3 else 3
        offset = (y * 3) % step
        for x in range(margin + offset, PAGE_W - margin, step):
            if ((x + y * 7) % 13) < 2:
                continue
            canvas.point(x, y)

    # Drop stones — ripples
    events = make_trail(10, bias, spread, contour=0)
    cx = (margin + PAGE_W - margin) // 2
    base_stride = 22

    for i, (cv, is_rest, burst) in enumerate(reversed(events)):
        age = len(events) - 1 - i
        # Horizontal position: newest at right, spreading left
        ex = cx + 60 - i * base_stride
        if ex < margin or ex > PAGE_W - margin:
            continue

        # Vertical: pitch maps to surface position
        ey = bias_y + int(cv * vp_h * 0.4)
        ey = max(vp_top + 3, min(vp_bot - 3, ey))

        if is_rest:
            # Ghost dot — no ripple, just presence
            canvas.set_color(Color.Low)
            canvas.point(ex, ey)
            continue

        # Ripple rings — older = wider + dimmer
        fade = max(5, 100 - age * 15)
        n_rings = 1 + (100 - fade) // 30  # 1-4 rings
        for r in range(n_rings):
            radius = 2 + r * 3 + (100 - fade) // 20
            if fade > 70:
                col = Color.Bright if r == 0 else Color.MediumBright
            elif fade > 40:
                col = Color.Medium if r == 0 else Color.MediumLow
            else:
                col = Color.Low
            canvas.set_color(col)
            # Draw ring as 8 points (no circle primitive)
            for a in range(16):
                angle = a * math.pi * 2 / 16
                px = ex + int(radius * math.cos(angle))
                py = ey + int(radius * math.sin(angle))
                if vp_top <= py <= vp_bot and margin <= px <= PAGE_W - margin:
                    canvas.point(px, py)

        # Center stone
        if age == 0:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(ex - 1, ey - 1, 3, 3)
            # Crosshair beacon
            canvas.set_color(Color.MediumBright)
            for dx, dy in [(4, 0), (-4, 0), (0, 4), (0, -4)]:
                px, py = ex + dx, ey + dy
                if vp_top <= py <= vp_bot:
                    canvas.point(px, py)
        else:
            canvas.set_color(Color.MediumBright if fade > 50 else Color.Medium)
            canvas.point(ex, ey)

    # Constellation twinkle — random dim stars in the fog
    rng2 = random.Random(0x5EA)
    for _ in range(30):
        sx = rng2.randint(margin, PAGE_W - margin)
        sy = rng2.randint(vp_top, vp_bot)
        d = abs(sy - bias_y) / max(1, spread_y)
        if d > 2.0:
            continue
        brightness = math.exp(-d * d * 0.8)
        if brightness < 0.15:
            continue
        col = Color.Low if brightness < 0.5 else Color.MediumLow
        canvas.set_color(col)
        canvas.point(sx, sy)

    draw_footer(canvas, ["LiveR", "LiveM", "NewR", "NewM", "NEXT"])


# ═══════════════════════════════════════════════════════════════════════
# CONCEPT 2: ROOT SYSTEM
# L-system branches (flora) + dot tracker (meadowphysics) + organic growth
#
# A tree grows from the right edge. Each event is a branch. The trunk
# is the timeline — thick and bright near the base (now), thinning as
# it reaches back into the past. Pitch determines branch angle: high
# notes reach up, low notes reach down. Rests are nodes with no branch.
# Complexity controls how many sub-branches sprout. The whole organism
# breathes — it is alive, not plotted.
# ═══════════════════════════════════════════════════════════════════════

def render_root_system(canvas):
    draw_header(canvas, "LIVE", "root")

    margin = 8
    vp_top, vp_bot = CONTENT_TOP, CONTENT_BOT
    vp_h = vp_bot - vp_top
    vp_cy = (vp_top + vp_bot) // 2

    events = make_trail(10, bias=50, spread=50, contour=0, rng_seed=0xF00D)

    # Trunk: main axis from right to left
    trunk_x_start = PAGE_W - margin - 4
    trunk_x_end = margin + 20
    trunk_y = vp_cy

    # Draw trunk — thick at base (right), thin at root tip (left)
    n_events = len(events)
    for x in range(trunk_x_end, trunk_x_start + 1):
        # Trunk thickens toward the right (now)
        t = (x - trunk_x_end) / max(1, trunk_x_start - trunk_x_end)
        thickness = 1 + int(t * 2)  # 1-3 px
        fade = int(t * 100)
        col = (Color.Bright if fade > 70 else
               Color.Medium if fade > 40 else
               Color.MediumLow if fade > 15 else Color.Low)
        canvas.set_color(col)
        for dy in range(-thickness // 2, thickness // 2 + 1):
            canvas.point(x, trunk_y + dy)

    # Branches sprout from trunk positions
    branch_spacing = (trunk_x_start - trunk_x_end) / max(1, n_events - 1)

    for i, (cv, is_rest, burst) in enumerate(reversed(events)):
        age = n_events - 1 - i
        bx = trunk_x_start - i * branch_spacing
        bx = int(bx)
        if bx < trunk_x_end or bx > trunk_x_start:
            continue

        fade = max(10, 100 - age * 12)
        col = (Color.Bright if fade > 70 else
               Color.MediumBright if fade > 50 else
               Color.Medium if fade > 30 else
               Color.MediumLow if fade > 15 else Color.Low)

        if is_rest:
            # Bare node on trunk
            canvas.set_color(Color.Low)
            canvas.fill_rect(int(bx) - 1, trunk_y - 1, 3, 3)
            continue

        # Branch angle from pitch cv (-1..+1 → -60°..+60°)
        angle = cv * math.pi / 3
        branch_len = 8 + int(fade * 12 / 100)

        # Main branch
        end_x = bx + int(branch_len * math.cos(angle))
        end_y = trunk_y - int(branch_len * math.sin(angle))
        end_x = max(margin, min(PAGE_W - margin, end_x))
        end_y = max(vp_top + 1, min(vp_bot - 1, end_y))

        canvas.set_color(col)
        canvas.line(int(bx), trunk_y, end_x, end_y)

        # Node at branch tip
        if age == 0:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(end_x - 1, end_y - 1, 3, 3)
        else:
            canvas.point(end_x, end_y)

        # Sub-branches for burst events
        if burst and fade > 30:
            sub_angle = angle + 0.5
            sub_len = branch_len // 2
            sx = bx + int(sub_len * math.cos(sub_angle))
            sy = trunk_y - int(sub_len * math.sin(sub_angle))
            sy = max(vp_top + 1, min(vp_bot - 1, sy))
            canvas.set_color(Color.MediumLow)
            canvas.line(end_x, end_y, sx, sy)
            canvas.point(sx, sy)

            sub_angle2 = angle - 0.4
            sx2 = bx + int(sub_len * math.cos(sub_angle2))
            sy2 = trunk_y - int(sub_len * math.sin(sub_angle2))
            sy2 = max(vp_top + 1, min(vp_bot - 1, sy2))
            canvas.line(end_x, end_y, sx2, sy2)

    # Soil line — faint ground reference
    canvas.set_color(Color.Low)
    canvas.hline(margin, vp_bot - 1, PAGE_W - 2 * margin)

    # Root hairs — small downward lines from trunk
    rng = random.Random(0xB00F)
    for i in range(15):
        rx = rng.randint(trunk_x_end + 10, trunk_x_start)
        rh = rng.randint(2, 6)
        canvas.set_color(Color.Low)
        canvas.vline(rx, vp_bot - 1 - rh, rh)

    draw_footer(canvas, ["LiveR", "LiveM", "NewR", "NewM", "NEXT"])


# ═══════════════════════════════════════════════════════════════════════
# CONCEPT 3: SAND DUNES
# Terrain profile (dunes) + star field (constellations) + horizon glow
#
# The note history is a mountain ridge. Each event raises a peak whose
# height is pitch. The ridge runs left-to-right across the full width.
# Newer peaks are bright and sharp; older peaks erode into the sand.
# Above the ridge: a star field where density = spread. The current
# note has a beacon star — a cross of 5 bright pixels. Below the
# ridge: the bell envelope is a luminous desert floor, warm and dim.
# It is landscape, not data. You read it like terrain.
# ═══════════════════════════════════════════════════════════════════════

def render_sand_dunes(canvas):
    draw_header(canvas, "LIVE", "dune")

    margin = 8
    vp_top, vp_bot = CONTENT_TOP, CONTENT_BOT
    vp_h = vp_bot - vp_top
    vp_cy = (vp_top + vp_bot) // 2

    events = make_trail(16, bias=50, spread=50, contour=5, rng_seed=0xD00D)

    # Desert floor — luminous gradient below the ridge line
    ridge_base_y = vp_cy + vp_h // 4   # ridge sits in upper 3/4
    for y in range(ridge_base_y, vp_bot + 1):
        d = (y - ridge_base_y) / max(1, vp_bot - ridge_base_y)
        col = Color.Low if d < 0.7 else Color.MediumLow
        canvas.set_color(col)
        # Sparse horizontal lines
        step = 3 + int(d * 3)
        offset = (y * 5) % step
        for x in range(margin + offset, PAGE_W - margin, step):
            canvas.point(x, y)

    # Ridge profile — smooth terrain line
    n = len(events)
    ridge_pts = []
    x_start, x_end = margin, PAGE_W - margin
    step_x = (x_end - x_start) / max(1, n - 1)

    contour_drift = 0
    for i, (cv, is_rest, burst) in enumerate(events):
        x = int(x_start + i * step_x)
        # Height: pitch maps to peak elevation above base
        h = int(abs(cv) * vp_h * 0.55)
        if cv < 0:
            h = int(-cv * vp_h * 0.3)  # low notes = shallow dunes
        else:
            h = int(cv * vp_h * 0.55)  # high notes = tall peaks

        contour_drift += 5 * 0.004 * i
        y = ridge_base_y - h - int(contour_drift * 3)
        y = max(vp_top + 2, min(vp_bot - 2, y))

        if is_rest:
            # Saddle/dip in the ridge
            y = ridge_base_y - 1
        ridge_pts.append((x, y, is_rest, i))

    # Draw the ridge silhouette — filled from ridge line down to base
    # First: ridge line itself
    for i in range(len(ridge_pts) - 1):
        x1, y1, rest1, idx1 = ridge_pts[i]
        x2, y2, rest2, idx2 = ridge_pts[i + 1]

        age = n - 1 - idx1
        fade = max(10, 100 - age * 7)
        col = (Color.Bright if fade > 70 else
               Color.MediumBright if fade > 50 else
               Color.Medium if fade > 30 else
               Color.MediumLow if fade > 15 else Color.Low)

        canvas.set_color(col)
        canvas.line(x1, y1, x2, y2)

        # Fill below the line segment (terrain mass)
        y_max = max(y1, y2)
        fill_col = Color.Low if fade < 40 else Color.MediumLow
        canvas.set_color(fill_col)
        for fy in range(y_max + 1, ridge_base_y + 1, 2):
            fx_start = min(x1, x2)
            fx_end = max(x1, x2)
            # Interpolate ridge height at this y
            if fy < ridge_base_y:
                canvas.hline(fx_start, fy, fx_end - fx_start + 1)

    # Beacon star on current peak
    if ridge_pts:
        bx, by, _, _ = ridge_pts[-1]
        canvas.set_color(Color.Bright)
        canvas.fill_rect(bx - 1, by - 1, 3, 3)
        # Cross rays
        for dx, dy, length in [(0, -1, 4), (0, 1, 4), (-1, 0, 5), (1, 0, 5)]:
            for d in range(2, length):
                px = bx + dx * d
                py = by + dy * d
                if vp_top <= py <= vp_bot:
                    canvas.point(px, py)

    # Star field above the ridge
    rng = random.Random(0x57A8)
    spread_density = 40  # higher spread = more stars
    for _ in range(spread_density):
        sx = rng.randint(margin, PAGE_W - margin)
        sy = rng.randint(vp_top, ridge_base_y - 3)
        # Check if above local ridge
        above = True
        for x, y, _, _ in ridge_pts:
            if abs(sx - x) < step_x / 2 and sy >= y:
                above = False
                break
        if not above:
            continue
        brightness = rng.random()
        if brightness < 0.5:
            col = Color.Low
        elif brightness < 0.85:
            col = Color.MediumLow
        else:
            col = Color.Medium
        canvas.set_color(col)
        canvas.point(sx, sy)
        # A few stars are brighter with a small cross
        if brightness > 0.92:
            canvas.point(sx + 1, sy)
            canvas.point(sx - 1, sy)

    # Wind lines — faint diagonal streaks across the dunes
    canvas.set_color(Color.Low)
    rng_wind = random.Random(0xB00F)
    for _ in range(8):
        wx = rng_wind.randint(margin, PAGE_W - margin - 20)
        wy = rng_wind.randint(ridge_base_y - 5, vp_bot - 2)
        wl = rng_wind.randint(6, 18)
        canvas.hline(wx, wy, wl)

    draw_footer(canvas, ["LiveR", "LiveM", "NewR", "NewM", "NEXT"])


# ═══════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════

def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           'stochastic-art-previews')
    os.makedirs(out_dir, exist_ok=True)
    print("Stochastic Art — rendering 3 concepts...")

    concepts = [
        ("tide-pool",    render_tide_pool),
        ("root-system",  render_root_system),
        ("sand-dunes",   render_sand_dunes),
    ]

    for name, render_fn in concepts:
        fb, canvas = blank_canvas()
        render_fn(canvas)
        save(fb, name, out_dir)

    # Composite all three
    from PIL import Image
    imgs = []
    for name, _ in concepts:
        p = os.path.join(out_dir, f"{name}.png")
        imgs.append(Image.open(p))
    w = max(im.width for im in imgs)
    total_h = sum(im.height for im in imgs) + 10 * (len(imgs) - 1)
    comp = Image.new("L", (w, total_h), 255)
    y = 0
    for im in imgs:
        comp.paste(im, (0, y))
        y += im.height + 10
    path = os.path.join(out_dir, "00-all-concepts.png")
    comp.save(path)
    print(f"  {'00-all-concepts':40s} -> {path}")
    print("Done.")


if __name__ == "__main__":
    main()
