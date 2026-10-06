"""Small summary UI sprites live in firmware; the large FAT pack stays unchanged.

This module adds no runtime allocation. Run prepare_summary_assets.cjs first.
"""
from PIL import Image
from generate_assets import ROOT, OUT, numbers, font_set
from image_lzss import image_cpp

def main():
    cpp = '#include "SummaryAssets.h"\nnamespace assets {\n'
    header = '#pragma once\n#include "Assets.h"\nnamespace assets {\n'
    total = 0
    for kind in ('intro', 'end'):
        names = []
        for i in range(10):
            name = f'summary{kind.title()}{i}'
            code, size = image_cpp(name, Image.open(ROOT/f'assets/processed/summary/{kind}-{i}.png'), numbers)
            cpp += code; total += size; names.append('&'+name)
        header += f'extern const CompressedImage* summary{kind.title()}[10];\n'
        cpp += f'const CompressedImage* summary{kind.title()}[10] = {{{",".join(names)}}};\n'
    for key, name in [('closed','summaryClosed'),('back-short','summaryBackShort'),('back','summaryBack'),('front','summaryFront'),('arrows','summaryArrows'),('unfinished','summaryUnfinished')]+[(f'check-{i}',f'summaryCheck{i}') for i in range(3)]:
        code, size = image_cpp(name, Image.open(ROOT/f'assets/processed/summary/{key}.png'), numbers)
        cpp += code; total += size
        header += f'extern const CompressedImage {name};\n'
    cpp += font_set('summaryLabelFont','오늘 하루를 요약해줄게~오늘 하루도 수고했어~내일 봐~나가기',20,500)
    cpp += font_set('summaryTimeFont','0123456789:',24,500)
    header += 'extern const Font summaryLabelFont, summaryTimeFont;\n'
    (OUT/'SummaryAssets.h').write_text(header+'}\n',encoding='utf-8')
    (OUT/'SummaryAssets.cpp').write_text(cpp+'}\n',encoding='utf-8')
    print(f'Summary inline compressed sprites: {total:,} bytes; FAT asset pack unchanged')

if __name__ == '__main__':
    main()
