# ShadowEngine

ShadowEngine is a minimal cross-platform render engine. One platform implementation is selected at build time. The launch module drives that implementation through a shared application contract.

```text
Launch
  -> IApplication
       -> WindowsApplication     Windows
       -> EmptyApplication       every other platform
```

Windows creates a native window from the viewport configuration and renders into it with Direct3D 12. Every other platform reports that it is unsupported and exits.

On Windows, startup runs this sequence:

```text
main                     registers the D3D12 RHI and the Slang shader compiler
 -> WindowsApplication   loads Config/, creates the Win32 window
 -> Engine               creates the RHI device selected in Renderer.json, then the swap chain
 -> ShaderManager        loads slang-compiler.dll
 -> AssetManager         loads each Scene.json mesh through DescriptionParser, then the glTF source, per-renderer material bindings, and any textures
 -> Scene                one entity per instance, holding a MeshHandle and material-instance handles, plus directional, point, and sky lights
 -> AssetManager         loads the GlobalTexture.json environment map with stb's HDR loader
 -> Renderer             Renderer.json selects BlinnPhongRenderer, DebugRenderer, ForwardRenderer, or DeferredRenderer. The base syncs a render proxy, resolves the handles, and uploads one GPU batch per material slot that binds the active renderer. Each draw gets a 1024-byte constant slot and a depth buffer. Forward and Deferred upload the sky pipeline
 -> UIManager           one context for the selected renderer. Deferred and Forward keep separate Post, Light, and Insight parameters. BlinnPhong and Debug keep Insight only
 -> every frame          update the camera, sync the proxy, then Renderer::RenderFrame. It builds the ImGui frame, collects visible items, calls PrepareFrame, ensures UIRender, fills the pass context, runs that renderer's pass, and calls StoreFrameHistory. BlinnPhong and Debug leave the scene hooks empty and draw straight to the back buffer, then draw the interface. Forward uses the hooks for the depth prepass, environment cube, image-based lighting, shadows, and post. Deferred uses them for the G-buffer, the same environment, shadows, and post, then lights in a fullscreen pass. Both draw the interface after post
                         shaders are compiled from Shaders/ on this launch and kept in memory. Register deduplicates the vertex, fragment, material, and light paths together with the parameter names declared for that binding
                         the frame-rate number is an ImGui window drawn by UIRender into the back buffer
```

Rendering is split into a backend-agnostic core and pluggable backends. The core defines the render hardware interface (RHI) and the shader data model. Direct3D 12 and Vulkan implement the RHI. Shader compilation lives in its own module, which loads the Slang compiler at runtime instead of linking against it.

```text
ShadowEngine.exe
 ├── WindowsPlatform ── ImGuiWin32
 ├── RHID3D12 ────────── d3d12 / dxgi
 ├── RHIVulkan           empty implementation
 └── ShaderCompiler ──── Slang headers only
         ┆ loaded at runtime
         ↓
   slang-compiler.dll
```

Every module above links `Framework`. `Framework` depends on none of them.

## Building

The project targets the MSYS2 UCRT64 toolchain: GCC, Ninja, and CMake 3.20 or newer.

```sh
cmake --preset ucrt64
cmake --build --preset ucrt64
```

The executable is written to `build/bin`. The build copies the Slang runtime DLLs next to it. Static libraries are written to `build/lib`.

The engine reads `Config/` relative to the working directory, so run it from the repository root.

## Layout

```text
Assets/
  BaseMaterial/          Shared material parents and their parameter schemas
  EngineAsset/           Default material and default textures
  Environment/           HDR used by the sky background
  Mesh/                  One folder per model
    Cube/Mesh/           .asset.json and .glb
    Cube/Material/       .material.json
    Table/Texture/       One .texture.json per image. Encoding is sRGB or Linear. Mips are generated in memory at load
Config/                  Runtime configuration
Shaders/                 Slang shader sources, compiled at runtime
  Common/                Per-object data, vertex layout, descriptor-heap helpers, Blinn-Phong, Principled, and forward constants
  Material/              EvaluateMaterial for each surface
  Light/                 EvaluateLight for Blinn-Phong and Forward, Deferred fullscreen lighting, shared image-based lighting, and PCF shadow sampling
  BlinnPhong/            Blinn-Phong vertex and fragment entry points
  Principled/            Principled vertex entry point shared by Forward and Deferred
  Forward/               Forward fragment entry point
  Deferred/              G-buffer fragment. It packs a MaterialSample and does not read material parameters
  Depth/                 Forward depth prepass, plus the directional and point-light shadow shaders Forward and Deferred run
  ImageBasedLighting/    Irradiance, prefiltered environment, and BRDF lookup generation
  DebugRenderer/         Debug vertex and fragment entry points
  Sky/                   Fullscreen environment background
  PostProcess/           Motion, TAA, bloom, exposure, and tonemap shared by Forward and Deferred
  ImGui/                 Interface vertex and fragment entry points
Source/
  Interface/             Contracts shared by every module
  Framework/             Backend-agnostic engine core
    Asset/               Mesh, texture, and material assets, description data, and handles
    Common/              Base application, logging, shared types, third-party implementations
    Config/              Startup settings and description-file parsing
    Engine/              Engine entry object
    Material/            Materials, instances, parameter layout, and parameter meaning
    Render/              Renderer, camera, render proxy, queue, passes, sky background, and UIRender
    RHI/                 Render hardware interface abstraction
    Scene/               Scene, entities, transforms, and lights
    Shader/              Shader handles and shader management
    UI/                  UIManager data, per-pipeline parameters, and the ImGui frame. Drawing stays in Render/UIRender
  RHI/
    Direct3D12/          Direct3D 12 RHI backend
    Vulkan/              Vulkan RHI backend
  ShaderCompiler/        Slang shader compiler
  Platform/
    Windows/             Win32 application
    Empty/               Unsupported-platform application
  Launch/                Process entry point
  ThirdParty/            Vendored libraries
Tools/
  Blender/ShadowEngineExporter/          Blender 5.2 add-on. Writes ExportData next to the installed copy
  UnrealEngine/ShadowEngineExporter/     Unreal Editor plugin. Writes ExportData next to the copied plugin
```

