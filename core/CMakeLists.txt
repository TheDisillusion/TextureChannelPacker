# ------------------------------------------------------------------------------
# libtcp_core — pure C++, no Qt, no GUI.
#
# This library is the single source of truth for image representation,
# channel routing, packing, and export. The Qt app, the CLI, and the test
# suite all consume it identically. Any pixel-touching code must live here.
# ------------------------------------------------------------------------------

set(TCP_CORE_PUBLIC_HEADERS
    include/tcp/channel_ref.h
    include/tcp/exporter.h
    include/tcp/image.h
    include/tcp/loader.h
    include/tcp/pack_job.h
    include/tcp/pixel_format.h
    include/tcp/resize.h
    include/tcp/version.h
)

set(TCP_CORE_SOURCES
    src/exporter.cpp
    src/image.cpp
    src/loader_oiio.cpp
    src/pack_job.cpp
    src/resize.cpp
    src/version.cpp
)

add_library(tcp_core STATIC
    ${TCP_CORE_PUBLIC_HEADERS}
    ${TCP_CORE_SOURCES}
)

add_library(tcp::core ALIAS tcp_core)

target_include_directories(tcp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(tcp_core PUBLIC cxx_std_20)

# OpenImageIO link is opt-in until Phase 1.
if(TCP_ENABLE_OIIO)
    find_package(OpenImageIO CONFIG REQUIRED)
    target_link_libraries(tcp_core PUBLIC OpenImageIO::OpenImageIO)
    target_compile_definitions(tcp_core PUBLIC TCP_HAVE_OIIO=1)
endif()
