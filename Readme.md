# ShadowEngine

ShadowEngine is a minimal cross-platform render engine. One platform implementation is selected at build time. The launch module drives that implementation through a shared application contract.

```text
Launch
  -> IApplication
       -> WindowsApplication     Windows
       -> EmptyApplication       every other platform
```

Windows creates a native window from the viewport configuration. Every other platform reports that it is unsupported and exits.

## Layout

```text
Config/                  Runtime configuration
Source/
  Interface/             Contracts shared by every module
  Framework/
    Common/              Base application and shared runtime services
    Config/              Configuration storage and JSON parsing
  Platform/
    Windows/             Win32 application
    Empty/               Unsupported-platform application
  Launch/                Process entry point
  ThirdParty/            Vendored libraries
```

Implementation modules keep their API in `Public` and their `.cpp` files and internal headers in `Private`.

## Modules

### Interface

Defines the contracts used across the engine.

- `IRuntimeModule` is the lifecycle shared by runtime systems: initialize, tick, and finalize.
- `IApplication` is the application contract. It adds quit state to that lifecycle.
- `IInterface` and `IImplements` are the macros used to declare these contracts.

### Framework

Contains behavior shared by every platform.

- `Common` provides `BaseApplication`, the default lifecycle, and `GraphManager`, the graphics-system placeholder.
- `Config` owns runtime settings. `ConfigManager` is the single store for those settings. `JsonConfigParser` reads configuration files and validates them before they enter that store.
- `BaseApplication` loads configuration before the selected platform continues initialization.

### Platform

Exactly one platform library is built.

| Host | Module | Responsibility |
| --- | --- | --- |
| Windows | `WindowsPlatform` | Creates and runs the Win32 window |
| Other systems | `EmptyPlatform` | Reports that the platform is unsupported |

Both modules supply the global application object consumed by Launch. Launch does not depend on a concrete platform type.

`WindowsApplication` uses the viewport configuration for the window client size and ends the main loop when the window closes. `EmptyApplication` reports `Platform Unsupported` and then quits.

### Launch

Owns the process entry point. It initializes the application, ticks it until it quits, and then finalizes it.

### Config

`Config` holds runtime settings loaded at startup. `ViewportSetting` describes the viewport: resolution, aspect ratio, field of view, clip planes, background color, and clear mode. Later changes, including changes from a UI, go through `ConfigManager` rather than through another copy of the settings.

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
