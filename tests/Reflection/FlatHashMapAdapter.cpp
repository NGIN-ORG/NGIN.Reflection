// FlatHashMapAdapter.cpp — tests for NGIN FlatHashMap adapter

#include <boost/ut.hpp>

#include <NGIN/Reflection/Adapters.hpp>

using namespace boost::ut;

suite<"NGIN::Reflection::FlatHashMapAdapter"> fmapSuite = []
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  "FlatHashMap_Basic"_test = []
  {
    NGIN::Containers::FlatHashMap<int, int> m;
    // Insert a couple entries
    m.Insert(1, 10);
    m.Insert(2, 20);
    auto a = MakeFlatHashMapAdapter(m);
    expect(eq(a.size(), NGIN::UIntSize{2}));
    expect(a.contains_key(Any::make(1)));
    expect(eq(a.find_value(Any::make(2)).As<int>(), 20));
    expect(!a.contains_key(Any::make(3)));
  };
};
