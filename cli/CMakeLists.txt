# ------------------------------------------------------------------------------
# tcp_cli — headless command-line driver around tcp::core.
# Gated behind TCP_BUILD_CLI; enable with -DTCP_BUILD_CLI=ON or via the preset.
# ------------------------------------------------------------------------------

find_package(CLI11 CONFIG REQUIRED)

add_executable(tcp_cli
    src/main.cpp
)

set_target_properties(tcp_cli PROPERTIES
    OUTPUT_NAME tcp
)

target_link_libraries(tcp_cli PRIVATE
    tcp::core
    CLI11::CLI11
)
