"""Compose stage sprites, food bite sprites, and original letter-sheet frames.

Only source art is an input. Figma reference screenshots are QA, never textures.
"""
from pathlib import Path
import json,re
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'assets/source/motion';OUT=ROOT/'assets/processed/motion';OUT.mkdir(parents=True,exist_ok=True)
resample=Image.Resampling.LANCZOS
def placed(image,x,y,w,h):
    canvas=Image.new('RGBA',(240,320));image=image.convert('RGBA').resize((round(w),round(h)),resample)
    canvas.alpha_composite(image,(round(x),round(y)));return canvas
cry=json.loads((ROOT/'design/cry-geometry.json').read_text())
cry_entries=[None]*30
for f in cry:
    if f['column']:continue
    index=f['species']*3+f['stage']-1;b=f['layers'][0]
    placed(Image.open(SRC/f'body-{f["species"]}-{f["stage"]}.png'),b['x'],b['y'],b['w'],b['h']).save(OUT/f'cry-{index}.png')
    frames=sorted([x for x in cry if x['species']==f['species'] and x['stage']==f['stage']],key=lambda x:x['column'])
    cry_entries[index]={'node':f['id'],'tears':[x['layers'][1] if len(x['layers'])>1 else None for x in frames]}
placed(Image.open(SRC/'tear.png'),0,0,28,37).save(OUT/'tear.png')

def segments(mask):
    padded=np.concatenate(([False],mask,[False])).astype(np.int8)
    return list(zip(np.flatnonzero(np.diff(padded)==1),np.flatnonzero(np.diff(padded)==-1)))
letters=json.loads((ROOT/'design/letter-assets.json').read_text())['assets']
letter_frames=[]
for a in sorted(letters,key=lambda a:a['id']):
    image=Image.open(SRC/a['file']).convert('RGBA');alpha=np.asarray(image)[:,:,3]
    rows=[(lo,hi) for lo,hi in segments(np.any(alpha>8,axis=1)) if hi-lo>40]
    assert len(rows)==2,(a['id'],rows)
    pieces=[]
    for row,(top,bottom) in enumerate(rows):
        cols=[(lo,hi) for lo,hi in segments(np.any(alpha[top:bottom]>8,axis=0)) if hi-lo>60]
        assert len(cols)==(7 if row==0 else 8),(a['id'],cols)
        for left,right in cols:
            subalpha=alpha[top:bottom,left:right];yy,xx=np.nonzero(subalpha>8)
            box=(left+int(xx.min()),top+int(yy.min()),left+int(xx.max())+1,top+int(yy.max())+1)
            pieces.append((box,image.crop(box)))
    # Constant scale and baseline keep the paper's rise visible across frames.
    scale=226/max(p.width for _,p in pieces)
    idx=len(letter_frames);letter_frames.append({'node':a['id'],'boxes':[list(map(int,b)) for b,_ in pieces]})
    for n,(_,p) in enumerate(pieces):
        w,h=p.width*scale,p.height*scale
        placed(p,120-w/2,320-h,w,h).save(OUT/f'letter-{idx}-{n}.png')

names=['사과','감','바나나','멜론','블루베리','핫도그','솜사탕','딸기케이크','체리','오렌지','레몬','키위','팬케이크','쿠키','복숭아','김밥','석류','애플망고','파인애플','옥수수','크루아상','바닐라아이스크림','새우','무화과','용과','초밥','감자튀김','당고','빵','초코아이스크림','버섯']
nodes=json.loads((ROOT/'design/food-bites.json').read_text(encoding='utf-8'))
files={a['id']:a for a in json.loads((ROOT/'design/bite-assets.json').read_text())['assets'] if 'url' in a}
bites=[]
for i,name in enumerate(names):
    source_name='크로아상' if name=='크루아상' else name
    candidates=[n for n in nodes if n['type']=='BOOLEAN_OPERATION' and any(re.sub(r'[\s\d]','',x)==source_name for x in n['originals'])]
    candidates.sort(key=lambda n:n['x'])
    actual=[]
    for n in candidates:
        if n['id'] not in files:break # Do not jump over an unavailable bite.
        p=Image.open(SRC/files[n['id']]['file']).convert('RGBA');k=min(103/n['w'],79/n['h'])
        w,h=n['w']*k,n['h']*k
        placed(p,124.5-w/2,258-h,w,h).save(OUT/f'bite-{i}-{len(actual)}.png');actual.append(n['id'])
    bites.append({'food':i+1,'nodes':actual,'pending':[n['id'] for n in candidates if n['id'] not in actual]})
(ROOT/'design/motion-registry.json').write_text(json.dumps({'schema':1,'cry':cry_entries,'letters':letter_frames,'bites':bites,'timing':'provisional configurable defaults; storyboard has no native keyframe timing'},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Prepared',sum(x is not None for x in cry_entries),'cry stage bodies,',len(letter_frames)*15,'letter frames,',sum(len(x['nodes']) for x in bites),'bite frames')
