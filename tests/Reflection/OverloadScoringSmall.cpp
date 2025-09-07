// OverloadScoringSmall.cpp — tests for small integer promotions and refined scoring

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

namespace SmallDemo
{
  struct H
  {
    int f(int) const { return 1; }
    long f(long) const { return 2; }
    double f(double) const { return 3.0; }
    friend void ngin_reflect(NGIN::Reflection::Tag<H>, NGIN::Reflection::Builder<H> &b)
    {
      b.method<static_cast<int (H::*)(int) const>(&H::f)>("f");
      b.method<static_cast<long (H::*)(long) const>(&H::f)>("f");
      b.method<static_cast<double (H::*)(double) const>(&H::f)>("f");
    }
  };
}

suite<"NGIN::Reflection::OverloadScoringSmall"> over2 = []
{
  using namespace NGIN::Reflection;
  using SmallDemo::H;

  "Char_Promotes_To_Int"_test = []
  {
    auto t = TypeOf<H>();
    H h{};
    Any arg = Any::make(static_cast<char>(5));
    auto m = t.ResolveMethod("f", &arg, 1).value();
    auto out = m.Invoke(&h, &arg, 1).value();
    expect(eq(out.as<int>(), 1));
  };

  "Short_Promotes_To_Int"_test = []
  {
    auto t = TypeOf<H>();
    H h{};
    Any arg = Any::make(static_cast<short>(7));
    auto m = t.ResolveMethod("f", &arg, 1).value();
    auto out = m.Invoke(&h, &arg, 1).value();
    expect(eq(out.as<int>(), 1));
  };

  "Float_Promotes_To_Double"_test = []
  {
    auto t = TypeOf<H>();
    H h{};
    Any arg = Any::make(1.5f);
    auto m = t.ResolveMethod("f", &arg, 1).value();
    auto out = m.Invoke(&h, &arg, 1).value();
    expect(eq(out.as<double>(), 3.0));
  };

  "Long_Prefers_Long_Over_Int"_test = []
  {
    auto t = TypeOf<H>();
    H h{};
    Any arg = Any::make(static_cast<long>(9));
    auto m = t.ResolveMethod("f", &arg, 1).value();
    auto out = m.Invoke(&h, &arg, 1).value();
    expect(eq(out.as<long>(), 2));
  };
};
