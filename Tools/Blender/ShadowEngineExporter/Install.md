# ShadowEngine Exporter

For artists. Follow the steps once.

## 1. Copy the add-on

Copy the whole `ShadowEngineExporter` folder. It needs these items:

- `__init__.py`
- `blender_manifest.toml`
- `Content`
- `Config`
- `ExportData`

Open File Explorer, paste this line into the address bar, and press Enter:

```text
%APPDATA%\Blender Foundation\Blender\5.2\extensions\user_default
```

If that folder does not exist, create it. Starting from `Blender Foundation`, add `Blender`, `5.2`, `extensions`, and `user_default`.

Copy `ShadowEngineExporter` into `user_default` without renaming it. The result is:

```text
%APPDATA%\Blender Foundation\Blender\5.2\extensions\user_default\ShadowEngineExporter
```

After the add-on files change, copy the folder again and restart Blender. The copy inside the extensions folder is the one Blender runs.

## 2. Enable the add-on

1. Open Blender 5.2. If it was already open, close it and open it again.
2. Choose **Edit → Preferences**.
3. On the left, choose **Add-ons**. Stay off **Get Extensions**. Installed on that page only means the files are present. It has no enable switch.
4. If the list shows only enabled add-ons, turn off **Enabled Only**.
5. Search for `ShadowEngine`.
6. Click the checkbox beside **ShadowEngine Exporter**. A check mark means it is on.

If the add-on is missing, return to Get Extensions and confirm it is listed under Installed, and that the copied folder is still named `ShadowEngineExporter`. Quit Blender completely, then open it again.

Enabling the add-on copies any missing parent group into the file that is open at that moment. Another `.blend` file does not receive those groups on its own.

## 3. Bring the parent groups into a file

The three groups live in `Content\ShadowEngineMaterials.blend`: `ShadowEngine_Pure`, `ShadowEngine_Texture`, and `ShadowEngine_Translucent`. Use these groups. Leave each socket type as it is. The texture inputs stay **Color**. A shader node group has no Image socket.

1. Choose **File → Append**.
2. Open `ShadowEngineMaterials.blend` in the installed add-on's `Content` folder.
3. Open **NodeTree**.
4. Append `ShadowEngine_Pure`, `ShadowEngine_Texture`, and `ShadowEngine_Translucent`.

Leave the group as one shared datablock. **Make Single User** renames it, for example to `ShadowEngine_Texture.001`. `Config\ParentMaterials.json` then no longer matches that name, and the slot uses the default material.

## 4. Assign a parent to a material

Blender has no material-instance asset. The node group is the parent. Each material stores its own numbers and textures on the group node inside that material. Objects that share one set of parameters use the same Blender material. Editing the nodes inside the group changes every material that uses it. Editing the values on one material changes only that material.

1. Select the object and open the **Shading** workspace.
2. Create or choose a material.
3. Press **Shift+A**, choose **Group**, then choose one of the three groups.
4. Connect the group's **BSDF** output to **Material Output → Surface**.
5. Set `BaseColor` and `Roughness` on that group node. `ShadowEngine_Texture` also has `Metallic`. `ShadowEngine_Translucent` also has `Alpha`. Leave these sockets unconnected. A connected value socket exports as the default material.
6. For `ShadowEngine_Texture`, add **Image Texture** nodes outside the group. Connect each node's **Color** output to `BaseColorTexture`, `RoughnessTexture`, `MetallicTexture`, `NormalTexture`, or `OcclusionTexture`. On the image node, set the base-color **Color Space** to **sRGB**. Set roughness, metallic, normal, and occlusion to **Non-Color**. A normal map uses Blender tangent space, with the green channel pointing up.
7. For `ShadowEngine_Translucent`, set the material **Render Method** to **Blended**.

`Config\ParentMaterials.json` maps those group names to the ShadowEngine parent materials. A material exports when the active **Material Output** surface comes from one of these groups and every required socket is present. Anything else uses `Assets\EngineAsset\DefaultAsset.material.json`, and the warning names that material.

The groups write these parameters:

- `ShadowEngine_Pure`: `BaseColor`, `Roughness`. Surface is Opaque.
- `ShadowEngine_Texture`: `BaseColor`, `Metallic`, `Roughness`, and the five textures above. Surface is Opaque. All five images are required.
- `ShadowEngine_Translucent`: `BaseColor`, `Roughness`, `Alpha`. Surface is Translucent.

`TwoSided` is written false.

## 5. Export

The export is the current scene. You do not select meshes, and you do not export a Collection.

1. Stay in **Object Mode**. Export does nothing in Edit Mode.
2. Choose **File → Export → ShadowEngine (.glb)**. The 3D Viewport **ShadowEngine** tab has the same command.

Every mesh in the scene is exported. Names that differ only by a numeric suffix, such as `Sphere` and `Sphere.001`, share one mesh file and the materials of the object whose name has no suffix. Each object still keeps its own position. An object with a different name is exported on its own, even when it uses the same mesh data. Two slots on that shared mesh whose material names differ only by the suffix write one material file, and the later slot replaces the earlier one.

A Sun becomes the directional light. A second Sun is skipped. Point lights and the world background are written as well. A point light's intensity is its watt value multiplied by **Point Energy Scale** (default `0.01`). Its radius is the object property `Radius`, or **Point Radius** (default `10`) when that property is absent. Cameras, empties, curves, armatures, spot lights, and area lights are skipped. A hidden object is exported when it is still in the scene.

## 6. Where the files go

The engine project's `Config\Scene.json` is left unchanged. The files are written inside the copied add-on:

```text
ShadowEngineExporter\ExportData
```

Each export clears the previous files there. `.gitkeep` stays.

The folder then contains:

- `Config\Scene.json`: where each model sits, how it is rotated, and the lights
- `Assets\Mesh\<name>\Mesh\`: the model and its `.asset.json`
- `Assets\Mesh\<name>\Material\`: one material file per slot that uses a parent group. It contains `Principled` only
- `Assets\Mesh\<name>\Texture\`: present only for `ShadowEngine_Texture`

The engine loads the material path written inside each `.asset.json`. Another `.material.json` in that folder is ignored until a mesh description names it.

`.gitkeep` is an empty file that keeps this folder in Git. Leave it in place.

## 7. View the export in the engine

The engine reads `Config\Scene.json` and the `Assets\` paths inside it from the repository root. Copy the exported pieces onto those paths:

- `ExportData\Assets\Mesh` onto the repository `Assets\Mesh`
- `ExportData\Config\Scene.json` onto the repository `Config\Scene.json`

Leave `Engine.json`, `Renderer.json`, and `GlobalTexture.json` where they are. `Principled` materials are read by both `ForwardRenderer` and `DeferredRenderer`. Run the engine from the repository root. The camera starts at `(0, 4, 14)` and looks toward the origin.
