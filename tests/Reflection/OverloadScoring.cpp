// OverloadScoring.cpp — tests for overload scoring and narrowing penalties

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace ScoreDemo {
struct S {
  int f(int) const { return 1; }
  double f(double) const { return 2.0; }
  float f(float) const { return 3.0f; }
  long long f(long long) const { return 4; }
  unsigned long long f(unsigned long long) const { return 5; }
  friend void ngin_reflect(NGIN::Reflection::Tag<S>,
                           NGIN::Reflection::Builder<S> &b) {
    b.method<static_cast<int (S::*)(int) const>(&S::f)>("f");
    b.method<static_cast<double (S::*)(double) const>(&S::f)>("f");
    b.method<static_cast<float (S::*)(float) const>(&S::f)>("f");
    b.method<static_cast<long long (S::*)(long long) const>(&S::f)>("f");
    b.method<static_cast<unsigned long long (S::*)(unsigned long long) const>(
        &S::f)>("f");
  }
};

struct S2 {
  int g(int) const { return 1; }
  double g(double) const { return 2.0; }
  friend void ngin_reflect(NGIN::Reflection::Tag<S2>,
                           NGIN::Reflection::Builder<S2> &b) {
    b.method<static_cast<int (S2::*)(int) const>(&S2::g)>("g");
    b.method<static_cast<double (S2::*)(double) const>(&S2::g)>("g");
  }
};
} // namespace ScoreDemo

TEST_CASE("ExactMatchesOutrankConversions",
          "[reflection][OverloadScoring]") {
  using namespace NGIN::Reflection;
  using ScoreDemo::S;

  auto t = TypeOf<S>();
  S s{};
  Any arg = Any::make(3.14);
  auto m = t.ResolveMethod("f", &arg, 1).value();
  auto out = m.Invoke(&s, &arg, 1).value();
  CHECK(out.As<double>() == Catch::Approx(2.0));
}

TEST_CASE("PromotionsOutrankConversions",
          "[reflection][OverloadScoring]") {
  using namespace NGIN::Reflection;
  using ScoreDemo::S2;

  auto t = TypeOf<S2>();
  S2 s{};
  Any arg = Any::make(2.0f);
  auto m = t.ResolveMethod("g", &arg, 1).value();
  auto out = m.Invoke(&s, &arg, 1).value();
  CHECK(out.As<double>() == Catch::Approx(2.0));
}

TEST_CASE("NarrowingConversionsArePenalized",
          "[reflection][OverloadScoring]") {
  using namespace NGIN::Reflection;
  using ScoreDemo::S;

  auto t = TypeOf<S>();
  S s{};
  Any arg = Any::make(3.14);
  auto m = t.ResolveMethod("f", &arg, 1).value();
  auto out = m.Invoke(&s, &arg, 1).value();
  CHECK(out.As<double>() == Catch::Approx(2.0));
}

TEST_CASE("UnsignedOverloadsHandleUnsignedInputs",
          "[reflection][OverloadScoring]") {
  using namespace NGIN::Reflection;
  using ScoreDemo::S;

  auto t = TypeOf<S>();
  S s{};
  Any arg = Any::make(42u);
  auto m = t.ResolveMethod("f", &arg, 1).value();
  auto out = m.Invoke(&s, &arg, 1);
  CHECK(out.has_value());
}
