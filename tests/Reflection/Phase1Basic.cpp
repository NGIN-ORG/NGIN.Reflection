// Phase1Basic.cpp — Basic smoke tests for Phase 1 reflection

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace DemoPhase1 {
struct User {
  int id{};
  float score{};

  friend void ngin_reflect(NGIN::Reflection::Tag<User>,
                           NGIN::Reflection::Builder<User> &b) {
    b.field<&User::id>("id");
    b.field<&User::score>("score");
  }
};

struct Named {
  int value{};
  friend void ngin_reflect(NGIN::Reflection::Tag<Named>,
                           NGIN::Reflection::Builder<Named> &b) {
    b.set_name("My::Named");
    b.field<&Named::value>("v");
  }
};
} // namespace DemoPhase1

TEST_CASE("TypeOfInfersNamesAndFields", "[reflection][Phase1Basic]") {
  using namespace NGIN::Reflection;
  using DemoPhase1::User;

  auto t = TypeOf<User>();
  INFO("TypeOf<User> must be IsValid");
  CHECK(t.IsValid());

  INFO("qualified name should default from Meta::TypeName");
  CHECK(t.QualifiedName() ==
        std::string_view{NGIN::Meta::TypeName<User>::qualifiedName});
  CHECK(t.FieldCount() == NGIN::UIntSize{2});

  CHECK(t.FieldAt(0).name() == std::string_view{"id"});
  CHECK(t.FieldAt(1).name() == std::string_view{"score"});

  User u{};
  *static_cast<int *>(t.FieldAt(0).GetMut(&u)) = 42;
  CHECK(u.id == 42);
}

TEST_CASE("ExplicitNamesAndAliasesAreRespected",
          "[reflection][Phase1Basic]") {
  using namespace NGIN::Reflection;
  using DemoPhase1::Named;

  auto t = TypeOf<Named>();
  CHECK(t.QualifiedName() == std::string_view{"My::Named"});
  auto f = t.GetField("v");
  CHECK(f.has_value());
}
