# ------------------------------------------------------------------------------
# Unit tests for tcp::core, powered by Catch2 v3.
# ------------------------------------------------------------------------------

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

set(TCP_TEST_SOURCES
    test_smoke.cpp
)

add_executable(tcp_tests ${TCP_TEST_SOURCES})

target_link_libraries(tcp_tests PRIVATE
    tcp::core
    Catch2::Catch2WithMain
)

catch_discover_tests(tcp_tests)
