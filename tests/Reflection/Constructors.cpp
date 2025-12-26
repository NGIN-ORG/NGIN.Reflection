// Constructors.cpp — tests for default and parameterized constructors

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace CtorDemo
{
  struct Point
  {
    int x{0};
    int y{0};
    Point() = default;
    Point(int a, int b) : x(a), y(b) {}
    friend void NginReflect(NGIN::Reflection::Tag<Point>,
                             NGIN::Reflection::TypeBuilder<Point> &b)
    {
      b.SetName("CtorDemo::Point");
      b.Field<&Point::x>("x");
      b.Field<&Point::y>("y");
      b.Constructor<int, int>();
    }
  };
} // namespace CtorDemo

TEST_CASE("DefaultConstructorProducesZeroPoint",
          "[reflection][Constructors]")
{
  using namespace NGIN::Reflection;
  using CtorDemo::Point;

  auto t = GetType<Point>();
  auto any = t.DefaultConstruct().value();
  auto p = any.Cast<Point>();
  CHECK(p.x == 0);
  CHECK(p.y == 0);
}

TEST_CASE("ParameterizedConstructorAcceptsInts",
          "[reflection][Constructors]")
{
  using namespace NGIN::Reflection;
  using CtorDemo::Point;

  auto t = GetType<Point>();
  Any args[2] = {Any{3}, Any{4}};
  auto any = t.Construct(args, 2).value();
  auto p = any.Cast<Point>();
  CHECK(p.x == 3);
  CHECK(p.y == 4);
}

TEST_CASE("ParameterizedConstructorConvertsArguments",
          "[reflection][Constructors]")
{
  using namespace NGIN::Reflection;
  using CtorDemo::Point;

  auto t = GetType<Point>();
  Any args[2] = {Any{3.5}, Any{4.0f}};
  auto any = t.Construct(args, 2).value();
  auto p = any.Cast<Point>();
  CHECK(p.x == 3);
  CHECK(p.y == 4);
}
