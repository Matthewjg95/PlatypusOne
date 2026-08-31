cmake_minimum_required(VERSION 3.22)

project(PlatypusOS
    VERSION 0.1.0
    DESCRIPTION "PlatypusOS - extensible embedded operating environment for Arduino UNO Q"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global build configuration (no global state in *code*; build flags only)
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PLATYPUS_BUILD_TESTS "Build unit tests" ON)
option(PLATYPUS_BUILD_APPS  "Build applications" ON)
option(PLATYPUS_TARGET_HOST "Build with host (simulated) HAL drivers" ON)

include(cmake/CompilerWarnings.cmake)

# ---------------------------------------------------------------------------
# Layers. Each subdirectory is an independently compilable CMake target.
# Dependency direction is strictly:  apps -> services -> platform
# ---------------------------------------------------------------------------
add_subdirectory(platform/hardware)
add_subdirectory(platform/display)
add_subdirectory(platform/camera)
add_subdirectory(platform/sensors)
add_subdirectory(platform/audio)

add_subdirectory(services/renderer)
add_subdirectory(services/geometry)
add_subdirectory(services/filesystem)
add_subdirectory(services/observation)
add_subdirectory(services/vision)
add_subdirectory(services/ai)
add_subdirectory(services/appfw)

# App *libraries* build unconditionally (tests and tools link them); the
# guard only gates app executables like the launcher.
add_subdirectory(apps/engineering_scout)
if(PLATYPUS_BUILD_APPS)
    add_subdirectory(apps/launcher)
    add_subdirectory(apps/settings)
endif()

add_subdirectory(tools/engineering_scout_capture)
add_subdirectory(tools/ui_preview)

if(PLATYPUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
