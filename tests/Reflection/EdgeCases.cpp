// EdgeCases.cpp — extended tests for errors, conversions, overloads, Any
// SBO/heap

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace EdgeDemo
{
  struct User
  {
    int id{};
    float score{};
  };
  inline void NginReflect(NGIN::Reflection::Tag<User>,
                           NGIN::Reflection::TypeBuilder<User> &b)
  {
    b.Field<&User::id>("id");
    b.Field<&User::score>("score");
  }

  struct Math
  {
    int mul(int a, int b) const { return a * b; }
    float mul(float a, float b) const { return a * b; }
    double mul(int a, double b) const { return a * b; }
    friend void NginReflect(NGIN::Reflection::Tag<Math>,
                             NGIN::Reflection::TypeBuilder<Math> &b)
    {
      b.Method<static_cast<int (Math::*)(int, int) const>(&Math::mul)>("mul");
      b.Method<static_cast<float (Math::*)(float, float) const>(&Math::mul)>(
          "mul");
      b.Method<static_cast<double (Math::*)(int, double) const>(&Math::mul)>(
          "mul");
    }
  };
} // namespace EdgeDemo

TEST_CASE("GetTypeReportsMissingType", "[reflection][EdgeCases]")
{
  CHECK_FALSE(NGIN::Reflection::GetType("NoSuch.Type").has_value());
}

TEST_CASE("MissingFieldLookupsFail", "[reflection][EdgeCases]")
{
  using EdgeDemo::User;
  auto t = NGIN::Reflection::GetType<User>();
  CHECK_FALSE(t.GetField("nope").has_value());
}

TEST_CASE("InvokeDetectsWrongArity", "[reflection][EdgeCases]")
{
  using EdgeDemo::Math;
  using namespace NGIN::Reflection;

  auto t = GetType<Math>();
  auto m = t.GetMethod("mul").value();
  Math math{};

  auto out0 = m.Invoke(&math, std::span<const Any>{});
  CHECK_FALSE(out0.has_value());

  Any three[3] = {Any{1}, Any{2}, Any{3}};
  auto out3 = m.Invoke(&math, three, 3);
  CHECK_FALSE(out3.has_value());
}

TEST_CASE("ResolveChoosesOverloadsCorrectly", "[reflection][EdgeCases]")
{
  using EdgeDemo::Math;
  using namespace NGIN::Reflection;

  auto t = GetType<Math>();
  Math math{};

  Any ii[2] = {Any{3}, Any{4}};
  auto m1 = t.ResolveMethod("mul", ii, 2).value();
  CHECK(m1.Invoke(&math, ii, 2).value().Cast<int>() == 12);

  Any id[2] = {Any{3}, Any{2.5}};
  auto m2 = t.ResolveMethod("mul", id, 2).value();
  CHECK(m2.Invoke(&math, id, 2).value().Cast<double>() == Catch::Approx(7.5));

  Any ff[2] = {Any{2.0f}, Any{5.0f}};
  auto m3 = t.ResolveMethod("mul", ff, 2).value();
  CHECK(m3.Invoke(&math, ff, 2).value().Cast<float>() == Catch::Approx(10.0f));
}

TEST_CASE("ResolveRejectsInvalidOverloads", "[reflection][EdgeCases]")
{
  using EdgeDemo::Math;
  using namespace NGIN::Reflection;

  auto t = GetType<Math>();

  Any bad[2] = {Any{std::string{"x"}}, Any{2}};
  auto mr = t.ResolveMethod("mul", bad, 2);
  CHECK_FALSE(mr.has_value());
}

TEST_CASE("AttributesAbsentWhenNotDeclared", "[reflection][EdgeCases]")
{
  using EdgeDemo::User;
  using namespace NGIN::Reflection;

  auto t = GetType<User>();
  auto f = t.GetField("id").value();
  CHECK_FALSE(f.Attribute("nope").has_value());
}

TEST_CASE("AnyCopiesHeapFallbackPayloads", "[reflection][EdgeCases]")
{
  using namespace NGIN::Reflection;

  struct Big
  {
    char buf[64];
    int v;
  };

  Big b{};
  b.buf[0] = 42;
  b.v = 99;

  Any a{b};
  Any c = a;

  CHECK(c.Cast<Big>().v == 99);
  CHECK(c.Cast<Big>().buf[0] == char{42});
}
