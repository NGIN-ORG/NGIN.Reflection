// Functions.cpp - tests for free/static function reflection

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace FunctionDemo
{
  int add(int a, int b) { return a + b; }
  double mul(double a, double b) { return a * b; }

  int f(int v) { return v + 1; }
  double f(double v) { return v + 0.5; }

  struct Math
  {
    static int twice(int v) { return v * 2; }
    friend void ngin_reflect(NGIN::Reflection::Tag<Math>, NGIN::Reflection::TypeBuilder<Math> &b)
    {
      b.static_method<&Math::twice>("Math::twice");
    }
  };

  inline void RegisterFunctions()
  {
    static bool registered = false;
    if (registered)
      return;
    using namespace NGIN::Reflection;
    RegisterFunction<&add>("add");
    RegisterFunction<&mul>("mul");
    RegisterFunction<static_cast<int (*)(int)>(&f)>("f");
    RegisterFunction<static_cast<double (*)(double)>(&f)>("f");
    (void)GetType<Math>();
    registered = true;
  }
} // namespace FunctionDemo

TEST_CASE("ResolveFunctionInvokesFreeFunctions", "[reflection][Function]")
{
  using namespace NGIN::Reflection;
  FunctionDemo::RegisterFunctions();

  Any args[2] = {Any{3}, Any{4}};
  auto rf = ResolveFunction("add", args, 2).value();
  auto out = rf.Invoke(args, 2).value();
  CHECK(out.Cast<int>() == 7);
}

TEST_CASE("ResolveFunctionSelectsOverload", "[reflection][Function]")
{
  using namespace NGIN::Reflection;
  FunctionDemo::RegisterFunctions();

  Any arg{2.0};
  auto rf = ResolveFunction("f", &arg, 1).value();
  auto out = rf.Invoke(&arg, 1).value();
  CHECK(out.Cast<double>() == 2.5);
}

TEST_CASE("StaticMethodRegistrationUsesFunctions", "[reflection][Function]")
{
  using namespace NGIN::Reflection;
  FunctionDemo::RegisterFunctions();

  Any arg{6};
  auto rf = ResolveFunction("Math::twice", &arg, 1).value();
  auto out = rf.Invoke(&arg, 1).value();
  CHECK(out.Cast<int>() == 12);
}
