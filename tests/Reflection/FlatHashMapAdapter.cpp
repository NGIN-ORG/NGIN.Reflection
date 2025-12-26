// FlatHashMapAdapter.cpp — tests for NGIN FlatHashMap adapter

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Adapters.hpp>

TEST_CASE("FlatHashMapAdapterExposesContainerOperations",
          "[reflection][FlatHashMapAdapter]") {
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  NGIN::Containers::FlatHashMap<int, int> m;
  m.Insert(1, 10);
  m.Insert(2, 20);

  auto a = MakeFlatHashMapAdapter(m);
  CHECK(a.Size() == NGIN::UIntSize{2});
  CHECK(a.ContainsKey(Any{1}));
  CHECK(a.FindValue(Any{2}).Cast<int>() == 20);
  CHECK_FALSE(a.ContainsKey(Any{3}));
}
