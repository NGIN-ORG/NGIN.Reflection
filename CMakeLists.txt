cmake_minimum_required(VERSION 3.16)
project(NGIN.Reflection VERSION 0.1 LANGUAGES CXX)

#-------------------------------------------------------------------------------
# Policies
#-------------------------------------------------------------------------------
cmake_policy(SET CMP0077 NEW)

#-------------------------------------------------------------------------------
# Options
#-------------------------------------------------------------------------------
option(NGIN_REFLECTION_BUILD_TESTS "Build NGIN.Reflection tests" ON)
option(NGIN_REFLECTION_BUILD_EXAMPLES "Build NGIN.Reflection examples" OFF)
option(NGIN_REFLECTION_BUILD_BENCHMARKS "Build NGIN.Reflection benchmarks" OFF)
option(NGIN_REFLECTION_ENABLE_ABI "Build ABI export entrypoint" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure NGIN.Base subproject keeps quiet stuff off here
set(NGIN_BASE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NGIN_BASE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NGIN_BASE_DEVELOPMENT_MODE OFF CACHE BOOL "" FORCE)
set(NGIN_BASE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

#-------------------------------------------------------------------------------
# Locate NGIN.Base
#-------------------------------------------------------------------------------
if(NOT TARGET NGIN::Base)
  find_package(NGINBase CONFIG QUIET)
  if(NOT NGINBase_FOUND)
    set(_base_dir "${CMAKE_CURRENT_LIST_DIR}/../NGIN.Base")
    if(EXISTS "${_base_dir}/CMakeLists.txt")
      message(STATUS "NGIN.Reflection: using sibling NGIN.Base from ${_base_dir}")
      set(NGIN_BASE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(NGIN_BASE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(NGIN_BASE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
      add_subdirectory("${_base_dir}" _ngin_base EXCLUDE_FROM_ALL)
    elseif(DEFINED NGIN_BASE_SOURCE_DIR)
      message(STATUS "NGIN.Reflection: using NGIN.Base from ${NGIN_BASE_SOURCE_DIR}")
      set(NGIN_BASE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(NGIN_BASE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(NGIN_BASE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
      add_subdirectory("${NGIN_BASE_SOURCE_DIR}" _ngin_base EXCLUDE_FROM_ALL)
    else()
      message(FATAL_ERROR "NGIN.Base not found. Install NGINBase or set NGIN_BASE_SOURCE_DIR to the source path.")
    endif()
  endif()
endif()

#-------------------------------------------------------------------------------
# Library Definition (shared/static controlled by BUILD_SHARED_LIBS)
#-------------------------------------------------------------------------------
add_library(NGIN.Reflection
  src/Registry.cpp
)
if(NGIN_REFLECTION_ENABLE_ABI)
  target_sources(NGIN.Reflection PRIVATE src/ABI.cpp)
  target_compile_definitions(NGIN.Reflection PUBLIC NGIN_REFLECTION_ENABLE_ABI=1)
  target_sources(NGIN.Reflection PRIVATE src/ABIMerge.cpp)
endif()

target_compile_features(NGIN.Reflection PUBLIC cxx_std_23)

target_include_directories(NGIN.Reflection
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(NGIN.Reflection
  PUBLIC
    NGIN::Base
)

# Platform and export macros (avoid generator expression on TYPE without target)
target_compile_definitions(NGIN.Reflection
  PUBLIC
    $<$<STREQUAL:$<PLATFORM_ID>,Windows>:NGIN_PLATFORM=\"Windows\"> 
    $<$<STREQUAL:$<PLATFORM_ID>,Darwin>:NGIN_PLATFORM=\"macOS\"> 
    $<$<STREQUAL:$<PLATFORM_ID>,Linux>:NGIN_PLATFORM=\"Linux\"> 
)

# Determine library type to set static/shared/export defines reliably for consumers
get_target_property(_ngin_reflection_type NGIN.Reflection TYPE)
if(_ngin_reflection_type STREQUAL "STATIC_LIBRARY")
  target_compile_definitions(NGIN.Reflection PUBLIC NGIN_REFLECTION_STATIC=1)
else()
  # Shared or module
  target_compile_definitions(NGIN.Reflection PUBLIC NGIN_REFLECTION_SHARED=1 NGIN_REFLECTION_EXPORTS=1)
endif()

add_library(NGIN::Reflection ALIAS NGIN.Reflection)

# Add -latomic if needed
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_libraries(NGIN.Reflection PUBLIC atomic)
endif()

#-------------------------------------------------------------------------------
# Installation
#-------------------------------------------------------------------------------
include(GNUInstallDirs)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS NGIN.Reflection EXPORT NGINReflectionTargets)

install(
  EXPORT NGINReflectionTargets
  FILE NGINReflectionTargets.cmake
  NAMESPACE NGIN::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NGINReflection
)

# Package config
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/NGINReflectionConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NGINReflectionConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/NGINReflectionConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NGINReflection
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NGINReflectionConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NGINReflectionConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NGINReflection
)

#-------------------------------------------------------------------------------
# Tests
#-------------------------------------------------------------------------------
if(NGIN_REFLECTION_BUILD_TESTS)
  include(CTest)
  enable_testing()
  set(NGIN_REFLECTION_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  set(NGIN_REFLECTION_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
  add_subdirectory(tests)
endif()

#-------------------------------------------------------------------------------
# Examples
#-------------------------------------------------------------------------------
if(NGIN_REFLECTION_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

#-------------------------------------------------------------------------------
# Benchmarks
#-------------------------------------------------------------------------------
if(NGIN_REFLECTION_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
