#include <NGIN/Reflection/Reflection.hpp>
#include <iostream>

namespace Demo
{
  struct Math
  {
    int mul(int a, int b) const { return a * b; }
    float mul(float a, float b) const { return a * b; }
    double mul(int a, double b) const { return a * b; }

    friend void NginReflect(NGIN::Reflection::Tag<Math>, NGIN::Reflection::TypeBuilder<Math> &b)
    {
      b.SetName("Demo::Math");
      b.Method<static_cast<int (Math::*)(int, int) const>(&Math::mul)>("mul");
      b.Method<static_cast<float (Math::*)(float, float) const>(&Math::mul)>("mul");
      b.Method<static_cast<double (Math::*)(int, double) const>(&Math::mul)>("mul");
    }
  };
}

int main()
{
  using namespace NGIN::Reflection;
  using Demo::Math;
  auto t = GetType<Math>();
  // Resolve on each call to demonstrate overload selection + conversions
  Math math{};

  Any args1[2] = {Any{3}, Any{4}};
  auto m1 = t.ResolveMethod("mul", args1, 2).value();
  std::cout << "mul(3,4) => " << m1.Invoke(&math, args1, 2).value().Cast<int>() << "\n";

  Any args2[2] = {Any{3}, Any{2.5}};
  auto m2 = t.ResolveMethod("mul", args2, 2).value();
  std::cout << "mul(3,2.5) => " << m2.Invoke(&math, args2, 2).value().Cast<double>() << "\n";

  Any args3[2] = {Any{2.0f}, Any{5.0f}};
  auto m3 = t.ResolveMethod("mul", args3, 2).value();
  std::cout << "mul(2f,5f) => " << m3.Invoke(&math, args3, 2).value().Cast<float>() << "\n";
  return 0;
}
