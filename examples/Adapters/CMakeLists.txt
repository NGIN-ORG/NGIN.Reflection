cmake_minimum_required(VERSION 3.16)
project(ReflectionAdapters LANGUAGES CXX)

add_executable(ReflectionAdapters main.cpp)
target_link_libraries(ReflectionAdapters PRIVATE NGIN::Reflection)
target_compile_features(ReflectionAdapters PRIVATE cxx_std_23)

