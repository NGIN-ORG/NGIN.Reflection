#include <NGIN/Reflection/TypeBuilder.hpp>
#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/ModuleInit.hpp>

namespace Interop
{
  // Same qualified name/typeId as in PluginA to force a conflict
  struct Common { int a{2}; };

  struct Multiplier
  {
    int Mul(int x, int y) const { return x * y; }
  };

  template <class T>
  void NginReflect(NGIN::Reflection::Tag<T>, NGIN::Reflection::TypeBuilder<T> &);

  template <>
  void NginReflect<Common>(NGIN::Reflection::Tag<Common>, NGIN::Reflection::TypeBuilder<Common> &b)
  {
    b.Field<&Common::a>("a");
  }

  template <>
  void NginReflect<Multiplier>(NGIN::Reflection::Tag<Multiplier>, NGIN::Reflection::TypeBuilder<Multiplier> &b)
  {
    b.Method<&Multiplier::Mul>("Mul");
  }
}

extern "C" NGIN_REFLECTION_API bool NGINReflectionModuleInit()
{
  using namespace NGIN::Reflection;
  return EnsureModuleInitialized("InteropPluginB", [](ModuleRegistration &module) {
    module.RegisterTypes<Interop::Common, Interop::Multiplier>();
  });
}
