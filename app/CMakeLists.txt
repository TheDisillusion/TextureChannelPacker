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
)

qt_add_executable(tcp_app
    ${TCP_APP_SOURCES}
)

set_target_properties(tcp_app PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME TextureChannelPacker
)

target_link_libraries(tcp_app PRIVATE
    tcp::core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
