"""Import the asset kit (Art/Kit) into /Game/Kit. Display assets only; no map, Source or Config changes.
  Surfaces: Art/Kit/Surfaces/<Name>/T_Kit_<Name>_{BaseColor,Normal,ORM}.jpg (Poly Haven, CC0; surfaces.json) ->
            /Game/Kit/Surfaces/Textures/T_Kit_<Name>_*, master M_Kit_Surface (world-aligned three-plane projection, so it
            tiles at real size on scaled cubes and on any mesh without UVs), instances MI_Kit_<Name>.
  Weapons:  Art/Kit/Weapons/<Name>/Exports/SM_Kit_<Name>.fbx + Textures -> /Game/Kit/Weapons/SM_Kit_<Name>, M_Kit_<Name>.
  Props:    Art/Kit/Props/<Name>/Exports/SM_Kit_<Name>.fbx + Textures -> /Game/Kit/Props/SM_Kit_<Name>, M_Kit_<Name>.
Run in the full editor with -ExecutePythonScript (Setup/Import-Kit.ps1). Re-running replaces the assets in place.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / 'Art' / 'Kit'
tools = u.AssetToolsHelpers.get_asset_tools()
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
ml = u.MaterialEditingLibrary
eal = u.EditorAssetLibrary
report = {'surfaces': {}, 'weapons': {}, 'props': {}, 'errors': []}


def import_texture(path, dest, name, kind, flip_green):
    t = u.AssetImportTask(); t.filename = str(path); t.destination_path = dest; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    tex = u.load_asset(dest + '/' + name); assert tex, name
    tex.set_editor_property('srgb', kind == 'BaseColor')
    if kind == 'ORM': tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS)
    if kind == 'Normal':
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel', flip_green)
    eal.save_loaded_asset(tex)
    return tex


# ---------------------------------------------------------------- surfaces
# Small value noise in world space (cm) for the large-scale variation; a struct, because a Custom node body cannot declare functions.
NOISE = '''
struct KitNoise {
 float h(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
 float n(float3 x) { float3 i = floor(x), f = frac(x); f = f * f * (3.0 - 2.0 * f);
  return lerp(lerp(lerp(h(i), h(i + float3(1, 0, 0)), f.x), lerp(h(i + float3(0, 1, 0)), h(i + float3(1, 1, 0)), f.x), f.y),
              lerp(lerp(h(i + float3(0, 0, 1)), h(i + float3(1, 0, 1)), f.x), lerp(h(i + float3(0, 1, 1)), h(i + float3(1, 1, 1)), f.x), f.y), f.z); }
 float fbm(float3 x) { return n(x) * 0.5 + n(x * 2.03 + 7.1) * 0.3 + n(x * 4.01 + 13.7) * 0.2; }
};
KitNoise K;
'''
TRIPLANAR = '''
float3 p = WorldPos / max(Tile, 1.0);
float3 n = normalize(WorldNormal);
float3 w = pow(abs(n), 4.0); w /= (w.x + w.y + w.z);
float2 uvx = p.yz * float2(1, -1), uvy = p.xz * float2(1, -1), uvz = p.xy;
'''
CODE_PLAIN = TRIPLANAR + '''
return Texture2DSample(Tex, TexSampler, uvx).rgb * w.x + Texture2DSample(Tex, TexSampler, uvy).rgb * w.y + Texture2DSample(Tex, TexSampler, uvz).rgb * w.z;
'''
# Colour without the visible repeat (2026-11-27): the same photo is sampled a second time 7 times larger and turned, and its
# tone relative to the average of the photo (the smallest mip) multiplies the sharp sample (Mix); on top a 30 m brightness
# variation (Macro) and darker, greyer patches a few metres wide (Dirt). The detail stays as sharp as before.
CODE_COLOR = NOISE + TRIPLANAR + '''
float3 c = Texture2DSample(Tex, TexSampler, uvx).rgb * w.x + Texture2DSample(Tex, TexSampler, uvy).rgb * w.y + Texture2DSample(Tex, TexSampler, uvz).rgb * w.z;
float2x2 R = float2x2(0.8253, -0.5646, 0.5646, 0.8253);
float2 bx = mul(R, uvx) * 0.143 + 0.37, by = mul(R, uvy) * 0.143 + 0.61, bz = mul(R, uvz) * 0.143 + 0.19;
float3 big = Texture2DSample(Tex, TexSampler, bx).rgb * w.x + Texture2DSample(Tex, TexSampler, by).rgb * w.y + Texture2DSample(Tex, TexSampler, bz).rgb * w.z;
float3 avg = Texture2DSampleLevel(Tex, TexSampler, float2(0.5, 0.5), 12.0).rgb;
c *= lerp(float3(1, 1, 1), clamp(big / max(avg, 0.02), 0.45, 1.7), Mix);
float far = K.fbm(WorldPos / 3000.0), mid = K.fbm(WorldPos / 650.0 + 31.7);
c *= 1.0 + Macro * (far - 0.5) * 2.4;
float d = smoothstep(0.52, 0.70, mid * 0.75 + far * 0.25);
float g = dot(c, float3(0.3, 0.59, 0.11));
c = lerp(c, lerp(c, float3(g, g, g), 0.45) * float3(0.66, 0.62, 0.56), d * Dirt);
return c;
'''
# World-space normal from three tangent-space samples (whiteout blend): each plane's XY bends its own two world axes.
CODE_NORMAL = TRIPLANAR + '''
float3 tx = Texture2DSample(Tex, TexSampler, uvx).rgb, ty = Texture2DSample(Tex, TexSampler, uvy).rgb, tz = Texture2DSample(Tex, TexSampler, uvz).rgb;
tx.xy = tx.xy * 2 - 1; ty.xy = ty.xy * 2 - 1; tz.xy = tz.xy * 2 - 1;
tx.z = sqrt(saturate(1 - dot(tx.xy, tx.xy))); ty.z = sqrt(saturate(1 - dot(ty.xy, ty.xy))); tz.z = sqrt(saturate(1 - dot(tz.xy, tz.xy)));
float3 s = sign(n);
float3 nx = float3(tx.z * s.x, tx.x, -tx.y) ;
float3 ny = float3(ty.x, ty.z * s.y, -ty.y);
float3 nz = float3(tz.x, tz.y, tz.z * s.z);
return normalize(nx * w.x + ny * w.y + nz * w.z + n * 0.001);
'''
# Second layer of the two-layer ground (M_Kit_SurfaceBlend): patches Cover (0..1 of the area) about BlendScale cm wide.
CODE_MASK = NOISE + '''
float f = K.fbm(WorldPos / max(BlendScale, 1.0) + 5.3) * 0.7 + K.fbm(WorldPos / max(BlendScale * 0.23, 1.0)) * 0.3;
float t = 0.5 + (0.5 - Cover) * 0.55;
return smoothstep(t - 0.07, t + 0.07, f);
'''


def scalar(mat, name, value, x, y):
    e = ml.create_material_expression(mat, u.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', value)
    return e


def custom(mat, code, inputs, x, y, out=None):
    c = ml.create_material_expression(mat, u.MaterialExpressionCustom, x, y)
    c.set_editor_property('code', code)
    c.set_editor_property('output_type', out or u.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = dict(inputs)
    inputs['WorldPos'] = ml.create_material_expression(mat, u.MaterialExpressionWorldPosition, x - 300, y + 60)
    if 'Tex' in inputs: inputs['WorldNormal'] = ml.create_material_expression(mat, u.MaterialExpressionVertexNormalWS, x - 300, y + 120)
    ins = []
    for nm in inputs:
        i = u.CustomInput(); i.set_editor_property('input_name', nm); ins.append(i)
    c.set_editor_property('inputs', ins)
    for nm, e in inputs.items(): assert ml.connect_material_expressions(e, '', c, nm), nm
    return c


def build_surface_master(name, default_tex, blend):
    dest = '/Game/Kit/Surfaces'
    mat = u.load_asset(dest + '/' + name)
    if mat is None: mat = tools.create_asset(name, dest, u.Material, u.MaterialFactoryNew())
    else: ml.delete_all_material_expressions(mat)
    mat.set_editor_property('tangent_space_normal', False)
    tint = ml.create_material_expression(mat, u.MaterialExpressionVectorParameter, -200, -500)
    tint.set_editor_property('parameter_name', 'Tint'); tint.set_editor_property('default_value', u.LinearColor(1, 1, 1, 1))
    vary = {'Mix': scalar(mat, 'Mix', 0.6, -1500, -700), 'Macro': scalar(mat, 'Macro', 0.2, -1500, -620), 'Dirt': scalar(mat, 'Dirt', 0.35, -1500, -540)}
    types = (('BaseColor', u.MaterialSamplerType.SAMPLERTYPE_COLOR), ('ORM', u.MaterialSamplerType.SAMPLERTYPE_MASKS), ('Normal', u.MaterialSamplerType.SAMPLERTYPE_NORMAL))

    def layer(suffix, y0):
        tile = scalar(mat, 'Tile' + suffix, 200.0, -1500, y0 + 500)
        out = {}
        for i, (key, st) in enumerate(types):
            p = ml.create_material_expression(mat, u.MaterialExpressionTextureObjectParameter, -1500, y0 + i * 300 - 300)
            p.set_editor_property('parameter_name', key + suffix); p.set_editor_property('texture', default_tex[key])
            p.set_editor_property('sampler_type', st)
            ins = {'Tex': p, 'Tile': tile}
            if key == 'BaseColor': ins.update(vary)
            out[key] = custom(mat, CODE_COLOR if key == 'BaseColor' else CODE_NORMAL if key == 'Normal' else CODE_PLAIN, ins, -1000, y0 + i * 300 - 300)
        return out
    a = layer('', 0)
    if blend:
        b = layer('2', 1200)
        mask = custom(mat, CODE_MASK, {'BlendScale': scalar(mat, 'BlendScale', 900.0, -1000, 900), 'Cover': scalar(mat, 'Cover', 0.3, -1000, 980)},
                      -700, 900, u.CustomMaterialOutputType.CMOT_FLOAT1)
        for key in a:
            l = ml.create_material_expression(mat, u.MaterialExpressionLinearInterpolate, -500, {'BaseColor': -300, 'ORM': 0, 'Normal': 300}[key])
            assert ml.connect_material_expressions(a[key], '', l, 'A') and ml.connect_material_expressions(b[key], '', l, 'B') and ml.connect_material_expressions(mask, '', l, 'Alpha')
            a[key] = l
    mul = ml.create_material_expression(mat, u.MaterialExpressionMultiply, -200, -300)
    assert ml.connect_material_expressions(a['BaseColor'], '', mul, 'A') and ml.connect_material_expressions(tint, '', mul, 'B')
    ml.connect_material_property(mul, '', u.MaterialProperty.MP_BASE_COLOR)
    for ch, prop in (('R', u.MaterialProperty.MP_AMBIENT_OCCLUSION), ('G', u.MaterialProperty.MP_ROUGHNESS), ('B', u.MaterialProperty.MP_METALLIC)):
        m = ml.create_material_expression(mat, u.MaterialExpressionComponentMask, -200, {'R': -60, 'G': 40, 'B': 140}[ch])
        for k in 'rgba': m.set_editor_property(k, k.upper() == ch)
        assert ml.connect_material_expressions(a['ORM'], '', m, '')
        ml.connect_material_property(m, '', prop)
    if blend:
        nn = ml.create_material_expression(mat, u.MaterialExpressionNormalize, -200, 300)
        assert ml.connect_material_expressions(a['Normal'], '', nn, '')
        ml.connect_material_property(nn, '', u.MaterialProperty.MP_NORMAL)
    else: ml.connect_material_property(a['Normal'], '', u.MaterialProperty.MP_NORMAL)
    ml.set_material_usage(mat, u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)  # the house parts are drawn as instances (BattleKitHouses.cpp)
    ml.recompile_material(mat); eal.save_loaded_asset(mat)
    return mat


# Regular patterns (bricks, tiles, planks): a turned, enlarged copy of the pattern would show through, so only a little of it.
PATTERNED = {'BrickRed': 0.2, 'StoneBlock': 0.25, 'Cobble': 0.3, 'Pavement': 0.3, 'RoofClay': 0.2, 'RoofSlate': 0.2, 'MetalPlate': 0.2, 'WoodRough': 0.3}
# Ground with a second layer in patches: name -> (second surface, share of the area, patch size in cm).
# Pairs of similar brightness only: pale gravel on brown earth and pale concrete on asphalt read as snow (seen 2026-11-27).
BLENDS = {'Grass': ('DirtPath', 0.22, 1100.0), 'GrassSparse': ('Grass', 0.40, 900.0), 'FarmSoil': ('GrassSparse', 0.25, 1400.0),
          'ForestFloor': ('Grass', 0.30, 800.0), 'DirtPath': ('GrassSparse', 0.28, 700.0), 'RockGround': ('GrassSparse', 0.25, 700.0)}
# Extra instances for the house parts (Art/Kit/Parts material slots): name -> (surface, tint, tile cm).
PART_INSTANCES = {'PartFrame': ('PlasterWhite', (0.95, 0.95, 0.92), 150.0), 'PartDoor': ('WoodRough', (0.50, 0.72, 0.58), 120.0),
                  'PartShutter': ('WoodRough', (0.48, 0.62, 0.85), 120.0), 'PartCloth': ('PlasterWhite', (0.70, 0.20, 0.15), 150.0),
                  'PartDark': ('MetalPlate', (0.25, 0.25, 0.27), 150.0)}

surfaces = json.loads((KIT / 'Surfaces' / 'surfaces.json').read_text(encoding='utf-8'))
textures, tiles, masters = {}, {}, {}
for name, info in sorted(surfaces.items()):
    try:
        textures[name] = {k: import_texture(KIT / 'Surfaces' / name / info['files'][k]['file'], '/Game/Kit/Surfaces/Textures',
                                            'T_Kit_%s_%s' % (name, k), k, False) for k in ('BaseColor', 'ORM', 'Normal')}
        tiles[name] = max(50.0, float(info['size_m'][0]) / 10.0)  # Poly Haven gives the photographed size in millimetres
    except Exception as e:  # keep going: one bad surface must not stop the kit
        report['errors'].append('surface %s: %r' % (name, e))


def surface_instance(mi_name, name, tint=None, tile=None, blend=None):
    kind = 'M_Kit_SurfaceBlend' if blend else 'M_Kit_Surface'
    if kind not in masters: masters[kind] = build_surface_master(kind, textures[name], bool(blend))
    mi = u.load_asset('/Game/Kit/Surfaces/' + mi_name)
    if mi is None: mi = tools.create_asset(mi_name, '/Game/Kit/Surfaces', u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    ml.clear_all_material_instance_parameters(mi)
    ml.set_material_instance_parent(mi, masters[kind])
    for k, t in textures[name].items(): ml.set_material_instance_texture_parameter_value(mi, k, t)
    ml.set_material_instance_scalar_parameter_value(mi, 'Tile', tile or tiles[name])
    ml.set_material_instance_scalar_parameter_value(mi, 'Mix', PATTERNED.get(name, 0.6))
    if tint: ml.set_material_instance_vector_parameter_value(mi, 'Tint', u.LinearColor(tint[0], tint[1], tint[2], 1))
    if blend:
        for k, t in textures[blend[0]].items(): ml.set_material_instance_texture_parameter_value(mi, k + '2', t)
        ml.set_material_instance_scalar_parameter_value(mi, 'Tile2', tiles[blend[0]])
        ml.set_material_instance_scalar_parameter_value(mi, 'Cover', blend[1])
        ml.set_material_instance_scalar_parameter_value(mi, 'BlendScale', blend[2])
    ml.update_material_instance(mi); eal.save_loaded_asset(mi)
    return mi


for name in sorted(textures):
    try:
        blend = BLENDS.get(name)
        if blend and blend[0] not in textures: blend = None
        mi = surface_instance('MI_Kit_' + name, name, blend=blend)
        report['surfaces'][name] = {'instance': mi.get_path_name(), 'tile_cm': tiles[name], 'second_layer': blend[0] if blend else None}
    except Exception as e:
        report['errors'].append('surface %s: %r' % (name, e))
for mi_name, (name, tint, tile) in PART_INSTANCES.items():
    try: surface_instance('MI_Kit_' + mi_name, name, tint=tint, tile=tile)
    except Exception as e: report['errors'].append('part material %s: %r' % (mi_name, e))

# ---------------------------------------------------------------- painted ground (map L: BattleMapCommandlet.cpp BuildTerrainL)
# The ground mesh of a height-field map carries the old flat colours as vertex colours (grass .16 .24 .11, field .30 .27 .12,
# wood .09 .17 .07, town .21 .21 .20, road .07 .08 .09, 'S' sectors .14 .19 .24, markers and water strongly coloured).
# M_Kit_GroundPaint reads them by their colour ratios (the height shading and the contour lines only change the brightness)
# and lays the kit surfaces: Grass, ForestFloor, FarmSoil, Pavement, Asphalt, Gravel. Markers and water keep their colour.
# Top-down projection (the ground is nearly flat), colour and normal only (12 textures; roughness is a constant per layer).
GROUND_LAYERS = ('Grass', 'ForestFloor', 'FarmSoil', 'Pavement', 'Asphalt', 'Gravel')
GROUND_WEIGHTS = NOISE + '''
float3 v = max(Paint.rgb, 0.0);
float gg = max(v.g, 0.004), rg = v.r / gg, bg = v.b / gg, lum = max(v.r, max(v.g, v.b));
float jit = (K.fbm(WorldPos / 420.0) - 0.5) * 0.16 + (K.fbm(WorldPos / 90.0 + 3.1) - 0.5) * 0.06;   // ragged borders
float special = max(max(step(1.6, bg), step(rg, 0.25)), step(2.0, rg));       // water, objective and home markers
float paved = smoothstep(0.62, 0.80, bg + jit);
float road = paved * (1.0 - smoothstep(0.10, 0.13, lum));
float stone = paved * smoothstep(1.10, 1.20, bg) * (1.0 - road / max(paved, 0.001));
float town = saturate(paved - road - stone);
float soil = (1.0 - paved) * smoothstep(0.82, 0.98, rg + jit);
float wood = (1.0 - paved) * (1.0 - smoothstep(0.56, 0.63, rg + jit * 0.6));
float grass = saturate(1.0 - paved - soil - wood);
float yard = town * smoothstep(0.52, 0.62, K.fbm(WorldPos / 1400.0 + 7.3) + jit);   // lawns and worn ground between the paving of a town
float worn = (town - yard) * smoothstep(0.58, 0.70, K.fbm(WorldPos / 700.0 + 19.1));
town -= yard + worn; grass += yard; soil += worn;
float glade = wood * smoothstep(0.50, 0.62, K.fbm(WorldPos / 600.0 + 41.3)) * 0.8;   // green patches on the forest floor
wood -= glade; grass += glade;
float W[6]; W[0] = grass; W[1] = wood; W[2] = soil; W[3] = town; W[4] = road; W[5] = stone;
float expected = grass * 0.24 + wood * 0.17 + soil * 0.30 + town * 0.21 + road * 0.085 + stone * 0.24;
float shade = lerp(1.0, clamp(lum / max(expected, 0.02), 0.72, 1.12), 0.7);   // keeps the contour lines and the height shading, softly
float2 xy = WorldPos.xy;
'''
GROUND_COLOR = GROUND_WEIGHTS + '''
float2x2 R = float2x2(0.8253, -0.5646, 0.5646, 0.8253);
float3 c = float3(0, 0, 0);
float3 G[6]; G[0] = float3(0.36, 0.68, 0.30); G[1] = float3(0.50, 0.60, 0.36); G[2] = G[3] = G[4] = float3(1, 1, 1); G[5] = float3(0.27, 0.28, 0.29);   // the gravel scan is nearly white: a home base read as snow   // the grass scan is straw yellow: push it to green
#define KIT_LAYER(T, S, tile, i) { float2 uv = xy / tile; float3 a = Texture2DSample(T, S, uv).rgb; \\
  float3 big = Texture2DSample(T, S, mul(R, uv) * 0.143 + 0.37).rgb; float3 avg = Texture2DSampleLevel(T, S, float2(0.5, 0.5), 12.0).rgb; \\
  c += a * lerp(float3(1, 1, 1), clamp(big / max(avg, 0.02), 0.45, 1.7), 0.6) * W[i] * G[i]; }
%(layers)s
#undef KIT_LAYER
c *= lerp(1.0, 0.72, town);   // the pale paving read as a white floor over a whole town
float far = K.fbm(WorldPos / 3000.0), mid = K.fbm(WorldPos / 650.0 + 31.7);
c *= 1.0 + 0.2 * (far - 0.5) * 2.4;
float d = smoothstep(0.52, 0.70, mid * 0.75 + far * 0.25);
float g = dot(c, float3(0.3, 0.59, 0.11));
c = lerp(c, lerp(c, float3(g, g, g), 0.45) * float3(0.66, 0.62, 0.56), d * 0.3);
return lerp(c * shade, v, special);
'''
GROUND_NORMAL = GROUND_WEIGHTS + '''
float2 t = float2(0, 0);
#define KIT_LAYER(T, S, tile, i) { t += (Texture2DSample(T, S, xy / tile).rg * 2.0 - 1.0) * W[i]; }
%(layers)s
#undef KIT_LAYER
float3 n = normalize(WorldNormal);
return normalize(float3(n.xy + t * (1.0 - special), n.z));
'''
GROUND_ROUGH = GROUND_WEIGHTS + '''
return lerp(grass * 0.92 + wood * 0.95 + soil * 0.93 + town * 0.85 + road * 0.78 + stone * 0.9, 0.35, step(1.6, bg));
'''
try:
    if all(n in textures for n in GROUND_LAYERS):
        dest = '/Game/Kit/Surfaces'
        mat = u.load_asset(dest + '/M_Kit_GroundPaint')
        if mat is None: mat = tools.create_asset('M_Kit_GroundPaint', dest, u.Material, u.MaterialFactoryNew())
        else: ml.delete_all_material_expressions(mat)
        mat.set_editor_property('tangent_space_normal', False)
        paint = ml.create_material_expression(mat, u.MaterialExpressionVertexColor, -1500, -600)
        calls = '\n'.join('KIT_LAYER(T%d, T%dSampler, %.1f, %d)' % (i, i, tiles[n], i) for i, n in enumerate(GROUND_LAYERS))

        def ground_node(code, key, y, out=None):
            ins = {'Paint': paint}
            if key:
                for i, n in enumerate(GROUND_LAYERS):
                    t = ml.create_material_expression(mat, u.MaterialExpressionTextureObject, -1500, y + i * 90)
                    t.set_editor_property('texture', textures[n][key])
                    t.set_editor_property('sampler_type', u.MaterialSamplerType.SAMPLERTYPE_COLOR if key == 'BaseColor' else u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
                    ins['T%d' % i] = t
            c = ml.create_material_expression(mat, u.MaterialExpressionCustom, -900, y)
            c.set_editor_property('code', code % {'layers': calls} if key else code)
            c.set_editor_property('output_type', out or u.CustomMaterialOutputType.CMOT_FLOAT3)
            ins['WorldPos'] = ml.create_material_expression(mat, u.MaterialExpressionWorldPosition, -1200, y + 600)
            if key == 'Normal': ins['WorldNormal'] = ml.create_material_expression(mat, u.MaterialExpressionVertexNormalWS, -1200, y + 660)
            cis = []
            for nm in ins:
                ci = u.CustomInput(); ci.set_editor_property('input_name', nm); cis.append(ci)
            c.set_editor_property('inputs', cis)
            for nm, e in ins.items(): assert ml.connect_material_expressions(e, '', c, nm), nm
            return c
        ml.connect_material_property(ground_node(GROUND_COLOR, 'BaseColor', -600), '', u.MaterialProperty.MP_BASE_COLOR)
        ml.connect_material_property(ground_node(GROUND_NORMAL, 'Normal', 300), '', u.MaterialProperty.MP_NORMAL)
        ml.connect_material_property(ground_node(GROUND_ROUGH, None, 1200, u.CustomMaterialOutputType.CMOT_FLOAT1), '', u.MaterialProperty.MP_ROUGHNESS)
        ml.recompile_material(mat); eal.save_loaded_asset(mat)
        mi = u.load_asset(dest + '/MI_Kit_GroundPaint')   # the surface table of BattleKitLook.cpp names instances
        if mi is None: mi = tools.create_asset('MI_Kit_GroundPaint', dest, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
        ml.set_material_instance_parent(mi, mat); ml.update_material_instance(mi); eal.save_loaded_asset(mi)
        report['surfaces']['GroundPaint'] = {'instance': mi.get_path_name(), 'layers': list(GROUND_LAYERS)}
except Exception as e:
    report['errors'].append('ground paint: %r' % (e,))

# ---------------------------------------------------------------- weapons and props (baked by Art/Kit/Scripts/make_weapon.py)
samplers = {'BaseColor': u.MaterialSamplerType.SAMPLERTYPE_COLOR, 'ORM': u.MaterialSamplerType.SAMPLERTYPE_MASKS, 'Normal': u.MaterialSamplerType.SAMPLERTYPE_NORMAL}


def import_model(group, name):
    src = KIT / group / name; dest = '/Game/Kit/' + group; asset = 'SM_Kit_' + name
    fbx = src / 'Exports' / (asset + '.fbx')
    if not fbx.exists(): return None
    t = u.AssetImportTask(); t.filename = str(fbx); t.destination_path = dest; t.destination_name = asset
    t.automated = True; t.replace_existing = True; t.save = True; t.factory = u.FbxFactory()
    o = u.FbxImportUI()
    o.import_as_skeletal = False; o.automated_import_should_detect_type = False
    o.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    o.import_mesh = True; o.import_animations = False; o.import_materials = False; o.import_textures = False
    d = o.static_mesh_import_data
    d.set_editor_property('combine_meshes', True)
    d.set_editor_property('auto_generate_collision', False)
    d.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t.options = o
    tools.import_asset_tasks([t])
    mesh = u.load_asset(dest + '/' + asset); assert isinstance(mesh, u.StaticMesh), asset
    mesh.set_editor_property('allow_cpu_access', True)  # the tracer start reads the muzzle vertices (ABattleUnit::TracerStart)
    mname = 'M_Kit_' + name
    mat = u.load_asset(dest + '/' + mname)
    if mat is None: mat = tools.create_asset(mname, dest, u.Material, u.MaterialFactoryNew())
    else: ml.delete_all_material_expressions(mat)
    for i, key in enumerate(('BaseColor', 'ORM', 'Normal')):
        tex = import_texture(src / 'Textures' / ('T_Kit_%s_%s.png' % (name, key)), dest + '/Textures', 'T_Kit_%s_%s' % (name, key), key, True)  # baked in Blender: OpenGL normals
        node = ml.create_material_expression(mat, u.MaterialExpressionTextureSample, -600, i * 260)
        node.set_editor_property('texture', tex); node.set_editor_property('sampler_type', samplers[key])
        if key == 'BaseColor': ml.connect_material_property(node, 'RGB', u.MaterialProperty.MP_BASE_COLOR)
        elif key == 'Normal': ml.connect_material_property(node, 'RGB', u.MaterialProperty.MP_NORMAL)
        else:
            ml.connect_material_property(node, 'G', u.MaterialProperty.MP_ROUGHNESS)
            ml.connect_material_property(node, 'B', u.MaterialProperty.MP_METALLIC)
    # The prop dressing draws these as instances (BattleKitProps.cpp); without the flag a game build falls back to the grey default material.
    ml.set_material_usage(mat, u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    ml.recompile_material(mat)
    mesh.set_material(0, mat)
    eal.save_loaded_asset(mat); eal.save_loaded_asset(mesh)
    b = mesh.get_bounding_box()
    return {'asset': mesh.get_path_name(), 'triangles': mesh.get_num_triangles(0),
            'box_min_cm': [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)],
            'box_max_cm': [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)]}


for group, key in (('Weapons', 'weapons'), ('Props', 'props')):
    base = KIT / group
    for d in sorted(p for p in base.iterdir() if p.is_dir()) if base.exists() else []:
        try:
            r = import_model(group, d.name)
            if r: report[key][d.name] = r
        except Exception as e:
            report['errors'].append('%s %s: %r' % (group, d.name, e))

# ---------------------------------------------------------------- house parts (Art/Kit/Scripts/make_parts.py)
# Box-modelled windows, doors and roofs. No textures of their own: each material slot (named by role) gets a world-aligned kit
# surface, so BattleKitHouses.cpp can scale a part unevenly. Glass is a plain dark, shiny, opaque material.
SLOT_MATERIALS = {'Frame': 'MI_Kit_PartFrame', 'Shutter': 'MI_Kit_PartShutter', 'Door': 'MI_Kit_PartDoor', 'Trim': 'MI_Kit_Concrete',
                  'Roof': 'MI_Kit_RoofClay', 'Wall': 'MI_Kit_PlasterWhite', 'Wood': 'MI_Kit_WoodRough', 'Metal': 'MI_Kit_MetalPlate',
                  'Dark': 'MI_Kit_PartDark', 'Cloth': 'MI_Kit_PartCloth', 'Brick': 'MI_Kit_BrickRed'}
report['parts'] = {}
try:
    parts_dir = KIT / 'Parts'; parts_dest = '/Game/Kit/Parts'
    glass = u.load_asset(parts_dest + '/M_Kit_Glass')
    if glass is None: glass = tools.create_asset('M_Kit_Glass', parts_dest, u.Material, u.MaterialFactoryNew())
    else: ml.delete_all_material_expressions(glass)
    for prop, value, y in ((u.MaterialProperty.MP_BASE_COLOR, (0.03, 0.045, 0.06), 0), (u.MaterialProperty.MP_ROUGHNESS, 0.08, 150),
                           (u.MaterialProperty.MP_METALLIC, 0.6, 300), (u.MaterialProperty.MP_SPECULAR, 0.9, 450)):
        if isinstance(value, tuple):
            e = ml.create_material_expression(glass, u.MaterialExpressionConstant3Vector, -400, y); e.set_editor_property('constant', u.LinearColor(*value, 1))
        else:
            e = ml.create_material_expression(glass, u.MaterialExpressionConstant, -400, y); e.set_editor_property('r', value)
        ml.connect_material_property(e, '', prop)
    ml.set_material_usage(glass, u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    ml.recompile_material(glass); eal.save_loaded_asset(glass)
    for d in sorted(p for p in parts_dir.iterdir() if p.is_dir()) if parts_dir.exists() else []:
        try:
            asset = 'SM_Kit_' + d.name; fbx = d / 'Exports' / (asset + '.fbx')
            if not fbx.exists(): continue
            t = u.AssetImportTask(); t.filename = str(fbx); t.destination_path = parts_dest; t.destination_name = asset
            t.automated = True; t.replace_existing = True; t.save = True; t.factory = u.FbxFactory()
            o = u.FbxImportUI()
            o.import_as_skeletal = False; o.automated_import_should_detect_type = False
            o.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
            o.import_mesh = True; o.import_animations = False; o.import_materials = False; o.import_textures = False
            sd = o.static_mesh_import_data
            sd.set_editor_property('combine_meshes', True); sd.set_editor_property('auto_generate_collision', False)
            sd.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
            t.options = o
            tools.import_asset_tasks([t])
            mesh = u.load_asset(parts_dest + '/' + asset); assert isinstance(mesh, u.StaticMesh), asset
            slots = []
            for i, sm in enumerate(mesh.static_materials):
                slot = str(sm.material_slot_name); slots.append(slot)
                mat = glass if slot == 'Glass' else u.load_asset('/Game/Kit/Surfaces/' + SLOT_MATERIALS[slot])
                assert mat, slot
                mesh.set_material(i, mat)
            eal.save_loaded_asset(mesh)
            b = mesh.get_bounding_box()
            report['parts'][d.name] = {'asset': mesh.get_path_name(), 'triangles': mesh.get_num_triangles(0), 'slots': slots,
                                       'box_min_cm': [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)],
                                       'box_max_cm': [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)]}
        except Exception as e:
            report['errors'].append('part %s: %r' % (d.name, e))
except Exception as e:
    report['errors'].append('parts: %r' % (e,))

# ---------------------------------------------------------------- effect flipbooks (Art/Kit/Scripts/make_flipbook.py)
# M_Kit_FX_Flipbook: unlit translucent sprite. Parameters: Frame (0..63 of the 8x8 sheet, blended with the next frame),
# Tint (multiplies the colour), Fade (multiplies the opacity). Instances MI_Kit_FX_<Name> carry the sheet.
CODE_FLIP = '''
float f = clamp(Frame, 0.0, 63.0); float i0 = floor(f); float i1 = min(i0 + 1.0, 63.0);
float2 uv0 = (UV + float2(fmod(i0, 8.0), floor(i0 / 8.0))) / 8.0;
float2 uv1 = (UV + float2(fmod(i1, 8.0), floor(i1 / 8.0))) / 8.0;
return lerp(Texture2DSample(Tex, TexSampler, uv0), Texture2DSample(Tex, TexSampler, uv1), f - i0);
'''
try:
    fx_dir = KIT / 'Effects'; fx_dest = '/Game/Kit/Effects'
    sheets = sorted(fx_dir.glob('T_Kit_FX_*.png')) if fx_dir.exists() else []
    fx_tex = {}
    for png in sheets:
        t = u.AssetImportTask(); t.filename = str(png); t.destination_path = fx_dest; t.destination_name = png.stem
        t.automated = True; t.replace_existing = True; t.save = True
        tools.import_asset_tasks([t])
        tex = u.load_asset(fx_dest + '/' + png.stem); assert tex, png.stem
        tex.set_editor_property('srgb', True)
        tex.set_editor_property('never_stream', True)   # billboards give the streamer no size: streamed sheets stayed at low mips and every puff was a soft blob
        eal.save_loaded_asset(tex); fx_tex[png.stem[len('T_Kit_FX_'):]] = tex
    if fx_tex:
        mat = u.load_asset(fx_dest + '/M_Kit_FX_Flipbook')
        if mat is None: mat = tools.create_asset('M_Kit_FX_Flipbook', fx_dest, u.Material, u.MaterialFactoryNew())
        else: ml.delete_all_material_expressions(mat)
        mat.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('two_sided', True)
        sheet = ml.create_material_expression(mat, u.MaterialExpressionTextureObjectParameter, -1100, 0)
        sheet.set_editor_property('parameter_name', 'Sheet'); sheet.set_editor_property('texture', next(iter(fx_tex.values())))
        frame = ml.create_material_expression(mat, u.MaterialExpressionScalarParameter, -1100, 200)
        frame.set_editor_property('parameter_name', 'Frame'); frame.set_editor_property('default_value', 20.0)
        fade = ml.create_material_expression(mat, u.MaterialExpressionScalarParameter, -500, 300)
        fade.set_editor_property('parameter_name', 'Fade'); fade.set_editor_property('default_value', 1.0)
        tint = ml.create_material_expression(mat, u.MaterialExpressionVectorParameter, -500, -250)
        tint.set_editor_property('parameter_name', 'Tint'); tint.set_editor_property('default_value', u.LinearColor(1, 1, 1, 1))
        uv = ml.create_material_expression(mat, u.MaterialExpressionTextureCoordinate, -1100, 350)
        c = ml.create_material_expression(mat, u.MaterialExpressionCustom, -800, 0)
        c.set_editor_property('code', CODE_FLIP); c.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
        ins = []
        for nm in ('Tex', 'UV', 'Frame'):
            i = u.CustomInput(); i.set_editor_property('input_name', nm); ins.append(i)
        c.set_editor_property('inputs', ins)
        assert ml.connect_material_expressions(sheet, '', c, 'Tex') and ml.connect_material_expressions(uv, '', c, 'UV') and ml.connect_material_expressions(frame, '', c, 'Frame')
        rgb = ml.create_material_expression(mat, u.MaterialExpressionComponentMask, -500, -80)
        for k in 'rgba': rgb.set_editor_property(k, k in 'rgb')
        a = ml.create_material_expression(mat, u.MaterialExpressionComponentMask, -500, 120)
        for k in 'rgba': a.set_editor_property(k, k == 'a')
        assert ml.connect_material_expressions(c, '', rgb, '') and ml.connect_material_expressions(c, '', a, '')
        col = ml.create_material_expression(mat, u.MaterialExpressionMultiply, -250, -150)
        assert ml.connect_material_expressions(rgb, '', col, 'A') and ml.connect_material_expressions(tint, '', col, 'B')
        op = ml.create_material_expression(mat, u.MaterialExpressionMultiply, -250, 150)
        assert ml.connect_material_expressions(a, '', op, 'A') and ml.connect_material_expressions(fade, '', op, 'B')
        ml.connect_material_property(col, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
        ml.connect_material_property(op, '', u.MaterialProperty.MP_OPACITY)
        ml.recompile_material(mat); eal.save_loaded_asset(mat)
        report['effects'] = {}
        for name, tex in fx_tex.items():
            mi = u.load_asset(fx_dest + '/MI_Kit_FX_' + name)
            if mi is None: mi = tools.create_asset('MI_Kit_FX_' + name, fx_dest, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
            ml.set_material_instance_parent(mi, mat)
            ml.set_material_instance_texture_parameter_value(mi, 'Sheet', tex)
            ml.update_material_instance(mi); eal.save_loaded_asset(mi)
            report['effects'][name] = mi.get_path_name()
except Exception as e:
    report['errors'].append('effects: %r' % (e,))

(KIT / 'unreal-import.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
u.log('KIT_IMPORT_DONE surfaces=%d weapons=%d props=%d parts=%d errors=%d' % (len(report['surfaces']), len(report['weapons']), len(report['props']), len(report['parts']), len(report['errors'])))
for e in report['errors']: u.log_warning('KIT_IMPORT_ERROR ' + e)
u.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
