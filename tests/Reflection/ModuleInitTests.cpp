// ModuleInitTests.cpp - coverage for module initialization and unload paths

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace ModuleInitDemo
{
  struct WithStatic
  {
    static int AddOne(int v) { return v + 1; }
  };

  inline void NginReflect(NGIN::Reflection::Tag<WithStatic>,
                          NGIN::Reflection::TypeBuilder<WithStatic> &b)
  {
    b.StaticMethod<&WithStatic::AddOne>("ModuleUnload_AddOne");
  }
} // namespace ModuleInitDemo

TEST_CASE("EnsureModuleInitialized runs once per module", "[reflection][ModuleInit]")
{
  using namespace NGIN::Reflection;

  int calls = 0;
  CHECK(EnsureModuleInitialized("ModuleInit.Once", [&](ModuleRegistration &) { ++calls; }));
  CHECK(EnsureModuleInitialized("ModuleInit.Once", [&](ModuleRegistration &) { ++calls; }));
  CHECK(calls == 1);

  CHECK(EnsureModuleInitialized("ModuleInit.Other", [&](ModuleRegistration &) { ++calls; }));
  CHECK(calls == 2);

  int failCalls = 0;
  CHECK_FALSE(EnsureModuleInitialized("ModuleInit.Fail", [&](ModuleRegistration &) {
    ++failCalls;
    return false;
  }));
  CHECK(EnsureModuleInitialized("ModuleInit.Fail", [&](ModuleRegistration &) {
    ++failCalls;
    return true;
  }));
  CHECK(failCalls == 2);
}

TEST_CASE("UnregisterModule removes types and functions", "[reflection][ModuleInit]")
{
  using namespace NGIN::Reflection;

  ModuleRegistration module{"ModuleInit.Unload"};
  module.RegisterType<ModuleInitDemo::WithStatic>();
  const auto moduleId = module.GetModuleId();

  CHECK(TryGetType<ModuleInitDemo::WithStatic>().has_value());

  auto fn = GetFunction("ModuleUnload_AddOne");
  REQUIRE(fn.has_value());
  auto result = fn->InvokeAs<int>(1);
  REQUIRE(result.has_value());
  CHECK(result.value() == 2);

  CHECK(UnregisterModule(moduleId));

  CHECK_FALSE(TryGetType<ModuleInitDemo::WithStatic>().has_value());
  CHECK_FALSE(FindFunction("ModuleUnload_AddOne").has_value());
  CHECK_FALSE(fn->IsValid());
  CHECK_FALSE(fn->InvokeAs<int>(1).has_value());

  int reinitCalls = 0;
  CHECK(EnsureModuleInitialized("ModuleInit.Unload", [&](ModuleRegistration &) { ++reinitCalls; }));
  CHECK(reinitCalls == 1);
}
