#!/usr/bin/env python3
"""
Stochastic sequence-edit page audit: all 4 views, current vs improved.

Coordinate system (UI-VARIANTS.md / WindowPainter):
  Header:  y = 0..10  — separator at y=10, text baseline at y=6
  Content: y = 11..52 — safe area, 42 px height
  Footer:  y = 53..63 — separator at y=53, dividers at x=51,102,153,204, labels at y=61

Font yOffset (draw_text bitmap top = y + yOffset):
  Tiny  (5×5): yOffset = -4 → bitmap at y-4 .. y   → min safe y = 11+4 = 15
  Small (7×7): yOffset = -6 → bitmap at y-6 .. y   → min safe y = 11+6 = 17
  Max safe y for any text = 52 (bottom pixel = y)

Usage:
    cd ui-preview
    python3 render_stochastic_audit.py
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
CONTENT_H = CONTENT_BOT - CONTENT_TOP + 1       # 42 px
FOOTER_DIVIDERS = [51, 102, 153, 204]
FOOTER_LABEL_Y = 61

# Font-safe y boundaries within content area
TINY_Y_MIN = CONTENT_TOP + 4                     # 15 (bitmap top = 11)
SMALL_Y_MIN = CONTENT_TOP + 6                    # 17 (bitmap top = 11)
TEXT_Y_MAX = CONTENT_BOT                          # 52 (bitmap bottom)


# ── Shared helpers ──────────────────────────────────────────────────────

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


# ═══════════════════════════════════════════════════════════════════════
# PAGE 1: LIVE — the walker
# Content layout (42px, y=11..52):
#   y=11..15  Tiny row 1 — rhythm params (5px)
#   y=16      gap
#   y=17..21  Tiny row 2 — pitch params (5px)
#   y=22..23  gap
#   y=24..51  Walker viewport (28px)
# ═══════════════════════════════════════════════════════════════════════

def render_live_current(canvas):
    draw_header(canvas, "LIVE")

    # ── Walker viewport ──────────────────────────────────────────────
    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 24, CONTENT_BOT - 1                  # 24..51 = 28px
    vp_h = vp_bot - vp_top
    vp_cy = (vp_top + vp_bot) // 2

    bias, spread = 50, 50
    bias_y = vp_top + int((100 - bias) * vp_h / 100)
    spread_y = max(2.0, (spread * vp_h) / 200.0)
    for y in range(vp_top, vp_bot + 1):
        d = (y - bias_y) / spread_y
        env = math.exp(-d * d * 1.4)
        if env < 0.25:
            continue
        col = Color.Low if env < 0.85 else Color.MediumLow
        canvas.set_color(col)
        step = 2 if env >= 0.6 else 3
        offset = (y * 7) % step
        for x in range(vp_left + offset, vp_right, step):
            canvas.point(x, y)

    rng = random.Random(0xC0FFEE)
    duration, variation, rest = 5, 30, 10
    complexity, contour = 50, 0
    repeats = 20
    base_stride = 6 + (7 - duration) * 5
    positions = []
    x, y = vp_right - 2, bias_y
    for n in range(12):
        jit = rng.randint(-variation // 8, variation // 8) if variation > 0 else 0
        x -= max(2, base_stride + jit)
        if x < vp_left:
            break
        swing_h = int((spread * (vp_h - 4)) / 200)
        leap_range = 1 + int((complexity * swing_h) / 100)
        dy = rng.randint(-leap_range, leap_range)
        y = bias_y + int((rng.random() - 0.5) * 2 * swing_h * (spread / 100))
        y += dy
        y = max(vp_top + 2, min(vp_bot - 2, y))
        is_rest = rng.random() * 100 < rest
        positions.append((x, y, is_rest, n))

    for px, py, is_rest, age in reversed(positions):
        if is_rest:
            canvas.set_color(Color.Low)
            canvas.hline(px - 1, vp_cy, 3)
            continue
        fade = max(0, 100 - age * 12)
        ts = (fade + repeats) // 2
        col = (Color.MediumBright if ts >= 70 else
               Color.Medium if ts >= 45 else
               Color.MediumLow if ts >= 20 else Color.Low)
        canvas.set_color(col)
        if age == 0:
            canvas.fill_rect(px - 1, py - 1, 3, 3)
        else:
            canvas.point(px, py)

    now_x, now_y = vp_right - 2, bias_y
    canvas.set_color(Color.Bright)
    canvas.fill_rect(now_x - 2, now_y - 2, 5, 5)
    canvas.set_color(Color.MediumBright)
    canvas.point(now_x + 3, now_y)
    canvas.point(now_x - 3, now_y)
    canvas.point(now_x, now_y + 3)
    canvas.point(now_x, now_y - 3)

    # ── 16 tiny labels crammed into 2 rows ──
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    top = [("D 1/16", 0), ("V 30", 32), ("R 10", 64), ("N 1", 96),
           ("B 0", 128), ("C 0", 160), ("T 50", 192), ("F 50", 224)]
    for txt, x in top:
        canvas.draw_text(x, TINY_Y_MIN, txt)               # y=15, bitmap 11..15
    bot = [("X 50", 0), ("O +0", 32), ("I 50", 64), ("S 50", 96),
           ("E 20", 128), ("G 0", 160), ("L 0", 192), ("A 0", 224)]
    for txt, x in bot:
        canvas.draw_text(x, TINY_Y_MIN + 8, txt)            # y=23, bitmap 19..23

    draw_footer(canvas, ["LiveR", "LiveM", "NewR", "NewM", "NEXT"])


def render_live_improved(canvas):
    draw_header(canvas, "LIVE", "LiveR|LiveM")

    # ── Walker viewport — larger since fewer params ──
    vp_left, vp_right = 8, PAGE_W - 8
    vp_top, vp_bot = 24, CONTENT_BOT - 1
    vp_h = vp_bot - vp_top
    vp_cy = (vp_top + vp_bot) // 2

    bias, spread = 50, 50
    bias_y = vp_top + int((100 - bias) * vp_h / 100)
    spread_y = max(2.0, (spread * vp_h) / 200.0)
    for y in range(vp_top, vp_bot + 1):
        d = (y - bias_y) / spread_y
        env = math.exp(-d * d * 1.4)
        if env < 0.25:
            continue
        col = Color.Low if env < 0.85 else Color.MediumLow
        canvas.set_color(col)
        step = 2 if env >= 0.6 else 3
        offset = (y * 7) % step
        for x in range(vp_left + offset, vp_right, step):
            canvas.point(x, y)

    rng = random.Random(0xC0FFEE)
    duration, variation, rest = 5, 30, 10
    complexity, contour = 50, 0
    repeats = 20
    base_stride = 6 + (7 - duration) * 5
    positions = []
    x, y = vp_right - 2, bias_y
    for n in range(12):
        jit = rng.randint(-variation // 8, variation // 8) if variation > 0 else 0
        x -= max(2, base_stride + jit)
        if x < vp_left:
            break
        swing_h = int((spread * (vp_h - 4)) / 200)
        leap_range = 1 + int((complexity * swing_h) / 100)
        dy = rng.randint(-leap_range, leap_range)
        y = bias_y + int((rng.random() - 0.5) * 2 * swing_h * (spread / 100))
        y += dy
        y = max(vp_top + 2, min(vp_bot - 2, y))
        is_rest = rng.random() * 100 < rest
        positions.append((x, y, is_rest, n))

    for px, py, is_rest, age in reversed(positions):
        if is_rest:
            canvas.set_color(Color.Low)
            canvas.hline(px - 1, vp_cy, 3)
            continue
        fade = max(0, 100 - age * 12)
        ts = (fade + repeats) // 2
        col = (Color.MediumBright if ts >= 70 else
               Color.Medium if ts >= 45 else
               Color.MediumLow if ts >= 20 else Color.Low)
        canvas.set_color(col)
        if age == 0:
            canvas.fill_rect(px - 1, py - 1, 3, 3)
        else:
            canvas.point(px, py)

    now_x, now_y = vp_right - 2, bias_y
    canvas.set_color(Color.Bright)
    canvas.fill_rect(now_x - 2, now_y - 2, 5, 5)
    canvas.set_color(Color.MediumBright)
    canvas.point(now_x + 3, now_y)
    canvas.point(now_x - 3, now_y)
    canvas.point(now_x, now_y + 3)
    canvas.point(now_x, now_y - 3)

    # ── 2 grouped rows, readable labels ──
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    # Row 1: Rhythm — y=15, bitmap 11..15
    canvas.draw_text(4, TINY_Y_MIN, "DUR 1/16")
    canvas.draw_text(64, TINY_Y_MIN, "VAR 30")
    canvas.draw_text(120, TINY_Y_MIN, "RST 10")
    canvas.draw_text(176, TINY_Y_MIN, "RNG 1")
    # Group divider
    canvas.set_color(Color.Low)
    canvas.hline(4, 17, PAGE_W - 8)
    # Row 2: Pitch — y=19, bitmap 15..19
    canvas.set_color(Color.Medium)
    canvas.draw_text(4, 19, "MEL 50")
    canvas.draw_text(64, 19, "DRF +0")
    canvas.draw_text(120, 19, "BIA 50")
    canvas.draw_text(176, 19, "SPR 50")

    draw_footer(canvas, ["LoopR", "LoopM", "NewR", "NewM", "NEXT"])


# ═══════════════════════════════════════════════════════════════════════
# PAGE 2: LOOP — the tape
# Content layout (42px, y=11..52):
#   y=11..15  Tiny param row (5px)
#   y=18..32  Tape + brackets (15px tape, brackets 3px above/below)
#   y=36..38  Boredom bar (3px)
#   y=42..46  Bottom params (5px)
# ═══════════════════════════════════════════════════════════════════════

def render_loop_current(canvas):
    draw_header(canvas, "LOOP")

    margin = 10
    tape_top, tape_bot = 18, 32
    tape_h = tape_bot - tape_top + 1
    avail_w = PAGE_W - 2 * margin
    seq_size = 16
    lf, ll = 2, 13

    # Tape outline
    canvas.set_color(Color.Low)
    canvas.draw_rect(margin, tape_top, avail_w, tape_h)

    rng = random.Random(42)
    current_step = 7
    step_w = avail_w // seq_size
    for i in range(seq_size):
        x = margin + i * step_w
        in_window = (lf <= i <= ll)
        is_rest = rng.random() < 0.2
        is_active = (i == current_step)

        col = (Color.Bright if is_active else
               Color.Low if not in_window else
               Color.Medium if is_rest else Color.MediumBright)
        canvas.set_color(col)
        canvas.draw_rect(x, tape_top, step_w - 1, tape_h)

        if not is_rest and in_window:
            bar_h = rng.randint(3, tape_h - 3)
            fill_col = Color.Bright if is_active else Color.Medium
            canvas.set_color(fill_col)
            canvas.fill_rect(x + 1, tape_bot - bar_h, step_w - 3, bar_h)

    # Brackets
    bx_first = margin + lf * step_w
    bx_last = margin + (ll + 1) * step_w - 2
    canvas.set_color(Color.Bright)
    canvas.vline(bx_first - 1, tape_top - 3, tape_h + 7)
    canvas.hline(bx_first - 1, tape_top - 3, 4)
    canvas.hline(bx_first - 1, tape_bot + 3, 4)
    canvas.vline(bx_last + 1, tape_top - 3, tape_h + 7)
    canvas.hline(bx_last - 2, tape_top - 3, 4)
    canvas.hline(bx_last - 2, tape_bot + 3, 4)

    # Boredom bar
    boredom_y = 36
    half_w = (avail_w - 2) // 2
    fill_r, fill_m = 65, 30
    canvas.set_color(Color.Low)
    canvas.draw_rect(margin, boredom_y, half_w, 3)
    canvas.draw_rect(margin + half_w + 2, boredom_y, half_w, 3)
    canvas.set_color(Color.MediumBright)
    canvas.fill_rect(margin, boredom_y, (fill_r * half_w) // 100, 3)
    canvas.fill_rect(margin + half_w + 2, boredom_y, (fill_m * half_w) // 100, 3)

    # Tiny params — row at top
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(4, TINY_Y_MIN, "PR100")
    canvas.draw_text(34, TINY_Y_MIN, "PM100")
    canvas.draw_text(66, TINY_Y_MIN, "M+0")
    canvas.draw_text(94, TINY_Y_MIN, "J0")
    canvas.draw_text(120, TINY_Y_MIN, "S0")

    # Bottom params — y=42, bitmap 38..42
    canvas.draw_text(margin, 42, "MK100")
    canvas.draw_text(PAGE_W - margin - 32, 42, "TL+0")

    draw_footer(canvas, ["LoopR", "LoopM", "NewR", "NewM", "NEXT"])


def render_loop_improved(canvas):
    draw_header(canvas, "LOOP", "P:12")

    margin = 10
    tape_top, tape_bot = 18, 32
    tape_h = tape_bot - tape_top + 1
    avail_w = PAGE_W - 2 * margin
    seq_size = 16
    lf, ll = 2, 13

    canvas.set_color(Color.Low)
    canvas.draw_rect(margin, tape_top, avail_w, tape_h)

    rng = random.Random(42)
    current_step = 7
    step_w = avail_w // seq_size
    for i in range(seq_size):
        x = margin + i * step_w
        in_window = (lf <= i <= ll)
        is_rest = rng.random() < 0.2
        is_active = (i == current_step)

        col = (Color.Bright if is_active else
               Color.Low if not in_window else
               Color.Medium if is_rest else Color.MediumBright)
        canvas.set_color(col)
        canvas.draw_rect(x, tape_top, step_w - 1, tape_h)

        if not is_rest and in_window:
            bar_h = rng.randint(3, tape_h - 3)
            fill_col = Color.Bright if is_active else Color.Medium
            canvas.set_color(fill_col)
            canvas.fill_rect(x + 1, tape_bot - bar_h, step_w - 3, bar_h)

    # Brackets
    bx_first = margin + lf * step_w
    bx_last = margin + (ll + 1) * step_w - 2
    canvas.set_color(Color.Bright)
    canvas.vline(bx_first - 1, tape_top - 3, tape_h + 7)
    canvas.hline(bx_first - 1, tape_top - 3, 4)
    canvas.hline(bx_first - 1, tape_bot + 3, 4)
    canvas.vline(bx_last + 1, tape_top - 3, tape_h + 7)
    canvas.hline(bx_last - 2, tape_top - 3, 4)
    canvas.hline(bx_last - 2, tape_bot + 3, 4)

    # Boredom bar with labels
    boredom_y = 36
    half_w = (avail_w - 2) // 2
    fill_r, fill_m = 65, 30
    canvas.set_color(Color.Low)
    canvas.draw_rect(margin, boredom_y, half_w, 3)
    canvas.draw_rect(margin + half_w + 2, boredom_y, half_w, 3)
    canvas.set_color(Color.MediumBright)
    canvas.fill_rect(margin, boredom_y, (fill_r * half_w) // 100, 3)
    canvas.fill_rect(margin + half_w + 2, boredom_y, (fill_m * half_w) // 100, 3)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(margin, 40, "R")                      # bitmap 36..40
    canvas.draw_text(margin + half_w + 2, 40, "M")

    # Readable parameter labels — 2 rows
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    # Top row — y=15, bitmap 11..15
    canvas.draw_text(4, TINY_Y_MIN, "PAT 100|100  MUT +0  JMP 0  SLP 0")
    # Bottom row — y=48, bitmap 44..48
    canvas.draw_text(4, 48, "WIN 2-13  SIZ 16  ROT +0  MK 100  TL +0")

    draw_footer(canvas, ["LoopR", "LoopM", "NewR", "NewM", "NEXT"])


# ═══════════════════════════════════════════════════════════════════════
# PAGE 3: PITCH TICKETS
# Content layout (42px, y=11..52):
#   y=11..17  Small info text (7px)
#   y=19..43  Bar chart (25px max)
#   y=45..49  Note names (5px)
# ═══════════════════════════════════════════════════════════════════════

def render_pitch_current(canvas):
    draw_header(canvas, "SCALE TICKETS")

    scale_size = 7
    base_y = 43                                             # bar bottom
    bar_max_h = 24                                          # bars y=19..43
    bar_w = max(3, (PAGE_W - 16) // scale_size - 2)
    gap = 2
    total_w = scale_size * (bar_w + gap) - gap
    x_offset = max(8, (PAGE_W - total_w) // 2)

    tickets = [100, 80, 60, 0, 40, 60, 80]
    denom = max(1, max(tickets))
    active_degree = 2

    for i in range(scale_size):
        x = x_offset + i * (bar_w + gap)
        t = tickets[i]
        h = max(1, (t * bar_max_h) // denom) if denom > 0 else 1
        active = (i == active_degree)
        canvas.set_color(Color.Bright if active else Color.Medium)
        canvas.fill_rect(x, base_y - h, bar_w, h)
        if active:
            canvas.set_color(Color.Bright)
            canvas.hline(x - 1, base_y + 2, bar_w + 2)

        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Low)
        note_names = ["C", "D", "E", "F", "G", "A", "B"]
        tw = len(note_names[i]) * 4
        canvas.draw_text(x + (bar_w - tw) // 2, 49, note_names[i])  # bitmap 45..49

    # Info line — y=17, bitmap 11..17
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    canvas.draw_text(8, SMALL_Y_MIN, "DEG 3: 0")

    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(PAGE_W - 72, TINY_Y_MIN, "DROT:+0 MROT:+0")

    draw_footer(canvas, ["LoopM", "NewM", "DROT", "MROT", "NEXT"], active=2)


def render_pitch_improved(canvas):
    draw_header(canvas, "PITCH", "Major")

    scale_size = 7
    base_y = 43
    bar_max_h = 24
    bar_w = max(3, (PAGE_W - 16) // scale_size - 2)
    gap = 2
    total_w = scale_size * (bar_w + gap) - gap
    x_offset = max(8, (PAGE_W - total_w) // 2)

    tickets = [100, 80, 60, 0, 40, 60, 80]
    denom = max(1, max(tickets))
    note_names = ["C", "D", "E", "F", "G", "A", "B"]
    active_degree = 2
    selected = 4

    avg = sum(tickets) / len(tickets) if tickets else 0
    avg_h = int((avg * bar_max_h) / denom) if denom > 0 else 0
    if 0 < avg_h < bar_max_h:
        canvas.set_color(Color.Low)
        canvas.hline(x_offset - 2, base_y - avg_h, total_w + 4)

    for i in range(scale_size):
        x = x_offset + i * (bar_w + gap)
        t = tickets[i]
        active = (i == active_degree)
        sel = (i == selected)

        if t == 0:
            canvas.set_color(Color.Low)
            canvas.draw_rect(x, base_y - 2, bar_w - 1, 2)
        else:
            h = max(1, (t * bar_max_h) // denom) if denom > 0 else 1
            canvas.set_color(Color.Bright if active else
                             Color.MediumBright if sel else Color.Medium)
            canvas.fill_rect(x, base_y - h, bar_w, h)

        if active:
            canvas.set_color(Color.Bright)
            canvas.hline(x - 1, base_y + 2, bar_w + 2)
        elif sel:
            canvas.set_color(Color.MediumBright)
            canvas.hline(x - 1, base_y + 2, bar_w + 2)

        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Low)
        tw = len(note_names[i]) * 4
        canvas.draw_text(x + (bar_w - tw) // 2, 49, note_names[i])

    # Info — y=17, bitmap 11..17
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    canvas.draw_text(8, SMALL_Y_MIN, "G: 40/100")

    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(100, TINY_Y_MIN, "ROT +0  MSK +0")

    draw_footer(canvas, ["LoopM", "NewM", "ROT", "MSK", "NEXT"])


# ═══════════════════════════════════════════════════════════════════════
# PAGE 4: DURATION TICKETS
# Content layout (42px, y=11..52):
#   y=11..17  Small info text (7px)
#   y=19..43  Bar chart (25px max)
#   y=45..49  Duration labels (5px)
# ═══════════════════════════════════════════════════════════════════════

def render_duration_current(canvas):
    draw_header(canvas, "DURATION TICKETS")

    num_slots = 8
    base_y = 43
    bar_max_h = 24
    bar_w = 12
    gap = bar_w
    total_w = num_slots * (bar_w + gap) - gap
    x_offset = max(8, (PAGE_W - total_w) // 2)

    tickets = [0, 0, 0, 20, 50, 80, 30, 0]
    denom = max(max(tickets), 1)
    active_idx = 5
    labels = ["1/2", "1/4", "3/16", "1/8", "1/8T", "1/16", "1/16T", "1/32"]

    for i in range(num_slots):
        x = x_offset + i * (bar_w + gap)
        t = tickets[i]
        active = (i == active_idx)

        if t == 0:
            canvas.set_color(Color.Low)
            canvas.draw_rect(x, base_y - 2, bar_w - 1, 2)
        else:
            h = max(1, (t * bar_max_h) // denom)
            canvas.set_color(Color.Bright if active else
                             Color.MediumBright if i == 5 else Color.Medium)
            canvas.fill_rect(x, base_y - h, bar_w, h)

        if active:
            canvas.set_color(Color.Bright)
            canvas.hline(x - 1, base_y + 2, bar_w + 2)

        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Low)
        canvas.draw_text(x + 1, 49, labels[i])

    # Info — y=17, bitmap 11..17
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    canvas.draw_text(8, SMALL_Y_MIN, "1/16: 80")

    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(PAGE_W - 56, TINY_Y_MIN, "REST 0%")

    draw_footer(canvas, ["LiveR", "NewR", "RST", None, "NEXT"])


def render_duration_improved(canvas):
    draw_header(canvas, "DURATION", "LiveR")

    num_slots = 8
    base_y = 43
    bar_max_h = 24
    bar_w = 12
    gap = 10
    total_w = num_slots * (bar_w + gap) - gap
    x_offset = max(8, (PAGE_W - total_w) // 2)

    tickets = [0, 0, 0, 20, 50, 80, 30, 0]
    denom = max(max(tickets), 1)
    active_idx = 5
    selected = 5
    labels = ["1/2", "1/4", "3/16", "1/8", "1/8T", "1/16", "1/16T", "1/32"]

    avg = sum(tickets) / len(tickets) if tickets else 0
    avg_h = int((avg * bar_max_h) / denom) if denom > 0 else 0
    if 0 < avg_h < bar_max_h:
        canvas.set_color(Color.Low)
        canvas.hline(x_offset - 2, base_y - avg_h, total_w + 4)

    for i in range(num_slots):
        x = x_offset + i * (bar_w + gap)
        t = tickets[i]
        active = (i == active_idx)
        sel = (i == selected)

        if t == 0:
            canvas.set_color(Color.Low)
            canvas.draw_rect(x, base_y - 2, bar_w - 1, 2)
        else:
            h = max(1, (t * bar_max_h) // denom)
            canvas.set_color(Color.Bright if active else
                             Color.MediumBright if sel else Color.Medium)
            canvas.fill_rect(x, base_y - h, bar_w, h)

        if active:
            canvas.set_color(Color.Bright)
            canvas.hline(x - 1, base_y + 2, bar_w + 2)
        elif sel:
            canvas.set_color(Color.MediumBright)
            canvas.hline(x - 1, base_y + 2, bar_w + 2)

        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Low)
        tw = len(labels[i]) * 4
        canvas.draw_text(x + (bar_w - tw) // 2, 49, labels[i])

    # Info — y=17, bitmap 11..17
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    canvas.draw_text(8, SMALL_Y_MIN, "1/16  80/100")

    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(PAGE_W - 56, TINY_Y_MIN, "REST 0%")
    canvas.set_color(Color.Low)
    canvas.draw_rect(PAGE_W - 58, TINY_Y_MIN - 1, 54, 7)

    draw_footer(canvas, ["LiveR", "NewR", "REST", None, "NEXT"])


# ═══════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════

def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           'stochastic-audit-previews')
    os.makedirs(out_dir, exist_ok=True)
    print("Stochastic UI Audit — rendering current vs improved...")

    pages = [
        ("01-live-current",    render_live_current),
        ("02-live-improved",   render_live_improved),
        ("03-loop-current",    render_loop_current),
        ("04-loop-improved",   render_loop_improved),
        ("05-pitch-current",   render_pitch_current),
        ("06-pitch-improved",  render_pitch_improved),
        ("07-duration-current",  render_duration_current),
        ("08-duration-improved", render_duration_improved),
    ]

    for name, render_fn in pages:
        fb, canvas = blank_canvas()
        render_fn(canvas)
        save(fb, name, out_dir)

    # Composite: all 4 current in one image, all 4 improved in one image
    from PIL import Image

    def composite(names, out_name):
        imgs = []
        for n in names:
            p = os.path.join(out_dir, f"{n}.png")
            imgs.append(Image.open(p))
        w = max(im.width for im in imgs)
        total_h = sum(im.height for im in imgs) + 10 * (len(imgs) - 1)
        comp = Image.new("L", (w, total_h), 255)
        y = 0
        for im in imgs:
            comp.paste(im, (0, y))
            y += im.height + 10
        path = os.path.join(out_dir, f"{out_name}.png")
        comp.save(path)
        print(f"  {out_name:40s} -> {path}")

    composite([n for n, _ in pages[:8:2]], "00-all-current")
    composite([n for n, _ in pages[1:8:2]], "00-all-improved")

    print("Done.")


if __name__ == "__main__":
    main()
