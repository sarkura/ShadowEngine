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
 -> AssetManager         loads each Scene.json mesh: .asset.json, its .material.json files, then the glTF source
 -> Scene                one entity per instance, each with its own transform
 -> Renderer             uploads one GPU batch per material slot, then the constant buffer and the depth buffer
 -> every frame          move the camera, write world matrices, draw each entity, present
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
Assets/                  Mesh and material descriptions, plus glTF sources
  Mesh/                  .asset.json and .glb
  Material/              .material.json
Config/                  Runtime configuration
Shaders/                 Slang shader sources
Source/
  Interface/             Contracts shared by every module
  Framework/             Backend-agnostic engine core
    Asset/               Assets, asset registry and asset management
    Common/              Base application, logging, shared types, third-party implementations
    Config/              Configuration storage and JSON parsing
    Engine/              Engine entry object
    Material/            Materials, material instances and parameters
    Render/              Renderer, render passes and render queue
    RHI/                 Render hardware interface abstraction
    Scene/               Scene, entities and transforms
    Shader/              Shader objects, reflection data and shader management
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
```

Implementation modules keep their API in `Public` and their `.cpp` files and internal headers in `Private`. Include paths start at `Source/`, for example `#include "RHI/Direct3D12/Public/D3D12Device.h"`.

Abstractions and their implementations follow one pattern. `Interface` declares `IApplication` and `Platform/*` implements it. `Framework/RHI` declares the RHI and `RHI/*` implements it.

## Modules

The startup path above is implemented: `Common`, `Config`, `Engine`, `Asset`, `Scene`, `Material`, `Render`, `RHI`, `Shader`, the D3D12 backend and `ShaderCompiler`. `UI`, `RenderPass`, `RenderQueue` and the Vulkan backend are still skeletons. Dear ImGui is linked, but the FPS readout is a Win32 layered window. The sections below describe the responsibility of each module.

### Interface

Defines the contracts used across the engine.

- `IRuntimeModule` is the lifecycle shared by runtime systems: initialize, tick, and finalize.
- `IApplication` is the application contract. It adds quit state to that lifecycle.
- `IInterface` and `IImplements` are the macros used to declare these contracts.

### Framework

Contains everything that does not depend on a graphics API, a shader compiler, or an operating system. It is one static library.

