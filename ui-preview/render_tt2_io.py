#!/usr/bin/env python3
"""
TT2 I/O config page modeled on RoutingPage::drawTabEditor — WindowPainter-style
header (title + centered view tag + hline), a name-gutter matrix with column
headers + scrollbar, and the 5-cell function footer. Two views toggled by F1/F2:
  OUTPUTS = 8 CV outs x {RANGE, QUANT, OFFSET, ROOT}
  INPUTS  = trigger + CV inputs -> source
Output: ui-preview/tt2-io/<variant>.png
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from canvas import FrameBuffer, Canvas, Font, Color, BlendMode, framebuffer_to_image

PW, PH = 256, 64


def _clear(c):
    c.set_blend_mode(BlendMode.Set); c.set_color(Color.None_); c.fill()


def _footer(c, names, highlight=-1):
    c.set_blend_mode(BlendMode.Set); c.set_color(Color.Medium)
    c.hline(0, PH - 11, PW)
    for i in range(1, 5):
        c.vline((PW * i) // 5, PH - 10, 10)
    c.set_font(Font.Tiny)
    for i, name in enumerate(names):
        if not name:
            continue
        x0, x1 = (PW * i) // 5, (PW * (i + 1)) // 5
        tx = x0 + (x1 - x0 - c.text_width(name)) // 2
        if i == highlight:
            c.set_color(Color.Medium); c.fill_rect(x0 + 1, PH - 10, (x1 - x0) - 1, 9)
            c.set_blend_mode(BlendMode.Sub); c.set_color(Color.Bright)
            c.draw_text(tx, PH - 3, name)
            c.set_blend_mode(BlendMode.Set)
        else:
            c.set_color(Color.Bright)
            c.draw_text(tx, PH - 3, name)


def _header(c, view):
    c.set_font(Font.Tiny)
    c.set_color(Color.Bright); c.draw_text(2, 7, "TT2 I/O")
    tag = view
    c.set_color(Color.Medium); c.draw_text((PW - c.text_width(tag)) // 2, 7, tag)
    c.set_color(Color.Low); c.hline(0, 10, PW)


def _matrix(c, view, gutter, col_hdrs, cells, cur_row, cur_col, footer, scroll=0):
    _clear(c)
    _header(c, view)
    nameW = 30
    gridL = nameW + 2
    barW = 4 if len(gutter) > 5 else 0
    colW = (PW - gridL - barW) // len(col_hdrs)
    # column headers
    c.set_color(Color.Medium)
    for ci, h in enumerate(col_hdrs):
        cx = gridL + ci * colW + colW // 2
        c.draw_text(cx - c.text_width(h) // 2, 17, h)
    top, rowH, visible = 20, 7, 5
    n = len(gutter)
    if cur_row < scroll: scroll = cur_row
    elif cur_row >= scroll + visible: scroll = cur_row - visible + 1
    scroll = max(0, min(scroll, max(0, n - visible)))
    for vi in range(visible):
        r = scroll + vi
        if r >= n: break
        y = top + vi * rowH
        sel_row = (r == cur_row)
        c.set_color(Color.Bright if sel_row else Color.Medium)
        c.draw_text(2, y + 5, gutter[r])
        for ci in range(len(col_hdrs)):
            cx = gridL + ci * colW + colW // 2
            txt = cells[r][ci]
            isc = sel_row and ci == cur_col
            if isc:
                c.set_color(Color.MediumBright); c.fill_rect(cx - colW // 2 + 1, y, colW - 1, rowH)
                c.set_blend_mode(BlendMode.Sub)
            c.set_color(Color.Bright if isc else (Color.Medium if txt not in ("-", "") else Color.MediumLow))
            c.draw_text(cx - c.text_width(txt) // 2, y + 5, txt)
            if isc:
                c.set_blend_mode(BlendMode.Set)
    if n > visible:
        ty, th = 19, top + visible * rowH - 19
        c.set_color(Color.Low); c.vline(PW - 2, ty, th)
        c.set_color(Color.Bright); c.fill_rect(PW - 3, ty + th * scroll // n, 3, max(4, th * visible // n))
    # footer: the 4 param columns + NEXT; active column highlighted (F-key selects it)
    _footer(c, footer, highlight=cur_col)


def _inputs_grouped(c, trig, cvin_c1, cvin_c2, cur, midi):
    # TRIG (T1-4) + CV-IN 2-up (col1 IN/PARAM/T, col2 X/Y/Z) + MIDI column.
    # cur = ("T", i) | ("C1", i) | ("C2", i) | ("M", 0)
    _clear(c)
    c.set_font(Font.Tiny)
    c.set_color(Color.Bright); c.draw_text(2, 7, "TT2 I/O")
    c.set_color(Color.Medium); c.draw_text((PW - c.text_width("IN+MIDI")) // 2, 7, "IN+MIDI")
    c.set_color(Color.Low); c.hline(0, 10, PW)
    top, rowH = 15, 7

    def cell(nx, vx, vw, nm, v, sel):
        c.set_color(Color.Low); c.draw_text(nx, cellY + 5, nm)
        if sel:
            c.set_color(Color.MediumBright); c.fill_rect(vx - 1, cellY, vw, rowH)
            c.set_blend_mode(BlendMode.Sub)
        c.set_color(Color.Bright if sel else (Color.Medium if v != "-" else Color.MediumLow))
        c.draw_text(vx, cellY + 5, v)
        if sel:
            c.set_blend_mode(BlendMode.Set)

    # column titles
    c.set_color(Color.Low)
    c.draw_text(2, top + 4, "TRIG")
    c.draw_text(66, top + 4, "CV IN")
    c.draw_text(150, top + 4, "MIDI")

    # TRIG block: 2-up (T1-4 | T5-8) -> 8 trigger inputs
    for i in range(8):
        col, row = i // 4, i % 4
        cellY = top + 8 + row * rowH
        nx = 2 + col * 30
        cell(nx, nx + 13, 16, "T%d" % (i + 1), trig[i], cur == ("T", i))
    # CV IN: 2 columns (IN/PRM/T | X/Y/Z)
    c1, c2 = ["IN", "PRM", "T"], ["X", "Y", "Z"]
    for i in range(3):
        cellY = top + 8 + i * rowH
        cell(66, 92, 20, c1[i], cvin_c1[i], cur == ("C1", i))
        cell(116, 126, 18, c2[i], cvin_c2[i], cur == ("C2", i))
    # MIDI column (single source value)
    cellY = top + 8
    cell(150, 150, 32, "", midi, cur == ("M", 0))
    # footer: input column groups + NEXT; active group highlighted (F-key selects it)
    grp = {"T": 0, "C1": 1, "C2": 1, "M": 2}.get(cur[0], 1)
    _footer(c, ["TRIG", "CV IN", "MIDI", "", "NEXT"], highlight=grp)


def _inputs_spread(c, trig, cvin_c1, cvin_c2, cur, midi):
    # Same content as _inputs_grouped but distributed across the full 256px so
    # nothing is cramped and the right gutter isn't black: TRIG left, CV IN
    # centre, MIDI far right.
    _clear(c)
    c.set_font(Font.Tiny)
    c.set_color(Color.Bright); c.draw_text(2, 7, "TT2 I/O")
    c.set_color(Color.Medium); c.draw_text((PW - c.text_width("IN+MIDI")) // 2, 7, "IN+MIDI")
    c.set_color(Color.Low); c.hline(0, 10, PW)
    top, rowH = 15, 7

    def cell(nx, vx, vw, nm, v, sel):
        c.set_color(Color.Low); c.draw_text(nx, cellY + 5, nm)
        if sel:
            c.set_color(Color.MediumBright); c.fill_rect(vx - 1, cellY, vw, rowH)
            c.set_blend_mode(BlendMode.Sub)
        c.set_color(Color.Bright if sel else (Color.Medium if v != "--" else Color.MediumLow))
        c.draw_text(vx, cellY + 5, v)
        if sel:
            c.set_blend_mode(BlendMode.Set)

    c.set_color(Color.Low)
    c.draw_text(2, top + 4, "TRIG")
    c.draw_text(98, top + 4, "CV IN")
    c.draw_text(208, top + 4, "MIDI")

    for i in range(8):
        col, row = i // 4, i % 4
        cellY = top + 8 + row * rowH
        nx = 2 + col * 42
        cell(nx, nx + 16, 18, "T%d" % (i + 1), trig[i], cur == ("T", i))
    c1, c2 = ["IN", "PRM", "T"], ["X", "Y", "Z"]
    for i in range(3):
        cellY = top + 8 + i * rowH
        cell(98, 118, 22, c1[i], cvin_c1[i], cur == ("C1", i))
        cell(152, 170, 22, c2[i], cvin_c2[i], cur == ("C2", i))
    cellY = top + 8
    cell(208, 208, 44, "", midi, cur == ("M", 0))
    grp = {"T": 0, "C1": 1, "C2": 1, "M": 2}.get(cur[0], 1)
    _footer(c, ["TRIG", "CV IN", "MIDI", "", "NEXT"], highlight=grp)


def main():
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tt2-io")
    os.makedirs(out, exist_ok=True)

    # OUTPUTS view — 8 CV outs x 4 shaping attrs; footer = the 4 columns + NEXT.
    # cur_col = active column (F1-F4); cur_row = encoder-selected row.
    foot = ["RNG", "QNT", "OFF", "ROOT", "NEXT"]
    gut = ["CV%d" % (i + 1) for i in range(8)]
    cells = [
        ["5B", "-", "0", "C"], ["5B", "Maj", "0", "C"], ["5U", "-", "+.12", "C"],
        ["2B", "Min", "0", "G"], ["5B", "-", "-.05", "C"], ["1U", "-", "0", "C"],
        ["5B", "Chr", "0", "A"], ["5B", "-", "0", "C"],
    ]
    fb = FrameBuffer(PW, PH); c = Canvas(fb)
    _matrix(c, "OUTPUTS", gut, ["RNG", "QNT", "OFF", "ROOT"], cells, 2, 0, foot)
    framebuffer_to_image(fb, 4).save(os.path.join(out, "tt2-io-out.png")); print("out")

    # INPUTS view — TRIG + CV-IN 2-up + MIDI column; header IN+MIDI
    fb = FrameBuffer(PW, PH); c = Canvas(fb)
    _inputs_grouped(c, ["I1", "I2", "I3", "I4", "G1", "G2", "L1", "-"],
                    cvin_c1=["I1", "I2", "-"], cvin_c2=["I3", "I4", "-"],
                    cur=("C1", 0), midi="OMNI")
    framebuffer_to_image(fb, 4).save(os.path.join(out, "tt2-io-in.png")); print("in")

    # PROPOSED: 2-letter codes (IN/CV/GT/RT/LC/LG, None=--), stress-testing the
    # longest 3-char codes in every value box.
    fb = FrameBuffer(PW, PH); c = Canvas(fb)
    _inputs_grouped(c, ["IN1", "IN2", "IN3", "IN4", "GT1", "GT8", "LG4", "LC8"],
                    cvin_c1=["IN1", "CV8", "RT4"], cvin_c2=["LC8", "--", "RT1"],
                    cur=("C1", 0), midi="OMNI")
    framebuffer_to_image(fb, 4).save(os.path.join(out, "tt2-io-in-proposed.png")); print("in-proposed")

    fb = FrameBuffer(PW, PH); c = Canvas(fb)
    _inputs_spread(c, ["IN1", "IN2", "IN3", "IN4", "GT1", "GT8", "LG4", "LC8"],
                   cvin_c1=["IN1", "CV8", "RT4"], cvin_c2=["LC8", "--", "RT1"],
                   cur=("C1", 0), midi="OMNI")
    framebuffer_to_image(fb, 4).save(os.path.join(out, "tt2-io-in-spread.png")); print("in-spread")

    fb = FrameBuffer(PW, PH); c = Canvas(fb)
    _matrix(c, "OUTPUTS", gut, ["RNG", "QNT", "OFF", "ROOT"], cells, 2, 0, foot)
    framebuffer_to_image(fb, 4).save(os.path.join(out, "tt2-io-out-proposed.png")); print("out-proposed")


if __name__ == "__main__":
    main()
