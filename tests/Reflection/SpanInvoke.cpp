// SpanInvoke.cpp — tests for span-based invoke and resolve overloads

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

namespace SpanDemo
{
  struct C
  {
    int inc(int v) const { return v + 1; }
    friend void ngin_reflect(NGIN::Reflection::tag<C>, NGIN::Reflection::Builder<C> &b)
    {
      b.set_name("SpanDemo::C");
      b.method<&C::inc>("inc");
    }
  };
}

suite<"NGIN::Reflection::SpanInvoke"> spanInvokeSuite = []
{
  using namespace NGIN::Reflection;
  using SpanDemo::C;

  "Invoke_With_Span"_test = []
  {
    auto t = TypeOf<C>();
    C c{};
    Any buf[1] = {Any::make(41)};
    std::span<const Any> args{buf, 1};
    auto m = t.ResolveMethod("inc", args).value();
    auto out = m.Invoke(&c, args).value();
    expect(eq(out.as<int>(), 42));
  };
};
