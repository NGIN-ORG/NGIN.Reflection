cmake_minimum_required(VERSION 3.16)
project(NGINReflectionTests LANGUAGES CXX)

set(_reflection_root_dir "${NGIN_REFLECTION_ROOT_DIR}")
if(NOT _reflection_root_dir)
  get_filename_component(_reflection_root_dir "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()

include("${_reflection_root_dir}/cmake/CPM.cmake")
include(CTest)

set(CPM_SOURCE_CACHE "${PROJECT_BINARY_DIR}/cpm-cache" CACHE PATH "CPM cache")

CPMAddPackage(
  NAME Catch2
  VERSION 3.5.3
  GITHUB_REPOSITORY catchorg/Catch2
  OPTIONS
    "CATCH_ENABLE_COVERAGE OFF"
    "CATCH_ENABLE_WERROR OFF"
    "CATCH_INSTALL_DOCS OFF"
    "CATCH_INSTALL_EXTRAS OFF"
)

if(Catch2_SOURCE_DIR)
  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
endif()
if(Catch2_DIR)
  list(APPEND CMAKE_MODULE_PATH ${Catch2_DIR})
endif()
include(Catch)

add_library(ngin_reflection_test_config INTERFACE)
target_link_libraries(ngin_reflection_test_config INTERFACE NGIN::Reflection)
target_compile_features(ngin_reflection_test_config INTERFACE cxx_std_23)

set(REFLECTION_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Reflection/UnifiedRuntime.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Reflection/ModuleReplace.cpp
)

foreach(test_src IN LISTS REFLECTION_TEST_SOURCES)
  get_filename_component(test_name "${test_src}" NAME_WE)
  set(exe_name "Reflection_${test_name}Tests")
  add_executable(${exe_name} ${test_src})
  target_link_libraries(${exe_name} PRIVATE Catch2::Catch2WithMain ngin_reflection_test_config)
  catch_discover_tests(${exe_name}
    TEST_PREFIX "Reflection.${test_name}."
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${exe_name}>)
endforeach()

if(NGIN_REFLECTION_ENABLE_ABI)
  add_library(InteropPluginA SHARED
    ${_reflection_root_dir}/src/Registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interop/PluginA.cpp)
  target_include_directories(InteropPluginA PRIVATE ${_reflection_root_dir}/include)
  target_link_libraries(InteropPluginA PRIVATE NGIN::Base::Foundation)
  target_compile_features(InteropPluginA PRIVATE cxx_std_23)
  target_compile_definitions(InteropPluginA PRIVATE NGIN_REFLECTION_ENABLE_ABI=1 NGIN_REFLECTION_EXPORTS)

  add_library(InteropPluginB SHARED
    ${_reflection_root_dir}/src/Registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interop/PluginB.cpp)
  target_include_directories(InteropPluginB PRIVATE ${_reflection_root_dir}/include)
  target_link_libraries(InteropPluginB PRIVATE NGIN::Base::Foundation)
  target_compile_features(InteropPluginB PRIVATE cxx_std_23)
  target_compile_definitions(InteropPluginB PRIVATE NGIN_REFLECTION_ENABLE_ABI=1 NGIN_REFLECTION_EXPORTS)

  add_executable(Interop_Host ${CMAKE_CURRENT_SOURCE_DIR}/Interop/Host.cpp)
  target_link_libraries(Interop_Host PRIVATE Catch2::Catch2WithMain ngin_reflection_test_config)
  add_dependencies(Interop_Host InteropPluginA InteropPluginB)
  catch_discover_tests(Interop_Host
    TEST_PREFIX "Interop.Host."
    WORKING_DIRECTORY $<TARGET_FILE_DIR:Interop_Host>)

  foreach(tgt IN ITEMS InteropPluginA InteropPluginB)
    set_target_properties(${tgt} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:Interop_Host>
      LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:Interop_Host>
      ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      POSITION_INDEPENDENT_CODE ON)
  endforeach()

  if(APPLE)
    set(_reflection_origin "@loader_path")
  else()
    set(_reflection_origin "$ORIGIN")
  endif()
  set_target_properties(Interop_Host PROPERTIES BUILD_RPATH "${_reflection_origin}")
  set_target_properties(InteropPluginA InteropPluginB PROPERTIES BUILD_RPATH "${_reflection_origin}")
endif()
