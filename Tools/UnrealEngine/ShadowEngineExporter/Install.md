# ShadowEngine Exporter

For artists. Follow the steps once. Copy the folder. Do not link it.

## 1. Copy the plugin

Copy the whole `ShadowEngineExporter` folder into the Unreal project's `Plugins` directory. The folder name stays `ShadowEngineExporter`.

```text
YourProject\Plugins\ShadowEngineExporter
```

The folder needs `ShadowEngineExporter.uplugin`, `Source`, `Config`, `Content`, and `ExportData`.

After the plugin files change, copy the folder again, close the editor, and let the project compile on the next launch.

## 2. Enable the plugin

1. Close the Unreal Editor if it is open.
2. Open the project again. Unreal compiles the plugin on startup when it is new.
3. Choose **Edit → Plugins**.
4. Search for `ShadowEngine`.
5. Enable **ShadowEngine Exporter** if it is off, then restart the editor when asked.

The plugin turns on by default the first time Unreal finds it.

## 3. Use a parent material

The plugin contains `M_Pure`, `M_Texture`, and `M_Translucent`. Choose **Edit → Project Settings → Plugins → ShadowEngine Exporter**. Pure, Texture, and Translucent already point at those three. Change the pickers there if you replace the parent materials.

A slot exports when its material is a material instance of one of those parents. The parent material used directly, an empty slot, and any other material write `Assets\EngineAsset\DefaultAsset.material.json`. The Output Log names each of those slots. The scene file is still written. Any warning turns the completion notice red.

Create the instance from the parent. In the Content Browser, right-click `M_Pure`, `M_Texture`, or `M_Translucent` and choose **Create Material Instance**. An empty instance has a **Parent** field in its details. Set that field to one of the three parents. A new material-instance dialog does not ask for the parent.

A change inside the parent material reaches every instance. A value set on an instance belongs to that instance. Actors that share one set of parameters use the same material instance.

The instance must expose these parameter names:

- Pure: `BaseColor`, `Roughness`
- Texture: `BaseColor`, `Metallic`, `Roughness`, `BaseColorTexture`, `RoughnessTexture`, `MetallicTexture`, `NormalTexture`, `OcclusionTexture`
- Translucent: `BaseColor`, `Roughness`, `Alpha`

A missing name writes the default material. `BaseColorTexture` stays sRGB. Roughness, metallic, normal, and occlusion stay linear. A normal map uses OpenGL tangent space, with the green channel pointing up. The exporter copies each texture's source image and does not bake the material graph. Lines from GLTFExporter about baking being disabled are expected.

Pure and Texture write Surface Opaque. Translucent writes Surface Translucent. `TwoSided` is written false.

## 4. Export

The export is the current editor level. You do not select actors first.

Choose **Tools → ShadowEngine → Export ShadowEngine Scene**.

Every `StaticMeshActor` is exported, including hidden ones. Actors that use the same static mesh and the same materials share one mesh file. Each actor still keeps its own position. A different material assignment on the same mesh is written as another asset and still uses that mesh file. Materials come from the actor, including a material override. A static mesh on a Blueprint, and instanced static meshes, are left out of the file.

One directional light becomes `DirectLight`. Point lights become `PointLight`. The first sky light becomes `SkyLight`. If the level has no sky light, a default sky is written with color `(0.45, 0.55, 0.75)` and intensity `0.25`. Extra directional lights and extra sky lights are skipped. Spot lights and rect lights are skipped. Cameras, skeletal meshes, foliage, and other actor types are not written.

A point light's intensity is its Unreal intensity multiplied by `0.01`. Its radius is the actor property `Radius` when that property exists, otherwise `10`. Directional and sky intensities are copied as written.

Mesh vertices and actor transforms use Unreal's glTF conversion: centimeters become meters, and the axes match the exported glTF mesh. Rotation is stored as pitch, yaw, roll in radians.

## 5. Where the files go

The engine project's `Config\Scene.json` is left unchanged. The files are written inside the copied plugin:

```text
ShadowEngineExporter\ExportData
```

Each export clears the previous files there. `.gitkeep` stays.

The folder then contains:

- `Config\Scene.json`: where each model sits, how it is rotated, and the lights
- `Assets\Mesh\<name>\Mesh\`: the model and its `.asset.json`
- `Assets\Mesh\<name>\Material\`: one material file per slot that uses a parent from the plugin settings. It contains `Principled` only. `Parent` inside that object points at `Pure`, `Texture`, or `Translucent` under the repository `Assets\BaseMaterial`
- `Assets\Mesh\<name>\Texture\`: present only for a `Texture` parent

The engine loads the material path written inside each `.asset.json`. Another `.material.json` in that folder is ignored until a mesh description names it.

`.gitkeep` is an empty file that keeps this folder in Git. Leave it in place.

## 6. View the export in the engine

The engine reads `Config\Scene.json` and the `Assets\` paths inside it from the repository root. Copy the exported pieces onto those paths:

- `ExportData\Assets\Mesh` onto the repository `Assets\Mesh`
- `ExportData\Config\Scene.json` onto the repository `Config\Scene.json`

Leave `Engine.json`, `Renderer.json`, and `GlobalTexture.json` where they are. `Principled` materials are read by both `ForwardRenderer` and `DeferredRenderer`. Run the engine from the repository root. The camera starts at `(0, 4, 14)` and looks toward the origin.
