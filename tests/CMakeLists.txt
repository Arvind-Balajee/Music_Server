# GLOB is used intentionally (see shared/CMakeLists.txt) so each agent can drop
# test files under tests/unit or tests/integration without editing this file.
file(GLOB_RECURSE MUSICBOX_TEST_SOURCES CONFIGURE_DEPENDS unit/*.cpp integration/*.cpp)

add_executable(musicbox_tests ${MUSICBOX_TEST_SOURCES})
target_link_libraries(musicbox_tests
    PRIVATE
        musicbox_server_lib
        musicbox_shared
        Catch2::Catch2WithMain
        musicbox_warnings
)

include(CTest)
include(Catch)
catch_discover_tests(musicbox_tests)
