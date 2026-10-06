"""Check actual C++ decoded pixels and produce Figma/catalog comparison sheets.

Run build/catalog-tests.exe build/catalog-direct before this script. Expected
pixels come from the prepared PNGs, independently of the compression encoder.
"""
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]


def main():
    target=ROOT/'build/catalog-direct'
    out=ROOT/'design/previews/catalog-direct'
    out.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((ROOT/'design/catalog-assets.json').read_text(encoding='utf-8'))
    for frame in manifest['frames']:
        for asset in frame['assets']:
            data=(ROOT/'assets/source/catalog'/asset['file']).read_bytes()
            assert data and hashlib.sha256(data).hexdigest()==asset['sha256']
    verified=0
    for binary in target.glob('asset-*.bin'):
        name=binary.stem.removeprefix('asset-')
        path=ROOT/'assets/processed'/f'{name}.png'
        if not path.is_file(): path=ROOT/'assets/processed/catalog'/f'{name}.png'
        rgba=np.asarray(Image.open(path).convert('RGBA'),dtype=np.uint32)
        alpha=rgba[:,:,3]
        channels=[]
        for channel,shift,maximum in [(0,3,31),(1,2,63),(2,3,31)]:
            channels.append(((rgba[:,:,channel]>>shift)*alpha+maximum*(255-alpha)+127)//255)
        expected=(channels[0]<<11)|(channels[1]<<5)|channels[2]
        actual=np.frombuffer(binary.read_bytes(),dtype='<u2').reshape(320,240)
        assert np.array_equal(expected,actual),f'Pixel mismatch: {name}'
        verified+=1
    assert verified==54
    old=ROOT/'.local/catalog-direct-before'
    unchanged=[];changed=[]
    for p in target.glob('product-*.ppm'):
        index=int(p.stem.split('-')[-1])
        if (old/p.name).is_file():
            (unchanged if p.read_bytes()==(old/p.name).read_bytes() else changed).append(index)
    if old.exists(): assert set(changed)=={13,14,15,16},changed
    # Screenshots are comparison targets only. They never enter asset generation.
    geometry=json.loads((ROOT/'design/catalog-geometry.json').read_text())
    rows=[]
    for i,entry in enumerate(geometry['characters']):
        rows.append((entry['id'],f'character-{i//3}-open-{i%3}'))
    for i in [0,4,21,29]: rows.append((geometry['foods'][i]['id'],f'food-0-open-{i}'))
    for i in [0,4]: rows.append((geometry['locked'][i]['id'],f'character-{1-i//3}-locked-{3+i%3}'))
    sheet=Image.new('RGB',(960,350*((len(rows)+1)//2)),'#ddd')
    d=ImageDraw.Draw(sheet)
    for i,(node,stem) in enumerate(rows):
        x=(i%2)*480;y=(i//2)*350
        d.text((x+8,y+5),f'Figma {node}',fill='black')
        d.text((x+248,y+5),'C++ LCD render',fill='black')
        reference=Image.open(ROOT/'design/references/catalog'/f'{node.replace(":","-")}.png').convert('RGB').resize((240,320),Image.Resampling.LANCZOS)
        actual=Image.open(target/f'{stem}.ppm')
        sheet.paste(reference,(x,y+25));sheet.paste(actual,(x+240,y+25))
    sheet.save(out/'figma-comparison.png')
    foods=Image.new('RGB',(240*8,320*4),'#ddd')
    for i in range(31): foods.paste(Image.open(target/f'food-0-open-{i}.ppm'),((i%8)*240,(i//8)*320))
    foods.save(out/'foods.png')
    for p in target.glob('*.ppm'): Image.open(p).save(out/(p.stem+'.png'))
    report={'source_frames':len(manifest['frames']),'source_assets':sum(len(f['assets']) for f in manifest['frames']),
            'exact_decoded_images':verified,'unchanged_screens':sorted(unchanged),'changed_screens':sorted(changed)}
    (target/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__=='__main__':main()
