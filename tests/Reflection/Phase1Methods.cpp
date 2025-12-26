// Phase1Methods.cpp — Methods, invocation, and attributes

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

#include <variant>

namespace DemoPhase1M {
struct Calc {
  int base{0};
  int add(int x) const { return base + x; }

  friend void NginReflect(NGIN::Reflection::Tag<Calc>,
                           NGIN::Reflection::TypeBuilder<Calc> &b) {
    b.SetName("Demo::Calc");
    b.Field<&Calc::base>("base");
    b.Method<&Calc::add>("add");
    b.Attribute("category", std::string_view{"math"});
    b.FieldAttribute<&Calc::base>("min", std::int64_t{0});
    b.MethodAttribute<&Calc::add>("group", std::string_view{"arith"});
  }
};
} // namespace DemoPhase1M

TEST_CASE("MethodInvocationReturnsExpectedResult",
          "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  auto t = GetType<Calc>();
  auto m = t.GetMethod("add").value();
  CHECK(m.GetParameterCount() == NGIN::UIntSize{1});

  Calc c{2};
  Any args[1] = {Any{5}};
  auto out = m.Invoke(&c, args, 1).value();
  CHECK(out.Cast<int>() == 7);
}

TEST_CASE("FieldMutatorsEnforceTypes", "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  Calc c{1};
  auto t = GetType<Calc>();
  auto fexp = t.GetField("base");
  REQUIRE(fexp.has_value());
  auto f = fexp.value();
  CHECK(c.base == 1);

  CHECK(f.SetAny(&c, Any{10}).has_value());
  CHECK(c.base == 10);

  auto av = f.GetAny(&c);
  CHECK(av.Cast<int>() == 10);
  CHECK_FALSE(f.SetAny(&c, Any{3.14f}).has_value());
}

TEST_CASE("FieldTypedAccessUsesReferences", "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  Calc c{1};
  auto t = GetType<Calc>();
  auto f = t.GetField("base").value();

  CHECK(f.Set(c, 12).has_value());
  auto v = f.Get<int>(c).value();
  CHECK(v == 12);
}

TEST_CASE("FieldAndMethodAttributesAreExposed",
          "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  auto t = GetType<Calc>();
  auto f = t.GetField("base").value();
  auto fa = f.Attribute("min").value();
  CHECK(fa.Key() == std::string_view{"min"});
  CHECK(std::holds_alternative<std::int64_t>(fa.Value()));

  auto m = t.GetMethod("add").value();
  auto ma = m.Attribute("group").value();
  CHECK(ma.Key() == std::string_view{"group"});
}

TEST_CASE("TypeAttributesAreRetrievable",
          "[reflection][Phase1Methods]") {
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  auto t = GetType<Calc>();
  auto av = t.Attribute("category").value();
  CHECK(av.Key() == std::string_view{"category"});

  auto *sval = std::get_if<std::string_view>(&av.Value());
  REQUIRE(sval != nullptr);
  CHECK(*sval == std::string_view{"math"});
}
