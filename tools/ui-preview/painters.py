"""
Replica of firmware painters used by edit pages.
"""

from canvas import BlendMode, Canvas, Color, Font

PageWidth = 256
PageHeight = 64
HeaderHeight = 10
FooterHeight = 10
FunctionKeyCount = 5


def _draw_inverted_text(canvas: Canvas, x: int, y: int, text: str, inverted: bool = True):
    canvas.set_font(Font.Tiny)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Bright)
    if inverted:
        canvas.fill_rect(x - 1, y - 5, canvas.text_width(text) + 1, 7)
        canvas.set_blend_mode(BlendMode.Sub)
    canvas.draw_text(x, y, text)


class WindowPainter:
    @staticmethod
    def clear(canvas: Canvas):
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.None_)
        canvas.fill()

    @staticmethod
    def draw_frame(canvas: Canvas, x: int, y: int, w: int, h: int):
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.None_)
        canvas.fill_rect(x, y, w, h)
        canvas.set_color(Color.Bright)
        canvas.draw_rect(x, y, w, h)

    @staticmethod
    def draw_header(canvas: Canvas, track: int = 0, play_pattern: int = 0,
                    edit_pattern: int = 0, snapshot_active: bool = False,
                    song_active: bool = False, mode: str = ""):
        # Clock placeholder
        canvas.set_font(Font.Tiny)
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.Bright)
        canvas.draw_text(2, 6, "A 120.0")
        # Track / pattern
        canvas.draw_text(40, 6, f"T{track + 1}")
        if snapshot_active:
            _draw_inverted_text(canvas, 56, 6, "SNAP", True)
        else:
            _draw_inverted_text(canvas, 56, 6, f"P{play_pattern + 1}", song_active)
            _draw_inverted_text(canvas, 75, 6, f"E{edit_pattern + 1}", play_pattern == edit_pattern)
        # Mode
        canvas.set_color(Color.Bright)
        canvas.draw_text(PageWidth - canvas.text_width(mode) - 2, 6, mode)
        # Separator
        canvas.set_color(Color.Medium)
        canvas.hline(0, HeaderHeight, PageWidth)

    @staticmethod
    def draw_active_function(canvas: Canvas, function: str):
        canvas.set_font(Font.Tiny)
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.Bright)
        canvas.draw_text(100, 6, function)

    @staticmethod
    def draw_accumulator_value(canvas: Canvas, value: int, enabled: bool):
        if not enabled:
            return
        canvas.set_font(Font.Tiny)
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.Medium)
        sign = "+" if value >= 0 else ""
        canvas.draw_text(176, 6, f"{sign}{value}")

    @staticmethod
    def draw_footer(canvas: Canvas, names=None, highlight=-1):
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.Medium)
        canvas.hline(0, PageHeight - FooterHeight - 1, PageWidth)
        if names is None:
            return
        for i in range(FunctionKeyCount):
            if i + 1 < FunctionKeyCount:
                x = (PageWidth * (i + 1)) // FunctionKeyCount
                canvas.vline(x, PageHeight - FooterHeight, FooterHeight)
        canvas.set_font(Font.Tiny)
        for i in range(FunctionKeyCount):
            name = names[i] if i < len(names) else None
            if name:
                canvas.set_color(Color.Bright if i == highlight else Color.Medium)
                x0 = (PageWidth * i) // FunctionKeyCount
                x1 = (PageWidth * (i + 1)) // FunctionKeyCount
                w = x1 - x0 + 1
                canvas.draw_text(x0 + (w - canvas.text_width(name)) // 2, PageHeight - 3, name)


class SequencePainter:
    @staticmethod
    def draw_loop_start(canvas: Canvas, x: int, y: int, w: int):
        canvas.vline(x, y - 1, 3)
        canvas.point(x + 1, y)

    @staticmethod
    def draw_loop_end(canvas: Canvas, x: int, y: int, w: int):
        x = x + w - 1
        canvas.vline(x, y - 1, 3)
        canvas.point(x - 1, y)

    @staticmethod
    def draw_probability(canvas: Canvas, x: int, y: int, w: int, h: int, probability: int, max_probability: int):
        canvas.set_blend_mode(BlendMode.Set)
        pw = (w * probability) // max_probability
        canvas.set_color(Color.Bright)
        canvas.fill_rect(x, y, pw, h)
        canvas.set_color(Color.Medium)
        canvas.fill_rect(x + pw, y, w - pw, h)

    @staticmethod
    def draw_length(canvas: Canvas, x: int, y: int, w: int, h: int, length: int, max_length: int):
        canvas.set_blend_mode(BlendMode.Set)
        gw = ((w - 1) * length) // max_length
        canvas.set_color(Color.Bright)
        canvas.vline(x, y, h)
        canvas.hline(x, y, gw)
        canvas.vline(x + gw, y, h)
        canvas.hline(x + gw, y + h - 1, w - gw)

    @staticmethod
    def draw_length_range(canvas: Canvas, x: int, y: int, w: int, h: int, length: int, range: int, max_length: int):
        canvas.set_blend_mode(BlendMode.Set)
        gw = ((w - 1) * length) // max_length
        rw = ((w - 1) * max(0, min(max_length, length + range))) // max_length
        canvas.set_color(Color.Medium)
        canvas.vline(x, y, h)
        canvas.hline(x, y, gw)
        canvas.vline(x + gw, y, h)
        canvas.hline(x + gw, y + h - 1, w - gw)
        canvas.set_color(Color.Bright)
        a = min(gw, rw)
        b = max(gw, rw)
        canvas.fill_rect(x + a, y + 2, b - a + 1, h - 4)

    @staticmethod
    def draw_offset(canvas: Canvas, x: int, y: int, w: int, h: int, offset: int, min_offset: int, max_offset: int):
        canvas.set_blend_mode(BlendMode.Set)
        def remap(value):
            return ((w - 1) * (value - min_offset)) // (max_offset - min_offset)
        canvas.set_color(Color.Medium)
        canvas.fill_rect(x, y, w, h)
        canvas.set_color(Color.None_)
        canvas.vline(x + remap(0), y, h)
        canvas.set_color(Color.Bright)
        canvas.vline(x + remap(offset), y, h)

    @staticmethod
    def draw_retrigger(canvas: Canvas, x: int, y: int, w: int, h: int, retrigger: int, max_retrigger: int):
        canvas.set_blend_mode(BlendMode.Set)
        bw = w // max_retrigger
        x += (w - bw * retrigger) // 2
        canvas.set_color(Color.Bright)
        for i in range(retrigger):
            canvas.fill_rect(x, y, bw // 2, h)
            x += bw

    @staticmethod
    def draw_slide(canvas: Canvas, x: int, y: int, w: int, h: int, active: bool):
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.Bright)
        if active:
            # diagonal line
            for i in range(w + 1):
                canvas.point(x + i, y + h - (i * h) // w)
        else:
            canvas.hline(x, y + h, w)

    @staticmethod
    def draw_sequence_progress(canvas: Canvas, x: int, y: int, w: int, h: int, progress: float):
        if progress < 0:
            return
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.Medium)
        canvas.fill_rect(x, y, w, h)
        canvas.set_color(Color.Bright)
        canvas.vline(x + int(progress * w), y, h)
