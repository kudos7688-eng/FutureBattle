"""Verify requested/predicted LOD and actual submitted body triangle counts."""
import json, sys
from pathlib import Path

p = Path(sys.argv[1]); rows = []
for f in sorted(p.glob('seed*-B-*-r*.json')):
    d = json.loads(f.read_text(encoding='utf-8-sig')); ss = d['samples']
    assert len(ss) == 900
    assert all(len(s['lod_codes']) == 500 and set(s['lod_codes']) <= set('012') for s in ss)
    assert all(sum(s[f'requested_lod{i}'] for i in range(3)) == s['actor_shown']+s['mass_shown'] for s in ss)
    row = dict(file=f.name, predicted_mismatch_max=max(s['predicted_lod_mismatches'] for s in ss))
    if d['filter'] in ('LOD0', 'LOD1', 'LOD2'):
        level = int(d['filter'][-1]); assert row['predicted_mismatch_max'] == 0, row
        assert all(s['lod_codes'] == str(level)*500 for s in ss)
        expected = 0; actual = sum(s['actor_body_triangles']+s['mass_body_triangles'] for s in ss)
        for group in ('Actor', 'Mass'):
            asset = next(a for a in d['assets'] if a['group'] == group and a['type'] == 'skeletal')
            tris = asset['lods'][level]['triangles']
            expected += tris*sum(s[group.lower()+'_drawn'] for s in ss)
        relative = abs(actual-expected)/expected if expected else 0
        # A prior render LOD can linger for a frame; large failures must not pass as a forced-LOD result.
        assert relative < .002, (f.name, actual, expected)
        row.update(submitted_triangles=actual, expected_triangles=expected, relative_difference=relative)
    rows.append(row)
assert rows
(p/'lod-validation.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
print('LOD validation:', len(rows), 'runs; requested/predicted fixed LOD exact, submitted triangle tolerance <0.2%')
