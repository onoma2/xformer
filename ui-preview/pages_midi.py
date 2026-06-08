"""
MIDI page — proposed 12th routing tab, after BUS. Self-contained like the bus hub:
its own grid, left cursor bar, draft->COMMIT. Rows are dynamically populated — one
per route whose source is MIDI (grows as you learn, empty when none). Each row shows
the legacy MidiSource fields: target it drives | Port | Channel | Event | CC/Note.
"""
from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

# column geometry
TGT_X = 6            # target (what the route drives)
DIV = 76            # divider: target | midi-source fields
PORT_X = 82
CH_X = 110
EVT_X = 132
NUM_X = 170


def _clear(canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()


def _footer(canvas, names, highlight=-1):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    for i in range(1, 5):
        canvas.vline((PW * i) // 5, PH - 10, 10)
    canvas.set_font(Font.Tiny)
    for i, name in enumerate(names):
        if not name:
            continue
        x0 = (PW * i) // 5
        x1 = (PW * (i + 1)) // 5
        if i == highlight:
            canvas.set_color(Color.Medium)
            canvas.fill_rect(x0 + 1, PH - 10, (x1 - x0) - 1, 9)
            canvas.set_color(Color.None_)
        else:
            canvas.set_color(Color.Bright)
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)


def _header(canvas):
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(3, 7, "MIDI")
    canvas.set_color(Color.Low)
    canvas.draw_text(PORT_X, 7, "PORT")
    canvas.draw_text(CH_X, 7, "CH")
    canvas.draw_text(EVT_X, 7, "EVENT")
    canvas.draw_text(NUM_X, 7, "CC/NOTE")
    canvas.hline(0, 10, PW)
    canvas.vline(DIV, 11, PH - 22)


# (target, port, channel, event, num) — event in {CCa,CCr,PB,Nt}; num "" for PB
_ROUTES = [
    ("TRANSP\x7f1", "DIN", "1", "CCa", "74"),
    ("TEMPO", "USB", "2", "CCr", "16"),
    ("OCTAVE\x7f3", "DIN", "1", "PB", ""),
    ("SLIDE\x7f5", "DIN", "10", "Nt", "C3"),
]


def render_midi(canvas):
    _clear(canvas)
    _header(canvas)
    top, rh = 14, 10
    focus_row = 0
    for i, (tgt, port, ch, evt, num) in enumerate(_ROUTES):
        y = top + i * rh
        focus = (i == focus_row)
        if focus:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(0, y - 1, 2, rh - 1)
        canvas.set_color(Color.Bright if focus else Color.Medium)
        canvas.draw_text(TGT_X, y + 4, tgt)
        canvas.draw_text(PORT_X, y + 4, port)
        canvas.draw_text(CH_X, y + 4, ch)
        canvas.draw_text(EVT_X, y + 4, evt)
        if num:
            canvas.draw_text(NUM_X, y + 4, num)
        else:
            canvas.set_color(Color.Low)
            canvas.draw_text(NUM_X, y + 4, "-")
    _footer(canvas, ["VIEW", "EDIT", "", "", ""])


def _render_edit(canvas, edit_row, col):
    # col in {"port","ch","evt","num"} = the VIEW-advanced editable column (bright)
    _clear(canvas)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(3, 7, "MIDI")
    canvas.set_color(Color.Medium)
    tag = {"port": "PORT", "ch": "CHANNEL", "evt": "EVENT", "num": "CC/NOTE"}[col]
    canvas.draw_text(PORT_X, 7, tag)
    canvas.hline(0, 10, PW)
    canvas.vline(DIV, 11, PH - 22)
    top, rh = 14, 10
    cols = ["port", "ch", "evt", "num"]
    xs = {"port": PORT_X, "ch": CH_X, "evt": EVT_X, "num": NUM_X}
    for i, (tgt, port, ch, evt, num) in enumerate(_ROUTES):
        y = top + i * rh
        editing = (i == edit_row)
        if editing:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(0, y - 1, 2, rh - 1)
        canvas.set_color(Color.Bright if editing else Color.Medium)
        canvas.draw_text(TGT_X, y + 4, tgt)
        vals = {"port": port, "ch": ch, "evt": evt, "num": num or "-"}
        for c in cols:
            hot = editing and c == col
            canvas.set_color(Color.Bright if hot else (Color.Medium if editing else Color.Low))
            canvas.draw_text(xs[c], y + 4, vals[c])
    _footer(canvas, ["VIEW", "EDIT", "LEARN", "CANCEL", "COMMIT"])


def render_midi_edit(canvas):
    _render_edit(canvas, 0, "evt")


def render_midi_learn(canvas):
    # armed: capture popup over the edited row; next matching event fills the draft
    _render_edit(canvas, 0, "evt")
    bw, bh = 120, 16
    bx, by = (PW - bw) // 2, (PH - 11 - bh) // 2
    canvas.set_color(Color.None_)
    canvas.fill_rect(bx, by, bw, bh)
    canvas.set_color(Color.Bright)
    canvas.draw_rect(bx, by, bw, bh)
    m1 = "waiting for MIDI\x85"
    canvas.draw_text(bx + (bw - canvas.text_width(m1)) // 2, by + 11, m1)


def render_midi_empty(canvas):
    _clear(canvas)
    _header(canvas)
    canvas.set_color(Color.Low)
    msg = "no MIDI assignments - learn from the matrix"
    canvas.draw_text((PW - canvas.text_width(msg)) // 2, 34, msg)
    _footer(canvas, ["VIEW", "EDIT", "", "", ""])
