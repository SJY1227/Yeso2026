"""RGB565 backgrounds and small antialiased font subsets for the initial home UI."""
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np
from image_lzss import image_cpp

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "firmware" / "RoutineDevice" / "src" / "generated"
FONT = ROOT / "assets" / "fonts" / "NotoSansKR-VF.ttf"


def numbers(values, columns=20):
    values = list(values)
    return "\n".join("  " + ",".join(str(int(x)) for x in values[i:i+columns]) + ","
                     for i in range(0, len(values), columns))


def font_set(name, text, size, weight):
    font = ImageFont.truetype(str(FONT), size)
    font.set_variation_by_axes([weight])
    data, glyphs = [], []
    for char in sorted(set(text)):
        left, top, right, bottom = font.getbbox(char, anchor="ls")
        w, h = max(1, right-left), max(1, bottom-top)
        image = Image.new("L", (w, h))
        ImageDraw.Draw(image).text((-left, -top), char, font=font, fill=255, anchor="ls")
        glyphs.append((ord(char), len(data), w, h, left, top, round(font.getlength(char)*64)))
        data.extend(image.tobytes())
    cpp = f"const uint8_t {name}Pixels[] = {{\n{numbers(data)}\n}};\n"
    cpp += f"const Glyph {name}Glyphs[] = {{\n"
    cpp += "\n".join("  {"+",".join(map(str,g))+"}," for g in glyphs)+"\n};\n"
    cpp += f"const Font {name} = {{{name}Pixels, {name}Glyphs, {len(glyphs)}}};\n"
    return cpp


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    header = '''#pragma once
#include <cstdint>
#include <cstddef>
#include "../presentation/ImageRenderer.h"
namespace assets {
struct Glyph { uint32_t codepoint, offset; uint16_t width, height; int16_t left, top, advance64; };
struct Font { const uint8_t* pixels; const Glyph* glyphs; size_t count; uint8_t bitsPerPixel=8; bool compressed=false; size_t pixelBytes=0; };
extern const CompressedImage pinkBase;
extern const CompressedImage darkBase;
extern const Font greetingFont;
extern const Font timeFont;
}
'''
    (OUT / "Assets.h").write_text(header, encoding="utf-8")
    cpp = '#include "Assets.h"\nnamespace assets {\n'
    for theme in ["pink", "dark"]:
        image = Image.open(ROOT / f"assets/processed/{theme}-base.png").convert("RGB")
        assert image.size == (240, 320)
        cpp += image_cpp(f'{theme}Base', image, numbers)[0]
    cpp += font_set("greetingFont", "좋은 하루 보내~", 22, 500)
    cpp += font_set("timeFont", "AMP0123456789:- ?", 16, 600)
    cpp += "}\n"
    (OUT / "Assets.cpp").write_text(cpp, encoding="utf-8")
    print("Generated RGB565 backgrounds and Noto Sans KR glyph subsets")


if __name__ == "__main__":
    main()
