"""PC-only fixtures; never writes synthetic routines or progress to hardware."""
from pathlib import Path
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parents[1]
out=root/'design/previews/v07';out.mkdir(parents=True,exist_ok=True)
def sheet(items,columns,name):
    canvas=Image.new('RGB',(columns*240,((len(items)+columns-1)//columns)*344),'white')
    draw=ImageDraw.Draw(canvas)
    for i,(stem,label) in enumerate(items):
        x,y=(i%columns)*240,(i//columns)*344
        canvas.paste(Image.open(root/f'build/scenarios/{stem}.ppm'),(x,y+24));draw.text((x+6,y+5),label,fill='black')
    canvas.save(out/(name+'.png'))
sheet([(f'home-{c}-{s}',f'Character {c} / stage {s}') for c in range(10) for s in range(1,4)],6,'stages')
sheet([(f'flow-{c}-{s}',f'Character {c} / screen {s}') for c in range(10) for s in (1,2,4)],6,'flows')
sheet([(f'food-{i}',f'Food {i}') for i in range(1,32)],6,'foods')
print('Wrote stage, themed-flow, food contact sheets to',out)
sheet([(f'letter-motion-{i}',f'Letter frame {i}') for i in range(21)],7,'letter-motion')
sheet([(f'cry-motion-{c}-{s}-{i}',f'Cry {c} stage {s} frame {i}') for c in (0,6,9) for s in range(1,4) for i in (0,2,4)],9,'cry-motion')
sheet([(f'eat-motion-{f}-{i}',f'Food {f} frame {i}') for f in (1,4,7,12,23,31) for i in range(4)],4,'eat-motion')
for name,stems,duration in [('letter',[f'letter-motion-{i}' for i in range(21)],100),('cry',[f'cry-motion-0-1-{i}' for i in range(5)],180)]:
    frames=[Image.open(root/f'build/scenarios/{s}.ppm').convert('RGB') for s in stems]
    frames[0].save(out/(name+'.gif'),save_all=True,append_images=frames[1:],duration=duration,loop=0)
