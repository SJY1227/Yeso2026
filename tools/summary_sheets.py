"""QA only: compare Figma screenshots with the actual C++ summary renderer.

Neither screenshots nor these contact sheets are firmware asset inputs.
"""
from pathlib import Path
from PIL import Image, ImageDraw
from generate_assets import ROOT

def main():
    rendered=ROOT/'build/summary-preview'
    reference=ROOT/'design/references/summary'
    out=ROOT/'design/previews/summary'
    out.mkdir(parents=True,exist_ok=True)
    pairs=[('102-2398','intro-0'),('102-3170','sheet-0-0'),('102-3246','sheet-0-1'),('102-3214','end-0')]
    sheet=Image.new('RGB',(960,690),'#eee')
    draw=ImageDraw.Draw(sheet)
    for i,(ref,name) in enumerate(pairs):
        sheet.paste(Image.open(reference/(ref+'.png')),(i*240,23))
        image=Image.open(rendered/(name+'.ppm'))
        image.save(out/(name+'.png'))
        sheet.paste(image,(i*240,370))
        draw.text((i*240+8,4),'Figma '+ref,fill='black')
        draw.text((i*240+8,350),'Firmware '+name,fill='black')
    sheet.save(out/'comparison.png')
    for kind in ('intro','sheet','end'):
        sheet=Image.new('RGB',(1200,640),'#eee')
        for theme in range(10):
            name=f'{kind}-{theme}'+('-0' if kind=='sheet' else '')
            sheet.paste(Image.open(rendered/(name+'.ppm')),((theme%5)*240,(theme//5)*320))
        sheet.save(out/(kind+'-themes.png'))
    frames=[Image.open(rendered/f'motion-{i}.ppm') for i in range(6)]
    frames.append(Image.open(rendered/'sheet-0-0.ppm'))
    frames[0].save(out/'letter.gif',save_all=True,append_images=frames[1:],duration=[180]*6+[1300],loop=0)
    print('Summary QA: original/rendered comparison, ten themes and six-frame letter sequence')

if __name__=='__main__':
    main()
