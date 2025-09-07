// TypedResolve.cpp — tests for compile-time typed ResolveMethod

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

namespace TRDemo
{
  struct M
  {
    int mul(int a, int b) const { return a * b; }
    float mul(float a, float b) const { return a * b; }
    void ping(int) const {}
    friend void ngin_reflect(NGIN::Reflection::Tag<M>, NGIN::Reflection::Builder<M> &b)
    {
      b.method<static_cast<int (M::*)(int, int) const>(&M::mul)>("mul");
      b.method<static_cast<float (M::*)(float, float) const>(&M::mul)>("mul");
      b.method<static_cast<void (M::*)(int) const>(&M::ping)>("ping");
    }
  };
}

suite<"NGIN::Reflection::TypedResolve"> typedResolveSuite = []
{
  using namespace NGIN::Reflection;
  using TRDemo::M;

  "Resolve_By_Types_Exact"_test = []
  {
    auto t = TypeOf<M>();
    auto m1 = t.ResolveMethod<int, int, int>("mul").value();
    auto m2 = t.ResolveMethod<float, float, float>("mul").value();
    M obj{};
    Any ii[2] = {Any::make(3), Any::make(4)};
    Any ff[2] = {Any::make(2.0f), Any::make(5.0f)};
    expect(eq(m1.Invoke(&obj, ii, 2).value().as<int>(), 12));
    expect(eq(m2.Invoke(&obj, ff, 2).value().as<float>(), 10.0f));
  };

  "Resolve_Void_Return"_test = []
  {
    auto t = TypeOf<M>();
    auto m = t.ResolveMethod<void, int>("ping").value();
    M obj{};
    Any arg = Any::make(1);
    auto out = m.Invoke(&obj, &arg, 1).value();
    expect(out.is_void());
  };
};
