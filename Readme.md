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
 -> Renderer             Renderer.json selects BlinnPhongRenderer, DebugRenderer, or ForwardRenderer. DeferredRenderer source files exist and are not selectable yet. The base syncs a render proxy, resolves the handles, and uploads one GPU batch per material slot that binds the active renderer. Each draw gets a 1024-byte constant slot and a depth buffer. Only Forward uploads the sky pipeline
 -> every frame          update the camera, sync the proxy, then Renderer::RenderFrame. It collects visible items, calls PrepareFrame, fills the pass context, runs that renderer's pass, and calls StoreFrameHistory. BlinnPhong and Debug leave those hooks empty and draw straight to the back buffer. Forward uses them for the depth prepass, environment cube, image-based lighting, shadows, and post
                         shaders are compiled from Shaders/ on this launch and kept in memory. Register deduplicates the vertex, fragment, material, and light paths together with the parameter names declared for that binding
                         the FPS number is drawn with Win32 GDI, outside the swap chain
```

Rendering is split into a backend-agnostic core and pluggable backends. The core defines the render hardware interface (RHI) and the shader data model. Direct3D 12 and Vulkan implement the RHI. Shader compilation lives in its own module, which loads the Slang compiler at runtime instead of linking against it.

```text
ShadowEngine.exe
 ├── WindowsPlatform ── ImGuiWin32
 ├── RHID3D12 ────────── ImGuiDX12, d3d12 / dxgi
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
  Common/                Per-object data, vertex layout, descriptor-heap helpers, Blinn-Phong and forward constants
  Material/              EvaluateMaterial for each surface
  Light/                 EvaluateLight for Blinn-Phong and the forward pipeline, plus shared image-based lighting and PCF shadow sampling
  BlinnPhong/            Blinn-Phong vertex and fragment entry points
  Forward/               Forward vertex and fragment entry points
  Depth/                 Shared depth prepass, plus the directional and point-light shadow depth shaders Forward runs
  ImageBasedLighting/    Irradiance, prefiltered environment, and BRDF lookup generation
  DebugRenderer/         Debug vertex and fragment entry points
  Sky/                   Fullscreen environment background
Source/
  Interface/             Contracts shared by every module
  Framework/             Backend-agnostic engine core
    Asset/               Mesh, texture, and material assets, description data, and handles
    Common/              Base application, logging, shared types, third-party implementations
    Config/              Startup settings and description-file parsing
    Engine/              Engine entry object
    Material/            Materials, instances, parameter layout, and parameter meaning
    Render/              Renderer, camera, render proxy, queue, passes, and sky background
    RHI/                 Render hardware interface abstraction
    Scene/               Scene, entities, transforms, and lights
    Shader/              Shader handles and shader management
    UI/                  UI management and ImGui rendering
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
  Blender/ShadowEngineExporter/   Blender 5.2 add-on. Writes ExportData next to the installed copy
```

Implementation modules keep their API in `Public` and their `.cpp` files and internal headers in `Private`. Include paths start at `Source/`, for example `#include "RHI/Direct3D12/Public/D3D12Device.h"`.

Abstractions and their implementations follow one pattern. `Interface` declares `IApplication` and `Platform/*` implements it. `Framework/RHI` declares the RHI and `RHI/*` implements it.

## Blender exporter

`Tools/Blender/ShadowEngineExporter` is a Blender 5.2 add-on. Its manifest id is `ShadowEngineExporter`, and the installed folder uses that same name. Artist steps are in `Tools/Blender/ShadowEngineExporter/Install.md`. The add-on writes the current scene into `ExportData` beside the installed copy. It clears that folder first and leaves `.gitkeep`. The engine keeps reading `Config/` and `Assets/` from the repository root, so a preview copies `ExportData/Assets/Mesh` onto `Assets/Mesh` and `ExportData/Config/Scene.json` onto `Config/Scene.json`. `Engine.json`, `Renderer.json`, and `GlobalTexture.json` stay as they are.

`File → Export → ShadowEngine (.glb)` and the 3D Viewport **ShadowEngine** tab both export the whole scene in Object Mode. Hidden objects that remain in the scene are included. Cameras, empties, curves, armatures, spot lights, and area lights are skipped. Object names that differ only by a Blender suffix such as `.001` share one `.glb`, one `.asset.json`, and one material. Each object still writes its own transform. The shared files come from the object whose name has no suffix. An empty material slot is named from the model and the slot index, such as `Cube_0`.

