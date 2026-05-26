cmake_minimum_required(VERSION 3.20)
project(TCM VERSION 1.0.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++ standard & compile flags
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

# ---------------------------------------------------------------------------
# Option: AddressSanitizer
# ---------------------------------------------------------------------------
option(TCM_ENABLE_ASAN "Build with AddressSanitizer" OFF)
if(TCM_ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# ---------------------------------------------------------------------------
# Option: Static binary
# ---------------------------------------------------------------------------
option(TCM_STATIC "Build static binary" OFF)
if(TCM_STATIC)
    message(STATUS "Static binary enabled")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    set(BUILD_SHARED_LIBS OFF)
endif()

# ---------------------------------------------------------------------------
# FetchContent: Google Test
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# FetchContent: nlohmann/json
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# System packages
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

# libssh2
pkg_check_modules(LIBSSH2 REQUIRED libssh2)
message(STATUS "libssh2 version: ${LIBSSH2_VERSION}")

# ncurses
find_package(Curses REQUIRED)
message(STATUS "Curses found: ${CURSES_LIBRARIES}")

# OpenSSL
find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")

# ---------------------------------------------------------------------------
# Global include directories (shared across all targets)
# ---------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Enable CTest
# ---------------------------------------------------------------------------
enable_testing()
include(GoogleTest)

# ---------------------------------------------------------------------------
# Source subdirectories → library targets
# ---------------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/protocol)
add_subdirectory(src/tui)
add_subdirectory(src/logger)
add_subdirectory(src/macro)
add_subdirectory(src/credential)
add_subdirectory(src/app)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(tcm src/main.cpp)
target_link_libraries(tcm
    PRIVATE
        tcm_app
        tcm_core
        tcm_protocol
        tcm_tui
        tcm_logger
        tcm_macro
        tcm_credential
)

# ---------------------------------------------------------------------------
# Test subdirectories (delegated to tests/CMakeLists.txt)
# ---------------------------------------------------------------------------
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Smoke test executable (not a CTest test, just a runnable binary)
# ---------------------------------------------------------------------------
add_executable(tcm_smoke_test src/smoke_test.cpp)
target_link_libraries(tcm_smoke_test
    PRIVATE
        tcm_core
        tcm_logger
)
