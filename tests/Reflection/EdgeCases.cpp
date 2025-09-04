// EdgeCases.cpp — extended tests for errors, conversions, overloads, Any SBO/heap

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

namespace EdgeDemo {
  struct User { int id{}; float score{}; };
  inline void ngin_reflect(NGIN::Reflection::tag<User>, NGIN::Reflection::Builder<User>& b) {
    b.field<&User::id>("id");
    b.field<&User::score>("score");
  }

  struct Math {
    int mul(int a, int b) const { return a*b; }
    float mul(float a, float b) const { return a*b; }
    double mul(int a, double b) const { return a*b; }
    friend void ngin_reflect(NGIN::Reflection::tag<Math>, NGIN::Reflection::Builder<Math>& b) {
      b.method<static_cast<int (Math::*)(int,int) const>(&Math::mul)>("mul");
      b.method<static_cast<float (Math::*)(float,float) const>(&Math::mul)>("mul");
      b.method<static_cast<double (Math::*)(int,double) const>(&Math::mul)>("mul");
    }
  };
}

suite<"NGIN::Reflection::Edge"> edge = [] {
  using namespace NGIN::Reflection;
  using EdgeDemo::User;
  using EdgeDemo::Math;

  "Type_NotFound"_test = [] {
    auto r = NGIN::Reflection::type("NoSuch.Type");
    expect(!r.has_value());
  };

  "Field_NotFound"_test = [] {
    auto t = type_of<User>();
    auto f = t.GetField("nope");
    expect(!f.has_value());
  };

  "Method_WrongArity"_test = [] {
    auto t = type_of<Math>();
    auto m = t.GetMethod("mul").value();
    Math math{};
    Any none[0] = {};
    auto out0 = m.invoke(&math, none, 0);
    expect(!out0.has_value());
    Any three[3] = { Any::make(1), Any::make(2), Any::make(3) };
    auto out3 = m.invoke(&math, three, 3);
    expect(!out3.has_value());
  };

  "Overload_Resolution_Exact_And_Convert"_test = [] {
    auto t = type_of<Math>();
    Math math{};

    Any ii[2] = { Any::make(3), Any::make(4) };
    auto m1 = t.ResolveMethod("mul", ii, 2).value();
    expect(eq(m1.invoke(&math, ii, 2).value().as<int>(), 12));

    Any id[2] = { Any::make(3), Any::make(2.5) };
    auto m2 = t.ResolveMethod("mul", id, 2).value();
    expect(eq(m2.invoke(&math, id, 2).value().as<double>(), 7.5));

    Any ff[2] = { Any::make(2.0f), Any::make(5.0f) };
    auto m3 = t.ResolveMethod("mul", ff, 2).value();
    expect(eq(m3.invoke(&math, ff, 2).value().as<float>(), 10.0f));
  };

  "Resolve_NoViable_And_Conversion_Fail"_test = [] {
    auto t = type_of<Math>();
    Math math{};
    Any bad[2] = { Any::make(std::string{"x"}), Any::make(2) };
    auto mr = t.ResolveMethod("mul", bad, 2);
    expect(!mr.has_value());
  };

  "Attribute_NotFound"_test = [] {
    auto t = type_of<User>();
    auto f = t.GetField("id").value();
    auto a = f.attribute("nope");
    expect(!a.has_value());
  };

  "Any_Copy_And_Heap_Fallback"_test = [] {
    struct Big { char buf[64]; int v; };
    Big b{}; b.buf[0]=42; b.v = 99;
    Any a = Any::make(b);      // heap fallback (sizeof Big > 32)
    Any c = a;                 // copy
    expect(eq(c.as<Big>().v, 99));
    expect(eq(c.as<Big>().buf[0], char{42}));
  };
};
