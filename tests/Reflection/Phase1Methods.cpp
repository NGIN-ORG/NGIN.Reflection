// Phase1Methods.cpp — Methods, invocation, and attributes

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

#include <variant>

namespace DemoPhase1M {
struct Calc {
  int base{0};
  int add(int x) const { return base + x; }

  friend void ngin_reflect(NGIN::Reflection::Tag<Calc>,
                           NGIN::Reflection::Builder<Calc> &b) {
    b.set_name("Demo::Calc");
    b.field<&Calc::base>("base");
    b.method<&Calc::add>("add");
    b.attribute("category", std::string_view{"math"});
    b.field_attribute<&Calc::base>("min", std::int64_t{0});
    b.method_attribute<&Calc::add>("group", std::string_view{"arith"});
  }
};
} // namespace DemoPhase1M

TEST_CASE("MethodInvocationReturnsExpectedResult",
          "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  auto t = TypeOf<Calc>();
  auto m = t.GetMethod("add").value();
  CHECK(m.GetParameterCount() == NGIN::UIntSize{1});

  Calc c{2};
  Any args[1] = {Any::make(5)};
  auto out = m.Invoke(&c, args, 1).value();
  CHECK(out.As<int>() == 7);
}

TEST_CASE("FieldMutatorsEnforceTypes", "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  Calc c{1};
  auto t = TypeOf<Calc>();
  auto fexp = t.GetField("base");
  REQUIRE(fexp.has_value());
  auto f = fexp.value();
  CHECK(c.base == 1);

  CHECK(f.SetAny(&c, Any::make(10)).has_value());
  CHECK(c.base == 10);

  auto av = f.GetAny(&c);
  CHECK(av.As<int>() == 10);
  CHECK_FALSE(f.SetAny(&c, Any::make(3.14f)).has_value());
}

TEST_CASE("FieldAndMethodAttributesAreExposed",
          "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  auto t = TypeOf<Calc>();
  auto f = t.GetField("base").value();
  auto fa = f.attribute("min").value();
  CHECK(fa.key() == std::string_view{"min"});
  CHECK(std::holds_alternative<std::int64_t>(fa.value()));

  auto m = t.GetMethod("add").value();
  auto ma = m.attribute("group").value();
  CHECK(ma.key() == std::string_view{"group"});
}

TEST_CASE("TypeAttributesAreRetrievable",
          "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  auto t = TypeOf<Calc>();
  auto av = t.Attribute("category").value();
  CHECK(av.key() == std::string_view{"category"});

  auto *sval = std::get_if<std::string_view>(&av.value());
  REQUIRE(sval != nullptr);
  CHECK(*sval == std::string_view{"math"});
}
