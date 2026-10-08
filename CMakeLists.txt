cmake_minimum_required(VERSION 3.20)

project(ShadowEngine
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "ShadowEngine"
)


# ------------------------------------------------------------------------------
# Global C++20 Standard and Compiler Settings
# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(SHADOW_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set(SHADOW_THIRDPARTY_DIR ${SHADOW_SOURCE_DIR}/ThirdParty)

# ------------------------------------------------------------------------------
# Third-Party Libraries
# ------------------------------------------------------------------------------

# RapidJSON (header-only)
add_library(RapidJSON INTERFACE)
target_include_directories(RapidJSON SYSTEM INTERFACE
    ${SHADOW_THIRDPARTY_DIR}/RapidJSON/include
)

# Stb (header-only, implementation compiled in ThirdPartyImpl.cpp)
add_library(Stb INTERFACE)
target_include_directories(Stb SYSTEM INTERFACE
    ${SHADOW_THIRDPARTY_DIR}/Stb
)

# Cgltf (header-only, implementation compiled in ThirdPartyImpl.cpp)
add_library(Cgltf INTERFACE)
target_include_directories(Cgltf SYSTEM INTERFACE
    ${SHADOW_THIRDPARTY_DIR}/Cgltf/cgltf
)

# GLM (header-only)
add_library(GLM INTERFACE)
target_include_directories(GLM SYSTEM INTERFACE
    ${SHADOW_THIRDPARTY_DIR}/GLM
)

# MeshOptimizer
file(GLOB MESHOPTIMIZER_SOURCES CONFIGURE_DEPENDS
    "${SHADOW_THIRDPARTY_DIR}/MeshOptimizer/src/*.cpp"
)
add_library(MeshOptimizer STATIC ${MESHOPTIMIZER_SOURCES})
target_include_directories(MeshOptimizer SYSTEM PUBLIC
    ${SHADOW_THIRDPARTY_DIR}/MeshOptimizer/src
)

# Dear ImGui core
add_library(ImGui STATIC
    ${SHADOW_THIRDPARTY_DIR}/Imgui/imgui.cpp
    ${SHADOW_THIRDPARTY_DIR}/Imgui/imgui_demo.cpp
    ${SHADOW_THIRDPARTY_DIR}/Imgui/imgui_draw.cpp
    ${SHADOW_THIRDPARTY_DIR}/Imgui/imgui_tables.cpp
    ${SHADOW_THIRDPARTY_DIR}/Imgui/imgui_widgets.cpp
)
target_include_directories(ImGui SYSTEM PUBLIC
    ${SHADOW_THIRDPARTY_DIR}/Imgui
)

# Slang (headers only, slang-compiler.dll is loaded at runtime by ShaderCompiler)
add_library(SlangHeaders INTERFACE)
target_include_directories(SlangHeaders SYSTEM INTERFACE
    ${SHADOW_THIRDPARTY_DIR}/Slang/include
)
file(GLOB SLANG_RUNTIME_DLLS "${SHADOW_THIRDPARTY_DIR}/Slang/bin/*.dll")
list(FILTER SLANG_RUNTIME_DLLS EXCLUDE REGEX "/gfx\\.dll$")

# Slang's Windows DXIL backend dependencies (the engine does not call the DXC API)
set(DXC_RUNTIME_DLLS "")
if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64)$")
        set(DXC_ARCH arm64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DXC_ARCH x64)
    else()
        set(DXC_ARCH x86)
    endif()

    set(DXC_RUNTIME_DIR ${SHADOW_THIRDPARTY_DIR}/DXC/Windows/bin/${DXC_ARCH})
    set(DXC_RUNTIME_DLLS
        ${DXC_RUNTIME_DIR}/dxcompiler.dll
        ${DXC_RUNTIME_DIR}/dxil.dll
    )
    foreach(DXC_DLL IN LISTS DXC_RUNTIME_DLLS)
        if(NOT EXISTS ${DXC_DLL})
            message(FATAL_ERROR "DXC runtime library not found: ${DXC_DLL}")
        endif()
    endforeach()
endif()

# ------------------------------------------------------------------------------
# 1. Interface Module (Header-Only Interface Library)
# ------------------------------------------------------------------------------
add_library(Interface INTERFACE)

# Expose the Source directory as INTERFACE
# Any target linking against Interface automatically inherits this include path
target_include_directories(Interface INTERFACE
    ${SHADOW_SOURCE_DIR}
)

# ------------------------------------------------------------------------------
# 2. Framework Module (Base Framework, backend-agnostic)
# ------------------------------------------------------------------------------
file(GLOB_RECURSE FRAMEWORK_SOURCES CONFIGURE_DEPENDS
    "${SHADOW_SOURCE_DIR}/Framework/*.cpp"
)
add_library(Framework STATIC ${FRAMEWORK_SOURCES})

# Link Interface using PUBLIC to propagate the include directory to Framework and its consumers
target_link_libraries(Framework
    PUBLIC
    Interface
    GLM
    PRIVATE
    RapidJSON
    Stb
    Cgltf
    MeshOptimizer
    ImGui
)

