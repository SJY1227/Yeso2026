"""Generate native LCD assets from retained Figma layers, never QA screenshots."""
import json
import re
from PIL import Image, ImageDraw, ImageFont
from generate_assets import ROOT, OUT, numbers, font_set
from image_lzss import image_cpp


def main():
    geometry = json.loads((ROOT / 'design/catalog-geometry.json').read_text())
    registry = json.loads((ROOT / 'design/character-registry.json').read_text(encoding='utf-8'))['characters']
    for r in registry[2:]:
        for stage in range(3):
            geometry['characters'].append({'nameX':120})
            geometry['locked'].append({'nameX':107} if stage==2 else None)
    header = '#pragma once\n#include "Assets.h"\nnamespace assets {\n'
    header += 'struct CatalogArt { const CompressedImage* image; int16_t nameX; };\n'
    cpp = '#include "CatalogAssets.h"\nnamespace assets {\n'
    total = 0
    for category, array in [('characters','characterArt'),('locked','lockedCharacterArt'),('foods','foodArt')]:
        entries = []
        for i, slot in enumerate(geometry[category]):
            if slot is None:
                entries.append('  {nullptr, 120},');continue
            stem = f'food-{i+1}' if category == 'foods' else f'{category}-{i}'
            name = f'{array}{i}'
            path = f'assets/processed/themes/character-{i}.png' if category=='characters' else f'assets/processed/themes/locked-{i}.png' if category=='locked' and i>=6 else f'assets/processed/catalog/{stem}.png'
            image = Image.open(ROOT / path)
            source, size = image_cpp(name, image, numbers)
            cpp += source; total += size
            entries.append(f'  {{&{name}, {round(slot["nameX"])} }},')
        header += f'extern const CatalogArt {array}[{len(entries)}];\n'
        cpp += f'const CatalogArt {array}[] = {{\n' + '\n'.join(entries) + '\n};\n'
    for stem, name in [('arrows','catalogArrows'),('topbar-pink','catalogPinkBar'),('topbar-dark','catalogDarkBar')]:
        source, size = image_cpp(name, Image.open(ROOT / f'assets/processed/catalog/{stem}.png'), numbers)
        cpp += source; total += size
        header += f'extern const CompressedImage {name};\n'
    # Figma specifies an emoji text glyph, so render the local emoji font.
    lock = Image.new('RGBA', (960,1280))
    font = ImageFont.truetype('C:/Windows/Fonts/seguiemj.ttf',80)
    ImageDraw.Draw(lock).text((127*4,279*4), '\U0001f512', font=font, anchor='mt', embedded_color=True)
    lock = lock.resize((240,320),Image.Resampling.LANCZOS)
    lock.save(ROOT/'assets/processed/catalog/lock.png')
    source,size=image_cpp('catalogLock',lock,numbers)
    cpp+=source;total+=size;header+='extern const CompressedImage catalogLock;\n'
    labels = ''.join(re.findall(r'"([^"\n]*)"', (ROOT/'firmware/RoutineDevice/src/content/Catalog.cpp').read_text(encoding='utf-8')))
    labels += '0123456789/ >도감다시 키우기선택확인취소홈으로돌아가기???획득개남은'
    for name,size,weight,chars in (
        ('catalogFont',20,500,labels),
        ('catalogHeadingFont',22,500,'도감0123456789/'),
        ('catalogDetailFont',14,500,'0123456789 /획득개남은'),
        ('catalogPromptFont',24,500,'도감을 선택해주세요!다시 키우겠습니까?함께 지낼까요?'),
    ):
        header+=f'extern const Font {name};\n'
        cpp+=font_set(name,chars,size,weight)
    (OUT/'CatalogAssets.h').write_text(header+'}\n',encoding='utf-8')
    (OUT/'CatalogAssets.cpp').write_text(cpp+'}\n',encoding='utf-8')
    print(f'Catalog: 30 stages + 14 silhouettes + 31 foods + source UI; {total} compressed bytes')


if __name__=='__main__':
    main()
