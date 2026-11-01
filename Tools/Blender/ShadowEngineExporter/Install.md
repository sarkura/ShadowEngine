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

## 2. Enable the add-on

1. Open Blender 5.2. If it was already open, close it and open it again.
2. Choose **Edit → Preferences**.
3. On the left, choose **Add-ons**. Stay off **Get Extensions**. Installed on that page only means the files are present. It has no enable switch.
4. If the list shows only enabled add-ons, turn off **Enabled Only**.
5. Search for `ShadowEngine`.
6. Click the checkbox beside **ShadowEngine Exporter**. A check mark means it is on.

If the add-on is missing, return to Get Extensions and confirm it is listed under Installed, and that the copied folder is still named `ShadowEngineExporter`. Quit Blender completely, then open it again.

## 3. Use a parent material

The add-on loads three node groups from `Content\ShadowEngineMaterials.blend`: `ShadowEngine_Pure`, `ShadowEngine_Texture`, and `ShadowEngine_Translucent`. Do not build these nodes by hand.

1. Select the object and open the **Shading** workspace.
2. Press **Shift+A**, choose **Group**, then choose one of those three groups.
3. Connect the group's **BSDF** output to **Material Output → Surface**.
4. Change the values on that group node. Those values belong to this material. The group itself stays shared.
5. For `ShadowEngine_Texture`, add **Image Texture** nodes outside the group and connect them to `BaseColorTexture`, `RoughnessTexture`, `MetallicTexture`, `NormalTexture`, and `OcclusionTexture`. Set the base-color image to **sRGB**. Set the other images to **Non-Color**.
6. For `ShadowEngine_Translucent`, set the material **Render Method** to **Blended**.

`Config\ParentMaterials.json` maps those group names to the ShadowEngine parent materials. A material exports only when its surface output is one of these groups. Anything else uses `Assets\EngineAsset\DefaultAsset.material.json`, and the warning names that material.

## 4. Export

The export is the current scene. You do not select meshes, and you do not export a Collection.

1. Stay in **Object Mode**. Export does nothing in Edit Mode.
2. Choose **File → Export → ShadowEngine (.glb)**.

Every mesh in the scene is exported. Names that differ only by a numeric suffix, such as `Sphere` and `Sphere.001`, share one mesh and one material. Each object still keeps its own position. A Sun becomes the directional light. Point lights and the world background are written as well. Cameras, empties, curves, armatures, spot lights, and area lights are skipped. A hidden object is exported when it is still in the scene.

## 5. Where the files go

The engine project’s `Config\Scene.json` is left unchanged. The files are written inside the copied add-on:

```text
ShadowEngineExporter\ExportData
```

Each export clears the previous files there. `.gitkeep` stays.

The folder then contains:

- `Config\Scene.json`: where each model sits, how it is rotated, and the lights
- `Assets\Mesh\<name>\Mesh\`: the model and its `.asset.json`
- `Assets\Mesh\<name>\Material\`: one material file per slot that uses a parent group. It contains `Principled` only
- `Assets\Mesh\<name>\Texture\`: present only for `ShadowEngine_Texture`

`.gitkeep` is an empty file that keeps this folder in Git. Leave it in place.

## 6. View the export in the engine

The engine reads `Config\Scene.json` and the `Assets\` paths inside it from the repository root. Copy the exported pieces onto those paths:

- `ExportData\Assets\Mesh` onto the repository `Assets\Mesh`
- `ExportData\Config\Scene.json` onto the repository `Config\Scene.json`

Leave `Engine.json`, `Renderer.json`, and `GlobalTexture.json` where they are. `Principled` materials are read by both `ForwardRenderer` and `DeferredRenderer`. Run the engine from the repository root. The camera starts at `(0, 4, 14)` and looks toward the origin.
