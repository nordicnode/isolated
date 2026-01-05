cmake_minimum_required(VERSION 3.16)
project(Isolated VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for CUDA (optional)
include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(CMAKE_CUDA_ARCHITECTURES 75) # Turing
    add_compile_definitions(HAVE_CUDA)
    message(STATUS "CUDA enabled")
else()
    message(STATUS "CUDA not found, building CPU-only version")
endif()

# Compiler-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2 /arch:AVX2)
        add_compile_definitions(_USE_MATH_DEFINES)  # For M_PI
    else()
        add_compile_options(-O3 -march=native -flto)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        # SIMD support
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
        if(COMPILER_SUPPORTS_AVX2)
            add_compile_options(-mavx2)
        endif()
    endif()
endif()

# OpenMP (optional on Windows)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found")
else()
    message(STATUS "OpenMP not found, single-threaded build")
endif()

# FetchContent for optional dependencies
include(FetchContent)

# Eigen (header-only)
find_package(Eigen3 3.3 CONFIG)
if(NOT Eigen3_FOUND)
    message(STATUS "Eigen3 not found via config, trying FetchContent")
    FetchContent_Declare(
        eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 3.4.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(eigen)
endif()

# fmt library (optional, fallback to FetchContent)
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    message(STATUS "fmt not found, using FetchContent")
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 10.2.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(fmt)
endif()

# Raylib (fetched automatically)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# Dear ImGui (docking branch for rlImGui compatibility)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# Build imgui as a library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR})

# rlImGui (Raylib binding for Dear ImGui)
FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(rlimgui)

# Build rlImGui as library
add_library(rlimgui_lib STATIC
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui_lib PUBLIC ${rlimgui_SOURCE_DIR})
target_link_libraries(rlimgui_lib PUBLIC imgui_lib raylib)

# EnTT (ECS)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(entt)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files (exclude .cu if no CUDA)
if(CMAKE_CUDA_COMPILER)
    file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.cu")
else()
    file(GLOB_RECURSE SOURCES "src/*.cpp")
endif()

# Create static library for reuse
add_library(isolated_lib STATIC ${SOURCES})
target_include_directories(isolated_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Link dependencies
target_link_libraries(isolated_lib PUBLIC fmt::fmt raylib rlimgui_lib EnTT::EnTT)

if(OpenMP_CXX_FOUND)
    target_link_libraries(isolated_lib PUBLIC OpenMP::OpenMP_CXX)
endif()

if(Eigen3_FOUND)
    target_link_libraries(isolated_lib PUBLIC Eigen3::Eigen)
else()
    target_link_libraries(isolated_lib PUBLIC eigen)
endif()

# Main executable
add_executable(isolated main.cpp)
target_link_libraries(isolated PRIVATE isolated_lib)

# Tests
enable_testing()
add_subdirectory(tests)
