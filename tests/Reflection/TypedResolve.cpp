// TypedResolve.cpp — tests for compile-time typed ResolveMethod

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace TRDemo {
struct M {
  int mul(int a, int b) const { return a * b; }
  float mul(float a, float b) const { return a * b; }
  void ping(int) const {}
  friend void ngin_reflect(NGIN::Reflection::Tag<M>,
                           NGIN::Reflection::Builder<M> &b) {
    b.method<static_cast<int (M::*)(int, int) const>(&M::mul)>("mul");
    b.method<static_cast<float (M::*)(float, float) const>(&M::mul)>("mul");
    b.method<static_cast<void (M::*)(int) const>(&M::ping)>("ping");
  }
};
} // namespace TRDemo

TEST_CASE("ResolveSelectsOverloadBySignature",
          "[reflection][TypedResolve]") {
  using namespace NGIN::Reflection;
  using TRDemo::M;

  auto t = TypeOf<M>();
  auto m1 = t.ResolveMethod<int, int, int>("mul").value();
  auto m2 = t.ResolveMethod<float, float, float>("mul").value();
  M obj{};
  Any ii[2] = {Any{3}, Any{4}};
  Any ff[2] = {Any{2.0f}, Any{5.0f}};
  CHECK(m1.Invoke(&obj, ii, 2).value().Cast<int>() == 12);
  CHECK(m2.Invoke(&obj, ff, 2).value().Cast<float>() == 10.0f);
}

TEST_CASE("ResolveSupportsVoidReturns", "[reflection][TypedResolve]") {
  using namespace NGIN::Reflection;
  using TRDemo::M;

  auto t = TypeOf<M>();
  auto m = t.ResolveMethod<void, int>("ping").value();
  M obj{};
  Any arg{1};
  auto out = m.Invoke(&obj, &arg, 1).value();
  CHECK_FALSE(out.HasValue());
}