A material file contains `ForwardRenderer` only. `Parent` sits inside that object and names one of the three base materials. The instance writes `Surface`, `TwoSided`, and `Parameters` using the parent's types and values, and leaves the shader paths and the parameter schema on the parent. `DeferredRenderer` can be added later. `BlinnPhongRenderer` and `DebugRenderer` are not written. A material custom property `Surface` may be `Opaque`, `Cutout`, or `Translucent`. Without it, a Principled Alpha below `0.999` becomes `Translucent` and any other value becomes `Opaque`. `TwoSided` is true only when that custom property is true. A non-opaque surface uses `ForwardTranslucentMaterial` and drops textures. An opaque surface with a base-color image uses `ForwardTextureMaterial`; missing roughness, metallic, and occlusion images become a white texture, and a missing normal becomes a flat normal. Every other surface uses `ForwardPureMaterial` with `BaseColor` and `Roughness`. `Cutout` is written, and the shared material shader does not clip on `CutoutThreshold` yet.

One Sun becomes `DirectLight`. Its direction is the light's local -Z after the axis conversion below. A second Sun is skipped. A point light becomes `PointLight`. Its intensity is the Blender watt value multiplied by **Point Energy Scale** (default `0.01`). Its radius is the object property `Radius`, or **Point Radius** (default `10`) when that property is absent. The world Background color and Strength become the one `SkyLight`. With no world, the color is `(0.45, 0.55, 0.75)` and the intensity is `0.25`. `SkyLight` scales image-based lighting on meshes. The sky background still samples the HDR named by `GlobalTexture.json`.

Blender is Z-up. The exporter converts a transform into the engine's Y-up space before writing it: `(x, y, z)` becomes `(x, z, -y)`. Rotation is the converted ZXY Euler in radians, stored as pitch, yaw, roll. The glTF file is exported from a temporary copy with an identity transform, so the node transform is not baked into the vertices and `Scene.json` places each instance.

## Modules

