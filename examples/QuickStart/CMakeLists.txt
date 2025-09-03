cmake_minimum_required(VERSION 3.16)
project(ReflectionQuickStart LANGUAGES CXX)

add_executable(ReflectionQuickStart main.cpp)
target_link_libraries(ReflectionQuickStart PRIVATE NGIN::Reflection)
target_compile_features(ReflectionQuickStart PRIVATE cxx_std_23)

