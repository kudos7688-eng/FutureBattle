"""Export the H command map from its parcel and road definitions. Distances are metres."""
from collections import defaultdict
from pathlib import Path
import re
from xml.etree.ElementTree import Element, SubElement, ElementTree

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Game/FutureBattle/Source/FutureBattle/Prototype"
parcels = (SRC / "BattleSectors.h").read_text(encoding="utf-8")
east = re.findall(r'"([RBJSHWFT^]{10})"', parcels)
assert len(east) == 14
grid = [left[::-1] + right for left, right in zip(east[::-1], east)]
def pairs(line):
    return [(int(x), int(y)) for x, y in re.findall(r'\{(-?\d+),(-?\d+)\}', line)]
roads = [(line.split("::")[1].split(",")[0], pairs(line))
         for line in (SRC / "BattleSectorRoads.inl").read_text().splitlines() if line.startswith("Road(")]
network = defaultdict(set)
for lane, points in roads:
    for a, b in zip(points, points[1:]):
        assert (a[0] == b[0]) != (a[1] == b[1])
        network[a].add(b)
        network[b].add(a)
seen = set()
todo = [(-420, 0)]
while todo:
    point = todo.pop()
    if point not in seen:
        seen.add(point)
        todo.extend(network[point] - seen)
assert seen == network.keys() and (420, 0) in seen
bridges = {a[1] for a in seen for b in network[a] if a[0] < 0 < b[0]}
assert bridges == {-225, 0, 225}
for y in (-125,125):
    assert (-100,y) in seen and (100,y) in seen

# Both residential districts and both warehouse districts must each form a single block.
for kind in "HW":
    parcels_of_kind = {(x, y) for y, row in enumerate(grid) for x, k in enumerate(row) if k == kind}
    components = []
    while parcels_of_kind:
        work = [parcels_of_kind.pop()]
        component = set(work)
        while work:
            x, y = work.pop()
            for neighbour in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
                if neighbour in parcels_of_kind:
                    parcels_of_kind.remove(neighbour)
                    component.add(neighbour)
                    work.append(neighbour)
        components.append(component)
    assert len(components) == 2 and len(components[0]) == len(components[1]), kind
    print(f"{kind}: two contiguous districts of {len(components[0])} parcels each")

source = (SRC / "BattleFieldLayout.h").read_text(encoding="utf-8").split('D.bSectors=true;',1)[1].split('D.Objectives.Add',1)[0]
objectives = [(int(x),int(y)) for x,y in re.findall(r'FIntPoint\((\d+),(\d+)\)',source)]
assert len(set(objectives)) == 8
assert objectives == [(19-x,13-y) for x,y in reversed(objectives)]
river = pairs(parcels.split("RiverNorth[]=",1)[1].split(";",1)[0])
river = [(-x,-y) for x,y in reversed(river[1:])] + river
def river_x(y):
    for (a,b),(c,d) in zip(river,river[1:]):
        if b <= y <= d:
            return a+(c-a)*(y-b)/(d-b)
    raise ValueError(y)

svg = Element("svg", xmlns="http://www.w3.org/2000/svg", width="1160", height="950", viewBox="0 0 1160 950")
def add(tag, **attrs):
    return SubElement(svg,tag,{k.replace('_','-'):str(v) for k,v in attrs.items()})
def label(x,y,text,size=15,**attrs):
    add("text", x=x,y=y,font_family="Meiryo, sans-serif",font_size=size,**attrs).text=str(text)
def xy(p):
    return (580+p[0],478-p[1])
def path(points,color,width):
    add("polyline", points=" ".join("%g,%g"%xy(p) for p in points), fill="none", stroke=color, stroke_width=width,stroke_linejoin="round")
palette={'H':'#d6ba9e','W':'#b1b7b7','J':'#d4cabb','F':'#d9ca89','T':'#8fa884','^':'#a2a278','R':'#b2c6ad','B':'#d4cabb','S':'#bbcaad'}
add("rect",width=1160,height=950,fill="#faf8f0")
label(80,48,"マップH　河川沿いの集落と街道",29,fill="#233d35",font_weight="bold")
label(80,81,"1区画 50 m　／　東西 1,000 m × 南北 700 m　／　北が上",16,fill="#41534b")
for x in range(20):
    label(105+x*50,119,chr(65+x),13,text_anchor="middle")
for y,row in enumerate(grid):
    sy=128+(13-y)*50
    label(66,sy+30,y+1,13,text_anchor="end")
    for x,k in enumerate(row):
        sx=80+x*50
        add("rect",x=sx,y=sy,width=50,height=50,fill=palette[k])
        if k in 'HW':
            side=1 if x>=10 else -1
            cx=sx+25+side*(8 if k=='H' else 8.5)
            cy=sy+25-side*(7 if k=='H' else 13)
            w,h=(15,11) if k=='H' else (20,15)
            add("rect",x=cx-w/2,y=cy-h/2,width=w,height=h,fill="#786255" if k=='H' else "#5b6b73")
        if k=='F':
            for n in range(8,49,10):
                add("path",d=f"M{sx+2} {sy+n}h46",stroke="#c4b778",stroke_width=1)
        if k=='T':
            for dx,dy in [(11,12),(30,17),(20,33),(39,37)]:
                add("circle",cx=sx+dx,cy=sy+dy,r=5,fill="#738d6c")
        if k=='^':
            for r in [18,12,6]:
                add("ellipse",cx=sx+25,cy=sy+25,rx=r,ry=r*.65,fill="none",stroke="#797e60")
for i in range(21):
    add("path",d=f"M{80+50*i} 128v700",stroke="#444c42",stroke_opacity=".19")
for i in range(15):
    add("path",d=f"M80 {128+50*i}h1000",stroke="#444c42",stroke_opacity=".19")
path(river,"#659ab0",10)
for y in (-125,125):
    path([(-300 if y>0 else -100,y),(300 if y<0 else 100,y)],"#97754e",3)
    path([(river_x(y)-6,y),(river_x(y)+6,y)],"#b5ccbe",3)
for lane,pts in roads:
    w={"Centre":14,"Connector":5.5,"North":8.5,"South":8.5}[lane]
    path(pts,"#857d71",w+2)
    path(pts,"#eee5d5",w)
for i,(x,y) in enumerate(objectives):
    cx,cy=xy(((x-9.5)*50,(y-6.5)*50))
    add("circle",cx=cx,cy=cy,r=10,fill="#304d52")
    label(cx,cy+4,str(i+1),12,text_anchor="middle",fill="white")
for x in [-420,420]:
    cx,cy=xy((x,0))
    add("rect",x=cx-6,y=cy-6,width=12,height=12,fill="#993f36")
for i,(k,name) in enumerate([('H','住宅街'),('W','倉庫街'),('F','農地'),('T','林'),('^','丘'),('R','川岸')]):
    px=80+i*167
    add("rect",x=px,y=854,width=20,height=15,fill=palette[k])
    label(px+27,868,name,15)
label(80,904,"赤い四角＝本拠地　　丸数字＝得点拠点　　中央の太い道路＝大通り　　浅瀬へ延びる細い茶線＝農道",15)
ElementTree(svg).write(ROOT/'Docs/MapH-Sectors.svg',encoding="utf-8",xml_declaration=True)
print(f"{len(grid)*20} parcels, {len(seen)} connected junctions, {sum(map(len,network.values()))//2} road segments, 3 bridges and 2 ford approaches")
