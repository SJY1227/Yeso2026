"""Independent pack/source pixel check plus review sheets. Never reads QA screenshots as assets."""
from pathlib import Path
import json, struct, zlib, sys
from PIL import Image, ImageDraw
from image_lzss import decompress, image_bytes
ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'assets/packed/manifest.json').read_text())
data=(ROOT/'assets/packed/ui.pak').read_bytes()
assert data[:8]==b'RDAS0001'
size,crc=struct.unpack('<II',data[8:16]);assert len(data)==size+16 and zlib.crc32(data[16:])==crc
def source(name):
    for prefix,stem in [('themehomebase','homebase'),('themebackground','background'),('themepaper','paper'),('themehome','home'),('themetopbar','topbar'),('themearrows','arrows'),('homecharacter','homecharacter'),('rewardfood','rewardfood')]:
        if name.startswith(prefix):return ROOT/f'assets/processed/themes/{stem}-{name[len(prefix):]}.png'
    if name.startswith('characterArt'):return ROOT/f'assets/processed/themes/character-{name[12:]}.png'
    if name.startswith('lockedCharacterArt'):
        i=int(name[18:]);return ROOT/f'assets/processed/{"catalog" if i<6 else "themes"}/locked-{i}.png'
    if name.startswith('foodArt'):return ROOT/f'assets/processed/catalog/food-{int(name[7:])+1}.png'
    if name=='letterNotification':return ROOT/'assets/processed/themes/letter-notification.png'
    if name=='tearSprite':return ROOT/'assets/processed/motion/tear.png'
    if name.startswith('cry') and name[3:].isdigit():return ROOT/f'assets/processed/motion/cry-{name[3:]}.png'
    if name.startswith('letter') and 'frame' in name:
        a,b=name[6:].split('frame');return ROOT/f'assets/processed/motion/letter-{a}-{b}.png'
    if name.startswith('bite'):
        a,b=name[4:].split('frame');return ROOT/f'assets/processed/motion/bite-{a}-{b}.png'
    maps={'pinkBase':'pink-base','darkBase':'dark-base','letterImage':'product-letter','routineImage':'product-routine','catalogImage':'product-catalog','cryImage':'product-cry','blueberryImage':'product-blueberry'}
    if name in maps:return ROOT/f'assets/processed/{maps[name]}.png'
    maps={'catalogArrows':'arrows','catalogPinkBar':'topbar-pink','catalogDarkBar':'topbar-dark','catalogLock':'lock'}
    return ROOT/f'assets/processed/catalog/{maps[name]}.png'
for entry in manifest['entries']:
    png=source(entry['name']);box,raw=image_bytes(Image.open(png));assert list(box)==entry['box'],entry['name']
    encoded=data[16+entry['offset']:16+entry['offset']+entry['size']]
    assert decompress(encoded,len(raw))==raw,entry['name']
print(f'PASS: all {len(manifest["entries"])} archive entries match independently decoded original processed pixels')
if '--pack-only' in sys.argv:sys.exit(0)
registry=json.loads((ROOT/'design/character-registry.json').read_text(encoding='utf-8'))['characters']
out=ROOT/'design/previews/ten-characters';out.mkdir(parents=True,exist_ok=True)
def sheet(items,cols,name):
    result=Image.new('RGB',(cols*240,((len(items)+cols-1)//cols)*348),'white');d=ImageDraw.Draw(result)
    for i,(file,label) in enumerate(items):
        x,y=(i%cols)*240,(i//cols)*348;result.paste(Image.open(file).convert('RGB'),(x,y+28));d.text((x+6,y+5),label,fill='black')
    result.save(out/name)
pre=ROOT/'build/ten-previews'
sheet([(pre/f'home-{i}.ppm',f'{i}: {r["key"]}') for i,r in enumerate(registry)],5,'homes.png')
sheet([(pre/f'character-{i}-open-{stage}.ppm',f'{i}: {r["key"]} / stage {stage+1}') for i,r in enumerate(registry) for stage in range(3)],6,'catalog-open.png')
sheet([(pre/f'character-{i}-locked-2.ppm',f'{i}: {r["key"]} / locked adult') for i,r in enumerate(registry)],5,'catalog-locked.png')
sheet([(pre/f'character-0-locked-{i}.ppm',f'entry {i+1}/30') for i in range(30)],6,'all-locked.png')
