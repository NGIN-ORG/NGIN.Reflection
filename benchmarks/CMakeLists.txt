cmake_minimum_required(VERSION 3.16)
project(NGINReflectionBenchmarks LANGUAGES CXX)

add_executable(ReflectionBench ReflectionBench.cpp)
target_link_libraries(ReflectionBench PRIVATE NGIN::Reflection)
target_compile_features(ReflectionBench PRIVATE cxx_std_23)

add_executable(NameInternerBench NameInternerBench.cpp)
target_link_libraries(NameInternerBench PRIVATE NGIN::Reflection)
target_compile_features(NameInternerBench PRIVATE cxx_std_23)

add_executable(NameLookupBench NameLookupBench.cpp)
target_link_libraries(NameLookupBench PRIVATE NGIN::Reflection)
target_compile_features(NameLookupBench PRIVATE cxx_std_23)
