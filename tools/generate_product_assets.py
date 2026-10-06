"""Compact lossless RGB565 runs; full Hangul bitmap font and antialiased UI subset."""
from pathlib import Path
from PIL import Image,ImageFont,ImageDraw
import numpy as np
from generate_assets import numbers, font_set, FONT, ROOT
from image_lzss import image_cpp

OUT=ROOT/'firmware/RoutineDevice/src/generated'
def main():
    header='''#pragma once
#include "Assets.h"
namespace assets {
extern const CompressedImage letterImage, routineImage, catalogImage, cryImage, blueberryImage;
extern const Font koreanFont, uiFont, growthFont, statusFont;
}
'''
    cpp='#include "ProductAssets.h"\nnamespace assets {\n'
    total=0
    for key in ['letter','routine','catalog','cry','blueberry']:
        im=Image.open(ROOT/f'assets/processed/product-{key}.png').convert('RGBA' if key in ('cry','blueberry') else 'RGB')
        source,size=image_cpp(key+'Image',im,numbers)
        cpp+=source;total+=size
    font=ImageFont.truetype(str(FONT),22);font.set_variation_by_axes([500])
    data=[];glyphs=[]
    # Modern Korean syllables + Korean jamo + ASCII, not a sample-text-only font.
    points=list(range(32,127))+list(range(0x3131,0x318f))+list(range(0xac00,0xd7a4))
    for cp in points:
        char=chr(cp);l,t,r,b=font.getbbox(char,anchor='ls');w,h=max(1,r-l),max(1,b-t)
        im=Image.new('L',(w,h));ImageDraw.Draw(im).text((-l,-t),char,font=font,fill=255,anchor='ls')
        # Four coverage levels retain antialiasing for arbitrary server titles,
        # at twice the old binary font size rather than eight times its size.
        levels=((np.asarray(im,dtype=np.uint16).ravel()*3+127)//255).astype('uint8')
        padded=np.pad(levels,(0,(-len(levels))%4)).reshape(-1,4)
        packed=(padded[:,0]<<6)|(padded[:,1]<<4)|(padded[:,2]<<2)|padded[:,3]
        glyphs.append((cp,len(data),w,h,l,t,round(font.getlength(char)*64)))
        data.extend(packed)
    cpp+='const uint8_t koreanPixels[] = {\n'+numbers(data)+'\n};\nconst Glyph koreanGlyphs[] = {\n'
    cpp+='\n'.join('  {'+','.join(map(str,g))+'},' for g in glyphs)+'\n};\n'
    cpp+=f'const Font koreanFont = {{koreanPixels,koreanGlyphs,{len(glyphs)},2}};\n'
    labels='>확인취소포기완료진짜로...?하시겠습니까도감을선택해주세요캐릭터먹이좋은하루보내~'
    cpp+=font_set('uiFont',labels,20,500)
    # Native-size coverage masks keep the small home labels legible on a 240px LCD.
    # Include every digit because these are live growth values, not baked strings.
    cpp+=font_set('growthFont','0123456789단계 /',18,500)
    status_labels='아직 일정이 없어요일정을 기다리고 있어요오늘 하루도 수고했어~'
    cpp+=font_set('statusFont',status_labels,14,500)+'}\n'
    (OUT/'ProductAssets.h').write_text(header,encoding='utf-8');(OUT/'ProductAssets.cpp').write_text(cpp,encoding='utf-8')
    print(f'Product image bytes={total}; Korean bitmap bytes={len(data)}; glyphs={len(glyphs)}')
if __name__=='__main__': main()
