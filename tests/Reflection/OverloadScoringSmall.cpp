// OverloadScoringSmall.cpp — tests for small integer promotions and refined
// scoring

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace SmallDemo {
struct H {
  int f(int) const { return 1; }
  long f(long) const { return 2; }
  double f(double) const { return 3.0; }
  friend void ngin_reflect(NGIN::Reflection::Tag<H>,
                           NGIN::Reflection::TypeBuilder<H> &b) {
    b.method<static_cast<int (H::*)(int) const>(&H::f)>("f");
    b.method<static_cast<long (H::*)(long) const>(&H::f)>("f");
    b.method<static_cast<double (H::*)(double) const>(&H::f)>("f");
  }
};
} // namespace SmallDemo

TEST_CASE("CharPromotesToIntOverload",
          "[reflection][OverloadScoringSmall]") {
  using namespace NGIN::Reflection;
  using SmallDemo::H;

  auto t = GetType<H>();
  H h{};
  Any arg{static_cast<char>(5)};
  auto m = t.ResolveMethod("f", &arg, 1).value();
  auto out = m.Invoke(&h, &arg, 1).value();
  CHECK(out.Cast<int>() == 1);
}

TEST_CASE("ShortPromotesToIntOverload",
          "[reflection][OverloadScoringSmall]") {
  using namespace NGIN::Reflection;
  using SmallDemo::H;

  auto t = GetType<H>();
  H h{};
  Any arg{static_cast<short>(7)};
  auto m = t.ResolveMethod("f", &arg, 1).value();
  auto out = m.Invoke(&h, &arg, 1).value();
  CHECK(out.Cast<int>() == 1);
}

TEST_CASE("FloatPromotesToDoubleOverload",
          "[reflection][OverloadScoringSmall]") {
  using namespace NGIN::Reflection;
  using SmallDemo::H;

  auto t = GetType<H>();
  H h{};
  Any arg{1.5f};
  auto m = t.ResolveMethod("f", &arg, 1).value();
  auto out = m.Invoke(&h, &arg, 1).value();
  CHECK(out.Cast<double>() == Catch::Approx(3.0));
}

TEST_CASE("LongPrefersLongOverloadOverInt",
          "[reflection][OverloadScoringSmall]") {
  using namespace NGIN::Reflection;
  using SmallDemo::H;

  auto t = GetType<H>();
  H h{};
  Any arg{static_cast<long>(9)};
  auto m = t.ResolveMethod("f", &arg, 1).value();
  auto out = m.Invoke(&h, &arg, 1).value();
  CHECK(out.Cast<long>() == 2);
}
