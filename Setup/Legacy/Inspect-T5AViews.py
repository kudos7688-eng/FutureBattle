"""QA contact sheets: real replay captures only, no rescaling of benchmark output."""
import argparse
from pathlib import Path
from PIL import Image,ImageDraw

def main(folder,frame):
 profiles=['Current','Software','Light','ClassicShadow','NoShadow','Medium','Low']
 for view in ('Recorded','RTS60','TPS','FPS'):
  sheet=Image.new('RGB',(960,4*299),'#202020')
  labels=ImageDraw.Draw(sheet)
  for i,profile in enumerate(profiles):
   src=folder/f'seed1-A2-{profile}-{view}-All-r1-frame{frame}.png'
   x=i%2*480;y=i//2*299
   with Image.open(src) as im:
    assert im.size==(1920,1080),src
    sheet.paste(im.convert('RGB').resize((480,270),Image.Resampling.LANCZOS),(x,y+24))
   labels.text((x+6,y+4),f'{view} | {profile} | frame {frame}',fill='white')
  sheet.save(folder/f'A2-{view}-profiles.jpg',quality=93)

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--frame',type=int,default=422)
 args=p.parse_args();main(args.folder,args.frame)
