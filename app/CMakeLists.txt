# ------------------------------------------------------------------------------
# tcp_app — Qt 6 desktop application.
#
# This target is gated behind TCP_BUILD_APP because Qt is a heavy dependency
# and Phase 0 / Phase 1 do not require it. Enable with:
#     cmake -DTCP_BUILD_APP=ON ...
# ------------------------------------------------------------------------------

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

qt_standard_project_setup()

set(TCP_APP_SOURCES
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/job_controller.cpp
    src/job_controller.h
    src/input_slot_widget.cpp
    src/input_slot_widget.h
    src/output_panel.cpp
    src/output_panel.h
)

qt_add_executable(tcp_app
    ${TCP_APP_SOURCES}
)

qt_add_resources(tcp_app "tcp_resources"
    PREFIX "/tcp"
    BASE   ${CMAKE_CURRENT_SOURCE_DIR}/resources
    FILES  resources/theme/dark.qss
)

set_target_properties(tcp_app PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME TextureChannelPacker
    AUTOMOC ON
)

target_include_directories(tcp_app PRIVATE src)

target_link_libraries(tcp_app PRIVATE
    tcp::core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

# Qt plugin deployment: vcpkg's applocal.ps1 post-link hook copies the Qt DLLs
# next to the executable, but plugins are a separate concern. We copy the two
# we care about — `platforms/` (required for QGuiApplication to start) and
# `imageformats/` (needed for thumbnails of common formats) — from the vcpkg
# install tree.
if(WIN32)
    set(_tcp_vcpkg_install "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
    set(_tcp_plugins_debug   "${_tcp_vcpkg_install}/debug/Qt6/plugins")
    set(_tcp_plugins_release "${_tcp_vcpkg_install}/Qt6/plugins")
    add_custom_command(TARGET tcp_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${_tcp_plugins_debug}/platforms,${_tcp_plugins_release}/platforms>"
            "$<TARGET_FILE_DIR:tcp_app>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${_tcp_plugins_debug}/imageformats,${_tcp_plugins_release}/imageformats>"
            "$<TARGET_FILE_DIR:tcp_app>/imageformats"
        COMMENT "Deploying Qt plugins (platforms, imageformats)"
        VERBATIM
    )
endif()
