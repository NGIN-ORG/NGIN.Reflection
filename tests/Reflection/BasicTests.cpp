/// @file BasicTests.cpp
/// @brief Basic smoke tests for NGIN.Reflection skeleton.

#include <catch2/catch_test_macros.hpp>
#include <NGIN/Reflection/Reflection.hpp>

TEST_CASE("LibraryNameReturnsModuleIdentifier", "[reflection][Basics]") {
  CHECK(NGIN::Reflection::LibraryName() == std::string_view{"NGIN.Reflection"});
}
