# ShadowEngine Exporter for Blender 5.2.
# Install from Blender: Edit > Preferences > Get Extensions > Install from Disk,
# and choose this folder (or a zip of it). Then use File > Export > ShadowEngine.
# Results are written under this add-on's ExportData directory.

import json
import re
import shutil
from pathlib import Path

import bpy
from bpy.props import FloatProperty
from bpy.types import Operator, Panel
from mathutils import Matrix, Vector

BLENDER_TO_GLTF = Matrix((
    (1.0, 0.0, 0.0, 0.0),
    (0.0, 0.0, 1.0, 0.0),
    (0.0, -1.0, 0.0, 0.0),
    (0.0, 0.0, 0.0, 1.0),
))

ADDON_DIR = Path(__file__).resolve().parent
PARENT_BLEND = ADDON_DIR / "Content" / "ShadowEngineMaterials.blend"
PARENT_CONFIG = ADDON_DIR / "Config" / "ParentMaterials.json"
DEFAULT_MATERIAL = "Assets/EngineAsset/DefaultAsset.material.json"
ASSET_MESH = Path("Assets") / "Mesh"

def rounded(value):
    number = round(float(value), 6)
    if number == 0.0:
        return 0.0
    return number


def color3(values):
    return [rounded(values[0]), rounded(values[1]), rounded(values[2])]


def clean_name(name):
    cleaned = bpy.path.clean_name(name).strip()
    return cleaned if cleaned else "Object"


def shared_model_name(name):
    return clean_name(re.sub(r"\.\d{3}$", "", name))


def relative_asset(path):
    return path.as_posix()


def clear_export_directory(root):
    root.mkdir(parents=True, exist_ok=True)
    for child in root.iterdir():
        if child.name == ".gitkeep":
            continue
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()


def write_json(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=4) + "\n", encoding="utf-8")


def linked_image(socket, depth=0):
    if socket is None or not socket.is_linked or depth > 4:
        return None
    node = socket.links[0].from_node
    if node.type == "TEX_IMAGE" and node.image is not None:
        return node.image
    if node.type == "NORMAL_MAP" and "Color" in node.inputs:
        return linked_image(node.inputs["Color"], depth + 1)
    return None


def load_parent_map():
    payload = json.loads(PARENT_CONFIG.read_text(encoding="utf-8"))
    parents = {}
    for key, entry in payload.get("Principled", {}).items():
        blender_name = entry.get("Blender", "")
        shadow = entry.get("ShadowEngine", "")
        if not blender_name or not shadow:
            continue
        parents[blender_name] = {
            "kind": key,
            "shadow": shadow,
            "surface": "Translucent" if key == "Translucent" else "Opaque",
        }
    return parents


def ensure_parent_groups():
    if not PARENT_BLEND.is_file():
        return
    existing = set(bpy.data.node_groups.keys())
    with bpy.data.libraries.load(str(PARENT_BLEND), link=False) as (data_from, data_to):
        data_to.node_groups = [name for name in data_from.node_groups if name not in existing]


def parent_group(material, parents):
    if material is None or not material.use_nodes or material.node_tree is None:
        return None, None
    for node in material.node_tree.nodes:
        if node.type != "OUTPUT_MATERIAL" or not node.is_active_output:
            continue
        surface = node.inputs.get("Surface")
        if surface is None or not surface.is_linked:
            return None, None
        source = surface.links[0].from_node
        if source.type == "GROUP" and source.node_tree is not None and source.node_tree.name in parents:
            return source, parents[source.node_tree.name]
    return None, None


def unlinked_color(node, name):
    socket = node.inputs.get(name)
    if socket is None or socket.is_linked:
        return None
    return tuple(socket.default_value[:3])


def unlinked_float(node, name):
    socket = node.inputs.get(name)
    if socket is None or socket.is_linked:
        return None
    value = socket.default_value
    if hasattr(value, "__len__"):
        return None
    return float(value)


def texture_encoding(image, color_map):
    if color_map and image.colorspace_settings.name == "sRGB":
        return "sRGB"
    return "Linear"


