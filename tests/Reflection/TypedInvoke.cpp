// TypedInvoke.cpp — tests for invoking with typed arguments (no Any array)

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace TIDemo {
struct M {
  int mul(int a, int b) const { return a * b; }
  double mul(double a, double b) const { return a * b; }
  void ping(int) const {}
  friend void NginReflect(NGIN::Reflection::Tag<M>,
                           NGIN::Reflection::TypeBuilder<M> &b) {
    b.Method<static_cast<int (M::*)(int, int) const>(&M::mul)>("mul");
    b.Method<static_cast<double (M::*)(double, double) const>(&M::mul)>("mul");
    b.Method<static_cast<void (M::*)(int) const>(&M::ping)>("ping");
  }
};
} // namespace TIDemo

TEST_CASE("MethodInvocationWithTypedArguments",
          "[reflection][TypedInvoke]") {
  using namespace NGIN::Reflection;
  using TIDemo::M;

  auto t = GetType<M>();
  M obj{};
  auto m = t.ResolveMethod<int, int, int>("mul").value();
  auto out = m.InvokeAs<int>(obj, 3, 4).value();
  CHECK(out == 12);
}

TEST_CASE("TypeInvocationWithTypedArguments",
          "[reflection][TypedInvoke]") {
  using namespace NGIN::Reflection;
  using TIDemo::M;

  auto t = GetType<M>();
  M obj{};
  auto out = t.InvokeAs<int, int, int>("mul", obj, 5, 6).value();
  CHECK(out == 30);
}

TEST_CASE("TypedInvokeSupportsVoidReturn",
          "[reflection][TypedInvoke]") {
  using namespace NGIN::Reflection;
  using TIDemo::M;

  auto t = GetType<M>();
  M obj{};
  auto r = t.InvokeAs<void, int>("ping", obj, 1);
  CHECK(r.has_value());
}

TEST_CASE("TypedInvokeHandlesDoubleOverload",
          "[reflection][TypedInvoke]") {
  using namespace NGIN::Reflection;
  using TIDemo::M;

  auto t = GetType<M>();
  M obj{};
  auto out = t.InvokeAs<double, double, double>("mul", obj, 1.5, 2.0).value();
  CHECK(out == Catch::Approx(3.0));
}

TEST_CASE("ResolveReportsMissingOverloads", "[reflection][TypedInvoke]") {
  using namespace NGIN::Reflection;
  using TIDemo::M;

  auto t = GetType<M>();
  auto m = t.ResolveMethod<int, int, double>("mul");
  CHECK_FALSE(m.has_value());
}
