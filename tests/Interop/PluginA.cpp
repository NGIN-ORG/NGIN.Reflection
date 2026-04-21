#include <NGIN/Reflection/Reflection.hpp>

namespace Interop
{
  struct Adder
  {
    int Add(int lhs, int rhs) const { return lhs + rhs; }
  };

  inline void NginReflect(NGIN::Reflection::Tag<Adder>, NGIN::Reflection::TypeBuilder<Adder> &builder)
  {
    builder.SetName("Interop::Adder");
    builder.Method<&Adder::Add>("Add");
  }
} // namespace Interop

extern "C" NGIN_REFLECTION_API bool NGINReflectionModuleInit()
{
  using namespace NGIN::Reflection;
  return EnsureModuleInitialized("Interop.PluginA", [](ModuleRegistration &module) {
    module.RegisterType<Interop::Adder>();
  });
}
