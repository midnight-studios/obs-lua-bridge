# Standalone build of the core library and its unit tests, for CI on all
# platforms. The core needs no OBS, Qt or obs-deps, so this builds in a minute
# where the plugin build needs the full OBS dependencies.
#
#   cmake -S tests -B build_tests
#   cmake --build build_tests --config Release
#   ctest --test-dir build_tests -C Release --output-on-failure
cmake_minimum_required(VERSION 3.24)

project(obs-lua-bridge-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(nlohmann_json)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}/../src")

# Same sources as obs-lua-bridge-core in the top-level CMakeLists.txt
add_library(
  obs-lua-bridge-core
  STATIC
  ${_src}/registry.cpp
  ${_src}/event-data.cpp
  ${_src}/state-events.cpp
  ${_src}/dock-logic.cpp
  ${_src}/rate-limit.cpp
  ${_src}/log-limiter.cpp
)
target_include_directories(obs-lua-bridge-core PUBLIC ${_src})
target_link_libraries(obs-lua-bridge-core PUBLIC nlohmann_json::nlohmann_json)

add_executable(registry-tests registry-tests.cpp)
target_link_libraries(registry-tests PRIVATE obs-lua-bridge-core)

if(MSVC)
  # The plugin's MSVC options (cmake/windows/compilerconfig.cmake). The
  # conforming preprocessor is required: the tests pass raw string literals as
  # macro arguments, which the traditional preprocessor in VS 2022 mishandles.
  set(_msvc_options /W4 /utf-8 /Zc:__cplusplus /Zc:preprocessor)
  target_compile_options(obs-lua-bridge-core PRIVATE ${_msvc_options})
  target_compile_options(registry-tests PRIVATE ${_msvc_options})
else()
  target_compile_options(obs-lua-bridge-core PRIVATE -Wall -Wextra)
  target_compile_options(registry-tests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME registry-tests COMMAND registry-tests)
