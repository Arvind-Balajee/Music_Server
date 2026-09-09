find_package(Threads REQUIRED)
find_package(SQLite3)

# GLOB is used intentionally (see shared/CMakeLists.txt) so Agents 1-4 can add
# source files under their own subdirectory (net/, http/, db/, library/,
# streaming/, config/) without editing this shared build file. main.cpp is built
# into the musicbox-server executable, not the library, so tests can link the
# library without pulling in main().
file(GLOB_RECURSE MUSICBOX_SERVER_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER MUSICBOX_SERVER_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

if(MUSICBOX_SERVER_SOURCES)
    add_library(musicbox_server_lib STATIC ${MUSICBOX_SERVER_SOURCES})
    set(MUSICBOX_SERVER_LIB_SCOPE PUBLIC)
    set(MUSICBOX_SERVER_LIB_PRIVATE_SCOPE PRIVATE)
else()
    # No implementations landed yet (interface headers only) — keep the build
    # green until Agents 1-4 add their first source files.
    add_library(musicbox_server_lib INTERFACE)
    set(MUSICBOX_SERVER_LIB_SCOPE INTERFACE)
    set(MUSICBOX_SERVER_LIB_PRIVATE_SCOPE INTERFACE)
endif()

target_include_directories(musicbox_server_lib ${MUSICBOX_SERVER_LIB_SCOPE} include)
target_link_libraries(musicbox_server_lib
    ${MUSICBOX_SERVER_LIB_SCOPE}
        musicbox_shared
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
        Threads::Threads
)
target_link_libraries(musicbox_server_lib ${MUSICBOX_SERVER_LIB_PRIVATE_SCOPE} musicbox_warnings)

if(SQLite3_FOUND)
    # CMake >= 3.31 provides the non-deprecated SQLite3::SQLite3 name; older
    # CMake (down to our 3.24 minimum) only has the SQLite::SQLite3 alias.
    if(TARGET SQLite3::SQLite3)
        target_link_libraries(musicbox_server_lib ${MUSICBOX_SERVER_LIB_SCOPE} SQLite3::SQLite3)
    else()
        target_link_libraries(musicbox_server_lib ${MUSICBOX_SERVER_LIB_SCOPE} SQLite::SQLite3)
    endif()
else()
    message(WARNING "System SQLite3 not found; Agent 3 must wire up a FetchContent fallback in server/CMakeLists.txt before the db/ sources will link.")
endif()

if(TARGET musicbox_sanitizers)
    target_link_libraries(musicbox_server_lib ${MUSICBOX_SERVER_LIB_SCOPE} musicbox_sanitizers)
endif()

add_executable(musicbox-server src/main.cpp)
target_link_libraries(musicbox-server PRIVATE musicbox_server_lib musicbox_warnings)
