#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

#include <array>

namespace RuntimeDemo
{
  struct Entity
  {
    int id{7};
  };

  struct User : Entity
  {
    std::string name{"Ada"};
    int score{10};

    int GetScore() const { return score; }
    void SetScore(int value) { score = value; }
    int Add(int lhs, int rhs) const { return lhs + rhs; }
  };

  enum class Role
  {
    Player = 1,
    Admin = 2,
  };

  inline int Multiply(int lhs, int rhs)
  {
    return lhs * rhs;
  }

  inline void NginReflect(NGIN::Reflection::Tag<Entity>, NGIN::Reflection::TypeBuilder<Entity> &builder)
  {
    builder.SetName("RuntimeDemo::Entity");
    builder.Field<&Entity::id>("id");
    builder.Attribute("category", std::string("entity"));
  }

  inline void NginReflect(NGIN::Reflection::Tag<User>, NGIN::Reflection::TypeBuilder<User> &builder)
  {
    builder.SetName("RuntimeDemo::User");
    builder.Field<&User::name>("name");
    builder.Property<&User::GetScore, &User::SetScore>("score");
    builder.Method<&User::Add>("Add");
    builder.Base<Entity>();
    builder.Attribute("category", std::string("user"));
  }

  inline void NginReflect(NGIN::Reflection::Tag<Role>, NGIN::Reflection::TypeBuilder<Role> &builder)
  {
    builder.SetName("RuntimeDemo::Role");
    builder.EnumValue("Player", Role::Player);
    builder.EnumValue("Admin", Role::Admin);
  }
} // namespace RuntimeDemo

TEST_CASE("Unified runtime supports local metadata access and invocation", "[reflection][unified]")
{
  using namespace NGIN::Reflection;

  ModuleRegistration module{"RuntimeDemo.Module"};
  module.RegisterTypes<RuntimeDemo::Entity, RuntimeDemo::User, RuntimeDemo::Role>();
  module.RegisterFunction<&RuntimeDemo::Multiply>("RuntimeDemo::Multiply");
  REQUIRE(module.Commit());

  auto snapshot = GetRegistrySnapshot();
  CHECK(snapshot.moduleCount >= 1);
  CHECK(snapshot.typeCount >= 3);

  auto type = GetType("RuntimeDemo::User");
  REQUIRE(type.has_value());
  CHECK(type->FieldCount() == 1);
  CHECK(type->PropertyCount() == 1);
  CHECK(type->MethodCount() == 1);
  CHECK(type->BaseCount() == 1);

  auto instance = type->Construct();
  REQUIRE(instance.has_value());

  auto nameField = type->GetField("name");
  REQUIRE(nameField.has_value());
  auto nameValue = nameField->Read(*instance);
  REQUIRE(nameValue.has_value());
  REQUIRE(nameValue->TryAs<std::string>() != nullptr);
  CHECK(*nameValue->TryAs<std::string>() == "Ada");

  Value updatedName{std::string{"Grace"}};
  REQUIRE(nameField->Write(*instance, updatedName).has_value());
  auto updatedNameValue = nameField->Read(*instance);
  REQUIRE(updatedNameValue.has_value());
  CHECK(*updatedNameValue->TryAs<std::string>() == "Grace");

  auto scoreProperty = type->GetProperty("score");
  REQUIRE(scoreProperty.has_value());
  auto scoreValue = scoreProperty->Read(*instance);
  REQUIRE(scoreValue.has_value());
  CHECK(*scoreValue->TryAs<std::int64_t>() == 10);

  Value newScore{std::int64_t{42}};
  REQUIRE(scoreProperty->Write(*instance, newScore).has_value());
  auto changedScore = scoreProperty->Read(*instance);
  REQUIRE(changedScore.has_value());
  CHECK(*changedScore->TryAs<std::int64_t>() == 42);

  std::array<Value, 2> addArgs{Value{std::int64_t{2}}, Value{std::int64_t{5}}};
  auto method = type->ResolveMethod("Add", addArgs);
  REQUIRE(method.has_value());
  auto sum = method->Invoke(*instance, addArgs);
  REQUIRE(sum.has_value());
  REQUIRE(sum->TryAs<std::int64_t>() != nullptr);
  CHECK((*sum->TryAs<std::int64_t>()) == 7);

  auto base = type->BaseAt(0);
  REQUIRE(base.has_value());
  auto upcast = base->Upcast(*instance);
  REQUIRE(upcast.has_value());
  auto entityType = GetType("RuntimeDemo::Entity");
  REQUIRE(entityType.has_value());
  auto idField = entityType->GetField("id");
  REQUIRE(idField.has_value());
  auto idValue = idField->Read(*upcast);
  REQUIRE(idValue.has_value());
  CHECK(*idValue->TryAs<std::int64_t>() == 7);

  auto roleType = GetType("RuntimeDemo::Role");
  REQUIRE(roleType.has_value());
  REQUIRE(roleType->IsEnum());
  CHECK(roleType->EnumValueCount() == 2);
  auto admin = roleType->GetEnumValue("Admin");
  REQUIRE(admin.has_value());
  CHECK(admin->UnsignedValue() == 2u);

  auto fn = GetFunction("RuntimeDemo::Multiply");
  REQUIRE(fn.has_value());
  std::array<Value, 2> multiplyArgs{Value{std::int64_t{6}}, Value{std::int64_t{7}}};
  auto product = fn->Invoke(multiplyArgs);
  REQUIRE(product.has_value());
  REQUIRE(product->TryAs<std::int64_t>() != nullptr);
  CHECK((*product->TryAs<std::int64_t>()) == 42);

  auto attribute = type->AttributeAt(0);
  REQUIRE(attribute.has_value());
  CHECK(attribute->Name() == "category");
  REQUIRE(std::get_if<std::string>(&attribute->Value()) != nullptr);
  CHECK(*std::get_if<std::string>(&attribute->Value()) == "user");
}
