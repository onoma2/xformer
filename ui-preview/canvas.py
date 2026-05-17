"""
Python replica of the firmware Canvas/FrameBuffer for exact OLED preview rendering.

Parses the C++ bitmap font headers so font data stays in sync with the firmware.
"""

import os
import re
from dataclasses import dataclass
from enum import IntEnum
from typing import List


class BlendMode(IntEnum):
    Set = 0
    Add = 1
    Sub = 2


class Color(IntEnum):
    None_ = 0
    Low = 0x3
    MediumLow = 0x5
    Medium = 0x7
    MediumBright = 0xa
    Bright = 0xf


class Font(IntEnum):
    Tiny = 0
    Small = 1
    Tele = 2


@dataclass(frozen=True)
class BitmapFontGlyph:
    offset: int
    width: int
    height: int
    xAdvance: int
    xOffset: int
    yOffset: int


@dataclass(frozen=True)
class BitmapFont:
    bpp: int
    bitmap: bytes
    glyphs: List[BitmapFontGlyph]
    first: int
    last: int
    yAdvance: int


def _parse_font_header(filepath: str, font_name: str) -> BitmapFont:
    with open(filepath, 'r') as f:
        content = f.read()

    # Bitmap array: static uint8_t tiny5x5_bitmap[] = { 0x17, 0x2d, ... };
    bitmap_pat = re.compile(
        rf'static\s+uint8_t\s+{font_name}_bitmap\[\s*\]\s*=\s*\{{(.*?)\}};',
        re.DOTALL,
    )
    bitmap_m = bitmap_pat.search(content)
    if not bitmap_m:
        raise ValueError(f'Could not find bitmap array for {font_name} in {filepath}')
    bitmap_text = bitmap_m.group(1)
    bytes_list = [int(b, 16) for b in re.findall(r'0x[0-9a-fA-F]{2}', bitmap_text)]
    bitmap = bytes(bytes_list)

    # Glyph array: static BitmapFontGlyph tiny5x5_glyphs[] = { {0,0,0,3,0,1}, ... };
    glyphs_pat = re.compile(
        rf'static\s+BitmapFontGlyph\s+{font_name}_glyphs\[\s*\]\s*=\s*\{{(.*?)\}};',
        re.DOTALL,
    )
    glyphs_m = glyphs_pat.search(content)
    if not glyphs_m:
        raise ValueError(f'Could not find glyphs array for {font_name} in {filepath}')
    glyphs_text = glyphs_m.group(1)
    glyph_tuples = re.findall(
        r'\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}',
        glyphs_text,
    )
    glyphs = [BitmapFontGlyph(*map(int, g)) for g in glyph_tuples]

    # Font struct: static BitmapFont tiny5x5 = { 1, tiny5x5_bitmap, tiny5x5_glyphs, 32, 126, 8 };
    font_pat = re.compile(
        rf'static\s+BitmapFont\s+{font_name}\s*=\s*\{{\s*\d+\s*,\s*\w+\s*,\s*\w+\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}};',
        re.DOTALL,
    )
    font_m = font_pat.search(content)
    if not font_m:
        raise ValueError(f'Could not find font struct for {font_name} in {filepath}')
    first = int(font_m.group(1))
    last = int(font_m.group(2))
    yAdvance = int(font_m.group(3))
    # bpp is always the first field (usually 1 or 4)
    bpp_pat = re.compile(rf'static\s+BitmapFont\s+{font_name}\s*=\s*\{{\s*(\d+)\s*,')
    bpp_m = bpp_pat.search(content)
    bpp = int(bpp_m.group(1)) if bpp_m else 1

    return BitmapFont(bpp, bitmap, glyphs, first, last, yAdvance)


def _load_fonts() -> dict:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, '../..'))
    fonts_dir = os.path.join(project_root, 'src/core/gfx/fonts')

    fonts = {}
    for font_enum, font_name in [(Font.Tiny, 'tiny5x5')]:
        path = os.path.join(fonts_dir, f'{font_name}.h')
        fonts[font_enum] = _parse_font_header(path, font_name)
    return fonts


_FONTS = _load_fonts()


class FrameBuffer:
    def __init__(self, width: int, height: int):
        self.width = width
        self.height = height
        self.data = bytearray(width * height)


