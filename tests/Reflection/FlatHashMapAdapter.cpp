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
  CHECK(a.size() == NGIN::UIntSize{2});
  CHECK(a.contains_key(Any::make(1)));
  CHECK(a.find_value(Any::make(2)).As<int>() == 20);
  CHECK_FALSE(a.contains_key(Any::make(3)));
}
