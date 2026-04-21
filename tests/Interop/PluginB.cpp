#include <NGIN/Reflection/Reflection.hpp>

namespace Interop
{
  struct Multiplier
  {
    int Mul(int lhs, int rhs) const { return lhs * rhs; }
  };

  inline void NginReflect(NGIN::Reflection::Tag<Multiplier>, NGIN::Reflection::TypeBuilder<Multiplier> &builder)
  {
    builder.SetName("Interop::Multiplier");
    builder.Method<&Multiplier::Mul>("Mul");
  }
} // namespace Interop

extern "C" NGIN_REFLECTION_API bool NGINReflectionModuleInit()
{
  using namespace NGIN::Reflection;
  return EnsureModuleInitialized("Interop.PluginB", [](ModuleRegistration &module) {
    module.RegisterType<Interop::Multiplier>();
  });
}
