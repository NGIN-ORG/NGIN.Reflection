#include <NGIN/Reflection/Reflection.hpp>

namespace Interop
{
  struct Adder
  {
    int Add(int lhs, int rhs) const { return lhs + rhs; }
  };

  struct Offsetter
  {
    explicit Offsetter(NGIN::Memory::Shared<int> offsetIn)
        : offset(std::move(offsetIn))
    {
    }

    int AddOffset(int value) const { return value + *offset; }
    NGIN::Memory::Shared<int> offset{};
  };

  inline void NginReflect(NGIN::Reflection::Tag<Adder>, NGIN::Reflection::TypeBuilder<Adder> &builder)
  {
    builder.SetName("Interop::Adder");
    builder.Method<&Adder::Add>("Add");
  }

  inline void NginReflect(NGIN::Reflection::Tag<Offsetter>, NGIN::Reflection::TypeBuilder<Offsetter> &builder)
  {
    builder.SetName("Interop::Offsetter");
    builder.InjectableConstructor<
        NGIN::Reflection::NamedConstructorDependency<NGIN::Memory::Shared<int>, "offset">>();
    builder.Method<&Offsetter::AddOffset>("AddOffset");
  }
} // namespace Interop

extern "C" NGIN_REFLECTION_API bool NGINReflectionModuleInit()
{
  using namespace NGIN::Reflection;
  return EnsureModuleInitialized("Interop.PluginA", [](ModuleRegistration &module) {
    module.RegisterType<Interop::Adder>();
    module.RegisterType<Interop::Offsetter>();
  });
}
