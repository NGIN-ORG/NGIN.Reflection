// AdaptersTests.cpp — basic tests for sequence/tuple/variant adapters

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Adapters.hpp>

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

TEST_CASE("SequenceAdaptersExposeIndexedAccess", "[reflection][Adapters]")
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  std::vector<int> v{1, 2, 3};
  auto a = MakeSequenceAdapter(v);
  CHECK(a.Size() == NGIN::UIntSize{3});
  CHECK(a.Element(1).Cast<int>() == 2);
}

TEST_CASE("SequenceAdaptersHandleNGINVector", "[reflection][Adapters]")
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  NGIN::Containers::Vector<int> v;
  v.PushBack(4);
  v.PushBack(5);
  auto a = MakeSequenceAdapter(v);
  CHECK(a.Size() == NGIN::UIntSize{2});
  CHECK(a.Element(0).Cast<int>() == 4);
}

TEST_CASE("TupleAdapterIndexesCompileTimeElements",
          "[reflection][Adapters]")
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  auto t = std::make_tuple(7, 8.5);
  auto a = MakeTupleAdapter(t);
  CHECK(decltype(a)::Size() == NGIN::UIntSize{2});
  CHECK(a.Get<0>().Cast<int>() == 7);
}

TEST_CASE("VariantAdapterExposesCurrentAlternative",
          "[reflection][Adapters]")
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  std::variant<int, float> v{42};
  auto a = MakeVariantAdapter(v);
  CHECK(a.Index() == NGIN::UIntSize{0});
  CHECK(a.Get().Cast<int>() == 42);
}

TEST_CASE("OptionalAdapterReportsPresenceAndValue",
          "[reflection][Adapters]")
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  std::optional<int> o{};
  auto oa = MakeOptionalAdapter(o);
  CHECK_FALSE(oa.HasValue());
  o = 7;
  CHECK(oa.HasValue());
  CHECK(oa.Value().Cast<int>() == 7);
}

TEST_CASE("MapAdapterSupportsStdMap", "[reflection][Adapters]")
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  std::map<int, std::string> m;
  m.emplace(1, std::string{"one"});
  auto ma = MakeMapAdapter(m);
  CHECK(ma.Size() == NGIN::UIntSize{1});
  CHECK(ma.ContainsKey(Any{1}));
  CHECK(ma.FindValue(Any{1}).Cast<std::string>() == std::string{"one"});
  CHECK_FALSE(ma.ContainsKey(Any{2}));
}

TEST_CASE("MapAdapterConvertsKeyTypesWhenPossible",
          "[reflection][Adapters]")
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::Adapters;

  std::unordered_map<unsigned, int> m;
  m.emplace(42u, 99);
  auto ma = MakeMapAdapter(m);
  CHECK(ma.ContainsKey(Any{42}));
  CHECK(ma.FindValue(Any{42}).Cast<int>() == 99);
}
