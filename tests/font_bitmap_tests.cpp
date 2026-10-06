#include "../firmware/RoutineDevice/src/presentation/FontBitmap.h"
#include "../firmware/RoutineDevice/src/generated/ProductAssets.h"
#include "../firmware/RoutineDevice/src/application/StateCodec.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>

int main(){
  const auto& font=assets::koreanFont;
  assert(font.count==11361&&font.compressed&&font.bitsPerPixel==2);
  std::vector<uint8_t> original;
  uint8_t buffer[130];
  for(size_t i=0;i<font.count;++i){
    std::fill_n(buffer,sizeof(buffer),0xcd);
    const size_t bytes=(size_t(font.glyphs[i].width)*font.glyphs[i].height+3)/4;
    const auto* decoded=routine::ui::fontBitmap(font,i,buffer+1,128);
    assert(decoded==buffer+1&&buffer[0]==0xcd&&buffer[129]==0xcd);
    original.insert(original.end(),decoded,decoded+bytes);
  }
  // Independently captured from the complete uncompressed v0.8.2 font.
  assert(original.size()==1188324);
  assert(routine::application::crc32(original.data(),original.size())==0x29d206c6);
  const uint8_t overlap[]={2,0xaa,0,0};
  assets::Glyph glyph{65,0,16,1,0,0,640};
  assets::Font sample{overlap,&glyph,1,2,true,sizeof(overlap)};
  assert(routine::ui::fontBitmap(sample,0,buffer,4)==buffer);
  for(unsigned i=0;i<4;++i)assert(buffer[i]==0xaa);
  assert(!routine::ui::fontBitmap(sample,1,buffer,128));
  assert(!routine::ui::fontBitmap(sample,0,buffer,3));
  for(size_t n=0;n<sizeof(overlap);++n){sample.pixelBytes=n;assert(!routine::ui::fontBitmap(sample,0,buffer,128));}
  const uint8_t invalidMatch[]={1,0,0};sample.pixels=invalidMatch;sample.pixelBytes=3;
  assert(!routine::ui::fontBitmap(sample,0,buffer,128));
  const uint8_t trailing[]={2,0xaa,0,0,0};sample.pixels=trailing;sample.pixelBytes=5;
  assert(!routine::ui::fontBitmap(sample,0,buffer,128));
  glyph.offset=6;assert(!routine::ui::fontBitmap(sample,0,buffer,128));
  std::puts("PASS: all 11361 font glyphs equal original 1188324 bytes (CRC 29d206c6), bounded LZSS decode and corrupt streams");
}
