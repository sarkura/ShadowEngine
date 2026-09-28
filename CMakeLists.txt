cmake_minimum_required(VERSION 3.20)

project(ShadowEngine
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "ShadowEngine"
)


# ------------------------------------------------------------------------------
# Global C++20 Standard and Compiler Settings
# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Header-only Third-Party Libraries
# ------------------------------------------------------------------------------
add_library(RapidJSON INTERFACE)
target_include_directories(RapidJSON SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/ThirdParty/RapidJSON/include
)

add_library(Stb INTERFACE)
target_include_directories(Stb SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/ThirdParty/Stb
)

# ------------------------------------------------------------------------------
# 1. Interface Module (Header-Only Interface Library)
# ------------------------------------------------------------------------------
add_library(Interface INTERFACE)

# Expose the Source directory as INTERFACE
# Any target linking against Interface automatically inherits this include path
target_include_directories(Interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# ------------------------------------------------------------------------------
# 2. Framework Module (Base Framework)
# ------------------------------------------------------------------------------
file(GLOB_RECURSE FRAMEWORK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Framework/*.cpp"
)

add_library(Framework STATIC ${FRAMEWORK_SOURCES})

# Link Interface using PUBLIC to propagate the include directory to Framework and its consumers
target_link_libraries(Framework
    PUBLIC Interface
    PRIVATE
    RapidJSON
    Stb
)

# ------------------------------------------------------------------------------
# 3. Platform Module
# ------------------------------------------------------------------------------
if(WIN32)
    file(GLOB_RECURSE WINDOWS_PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Platform/Windows/*.cpp"
    )

    add_library(WindowsPlatform STATIC ${WINDOWS_PLATFORM_SOURCES})

    target_link_libraries(WindowsPlatform
        PUBLIC Framework
        PRIVATE user32
    )

    set(SHADOW_PLATFORM_TARGET WindowsPlatform)
else()
    file(GLOB_RECURSE EMPTY_PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Platform/Empty/*.cpp"
    )

    add_library(EmptyPlatform STATIC ${EMPTY_PLATFORM_SOURCES})

    target_link_libraries(EmptyPlatform PUBLIC Framework)

    set(SHADOW_PLATFORM_TARGET EmptyPlatform)
endif()

# ------------------------------------------------------------------------------
# 4. Launch Module (Executable Entry Point)
# ------------------------------------------------------------------------------
file(GLOB_RECURSE LAUNCH_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Launch/*.cpp"
)

add_executable(ShadowEngine ${LAUNCH_SOURCES})

# Link the platform selected for the current operating system.
target_link_libraries(ShadowEngine PRIVATE ${SHADOW_PLATFORM_TARGET})