Implementation modules keep their API in `Public` and their `.cpp` files and internal headers in `Private`. Include paths start at `Source/`, for example `#include "RHI/Direct3D12/Public/D3D12Device.h"`.

Abstractions and their implementations follow one pattern. `Interface` declares `IApplication` and `Platform/*` implements it. `Framework/RHI` declares the RHI and `RHI/*` implements it.

## Scene exporters

Both exporters write the same scene bundle: `Config/Scene.json`, one `.glb` plus `.asset.json` per mesh, a `Principled` `.material.json` per exported slot, and `.texture.json` files for a Texture parent. Each export clears that tool's `ExportData` and leaves `.gitkeep`. The engine keeps reading `Config/` and `Assets/` from the repository root, so a preview copies `ExportData/Assets/Mesh` onto `Assets/Mesh` and `ExportData/Config/Scene.json` onto `Config/Scene.json`. `Engine.json`, `Renderer.json`, and `GlobalTexture.json` stay as they are. `LoadMesh` follows the material path stored in each `.asset.json`. Another `.material.json` in that folder is ignored until a mesh description names it.

A written material contains `Principled` only. `Parent` sits inside that object and names `Assets/BaseMaterial/Pure.material.json`, `Texture.material.json`, or `Translucent.material.json`. The instance writes `Surface`, `TwoSided`, and `Parameters`. Shader paths and the parameter schema stay on the parent. Forward and Deferred both read that block. `BlinnPhong` and `Debug` are not written. `Surface` and `TwoSided` come from the parent kind: Pure and Texture are Opaque, Translucent is Translucent, and `TwoSided` is false. A slot that does not match a parent, or that is missing a required parameter or texture, stores `Assets/EngineAsset/DefaultAsset.material.json` and records a warning. The scene file is still written.

Pure writes `BaseColor` and `Roughness`. Texture adds `Metallic`, `BaseColorTexture`, `RoughnessTexture`, `MetallicTexture`, `NormalTexture`, and `OcclusionTexture`. Translucent adds `Alpha`. `BaseColorTexture` is `sRGB` when the source image is marked sRGB, and `Linear` otherwise. The other four textures are `Linear`. In `TextureMaterial`, albedo is the sampled color times `BaseColor`, roughness is the sampled red channel times `Roughness`, and metallic is the sampled red channel times `Metallic`. Occlusion is stored on its own and is not multiplied into albedo. A normal map is tangent space with the green channel pointing up. The engine reconstructs the tangent frame from the position and the texture coordinates.

One directional light becomes `DirectLight`. Its intensity is copied as authored. A second one is skipped, because a second `DirectLight` in `Scene.json` fails to load. Point lights become `PointLight`. Intensity is the source value multiplied by `0.01`. Radius is the custom property `Radius`, or `10` when that property is absent. One `SkyLight` is always present. Its color and intensity scale image-based lighting. The sky background still samples the HDR named by `GlobalTexture.json`. With no sky source, the color is `(0.45, 0.55, 0.75)` and the intensity is `0.25`. A second `SkyLight` in `Scene.json` fails to load.

### Blender

`Tools/Blender/ShadowEngineExporter` is a Blender 5.2 add-on. Its manifest id is `ShadowEngineExporter`, and the installed folder uses that same name. Artist steps are in `Tools/Blender/ShadowEngineExporter/Install.md`. `Content/ShadowEngineMaterials.blend` holds the node groups `ShadowEngine_Pure`, `ShadowEngine_Texture`, and `ShadowEngine_Translucent`. `Config/ParentMaterials.json` maps those names to the three parent materials. Enabling the add-on appends a missing group into the file that is open at that moment. Another `.blend` file receives the groups through **File → Append → NodeTree**.

`File → Export → ShadowEngine (.glb)` and the 3D Viewport **ShadowEngine** tab both export the whole scene in Object Mode. Hidden objects that remain in the scene are included. Cameras, empties, curves, armatures, spot lights, and area lights are skipped. Object names that differ only by a Blender suffix such as `.001` share one `.glb`, one `.asset.json`, and the materials of the object whose name has no suffix. Each object still writes its own transform. An object with a different name is a separate export, even when the mesh data matches. Two slots whose material names collapse to the same cleaned name write one material file, and the later slot replaces the earlier one. An empty material slot is named from the model and the slot index, such as `Cube_0`.

A material matches when the active Material Output surface is driven by a group node whose node-tree name is in `ParentMaterials.json`. Blender has no material-instance asset. The node group is the parent, and each material stores its own socket values. `BaseColor`, `Roughness`, `Metallic`, and `Alpha` are read from unconnected sockets. Texture images are Image Texture nodes outside the group, with their Color output linked into the group's Color sockets. A linked value socket, a missing image, or an unrecognized group selects the default material.

