#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace ReplaceDemo
{
  struct CounterV1
  {
    int value{1};
    int Increment(int amount) const { return value + amount; }
  };

  struct CounterV2
  {
    int value{2};
    int Multiply(int amount) const { return value * amount; }
  };

  struct OtherOwnerCounter
  {
    int value{3};
  };

  inline void NginReflect(NGIN::Reflection::Tag<CounterV1>, NGIN::Reflection::TypeBuilder<CounterV1> &builder)
  {
    builder.SetName("ReplaceDemo::Counter");
    builder.Field<&CounterV1::value>("value");
    builder.Method<&CounterV1::Increment>("Increment");
  }

  inline void NginReflect(NGIN::Reflection::Tag<CounterV2>, NGIN::Reflection::TypeBuilder<CounterV2> &builder)
  {
    builder.SetName("ReplaceDemo::Counter");
    builder.Field<&CounterV2::value>("value");
    builder.Method<&CounterV2::Multiply>("Multiply");
  }

  inline void NginReflect(NGIN::Reflection::Tag<OtherOwnerCounter>, NGIN::Reflection::TypeBuilder<OtherOwnerCounter> &builder)
  {
    builder.SetName("ReplaceDemo::Counter");
    builder.Field<&OtherOwnerCounter::value>("value");
  }
} // namespace ReplaceDemo

TEST_CASE("Replacing a module invalidates stale handles and swaps metadata", "[reflection][reload]")
{
  using namespace NGIN::Reflection;

  ModuleRegistration v1{"ReplaceDemo.Module"};
  v1.RegisterType<ReplaceDemo::CounterV1>();
  REQUIRE(v1.Commit());

  auto oldType = GetType("ReplaceDemo::Counter");
  REQUIRE(oldType.has_value());
  CHECK(oldType->IsValid());
  CHECK(oldType->GetMethod("Increment").has_value());

  ModuleRegistration v2{"ReplaceDemo.Module"};
  v2.RegisterType<ReplaceDemo::CounterV2>();
  REQUIRE(v2.Commit());

  CHECK_FALSE(oldType->IsValid());

  auto newType = GetType("ReplaceDemo::Counter");
  REQUIRE(newType.has_value());
  CHECK(newType->GetMethod("Multiply").has_value());
  CHECK_FALSE(newType->GetMethod("Increment").has_value());
}

TEST_CASE("Different modules cannot claim the same type identity", "[reflection][collision]")
{
  using namespace NGIN::Reflection;

  ModuleRegistration ownerA{"ReplaceDemo.OwnerA"};
  ownerA.RegisterType<ReplaceDemo::CounterV1>();
  REQUIRE(ownerA.Commit());

  ModuleRegistration ownerB{"ReplaceDemo.OwnerB"};
  ownerB.RegisterType<ReplaceDemo::OtherOwnerCounter>();
  Error error{};
  CHECK_FALSE(ownerB.Commit(&error));
  CHECK(error.code == ErrorCode::Conflict);
}
