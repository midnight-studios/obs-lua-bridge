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
add_library(${CMAKE_PROJECT_NAME}-core STATIC src/registry.cpp src/event-data.cpp src/state-events.cpp)
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

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.cpp src/procs.cpp src/signals.cpp src/websocket-vendor.cpp)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(ENABLE_TESTS)
  enable_testing()
  add_executable(registry-tests tests/registry-tests.cpp)
  target_link_libraries(registry-tests PRIVATE ${CMAKE_PROJECT_NAME}-core)
  add_test(NAME registry-tests COMMAND registry-tests)
endif()
