// DescribeFallback.cpp — tests for Describe<T> trait fallback and cvref normalization

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

#include <utility>

using namespace boost::ut;

// Provide a Describe<T> specialization for a 3rd-party type we can't modify
namespace NGIN::Reflection
{
  template <>
  struct Describe<std::pair<int, int>>
  {
    static void Do(Builder<std::pair<int, int>> &b)
    {
      // Use a stable literal for the name (InternName currently returns the view)
      b.set_name("std::pair<int,int>");
      b.field<&std::pair<int, int>::first>("first");
      b.field<&std::pair<int, int>::second>("second");
    }
  };
} // namespace NGIN::Reflection

suite<"NGIN::Reflection::DescribeFallback"> describeFallbackSuite = []
{
  using namespace NGIN::Reflection;

  "Trait_Fallback_Fields"_test = []
  {
    auto t = TypeOf<std::pair<int, int>>();
    expect(eq(t.FieldCount(), NGIN::UIntSize{2})) << "expected two fields";

    auto fFirst = t.GetField("first").value();
    auto fSecond = t.GetField("second").value();

    std::pair<int, int> p{0, 0};
    expect(fFirst.SetAny(&p, Any::make(42)).has_value());
    expect(fSecond.SetAny(&p, Any::make(7)).has_value());

    expect(eq(p.first, 42));
    expect(eq(p.second, 7));

    // Round-trip via Any
    expect(eq(fFirst.GetAny(&p).As<int>(), 42));
    expect(eq(fSecond.GetAny(&p).As<int>(), 7));
  };

  "CVRef_Normalization_SameRecord"_test = []
  {
    auto t0 = TypeOf<std::pair<int, int>>();
    auto t1 = TypeOf<const std::pair<int, int> &>();
    expect(eq(t0.GetTypeId(), t1.GetTypeId()));
    expect(eq(t0.QualifiedName(), t1.QualifiedName()));
    expect(eq(t0.FieldCount(), t1.FieldCount()));
  };
};
