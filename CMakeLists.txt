cmake_minimum_required(VERSION 3.24)
project(CppCoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# This project is consumed as a git submodule or built standalone; it is
# never `cmake --install`'d. Without this, CMake's Generate step fails
# whenever a dependency ends up being an in-tree FetchContent target
# rather than a real installed package (e.g. nlohmann_json auto-fetched
# below) -- external/CppLmmModelStore unconditionally runs install(EXPORT)
# and CMake validates that at generate time regardless of whether install
# will ever actually run.
set(CMAKE_SKIP_INSTALL_RULES ON)

include(FetchContent)

# Prefer an already-installed nlohmann_json (apt install
# nlohmann-json3-dev, vcpkg, conan, ...); otherwise fetch it ourselves.
# OVERRIDE_FIND_PACKAGE (CMake >= 3.24) makes every later find_package(nlohmann_json)
# call in this build -- including the one inside external/CppLmmModelStore --
# transparently resolve to this fetched copy instead of re-fetching or
# failing, so no system install is required on any platform.
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found on the system; fetching it instead")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.1
    GIT_SHALLOW TRUE
)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(httplib)

find_package(Threads REQUIRED)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/spdlog)

# nlohmann_json is resolved above (system copy or the OVERRIDE_FIND_PACKAGE
# fetch), so ModelStore's own find_package() call is satisfied and it does
# not need to FetchContent a second, export-incompatible copy.
set(MODELSTORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MODELSTORE_ALLOW_FETCHCONTENT OFF CACHE BOOL "" FORCE)
add_subdirectory(external/CppLmmModelStore)

add_subdirectory(src)

option(BUILD_EXAMPLES "Build examples/ (replay_demo, minimal_usage)" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
