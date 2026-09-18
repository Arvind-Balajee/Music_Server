# Domain models + cross-cutting utilities shared by the server and the sync
# client. GLOB is used intentionally here (not the usual CMake best practice) so
# agents adding source files under src/ don't need to touch this file and create
# merge conflicts across parallel worktrees (Plan.md §5/§27); re-run cmake
# configure after adding a new file.
file(GLOB_RECURSE MUSICBOX_SHARED_SOURCES CONFIGURE_DEPENDS src/*.cpp)

if(MUSICBOX_SHARED_SOURCES)
    # OpenSSL backs util/Sha256.cpp (docs/adr/0010-use-openssl-for-sha256.md).
    # Homebrew's openssl@3 is keg-only -- installed but not linked into the
    # Homebrew prefix -- so FindOpenSSL can't see it without a root hint.
    if(APPLE AND NOT OPENSSL_ROOT_DIR)
        execute_process(
            COMMAND brew --prefix openssl@3
            OUTPUT_VARIABLE MUSICBOX_BREW_OPENSSL_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE MUSICBOX_BREW_OPENSSL_RESULT)
        if(MUSICBOX_BREW_OPENSSL_RESULT EQUAL 0 AND EXISTS "${MUSICBOX_BREW_OPENSSL_PREFIX}")
            set(OPENSSL_ROOT_DIR "${MUSICBOX_BREW_OPENSSL_PREFIX}")
        endif()
    endif()
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)

    add_library(musicbox_shared STATIC ${MUSICBOX_SHARED_SOURCES})
    target_include_directories(musicbox_shared PUBLIC include)
    target_link_libraries(musicbox_shared PRIVATE OpenSSL::Crypto)
else()
    # No implementation files yet (header-only interfaces only).
    add_library(musicbox_shared INTERFACE)
    target_include_directories(musicbox_shared INTERFACE include)
endif()
