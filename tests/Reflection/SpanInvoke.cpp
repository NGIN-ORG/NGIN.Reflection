// SpanInvoke.cpp — tests for span-based invoke and resolve overloads

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace SpanDemo {
struct C {
  int inc(int v) const { return v + 1; }
  friend void ngin_reflect(NGIN::Reflection::Tag<C>,
                           NGIN::Reflection::Builder<C> &b) {
    b.set_name("SpanDemo::C");
    b.method<&C::inc>("inc");
  }
};
} // namespace SpanDemo

TEST_CASE("SpanOverloadInvocationSucceeds",
          "[reflection][SpanInvoke]") {
  using namespace NGIN::Reflection;
  using SpanDemo::C;

  auto t = TypeOf<C>();
  C c{};
  Any buf[1] = {Any{41}};
  std::span<const Any> args{buf, 1};
  auto m = t.ResolveMethod("inc", args).value();
  auto out = m.Invoke(&c, args).value();
  CHECK(out.Cast<int>() == 42);
}
