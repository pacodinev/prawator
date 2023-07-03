cmake_minimum_required(VERSION 3.15)

# Cmake template from here:
# https://github.com/cpp-best-practices/cpp_starter_project

project(parwator CXX)
include(cmake/StandardProjectSettings.cmake)
include(cmake/PreventInSourceBuilds.cmake)

# Only do these if this is the main project, and not if it is included through add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  # Optionally set things like CMAKE_CXX_STANDARD, CMAKE_POSITION_INDEPENDENT_CODE here

  # Let's ensure -std=c++xx instead of -std=g++xx
  set(CMAKE_CXX_EXTENSIONS OFF)

  # Let's nicely support folders in IDEs
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  # Testing only available if this is the main app
  # Note this needs to be done in the main CMakeLists
  # since it calls enable_testing, which must be in the
  # main CMakeLists.
  include(CTest)

endif()

# FetchContent added in CMake 3.11, downloads during the configure step
include(FetchContent)
# FetchContent_MakeAvailable was not added until CMake 3.14; use our shim
if(${CMAKE_VERSION} VERSION_LESS 3.14)
  include(cmake/add_FetchContent_MakeAvailable.cmake)
endif()



# Link this 'library' to use for configuration
add_library(project_config INTERFACE)
target_compile_features(project_config INTERFACE cxx_std_17) # C++17 at least

# standard compiler warnings
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_config)

# sanitizer options if supported by compiler
include(cmake/Sanitizers.cmake)
enable_sanitizers(project_config)

add_subdirectory(external)

add_subdirectory(include)
add_subdirectory(src)

add_subdirectory(app)

# add_subdirectory(apps);

# Testing only available if this is the main app
# Emergency override provided as well
if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR PARWATOR_BUILD_TESTING)
   AND BUILD_TESTING)
  add_subdirectory(tests)
endif()

# Options and variables are here:

option(WATOR_BUILD_MAP_READER "Option to enable building of map_reader binary" ON)
option(WATOR_CPU_PIN "Optimisation: Pin tasks to CPUS" ON)
option(WATOR_NUMA "Add support for NUMA" ON)
option(WATOR_NUMA_OPTIMIZE "Optimize when the NUMA node is only one" ON)

# config.h gen:

configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

target_include_directories(project_config INTERFACE "${PROJECT_BINARY_DIR}")
