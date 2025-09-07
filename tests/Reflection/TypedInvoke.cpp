// TypedInvoke.cpp — tests for invoking with typed arguments (no Any array)

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

namespace TIDemo
{
  struct M
  {
    int mul(int a, int b) const { return a * b; }
    double mul(double a, double b) const { return a * b; }
    void ping(int) const {}
    friend void ngin_reflect(NGIN::Reflection::tag<M>, NGIN::Reflection::Builder<M> &b)
    {
      b.method<static_cast<int (M::*)(int, int) const>(&M::mul)>("mul");
      b.method<static_cast<double (M::*)(double, double) const>(&M::mul)>("mul");
      b.method<static_cast<void (M::*)(int) const>(&M::ping)>("ping");
    }
  };
}

suite<"NGIN::Reflection::TypedInvoke"> typedInvokeSuite = []
{
  using namespace NGIN::Reflection;
  using TIDemo::M;

  "Method_Invoke_TypedArgs"_test = []
  {
    auto t = TypeOf<M>();
    M obj{};
    auto m = t.ResolveMethod<int, int, int>("mul").value();
    auto out = m.InvokeAs<int>(&obj, 3, 4).value();
    expect(eq(out, 12));
  };

  "Type_Invoke_TypedArgs"_test = []
  {
    auto t = TypeOf<M>();
    M obj{};
    auto out = t.InvokeAs<int, int, int>("mul", &obj, 5, 6).value();
    expect(eq(out, 30));
  };

  "Typed_Void_Invoke"_test = []
  {
    auto t = TypeOf<M>();
    M obj{};
    auto r = t.InvokeAs<void, int>("ping", &obj, 1);
    expect(r.has_value());
  };

  "Typed_Invoke_Double"_test = []
  {
    auto t = TypeOf<M>();
    M obj{};
    auto out = t.InvokeAs<double, double, double>("mul", &obj, 1.5, 2.0).value();
    expect(eq(out, 3.0));
  };

  "Typed_Resolve_NoMatch"_test = []
  {
    auto t = TypeOf<M>();
    // No overload with (int,double)
    auto m = t.ResolveMethod<int, int, double>("mul");
    expect(!m.has_value());
  };
};
