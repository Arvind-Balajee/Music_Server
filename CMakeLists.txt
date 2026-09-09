cmake_minimum_required(VERSION 3.24)
project(musicbox VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(MUSICBOX_BUILD_TESTS "Build the musicbox test suite" ON)

# Strict warnings on the compilers we actually target; do not force-enable flags
# a given toolchain doesn't support (Plan.md §28).
add_library(musicbox_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(musicbox_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Opt-in sanitizer builds: -DMUSICBOX_SANITIZE=address,undefined
if(MUSICBOX_SANITIZE)
    add_library(musicbox_sanitizers INTERFACE)
    target_compile_options(musicbox_sanitizers INTERFACE -fsanitize=${MUSICBOX_SANITIZE} -fno-omit-frame-pointer)
    target_link_options(musicbox_sanitizers INTERFACE -fsanitize=${MUSICBOX_SANITIZE})
endif()

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

if(MUSICBOX_BUILD_TESTS)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.6.0
    )
    FetchContent_MakeAvailable(catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    enable_testing()
endif()

add_subdirectory(shared)
add_subdirectory(server)

if(MUSICBOX_BUILD_TESTS)
    add_subdirectory(tests)
endif()