The startup path above is implemented: `Common`, `Config`, `Engine`, `Asset`, `Scene`, `Material`, `Render`, `RHI`, `Shader`, the D3D12 backend and `ShaderCompiler`. `UI` and the Vulkan backend are still skeletons. Dear ImGui is linked, but the FPS readout is a Win32 layered window. The sections below describe the responsibility of each module.

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
- `Asset` loads descriptions through `DescriptionParser`, then loads the glTF source and any textures. A `.asset.json` names the glTF source and one or more material slots. Each slot becomes one mesh section and stores a local AABB taken from its vertex positions. A `.material.json` has `Type` and one object per renderer (`BlinnPhongRenderer`, `DebugRenderer`, `ForwardRenderer`). Each renderer object may name `Parent`, a path to the base material for that renderer. The loader resolves that parent, then overlays this object's parameters. A value whose name or type is not in the parent schema fails to load. It checks the result against the shader layout and creates one `MaterialInstance`. `Parent` is only a description-file relationship. `MaterialInstance` stores a `MaterialHandle` and the merged parameter block, and it does not keep a parent instance. `Material` stores the shader and reflected layout for each renderer, and it stores no parameter values. A base file supplies `Vertex`, `Fragment`, and `Material`, and its `Parameters` name each field and its type, for example `"BaseColor": "float3"` or `"BaseColorTexture": "texture"`. Forward bases also supply `Surface` and `TwoSided`. The bases are `Assets/BaseMaterial/ForwardPure.material.json`, `ForwardTexture.material.json`, `ForwardTranslucent.material.json`, `BlinnPhongPure.material.json`, `BlinnPhongTexture.material.json`, and `Debug.material.json`. `Debug` declares no parameters. A renderer object with no `Parent` still supplies its own shader paths. Each object names `Vertex`, `Fragment`, `Material`, and `Parameters` when it defines that renderer. Lighting bindings can also name `Light`. A `ForwardRenderer` object also names `Surface` (`Opaque`, `Cutout`, or `Translucent`) and `TwoSided` (`true` or `false`). The three surface values are mutually exclusive. `TwoSided` false culls back faces. `TwoSided` true keeps both sides and the fragment flips the normal on a back face. BlinnPhong and Debug ignore both fields. Their mesh pipelines cull back faces with the same rule. On a base material each parameter is a type name: `float`, `float2`, `float3`, `float4`, or `texture`. On an instance each parameter is one typed value of that same type, for example `"BaseColor": {"float3": [1, 1, 1]}` or `"OcclusionTexture": {"texture": "Assets/Mesh/Table/Texture/AO.texture.json"}`. The JSON key is the shader field name. The parent schema is the set of names an instance may set. A `texture` value names a `.texture.json` with `Type`, `Source`, and `Encoding` (`sRGB` or `Linear`). `TextureAsset` stores that encoding. sRGB mip levels are averaged in linear light and stored encoded again. Linear images are averaged as stored. An sRGB texture is uploaded as an RGBA8 sRGB view, so sampling returns linear color and the material shader does not decode it again. Linear images use an RGBA8 view. `LoadEnvironmentMap` reads the HDR named by `GlobalTexture.json` with `stbi_loadf` and stores one linear RGBA32-float image, without a mip chain. Assets are exposed as `MeshHandle`, `TextureAssetHandle`, and `SamplerHandle`. `Asset` calls `MaterialParameter` to build a parameter block for each renderer binding, compiles that binding, and checks the reflected layout against the block. `ShaderManager::Register` runs once per renderer binding. A draw reads only the block named by the active renderer. If a binding fails to compile or its resources do not match the reflected layout, the mesh slot logs the error, loads `Renderer.json`'s `DefaultMaterial`, and rewrites that slot in the `.asset.json`. It does not decide what material fields mean.
- `Scene` holds entities, transforms, directional lights, point lights, and sky lights. An entity stores a `MeshHandle` and the `MaterialInstanceHandle` of each material slot. `Scene.json` groups instances under a mesh description path. Each instance has translation, rotation and scale. Each light has a `Type`. The scene has one directional light. It stores direction, color, and intensity, and a second one fails to load. A point light also stores position and radius, attenuates by inverse square, and reaches zero at its radius. The scene has one sky light. It stores color and intensity, and a second one fails to load. The directional vector is the direction rays travel. The scene exposes this state and does not build a render proxy, load assets, or draw.
- `Material` owns what a material is. `Material` stores one `ShaderHandle` per renderer name. `MaterialInstance` holds a `BaseMaterial`, one `MaterialParameterBlock` per renderer name, and the forward binding's `Surface`. `Surface` chooses the pass. It is not a shader uniform. `BaseMaterial` is a `MaterialHandle` and points at those shaders. Scalar values live in the block by JSON key. Texture parameters are `TextureAssetHandle` values, and every textured binding shares one `SamplerHandle`. `MaterialParameter` builds the block from the parsed binding and builds `MaterialParameterLayout` from shader reflection. `MaterialAsset` owns the `Material`. This module does not parse JSON, compile shaders, or upload GPU resources.
- `Render` owns the renderer base, `BlinnPhongRenderer`, `DebugRenderer`, `ForwardRenderer`, the fly camera, the render proxy, the render queue, and the render passes. `Renderer::Sync` reads entities, material-instance handles, and lights from the scene and rebuilds the proxy. The proxy carries a `MeshHandle`, material-instance handles, the one directional light, the point-light list, and the one sky light. The base resolves those handles and uploads one `MeshBatch` per material slot that has a shader for the active renderer. `Renderer::RenderFrame` is shared by the three selectable renderers. It calls `FillRenderQueue`, which builds a `RenderItem` for each accepted slot and skips a slot whose material has no shader for the active renderer. After the frustum test it calls `PrepareFrame`, fills `RenderPassContext` through `PopulateContext`, runs that renderer's pass, then calls `StoreFrameHistory`. Those three hooks are empty for BlinnPhong and Debug. Forward overrides them for the depth-prepass pipelines, the environment cube, image-based lighting, shadow maps, post, point-light selection, projection jitter, and the previous world matrices. BlinnPhong and Debug therefore still draw straight to the back buffer, and their shading shaders stay on the original Blinn-Phong and debug paths. `RenderItemFilter::Accept` currently accepts every slot. `Renderer::CollectVisibleItems` then drops a mesh section whose world AABB misses the unjittered view frustum. Every renderer uses that step, so later draws only see what remains. `Renderer::SelectVisiblePointLights` keeps point lights whose spheres intersect the same frustum. Forward asks for four. The directional light and the sky light are single and are not frustum tested. Previous world matrices are stored for every scene entity and read by that entity index, so a section that leaves and re-enters the frustum still receives its own previous matrix. The first frame copies the current matrices into the previous fields. `BlinnPhongRenderPass` and `ForwardRenderPass` write per-object data, then scalar values and descriptor indices at the offsets reported by reflection. `ForwardRenderPass` also writes the shared previous view-projection and world matrices, the directional light, at most four point lights inside the view frustum, and the sky light at those reflected offsets. `DebugRenderPass` writes the view, world, and normal matrices, and writes any scalar values and descriptor indices the debug material declared. `ForwardRenderPass` records shadow maps before the camera passes. The directional light gets one 2048 orthographic depth map fitted to the casters. Each point light that survived the frustum test gets a 1024 cube, up to four, and the cube stores linear distance over the light radius. Casters are the opaque and cutout sections that have a forward batch, including sections outside the camera frustum. Translucent sections are not drawn into the shadow map, and their shading writes invalid shadow-map indices so direct and point lights stay fully lit. A cutout section clips shadow pixels with the same `Opacity` and `CutoutThreshold` as the camera passes. Single-sided casters cull back faces. Two-sided casters keep both sides. Shading multiplies GGX by a 3×3 percentage-closer filter through a comparison sampler. A light with no map stays fully lit. Image-based lighting and the sky are not shadowed. BlinnPhong and Debug do not draw these maps. `ForwardRenderPass` then draws opaque and cutout sections in the depth prepass with color writes disabled. A cutout section uses its material pixel shader for that prepass and for the later color draw, so both discard a pixel when `CutoutThreshold` is greater than zero and `Opacity` is below it. Translucent sections skip the prepass. The color pass clears scene color without clearing depth, draws the sky, then shades opaque and cutout sections with an equal depth test and depth writes disabled. Translucent sections draw after that, far to near, with a less-equal depth test, no depth write, and alpha blending. `EvaluateMaterial` writes `Opacity` and `CutoutThreshold`. Opaque materials leave them at one and zero. `DrawSkyBackground` runs only in that color pass and samples the environment cubemap into an RGBA16 scene-color target. Forward direct and point lights use GGX. On the first frame Forward renders the equirectangular HDR into six RGBA32 cube faces, one quarter of the panorama width on each side, then releases that source. The sky light's color and intensity scale image-based lighting convolved from the cubemap: irradiance, four prefiltered levels, and a BRDF lookup. Those results stay small equirectangular maps. Convolution clamps sample luminance to 16 so the sun does not saturate the half-float maps. The sky background samples the cubemap and keeps the original HDR range. BlinnPhong and Debug do not build this cubemap. `EvaluateIBL` is the shared function. Deferred can call it later from the same material outputs; it does not run yet. Forward then runs one post stack: camera and mesh motion, TAA, bloom, and automatic exposure from the average log luminance, Reinhard, an sRGB encode, and a half-step dither into the UNORM back buffer. Shading, the sky, image-based lighting, TAA, and bloom all stay in linear scene color. The swap chain stays UNORM, so only the tonemap applies the encode. BlinnPhong and Debug draw straight to the back buffer. Sampled textures and samplers are allocated into shader-visible descriptor heaps that the shader indexes directly. Each object draw uses a 1024-byte constant slot. It does not parse JSON or interpret material field names on its own.
- `RHI` declares the device, adapter, swap chain, command list, fence, texture, sampler, shader, pipeline, descriptor, and buffer abstractions that backends implement. Vertex, index and constant buffers, sampled textures, and samplers addressed by descriptor indices are in use.
- `Shader` holds shader handles and the shader manager. It declares `IShaderCompiler` but does not know about materials. `ShaderManager::Register` deduplicates the vertex, fragment, material, and light paths together with that binding's scalar and texture field names. `LoadShaders` compiles `VertexMain` from the vertex file. The pixel compile prepends `MaterialSurfaceData` and `MaterialDescriptorIndices` from those field names, then includes the material file, the light file when the binding has one, and the fragment file, and looks up `PixelMain`. Slang reflection of that compile is the parameter layout. Bytecode stays in memory. There is no shader cache on disk.
- `UI` is still empty. The FPS number is drawn by `WindowsPlatform` with GDI.

