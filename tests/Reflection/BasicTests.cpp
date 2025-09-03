/// @file BasicTests.cpp
/// @brief Basic smoke tests for NGIN.Reflection skeleton.

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

suite<"NGIN::Reflection"> reflectionSuite = []
{
  "LibraryName"_test = []
  {
    expect(eq(NGIN::Reflection::LibraryName(), std::string_view{"NGIN.Reflection"}));
  };
};
