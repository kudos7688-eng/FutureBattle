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
TRIPLANAR = '''
float3 p = WorldPos / max(Tile, 1.0);
float3 n = normalize(WorldNormal);
float3 w = pow(abs(n), 4.0); w /= (w.x + w.y + w.z);
float2 uvx = p.yz * float2(1, -1), uvy = p.xz * float2(1, -1), uvz = p.xy;
'''
CODE_COLOR = TRIPLANAR + '''
return Texture2DSample(Tex, TexSampler, uvx).rgb * w.x + Texture2DSample(Tex, TexSampler, uvy).rgb * w.y + Texture2DSample(Tex, TexSampler, uvz).rgb * w.z;
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


def custom(mat, code, tex_param, tile, x, y):
    c = ml.create_material_expression(mat, u.MaterialExpressionCustom, x, y)
    c.set_editor_property('code', code)
    c.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for nm in ('Tex', 'WorldPos', 'WorldNormal', 'Tile'):
        i = u.CustomInput(); i.set_editor_property('input_name', nm); ins.append(i)
    c.set_editor_property('inputs', ins)
    wp = ml.create_material_expression(mat, u.MaterialExpressionWorldPosition, x - 300, y + 60)
    vn = ml.create_material_expression(mat, u.MaterialExpressionVertexNormalWS, x - 300, y + 120)
    assert ml.connect_material_expressions(tex_param, '', c, 'Tex')
    assert ml.connect_material_expressions(wp, '', c, 'WorldPos')
    assert ml.connect_material_expressions(vn, '', c, 'WorldNormal')
    assert ml.connect_material_expressions(tile, '', c, 'Tile')
    return c


def build_surface_master(default_tex):
    dest, name = '/Game/Kit/Surfaces', 'M_Kit_Surface'
    mat = u.load_asset(dest + '/' + name)
    if mat is None: mat = tools.create_asset(name, dest, u.Material, u.MaterialFactoryNew())
    else: ml.delete_all_material_expressions(mat)
    mat.set_editor_property('tangent_space_normal', False)
    tile = ml.create_material_expression(mat, u.MaterialExpressionScalarParameter, -1100, 500)
    tile.set_editor_property('parameter_name', 'Tile'); tile.set_editor_property('default_value', 200.0)
    tint = ml.create_material_expression(mat, u.MaterialExpressionVectorParameter, -500, -200)
    tint.set_editor_property('parameter_name', 'Tint'); tint.set_editor_property('default_value', u.LinearColor(1, 1, 1, 1))
    params = {}
    for i, (key, st) in enumerate((('BaseColor', u.MaterialSamplerType.SAMPLERTYPE_COLOR),
                                   ('ORM', u.MaterialSamplerType.SAMPLERTYPE_MASKS),
                                   ('Normal', u.MaterialSamplerType.SAMPLERTYPE_NORMAL))):
        p = ml.create_material_expression(mat, u.MaterialExpressionTextureObjectParameter, -1100, i * 300 - 300)
        p.set_editor_property('parameter_name', key); p.set_editor_property('texture', default_tex[key])
        p.set_editor_property('sampler_type', st); params[key] = p
    col = custom(mat, CODE_COLOR, params['BaseColor'], tile, -700, -300)
    orm = custom(mat, CODE_COLOR, params['ORM'], tile, -700, 0)
    nrm = custom(mat, CODE_NORMAL, params['Normal'], tile, -700, 300)
    mul = ml.create_material_expression(mat, u.MaterialExpressionMultiply, -300, -300)
    assert ml.connect_material_expressions(col, '', mul, 'A') and ml.connect_material_expressions(tint, '', mul, 'B')
    ml.connect_material_property(mul, '', u.MaterialProperty.MP_BASE_COLOR)
    mask = {}
    for ch, prop in (('R', u.MaterialProperty.MP_AMBIENT_OCCLUSION), ('G', u.MaterialProperty.MP_ROUGHNESS), ('B', u.MaterialProperty.MP_METALLIC)):
        m = ml.create_material_expression(mat, u.MaterialExpressionComponentMask, -300, {'R': -60, 'G': 40, 'B': 140}[ch])
        for k in 'rgba': m.set_editor_property(k, k.upper() == ch)
        assert ml.connect_material_expressions(orm, '', m, '')
        ml.connect_material_property(m, '', prop)
    ml.connect_material_property(nrm, '', u.MaterialProperty.MP_NORMAL)
    ml.recompile_material(mat); eal.save_loaded_asset(mat)
    return mat


surfaces = json.loads((KIT / 'Surfaces' / 'surfaces.json').read_text(encoding='utf-8'))
master = None
for name, info in sorted(surfaces.items()):
    try:
        tex = {k: import_texture(KIT / 'Surfaces' / name / info['files'][k]['file'], '/Game/Kit/Surfaces/Textures',
                                 'T_Kit_%s_%s' % (name, k), k, False) for k in ('BaseColor', 'ORM', 'Normal')}
        if master is None: master = build_surface_master(tex)
        mi_name = 'MI_Kit_' + name
        mi = u.load_asset('/Game/Kit/Surfaces/' + mi_name)
        if mi is None: mi = tools.create_asset(mi_name, '/Game/Kit/Surfaces', u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
        ml.set_material_instance_parent(mi, master)
        for k in tex: ml.set_material_instance_texture_parameter_value(mi, k, tex[k])
        tile_cm = max(50.0, float(info['size_m'][0]) / 10.0)  # Poly Haven gives the photographed size in millimetres
        ml.set_material_instance_scalar_parameter_value(mi, 'Tile', tile_cm)
        ml.update_material_instance(mi); eal.save_loaded_asset(mi)
        report['surfaces'][name] = {'instance': mi.get_path_name(), 'tile_cm': tile_cm}
    except Exception as e:  # keep going: one bad surface must not stop the kit
        report['errors'].append('surface %s: %r' % (name, e))

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
u.log('KIT_IMPORT_DONE surfaces=%d weapons=%d props=%d errors=%d' % (len(report['surfaces']), len(report['weapons']), len(report['props']), len(report['errors'])))
for e in report['errors']: u.log_warning('KIT_IMPORT_ERROR ' + e)
u.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
