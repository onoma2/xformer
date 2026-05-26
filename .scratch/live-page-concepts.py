"""Three poetic concept previews for the Stochastic LIVE hero page redesign.
All preserve header (0..10) and footer (53..63). Content area: y=11..52."""

import random, math
from PIL import Image, ImageDraw

W, H = 128, 64
random.seed(42)

# ============================================================
# Shared helpers
# ============================================================

def draw_header(draw, title):
    draw.rectangle([0, 0, W-1, 10], fill=0)
    draw.text((2, 1), title, fill=255)
    draw.line([(0, 10), (W-1, 10)], fill=60)

def draw_footer(draw, labels=["LoopR", "LoopM", "NewR", "NewM", "NEXT"]):
    draw.rectangle([0, 53, W-1, 63], fill=0)
    draw.line([(0, 53), (W-1, 53)], fill=60)
    col_step = W // len(labels)
    for i, lbl in enumerate(labels):
        x = col_step * i + 4
        draw.text((x, 55), lbl, fill=180)
        if i < len(labels) - 1:
            sep_x = col_step * (i + 1)
            draw.line([(sep_x, 55), (sep_x, 61)], fill=40)

def make_engine_history(n=12):
    """Simulate engine history: (cv, rest, children) tuples"""
    events = []
    cv = 0.0
    for i in range(n):
        rest = random.random() < 0.2
        children = random.randint(0, 3) if random.random() < 0.3 else 0
        if not rest:
            cv += random.uniform(-0.3, 0.3)
            cv = max(-4.0, min(4.0, cv))
        events.append((cv, rest, children))
    return events

# ============================================================
# Concept A — Constellation
# ============================================================