def to_engine_transform(matrix_world):
    converted = BLENDER_TO_GLTF @ matrix_world @ BLENDER_TO_GLTF.inverted()
    location, rotation, scale = converted.decompose()
    euler = rotation.to_euler("ZXY")
    return (
        [rounded(location.x), rounded(location.y), rounded(location.z)],
        [rounded(euler.x), rounded(euler.y), rounded(euler.z)],
        [rounded(scale.x), rounded(scale.y), rounded(scale.z)],
    )


def to_gltf_vector(vector):
    return Vector((vector.x, vector.z, -vector.y))


def save_image_copy(image, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    source = Path(bpy.path.abspath(image.filepath)) if image.filepath else None
    if (
        image.packed_file is None
        and source is not None
        and source.is_file()
        and source.suffix.lower() in {".png", ".jpg", ".jpeg"}
    ):
        target = destination.with_suffix(source.suffix.lower())
        if source.resolve() != target.resolve():
            shutil.copyfile(source, target)
        return target

    target = destination.with_suffix(".png")
    copy = image.copy()
    copy.filepath_raw = str(target)
    copy.file_format = "PNG"
    copy.save()
    bpy.data.images.remove(copy)
    return target


def texture_document(image_path, encoding):
    return {
        "Type": "Texture",
        "Source": relative_asset(image_path),
        "Encoding": encoding,
    }


class SHADOWENGINE_OT_export(Operator):
    bl_idname = "export_scene.shadow_engine"
    bl_label = "ShadowEngine"
    bl_description = (
        "Export the current scene. Meshes, the sun, point lights, and the world light are written. "
        "Other object types are skipped"
    )
    bl_options = {"REGISTER"}

    point_radius: FloatProperty(
        name="Point Radius",
        description="Reach of a point light when the light object has no Radius property",
        default=10.0,
        min=0.001,
    )
    point_energy_scale: FloatProperty(
        name="Point Energy Scale",
        description="Blender point-light watts multiplied into ShadowEngine intensity",
        default=0.01,
        min=0.0,
    )

    @classmethod
    def poll(cls, context):
        return context.mode == "OBJECT"

    def execute(self, context):
        try:
            parents = load_parent_map()
        except (OSError, json.JSONDecodeError) as error:
            self.report({"ERROR"}, f"Could not read ParentMaterials.json: {error}")
            return {"CANCELLED"}
        if not parents:
            self.report({"ERROR"}, "ParentMaterials.json has no Principled parents.")
            return {"CANCELLED"}

        root = Path(__file__).resolve().parent / "ExportData"
        clear_export_directory(root)

        meshes = [obj for obj in context.scene.objects if obj.type == "MESH"]

        warnings = []
        scene_meshes = {}
        groups = {}
        for obj in meshes:
            groups.setdefault(shared_model_name(obj.name), []).append(obj)
        for name, objects in groups.items():
            source = objects[0]
            for obj in objects:
                if re.search(r"\.\d{3}$", obj.name) is None:
                    source = obj
                    break
            asset_path, warning = self.export_object(context, root, source, name, parents)
            if asset_path is None:
                self.report({"ERROR"}, warning)
                return {"CANCELLED"}
            if warning:
                warnings.append(warning)
            for obj in objects:
                translation, rotation, scale = to_engine_transform(obj.matrix_world)
                scene_meshes.setdefault(asset_path, []).append({
                    "Transform": {
                        "Translation": translation,
                        "Rotation": rotation,
                        "Scale": scale,
                    }
                })

        lights, light_warnings = self.export_lights(context)
        warnings.extend(light_warnings)
        skipped = self.skipped_objects(context.scene)
        if skipped:
            warnings.append("Skipped " + ", ".join(skipped) + ".")
        scene_path = root / "Config" / "Scene.json"
        write_json(scene_path, {"Mesh": scene_meshes, "Light": lights})

        message = f"Exported {len(groups)} mesh(es), {len(meshes)} instance(s) to {scene_path}"
        if warnings:
            self.report({"WARNING"}, " ".join(warnings))
        self.report({"INFO"}, message)
        return {"FINISHED"}

    def export_object(self, context, root, obj, name, parents):
        mesh_dir = ASSET_MESH / name / "Mesh"
        glb_relative = mesh_dir / f"{name}.glb"
        asset_relative = mesh_dir / f"{name}.asset.json"
        glb_path = root / glb_relative
        glb_path.parent.mkdir(parents=True, exist_ok=True)

        view_layer = context.view_layer
        previous_active = view_layer.objects.active
        previous_selected = [item for item in context.selected_objects]
        temporary = obj.copy()
        temporary.data = obj.data
        temporary.parent = None
        temporary.matrix_world = Matrix.Identity(4)
        context.collection.objects.link(temporary)
        try:
            for item in context.selectable_objects:
                item.select_set(False)
            temporary.select_set(True)
            view_layer.objects.active = temporary
            with context.temp_override(active_object=temporary, selected_objects=[temporary]):
                result = bpy.ops.export_scene.gltf(
                    filepath=str(glb_path),
                    export_format="GLB",
                    use_selection=True,
                    export_yup=True,
                    export_apply=True,
                    export_materials="PLACEHOLDER",
                    export_texcoords=True,
                    export_normals=True,
                    export_animations=False,
                )
        except Exception as error:
            return None, f"glTF export failed for {obj.name}: {error}"
        finally:
            bpy.data.objects.remove(temporary)
            for item in previous_selected:
                if item.name in bpy.data.objects:
                    item.select_set(True)
            if previous_active is not None and previous_active.name in bpy.data.objects:
                view_layer.objects.active = previous_active

        if "FINISHED" not in result:
            return None, f"glTF export failed for {obj.name}"

        slots = list(obj.material_slots)
        if not slots:
            slots = [None]
        materials = []
        warnings = []
        for index, slot in enumerate(slots):
            material = None if slot is None else slot.material
            material_name = shared_model_name(material.name) if material is not None else f"{name}_{index}"
            material_relative = ASSET_MESH / name / "Material" / f"{material_name}.material.json"
            material_path, slot_warning = self.write_material(root, material_relative, material, parents)
            if slot_warning:
                warnings.append(slot_warning)
            materials.append({
                "slot": index,
                "material": material_path,
            })

        write_json(root / asset_relative, {
            "type": "Mesh",
            "source": relative_asset(glb_relative),
            "materials": materials,
        })
        return relative_asset(asset_relative), " ".join(warnings)

    def write_material(self, root, material_relative, material, parents):
        label = material.name if material is not None else "empty"
        group, parent = parent_group(material, parents)
        if group is None:
            return DEFAULT_MATERIAL, (
                f"{label} does not use a Principled parent group. The default material is used."
            )

        base_color = unlinked_color(group, "BaseColor")
        roughness = unlinked_float(group, "Roughness")
        if base_color is None or roughness is None:
            return DEFAULT_MATERIAL, (
                f"{label} is missing BaseColor or Roughness on {group.node_tree.name}. "
                "The default material is used."
            )

        parameters = {
            "BaseColor": {"float3": color3(base_color)},
            "Roughness": {"float": rounded(roughness)},
        }
        if parent["kind"] == "Texture":
            metallic = unlinked_float(group, "Metallic")
            if metallic is None:
                return DEFAULT_MATERIAL, (
                    f"{label} is missing Metallic on {group.node_tree.name}. The default material is used."
                )
            parameters["Metallic"] = {"float": rounded(metallic)}
            texture_dir = material_relative.parent.parent / "Texture"
            for socket_name, color_map in (
                ("BaseColorTexture", True),
                ("RoughnessTexture", False),
                ("MetallicTexture", False),
                ("NormalTexture", False),
                ("OcclusionTexture", False),
            ):
                image = linked_image(group.inputs.get(socket_name))
                if image is None:
                    return DEFAULT_MATERIAL, (
                        f"{label} is missing {socket_name} on {group.node_tree.name}. "
                        "The default material is used."
                    )
                saved = save_image_copy(image, root / texture_dir / clean_name(image.name))
                relative_image = saved.relative_to(root)
                description = relative_image.with_suffix(".texture.json")
                write_json(
                    root / description,
                    texture_document(relative_image, texture_encoding(image, color_map)),
                )
                parameters[socket_name] = {"texture": relative_asset(description)}
        elif parent["kind"] == "Translucent":
            alpha = unlinked_float(group, "Alpha")
            if alpha is None:
                return DEFAULT_MATERIAL, (
                    f"{label} is missing Alpha on {group.node_tree.name}. The default material is used."
                )
            parameters["Alpha"] = {"float": rounded(alpha)}

        write_json(root / material_relative, {
            "Type": "Material",
            "Principled": {
                "Parent": parent["shadow"],
                "Surface": parent["surface"],
                "TwoSided": False,
                "Parameters": parameters,
            },
        })
        return relative_asset(material_relative), ""

    def export_lights(self, context):
        lights = []
        warnings = []
        wrote_sun = False
        for obj in context.scene.objects:
            if obj.type != "LIGHT":
                continue
            light = obj.data
            if light.type == "SUN":
                if wrote_sun:
                    warnings.append(f"Skipped extra sun {obj.name}; the scene allows one DirectLight.")
                    continue
                direction = to_gltf_vector(-(obj.matrix_world.to_3x3() @ Vector((0.0, 0.0, 1.0))))
                if direction.length <= 0.0001:
                    warnings.append(f"Skipped sun {obj.name}; its direction is zero.")
                    continue
                direction.normalize()
                lights.append({
                    "Type": "DirectLight",
                    "Direction": color3(direction),
                    "Color": color3(light.color),
                    "Intensity": rounded(light.energy),
                })
                wrote_sun = True
            elif light.type == "POINT":
                position = to_gltf_vector(obj.matrix_world.translation)
                radius = float(obj["Radius"]) if "Radius" in obj else self.point_radius
                lights.append({
                    "Type": "PointLight",
                    "Position": color3(position),
                    "Color": color3(light.color),
                    "Intensity": rounded(max(light.energy, 0.0) * self.point_energy_scale),
                    "Radius": rounded(max(radius, 0.001)),
                })
            else:
                continue

        sky_color, sky_intensity = world_sky(context.scene.world)
        lights.append({
            "Type": "SkyLight",
            "Color": color3(sky_color),
            "Intensity": rounded(max(sky_intensity, 0.0)),
        })
        return lights, warnings

    def skipped_objects(self, scene):
        grouped = {}
        for obj in scene.objects:
            if obj.type == "MESH":
                continue
            if obj.type == "LIGHT" and obj.data.type in {"SUN", "POINT"}:
                continue
            grouped.setdefault(obj.type, []).append(obj.name)
        labels = {
            "CAMERA": "camera",
            "EMPTY": "empty",
            "CURVE": "curve",
            "SURFACE": "surface",
            "META": "metaball",
            "FONT": "text",
            "ARMATURE": "armature",
            "LATTICE": "lattice",
            "SPEAKER": "speaker",
            "LIGHT": "unsupported light",
            "LIGHT_PROBE": "light probe",
            "VOLUME": "volume",
            "GREASEPENCIL": "grease pencil",
            "CURVES": "curves",
            "POINTCLOUD": "point cloud",
        }
        summary = []
        for obj_type, names in grouped.items():
            label = labels.get(obj_type, obj_type.lower())
            if len(names) == 1:
                summary.append(f"{label} {names[0]}")
            else:
                summary.append(f"{len(names)} {label}s")
        return summary


def world_sky(world):
    color = (0.45, 0.55, 0.75)
    intensity = 0.25
    if world is None:
        return color, intensity
    color = tuple(world.color)
    if world.use_nodes and world.node_tree is not None:
        for node in world.node_tree.nodes:
            if node.type == "BACKGROUND":
                if "Color" in node.inputs:
                    color = tuple(node.inputs["Color"].default_value[:3])
                if "Strength" in node.inputs:
                    intensity = float(node.inputs["Strength"].default_value)
                break
    return color, intensity


class SHADOWENGINE_PT_export(Panel):
    bl_label = "ShadowEngine"
    bl_idname = "SHADOWENGINE_PT_export"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "ShadowEngine"

    def draw(self, context):
        layout = self.layout
        layout.operator(SHADOWENGINE_OT_export.bl_idname, text="Export Scene")
        layout.label(text="Writes the scene into ExportData.")
        layout.label(text="Unsupported objects are skipped.")


def menu_export(self, context):
    self.layout.operator(SHADOWENGINE_OT_export.bl_idname, text="ShadowEngine (.glb)")


CLASSES = (
    SHADOWENGINE_OT_export,
    SHADOWENGINE_PT_export,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_file_export.append(menu_export)
    ensure_parent_groups()


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_export)
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
