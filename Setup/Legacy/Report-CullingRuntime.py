import json,re,sys
from pathlib import Path
p=Path(sys.argv[1]);rows=[]
for profile in ['Light','Current']:
    text=(p/f'{profile}.log').read_text(encoding='utf-8-sig')
    for name,value,origin in re.findall(r'(r\.[A-Za-z0-9_.]+) = "([^"]+)"\s+LastSetBy: (\w+)',text):
        rows.append(dict(profile=profile,name=name,value=value,last_set_by=origin))
assert len(rows)==26
(p/'runtime-settings.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps(rows,indent=2))
