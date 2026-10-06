"""Generate the table-driven ten-species theme registry from retained assets."""
import json
from PIL import Image
from generate_assets import ROOT, OUT, numbers
from image_lzss import image_cpp

def main():
    records = json.loads((ROOT/'design/character-registry.json').read_text(encoding='utf-8'))['characters']
    header = '''#pragma once
#include "Assets.h"
namespace assets {
struct CharacterTheme {
  const CompressedImage *topbar, *arrows, *background, *homeBase, *paper;
  uint16_t panel, button, accent, track, greeting, routineButton;
};
extern const CharacterTheme characterThemes[10];
extern const CompressedImage* homeCharacterArt[30];
extern const CompressedImage* rewardFoodArt[31];
extern const CompressedImage letterNotification;
const CharacterTheme& characterTheme(uint8_t id);
}
'''
    cpp = '#include "ThemeAssets.h"\nnamespace assets {\n'
    geometry=json.loads((ROOT/'design/extended-geometry.json').read_text(encoding='utf-8'))
    entries = []
    def color(value):
        rgb = int(value,16)
        return ((rgb>>19)<<11)|(((rgb>>10)&63)<<5)|((rgb>>3)&31)
    for r in records:
        i=r['id']
        for stem in ['topbar','arrows','background','homebase','paper']:
            cpp += image_cpp(f'theme{stem}{i}',Image.open(ROOT/f'assets/processed/themes/{stem}-{i}.png'),numbers)[0]
        values=[color(r[k]) for k in ['panel','button','accent','track']]+[65535 if i==1 else 0,color(geometry['buttonColors'][i])]
        entries.append(f'  {{&themetopbar{i}, &themearrows{i}, &themebackground{i}, &themehomebase{i}, &themepaper{i}, '+','.join(map(str,values))+'},')
    for array,stem,count in [('homeCharacterArt','homecharacter',30),('rewardFoodArt','rewardfood',31)]:
        items=[]
        for i in range(count):
            name=f'{stem}{i}'
            cpp+=image_cpp(name,Image.open(ROOT/f'assets/processed/themes/{stem}-{i}.png'),numbers)[0]
            items.append('&'+name)
        cpp+=f'const CompressedImage* {array}[{count}] = {{'+','.join(items)+'};\n'
    cpp+=image_cpp('letterNotification',Image.open(ROOT/'assets/processed/themes/letter-notification.png'),numbers)[0]
    cpp+='const CharacterTheme characterThemes[10] = {\n'+'\n'.join(entries)+'\n};\n'
    cpp+='const CharacterTheme& characterTheme(uint8_t id) { return characterThemes[id<10?id:0]; }\n}\n'
    (OUT/'ThemeAssets.h').write_text(header,encoding='utf-8')
    (OUT/'ThemeAssets.cpp').write_text(cpp,encoding='utf-8')

if __name__=='__main__': main()
