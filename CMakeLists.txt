cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(ENABLE_TESTS "Build unit tests" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

find_package(nlohmann_json 3.11 REQUIRED)

# Registry and validation: no libobs dependency, shared by the plugin and the unit tests
add_library(
  ${CMAKE_PROJECT_NAME}-core
  STATIC
  src/registry.cpp
  src/event-data.cpp
  src/state-events.cpp
  src/dock-logic.cpp
  src/rate-limit.cpp
  src/log-limiter.cpp
)
target_include_directories(${CMAKE_PROJECT_NAME}-core PUBLIC src)
target_link_libraries(${CMAKE_PROJECT_NAME}-core PUBLIC nlohmann_json::nlohmann_json)
# Linked into a shared module, so it needs -fPIC on Linux
set_property(TARGET ${CMAKE_PROJECT_NAME}-core PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(${CMAKE_PROJECT_NAME} MODULE)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_PROJECT_NAME}-core)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/procs.cpp
    src/signals.cpp
    src/websocket-vendor.cpp
    src/dock/dock-events.cpp
    src/dock/lua-bridge-dock.cpp
    src/dock/owner-section.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Ship the Lua helper and examples (lua/) in the plugin's data folder, in the
# same layout on every platform (<data>/lua/luabridge.lua, <data>/lua/examples/),
# so the examples' dofile(script_path() .. "../luabridge.lua") works as shipped.
# Also ship the license files with the plugin.
set(_license_files "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md")
if(OS_WINDOWS)
  install(DIRECTORY lua/ DESTINATION "${CMAKE_PROJECT_NAME}/data/lua")
  install(FILES ${_license_files} DESTINATION "${CMAKE_PROJECT_NAME}")
  install(FILES packaging/windows/INSTALL.txt DESTINATION .)
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/lua"
      "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${CMAKE_PROJECT_NAME}/lua"
    COMMENT "Copy lua/ to rundir"
    VERBATIM
  )
elseif(OS_MACOS)
  file(GLOB_RECURSE _lua_files "${CMAKE_CURRENT_SOURCE_DIR}/lua/*")
  foreach(_file IN LISTS _lua_files)
    cmake_path(RELATIVE_PATH _file BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE _relative)
    cmake_path(GET _relative PARENT_PATH _relative)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${_file}")
    set_property(SOURCE "${_file}" PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${_relative}")
  endforeach()
  foreach(_file IN LISTS _license_files)
    target_add_resource(${CMAKE_PROJECT_NAME} "${_file}")
  endforeach()
else()
  install(DIRECTORY lua/ DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/lua")
  install(FILES ${_license_files} DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${CMAKE_PROJECT_NAME}")
endif()

option(ENABLE_FUZZING "Build the libFuzzer + AddressSanitizer target for the core library (MSVC)" OFF)
if(ENABLE_FUZZING)
  # Core sources compiled again with sanitizers, so the whole core is instrumented
  add_executable(
    registry-fuzz
    tests/fuzz/registry-fuzz.cpp
    src/registry.cpp
    src/event-data.cpp
    src/state-events.cpp
    src/dock-logic.cpp
    src/rate-limit.cpp
    src/log-limiter.cpp
  )
  target_include_directories(registry-fuzz PRIVATE src)
  target_link_libraries(registry-fuzz PRIVATE nlohmann_json::nlohmann_json)
  # /GL (whole-program optimization, on by default in this template) doesn't mix with the sanitizers
  target_compile_options(registry-fuzz PRIVATE /fsanitize=address /fsanitize=fuzzer /GL- /Zi)
  target_link_options(registry-fuzz PRIVATE /LTCG:OFF /INCREMENTAL:NO)
endif()

if(ENABLE_TESTS)
  enable_testing()
  add_executable(registry-tests tests/registry-tests.cpp)
  target_link_libraries(registry-tests PRIVATE ${CMAKE_PROJECT_NAME}-core)
  add_test(NAME registry-tests COMMAND registry-tests)
endif()
