"""Compare actual ShotDirection samples with independent analytic angular laws.
Python stdlib only. Capsule ray geometry: radius 36, half-height 90 cm;
origin z=60, target aim z=35, target centre z=0. No weapon range clipping.
"""
import argparse, array, csv, json, math
from pathlib import Path

def folded_cdf(t, half):
    return sum((math.cos(k*half)-math.cos(min(k*half+t, math.pi)))/2
               for k in range(math.ceil(math.pi/half)))

def capsule_probability(deg, distance, radial=400, azimuth=720):
    half=math.radians(deg)
    # Independent numerical integration, polar angle midpoint and exact bin masses.
    weights=[]
    for i in range(radial):
        lo,hi=half*i/radial,half*(i+1)/radial
        weights.append(((math.cos(lo)-math.cos(hi))/(1-math.cos(half)),
                        folded_cdf(hi,half)-folded_cdf(lo,half)))
    length=math.hypot(distance,25); ax,az=distance/length,-25/length
    # Orthonormal plane around the actual downward aim direction.
    around=[(math.cos(2*math.pi*(j+.5)/azimuth),math.sin(2*math.pi*(j+.5)/azimuth)) for j in range(azimuth)]
    uniform=folded=0
    for i,(wu,wf) in enumerate(weights):
        phi=half*(i+.5)/radial; cp,sp=math.cos(phi),math.sin(phi)
        hits=0
        for co,si in around:
            dx=cp*ax-sp*co*az; dy=sp*si; dz=cp*az+sp*co*ax
            # Ray vs vertical finite segment, same geometry independently expanded.
            uv=108*dz; uw=-distance*dx+114*dz; vw=12312
            s=max(0,min(1,(vw-uv*uw)/(11664-uv*uv)))
            t=max(0,uv*s-uw)
            if t==0:s=max(0,min(1,vw/11664))
            hits+=(dx*t-distance)**2+(dy*t)**2+(114+dz*t-108*s)**2<=1296
        uniform+=wu*hits/azimuth; folded+=wf*hits/azimuth
    return uniform,folded

def main(folder):
    hist=list(csv.DictReader((folder/'histogram.csv').open(encoding='utf-8-sig')))
    cdf=[]; distributions=[]
    for deg in (1.2,4.,.35):
        samples=array.array('f');samples.frombytes((folder/f'angles-{deg:.2f}.f32').read_bytes())
        assert len(samples)>=1000000 and min(samples)>=0 and max(samples)<=deg+1e-4
        rows=[r for r in hist if float(r['degrees'])==deg]; du=df=0
        for r in rows:
            angle=float(r['bin_upper_degrees']); empirical=float(r['cdf']);half=math.radians(deg);t=math.radians(angle)
            u=(1-math.cos(t))/(1-math.cos(half));f=folded_cdf(t,half)
            du=max(du,abs(empirical-u));df=max(df,abs(empirical-f))
            cdf.append(dict(degrees=deg,angle=angle,empirical=empirical,uniform=u,appendix_folded=f))
        distributions.append(dict(degrees=deg,samples=len(samples),max_cdf_error_uniform=du,max_cdf_error_folded=df))
    comparisons=[]
    for r in csv.DictReader((folder/'capsule.csv').open(encoding='utf-8-sig')):
        deg=float(r['degrees']);d=float(r['distance_m']);actual=float(r['probability'])
        u,f=capsule_probability(deg,d*100)
        comparisons.append(dict(degrees=deg,distance_m=d,engine=actual,uniform=u,appendix_folded=f,engine_minus_folded=actual-f))
    for name,rows in [('cdf-comparison.csv',cdf),('hit-comparison.csv',comparisons)]:
        with (folder/name).open('w',newline='',encoding='utf-8') as out:
            w=csv.DictWriter(out,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
    # Double grid on the hardest partial-hit cases; quantify integration convergence.
    convergence=[]
    for deg,d in [(4,10),(4,50),(1.2,30),(1.2,50)]:
        u,f=capsule_probability(deg,d*100,800,1440)
        old=next(r for r in comparisons if r['degrees']==deg and r['distance_m']==d)
        convergence.append(dict(degrees=deg,distance_m=d,folded_refined=f,change=f-old['appendix_folded']))
    result=dict(distributions=distributions,hit_comparisons=comparisons,quadrature_convergence=convergence,
                max_probability_error=max(abs(r['engine_minus_folded']) for r in comparisons),
                note='Appendix full original simulation table/script not present; independent reconstruction of its stated acos/fmod law. Original published example: 4 deg, 10m, folded .712, uniform .534. Geometry-only, no range clipping.')
    (folder/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);main(p.parse_args().folder)
