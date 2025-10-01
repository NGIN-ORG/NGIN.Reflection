#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

#include <utility>

// Provide a Describe<T> specialization for a 3rd-party type we can't modify
namespace NGIN::Reflection
{
  template <>
  struct Describe<std::pair<int, int>>
  {
    static void Do(Builder<std::pair<int, int>> &b)
    {
      b.set_name("std::pair<int,int>");
      b.field<&std::pair<int, int>::first>("first");
      b.field<&std::pair<int, int>::second>("second");
    }
  };
} // namespace NGIN::Reflection

TEST_CASE("DescribeFallbackExposesFields", "[reflection][DescribeFallback]")
{
  using namespace NGIN::Reflection;

  auto t = TypeOf<std::pair<int, int>>();
  CHECK(t.FieldCount() == NGIN::UIntSize{2});

  auto fFirst = t.GetField("first").value();
  auto fSecond = t.GetField("second").value();

  std::pair<int, int> p{0, 0};
  CHECK(fFirst.SetAny(&p, Any{42}).has_value());
  CHECK(fSecond.SetAny(&p, Any{7}).has_value());

  CHECK(p.first == 42);
  CHECK(p.second == 7);

  CHECK(fFirst.GetAny(&p).Cast<int>() == 42);
  CHECK(fSecond.GetAny(&p).Cast<int>() == 7);
}

TEST_CASE("DescribeAppliesCvrefNormalization",
          "[reflection][DescribeFallback]")
{
  using namespace NGIN::Reflection;

  auto t0 = TypeOf<std::pair<int, int>>();
  auto t1 = TypeOf<const std::pair<int, int> &>();

  CHECK(t0.GetTypeId() == t1.GetTypeId());
  CHECK(t0.QualifiedName() == t1.QualifiedName());
  CHECK(t0.FieldCount() == t1.FieldCount());
}
