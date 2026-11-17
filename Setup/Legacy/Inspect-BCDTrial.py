"""Build contact sheets from preserved C/D captures without changing source images."""
from pathlib import Path
import sys
from PIL import Image,ImageDraw
p=Path(sys.argv[1]); stage=sys.argv[2];frame=int(sys.argv[3]) if len(sys.argv)>3 else 450
if stage=='C':
    for view in ['TPS','FPS']:
        filters=['Reference','H0','H10','H20','H30']
        sheet=Image.new('RGB',(1440,294*len(filters)),'#202020');draw=ImageDraw.Draw(sheet)
        for y,filt in enumerate(filters):
            for x,n in enumerate(range(frame-1,frame+2)):
                file=p/f'seed1-C-Light-{view}-{filt}-r1-frame{n}.png'
                im=Image.open(file).convert('RGB');sheet.paste(im.resize((480,270)),(480*x,294*y+24))
                draw.text((480*x+5,294*y+5),f'{view} {filt} frame {n}',fill='white')
        sheet.save(p/f'C-{view}-consecutive.png')
        crop=Image.new('RGB',(1200,1272),'#202020');draw=ImageDraw.Draw(crop)
        for y,filt in enumerate(['Reference','H0','H20']):
            for x,n in enumerate(range(frame-1,frame+2)):
                im=Image.open(p/f'seed1-C-Light-{view}-{filt}-r1-frame{n}.png').convert('RGB')
                crop.paste(im.crop((760,340,1160,740)),(400*x,424*y+24))
                draw.text((400*x+5,424*y+5),f'{view} {filt} frame {n} native',fill='white')
        crop.save(p/f'C-{view}-native.png')
else:
    filters=['All','Shadow30','Shadow60','Material40','Combined','Bias1']
    for view in ['Recorded','RTS60','TPS','FPS']:
        sheet=Image.new('RGB',(1440,588),'#202020');draw=ImageDraw.Draw(sheet)
        for i,filt in enumerate(filters):
            file=p/f'seed1-D-Light-{view}-{filt}-r1-frame{frame}.png'
            im=Image.open(file).convert('RGB');x=i%3*480;y=i//3*294
            sheet.paste(im.resize((480,270)),(x,y+24));draw.text((x+5,y+5),f'{view} {filt}',fill='white')
        sheet.save(p/f'D-{view}-contact.png')
        crop=Image.new('RGB',(1800,624),'#202020');draw=ImageDraw.Draw(crop)
        for i,filt in enumerate(['All','Shadow30','Combined']):
            im=Image.open(p/f'seed1-D-Light-{view}-{filt}-r1-frame{frame}.png').convert('RGB')
            crop.paste(im.crop((600,450,1200,1050)),(600*i,24));draw.text((600*i+5,5),f'{view} {filt} native pixels',fill='white')
        crop.save(p/f'D-{view}-crop.png')