class Canvas:
    def __init__(self, framebuffer: FrameBuffer, brightness: float = 1.0):
        self._fb = framebuffer
        self._brightness = brightness
        self._right = framebuffer.width - 1
        self._bottom = framebuffer.height - 1
        self._color = 0xf
        self._blend_mode = BlendMode.Set
        self._font = Font.Tiny

    def set_color(self, color: Color):
        self._color = min(255, int(int(color) * self._brightness))

    def set_blend_mode(self, mode: BlendMode):
        self._blend_mode = mode

    def set_font(self, font: Font):
        self._font = font

    def color(self) -> int:
        return self._color

    def blend_mode(self) -> BlendMode:
        return self._blend_mode

    def font(self) -> Font:
        return self._font

    # ------------------------------------------------------------------
    # Internal blit
    # ------------------------------------------------------------------
    def _blit(self, x: int, y: int, col: int):
        if x < 0 or x > self._right or y < 0 or y > self._bottom:
            return
        idx = y * self._fb.width + x
        if self._blend_mode == BlendMode.Set:
            self._fb.data[idx] = col
        elif self._blend_mode == BlendMode.Add:
            # C++ does raw += on uint8_t, but display clamps to 15.
            # We saturate to match the visible result.
            self._fb.data[idx] = min(15, self._fb.data[idx] + col)
        elif self._blend_mode == BlendMode.Sub:
            self._fb.data[idx] = max(0, self._fb.data[idx] - min(self._fb.data[idx], col))

    # ------------------------------------------------------------------
    # Primitives
    # ------------------------------------------------------------------
    def fill(self):
        for i in range(len(self._fb.data)):
            self._fb.data[i] = self._color

    def point(self, x: int, y: int):
        self._blit(x, y, self._color)

    def hline(self, x: int, y: int, w: int):
        if y < 0 or y > self._bottom:
            return
        x0 = x
        x1 = x + w - 1
        if x0 < 0:
            x0 = 0
        if x1 > self._right:
            x1 = self._right
        for xi in range(x0, x1 + 1):
            self._blit(xi, y, self._color)

    def vline(self, x: int, y: int, h: int):
        if x < 0 or x > self._right:
            return
        y0 = y
        y1 = y + h - 1
        if y0 < 0:
            y0 = 0
        if y1 > self._bottom:
            y1 = self._bottom
        for yi in range(y0, y1 + 1):
            self._blit(x, yi, self._color)

    def draw_rect(self, x: int, y: int, w: int, h: int):
        self.hline(x, y, w)
        self.hline(x, y + h - 1, w)
        self.vline(x, y + 1, h - 2)
        self.vline(x + w - 1, y + 1, h - 2)

    def fill_rect(self, x: int, y: int, w: int, h: int):
        x0 = x
        x1 = x + w - 1
        y0 = y
        y1 = y + h - 1
        if x0 < 0:
            x0 = 0
        if x1 > self._right:
            x1 = self._right
        if y0 < 0:
            y0 = 0
        if y1 > self._bottom:
            y1 = self._bottom
        for yi in range(y0, y1 + 1):
            for xi in range(x0, x1 + 1):
                self._blit(xi, yi, self._color)

    # ------------------------------------------------------------------
    # Bitmap font text
    # ------------------------------------------------------------------
    def _draw_bitmap_1bit(self, x: int, y: int, w: int, h: int, bitmap: bytes, offset: int):
        byte_idx = offset
        shift = 0
        for dy in range(h):
            for dx in range(w):
                px = x + dx
                py = y + dy
                if px < 0 or px > self._right or py < 0 or py > self._bottom:
                    shift += 1
                    if shift >= 8:
                        shift = 0
                        byte_idx += 1
                    continue
                pixel = ((bitmap[byte_idx] >> shift) & 1) * self._color
                shift += 1
                if shift >= 8:
                    shift = 0
                    byte_idx += 1
                self._blit(px, py, pixel)

    def draw_text(self, x: int, y: int, text: str):
        font = _FONTS[self._font]
        ox = x
        for ch in text:
            if ch == '\n':
                x = ox
                y += font.yAdvance
                continue
            code = ord(ch)
            if code < font.first or code > font.last:
                continue
            g = font.glyphs[code - font.first]
            if g.width > 0 and g.height > 0:
                self._draw_bitmap_1bit(
                    x + g.xOffset, y + g.yOffset,
                    g.width, g.height,
                    font.bitmap, g.offset,
                )
            x += g.xAdvance

    def draw_text_centered(self, x: int, y: int, w: int, h: int, text: str):
        self.draw_text_aligned(x, y, w, h, 'center', 'center', text)

    def draw_text_aligned(self, x: int, y: int, w: int, h: int,
                          horizontal_align: str, vertical_align: str, text: str):
        if horizontal_align == 'center':
            x += (w - self.text_width(text) + 1) // 2
        elif horizontal_align == 'right':
            x += w - self.text_width(text)
        # left: no change

        if vertical_align == 'center':
            y += (h - self.text_height(text) + 1) // 2
        elif vertical_align == 'bottom':
            y += h - self.text_height(text)
        # top: no change

        # tiny5x5 font offset is 5 (matches C++ bitmapFontOffset)
        font = _FONTS[self._font]
        if self._font == Font.Tiny:
            y += 5
        elif self._font in (Font.Small, Font.Tele):
            y += 8

        self.draw_text(x, y, text)

    def text_width(self, text: str) -> int:
        font = _FONTS[self._font]
        width = 0
        for ch in text:
            code = ord(ch)
            if code < font.first or code > font.last:
                continue
            g = font.glyphs[code - font.first]
            width += g.xAdvance
        return width

    def text_height(self, text: str) -> int:
        font = _FONTS[self._font]
        height = 6 if self._font == Font.Tiny else (10 if self._font == Font.Small else 8)
        for ch in text:
            if ch == '\n':
                height += font.yAdvance
        return height


# ----------------------------------------------------------------------
# Export framebuffer to Pillow Image
# ----------------------------------------------------------------------
def framebuffer_to_image(fb: FrameBuffer, scale: int = 4):
    from PIL import Image
    img = Image.new('L', (fb.width, fb.height))
    pixels = img.load()
    for y in range(fb.height):
        base = y * fb.width
        for x in range(fb.width):
            v = min(15, fb.data[base + x])
            pixels[x, y] = int(v * 255 / 15)
    if scale > 1:
        img = img.resize((fb.width * scale, fb.height * scale), Image.NEAREST)
    return img
