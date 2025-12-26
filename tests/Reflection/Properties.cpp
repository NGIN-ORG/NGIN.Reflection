// Properties.cpp - tests for property registration and access

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace PropDemo {
struct User {
  int score{0};
  int GetScore() const { return score; }
  void SetScore(int v) { score = v; }
  friend void NginReflect(NGIN::Reflection::Tag<User>, NGIN::Reflection::TypeBuilder<User> &b) {
    b.Property<&User::GetScore, &User::SetScore>("score");
  }
};

struct RefProp {
  int value{0};
  int &Value() { return value; }
  friend void NginReflect(NGIN::Reflection::Tag<RefProp>, NGIN::Reflection::TypeBuilder<RefProp> &b) {
    b.Property<&RefProp::Value>("value");
  }
};

struct ReadOnly {
  int value{3};
  const int &Value() const { return value; }
  friend void NginReflect(NGIN::Reflection::Tag<ReadOnly>, NGIN::Reflection::TypeBuilder<ReadOnly> &b) {
    b.Property<&ReadOnly::Value>("value");
  }
};
} // namespace PropDemo

TEST_CASE("PropertyGetterSetterRoundTrip", "[reflection][Properties]") {
  using namespace NGIN::Reflection;
  using PropDemo::User;

  auto t = GetType<User>();
  auto p = t.GetProperty("score").value();

  User u{7};
  CHECK(p.Get<int>(u).value() == 7);
  CHECK(p.Set(u, 12).has_value());
  CHECK(p.Get<int>(u).value() == 12);
}

TEST_CASE("PropertyImplicitSetterFromRefGetter", "[reflection][Properties]") {
  using namespace NGIN::Reflection;
  using PropDemo::RefProp;

  auto t = GetType<RefProp>();
  auto p = t.GetProperty("value").value();

  RefProp r{5};
  CHECK(p.Get<int>(r).value() == 5);
  CHECK(p.Set(r, 21).has_value());
  CHECK(r.value == 21);
}

TEST_CASE("PropertyReadOnlyRejectsSet", "[reflection][Properties]") {
  using namespace NGIN::Reflection;
  using PropDemo::ReadOnly;

  auto t = GetType<ReadOnly>();
  auto p = t.GetProperty("value").value();

  ReadOnly r{};
  CHECK(p.Get<int>(r).value() == 3);
  CHECK_FALSE(p.Set(r, 4).has_value());
}

TEST_CASE("MemberEnumerationIncludesProperties", "[reflection][Properties]") {
  using namespace NGIN::Reflection;
  using PropDemo::User;

  auto t = GetType<User>();
  CHECK(t.MemberCount() == t.FieldCount() + t.PropertyCount() + t.MethodCount() + t.ConstructorCount());
  bool sawProperty = false;
  for (NGIN::UIntSize i = 0; i < t.MemberCount(); ++i) {
    auto m = t.MemberAt(i);
    if (m.IsProperty()) {
      auto p = m.AsProperty();
      if (p.Name() == std::string_view{"score"})
        sawProperty = true;
    }
  }
  CHECK(sawProperty);
}