def draw_constellation(draw, events, params):
    """Star-field trail: events appear at their pitch position, fade + drift up.
    Params become a single compact instrument strip at y=12-13."""
    vp_top, vp_bot = 14, 52
    vp_cy = (vp_top + vp_bot) // 2
    vp_h = vp_bot - vp_top
    bias_y = vp_cy - params.get("bias", 50) // 20
    spread = max(2, params.get("spread", 50) // 3)
    
    # Nebula glow — soft haze behind stars
    for y in range(vp_top, vp_bot + 1):
        dy = y - bias_y
        env = max(0, 255 - (dy * dy * 3) // (spread * spread))
        if env < 20:
            continue
        level = env // 8
        for x in range(6, W - 6, 4):
            if (x + y * 3) % 13 < 2:
                continue
            draw.point((x, y), fill=level)

    # Constellation lines — faint octave rails
    for oct_off in range(-2, 3):
        y = vp_cy + oct_off * 10
        if vp_top <= y <= vp_bot:
            draw.line([(6, y), (W - 6, y)], fill=15)

    # Stars from events
    n = len(events)
    for i, (cv, rest, children) in enumerate(events):
        age = n - 1 - i  # 0 = newest
        offset = int(-cv * 4.0)
        py = bias_y + offset
        py = max(vp_top + 1, min(vp_bot - 1, py))
        px = W - 8 - age * 8
        if px < 8:
            px = 8 + (age % 4) * 3  # cluster old stars
        
        fade = max(20, 255 - age * 18)
        
        if rest:
            # Rest = faint ring
            draw.ellipse([px-2, py-2, px+2, py+2], outline=fade)
        else:
            # Active star = filled circle + cross
            r = 3 if age == 0 else 2
            draw.ellipse([px-r, py-r, px+r, py+r], fill=fade)
            if age == 0:
                # Diamond crosshair on newest
                draw.line([(px - 5, py), (px + 5, py)], fill=255)
                draw.line([(px, py - 5), (px, py + 5)], fill=255)

            # Burst children as faint companion dots
            if children > 0:
                for c in range(children):
                    cx = px + (c + 1) * 4
                    cy = py + random.randint(-2, 2)
                    if 8 <= cx <= W - 8:
                        draw.point((cx, cy), fill=fade // 2)
                        draw.point((cx, cy + 1), fill=fade // 2)

        # Slide connector between consecutive non-rest events
        if not rest and i < n - 1:
            ncv, nrest, _ = events[i + 1]
            if not nrest:
                noff = int(-ncv * 4.0)
                npy = bias_y + noff
                npy = max(vp_top + 1, min(vp_bot - 1, npy))
                npx = px - 8
                draw.line([(px, py), (npx, npy)], fill=fade // 4)

    # Instrument strip — 8 critical params as tiny labels at y=11-12
    strip_x = 6
    labels = [
        f"D{params.get('duration',3)}", f"V{params.get('variation',0)}",
        f"R{params.get('rest',0)}", f"B{params.get('burst',0)}",
        f"C{params.get('complexity',50)}", f"S{params.get('spread',50)}",
        f"I{params.get('bias',50)}", f"E{params.get('repeat',0)}"
    ]
    for lbl in labels:
        draw.text((strip_x, 11), lbl, fill=140)
        strip_x += 15

def make_constellation():
    img = Image.new("1", (W, H), 0)
    draw = ImageDraw.Draw(img)
    draw_header(draw, "LIVE")
    draw_footer(draw)
    events = make_engine_history()
    params = {"bias": 50, "spread": 50, "duration": 3, "variation": 16,
              "rest": 0, "burst": 0, "complexity": 50, "repeat": 0}
    draw_constellation(draw, events, params)
    return img

# ============================================================
# Concept B — Rain / Ink Drop
# ============================================================

def draw_rain(draw, events, params):
    """Musical rain: events drop as ink dots from the top, at speeds set by pitch.
    Burst = splashes, Slide = wisps, Rests = empty drops."""
    vp_top, vp_bot = 14, 52
    vp_h = vp_bot - vp_top
    bias_y = vp_top + vp_h // 2
    
    # Mist band — the bell haze as horizontal fog
    spread = max(3, params.get("spread", 50) // 2)
    for y in range(vp_top, vp_bot + 1):
        dy = (y - bias_y)
        env = max(0, 255 - (dy * dy * 5) // (spread * spread))
        if env < 10:
            continue
        level = env // 10
        for x in range(6, W - 6):
            if (x * 7 + y * 13) % 17 < 2:
                draw.point((x, y), fill=level)

    # Pitch rails — faint
    for r in range(-2, 3):
        ry = bias_y + r * 8
        if vp_top <= ry <= vp_bot:
            draw.line([(4, ry), (W - 4, ry)], fill=12)

    # Rain drops: each event spawns a column at horizontal position
    n = len(events)
    for i, (cv, rest, children) in enumerate(events):
        age = n - 1 - i
        col_x = 8 + i * 9
        if col_x > W - 8:
            col_x = W - 8 - (i - (W - 16) // 9) * 9
        
        # Drop falls from top; its vertical span shows pitch
        offset = int(-cv * 3.5)
        drop_top = bias_y + offset - 6
        drop_bot = bias_y + offset + 6
        drop_top = max(vp_top + 1, drop_top)
        drop_bot = min(vp_bot - 1, drop_bot)
        
        fade = max(15, 200 - age * 15)
        
        if rest:
            # Empty circle
            cy = (drop_top + drop_bot) // 2
            draw.ellipse([col_x-2, cy-2, col_x+2, cy+2], outline=fade)
        else:
            # Ink drop — vertical streak + dot at bottom
            streak_top = drop_top + 2
            streak_bot = drop_bot - 1
            drop_y = drop_bot
            
            # Fading streak
            for sy in range(streak_top, streak_bot):
                if sy < vp_top or sy > vp_bot:
                    continue
                alpha = max(10, fade - (streak_bot - sy) * 8)
                draw.point((col_x, sy), fill=alpha)
            
            # Impact dot
            r = 2 if age == 0 else 1
            draw.ellipse([col_x-r, drop_y-r, col_x+r, drop_y+r], fill=fade)
            
            # Splash (burst children) — tiny dots radiating
            if children > 0:
                for c in range(children):
                    angle = (c + 1) * math.pi / (children + 1)
                    sx = int(col_x + math.cos(angle) * 5)
                    sy = int(drop_y - math.sin(angle) * 4)
                    if vp_top <= sy <= vp_bot and 4 <= sx <= W - 4:
                        draw.point((sx, sy), fill=fade // 2)
            
            # Wisp between consecutive non-rests
            if not rest and i < n - 1:
                ncv, nrest, _ = events[i + 1]
                if not nrest:
                    noff = int(-ncv * 3.5)
                    n_bot = bias_y + noff + 6
                    n_bot = min(vp_bot - 1, n_bot)
                    nx = col_x + 9
                    if nx <= W - 8:
                        for t in range(5):
                            tx = col_x + (nx - col_x) * t // 5
                            ty = drop_bot + (n_bot - drop_bot) * t // 5
                            if vp_top <= ty <= vp_bot:
                                draw.point((tx, ty), fill=fade // 5)

    # Weather report strip at y=11-12
    strip_x = 6
    labels = [
        f"D{params.get('duration',3)}", f"V{params.get('variation',16)}",
        f"R{params.get('rest',0)}", f"B{params.get('burst',0)}",
        f"X{params.get('complexity',50)}", f"S{params.get('spread',50)}",
        f"I{params.get('bias',50)}", f"E{params.get('repeat',0)}"
    ]
    for lbl in labels:
        draw.text((strip_x, 11), lbl, fill=140)
        strip_x += 15

def make_rain():
    img = Image.new("1", (W, H), 0)
    draw = ImageDraw.Draw(img)
    draw_header(draw, "LIVE")
    draw_footer(draw)
    events = make_engine_history()
    params = {"bias": 50, "spread": 50, "duration": 3, "variation": 16,
              "rest": 0, "burst": 0, "complexity": 50, "repeat": 0}
    draw_rain(draw, events, params)
    return img

# ============================================================
# Concept C — Sonar / Pulse Rings
# ============================================================

def draw_sonar(draw, events, params):
    """Concentric rings radiate from each event at its pitch height.
    Ring radius = event age. Burst = nested rings. Slide = arced connectors."""
    vp_top, vp_bot = 14, 52
    vp_h = vp_bot - vp_top
    
    bias_y = vp_top + vp_h // 2
    spread = max(3, params.get("spread", 50) // 3)
    
    # Center glow
    for y in range(vp_top, vp_bot + 1):
        dy = y - bias_y
        env = max(0, 255 - (dy * dy * 5) // (spread * spread))
        if env < 15:
            continue
        level = env // 12
        cx = W // 2
        for x in range(cx - spread, cx + spread + 1):
            if (x * 7 + y * 11) % 15 < 2:
                draw.point((x, y), fill=level)

    # Pitch rails
    for r in range(-2, 3):
        ry = bias_y + r * 8
        if vp_top <= ry <= vp_bot:
            draw.line([(4, ry), (W - 4, ry)], fill=12)

    # Sonar rings
    n = len(events)
    for i, (cv, rest, children) in enumerate(events):
        age = n - 1 - i
        offset = int(-cv * 4.0)
        ey = bias_y + offset
        ey = max(vp_top + 1, min(vp_bot - 1, ey))
        
        # Horizontal position: newest at right, oldest at left
        ex = W - 10 - age * 8
        if ex < 10:
            continue
        
        fade = max(15, 200 - age * 14)
        
        if rest:
            # Faint ring only (no center)
            r_ring = 3
            draw.ellipse([ex-r_ring, ey-r_ring, ex+r_ring, ey+r_ring], 
                        outline=fade // 2)
        else:
            # Center dot
            r_dot = 2 if age == 0 else 1
            draw.ellipse([ex-r_dot, ey-r_dot, ex+r_dot, ey+r_dot], fill=fade)
            
            # Expanding ring (radius proportional to gate-length via age)
            r_ring = 2 + age
            if ex - r_ring >= 2 and ex + r_ring <= W - 2:
                draw.ellipse([ex-r_ring, ey-r_ring, ex+r_ring, ey+r_ring],
                           outline=fade // 3)
            
            # Secondary ring (double pulse)
            if age < 3:
                r2 = r_ring - 1
                draw.ellipse([ex-r2, ey-r2, ex+r2, ey+r2], outline=fade // 2)
            
            # Burst children: nested smaller rings
            if children > 0:
                for c in range(children):
                    cr = r_ring - 1 - c
                    if cr < 1:
                        continue
                    draw.ellipse([ex-cr, ey-cr, ex+cr, ey+cr],
                               outline=fade // 4)

            # Arced connector to next event
            if not rest and i < n - 1:
                ncv, nrest, _ = events[i + 1]
                if not nrest:
                    noff = int(-ncv * 4.0)
                    ney = bias_y + noff
                    ney = max(vp_top + 1, min(vp_bot - 1, ney))
                    nex = ex - 8
                    if nex >= 10:
                        # Arc connecting centers
                        mid_y = (ey + ney) // 2
                        for t in range(6):
                            px = ex - 8 * t // 5
                            py = ey + (ney - ey) * t // 5
                            draw.point((px, py), fill=fade // 5)

    # Frequency bars along right edge (param analyzer strip)
    bar_params = [
        ("D", params.get("duration", 3), 7),
        ("V", params.get("variation", 16) + 100, 200),
        ("R", params.get("rest", 0), 100),
        ("B", params.get("burst", 0), 100),
        ("X", params.get("complexity", 50), 100),
        ("S", params.get("spread", 50), 100),
        ("I", params.get("bias", 50), 100),
        ("E", params.get("repeat", 0), 100),
    ]
    bar_left = W - 22
    for j, (label, val, vmax) in enumerate(bar_params):
        bx = bar_left + j * 2
        bar_h = (val * 6) // vmax
        bar_y_base = vp_bot - 2
        for bh in range(bar_h):
            by = bar_y_base - bh
            if by >= vp_top:
                draw.point((bx, by), fill=120)
        # tiny label above bar
        if bar_y_base - bar_h - 5 >= vp_top:
            draw.text((bx - 1, bar_y_base - bar_h - 5), label, fill=60)

    # Mini readout strip at y=11-12
    strip_x = 6
    labels = [
        f"D{params.get('duration',3)}", f"V{params.get('variation',16)}",
        f"R{params.get('rest',0)}", f"B{params.get('burst',0)}",
        f"C{params.get('complexity',50)}", f"S{params.get('spread',50)}",
        f"I{params.get('bias',50)}", f"E{params.get('repeat',0)}"
    ]
    for lbl in labels:
        draw.text((strip_x, 11), lbl, fill=140)
        strip_x += 15


def make_sonar():
    img = Image.new("1", (W, H), 0)
    draw = ImageDraw.Draw(img)
    draw_header(draw, "LIVE")
    draw_footer(draw)
    events = make_engine_history()
    params = {"bias": 50, "spread": 50, "duration": 3, "variation": 16,
              "rest": 0, "burst": 0, "complexity": 50, "repeat": 0}
    draw_sonar(draw, events, params)
    return img

# ============================================================
# Render all 3 and save
# ============================================================

if __name__ == "__main__":
    concepts = [
        ("A — Constellation", make_constellation()),
        ("B — Rain / Ink Drop", make_rain()),
        ("C — Sonar / Pulse Rings", make_sonar()),
    ]
    
    for name, img in concepts:
        path = f"/Users/foronoma/Work/Code/Eurorack/performer-phazer/.scratch/live-concept-{name[0].lower()}.png"
        img.save(path)
        # scale 4x for readability
        big = img.resize((W * 4, H * 4), Image.NEAREST)
        big.save(path.replace(".png", "-4x.png"))
        print(f"  {name} → {path}")

    print("\nDone. All 3 concepts rendered.")