`BaseApplication` loads configuration before the selected platform continues initialization.

### RHI Backends

Each backend is a separate static library. It implements `Framework/RHI` and owns the graphics API and ImGui backend it needs.

| Module | Host | Links | Status |
| --- | --- | --- | --- |
| `RHID3D12` | Windows | `d3d12`, `dxgi`, `dxguid`, `ImGuiDX12` | Device, swap chain, command list, fence, pipeline, vertex, index and constant buffers, RGBA8, RGBA8 sRGB, and RGBA32-float sampled textures, directly indexed descriptor heaps, depth, cube depth, a comparison sampler, render target clear and present |
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

Compiles Slang shaders from `Shaders/` when a mesh or the sky is uploaded. It uses only the Slang headers and loads `slang-compiler.dll` at runtime, so the engine has no link-time dependency on Slang. There is no shader cache: each launch reads the `.slang` sources again, and the bytecode is not written to disk. The compile roots are the vertex and fragment files named by the active renderer binding. Before the pixel includes, the compiler emits `MaterialSurfaceData` from the binding's `float`, `float2`, `float3`, and `float4` keys, and `MaterialDescriptorIndices` from its `texture` keys plus one shared `Sampler`. The pixel compile then includes the material file, the light file when the binding has one, and the fragment file, so `PixelMain` can call `EvaluateMaterial` and `EvaluateLight`. A material shader reads scalars as `Surface.<JsonKey>` and textures as `Descriptors.<JsonKey>`. A local `MaterialSurfaceData` in a material file is only there so the editor can resolve those names; the compile uses the struct generated from JSON. Cube, sphere, and Suzanne share one Blinn-Phong path when their parameter names match. Forward uses three material shaders: `ForwardPureMaterial` for Plane, Sphere, Suzanne, and the default material, `ForwardTranslucentMaterial` for Cube, and `ForwardTextureMaterial` for the table. The textured material samples the textures named in its JSON through `LoadResourceDescriptor` and `LoadSamplerDescriptor` in `Shaders/Common/DescriptorHeap.slang`. Those helpers emit `ResourceDescriptorHeap` and `SamplerDescriptorHeap`. The indices live in the `b0` constant buffer. `Shaders/Material` evaluates the surface. `Shaders/Light` evaluates lighting and returns a light sample. `Shaders/BlinnPhong` and `Shaders/Forward` are the entry points. `Shaders/DebugRenderer` maps the world normal into color, and can also read `Surface` and `Descriptors` when that debug binding declares parameters. `Shaders/Sky` draws the environment map as the background. A mesh section whose material has no binding for the renderer selected in `Renderer.json` is not uploaded and is not drawn. A binding whose shader fails to compile, or whose reflected resources are missing, is replaced by `DefaultMaterial`.

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

