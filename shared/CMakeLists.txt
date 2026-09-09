# Domain models + cross-cutting utilities shared by the server and the sync
# client. GLOB is used intentionally here (not the usual CMake best practice) so
# agents adding source files under src/ don't need to touch this file and create
# merge conflicts across parallel worktrees (Plan.md §5/§27); re-run cmake
# configure after adding a new file.
file(GLOB_RECURSE MUSICBOX_SHARED_SOURCES CONFIGURE_DEPENDS src/*.cpp)

if(MUSICBOX_SHARED_SOURCES)
    add_library(musicbox_shared STATIC ${MUSICBOX_SHARED_SOURCES})
    target_include_directories(musicbox_shared PUBLIC include)
else()
    # No implementation files yet (header-only interfaces only).
    add_library(musicbox_shared INTERFACE)
    target_include_directories(musicbox_shared INTERFACE include)
endif()
