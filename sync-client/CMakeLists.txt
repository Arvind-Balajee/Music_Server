# Desktop CLI (Plan.md §12) that scans a local library and pushes changes to a
# running musicbox-server. GLOB is used intentionally (see shared/CMakeLists.txt)
# so the sync-client agent doesn't need to touch this file when adding sources.
file(GLOB_RECURSE MUSICBOX_SYNC_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER MUSICBOX_SYNC_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

if(MUSICBOX_SYNC_SOURCES)
    add_library(musicbox_sync_lib STATIC ${MUSICBOX_SYNC_SOURCES})
    target_include_directories(musicbox_sync_lib PUBLIC include)
    target_link_libraries(musicbox_sync_lib PUBLIC musicbox_shared nlohmann_json::nlohmann_json)
    target_link_libraries(musicbox_sync_lib PRIVATE musicbox_warnings)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
        add_executable(musicbox-sync src/main.cpp)
        target_link_libraries(musicbox-sync PRIVATE musicbox_sync_lib musicbox_warnings)
    endif()

    if(MUSICBOX_BUILD_TESTS)
        add_subdirectory(tests)
    endif()
endif()
