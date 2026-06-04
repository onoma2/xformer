"""
Proposed routing UI — per-track routing hero (Stochastic-hero + PhaseFlux-topic synthesis).

You are on a track; the page shows THAT track's routable params, segmented into topic
chips (the registry's tier bands). Each row: param name | source | depth | spread/link mark.
Track-first by construction — a route armed here targets only this track.
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

# topic chips for a Note track (the registry tier bands)
TOPICS = ["PITCH", "STEP", "PROB", "TRANSP"]

# per-topic param rows: (name, source or None, depth, linked-tracks-or-None)
TOPIC_ROWS = {
    "PITCH": [
        ("Transpose", "CV1",  "+50%", None),
        ("Octave",    None,   "",     None),
        ("Scale",     "M.C3", "+5",   "T3 T5"),   # unified: linked across tracks
        ("Root Note", None,   "",     None),
    ],
    "STEP": [
        ("First Step", None,  "",     None),
        ("Last Step",  "CV2", "+8",   None),
        ("Run Mode",   None,  "",     None),
        ("Divisor",    None,  "",     None),
        ("Clock Mult", "LFO", "+20%", None),
    ],
}

X_NAME = 4
X_SRC = 96
X_ARROW = 140
X_DEPTH_R = 196
X_MARK = 206
ROW_H = 6
TOP_Y = 25


def _header(canvas: Canvas, track: int, mode: str, topic: str):
    # StochasticSequenceEditPage idiom: clock/track left, topic name in the mode slot.
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "A 120.0")
    canvas.draw_text(54, 7, f"T{track}")
    canvas.draw_text(78, 7, mode)
    # topic name in the right mode slot (like "LIVE"/"LOOP")
    label = f"ROUTE {topic}"
    canvas.draw_text(PW - canvas.text_width(label) - 2, 7, label)
    canvas.set_color(Color.Medium)
    canvas.hline(0, 10, PW)


def _chips(canvas: Canvas, topics, selected: int):
    # borderless tab strip (Stochastic chip idiom): active bright, rest dim.
    canvas.set_font(Font.Tiny)
    x = 3
    for i, t in enumerate(topics):
        canvas.set_color(Color.Bright if i == selected else Color.MediumLow)
        canvas.draw_text(x, 19, t)
        if i == selected:
            canvas.hline(x, 21, canvas.text_width(t))
        x += canvas.text_width(t) + 8


def _footer(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["SRC", "LEARN", "SPREAD", "LINK", "EXIT"]
    for i in range(1, 5):
        canvas.vline((PW * i) // 5, PH - 10, 10)
    canvas.set_font(Font.Tiny)
    for i, name in enumerate(names):
        x0 = (PW * i) // 5
        x1 = (PW * (i + 1)) // 5
        canvas.set_color(Color.Medium)
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)


def render_routing_hero(canvas: Canvas, track: int = 3, mode: str = "NOTE",
                        topic_idx: int = 0, selected_row: int = 0):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()

    topic = TOPICS[topic_idx]
    _header(canvas, track, mode, topic)
    _chips(canvas, TOPICS, topic_idx)
    _footer(canvas)

    rows = TOPIC_ROWS.get(topic, [])
    canvas.set_font(Font.Tiny)
    for i, (name, src, depth, link) in enumerate(rows):
        y = TOP_Y + i * ROW_H
        rowsel = (i == selected_row)
        routed = src is not None

        if rowsel:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(0, y - 1, PW, ROW_H)
            canvas.set_blend_mode(BlendMode.Sub)

        # param name — bright if routed, dim if not
        canvas.set_color(Color.Bright if routed else Color.MediumLow)
        canvas.draw_text(X_NAME, y + 5, name)

        if routed:
            canvas.set_color(Color.Medium)
            canvas.draw_text(X_SRC, y + 5, src)
            canvas.draw_text(X_ARROW, y + 5, ">")
            canvas.set_color(Color.Bright)
            canvas.draw_text(X_DEPTH_R - canvas.text_width(depth), y + 5, depth)
            # link/spread mark
            if link:
                canvas.set_color(Color.MediumBright)
                canvas.draw_text(X_MARK, y + 5, link)   # unified: shows linked tracks
        else:
            canvas.set_color(Color.MediumLow)
            canvas.draw_text(X_SRC, y + 5, "--")

        if rowsel:
            canvas.set_blend_mode(BlendMode.Set)
