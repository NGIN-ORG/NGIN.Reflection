#include <NGIN/Reflection/Reflection.hpp>

#include <iostream>

namespace Demo
{
  struct User
  {
    int id{1};
    std::string name{"Ada"};
    int score{10};

    int GetScore() const { return score; }
    void SetScore(int value) { score = value; }
    int Add(int lhs, int rhs) const { return lhs + rhs; }

    friend void NginReflect(NGIN::Reflection::Tag<User>,
                            NGIN::Reflection::TypeBuilder<User> &builder)
    {
      builder.SetName("Demo::User");
      builder.Field<&User::id>("id");
      builder.Field<&User::name>("name");
      builder.Property<&User::GetScore, &User::SetScore>("score");
      builder.Method<&User::Add>("Add");
      builder.Attribute("category", std::string("demo"));
    }
  };
} // namespace Demo

int main()
{
  using namespace NGIN::Reflection;

  ModuleRegistration module{"Demo.QuickStart"};
  module.RegisterType<Demo::User>();
  if (!module.Commit())
    return 1;

  auto type = GetType("Demo::User");
  if (!type.has_value())
    return 1;

  auto instance = type->Construct();
  if (!instance.has_value())
    return 1;

  auto score = type->GetProperty("score");
  auto add = type->GetMethod("Add");
  if (!score.has_value() || !add.has_value())
    return 1;

  auto before = score->Read(*instance);
  if (!before.has_value() || before->TryAs<std::int64_t>() == nullptr)
    return 1;

  std::cout << "Initial score: " << *before->TryAs<std::int64_t>() << "\n";

  Value updatedScore{std::int64_t{21}};
  if (!score->Write(*instance, updatedScore).has_value())
    return 1;

  auto after = score->Read(*instance);
  if (!after.has_value() || after->TryAs<std::int64_t>() == nullptr)
    return 1;

  std::array<Value, 2> addArgs{Value{std::int64_t{2}}, Value{std::int64_t{5}}};
  auto sum = add->Invoke(*instance, addArgs);
  if (!sum.has_value() || sum->TryAs<std::int64_t>() == nullptr)
    return 1;

  std::cout << "Updated score: " << *after->TryAs<std::int64_t>() << "\n";
  std::cout << "Add(2, 5): " << *sum->TryAs<std::int64_t>() << "\n";
  return 0;
}
