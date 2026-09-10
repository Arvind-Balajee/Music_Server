# Own test binary for sync-client (kept separate from tests/musicbox_tests so
# this agent doesn't need to touch anything outside sync-client/ -- see the
# sync-client agent's charter). GLOB is used intentionally (see
# shared/CMakeLists.txt) so new test files don't require editing this list.
file(GLOB_RECURSE MUSICBOX_SYNC_TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

add_executable(musicbox_sync_tests ${MUSICBOX_SYNC_TEST_SOURCES})
target_link_libraries(musicbox_sync_tests
    PRIVATE
        musicbox_sync_lib
        musicbox_shared
        Catch2::Catch2WithMain
        musicbox_warnings
)

include(CTest)
include(Catch)
catch_discover_tests(musicbox_sync_tests)
