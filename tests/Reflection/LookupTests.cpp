// LookupTests.cpp — optional and non-registering lookup APIs

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace LookupDemo {
struct Unregistered {
  int v{};
};

struct Named {
  friend void NginReflect(NGIN::Reflection::Tag<Named>, NGIN::Reflection::TypeBuilder<Named> &b) {
    b.SetName("Lookup::Named");
  }
};

struct WithField {
  int value{};
  friend void NginReflect(NGIN::Reflection::Tag<WithField>, NGIN::Reflection::TypeBuilder<WithField> &b) {
    b.Field<&WithField::value>("value");
  }
};

struct WithMethods {
  int mul(int a, int b) const { return a * b; }
  float mul(float a, float b) const { return a * b; }
  friend void NginReflect(NGIN::Reflection::Tag<WithMethods>, NGIN::Reflection::TypeBuilder<WithMethods> &b) {
    b.Method<static_cast<int (WithMethods::*)(int, int) const>(&WithMethods::mul)>("mul");
    b.Method<static_cast<float (WithMethods::*)(float, float) const>(&WithMethods::mul)>("mul");
  }
};
} // namespace LookupDemo

TEST_CASE("TryGetTypeDoesNotRegister", "[reflection][Lookup]") {
  using namespace NGIN::Reflection;
  using LookupDemo::Unregistered;

  auto pre = TryGetType<Unregistered>();
  CHECK_FALSE(pre.has_value());

  auto t = GetType<Unregistered>();
  CHECK(t.IsValid());

  auto post = TryGetType<Unregistered>();
  REQUIRE(post.has_value());
  CHECK(post->GetTypeId() == t.GetTypeId());
}

TEST_CASE("FindTypeByNameReturnsOptional", "[reflection][Lookup]") {
  using namespace NGIN::Reflection;
  using LookupDemo::Named;

  auto t = GetType<Named>();
  auto found = FindType("Lookup::Named");
  REQUIRE(found.has_value());
  CHECK(found->GetTypeId() == t.GetTypeId());

  CHECK_FALSE(FindType("Lookup::Missing").has_value());
}

TEST_CASE("FindFieldReturnsOptional", "[reflection][Lookup]") {
  using namespace NGIN::Reflection;
  using LookupDemo::WithField;

  auto t = GetType<WithField>();
  auto present = t.FindField("value");
  REQUIRE(present.has_value());
  CHECK(present->Name() == std::string_view{"value"});

  CHECK_FALSE(t.FindField("missing").has_value());
}

TEST_CASE("FindMethodsReturnsOverloads", "[reflection][Lookup]") {
  using namespace NGIN::Reflection;
  using LookupDemo::WithMethods;

  auto t = GetType<WithMethods>();
  auto overloads = t.FindMethods("mul");
  CHECK(overloads.IsValid());
  CHECK(overloads.Size() == NGIN::UIntSize{2});

  auto first = t.FindMethod("mul");
  CHECK(first.has_value());

  auto missing = t.FindMethods("nope");
  CHECK_FALSE(missing.IsValid());
  CHECK(missing.Size() == NGIN::UIntSize{0});
}
