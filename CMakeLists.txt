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
add_library(Framework STATIC
    Source/Framework/Common/Private/BaseApplication.cpp
)

# Link Interface using PUBLIC to propagate the include directory to Framework and its consumers
target_link_libraries(Framework PUBLIC Interface)

# ------------------------------------------------------------------------------
# 3. Platform/Empty Module (Null/Empty Platform Implementation)
# ------------------------------------------------------------------------------
add_library(EmptyPlatform STATIC
    Source/Platform/Empty/Private/EmptyApplication.cpp
)

# Link Framework using PUBLIC to transitively pass Interface include paths and Framework symbols
target_link_libraries(EmptyPlatform PUBLIC Framework)

# ------------------------------------------------------------------------------
# 4. Launch Module (Executable Entry Point)
# ------------------------------------------------------------------------------
add_executable(ShadowEngine
    Source/Launch/Private/main.cpp
)

# Link Platform module using PRIVATE to acquire all required headers and dependencies
target_link_libraries(ShadowEngine PRIVATE EmptyPlatform)
