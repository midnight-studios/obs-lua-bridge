# Plugin bootstrap module

include_guard(GLOBAL)

# Map fallback configurations for optimized build configurations
# gersemi: off
set(
  CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO
    RelWithDebInfo
    Release
    MinSizeRel
    None
    ""
)
set(
  CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL
    MinSizeRel
    Release
    RelWithDebInfo
    None
    ""
)
set(
  CMAKE_MAP_IMPORTED_CONFIG_RELEASE
    Release
    RelWithDebInfo
    MinSizeRel
    None
    ""
)
# gersemi: on

# Prohibit in-source builds
if("${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  message(
    FATAL_ERROR
    "In-source builds are not supported. "
    "Specify a build directory via 'cmake -S <SOURCE DIRECTORY> -B <BUILD_DIRECTORY>' instead."
  )
  file(REMOVE_RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/CMakeCache.txt" "${CMAKE_CURRENT_SOURCE_DIR}/CMakeFiles")
endif()

# Add common module directories to default search path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/common")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" buildspec)

string(JSON _name GET ${buildspec} name)
string(JSON _website GET ${buildspec} website)
string(JSON _author GET ${buildspec} author)
string(JSON _email GET ${buildspec} email)
string(JSON _version GET ${buildspec} version)
string(JSON _bundleId GET ${buildspec} platformConfig macos bundleId)

set(PLUGIN_AUTHOR ${_author})
set(PLUGIN_WEBSITE ${_website})
set(PLUGIN_EMAIL ${_email})
set(MACOS_BUNDLEID ${_bundleId})

# The version may carry a pre-release suffix ("0.9.0-beta1"). PLUGIN_VERSION is
# the full string: what the plugin reports, and the package names. project(),
# the Windows version resource and the macOS bundle version only accept
# numbers, so _version (used by project()) and the MAJOR/MINOR/PATCH parts are
# the numeric "0.9.0".
if(NOT _version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[0-9A-Za-z]+)?$")
  message(FATAL_ERROR "buildspec.json version '${_version}' must be X.Y.Z or X.Y.Z-<suffix> (e.g. 0.9.0-beta1)")
endif()
set(PLUGIN_VERSION ${_version})
set(PLUGIN_VERSION_MAJOR ${CMAKE_MATCH_1})
set(PLUGIN_VERSION_MINOR ${CMAKE_MATCH_2})
set(PLUGIN_VERSION_PATCH ${CMAKE_MATCH_3})
set(_version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
set(PLUGIN_VERSION_NUMERIC ${_version})

include(buildnumber)
include(osconfig)

# Allow selection of common build types via UI
if(NOT CMAKE_GENERATOR MATCHES "(Xcode|Visual Studio .+)")
  if(NOT CMAKE_BUILD_TYPE)
    set(
      CMAKE_BUILD_TYPE
      "RelWithDebInfo"
      CACHE STRING
      "OBS build type [Release, RelWithDebInfo, Debug, MinSizeRel]"
      FORCE
    )
    set_property(
      CACHE CMAKE_BUILD_TYPE
      PROPERTY STRINGS Release RelWithDebInfo Debug MinSizeRel
    )
  endif()
endif()

# Disable exports automatically going into the CMake package registry
set(CMAKE_EXPORT_PACKAGE_REGISTRY FALSE)
# Enable default inclusion of targets' source and binary directory
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
