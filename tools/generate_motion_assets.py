"""Small shared sprite tables; motion does not store full-screen copies per frame."""
import json
from PIL import Image
from generate_assets import ROOT,OUT,numbers
from image_lzss import image_cpp
def main():
    m=json.loads((ROOT/'design/motion-registry.json').read_text(encoding='utf-8'))
    header='''#pragma once
#include "Assets.h"
namespace assets {
struct SpritePlacement { int16_t x,y,w,h; };
struct CryClip { const CompressedImage* body; SpritePlacement tears[5]; };
struct BiteClip { uint8_t count; const CompressedImage* frames[3]; };
extern const CryClip cryClips[30];
extern const BiteClip biteClips[31];
extern const CompressedImage tearSprite;
extern const CompressedImage* letterFrames[9][15];
extern const uint8_t characterLetter[10];
}
'''
    cpp='#include "MotionAssets.h"\nnamespace assets {\n'
    def emit(stem,name):return image_cpp(name,Image.open(ROOT/f'assets/processed/motion/{stem}.png'),numbers)[0]
    cpp+=emit('tear','tearSprite');cries=[]
    for i,c in enumerate(m['cry']):
        if c is None:cries.append('{nullptr,{}}');continue
        cpp+=emit(f'cry-{i}',f'cry{i}')
        positions=['{'+','.join(str(round(t[k])) for k in ('x','y','w','h'))+'}' if t else '{0,0,0,0}' for t in c['tears']]
        cries.append('{&cry'+str(i)+',{'+','.join(positions)+'}}')
    cpp+='const CryClip cryClips[30] = {\n'+',\n'.join(cries)+'};\n'
    entries=[]
    for i,b in enumerate(m['bites']):
        names=[]
        for n,_ in enumerate(b['nodes']):
            name=f'bite{i}frame{n}';cpp+=emit(f'bite-{i}-{n}',name);names.append('&'+name)
        entries.append('{'+str(len(names))+',{'+','.join(names+['nullptr']*(3-len(names)))+'}}')
    cpp+='const BiteClip biteClips[31] = {\n'+',\n'.join(entries)+'};\n'
    letters=[]
    for i in range(9):
        names=[]
        for frame in range(15):
            name=f'letter{i}frame{frame}';cpp+=emit(f'letter-{i}-{frame}',name);names.append('&'+name)
        letters.append('{'+','.join(names)+'}')
    cpp+='const CompressedImage* letterFrames[9][15]={'+','.join(letters)+'};\n'
    cpp+='const uint8_t characterLetter[10]={5,4,6,2,3,8,1,0,1,7};\n}\n'
    (OUT/'MotionAssets.h').write_text(header,encoding='utf-8');(OUT/'MotionAssets.cpp').write_text(cpp,encoding='utf-8')
if __name__=='__main__':main()
