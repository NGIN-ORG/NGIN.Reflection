// Phase1Methods.cpp — Methods, invocation, and attributes

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

namespace DemoPhase1M
{
  struct Calc
  {
    int base{0};
    int add(int x) const { return base + x; }

    friend void ngin_reflect(NGIN::Reflection::tag<Calc>, NGIN::Reflection::Builder<Calc> &b)
    {
      b.set_name("Demo::Calc");
      b.field<&Calc::base>("base");
      b.method<&Calc::add>("add");
      b.attribute("category", std::string_view{"math"});
      b.field_attribute<&Calc::base>("min", std::int64_t{0});
      b.method_attribute<&Calc::add>("group", std::string_view{"arith"});
    }
  };
}

suite<"NGIN::Reflection::Phase1Methods"> reflPhase1Methods = []
{
  using namespace NGIN::Reflection;
  using DemoPhase1M::Calc;

  "Method_Invoke_Int"_test = []
  {
    auto t = TypeOf<Calc>();
    auto m = t.GetMethod("add").value();
    expect(eq(m.GetParameterCount(), NGIN::UIntSize{1}));

    Calc c{2};
    Any args[1] = {Any::make(5)};
    auto out = m.Invoke(&c, args, 1).value();
    expect(eq(out.as<int>(), 7));
  };

  "Field_SetAny_TypeChecked"_test = []
  {
    Calc c{1};
    auto t = TypeOf<Calc>();
    auto fexp = t.GetField("base");
    expect(fexp.has_value());
    auto f = fexp.value();
    expect(eq(c.base, 1));
    expect(f.set_any(&c, Any::make(10)).has_value());
    expect(eq(c.base, 10));
    // get_any returns the current value
    auto av = f.get_any(&c);
    expect(eq(av.as<int>(), 10));
    // Type mismatch should fail
    expect(!f.set_any(&c, Any::make(3.14f)).has_value());
  };

  "Field_And_Method_Attributes"_test = []
  {
    auto t = TypeOf<Calc>();
    auto f = t.GetField("base").value();
    auto fa = f.attribute("min").value();
    expect(eq(fa.key(), std::string_view{"min"}));
    expect(std::holds_alternative<std::int64_t>(fa.value()));
    auto m = t.GetMethod("add").value();
    auto ma = m.attribute("group").value();
    expect(eq(ma.key(), std::string_view{"group"}));
  };

  "Type_Attribute_Basic"_test = []
  {
    auto t = TypeOf<Calc>();
    auto av = t.Attribute("category").value();
    expect(eq(av.key(), std::string_view{"category"}));
    // Holds variant; only check it is string_view with value "math"
    auto *sval = std::get_if<std::string_view>(&av.value());
    expect(bool{sval != nullptr});
    if (sval)
      expect(eq(*sval, std::string_view{"math"}));
  };
};
