"""Create labeled comparison crops from the long-shadow, fixed-pose QA captures."""
import json, sys
from pathlib import Path
from PIL import Image, ImageDraw
import numpy as np

p = Path(sys.argv[1])
filters = ['All', 'FrustumHide', 'FrustumShadow']
rows = []
for view in ('TPS', 'FPS'):
    for run in (1, 2):
        for frame in (449, 450, 451):
            images = [Image.open(p / f'seed1-Shadow-Light-{view}-{f}-r{run}-frame{frame}.png').convert('RGB') for f in filters]
            a, h, s = [np.asarray(im, dtype=float) for im in images]
            row = dict(view=view, run=run, frame=frame,
                       full_image_hide_mae=float(np.abs(a-h).mean()),
                       full_image_shadow_mae=float(np.abs(a-s).mean()))
            if view == 'FPS':
                # Explicitly selected floor patch where an off-screen shadow is visible.
                a, h, s = [x[940:1060, 480:620] for x in (a, h, s)]
                row.update(ground_roi_xyxy=[480, 940, 620, 1060],
                           ground_hide_mae=float(np.abs(a-h).mean()),
                           ground_shadow_mae=float(np.abs(a-s).mean()))
            rows.append(row)
            if run == 1 and frame == 450:
                sheet = Image.new('RGB', (960, 1692), '#202020')
                draw = ImageDraw.Draw(sheet)
                for i, (im, label) in enumerate(zip(images, filters)):
                    sheet.paste(im.resize((960, 540)), (0, i*564+24))
                    draw.text((8, i*564+4), f'{view} | {label} | frame {frame}', fill='white')
                sheet.save(p / f'{view}-contact.png')
                if view == 'FPS':
                    crop = Image.new('RGB', (1260, 375), '#202020')
                    draw = ImageDraw.Draw(crop)
                    for i, (im, label) in enumerate(zip(images, filters)):
                        crop.paste(im.crop((450, 850, 730, 1080)).resize((420, 345)), (420*i, 30))
                        draw.text((420*i+8, 8), label, fill='white')
                    crop.save(p / 'FPS-shadow-crop.png')
(p / 'image-qa.json').write_text(json.dumps(dict(
    case='Light long shadows, fixed pose, seed1; performance excluded',
    pixel_scale='0..255',
    limitation='Selected floor patch and three adjacent frames; not an all-frame or dynamic-pose visual proof.',
    comparisons=rows), indent=2), encoding='utf-8')
print(json.dumps(rows, indent=2))
