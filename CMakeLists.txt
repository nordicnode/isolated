cmake_minimum_required(VERSION 3.16)
project(Isolated VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# SIMD support
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
if(COMPILER_SUPPORTS_AVX2)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
endif()

# OpenMP
find_package(OpenMP REQUIRED)

# Eigen (header-only)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)

# fmt library
find_package(fmt REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create static library for reuse
add_library(isolated_lib STATIC ${SOURCES})
target_link_libraries(isolated_lib 
    PUBLIC 
    OpenMP::OpenMP_CXX 
    Eigen3::Eigen
    fmt::fmt
)
target_include_directories(isolated_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Main executable
add_executable(isolated main.cpp)
target_link_libraries(isolated PRIVATE isolated_lib)

# Tests
enable_testing()
add_subdirectory(tests)