The camera is a fly camera. It starts at `(0, 4, 14)`, looking at the origin. **W** and **S** move along the view, **A** and **D** strafe, **Q** moves up along world Y, and **E** moves down. The mouse changes yaw and pitch. Speeds come from `MovementSetting` in `Engine.json`. The cursor is captured when the window opens. **Esc** releases it, and a click in the window captures it again. **U** shows or hides the FPS number in the top-left corner. That number is drawn with `DrawText` into a layered window, so it stays above the flip-model swap chain.

### Launch

Owns the process entry point. It initializes the application, ticks it until it quits, and then finalizes it. The executable links the platform library, the RHI backends, and `ShaderCompiler`.

### Config

`Config` holds runtime settings loaded at startup from `Config/`.

| File | Contents |
| --- | --- |
| `Engine.json` | `ViewportSetting`: resolution, aspect ratio, field of view, clip planes, background color, and clear mode. `MovementSetting`: forward, right, up, down, yaw, and pitch speeds |
| `Renderer.json` | `Renderer`: `BlinnPhongRenderer`, `DebugRenderer`, or `ForwardRenderer`. `DefaultMaterial`: the `.material.json` used when a mesh material is missing or its binding does not match the shader. `RHISetting`: backend name (`D3D12`), VSync, debug layer, and back buffer count (2 to 8) |
| `Scene.json` | Scene meshes and lights. Each mesh key is an `.asset.json` path. Each instance stores translation, rotation in radians as pitch, yaw and roll, and scale. Each light has a `Type` of `DirectLight`, `PointLight`, or `SkyLight` |
| `GlobalTexture.json` | `EnvironmentMap`: path to the equirectangular HDR. Forward turns it into a cubemap for the sky and image-based lighting, then releases that source texture. BlinnPhong and Debug do not load it |

`JsonConfigParser` reads the four startup files and validates them before they enter `ConfigManager`. `ConfigManager` is the single store for those settings. Mesh and material description files are not cached there. `DescriptionParser` parses them on demand into the data types declared in `Asset/Description.h`. Later changes to startup settings, including changes from a UI, go through `ConfigManager` rather than through another copy of the settings.

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
| | | `ImGuiDX12` (static) | `RHID3D12` |
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