One Sun becomes `DirectLight`. Its direction is the light's local -Z after the axis conversion below. The world Background color and Strength become the one `SkyLight`.

Blender is Z-up. The exporter converts a transform into the engine's Y-up space before writing it: `(x, y, z)` becomes `(x, z, -y)`. Rotation is the converted ZXY Euler in radians, stored as pitch, yaw, roll. The glTF file is exported from a temporary copy with an identity transform, so the node transform is not baked into the vertices and `Scene.json` places each instance.

### Unreal

`Tools/UnrealEngine/ShadowEngineExporter` is an Unreal Editor plugin for 5.8. Artist steps are in `Tools/UnrealEngine/ShadowEngineExporter/Install.md`. Copy it into the project's `Plugins` folder. The plugin contains the parent materials `M_Pure`, `M_Texture`, and `M_Translucent`. **Project Settings → Plugins → ShadowEngine Exporter** stores those three references. The menu entry is **Tools → ShadowEngine → Export ShadowEngine Scene**.

The export walks `StaticMeshActor`s in the open level, including hidden actors. A static mesh on a Blueprint, and instanced static meshes, are left out. Actors that share one static mesh and the same material list share one `.asset.json`. Each actor still writes its own transform. A different material assignment on that mesh writes another `.asset.json` and reuses the `.glb`. Materials come from the actor, including a component override.

A slot matches when the material is a `UMaterialInstance` whose base material is one of the three parents loaded from the plugin settings. The parent material used on its own selects the default material. Parameters are read by the exact names listed above. The texture source image is copied. Material baking stays off, so a GLTFExporter line about baking being disabled is expected. Any warning is also written to `LogShadowEngineExport` and turns the completion notice red.

Mesh vertices use the glTF exporter scale of `0.01`, so centimeters become meters. Actor translation, rotation, and scale use `FGLTFCoreUtilities`, the same conversion as that mesh. Rotation is stored as pitch, yaw, roll in radians. The first sky light becomes `SkyLight`. Spot lights and rect lights are skipped.

## Modules

The startup path above is implemented: `Common`, `Config`, `Engine`, `Asset`, `Scene`, `Material`, `Render`, `UI`, `RHI`, `Shader`, the D3D12 backend and `ShaderCompiler`. The Vulkan backend is still a skeleton. Dear ImGui builds the settings panel and the frame-rate readout. `UIManager` owns that data, and `UIRender` draws it with the same Slang pipeline as the rest of the frame. The sections below describe the responsibility of each module.

### Interface

Defines the contracts used across the engine.

- `IRuntimeModule` is the lifecycle shared by runtime systems: initialize, tick, and finalize.
- `IApplication` is the application contract. It adds quit state to that lifecycle.
- `IInterface` and `IImplements` are the macros used to declare these contracts.

### Framework

Contains everything that does not depend on a graphics API, a shader compiler, or an operating system. It is one static library.

