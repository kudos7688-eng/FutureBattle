"""Contact sheets of the exact B replay captures; source PNGs are retained."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw

p = Path(sys.argv[1])
for view in ['Recorded', 'RTS60', 'TPS', 'FPS']:
    filters = ['All', 'LOD0', 'LOD1', 'LOD2', 'Dist10_30', 'Dist20_50', 'Screen120_48']
    sheet = Image.new('RGB', (1440, 3*294), '#202020')
    draw = ImageDraw.Draw(sheet)
    for i, filt in enumerate(filters):
        f = p / f'seed1-B-Light-{view}-{filt}-r1-frame450.png'
        im = Image.open(f).convert('RGB')
        x, y = i % 3 * 480, i // 3 * 294
        sheet.paste(im.resize((480, 270)), (x, y+24))
        draw.text((x+8, y+4), f'{view} | {filt}', fill='white')
    sheet.save(p / f'B-{view}-contact.png')
    if view in ['TPS', 'FPS']:
        crop = Image.new('RGB', (1800, 624), '#202020')
        draw = ImageDraw.Draw(crop)
        for i, filt in enumerate(['LOD0', 'LOD1', 'LOD2']):
            im = Image.open(p / f'seed1-B-Light-{view}-{filt}-r1-frame450.png').convert('RGB')
            crop.paste(im.crop((600, 450, 1200, 1050)), (i*600, 24))
            draw.text((i*600+8, 4), f'{view} | {filt} | native pixels', fill='white')
        crop.save(p / f'B-{view}-LOD-crop.png')
