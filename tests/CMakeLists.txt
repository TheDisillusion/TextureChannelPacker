# ------------------------------------------------------------------------------
# Unit tests for tcp::core, powered by Catch2 v3.
# ------------------------------------------------------------------------------

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

set(TCP_TEST_SOURCES
    test_smoke.cpp
    test_image.cpp
    test_channel_ref.cpp
    test_resize.cpp
    test_io_roundtrip.cpp
    test_pack_job.cpp
    test_preset.cpp
    test_project.cpp
)

if(TCP_ENABLE_DDS)
    list(APPEND TCP_TEST_SOURCES test_dds.cpp)
endif()

add_executable(tcp_tests ${TCP_TEST_SOURCES})

target_link_libraries(tcp_tests PRIVATE
    tcp::core
    Catch2::Catch2WithMain
)

catch_discover_tests(tcp_tests)

# CLI smoke test (only registered when tcp_cli is being built). Runs the real
# binary with `list-presets` and asserts at least one preset appears in stdout.
if(TARGET tcp_cli)
    add_test(NAME cli_list_presets
             COMMAND $<TARGET_FILE:tcp_cli> list-presets)
    set_tests_properties(cli_list_presets PROPERTIES
        PASS_REGULAR_EXPRESSION "unreal-orm"
        LABELS "cli")
endif()
