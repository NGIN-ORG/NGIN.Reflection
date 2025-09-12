cmake_minimum_required(VERSION 3.16)
project(NGINReflectionTests LANGUAGES CXX)

include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/BoostUT.cmake)

# ---------------------------------------------------------------------------
# Pull in Boost.UT
# ---------------------------------------------------------------------------
CPMAddPackage(
  NAME ut
  VERSION 2.3.1
  GITHUB_REPOSITORY boost-ext/ut
  OPTIONS
    "BOOST_UT_DISABLE_MODULE ON"
    "BOOST_UT_BUILD_EXAMPLES OFF"
    "BOOST_UT_BUILD_TESTS OFF"
)

# ---------------------------------------------------------------------------
# Common test config
# ---------------------------------------------------------------------------
add_library(ngin_reflection_ut_config INTERFACE)
target_link_libraries(ngin_reflection_ut_config INTERFACE Boost::ut NGIN::Reflection)
target_compile_features(ngin_reflection_ut_config INTERFACE cxx_std_23)

# main.cpp is our test runner entry point
set(MAIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

# ---------------------------------------------------------------------------
# Root-level tests (one exe per .cpp file, excluding main.cpp)
# ---------------------------------------------------------------------------
file(GLOB ROOT_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM ROOT_TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

foreach(test_src IN LISTS ROOT_TEST_SRCS)
  get_filename_component(test_name "${test_src}" NAME_WE)
  set(exe_name "Reflection_${test_name}")
  message(STATUS "Adding test executable: ${exe_name} from ${test_src}")
  add_executable(${exe_name} ${MAIN_SRC} ${test_src})
  target_link_libraries(${exe_name} PRIVATE ngin_reflection_ut_config)
  set_target_properties(${exe_name} PROPERTIES FOLDER "Tests")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${test_src})
  # Register individual Boost.UT tests with suite names from this file
  discover_boost_ut_test(${exe_name} TEST_SOURCE ${test_src})
endforeach()

# ---------------------------------------------------------------------------
# Subdirectory tests (one exe per .cpp file)
# ---------------------------------------------------------------------------
file(GLOB CHILDREN RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(child IN LISTS CHILDREN)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}")
    # Always skip Interop in auto-discovery; handled explicitly when ABI is enabled
    if(child STREQUAL "Interop")
      continue()
    endif()
    file(GLOB SUBDIR_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/*.cpp")
    foreach(test_src IN LISTS SUBDIR_SRCS)
      get_filename_component(test_name "${test_src}" NAME_WE)
      string(REPLACE "-" "_" child_safe "${child}")
      set(exe_name "${child_safe}_${test_name}")
      message(STATUS "Adding test executable: ${exe_name} from ${test_src}")
      add_executable(${exe_name} ${MAIN_SRC} ${test_src})
      target_link_libraries(${exe_name} PRIVATE ngin_reflection_ut_config)
      set_target_properties(${exe_name} PROPERTIES FOLDER "Tests")
      source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${test_src})
      # Register individual Boost.UT tests with suite names from this file
      discover_boost_ut_test(${exe_name} TEST_SOURCE ${test_src})
    endforeach()
  endif()
endforeach()

# ---------------------------------------------------------------------------
# Interop test: two plugins + host loader (requires ABI enabled)
# ---------------------------------------------------------------------------
if(NGIN_REFLECTION_ENABLE_ABI)
  # Build PluginA and PluginB as MODULE libraries with their own registry
  add_library(InteropPluginA SHARED
    ${CMAKE_SOURCE_DIR}/src/Registry.cpp
    ${CMAKE_SOURCE_DIR}/src/ABI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interop/PluginA.cpp)
  target_include_directories(InteropPluginA PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(InteropPluginA PRIVATE NGIN::Base)
  target_compile_features(InteropPluginA PRIVATE cxx_std_23)
  target_compile_definitions(InteropPluginA PRIVATE NGIN_REFLECTION_ENABLE_ABI=1 NGIN_REFLECTION_EXPORTS)

  add_library(InteropPluginB SHARED
    ${CMAKE_SOURCE_DIR}/src/Registry.cpp
    ${CMAKE_SOURCE_DIR}/src/ABI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interop/PluginB.cpp)
  target_include_directories(InteropPluginB PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(InteropPluginB PRIVATE NGIN::Base)
  target_compile_features(InteropPluginB PRIVATE cxx_std_23)
  target_compile_definitions(InteropPluginB PRIVATE NGIN_REFLECTION_ENABLE_ABI=1 NGIN_REFLECTION_EXPORTS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(InteropPluginA PRIVATE atomic)
    target_link_libraries(InteropPluginB PRIVATE atomic)
  endif()

  # Host test executable
  add_executable(Interop_Host ${MAIN_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/Interop/Host.cpp)
  target_link_libraries(Interop_Host PRIVATE ngin_reflection_ut_config)
  add_dependencies(Interop_Host InteropPluginA InteropPluginB)
  set_target_properties(Interop_Host PROPERTIES FOLDER "Tests")
  discover_boost_ut_test(Interop_Host TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Interop/Host.cpp)

  # Ensure plugins land next to the host test for dlopen across single- and multi-config generators
  # Place InteropPluginA/B DLLs in the same dir as Interop_Host executable
  foreach(tgt IN ITEMS InteropPluginA InteropPluginB)
    set_target_properties(${tgt} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:Interop_Host>
      LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:Interop_Host>
      ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
  endforeach()
endif()
