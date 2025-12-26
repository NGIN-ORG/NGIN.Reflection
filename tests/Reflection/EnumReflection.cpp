// EnumReflection.cpp - tests for enum registration and lookup

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Meta/TypeName.hpp>

namespace EnumDemo
{
  enum class Color : unsigned int
  {
    Red = 1,
    Green = 2,
    Blue = 4
  };

  inline void NginReflect(NGIN::Reflection::Tag<Color>, NGIN::Reflection::TypeBuilder<Color> &b)
  {
    b.SetName("EnumDemo::Color");
    b.EnumValue("Red", Color::Red);
    b.EnumValue("Green", Color::Green);
    b.EnumValue("Blue", Color::Blue);
  }
} // namespace EnumDemo

TEST_CASE("EnumValuesAreRegistered", "[reflection][Enum]")
{
  using namespace NGIN::Reflection;
  using EnumDemo::Color;

  auto t = GetType<Color>();
  CHECK(t.IsEnum());
  CHECK(t.EnumValueCount() == 3);
  auto red = t.GetEnumValue("Red").value();
  CHECK(red.Value().Cast<Color>() == Color::Red);
}

TEST_CASE("EnumParseAndStringify", "[reflection][Enum]")
{
  using namespace NGIN::Reflection;
  using EnumDemo::Color;

  auto t = GetType<Color>();
  auto v = t.ParseEnum("Blue").value();
  CHECK(v.Cast<Color>() == Color::Blue);
  auto name = t.EnumName(Any{Color::Green});
  CHECK(name.has_value());
  CHECK(name.value() == std::string_view{"Green"});
}

TEST_CASE("EnumUnderlyingTypeId", "[reflection][Enum]")
{
  using namespace NGIN::Reflection;
  using EnumDemo::Color;

  auto t = GetType<Color>();
  auto sv = NGIN::Meta::TypeName<unsigned int>::qualifiedName;
  auto want = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
  CHECK(t.EnumUnderlyingTypeId() == want);
}
