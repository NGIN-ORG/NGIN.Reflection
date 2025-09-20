cmake_minimum_required(VERSION 3.16)
project(NGINReflectionTests LANGUAGES CXX)

# Determine the top-level source/binary directories even when this subproject is
# included from an aggregate build.
set(_reflection_root_dir "${NGIN_REFLECTION_ROOT_DIR}")
if(NOT _reflection_root_dir)
  get_filename_component(_reflection_root_dir "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()
set(_reflection_binary_dir "${NGIN_REFLECTION_BINARY_DIR}")
if(NOT _reflection_binary_dir)
  get_filename_component(_reflection_binary_dir "${CMAKE_CURRENT_BINARY_DIR}/.." ABSOLUTE)
endif()

include("${_reflection_root_dir}/cmake/CPM.cmake")
include(CTest)

# Cache third-party sources to speed up repeated configuration runs (CI, IDE refresh)
set(CPM_SOURCE_CACHE "${PROJECT_BINARY_DIR}/cpm-cache" CACHE PATH "CPM cache")

# ---------------------------------------------------------------------------
# Pull in Catch2 via CPM
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Common test config / options
# ---------------------------------------------------------------------------
add_library(ngin_reflection_test_config INTERFACE)
target_link_libraries(ngin_reflection_test_config INTERFACE NGIN::Reflection)
target_compile_features(ngin_reflection_test_config INTERFACE cxx_std_23)

option(REFLECTION_TESTS_ONE_EXE_PER_FILE "Split each test source into its own executable" ON)
option(REFLECTION_ENABLE_ASAN "Enable Address/Undefined sanitizers for tests" OFF)

# Gather every test source so we can either create per-file binaries or a single aggregator
file(GLOB ROOT_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
set(REFLECTION_ALL_TEST_SRCS ${ROOT_TEST_SRCS})

file(GLOB CHILDREN RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(child IN LISTS CHILDREN)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}" AND NOT child STREQUAL "Interop")
    file(GLOB SUBDIR_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/*.cpp")
    list(APPEND REFLECTION_ALL_TEST_SRCS ${SUBDIR_SRCS})
  endif()
endforeach()

set(REFLECTION_TEST_TARGETS)

if(REFLECTION_TESTS_ONE_EXE_PER_FILE)
  foreach(test_src IN LISTS ROOT_TEST_SRCS)
    get_filename_component(test_name "${test_src}" NAME_WE)
    set(exe_name "Reflection_${test_name}")
    message(STATUS "Adding test executable: ${exe_name} from ${test_src}")
    add_executable(${exe_name} ${test_src})
    list(APPEND REFLECTION_TEST_TARGETS ${exe_name})
    target_link_libraries(${exe_name} PRIVATE Catch2::Catch2WithMain ngin_reflection_test_config)
    set_target_properties(${exe_name} PROPERTIES FOLDER "Tests")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${test_src})
    set(suite_name "${test_name}")
    string(REGEX REPLACE "Tests$" "" suite_name "${suite_name}")
    set(test_prefix "Reflection.${suite_name}.")
    catch_discover_tests(${exe_name}
      TEST_PREFIX "${test_prefix}"
      WORKING_DIRECTORY $<TARGET_FILE_DIR:${exe_name}>)
  endforeach()

  foreach(child IN LISTS CHILDREN)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}" AND NOT child STREQUAL "Interop")
      file(GLOB SUBDIR_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/*.cpp")
      foreach(test_src IN LISTS SUBDIR_SRCS)
        get_filename_component(test_name "${test_src}" NAME_WE)
        string(REGEX REPLACE "[^A-Za-z0-9_]" "_" child_safe "${child}")
        set(exe_name "${child_safe}_${test_name}")
        message(STATUS "Adding test executable: ${exe_name} from ${test_src}")
        add_executable(${exe_name} ${test_src})
        list(APPEND REFLECTION_TEST_TARGETS ${exe_name})
        target_link_libraries(${exe_name} PRIVATE Catch2::Catch2WithMain ngin_reflection_test_config)
        set_target_properties(${exe_name} PROPERTIES FOLDER "Tests")
        source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${test_src})
        set(suite_name "${test_name}")
        string(REGEX REPLACE "Tests$" "" suite_name "${suite_name}")
        if(child_safe STREQUAL "Reflection")
          set(test_prefix "Reflection.${suite_name}.")
        else()
          set(test_prefix "${child_safe}.${suite_name}.")
        endif()
        catch_discover_tests(${exe_name}
          TEST_PREFIX "${test_prefix}"
          WORKING_DIRECTORY $<TARGET_FILE_DIR:${exe_name}>)
      endforeach()
    endif()
  endforeach()
else()
  add_executable(Reflection_AllTests ${REFLECTION_ALL_TEST_SRCS})
  list(APPEND REFLECTION_TEST_TARGETS Reflection_AllTests)
  target_link_libraries(Reflection_AllTests PRIVATE Catch2::Catch2WithMain ngin_reflection_test_config)
  set_target_properties(Reflection_AllTests PROPERTIES FOLDER "Tests")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${REFLECTION_ALL_TEST_SRCS})
  catch_discover_tests(Reflection_AllTests
    TEST_PREFIX "Reflection.AllTests."
    WORKING_DIRECTORY $<TARGET_FILE_DIR:Reflection_AllTests>)
endif()

# ---------------------------------------------------------------------------
# Interop test: two plugins + host loader (requires ABI enabled)
# ---------------------------------------------------------------------------
if(NGIN_REFLECTION_ENABLE_ABI)
  add_library(InteropPluginA SHARED
    ${_reflection_root_dir}/src/Registry.cpp
    ${_reflection_root_dir}/src/ABI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interop/PluginA.cpp)
  target_include_directories(InteropPluginA PRIVATE ${_reflection_root_dir}/include)
  target_link_libraries(InteropPluginA PRIVATE NGIN::Base)
  target_compile_features(InteropPluginA PRIVATE cxx_std_23)
  target_compile_definitions(InteropPluginA PRIVATE NGIN_REFLECTION_ENABLE_ABI=1 NGIN_REFLECTION_EXPORTS)

  add_library(InteropPluginB SHARED
    ${_reflection_root_dir}/src/Registry.cpp
    ${_reflection_root_dir}/src/ABI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interop/PluginB.cpp)
  target_include_directories(InteropPluginB PRIVATE ${_reflection_root_dir}/include)
  target_link_libraries(InteropPluginB PRIVATE NGIN::Base)
  target_compile_features(InteropPluginB PRIVATE cxx_std_23)
  target_compile_definitions(InteropPluginB PRIVATE NGIN_REFLECTION_ENABLE_ABI=1 NGIN_REFLECTION_EXPORTS)

  add_executable(Interop_Host ${CMAKE_CURRENT_SOURCE_DIR}/Interop/Host.cpp)
  list(APPEND REFLECTION_TEST_TARGETS Interop_Host)
  target_link_libraries(Interop_Host PRIVATE Catch2::Catch2WithMain ngin_reflection_test_config)
  add_dependencies(Interop_Host InteropPluginA InteropPluginB)
  set_target_properties(Interop_Host PROPERTIES FOLDER "Tests")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CMAKE_CURRENT_SOURCE_DIR}/Interop/Host.cpp)
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

# ---------------------------------------------------------------------------
# Optional libatomic probing (only link when required)
# ---------------------------------------------------------------------------
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES atomic)
check_cxx_source_compiles("#include <atomic>\nint main(){std::atomic<int> x{0}; return x.load();}" REFLECTION_NEEDS_LIBATOMIC)
set(CMAKE_REQUIRED_LIBRARIES)
if(REFLECTION_NEEDS_LIBATOMIC)
  foreach(tgt IN ITEMS InteropPluginA InteropPluginB)
    if(TARGET ${tgt})
      target_link_libraries(${tgt} PRIVATE atomic)
    endif()
  endforeach()
endif()

# ---------------------------------------------------------------------------
# Common diagnostics / sanitizer toggles for tests
# ---------------------------------------------------------------------------
foreach(test_tgt IN LISTS REFLECTION_TEST_TARGETS)
  if(MSVC)
    target_compile_options(${test_tgt} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${test_tgt} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

if(REFLECTION_ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  foreach(test_tgt IN LISTS REFLECTION_TEST_TARGETS)
    target_compile_options(${test_tgt} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${test_tgt} PRIVATE -fsanitize=address,undefined)
  endforeach()
endif()
