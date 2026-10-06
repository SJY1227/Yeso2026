#pragma once
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
