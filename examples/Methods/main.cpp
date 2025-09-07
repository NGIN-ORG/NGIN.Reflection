#include <NGIN/Reflection/Reflection.hpp>
#include <iostream>

namespace Demo
{
  struct Math
  {
    int mul(int a, int b) const { return a * b; }
    float mul(float a, float b) const { return a * b; }
    double mul(int a, double b) const { return a * b; }

    friend void ngin_reflect(NGIN::Reflection::tag<Math>, NGIN::Reflection::Builder<Math> &b)
    {
      b.set_name("Demo::Math");
      b.method<static_cast<int (Math::*)(int, int) const>(&Math::mul)>("mul");
      b.method<static_cast<float (Math::*)(float, float) const>(&Math::mul)>("mul");
      b.method<static_cast<double (Math::*)(int, double) const>(&Math::mul)>("mul");
    }
  };
}

int main()
{
  using namespace NGIN::Reflection;
  using Demo::Math;
  auto t = TypeOf<Math>();
  // Resolve on each call to demonstrate overload selection + conversions
  Math math{};

  Any args1[2] = {Any::make(3), Any::make(4)};
  auto m1 = t.ResolveMethod("mul", args1, 2).value();
  std::cout << "mul(3,4) => " << m1.Invoke(&math, args1, 2).value().as<int>() << "\n";

  Any args2[2] = {Any::make(3), Any::make(2.5)};
  auto m2 = t.ResolveMethod("mul", args2, 2).value();
  std::cout << "mul(3,2.5) => " << m2.Invoke(&math, args2, 2).value().as<double>() << "\n";

  Any args3[2] = {Any::make(2.0f), Any::make(5.0f)};
  auto m3 = t.ResolveMethod("mul", args3, 2).value();
  std::cout << "mul(2f,5f) => " << m3.Invoke(&math, args3, 2).value().as<float>() << "\n";
  return 0;
}
