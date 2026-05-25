cmake_minimum_required(VERSION 3.24)

project(TextureChannelPacker
    VERSION 0.3.0
    DESCRIPTION "Native desktop tool for packing R/G/B/A channels of texture maps."
    LANGUAGES CXX
)

# ------------------------------------------------------------------------------
# Project-wide settings
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Predictable runtime layout: every executable and DLL lands next to the app.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Treat warnings as errors in our own code, not in third-party dependencies.
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor /utf-8)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

# ------------------------------------------------------------------------------
# Phase-gated dependency features.
#
# Each Phase enables additional vcpkg features through TCP_ENABLE_* options.
# Phase 0 only needs Catch2 for the test harness, so by default we keep the
# heavier deps (Qt, OpenImageIO) disabled. As later phases land they will flip
# their corresponding option ON via the top-level CMake or a preset override.
# ------------------------------------------------------------------------------
option(TCP_BUILD_TESTS "Build the unit test suite"      ON)
option(TCP_BUILD_APP   "Build the Qt desktop application (Phase 2+)" OFF)
option(TCP_BUILD_CLI   "Build the headless CLI (Phase 5+)"           OFF)
option(TCP_ENABLE_OIIO "Link OpenImageIO into the core (Phase 1+)"   OFF)
option(TCP_ENABLE_DDS  "Link DirectXTex for DDS + BC encoding (Phase 6+)" OFF)

# ------------------------------------------------------------------------------
# Sub-targets
# ------------------------------------------------------------------------------
add_subdirectory(core)

if(TCP_BUILD_APP)
    add_subdirectory(app)
endif()

if(TCP_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(TCP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
message(STATUS "")
message(STATUS "TextureChannelPacker ${PROJECT_VERSION}")
message(STATUS "  CXX compiler   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Generator      : ${CMAKE_GENERATOR}")
message(STATUS "  Build tests    : ${TCP_BUILD_TESTS}")
message(STATUS "  Build app      : ${TCP_BUILD_APP}")
message(STATUS "  Build CLI      : ${TCP_BUILD_CLI}")
message(STATUS "  OpenImageIO    : ${TCP_ENABLE_OIIO}")
message(STATUS "  DirectXTex     : ${TCP_ENABLE_DDS}")
message(STATUS "")