- `Common` provides `BaseApplication` and its default lifecycle, plus logging, shared types, and `NonCopyable`. `ThirdPartyImpl.cpp` is the single translation unit that compiles the stb and cgltf implementations.
- `Config` owns runtime settings. See [Config](#config).
- `Engine` is the composition root. `EngineModules` is the registry where backends and the shader compiler register factories at startup, so `Framework` never names a concrete implementation. `Engine` creates the device, swap chain, shader manager, asset manager, scene and renderer. Each frame it updates the camera and ticks the renderer.
- `Asset` loads mesh descriptions and tracks the resulting mesh assets in a registry. `Asset` is only a scene model. A `.asset.json` names the glTF source and one or more material slots. Each slot matches a glTF material index and becomes one mesh section.
- `Scene` holds entities and transforms. `Scene.json` groups instances under a mesh description path. Each instance has translation, rotation and scale. Light entries are parsed and are not spawned.
- `Material` describes a shader and its parameters. `Material` does not inherit `Asset`. A `.material.json` names the shader under `Shaders/` and holds a `parameters` object. Each mesh section keeps a `MaterialInstance`.
- `Render` owns the renderer, the fly camera, render passes, and the render queue. `Renderer` uploads one batch per material slot, writes a 256-byte constant slot per entity, and draws with depth. Render passes and the render queue are still empty.
- `RHI` declares the device, adapter, swap chain, command list, fence, texture, shader, pipeline, and buffer abstractions that backends implement. Vertex, index and constant buffers are in use. Sampler and descriptor abstractions are still empty.
- `Shader` holds shader objects, reflection data, and the shader manager. It declares `IShaderCompiler` but does not compile shaders itself. `ShaderManager` asks the device which bytecode format it needs, compiles through `IShaderCompiler`, and creates the RHI shader.
- `UI` is still empty. The FPS number is drawn by `WindowsPlatform` with GDI.

`BaseApplication` loads configuration before the selected platform continues initialization.

### RHI Backends

Each backend is a separate static library. It implements `Framework/RHI` and owns the graphics API and ImGui backend it needs.

| Module | Host | Links | Status |
| --- | --- | --- | --- |
| `RHID3D12` | Windows | `d3d12`, `dxgi`, `dxguid`, `ImGuiDX12` | Device, swap chain, command list, fence, pipeline, vertex, index and constant buffers, depth, render target clear and present |
| `RHIVulkan` | Every host | Nothing yet | Empty implementation, not registered, no Vulkan SDK required |

Each backend exposes one registration function, such as `RegisterD3D12RHI()`, which `main` calls before the application starts. The D3D12 backend requires feature level 12_0 and Shader Model 6.0. It picks the high-performance hardware adapter that supports feature level 12_0, then checks the driver's shader model. If either requirement fails, the engine logs the reason and the process exits. The backend consumes DXIL bytecode:

```text
.slang -> Slang -> DXIL (SM 6.0) -> D3D12
```

The engine requests `SLANG_DXIL` directly from the Slang API and passes the returned
bytecode to D3D12. It does not ask Slang for HLSL and does not invoke DXC itself.

The swap chain follows the window size. When the window is resized, the engine waits for the GPU and resizes the back buffers. While the window is minimized, rendering pauses and the application sleeps until the next window message.

When `DebugLayer` is enabled, the backend enables the D3D12 debug layer and forwards its warnings and errors to the engine log. Frames are fully synchronized: the CPU waits for the GPU after every present.

### ShaderCompiler

Compiles Slang shaders from `Shaders/`. It uses only the Slang headers and loads `slang-compiler.dll` at runtime. The engine therefore has no link-time dependency on Slang, and it can start without the Slang DLLs when shaders are precompiled.

The module implements `IShaderCompiler` and registers it with `RegisterSlangShaderCompiler()`. It resolves `slang_createGlobalSession` from the library and reaches everything else through Slang's COM interfaces. Each compile loads the module named after the file, finds the entry point for the requested stage, links it, and returns the bytecode. Slang diagnostics are included in the error message or logged as warnings.

`ShaderCompiler` is the only module that owns shader compiler runtimes. It declares the Slang DLLs and Slang's Windows DXIL backend dependencies in its `SHADOW_RUNTIME_DLLS` target property, and the executable copies whatever that property lists. The RHI backends never see compiler implementation details. They only report the bytecode format they consume through `RHIDevice::GetShaderFormat()`, such as DXIL for D3D12.

### Platform

Exactly one platform library is built.

| Host | Module | Responsibility |
| --- | --- | --- |
| Windows | `WindowsPlatform` | Creates and runs the Win32 window |
| Other systems | `EmptyPlatform` | Reports that the platform is unsupported |

Both modules supply the global application object consumed by Launch. Launch does not depend on a concrete platform type.

`WindowsApplication` uses the viewport configuration for the initial window client size and passes the window to the engine. Each tick it processes window messages first, measures the frame time, and renders only while the window is open. It forwards `WM_SIZE` to the engine and renders a frame for each one, so the image stays correct while the window is dragged. It ends the main loop when the window closes, and it releases the engine before destroying the window. `EmptyApplication` reports `Platform Unsupported` and then quits.

The camera is a fly camera. It starts at `(0, 4, 14)`, looking at the origin. **W** and **S** move along the view, **A** and **D** strafe, and the mouse changes yaw and pitch. The cursor is captured when the window opens. **Esc** releases it, and a click in the window captures it again. **U** shows or hides the FPS number in the top-left corner. That number is drawn with `DrawText` into a layered window, so it stays above the flip-model swap chain.

### Launch

Owns the process entry point. It initializes the application, ticks it until it quits, and then finalizes it. The executable links the platform library, the RHI backends, and `ShaderCompiler`.

### Config

`Config` holds runtime settings loaded at startup from `Config/`.

| File | Contents |
| --- | --- |
| `Engine.json` | `ViewportSetting`: resolution, aspect ratio, field of view, clip planes, background color, and clear mode |
| `Renderer.json` | `RHISetting`: backend name (`D3D12`), VSync, debug layer, and back buffer count (2 to 8) |
| `Scene.json` | Scene meshes and lights. Each mesh key is an `.asset.json` path. Each instance stores translation, rotation in radians as pitch, yaw and roll, and scale |

`JsonConfigParser` reads these files and validates them before they enter `ConfigManager`. `ConfigManager` is the single store for settings. Later changes, including changes from a UI, go through it rather than through another copy of the settings.

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
