cmake_minimum_required(VERSION 3.16)
project(ReflectionMethods LANGUAGES CXX)

add_executable(ReflectionMethods main.cpp)
target_link_libraries(ReflectionMethods PRIVATE NGIN::Reflection)
target_compile_features(ReflectionMethods PRIVATE cxx_std_23)

