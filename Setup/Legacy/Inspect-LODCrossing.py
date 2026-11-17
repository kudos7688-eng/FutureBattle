"""Native-pixel consecutive captures at the selected H20 threshold crossing."""
import json,sys
from pathlib import Path
from PIL import Image,ImageDraw
p=Path(sys.argv[1]);out=Image.new('RGB',(1200,848),'#202020');draw=ImageDraw.Draw(out);rows=[]
for y,view in enumerate(['TPS','FPS']):
    d=json.loads((p/f'seed1-C-Light-{view}-H20-r1.json').read_text());probe=d['lod_probe_index']
    for x,frame in enumerate([304,305,306]):
        s=d['samples'][frame];submitted=bool((int.from_bytes(bytes.fromhex(s['original_drawn_bits']),'little')>>probe)&1)
        rows.append(dict(view=view,frame=frame,probe=probe,lod=s['probe_lod'],submitted=submitted,distance_cm=s['probe_distance_cm']))
        im=Image.open(p/f'seed1-C-Light-{view}-H20-r1-frame{frame}.png').convert('RGB')
        out.paste(im.crop((760,340,1160,740)),(x*400,y*424+24))
        draw.text((x*400+5,y*424+5),f"{view} frame {frame} | probe LOD {int(s['probe_lod'])}",fill='white')
assert all(r['submitted'] for r in rows)
assert [r['lod'] for r in rows]==[0,1,1,0,1,1],rows
out.save(p/'H20-crossing-native.png')
(p/'crossing-qa.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps(rows,indent=2))
