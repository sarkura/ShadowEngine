# ShadowEngine Exporter

For artists. Follow the steps once.

## 1. Copy the add-on

Copy the whole `ShadowEngineExporter` folder. It needs these three items:

- `__init__.py`
- `blender_manifest.toml`
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

## 3. Export

The export is the current scene. You do not select meshes, and you do not export a Collection.

1. Stay in **Object Mode**. Export does nothing in Edit Mode.
2. Choose **File → Export → ShadowEngine (.glb)**.

You can also press `N`, open the **ShadowEngine** tab, and click **Export Scene**.

Every mesh in the scene is exported. Names that differ only by a numeric suffix, such as `Sphere` and `Sphere.001`, share one mesh and one material. Each object still keeps its own position. A Sun becomes the directional light. Point lights and the world background are written as well. Cameras, empties, curves, armatures, spot lights, and area lights are skipped. A hidden object is exported when it is still in the scene.

## 4. Where the files go

The engine project’s `Config\Scene.json` is left unchanged. The files are written inside the copied add-on:

```text
ShadowEngineExporter\ExportData
```

Each export clears the previous files there. `.gitkeep` stays.

The folder then contains:

- `Config\Scene.json`: where each model sits, how it is rotated, and the lights
- `Assets\<name>\Mesh\`: the model and its `.asset.json`
- `Assets\<name>\Material\`: one material file per model. It contains `ForwardRenderer` only. `Parent` inside that object points at `ForwardPure`, `ForwardTexture`, or `ForwardTranslucent` under the repository `Assets\BaseMaterial`. The file stores `Surface`, `TwoSided`, and the parameter values. Shader paths and parameter types stay on those base materials, which are not part of this export
- `Assets\<name>\Texture\`: present only when a material uses images

`.gitkeep` is an empty file that keeps this folder in Git. Leave it in place.

## 5. View the export in the engine

The engine reads `Config\Scene.json` and the `Assets\` paths inside it from the repository root. Copy the exported pieces onto those paths:

- `ExportData\Assets\<each model>` onto the repository `Assets\`
- `ExportData\Config\Scene.json` onto the repository `Config\Scene.json`

Leave `Engine.json`, `Renderer.json`, and `GlobalTexture.json` where they are. `Renderer.json` stays on `ForwardRenderer`, because the exported materials contain that binding. Run the engine from the repository root. The camera starts at `(0, 4, 14)` and looks toward the origin.
