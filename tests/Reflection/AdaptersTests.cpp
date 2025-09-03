// AdaptersTests.cpp — basic tests for sequence/tuple/variant adapters

#include <boost/ut.hpp>

#include <NGIN/Reflection/Adapters.hpp>

using namespace boost::ut;

suite<"NGIN::Reflection::Adapters"> adaptersSuite = []{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  "StdVectorSequence"_test = []{
    std::vector<int> v{1,2,3};
    auto a = MakeSequenceAdapter(v);
    expect(eq(a.size(), NGIN::UIntSize{3}));
    expect(eq(a.element(1).as<int>(), 2));
  };

  "NGINVectorSequence"_test = []{
    NGIN::Containers::Vector<int> v; v.PushBack(4); v.PushBack(5);
    auto a = MakeSequenceAdapter(v);
    expect(eq(a.size(), NGIN::UIntSize{2}));
    expect(eq(a.element(0).as<int>(), 4));
  };

  "TupleAdapter"_test = []{
    auto t = std::make_tuple(7, 8.5);
    auto a = MakeTupleAdapter(t);
    expect(eq(decltype(a)::size(), NGIN::UIntSize{2}));
    expect(eq(a.get<0>().as<int>(), 7));
  };

  "VariantAdapter"_test = []{
    std::variant<int, float> v{42};
    auto a = MakeVariantAdapter(v);
    expect(eq(a.index(), NGIN::UIntSize{0}));
    expect(eq(a.get().as<int>(), 42));
  };
};