# ------------------------------------------------------------------------------
# 3. ShaderCompiler Module (requests DXIL directly through the runtime-loaded Slang API)
# ------------------------------------------------------------------------------
file(GLOB_RECURSE SHADER_COMPILER_SOURCES CONFIGURE_DEPENDS
    "${SHADOW_SOURCE_DIR}/ShaderCompiler/*.cpp"
)
add_library(ShaderCompiler STATIC ${SHADER_COMPILER_SOURCES})
target_link_libraries(ShaderCompiler
    PUBLIC Framework
    PRIVATE SlangHeaders ${CMAKE_DL_LIBS}
)
if(WIN32)
    target_compile_definitions(ShaderCompiler PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Libraries ShaderCompiler loads at runtime. Executables copy them next to themselves.
set_property(TARGET ShaderCompiler PROPERTY SHADOW_RUNTIME_DLLS
    ${SLANG_RUNTIME_DLLS}
    ${DXC_RUNTIME_DLLS}
)

# ------------------------------------------------------------------------------
# 4. RHI Backend Modules
# ------------------------------------------------------------------------------

# Vulkan (empty implementation for now, no Vulkan SDK dependency)
file(GLOB_RECURSE RHI_VULKAN_SOURCES CONFIGURE_DEPENDS
    "${SHADOW_SOURCE_DIR}/RHI/Vulkan/*.cpp"
)
add_library(RHIVulkan STATIC ${RHI_VULKAN_SOURCES})
target_link_libraries(RHIVulkan PUBLIC Framework)

set(SHADOW_RHI_TARGETS RHIVulkan)

if(WIN32)
    # Dear ImGui DX12 backend
    add_library(ImGuiDX12 STATIC
        ${SHADOW_THIRDPARTY_DIR}/Imgui/backends/imgui_impl_dx12.cpp
    )
    target_include_directories(ImGuiDX12 SYSTEM PUBLIC
        ${SHADOW_THIRDPARTY_DIR}/Imgui/backends
    )
    target_link_libraries(ImGuiDX12
        PUBLIC ImGui
        PRIVATE d3d12 dxgi d3dcompiler
    )

    # Direct3D12
    file(GLOB_RECURSE RHI_D3D12_SOURCES CONFIGURE_DEPENDS
        "${SHADOW_SOURCE_DIR}/RHI/Direct3D12/*.cpp"
    )
    add_library(RHID3D12 STATIC ${RHI_D3D12_SOURCES})
    target_compile_definitions(RHID3D12 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(RHID3D12
        PUBLIC Framework
        PRIVATE ImGuiDX12 d3d12 dxgi dxguid
    )

    list(APPEND SHADOW_RHI_TARGETS RHID3D12)
endif()

# ------------------------------------------------------------------------------
# 5. Platform Module
# ------------------------------------------------------------------------------
if(WIN32)
    # Dear ImGui Win32 backend
    add_library(ImGuiWin32 STATIC
        ${SHADOW_THIRDPARTY_DIR}/Imgui/backends/imgui_impl_win32.cpp
    )
    target_include_directories(ImGuiWin32 SYSTEM PUBLIC
        ${SHADOW_THIRDPARTY_DIR}/Imgui/backends
    )
    target_link_libraries(ImGuiWin32
        PUBLIC ImGui
        PRIVATE gdi32 dwmapi
    )

    file(GLOB_RECURSE WINDOWS_PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${SHADOW_SOURCE_DIR}/Platform/Windows/*.cpp"
    )

    add_library(WindowsPlatform STATIC ${WINDOWS_PLATFORM_SOURCES})

    target_link_libraries(WindowsPlatform
        PUBLIC Framework
        PRIVATE ImGuiWin32 user32 gdi32
    )

    set(SHADOW_PLATFORM_TARGET WindowsPlatform)
else()
    file(GLOB_RECURSE EMPTY_PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${SHADOW_SOURCE_DIR}/Platform/Empty/*.cpp"
    )

    add_library(EmptyPlatform STATIC ${EMPTY_PLATFORM_SOURCES})

    target_link_libraries(EmptyPlatform PUBLIC Framework)

    set(SHADOW_PLATFORM_TARGET EmptyPlatform)
endif()

# ------------------------------------------------------------------------------
# 6. Launch Module (Executable Entry Point)
# ------------------------------------------------------------------------------
file(GLOB_RECURSE LAUNCH_SOURCES CONFIGURE_DEPENDS
    "${SHADOW_SOURCE_DIR}/Launch/*.cpp"
)

add_executable(ShadowEngine ${LAUNCH_SOURCES})

# Link the platform selected for the current operating system, the RHI backends and the shader compiler.
target_link_libraries(ShadowEngine PRIVATE
    ${SHADOW_PLATFORM_TARGET}
    ${SHADOW_RHI_TARGETS}
    ShaderCompiler
)

if(WIN32)
    target_compile_definitions(ShadowEngine PRIVATE SHADOW_WITH_D3D12=1)
    target_compile_definitions(WindowsPlatform PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    set_target_properties(ShadowEngine PROPERTIES ENABLE_EXPORTS TRUE)

    set(D3D12_AGILITY_BIN ${SHADOW_THIRDPARTY_DIR}/DirectX/AgilitySDK/build/native/bin/x64)
    add_custom_command(TARGET ShadowEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ShadowEngine>/D3D12
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${D3D12_AGILITY_BIN}/D3D12Core.dll
            $<TARGET_FILE_DIR:ShadowEngine>/D3D12/D3D12Core.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${D3D12_AGILITY_BIN}/d3d12SDKLayers.dll
            $<TARGET_FILE_DIR:ShadowEngine>/D3D12/d3d12SDKLayers.dll
    )
endif()

get_target_property(SHADER_COMPILER_RUNTIME_DLLS ShaderCompiler SHADOW_RUNTIME_DLLS)
if(WIN32 AND SHADER_COMPILER_RUNTIME_DLLS)
    add_custom_command(TARGET ShadowEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER_COMPILER_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:ShadowEngine>
        COMMAND_EXPAND_LISTS
    )
endif()