- `Common` provides `BaseApplication` and its default lifecycle, plus logging, shared types, and `NonCopyable`. `ThirdPartyImpl.cpp` is the single translation unit that compiles the stb and cgltf implementations.
- `Config` owns runtime settings. See [Config](#config).
- `Engine` is the composition root. `EngineModules` is the registry where backends and the shader compiler register factories at startup, so `Framework` never names a concrete implementation. `Engine` creates the device, swap chain, shader manager, asset manager, scene and renderer. It does not parse description files or pack material parameters. Each frame it updates the camera, asks the renderer to sync the proxy from the scene, and ticks the renderer.
- `Asset` loads descriptions through `DescriptionParser`, then loads the glTF source and any textures. A `.asset.json` names the glTF source and one or more material slots. Each slot becomes one mesh section and stores a local AABB taken from its vertex positions. A `.material.json` has `Type` and one object per surface (`Principled`, `BlinnPhong`, `Debug`). `Principled` is shared by Forward and Deferred. `BlinnPhong` and `Debug` map to `BlinnPhongRenderer` and `DebugRenderer` when the material loads. Each renderer object may name `Parent`, a path to the base material for that renderer. The loader resolves that parent, then overlays this object's parameters. A value whose name or type is not in the parent schema fails to load. It checks the result against the shader layout and creates one `MaterialInstance`. `Parent` is only a description-file relationship. `MaterialInstance` stores a `MaterialHandle` and the merged parameter block, and it does not keep a parent instance. `Material` stores the shader and reflected layout for each renderer, and it stores no parameter values. A BlinnPhong base supplies `Vertex`, `Fragment`, `Material`, an optional `Light`, and `Parameters` as type names, for example `"BaseColor": "float3"` or `"BaseColorTexture": "texture"`. A Principled base supplies `Vertex`, `Material`, `Surface`, `TwoSided`, and `Parameters`, and does not store `Fragment` or `Light`. The bases are `Assets/BaseMaterial/Pure.material.json`, `Texture.material.json`, `Translucent.material.json`, `BlinnPhongPure.material.json`, `BlinnPhongTexture.material.json`, and `Debug.material.json`. `Debug` declares no parameters. A surface with no `Parent` still supplies its own shader paths. After the parent merge, `Principled` expands into `ForwardRenderer` and `DeferredRenderer`. Opaque and cutout Deferred compile `Deferred/GBufferFragment.slang` and no light file. Translucent Deferred, and every Forward binding, compile `Forward/ForwardFragment.slang` and `Light/ForwardLight.slang`. `BlinnPhong` maps to `BlinnPhongRenderer` and `Debug` maps to `DebugRenderer`. `Surface` is `Opaque`, `Cutout`, or `Translucent`. The three surface values are mutually exclusive. `TwoSided` false culls back faces. `TwoSided` true keeps both sides and the fragment flips the normal on a back face. BlinnPhong and Debug ignore both fields. Their mesh pipelines cull back faces with the same rule. On a base material each parameter is a type name: `float`, `float2`, `float3`, `float4`, or `texture`. On an instance each parameter is one typed value of that same type, for example `"BaseColor": {"float3": [1, 1, 1]}` or `"OcclusionTexture": {"texture": "Assets/Mesh/Table/Texture/AO.texture.json"}`. The JSON key is the shader field name. The parent schema is the set of names an instance may set. A `texture` value names a `.texture.json` with `Type`, `Source`, and `Encoding` (`sRGB` or `Linear`). `TextureAsset` stores that encoding. sRGB mip levels are averaged in linear light and stored encoded again. Linear images are averaged as stored. An sRGB texture is uploaded as an RGBA8 sRGB view, so sampling returns linear color and the material shader does not decode it again. Linear images use an RGBA8 view. `LoadEnvironmentMap` reads the HDR named by `GlobalTexture.json` with `stbi_loadf` and stores one linear RGBA32-float image, without a mip chain. Assets are exposed as `MeshHandle`, `TextureAssetHandle`, and `SamplerHandle`. `Asset` calls `MaterialParameter` to build a parameter block for each renderer binding, compiles that binding, and checks the reflected layout against the block. `ShaderManager::Register` runs once per renderer binding. A draw reads only the block named by the active renderer. If a binding fails to compile or its resources do not match the reflected layout, the mesh slot logs the error, loads `Renderer.json`'s `DefaultMaterial`, and rewrites that slot in the `.asset.json`. It does not decide what material fields mean.
- `Scene` holds entities, transforms, directional lights, point lights, and sky lights. An entity stores a `MeshHandle` and the `MaterialInstanceHandle` of each material slot. `Scene.json` groups instances under a mesh description path. Each instance has translation, rotation and scale. Each light has a `Type`. The scene has one directional light. It stores direction, color, and intensity, and a second one fails to load. A point light also stores position and radius, attenuates by inverse square, and reaches zero at its radius. The scene has one sky light. It stores color and intensity, and a second one fails to load. The directional vector is the direction rays travel. The scene exposes this state and does not build a render proxy, load assets, or draw.
- `Material` owns what a material is. `Material` stores one `ShaderHandle` per renderer name. `MaterialInstance` holds a `BaseMaterial`, one `MaterialParameterBlock` per renderer name, and the `Surface` stored on the Forward and Deferred bindings. `Surface` chooses the pass. It is not a shader uniform. `BaseMaterial` is a `MaterialHandle` and points at those shaders. Scalar values live in the block by JSON key. Texture parameters are `TextureAssetHandle` values, and every textured binding shares one `SamplerHandle`. `MaterialParameter` builds the block from the parsed binding and builds `MaterialParameterLayout` from shader reflection. `MaterialAsset` owns the `Material`. This module does not parse JSON, compile shaders, or upload GPU resources.
- `Render` owns the renderer base, `BlinnPhongRenderer`, `DebugRenderer`, `ForwardRenderer`, `DeferredRenderer`, the fly camera, the render proxy, the render queue, the render passes, and `UIRender`. `Renderer::Sync` reads entities, material-instance handles, and lights from the scene and rebuilds the proxy. The proxy carries a `MeshHandle`, material-instance handles, the one directional light, the point-light list, and the one sky light. The base resolves those handles and uploads one `MeshBatch` per material slot that has a shader for the active renderer. `Renderer::RenderFrame` is shared by BlinnPhong, Debug, Forward, and Deferred. It calls `UIManager::BeginFrame`, then `FillRenderQueue`, which builds a `RenderItem` for each accepted slot and skips a slot whose material has no shader for the active renderer. After the frustum test it calls `PrepareFrame`, ensures `UIRender`, fills `RenderPassContext` through `PopulateContext`, runs that renderer's pass, then calls `StoreFrameHistory`. Those three hooks are empty for BlinnPhong and Debug. Forward overrides them for the depth-prepass pipelines, the environment cube, image-based lighting, shadow maps, post, point-light selection, projection jitter, and the previous world matrices. Deferred overrides them for the same environment cube, image-based lighting, shadow maps, post, point-light selection, jitter, and previous matrices, and it also creates the G-buffer and the fullscreen light pipeline. BlinnPhong and Debug therefore still draw straight to the back buffer, and their shading shaders stay on the original Blinn-Phong and debug paths. `RenderItemFilter::Accept` currently accepts every slot. `Renderer::CollectVisibleItems` then drops a mesh section whose world AABB misses the unjittered view frustum. Every renderer uses that step, so later draws only see what remains. `Renderer::SelectVisiblePointLights` keeps point lights whose spheres intersect the same frustum. Forward and Deferred each ask for four. The directional light and the sky light are single and are not frustum tested. Previous world matrices are stored for every scene entity and read by that entity index, so a section that leaves and re-enters the frustum still receives its own previous matrix. The first frame copies the current matrices into the previous fields. `BlinnPhongRenderPass`, `ForwardRenderPass`, and `DeferredRenderPass` write per-object data, then scalar values and descriptor indices at the offsets reported by reflection. `ForwardRenderPass` also writes the shared previous view-projection and world matrices, the directional light, at most four point lights inside the view frustum, and the sky light at those reflected offsets. `DeferredRenderPass` writes that same light block only for a binding whose layout contains it, which is the translucent Forward shader. Opaque and cutout G-buffer draws write the Principled prefix only. `DebugRenderPass` writes the view, world, and normal matrices, and writes any scalar values and descriptor indices the debug material declared. `ForwardRenderPass` and `DeferredRenderPass` record shadow maps before their camera passes. The directional light gets one 2048 orthographic depth map fitted to the casters. Each point light that survived the frustum test gets a 1024 cube, up to four, and the cube stores linear distance over the light radius. Casters are the opaque and cutout sections that have a batch for the active renderer, including sections outside the camera frustum. Translucent sections are not drawn into the shadow map, and their shading writes invalid shadow-map indices so direct and point lights stay fully lit. A cutout section clips shadow pixels with the same `Opacity` and `CutoutThreshold` as the camera passes. Single-sided casters cull back faces. Two-sided casters keep both sides. Shading multiplies GGX by a 3×3 percentage-closer filter through a comparison sampler. A light with no map stays fully lit. Image-based lighting and the sky are not shadowed. BlinnPhong and Debug do not draw these maps. `EvaluateMaterial` returns a `MaterialSample`: albedo, metallic, roughness, occlusion, emissive, world normal, `Opacity`, and `CutoutThreshold`. That result is not the JSON parameter block. `MaterialSurfaceData` is the input scalars. A textured material samples its maps first. Base color is multiplied by `Surface.BaseColor`. A normal map is converted from tangent space to world space before it leaves `EvaluateMaterial`. Opaque materials leave `Opacity` at one and `CutoutThreshold` at zero. `ForwardRenderPass` then draws opaque and cutout sections in the depth prepass with color writes disabled. A cutout section uses its material pixel shader for that prepass and for the later color draw, so both discard a pixel when `CutoutThreshold` is greater than zero and `Opacity` is below it. Translucent sections skip the prepass. The color pass clears scene color without clearing depth, draws the sky, then shades opaque and cutout sections with an equal depth test and depth writes disabled. Translucent sections draw after that, far to near, with a less-equal depth test, no depth write, and alpha blending. `DeferredRenderPass` does not run that prepass. It clears the G-buffer and depth, then draws opaque and cutout into three targets: `GBufferA` is `R8G8B8A8_UNorm` with linear albedo and metallic, `GBufferB` is `R16G16B16A16_Float` with the world normal, and `GBufferC` is `R8G8B8A8_UNorm` with roughness and occlusion. `Opacity` and `CutoutThreshold` clip while the G-buffer is written and are not stored. Emissive stays zero and is not stored. The sky is then drawn into RGBA16 scene color without reading the G-buffer. A fullscreen pass reads the three targets and depth, reconstructs world position from depth and the inverse view-projection, skips pixels whose depth is still the clear value, and writes GGX direct light, point light, and image-based sky light. It does not clear scene color and does not write depth. Translucent meshes draw after that into the same scene color, far to near, with a less-equal test, no depth write, and alpha blending. They use the Forward fragment and do not cast or receive shadows. `DrawSkyBackground` samples the environment cubemap into that RGBA16 target for both renderers. Direct and point lights use GGX. On the first frame Forward and Deferred render the equirectangular HDR into six RGBA32 cube faces, one quarter of the panorama width on each side, then release that source. The sky light's color and intensity scale image-based lighting convolved from the cubemap: irradiance, four prefiltered levels, and a BRDF lookup. Those results stay small equirectangular maps. Convolution clamps sample luminance to 16 so the sun does not saturate the half-float maps. The sky background samples the cubemap and keeps the original HDR range. BlinnPhong and Debug do not build this cubemap. `EvaluateIBL` is the shared function. Forward calls it from `ForwardLight.slang`. Deferred calls it from `DeferredLight.slang`. Occlusion scales only the diffuse image-based term. Both then run one post stack: camera and mesh motion, TAA, bloom, and automatic exposure from the average log luminance, Reinhard, an sRGB encode, and a half-step dither into the UNORM back buffer. Shading, the sky, image-based lighting, TAA, and bloom all stay in linear scene color. The swap chain stays UNORM, so only the tonemap applies the encode. BlinnPhong and Debug draw straight to the back buffer. Sampled textures and samplers are allocated into shader-visible descriptor heaps that the shader indexes directly. Each object draw uses a 1024-byte constant slot. `RenderFrame` calls `UIManager::BeginFrame` before the queue, then `UIRender::Ensure` after `PrepareFrame`. `PopulateContext` passes that `UIRender` to the pass. Forward and Deferred call `UIRender::Draw` after post. BlinnPhong and Debug call it after the mesh pass. The draw uploads the font as an RGBA8 texture, writes ImGui vertices into a persistently mapped upload buffer, and uses one 256-byte constant slot per ImGui command for the projection, texture index, and sampler index. The pipeline is `Shaders/ImGui/ImGui.slang`, alpha blended, with depth off. It does not parse JSON or interpret material field names on its own.
- `RHI` declares the device, adapter, swap chain, command list, fence, texture, sampler, shader, pipeline, descriptor, and buffer abstractions that backends implement. Vertex, index and constant buffers, sampled textures, and samplers addressed by descriptor indices are in use.
- `Shader` holds shader handles and the shader manager. It declares `IShaderCompiler` but does not know about materials. `ShaderManager::Register` deduplicates the vertex, fragment, material, and light paths together with that binding's scalar and texture field names. `LoadShaders` compiles `VertexMain` from the vertex file. When the binding has a material file, the pixel compile first emits `MaterialSurfaceData` and `MaterialDescriptorIndices` from those field names. It then includes `Common/ForwardConstants.slang` or `Common/ForwardTexturedConstants.slang` for `ForwardFragment`, and `Common/PrincipledConstants.slang` or `Common/PrincipledTexturedConstants.slang` for `GBufferFragment` and the cutout shadow fragments. The material file, the light file when the binding has one, and the fragment file follow, and the compile looks up `PixelMain`. The Principled prefix is per-object data plus the surface parameters, so the shared vertex still matches. Forward constants add the lights after that prefix. Slang reflection of that compile is the parameter layout. Bytecode stays in memory. There is no shader cache on disk.
- `UI` owns the interface data. `UIManager::Initialize` creates the ImGui context for the renderer named in `Renderer.json` and keeps a separate parameter block for that pipeline. Deferred and Forward each have Post, Light, and Insight. BlinnPhong and Debug have Insight only. The values live in `UIConfigManager` for the session and are not written back to JSON. `BeginFrame` always builds the frame-rate window when its switch is on. The settings panel is built only while it is visible. The panel order is Insight, then Light, then Post. `GetState` copies the active Deferred or Forward block into the snapshot that shadows, the sky, image-based lighting, and post read. BlinnPhong and Debug leave that snapshot at its defaults because those passes do not read it. This module does not create pipelines or submit GPU work.

`BaseApplication` loads configuration before the selected platform continues initialization.

### RHI Backends

Each backend is a separate static library. It implements `Framework/RHI` and owns the graphics API it needs. The interface is drawn by `UIRender` in `Framework`, and the Win32 input backend stays in `WindowsPlatform`.

| Module | Host | Links | Status |
| --- | --- | --- | --- |
| `RHID3D12` | Windows | `d3d12`, `dxgi`, `dxguid` | Device, swap chain, command list, fence, pipeline, vertex, index and constant buffers, RGBA8, RGBA8 sRGB, and RGBA32-float sampled textures, RGBA8 and RGBA16-float color targets, up to eight render targets, directly indexed descriptor heaps, depth, cube depth, clamp, point-clamp, and comparison samplers, render target clear and present |
| `RHIVulkan` | Every host | Nothing yet | Empty implementation, not registered, no Vulkan SDK required |

Each backend exposes one registration function, such as `RegisterD3D12RHI()`, which `main` calls before the application starts. The D3D12 backend requires feature level 12_0 and Shader Model 6.8. The executable exports `D3D12SDKVersion` (`619`) and `D3D12SDKPath` (`.\D3D12\`) from `Source/Launch/D3D12/Private/D3D12Agility.cpp`, so the loader uses the Agility SDK runtime copied to `build/bin/D3D12/` (`D3D12Core.dll` and `d3d12SDKLayers.dll`) instead of the inbox `d3d12.dll`. It picks the high-performance hardware adapter that supports feature level 12_0, then checks the shader model. If either requirement fails, the engine logs the reason and the process exits. Sampled textures and samplers live in two shader-visible descriptor heaps. The root signature is a single `b0` constant buffer, with `CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED` and `SAMPLER_HEAP_DIRECTLY_INDEXED`. The descriptor heaps are set before that root signature. A draw indexes the heaps from the constant buffer. The backend consumes DXIL bytecode:

```text
.slang -> Slang -> DXIL (SM 6.8) -> D3D12
```

The engine requests `SLANG_DXIL` directly from the Slang API and passes the returned
bytecode to D3D12. It does not ask Slang for HLSL and does not invoke DXC itself.

The swap chain follows the window size. When the window is resized, the engine waits for the GPU and resizes the back buffers. While the window is minimized, rendering pauses and the application sleeps until the next window message.

When `DebugLayer` is enabled, the backend enables the D3D12 debug layer and forwards its warnings and errors to the engine log. Frames are fully synchronized: the CPU waits for the GPU after every present.

### ShaderCompiler

Compiles Slang shaders from `Shaders/` when a mesh or the sky is uploaded, and on the first frame when `UIRender` loads `Shaders/ImGui/ImGui.slang`. It uses only the Slang headers and loads `slang-compiler.dll` at runtime, so the engine has no link-time dependency on Slang. There is no shader cache: each launch reads the `.slang` sources again, and the bytecode is not written to disk. The compile roots are the vertex and fragment files named by the active renderer binding. Before the pixel includes, the compiler emits `MaterialSurfaceData` from the binding's `float`, `float2`, `float3`, and `float4` keys, and `MaterialDescriptorIndices` from its `texture` keys plus one shared `Sampler`. The pixel compile then includes the material file, the light file when the binding has one, and the fragment file, so `PixelMain` can call `EvaluateMaterial` and `EvaluateLight`. A material shader reads scalars as `Surface.<JsonKey>` and textures as `Descriptors.<JsonKey>`. A local `MaterialSurfaceData` in a material file is only there so the editor can resolve those names; the compile uses the struct generated from JSON. Cube, sphere, and Suzanne share one Blinn-Phong path when their parameter names match. Principled uses `PureMaterial` for Plane, Sphere, Suzanne, and the default material, `TranslucentMaterial` for Cube, and `TextureMaterial` for the table. Forward and Deferred both compile those materials. The textured material samples the textures named in its JSON through `LoadResourceDescriptor` and `LoadSamplerDescriptor` in `Shaders/Common/DescriptorHeap.slang`. Those helpers emit `ResourceDescriptorHeap` and `SamplerDescriptorHeap`. The indices live in the `b0` constant buffer. `Shaders/Material` evaluates the surface into a `MaterialSample`. `Shaders/Light` evaluates lighting and returns a light sample. `GBufferFragment` packs that sample and does not call `EvaluateLight`. `Shaders/BlinnPhong`, `Shaders/Principled`, `Shaders/Forward`, and `Shaders/Deferred` are the entry points. `Shaders/DebugRenderer` maps the world normal into color, and can also read `Surface` and `Descriptors` when that debug binding declares parameters. `Shaders/Sky` draws the environment map as the background. A mesh section whose material has no binding for the renderer selected in `Renderer.json` is not uploaded and is not drawn. A binding whose shader fails to compile, or whose reflected resources are missing, is replaced by `DefaultMaterial`.

The module implements `IShaderCompiler` and registers it with `RegisterSlangShaderCompiler()`. It resolves `slang_createGlobalSession` from the library and reaches everything else through Slang's COM interfaces. A vertex compile loads the module named after the vertex file. A pixel compile builds one source that prepends the generated parameter structs, then includes the material file, the light file when the binding has one, and the fragment file, loads that as a module, finds `PixelMain`, links it, and returns the bytecode. The sky shaders load as ordinary vertex and fragment modules. Slang diagnostics are included in the error message or logged as warnings.

`ShaderCompiler` is the only module that owns shader compiler runtimes. It declares the Slang DLLs and Slang's Windows DXIL backend dependencies in its `SHADOW_RUNTIME_DLLS` target property, and the executable copies whatever that property lists. The RHI backends never see compiler implementation details. They only report the bytecode format they consume through `RHIDevice::GetShaderFormat()`, such as DXIL for D3D12.

### Platform

Exactly one platform library is built.

| Host | Module | Responsibility |
| --- | --- | --- |
| Windows | `WindowsPlatform` | Creates and runs the Win32 window |
| Other systems | `EmptyPlatform` | Reports that the platform is unsupported |

Both modules supply the global application object consumed by Launch. Launch does not depend on a concrete platform type.

`WindowsApplication` uses the viewport configuration for the initial window client size and passes the window to the engine. Each tick it processes window messages first, measures the frame time, and renders only while the window is open. It forwards `WM_SIZE` to the engine and renders a frame for each one, so the image stays correct while the window is dragged. It ends the main loop when the window closes, and it releases the engine before destroying the window. `EmptyApplication` reports `Platform Unsupported` and then quits.

The camera is a fly camera. It starts at `(0, 4, 14)`, looking at the origin. **W** and **S** move along the view, **A** and **D** strafe, **E** moves up along world Y, and **Q** moves down. The mouse changes yaw and pitch. Speeds come from `MovementSetting` in `Engine.json`. The settings panel starts visible, so the cursor is not captured until **U** hides it. **Esc** releases the cursor and does not change the panel. A click captures it again only while the panel is hidden. **U** shows or hides the panel. Showing it releases the cursor and ignores camera movement until **U** hides it again. The panel lists Insight, then Light, then Post. Debug and BlinnPhong list Insight only. Insight's frame-rate switch draws the number at the top left through `UIRender`, including while the panel is hidden.

### Launch

Owns the process entry point. It initializes the application, ticks it until it quits, and then finalizes it. The executable links the platform library, the RHI backends, and `ShaderCompiler`.

### Config

`Config` holds runtime settings loaded at startup from `Config/`.

| File | Contents |
| --- | --- |
| `Engine.json` | `ViewportSetting`: resolution, aspect ratio, field of view, clip planes, background color, and clear mode. `MovementSetting`: forward, right, up, down, yaw, and pitch speeds |
| `Renderer.json` | `Renderer`: `BlinnPhongRenderer`, `DebugRenderer`, `ForwardRenderer`, or `DeferredRenderer`. `DefaultMaterial`: the `.material.json` used when a mesh material is missing or its binding does not match the shader. `RHISetting`: backend name (`D3D12`), VSync, debug layer, and back buffer count (2 to 8) |
| `Scene.json` | Scene meshes and lights. Each mesh key is an `.asset.json` path. Each instance stores translation, rotation in radians as pitch, yaw and roll, and scale. Each light has a `Type` of `DirectLight`, `PointLight`, or `SkyLight` |
| `GlobalTexture.json` | `EnvironmentMap`: path to the equirectangular HDR. The engine always loads this file. Forward and Deferred turn it into a cubemap for the sky and image-based lighting, then release that source texture. BlinnPhong and Debug do not build the cubemap |

`JsonConfigParser` reads the four startup files and validates them before they enter `ConfigManager`. `ConfigManager` is the single store for those settings. Mesh and material description files are not cached there. `DescriptionParser` parses them on demand into the data types declared in `Asset/Description.h`. Later changes to startup settings go through `ConfigManager` rather than through another copy of the settings.

Interface parameters are a separate runtime store, `UIConfigManager`, forwarded by `ConfigManager` without the JSON mutex. `UIConfigSetting` holds `DeferredUIParameter`, `ForwardUIParameter`, `BlinnPhongUIParameter`, and `DebugUIParameter`. Deferred and Forward each contain `InsightUIParameter`, a light parameter block, and a post parameter block. BlinnPhong and Debug contain `InsightUIParameter` only. Nothing in this store is read from or written to `Config/`.

## Third-Party Libraries

Each library is a separate CMake target. A module links only the libraries it uses.

| Library | Version | Target | Used by |
| --- | --- | --- | --- |
| RapidJSON | 1.1.0 | `RapidJSON` (header-only) | `Framework` |
| stb | see below | `Stb` (header-only) | `Framework` |
| cgltf | 1.15 | `Cgltf` (header-only) | `Framework` |
| GLM | 1.0.3 | `GLM` (header-only) | `Framework`, public |
| meshoptimizer | 1.3 | `MeshOptimizer` (static) | `Framework` |
| Dear ImGui | 1.92.9b | `ImGui` (static, core only) | `Framework` |
| | | `ImGuiWin32` (static) | `WindowsPlatform` |
| Slang | 2026.18.3 | `SlangHeaders` (headers only) | `ShaderCompiler` |
| DXC | 1.9.2609.5 | runtime DLLs only, Windows | `ShaderCompiler`, loaded by Slang |
| DirectX 12 Agility SDK | 1.619.6 | runtime DLLs, Windows | `ShadowEngine`, loaded by the D3D12 loader |

The stb and cgltf implementations are compiled once, in `Framework/Common/Private/ThirdPartyImpl.cpp`. The ImGui Vulkan backend is not built while the Vulkan RHI is empty.

Slang ships only MSVC-built binaries. They work with the UCRT64 toolchain because Slang exposes a C entry point and COM-style interfaces, and its DLLs depend only on system libraries.

The engine uses neither the DXC API, headers, nor import libraries, and there is no engine-level HLSL compilation stage. DXC lives in `Source/ThirdParty/DXC/Windows` solely as Slang's downstream implementation dependency for its `SLANG_DXIL` target. The build copies `dxcompiler.dll` and `dxil.dll` for the target architecture from `bin/<arch>` next to the executable, and configuration fails if either file is missing. Slang may load `dxcompiler.dll` internally when it emits DXIL, and `dxcompiler.dll` loads `dxil.dll` to sign the result. Without them, shader compilation fails and the engine exits with a Slang diagnostic.

## Third-Party Notices

Vendored libraries remain under their own licenses. Their copyright notices stay with the files in `Source/ThirdParty`.

### RapidJSON

```text
Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.
```

RapidJSON is under the MIT License. The vendored tree also contains `msinttypes` r29, Copyright (c) 2006-2013 Alexander Chemeris, under the BSD License, and JSON.org data, Copyright (c) 2002 JSON.org, under the JSON License. Those extra components are not part of the engine build.

### stb

```text
stb_image         v2.30   Sean Barrett                        Public Domain
stb_image_write   v1.16   Sean Barrett                        Public Domain
stb_image_resize2 v2.18   Jeff Roberts and Jorge L Rodriguez  Public Domain
```

The upstream snapshot also includes a dual MIT / Unlicense text. These headers are provided with no warranty.

### cgltf

```text
Copyright (c) 2018-2021 Johannes Kuhlmann
```

cgltf is under the MIT License.

### GLM

```text
Copyright (c) 2005 - G-Truc Creation
```

GLM is under the Happy Bunny License or the MIT License.

### meshoptimizer

```text
Copyright (c) 2016-2026 Arseny Kapoulkine
```

meshoptimizer is under the MIT License.

### Dear ImGui

```text
Copyright (c) 2014-2026 Omar Cornut
```

Dear ImGui is under the MIT License.

### Slang

Slang is under the Apache License 2.0 with LLVM Exception. Its binaries bundle further components, such as glslang, SPIRV-Tools, LZ4, miniz, and mimalloc, whose licenses are in `Source/ThirdParty/Slang/LICENSES` and `Source/ThirdParty/Slang/third-party-notices`. Ship those notices with any build that includes the Slang DLLs.

### DirectX Shader Compiler

```text
(c) Microsoft Corporation
```

The DXC source is under the University of Illinois/NCSA Open Source License (`LICENSE-LLVM.txt`) and the MIT License (`LICENCE-MIT.txt`). The prebuilt release is also covered by the Microsoft Software License Terms for the DirectX Shader Compiler (`LICENSE-MS.txt`), which limit use to Windows and define which files may be redistributed. All three files are in `Source/ThirdParty/DXC/Windows`. Ship them with any build that includes `dxcompiler.dll` and `dxil.dll`.

### DirectX 12 Agility SDK

```text
(c) Microsoft Corporation
```

The Agility SDK is under the Microsoft Software License Terms for Microsoft DirectX, in `Source/ThirdParty/DirectX/AgilitySDK/LICENSE.txt`. The runtime redistributables are `D3D12Core.dll` and `d3d12SDKLayers.dll`. Ship that license with any build that includes them.
