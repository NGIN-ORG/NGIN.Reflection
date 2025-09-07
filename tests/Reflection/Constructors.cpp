// Constructors.cpp — tests for default and parameterized constructors

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

namespace CtorDemo
{
  struct Point
  {
    int x{0};
    int y{0};
    Point() = default;
    Point(int a, int b) : x(a), y(b) {}
    friend void ngin_reflect(NGIN::Reflection::tag<Point>, NGIN::Reflection::Builder<Point> &b)
    {
      b.set_name("CtorDemo::Point");
      b.field<&Point::x>("x");
      b.field<&Point::y>("y");
      b.constructor<int, int>();
    }
  };
}

suite<"NGIN::Reflection::Constructors"> ctors = []
{
  using namespace NGIN::Reflection;
  using CtorDemo::Point;

  "Default_Construct"_test = []
  {
    auto t = TypeOf<Point>();
    auto any = t.DefaultConstruct().value();
    auto p = any.as<Point>();
    expect(eq(p.x, 0));
    expect(eq(p.y, 0));
  };

  "Parameterized_Construct_With_Ints"_test = []
  {
    auto t = TypeOf<Point>();
    Any args[2] = {Any::make(3), Any::make(4)};
    auto any = t.Construct(args, 2).value();
    auto p = any.as<Point>();
    expect(eq(p.x, 3));
    expect(eq(p.y, 4));
  };

  "Parameterized_Construct_With_Convertible"_test = []
  {
    auto t = TypeOf<Point>();
    Any args[2] = {Any::make(3.5), Any::make(4.0f)};
    auto any = t.Construct(args, 2).value();
    auto p = any.as<Point>();
    expect(eq(p.x, 3));
    expect(eq(p.y, 4));
  };
};
